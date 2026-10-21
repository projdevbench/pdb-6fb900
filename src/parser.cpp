/**
 * @file parser.cpp
 * @brief Parsing implementation for Scheme syntax tree to expression tree conversion
 * 
 * This file implements the parsing logic that converts syntax trees into
 * expression trees that can be evaluated.
 * primitive operations, and function applications.
 *
 * The parse-time environment mirrors lexical scoping: every name bound by
 * lambda/let/letrec/internal define is added to it (with a dummy value), so a
 * locally bound name that coincides with a primitive or a reserved word is
 * parsed as an ordinary variable/application instead of the built-in form.
 */

#include "RE.hpp"
#include "Def.hpp"
#include "syntax.hpp"
#include "value.hpp"
#include "expr.hpp"
#include <map>
#include <string>
#include <iostream>
#include <cctype>

#define mp make_pair
using std::string;
using std::vector;
using std::pair;

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

namespace {

SymbolSyntax *asSymbol(const Syntax &stx) {
    return dynamic_cast<SymbolSyntax*>(stx.get());
}

List *asList(const Syntax &stx) {
    return dynamic_cast<List*>(stx.get());
}

bool isBound(const string &name, Assoc &env) {
    return lookup(name, env) != nullptr;
}

Assoc bindNames(const vector<string> &names, Assoc env) {
    for (const auto &name : names) {
        env = extend(name, VoidV(), env);
    }
    return env;
}

// First character must not be a digit or one of ".@"; '#', '\'', '"', '`' are forbidden.
bool isValidIdentifier(const string &s) {
    if (s.empty() || isdigit(static_cast<unsigned char>(s[0])) || s[0] == '.' || s[0] == '@') {
        return false;
    }
    for (char c : s) {
        if (c == '#' || c == '\'' || c == '"' || c == '`') {
            return false;
        }
    }
    return true;
}

string expectIdentifier(const Syntax &stx) {
    SymbolSyntax *sym = asSymbol(stx);
    if (sym == nullptr || !isValidIdentifier(sym->s)) {
        throw RuntimeError("Expected an identifier");
    }
    return sym->s;
}

// Names introduced by define must not collide with primitives or reserved words.
string expectDefinableName(const Syntax &stx) {
    string name = expectIdentifier(stx);
    if (primitives.count(name) || reserved_words.count(name)) {
        throw RuntimeError("Cannot redefine built-in: " + name);
    }
    return name;
}

vector<string> parseParams(const Syntax &stx) {
    List *lst = asList(stx);
    if (lst == nullptr) {
        throw RuntimeError("Malformed parameter list");
    }
    vector<string> params;
    for (const auto &p : lst->stxs) {
        params.push_back(expectIdentifier(p));
    }
    return params;
}

vector<Expr> parseAll(const vector<Syntax> &stxs, size_t from, Assoc &env) {
    vector<Expr> es;
    for (size_t i = from; i < stxs.size(); i++) {
        es.push_back(stxs[i]->parse(env));
    }
    return es;
}

// Collect names of definitions appearing at the top level of a body (including inside begin).
void collectDefines(const Syntax &stx, Assoc &env, vector<string> &names) {
    List *lst = asList(stx);
    if (lst == nullptr || lst->stxs.empty()) return;
    SymbolSyntax *head = asSymbol(lst->stxs[0]);
    if (head == nullptr || isBound(head->s, env)) return;
    if (head->s == "define" && lst->stxs.size() >= 2) {
        if (SymbolSyntax *name = asSymbol(lst->stxs[1])) {
            names.push_back(name->s);
        } else if (List *sig = asList(lst->stxs[1])) {
            if (!sig->stxs.empty() && asSymbol(sig->stxs[0]) != nullptr) {
                names.push_back(asSymbol(sig->stxs[0])->s);
            }
        }
    } else if (head->s == "begin") {
        for (size_t i = 1; i < lst->stxs.size(); i++) {
            collectDefines(lst->stxs[i], env, names);
        }
    }
}

// A body (of lambda/let/letrec/define) is a non-empty sequence with its own scope
// for internal definitions, which are bound (unassigned) when the body is entered.
Expr parseBody(const vector<Syntax> &stxs, size_t from, Assoc &env) {
    if (from >= stxs.size()) {
        throw RuntimeError("Empty body");
    }
    vector<string> defs;
    for (size_t i = from; i < stxs.size(); i++) {
        collectDefines(stxs[i], env, defs);
    }
    Assoc inner = bindNames(defs, env);
    vector<Expr> es = parseAll(stxs, from, inner);
    if (defs.empty() && es.size() == 1) {
        return es[0];
    }
    return Expr(new Begin(es, defs));
}

// Validates a binding list ((var expr) ...) and returns the bound names.
vector<string> bindingNames(const Syntax &stx) {
    List *lst = asList(stx);
    if (lst == nullptr) {
        throw RuntimeError("Malformed binding list");
    }
    vector<string> names;
    for (const auto &b : lst->stxs) {
        List *binding = asList(b);
        if (binding == nullptr || binding->stxs.size() != 2) {
            throw RuntimeError("Malformed binding");
        }
        names.push_back(expectIdentifier(binding->stxs[0]));
    }
    return names;
}

// Parses the expressions of a (validated) binding list in exprEnv.
vector<pair<string, Expr>> parseBindings(const Syntax &stx, const vector<string> &names, Assoc &exprEnv) {
    List *lst = asList(stx);
    vector<pair<string, Expr>> binds;
    for (size_t i = 0; i < names.size(); i++) {
        binds.push_back(mp(names[i], asList(lst->stxs[i])->stxs[1]->parse(exprEnv)));
    }
    return binds;
}

// Builds the expression for a primitive call; returns a null Expr on arity mismatch
// so that the error is deferred to run time (applying the primitive procedure).
Expr parsePrimitive(ExprType type, const vector<Expr> &a) {
    size_t n = a.size();
    switch (type) {
        case E_PLUS:   return n == 2 ? Expr(new Plus(a[0], a[1]))  : Expr(new PlusVar(a));
        case E_MINUS:  return n == 2 ? Expr(new Minus(a[0], a[1])) : Expr(new MinusVar(a));
        case E_MUL:    return n == 2 ? Expr(new Mult(a[0], a[1]))  : Expr(new MultVar(a));
        case E_DIV:    return n == 2 ? Expr(new Div(a[0], a[1]))   : Expr(new DivVar(a));
        case E_MODULO: if (n == 2) return Expr(new Modulo(a[0], a[1])); break;
        case E_EXPT:   if (n == 2) return Expr(new Expt(a[0], a[1])); break;
        case E_LT:     return n == 2 ? Expr(new Less(a[0], a[1]))      : Expr(new LessVar(a));
        case E_LE:     return n == 2 ? Expr(new LessEq(a[0], a[1]))    : Expr(new LessEqVar(a));
        case E_EQ:     return n == 2 ? Expr(new Equal(a[0], a[1]))     : Expr(new EqualVar(a));
        case E_GE:     return n == 2 ? Expr(new GreaterEq(a[0], a[1])) : Expr(new GreaterEqVar(a));
        case E_GT:     return n == 2 ? Expr(new Greater(a[0], a[1]))   : Expr(new GreaterVar(a));
        case E_CONS:   if (n == 2) return Expr(new Cons(a[0], a[1])); break;
        case E_CAR:    if (n == 1) return Expr(new Car(a[0])); break;
        case E_CDR:    if (n == 1) return Expr(new Cdr(a[0])); break;
        case E_LIST:   return Expr(new ListFunc(a));
        case E_SETCAR: if (n == 2) return Expr(new SetCar(a[0], a[1])); break;
        case E_SETCDR: if (n == 2) return Expr(new SetCdr(a[0], a[1])); break;
        case E_NOT:    if (n == 1) return Expr(new Not(a[0])); break;
        case E_AND:    return Expr(new AndVar(a));
        case E_OR:     return Expr(new OrVar(a));
        case E_EQQ:    if (n == 2) return Expr(new IsEq(a[0], a[1])); break;
        case E_BOOLQ:  if (n == 1) return Expr(new IsBoolean(a[0])); break;
        case E_INTQ:   if (n == 1) return Expr(new IsFixnum(a[0])); break;
        case E_NULLQ:  if (n == 1) return Expr(new IsNull(a[0])); break;
        case E_PAIRQ:  if (n == 1) return Expr(new IsPair(a[0])); break;
        case E_PROCQ:  if (n == 1) return Expr(new IsProcedure(a[0])); break;
        case E_SYMBOLQ:if (n == 1) return Expr(new IsSymbol(a[0])); break;
        case E_LISTQ:  if (n == 1) return Expr(new IsList(a[0])); break;
        case E_STRINGQ:if (n == 1) return Expr(new IsString(a[0])); break;
        case E_DISPLAY:if (n == 1) return Expr(new Display(a[0])); break;
        case E_VOID:   if (n == 0) return Expr(new MakeVoid()); break;
        case E_EXIT:   if (n == 0) return Expr(new Exit()); break;
        default: break;
    }
    return Expr(nullptr);
}

} // namespace

/**
 * @brief Default parse method (should be overridden by subclasses)
 */
Expr Syntax::parse(Assoc &env) {
    return ptr->parse(env);
}

Expr Number::parse(Assoc &env) {
    return Expr(new Fixnum(n));
}

Expr RationalSyntax::parse(Assoc &env) {
    return Expr(new RationalNum(numerator, denominator));
}

Expr SymbolSyntax::parse(Assoc &env) {
    return Expr(new Var(s));
}

Expr StringSyntax::parse(Assoc &env) {
    return Expr(new StringExpr(s));
}

Expr TrueSyntax::parse(Assoc &env) {
    return Expr(new True());
}

Expr FalseSyntax::parse(Assoc &env) {
    return Expr(new False());
}

Expr List::parse(Assoc &env) {
    if (stxs.empty()) {
        return Expr(new Quote(Syntax(new List())));
    }

    SymbolSyntax *id = asSymbol(stxs[0]);
    // Non-symbol operator, or a symbol bound in the current scope: ordinary application
    if (id == nullptr || isBound(id->s, env)) {
        return Expr(new Apply(stxs[0]->parse(env), parseAll(stxs, 1, env)));
    }
    const string &op = id->s;

    auto prim = primitives.find(op);
    if (prim != primitives.end()) {
        vector<Expr> parameters = parseAll(stxs, 1, env);
        Expr e = parsePrimitive(prim->second, parameters);
        if (e.get() != nullptr) {
            return e;
        }
        return Expr(new Apply(Expr(new Var(op)), parameters));
    }

    auto reserved = reserved_words.find(op);
    if (reserved != reserved_words.end()) {
        size_t n = stxs.size();
        switch (reserved->second) {
            case E_QUOTE:
                if (n != 2) throw RuntimeError("quote expects one argument");
                return Expr(new Quote(stxs[1]));

            case E_BEGIN:
                return Expr(new Begin(parseAll(stxs, 1, env)));

            case E_IF: {
                if (n != 3 && n != 4) throw RuntimeError("Malformed if");
                Expr alter = n == 4 ? stxs[3]->parse(env) : Expr(new Begin({}));
                return Expr(new If(stxs[1]->parse(env), stxs[2]->parse(env), alter));
            }

            case E_COND: {
                vector<vector<Expr>> clauses;
                for (size_t i = 1; i < n; i++) {
                    List *clause = asList(stxs[i]);
                    if (clause == nullptr || clause->stxs.empty()) throw RuntimeError("Malformed cond clause");
                    vector<Expr> exprs;
                    SymbolSyntax *head = asSymbol(clause->stxs[0]);
                    if (head != nullptr && head->s == "else" && !isBound("else", env)) {
                        exprs.push_back(Expr(new True()));
                    } else {
                        exprs.push_back(clause->stxs[0]->parse(env));
                    }
                    for (size_t j = 1; j < clause->stxs.size(); j++) {
                        exprs.push_back(clause->stxs[j]->parse(env));
                    }
                    clauses.push_back(exprs);
                }
                return Expr(new Cond(clauses));
            }

            case E_LAMBDA: {
                if (n < 3) throw RuntimeError("Malformed lambda");
                vector<string> params = parseParams(stxs[1]);
                Assoc inner = bindNames(params, env);
                return Expr(new Lambda(params, parseBody(stxs, 2, inner)));
            }

            case E_DEFINE: {
                if (n < 3) throw RuntimeError("Malformed define");
                if (asSymbol(stxs[1]) != nullptr) {
                    if (n != 3) throw RuntimeError("Malformed define");
                    return Expr(new Define(expectDefinableName(stxs[1]), stxs[2]->parse(env)));
                }
                List *sig = asList(stxs[1]);
                if (sig == nullptr || sig->stxs.empty()) throw RuntimeError("Malformed define");
                string name = expectDefinableName(sig->stxs[0]);
                vector<string> params;
                for (size_t i = 1; i < sig->stxs.size(); i++) {
                    params.push_back(expectIdentifier(sig->stxs[i]));
                }
                Assoc inner = bindNames(params, env);
                return Expr(new Define(name, Expr(new Lambda(params, parseBody(stxs, 2, inner)))));
            }

            case E_LET: {
                if (n < 3) throw RuntimeError("Malformed let");
                if (asSymbol(stxs[1]) != nullptr) {
                    // named let: ((letrec ((name (lambda (vars...) body...))) name) inits...)
                    if (n < 4) throw RuntimeError("Malformed named let");
                    string name = expectIdentifier(stxs[1]);
                    vector<string> vars = bindingNames(stxs[2]);
                    vector<Expr> inits;
                    for (auto &b : parseBindings(stxs[2], vars, env)) inits.push_back(b.second);
                    Assoc loopEnv = bindNames({name}, env);
                    Assoc inner = bindNames(vars, loopEnv);
                    Expr lambda(new Lambda(vars, parseBody(stxs, 3, inner)));
                    Expr loop(new Letrec({mp(name, lambda)}, Expr(new Var(name))));
                    return Expr(new Apply(loop, inits));
                }
                vector<string> vars = bindingNames(stxs[1]);
                vector<pair<string, Expr>> binds = parseBindings(stxs[1], vars, env);
                Assoc inner = bindNames(vars, env);
                return Expr(new Let(binds, parseBody(stxs, 2, inner)));
            }

            case E_LETREC: {
                if (n < 3) throw RuntimeError("Malformed letrec");
                vector<string> vars = bindingNames(stxs[1]);
                Assoc inner = bindNames(vars, env);
                vector<pair<string, Expr>> binds = parseBindings(stxs[1], vars, inner);
                return Expr(new Letrec(binds, parseBody(stxs, 2, inner)));
            }

            case E_SET:
                if (n != 3) throw RuntimeError("Malformed set!");
                return Expr(new Set(expectIdentifier(stxs[1]), stxs[2]->parse(env)));

            default:
                throw RuntimeError("Unknown reserved word: " + op);
        }
    }

    return Expr(new Apply(Expr(new Var(op)), parseAll(stxs, 1, env)));
}
