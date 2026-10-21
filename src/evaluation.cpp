/**
 * @file evaluation.cpp
 * @brief Expression evaluation implementation for the Scheme interpreter
 * @author luke36
 * 
 * This file implements evaluation methods for all expression types in the Scheme
 * interpreter. Functions are organized according to ExprType enumeration order
 * from Def.hpp for consistency and maintainability.
 *
 * Control forms (if/cond/begin/and/or/let/letrec) and procedure application are
 * evaluated by a single iterative loop (`run`), so expressions in tail position
 * — in particular tail calls — do not consume C++ stack.
 */

#include "value.hpp"
#include "expr.hpp" 
#include "RE.hpp"
#include "syntax.hpp"
#include <cstring>
#include <cstdlib>
#include <vector>
#include <map>
#include <unordered_map>
#include <climits>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

int compareNumericValues(const Value &v1, const Value &v2);

namespace {

bool isTrue(const Value &v) {
    return !(v->v_type == V_BOOL && !static_cast<Boolean*>(v.get())->b);
}

Assoc bindUnassigned(const std::vector<std::string> &names, Assoc env) {
    for (const auto &name : names) {
        env = extend(name, Value(nullptr), env);
    }
    return env;
}

long long gcdll(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// Builds a normalized number: an Integer when the denominator divides the numerator.
Value makeNumber(long long num, long long den) {
    if (den == 0) {
        throw RuntimeError("Division by zero");
    }
    if (den < 0) {
        num = -num;
        den = -den;
    }
    long long g = gcdll(num, den);
    if (g > 1) {
        num /= g;
        den /= g;
    }
    if (den == 1) {
        return IntegerV(static_cast<int>(num));
    }
    return RationalV(static_cast<int>(num), static_cast<int>(den));
}

void toFraction(const Value &v, long long &num, long long &den) {
    if (v->v_type == V_INT) {
        num = static_cast<Integer*>(v.get())->n;
        den = 1;
    } else if (v->v_type == V_RATIONAL) {
        Rational *r = static_cast<Rational*>(v.get());
        num = r->numerator;
        den = r->denominator;
    } else {
        throw RuntimeError("Wrong typename");
    }
}

enum ArithOp { OP_ADD, OP_SUB, OP_MUL, OP_DIV };

Value arith(const Value &a, const Value &b, ArithOp op) {
    if (a->v_type == V_INT && b->v_type == V_INT) {
        long long x = static_cast<Integer*>(a.get())->n;
        long long y = static_cast<Integer*>(b.get())->n;
        switch (op) {
            case OP_ADD: return IntegerV(static_cast<int>(x + y));
            case OP_SUB: return IntegerV(static_cast<int>(x - y));
            case OP_MUL: return IntegerV(static_cast<int>(x * y));
            case OP_DIV: return makeNumber(x, y);
        }
    }
    long long n1, d1, n2, d2;
    toFraction(a, n1, d1);
    toFraction(b, n2, d2);
    switch (op) {
        case OP_ADD: return makeNumber(n1 * d2 + n2 * d1, d1 * d2);
        case OP_SUB: return makeNumber(n1 * d2 - n2 * d1, d1 * d2);
        case OP_MUL: return makeNumber(n1 * n2, d1 * d2);
        case OP_DIV: return makeNumber(n1 * d2, d1 * n2);
    }
    throw RuntimeError("Unknown arithmetic operation");
}

// Left fold of a binary arithmetic operation over args[from..], starting from `acc`.
Value foldArith(Value acc, const std::vector<Value> &args, size_t from, ArithOp op) {
    for (size_t i = from; i < args.size(); i++) {
        acc = arith(acc, args[i], op);
    }
    return acc;
}

enum CmpOp { CMP_LT, CMP_LE, CMP_EQ, CMP_GE, CMP_GT };

bool cmpHolds(int c, CmpOp op) {
    switch (op) {
        case CMP_LT: return c < 0;
        case CMP_LE: return c <= 0;
        case CMP_EQ: return c == 0;
        case CMP_GE: return c >= 0;
        case CMP_GT: return c > 0;
    }
    return false;
}

Value compareChain(const std::vector<Value> &args, CmpOp op) {
    if (args.empty()) {
        throw RuntimeError("Wrong number of arguments for comparison");
    }
    long long n, d;
    for (const auto &v : args) {
        toFraction(v, n, d); // type check every argument
    }
    for (size_t i = 0; i + 1 < args.size(); i++) {
        if (!cmpHolds(compareNumericValues(args[i], args[i + 1]), op)) {
            return BooleanV(false);
        }
    }
    return BooleanV(true);
}

Pair *expectPair(const Value &v) {
    if (v->v_type != V_PAIR) {
        throw RuntimeError("Expected a pair");
    }
    return static_cast<Pair*>(v.get());
}

bool isDot(const Syntax &stx) {
    SymbolSyntax *sym = dynamic_cast<SymbolSyntax*>(stx.get());
    return sym != nullptr && sym->s == ".";
}

// Converts quoted syntax into a value; lists may be dotted as (a b . c).
Value syntaxToValue(const Syntax &stx) {
    SyntaxBase *s = stx.get();
    if (Number *num = dynamic_cast<Number*>(s)) return IntegerV(num->n);
    if (RationalSyntax *r = dynamic_cast<RationalSyntax*>(s)) return makeNumber(r->numerator, r->denominator);
    if (dynamic_cast<TrueSyntax*>(s)) return BooleanV(true);
    if (dynamic_cast<FalseSyntax*>(s)) return BooleanV(false);
    if (StringSyntax *str = dynamic_cast<StringSyntax*>(s)) return StringV(str->s);
    if (SymbolSyntax *sym = dynamic_cast<SymbolSyntax*>(s)) return SymbolV(sym->s);
    if (List *lst = dynamic_cast<List*>(s)) {
        const std::vector<Syntax> &xs = lst->stxs;
        size_t n = xs.size(), end = n;
        Value result = NullV();
        if (n >= 2 && isDot(xs[n - 2])) {
            if (n < 3) throw RuntimeError("Illegal dotted list");
            result = syntaxToValue(xs[n - 1]);
            end = n - 2;
        }
        for (size_t i = 0; i < end; i++) {
            if (isDot(xs[i])) throw RuntimeError("Illegal dotted list");
        }
        for (size_t i = end; i-- > 0;) {
            result = PairV(syntaxToValue(xs[i]), result);
        }
        return result;
    }
    throw RuntimeError("Unknown syntax in quote");
}

// Procedure values for primitives referenced as variables, e.g. ((if #t + -) 1 2).
Value primitiveProcedure(const std::string &name) {
    static std::unordered_map<std::string, Value> cache;
    auto cached = cache.find(name);
    if (cached != cache.end()) {
        return cached->second;
    }
    auto prim = primitives.find(name);
    if (prim == primitives.end()) {
        return Value(nullptr);
    }
    const std::vector<std::string> unary = {"x"}, binary = {"x", "y"};
    Expr x(new Var("x")), y(new Var("y"));
    std::vector<std::string> params;
    Expr body(nullptr);
    switch (prim->second) {
        case E_PLUS:    body = new PlusVar({}); break;
        case E_MINUS:   body = new MinusVar({}); break;
        case E_MUL:     body = new MultVar({}); break;
        case E_DIV:     body = new DivVar({}); break;
        case E_LT:      body = new LessVar({}); break;
        case E_LE:      body = new LessEqVar({}); break;
        case E_EQ:      body = new EqualVar({}); break;
        case E_GE:      body = new GreaterEqVar({}); break;
        case E_GT:      body = new GreaterVar({}); break;
        case E_LIST:    body = new ListFunc({}); break;
        case E_AND:     body = new AndVar({}); break;
        case E_OR:      body = new OrVar({}); break;
        case E_MODULO:  body = new Modulo(x, y); params = binary; break;
        case E_EXPT:    body = new Expt(x, y); params = binary; break;
        case E_CONS:    body = new Cons(x, y); params = binary; break;
        case E_SETCAR:  body = new SetCar(x, y); params = binary; break;
        case E_SETCDR:  body = new SetCdr(x, y); params = binary; break;
        case E_EQQ:     body = new IsEq(x, y); params = binary; break;
        case E_CAR:     body = new Car(x); params = unary; break;
        case E_CDR:     body = new Cdr(x); params = unary; break;
        case E_NOT:     body = new Not(x); params = unary; break;
        case E_BOOLQ:   body = new IsBoolean(x); params = unary; break;
        case E_INTQ:    body = new IsFixnum(x); params = unary; break;
        case E_NULLQ:   body = new IsNull(x); params = unary; break;
        case E_PAIRQ:   body = new IsPair(x); params = unary; break;
        case E_PROCQ:   body = new IsProcedure(x); params = unary; break;
        case E_SYMBOLQ: body = new IsSymbol(x); params = unary; break;
        case E_LISTQ:   body = new IsList(x); params = unary; break;
        case E_STRINGQ: body = new IsString(x); params = unary; break;
        case E_DISPLAY: body = new Display(x); params = unary; break;
        case E_VOID:    body = new MakeVoid(); break;
        case E_EXIT:    body = new Exit(); break;
        default: return Value(nullptr);
    }
    Value proc = ProcedureV(params, body, empty(), true);
    cache.emplace(name, proc);
    return proc;
}

// letrec: bind all names unassigned, evaluate every init in that scope, then assign.
Assoc letrecEnv(Letrec *node, Assoc env) {
    std::vector<AssocList*> slots;
    for (const auto &b : node->bind) {
        env = extend(b.first, Value(nullptr), env);
        slots.push_back(env.get());
    }
    std::vector<Value> values;
    for (const auto &b : node->bind) {
        values.push_back(b.second->eval(env));
    }
    for (size_t i = 0; i < slots.size(); i++) {
        slots[i]->v = values[i];
    }
    return env;
}

// Evaluates `cur` in `env`, iterating through tail positions and tail calls.
Value run(ExprBase *cur, Assoc env) {
    Value current_proc(nullptr); // keeps the running procedure's body alive
    for (;;) {
        switch (cur->e_type) {
            case E_IF: {
                If *node = static_cast<If*>(cur);
                cur = isTrue(node->cond->eval(env)) ? node->conseq.get() : node->alter.get();
                continue;
            }
            case E_COND: {
                Cond *node = static_cast<Cond*>(cur);
                ExprBase *next = nullptr;
                for (const auto &clause : node->clauses) {
                    Value test = clause[0]->eval(env);
                    if (!isTrue(test)) continue;
                    if (clause.size() == 1) return test;
                    for (size_t i = 1; i + 1 < clause.size(); i++) {
                        clause[i]->eval(env);
                    }
                    next = clause.back().get();
                    break;
                }
                if (next == nullptr) return SilentVoidV();
                cur = next;
                continue;
            }
            case E_BEGIN: {
                Begin *node = static_cast<Begin*>(cur);
                if (!node->defs.empty()) env = bindUnassigned(node->defs, env);
                if (node->es.empty()) return SilentVoidV();
                for (size_t i = 0; i + 1 < node->es.size(); i++) {
                    node->es[i]->eval(env);
                }
                cur = node->es.back().get();
                continue;
            }
            case E_AND: {
                AndVar *node = static_cast<AndVar*>(cur);
                if (node->rands.empty()) return BooleanV(true);
                for (size_t i = 0; i + 1 < node->rands.size(); i++) {
                    Value v = node->rands[i]->eval(env);
                    if (!isTrue(v)) return v;
                }
                cur = node->rands.back().get();
                continue;
            }
            case E_OR: {
                OrVar *node = static_cast<OrVar*>(cur);
                if (node->rands.empty()) return BooleanV(false);
                for (size_t i = 0; i + 1 < node->rands.size(); i++) {
                    Value v = node->rands[i]->eval(env);
                    if (isTrue(v)) return v;
                }
                cur = node->rands.back().get();
                continue;
            }
            case E_LET: {
                Let *node = static_cast<Let*>(cur);
                Assoc inner = env;
                for (const auto &b : node->bind) {
                    inner = extend(b.first, b.second->eval(env), inner);
                }
                env = inner;
                cur = node->body.get();
                continue;
            }
            case E_LETREC: {
                Letrec *node = static_cast<Letrec*>(cur);
                env = letrecEnv(node, env);
                cur = node->body.get();
                continue;
            }
            case E_APPLY: {
                Apply *node = static_cast<Apply*>(cur);
                Value proc = node->rator->eval(env);
                std::vector<Value> args;
                args.reserve(node->rand.size());
                for (const auto &r : node->rand) {
                    args.push_back(r->eval(env));
                }
                if (proc->v_type != V_PROC) {
                    throw RuntimeError("Attempt to apply a non-procedure");
                }
                Procedure *clos = static_cast<Procedure*>(proc.get());
                if (clos->is_primitive) {
                    ExprBase *body = clos->e.get();
                    if (Variadic *var = dynamic_cast<Variadic*>(body)) {
                        return var->evalRator(args);
                    }
                    if (body->e_type == E_AND || body->e_type == E_OR) {
                        bool isAnd = body->e_type == E_AND;
                        Value result = BooleanV(isAnd);
                        for (const auto &v : args) {
                            result = v;
                            if (isTrue(v) != isAnd) break;
                        }
                        return result;
                    }
                }
                if (args.size() != clos->parameters.size()) {
                    throw RuntimeError("Wrong number of arguments");
                }
                Assoc inner = clos->env;
                for (size_t i = 0; i < args.size(); i++) {
                    inner = extend(clos->parameters[i], args[i], inner);
                }
                env = inner;
                cur = clos->e.get();
                current_proc = proc;
                continue;
            }
            default:
                return cur->eval(env);
        }
    }
}

} // namespace

Value Fixnum::eval(Assoc &e) { // evaluation of a fixnum
    return IntegerV(n);
}

Value RationalNum::eval(Assoc &e) { // evaluation of a rational number
    return makeNumber(numerator, denominator);
}

Value StringExpr::eval(Assoc &e) { // evaluation of a string
    return StringV(s);
}

Value True::eval(Assoc &e) { // evaluation of #t
    return BooleanV(true);
}

Value False::eval(Assoc &e) { // evaluation of #f
    return BooleanV(false);
}

Value MakeVoid::eval(Assoc &e) { // (void)
    return VoidV();
}

Value Exit::eval(Assoc &e) { // (exit)
    return TerminateV();
}

Value Unary::eval(Assoc &e) { // evaluation of single-operator primitive
    return evalRator(rand->eval(e));
}

Value Binary::eval(Assoc &e) { // evaluation of two-operators primitive
    Value v1 = rand1->eval(e);
    return evalRator(v1, rand2->eval(e));
}

Value Variadic::eval(Assoc &e) { // evaluation of multi-operator primitive
    std::vector<Value> args;
    args.reserve(rands.size());
    for (const auto &r : rands) {
        args.push_back(r->eval(e));
    }
    return evalRator(args);
}

Value Var::eval(Assoc &e) { // evaluation of variable
    // Variable names can overlap with primitives and reserved words: local and
    // global bindings take precedence, then primitives evaluate to procedures.
    // Undefined (or not yet initialized) variables are a RuntimeError.
    if (Value *slot = lookup(x, e)) {
        if (slot->get() == nullptr) {
            throw RuntimeError("Variable used before initialization: " + x);
        }
        return *slot;
    }
    Value prim = primitiveProcedure(x);
    if (prim.get() != nullptr) {
        return prim;
    }
    throw RuntimeError("Undefined variable: " + x);
}

Value Plus::evalRator(const Value &rand1, const Value &rand2) { // +
    return arith(rand1, rand2, OP_ADD);
}

Value Minus::evalRator(const Value &rand1, const Value &rand2) { // -
    return arith(rand1, rand2, OP_SUB);
}

Value Mult::evalRator(const Value &rand1, const Value &rand2) { // *
    return arith(rand1, rand2, OP_MUL);
}

Value Div::evalRator(const Value &rand1, const Value &rand2) { // /
    return arith(rand1, rand2, OP_DIV);
}

Value Modulo::evalRator(const Value &rand1, const Value &rand2) { // modulo
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int dividend = dynamic_cast<Integer*>(rand1.get())->n;
        int divisor = dynamic_cast<Integer*>(rand2.get())->n;
        if (divisor == 0) {
            throw(RuntimeError("Division by zero"));
        }
        return IntegerV(dividend % divisor);
    }
    throw(RuntimeError("modulo is only defined for integers"));
}

Value PlusVar::evalRator(const std::vector<Value> &args) { // + with multiple args
    return foldArith(IntegerV(0), args, 0, OP_ADD);
}

Value MinusVar::evalRator(const std::vector<Value> &args) { // - with multiple args
    if (args.empty()) throw RuntimeError("Wrong number of arguments for -");
    if (args.size() == 1) return arith(IntegerV(0), args[0], OP_SUB);
    return foldArith(args[0], args, 1, OP_SUB);
}

Value MultVar::evalRator(const std::vector<Value> &args) { // * with multiple args
    return foldArith(IntegerV(1), args, 0, OP_MUL);
}

Value DivVar::evalRator(const std::vector<Value> &args) { // / with multiple args
    if (args.empty()) throw RuntimeError("Wrong number of arguments for /");
    if (args.size() == 1) return arith(IntegerV(1), args[0], OP_DIV);
    return foldArith(args[0], args, 1, OP_DIV);
}

Value Expt::evalRator(const Value &rand1, const Value &rand2) { // expt
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        int base = dynamic_cast<Integer*>(rand1.get())->n;
        int exponent = dynamic_cast<Integer*>(rand2.get())->n;
        
        if (exponent < 0) {
            throw(RuntimeError("Negative exponent not supported for integers"));
        }
        if (base == 0 && exponent == 0) {
            throw(RuntimeError("0^0 is undefined"));
        }
        
        long long result = 1;
        long long b = base;
        int exp = exponent;
        
        while (exp > 0) {
            if (exp % 2 == 1) {
                result *= b;
                if (result > INT_MAX || result < INT_MIN) {
                    throw(RuntimeError("Integer overflow in expt"));
                }
            }
            b *= b;
            if (b > INT_MAX || b < INT_MIN) {
                if (exp > 1) {
                    throw(RuntimeError("Integer overflow in expt"));
                }
            }
            exp /= 2;
        }
        
        return IntegerV((int)result);
    }
    throw(RuntimeError("Wrong typename"));
}

//A FUNCTION TO SIMPLIFY THE COMPARISON WITH INTEGER AND RATIONAL NUMBER
int compareNumericValues(const Value &v1, const Value &v2) {
    long long n1, d1, n2, d2;
    toFraction(v1, n1, d1);
    toFraction(v2, n2, d2);
    // denominators are positive, so cross-multiplication preserves the order
    long long left = n1 * d2, right = n2 * d1;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

Value Less::evalRator(const Value &rand1, const Value &rand2) { // <
    return BooleanV(compareNumericValues(rand1, rand2) < 0);
}

Value LessEq::evalRator(const Value &rand1, const Value &rand2) { // <=
    return BooleanV(compareNumericValues(rand1, rand2) <= 0);
}

Value Equal::evalRator(const Value &rand1, const Value &rand2) { // =
    return BooleanV(compareNumericValues(rand1, rand2) == 0);
}

Value GreaterEq::evalRator(const Value &rand1, const Value &rand2) { // >=
    return BooleanV(compareNumericValues(rand1, rand2) >= 0);
}

Value Greater::evalRator(const Value &rand1, const Value &rand2) { // >
    return BooleanV(compareNumericValues(rand1, rand2) > 0);
}

Value LessVar::evalRator(const std::vector<Value> &args) { // < with multiple args
    return compareChain(args, CMP_LT);
}

Value LessEqVar::evalRator(const std::vector<Value> &args) { // <= with multiple args
    return compareChain(args, CMP_LE);
}

Value EqualVar::evalRator(const std::vector<Value> &args) { // = with multiple args
    return compareChain(args, CMP_EQ);
}

Value GreaterEqVar::evalRator(const std::vector<Value> &args) { // >= with multiple args
    return compareChain(args, CMP_GE);
}

Value GreaterVar::evalRator(const std::vector<Value> &args) { // > with multiple args
    return compareChain(args, CMP_GT);
}

Value Cons::evalRator(const Value &rand1, const Value &rand2) { // cons
    return PairV(rand1, rand2);
}

Value ListFunc::evalRator(const std::vector<Value> &args) { // list function
    Value result = NullV();
    for (size_t i = args.size(); i-- > 0;) {
        result = PairV(args[i], result);
    }
    return result;
}

Value IsList::evalRator(const Value &rand) { // list?
    // Floyd cycle detection: a circular structure (via set-cdr!) is not a list
    ValueBase *slow = rand.get(), *fast = rand.get();
    for (;;) {
        for (int step = 0; step < 2; step++) {
            if (fast->v_type == V_NULL) return BooleanV(true);
            if (fast->v_type != V_PAIR) return BooleanV(false);
            fast = static_cast<Pair*>(fast)->cdr.get();
        }
        slow = static_cast<Pair*>(slow)->cdr.get();
        if (slow == fast) return BooleanV(false);
    }
}

Value Car::evalRator(const Value &rand) { // car
    return expectPair(rand)->car;
}

Value Cdr::evalRator(const Value &rand) { // cdr
    return expectPair(rand)->cdr;
}

Value SetCar::evalRator(const Value &rand1, const Value &rand2) { // set-car!
    expectPair(rand1)->car = rand2;
    return SilentVoidV();
}

Value SetCdr::evalRator(const Value &rand1, const Value &rand2) { // set-cdr!
    expectPair(rand1)->cdr = rand2;
    return SilentVoidV();
}

Value IsEq::evalRator(const Value &rand1, const Value &rand2) { // eq?
    // Check if type is Integer
    if (rand1->v_type == V_INT && rand2->v_type == V_INT) {
        return BooleanV((dynamic_cast<Integer*>(rand1.get())->n) == (dynamic_cast<Integer*>(rand2.get())->n));
    }
    // Check if type is Boolean
    else if (rand1->v_type == V_BOOL && rand2->v_type == V_BOOL) {
        return BooleanV((dynamic_cast<Boolean*>(rand1.get())->b) == (dynamic_cast<Boolean*>(rand2.get())->b));
    }
    // Check if type is Symbol
    else if (rand1->v_type == V_SYM && rand2->v_type == V_SYM) {
        return BooleanV((dynamic_cast<Symbol*>(rand1.get())->s) == (dynamic_cast<Symbol*>(rand2.get())->s));
    }
    // Check if type is Null or Void
    else if ((rand1->v_type == V_NULL && rand2->v_type == V_NULL) ||
             (rand1->v_type == V_VOID && rand2->v_type == V_VOID)) {
        return BooleanV(true);
    } else {
        return BooleanV(rand1.get() == rand2.get());
    }
}

Value IsBoolean::evalRator(const Value &rand) { // boolean?
    return BooleanV(rand->v_type == V_BOOL);
}

Value IsFixnum::evalRator(const Value &rand) { // number?
    return BooleanV(rand->v_type == V_INT);
}

Value IsNull::evalRator(const Value &rand) { // null?
    return BooleanV(rand->v_type == V_NULL);
}

Value IsPair::evalRator(const Value &rand) { // pair?
    return BooleanV(rand->v_type == V_PAIR);
}

Value IsProcedure::evalRator(const Value &rand) { // procedure?
    return BooleanV(rand->v_type == V_PROC);
}

Value IsSymbol::evalRator(const Value &rand) { // symbol?
    return BooleanV(rand->v_type == V_SYM);
}

Value IsString::evalRator(const Value &rand) { // string?
    return BooleanV(rand->v_type == V_STRING);
}

Value Begin::eval(Assoc &e) {
    return run(this, e);
}

Value Quote::eval(Assoc& e) {
    return syntaxToValue(s);
}

Value AndVar::eval(Assoc &e) { // and with short-circuit evaluation
    return run(this, e);
}

Value OrVar::eval(Assoc &e) { // or with short-circuit evaluation
    return run(this, e);
}

Value Not::evalRator(const Value &rand) { // not
    return BooleanV(!isTrue(rand));
}

Value If::eval(Assoc &e) {
    return run(this, e);
}

Value Cond::eval(Assoc &env) {
    return run(this, env);
}

Value Lambda::eval(Assoc &env) { 
    return ProcedureV(x, e, env);
}

Value Apply::eval(Assoc &e) {
    return run(this, e);
}

Value Define::eval(Assoc &env) {
    Value v = e->eval(env);
    if (env.get() == nullptr) {
        defineGlobal(var, v);
        return SilentVoidV();
    }
    // Internal definitions are pre-bound by the enclosing body; assign the nearest one.
    for (AssocList *i = env.get(); i != nullptr; i = i->next.get()) {
        if (i->x == var) {
            i->v = v;
            return SilentVoidV();
        }
    }
    env = extend(var, v, env);
    return SilentVoidV();
}

Value Let::eval(Assoc &env) {
    return run(this, env);
}

Value Letrec::eval(Assoc &env) {
    return run(this, env);
}

Value Set::eval(Assoc &env) {
    Value v = e->eval(env);
    Value *slot = lookup(var, env);
    if (slot == nullptr) {
        throw RuntimeError("set!: undefined variable " + var);
    }
    *slot = v;
    return SilentVoidV();
}

Value Display::evalRator(const Value &rand) { // display function
    if (rand->v_type == V_STRING) {
        String* str_ptr = dynamic_cast<String*>(rand.get());
        std::cout << str_ptr->s;
    } else {
        rand->show(std::cout);
    }
    
    return SilentVoidV();
}
