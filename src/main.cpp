#include "Def.hpp"
#include "syntax.hpp"
#include "expr.hpp"
#include "value.hpp"
#include "RE.hpp"
#include <sstream>
#include <iostream>
#include <map>
#include <pthread.h>

extern std::map<std::string, ExprType> primitives;
extern std::map<std::string, ExprType> reserved_words;

bool isExplicitVoidCall(Expr expr) {
    MakeVoid* make_void_expr = dynamic_cast<MakeVoid*>(expr.get());
    if (make_void_expr != nullptr) {
        return true;
    }
    
    Apply* apply_expr = dynamic_cast<Apply*>(expr.get());
    if (apply_expr != nullptr) {
        Var* var_expr = dynamic_cast<Var*>(apply_expr->rator.get());
        if (var_expr != nullptr && var_expr->x == "void") {
            return true;
        }
    }
    
    Begin* begin_expr = dynamic_cast<Begin*>(expr.get());
    if (begin_expr != nullptr && !begin_expr->es.empty()) {
        return isExplicitVoidCall(begin_expr->es.back());
    }
    
    If* if_expr = dynamic_cast<If*>(expr.get());
    if (if_expr != nullptr) {
        return isExplicitVoidCall(if_expr->conseq) || isExplicitVoidCall(if_expr->alter);
    }
    
    Cond* cond_expr = dynamic_cast<Cond*>(expr.get());
    if (cond_expr != nullptr) {
        for (const auto& clause : cond_expr->clauses) {
            if (clause.size() > 1 && isExplicitVoidCall(clause.back())) {
                return true;
            }
        }
    }
    return false;
}

void REPL(){
    // read - evaluation - print loop
    Assoc global_env = empty();
    while (1){
        #ifndef ONLINE_JUDGE
            std::cout << "scm> ";
        #endif
        if (readSpace(std :: cin).peek() == EOF)
            break;
        Syntax stx = readSyntax(std :: cin); // read
        try{
            Expr expr = stx -> parse(global_env); // parse
            // stx -> show(std :: cout); // syntax print
            Value val = expr -> eval(global_env);
            if (val -> v_type == V_TERMINATE)
                break;
            // Commands such as define, set!, display, (begin) print nothing; void values print #<void>
            bool silent = val -> v_type == V_VOID && static_cast<Void*>(val.get()) -> silent;
            if (!silent || isExplicitVoidCall(expr))
                val -> show(std :: cout); // value print
        }
        catch (const RuntimeError &RE){
            // std :: cout << RE.message();
            std :: cout << "RuntimeError";
        }
        std :: cout << std :: endl; // flush so earlier results survive a later crash
    }
}

static void *replThread(void *) {
    REPL();
    return nullptr;
}

int main(int argc, char *argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    // Deeply recursive Scheme programs need far more stack than the default,
    // so run the interpreter on a thread with a large stack when possible.
    const size_t stack_sizes[] = {size_t(256) << 20, size_t(64) << 20};
    for (size_t size : stack_sizes) {
        pthread_attr_t attr;
        pthread_t tid;
        pthread_attr_init(&attr);
        bool started = pthread_attr_setstacksize(&attr, size) == 0 &&
                       pthread_create(&tid, &attr, replThread, nullptr) == 0;
        pthread_attr_destroy(&attr);
        if (started) {
            pthread_join(tid, nullptr);
            return 0;
        }
    }
    REPL();
    return 0;
}
