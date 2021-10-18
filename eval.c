#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    UNDEFINED, // returned by assoc and lookup if key is not found
    SYM,
    INT,
    PAIR,
    BUILTIN,
    FUNC,
};
typedef enum Type Type;

typedef struct Value Value;

struct Pair {
    Value *car;
    Value *cdr;
};
typedef struct Pair Pair;

typedef struct Env Env;
struct Env {
    Env *parent;
    Value *bindings;
};

struct Func {
    Value *name;
    Value *params;
    Value *body;
    Env *env;
};
typedef struct Func Func;

typedef Value *(*imp)(Value *args);

struct Builtin {
    char *name;
    imp f;
};
typedef struct Builtin Builtin;

struct Value {
    Type type;
    union {
        char *sym;
        long long n;
        Pair pair;
        Func func;
        Builtin builtin;
    };
};

void *
xalloc(size_t size)
{
    void *p = calloc(1, size);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

Value *
alloc(Type t)
{
    Value *v = xalloc(sizeof(Value));
    v->type = t;
    return v;
}

#define ERRLEN 1024

int
is_nil(Value *v) {
    return v == NULL;
}

int
is_sym(Value *v) {
    return !is_nil(v) && v->type == SYM;
}

int
is_int(Value *v) {
    return !is_nil(v) && v->type == INT;
}

int
is_pair(Value *v) {
    return !is_nil(v) && v->type == PAIR;
}

int
is_func(Value *v) {
    return !is_nil(v) && v->type == FUNC;
}

int
is_builtin(Value *v) {
    return !is_nil(v) && v->type == BUILTIN;
}

Value *
car(Value *v)
{
    if (is_pair(v)) {
        return v->pair.car;
    } else {
        return NULL;
    }
}

Value *
cdr(Value *v)
{
    if (is_pair(v)) {
        return v->pair.cdr;
    } else {
        return NULL;
    }
}

Value *
caar(Value *v)
{
    return car(car(v));
}

Value *
cadr(Value *v)
{
    return car(cdr(v));
}

Value *
cddr(Value *v)
{
    return cdr(cdr(v));
}

Value *
cadar(Value *v)
{
    return car(cdr(car(v)));
}

Value *
caddr(Value *v)
{
    return car(cdr(cdr(v)));
}

Value *
cdddr(Value *v)
{
    return cdr(cdr(cdr(v)));
}

Value *
caddar(Value *v)
{
    return car(cdr(cdr(car(v))));
}

Value *
cons(Value *car, Value *cdr)
{
    Value *v = alloc(PAIR);
    v->pair.car = car;
    v->pair.cdr = cdr;
    return v;
}

Value *
mkint(long long n)
{
    Value *v = alloc(INT);
    v->n = n;
    return v;
}

Value *
mkfunc(Value *params, Value *body, Env *env)
{
    Value *v = alloc(FUNC);
    v->func.name = NULL;
    v->func.params = params;
    v->func.body = body;
    v->func.env = env;
    return v;
}

Value *
mkbuiltin(char *name, imp f)
{
    Value *v = alloc(BUILTIN);
    v->builtin.name = name;
    v->builtin.f = f;
    return v;
}

Value *symtab = NULL;

Value *
intern(char *s)
{
    Value *l = symtab;

    assert(is_nil(l) || is_pair(l));

    while (!is_nil(l)) {
        Value *sym = l->pair.car;
        assert(is_sym(sym));

        if (strcmp(sym->sym, s) == 0) {
            return sym;
        }

        l = l->pair.cdr;
    }

    Value *v = alloc(SYM);

    size_t len = strlen(s);
    char *s1 = xalloc(len + 1);
    v->sym = strncpy(s1, s, len);

    symtab = cons(v, symtab);

    return v;
}

void
fprint0(FILE *stream, Value *v, int depth)
{
    if (is_nil(v)) {
        fprintf(stream, "nil");
    } else if (is_sym(v)) {
        fprintf(stream, "%s", v->sym);
    } else if (is_int(v)) {
        fprintf(stream, "%lld", v->n);
    } else if (is_func(v)) {
        fprintf(stream, "#<function ");
        if (v->func.name != NULL) {
            fprintf(stream, "%s", v->func.name->sym);
        } else {
            fprintf(stream, "(anonymous)");
        }
        fprintf(stream, ">");
    } else if (is_builtin(v)) {
        fprintf(stream, "#<builtin %s>", v->builtin.name);
    } else if (is_pair(v)){
        fprintf(stream, "(");
        fprint0(stream, v->pair.car, depth+1);

        Value *cdr = v->pair.cdr;
        while (is_pair(cdr)) {
            fprintf(stream, " ");
            fprint0(stream, cdr->pair.car, depth+1);
            cdr = cdr->pair.cdr;
        }

        if (!is_nil(cdr)) {
            fprintf(stream, " . ");
            fprint0(stream, cdr, depth+1);
        }

        fprintf(stream, ")");
    } else {
        fprintf(stdout, "print: unknown type\n");
        exit(1);
    }

    if (depth == 0) {
        fprintf(stream, "\n");
    }
}

void
fprint(FILE *stream, Value *v)
{
    fprint0(stream, v, 0);
}

void
print(Value *v)
{
    fprint(stdout, v);
}

int
peek(FILE *stream)
{
    int c = fgetc(stream);
    if (c == EOF) {
        return EOF;
    }

    int res = ungetc(c, stream);
    if (res == EOF) {
        fprintf(stderr, "couldn't peek\n");
        exit(1);
    }
    return c;
}

int
xungetc(int c, FILE *stream)
{
    int res = ungetc(c, stream);
    if (res == EOF) {
        fprintf(stderr, "couldn't ungetc\n");
        exit(1);
    }
    return res;
}

void
skipspace(FILE *stream)
{
    int c;
    while ((c = peek(stream)) != EOF && isspace(c)) {
        fgetc(stream);
    }
}

Value *read1(FILE *stream);

Value *
readlist(FILE *stream, int first)
{
    skipspace(stream);

    int c = peek(stream);

    if (c == EOF) {
        fprintf(stderr, "expected value or ')' but got EOF\n");
        exit(1);
    } else if (c == ')') {
        // TODO: error handling
        fgetc(stream);
        return NULL;
    } else if (c == '.' && !first) {
        // TODO: error handling
        c = fgetc(stream);

        Value *cdr = read1(stream);
        skipspace(stream);
        int c = fgetc(stream);
        if (c != ')') {
            fprintf(stderr, "expected ')' but got '%c'\n", c);
            exit(1);
        }

        return cdr;
    } else {
        Value *car = read1(stream);
        Value *cdr = readlist(stream, 0);
        return cons(car, cdr);
    }
}

int
is_symchar(int c)
{
    return !isspace(c) && c != '(' && c != ')' && c != '.';
}

int
is_symstart(int c)
{
    return is_symchar(c) && !isdigit(c);
}

#define MAX_INTLEN 20 // includes optional leading -
#define MAX_SYMLEN 1024

long long
parseint(char *s)
{
    long long n = strtol(s, NULL, 10);
    if (errno == ERANGE) {
        fprintf(stderr, "integer too big '%s'\n", s);
        exit(1);
    }
    return n;
}

Value *
read1(FILE *stream)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(') {
        return readlist(stream, 1);
    } else if (c == '\'') {
        return cons(intern("quote"), cons(read1(stream), NULL));
    } else if ((c == '-' && isdigit(peek(stream))) || isdigit(c)) {
        char buf[MAX_INTLEN+1];

        int i = 0;
        do {
            if (i == MAX_INTLEN) {
                fprintf(stderr, "integer too long\n");
                exit(1);
            }
            buf[i++] = c;
            c = fgetc(stream);
        } while (isdigit(c));

        buf[i] = '\0';
        xungetc(c, stream);

        return mkint(parseint(buf));
    } else if (is_symstart(c)) {
        char buf[MAX_SYMLEN+1];

        int i;
        for (i = 0; is_symchar(c); i++) {
            buf[i] = c;
            c = fgetc(stream);

            if (i == MAX_SYMLEN) {
                fprintf(stderr, "symbol too long\n");
                exit(1);
            }
        }

        buf[i] = '\0';
        xungetc(c, stream);

        return intern(buf);
    } else {
        fprintf(stderr, "unexpected character: %c\n", c);
        exit(1);
    }
}

Value *undefined;

Value *
assoc(Value *v, Value *l)
{
    if (!is_pair(l) && !is_nil(l)) {
        fprintf(stderr, "assoc: expected list\n");
        exit(1);
    }

    while (!is_nil(l)) {
        if (caar(l) == v) {
            return cadar(l);
        }

        l = cdr(l);
    }

    return undefined;
}

Value *
append(Value *x, Value *y)
{
    if (is_nil(x)) {
        return y;
    } else {
        return cons(car(x), append(cdr(x), y));
    }
}

int
length(Value *l)
{
    if (!is_pair(l)) {
        return 0;
    }

    int len = 0;
    for (; l != NULL; l = cdr(l)) {
        len++;
    }

    return len;
}

void
checkargs(Value *name, Value *params, Value *args)
{
    // if it's a symbol, capture everything
    if (is_sym(params)) {
        return;
    }

    int nargs = 0;
    int varargs = 0;

    for (Value *p = params; is_pair(p); p = cdr(p)) {
        nargs += 1;

        if (is_sym(cdr(p))) {
            varargs = 1;
        }
    }

    int len = length(args);

    if (len < nargs && varargs) {
        fprintf(stderr, "%s: expected %d or more arguments, got %d\n", name->sym, nargs, len);
        exit(1);
    } else if (len != nargs && !varargs) {
        fprintf(stderr, "%s: expected %d arguments, got %d\n", name->sym, nargs, len);
        exit(1);
    }
}

Value *
zipargs(Value *x, Value *y)
{
    if (is_nil(x) && is_nil(y)) {
        return NULL;
    } else if (is_sym(x)) {
        return cons(cons(x, cons(y, NULL)), NULL);
    } else if (is_pair(x) && is_pair(y)) {
        return cons(cons(car(x), cons(car(y), NULL)), zipargs(cdr(x), cdr(y)));
    } else if (is_nil(x) || is_nil(y)) {
        fprintf(stderr, "zipargs: lists not the same length\n");
        exit(1);
    } else {
        fprintf(stderr, "zipargs: expected list\n");
        exit(1);
    }
}

Env *
clone(Env *env)
{
    Env *newenv = xalloc(sizeof(Env));
    newenv->parent = env;
    newenv->bindings = NULL;
    return newenv;
}

Value *
lookup(Value *name, Env *env)
{
    Value *v;

    for (; env != NULL; env = env->parent) {
        v = assoc(name, env->bindings);
        if (v != undefined) {
            return v;
        }
    }

    return undefined;
}

void
setname(Value *name, Value *value)
{
    if (is_func(value)) {
        value->func.name = name;
    }
}

Value *
def(Value *name, Value *value, Env *env)
{
    env->bindings = cons(cons(name, cons(value, NULL)), env->bindings);
    setname(name, value);
    return value;
}

Value *eval(Value *v, Env *env);

Value *
evcon(Value *conditions, Env *env)
{
    assert(is_pair(conditions) || is_nil(conditions));

    if (eval(caar(conditions), env)) {
        return eval(cadar(conditions), env);
    } else {
        return evcon(cdr(conditions), env);
    }
}

Value *
evlis(Value *params, Env *env)
{
    assert(is_pair(params) || is_nil(params));

    if (is_nil(params)) {
        return NULL;
    } else {
        return cons(eval(car(params), env), evlis(cdr(params), env));
    }
}

Env *globals = NULL;

Value *s_t;
Value *t; // s_t
Value *s_fn;
Value *s_quote;
Value *s_cond;
Value *s_def;

Value *
eval(Value *v, Env *env)
{
    if (is_pair(v) && car(v) == s_quote) {
        return cadr(v);
    } else if (is_pair(v) && car(v) == s_cond) {
        return evcon(cdr(v), env);
    } else if (is_pair(v) && car(v) == s_fn) {
        return mkfunc(cadr(v), cddr(v), env);
    } else if (is_pair(v) && car(v) == s_def && length(v) > 3) {
        // short form lambda definition, e.g. (def inc (x) (+ 1 x))
        return eval(cons(s_def, cons(cadr(v), cons(cons(s_fn, cons(caddr(v), cdddr(v))), NULL))), env);
    } else if (is_pair(v) && car(v) == s_def) {
        Value *name = cadr(v);
        Value *val = eval(caddr(v), env);

        if (!is_sym(name)) {
            fprintf(stderr, "def: expected symbol\n");
            exit(1);
        }

        Value *old = lookup(name, globals);
        if (old != undefined) {
            fprintf(stderr, "def: symbol already defined: ");
            fprint(stderr, name);
            exit(1);
        }

        return def(name, val, globals);
    } else if (is_pair(v)) {
        Value *f = eval(car(v), env);

        if (is_func(f)) {
            checkargs(f->func.name, f->func.params, cdr(v));
            Value *bindings = zipargs(f->func.params, evlis(cdr(v), env));
            Env *newenv = clone(f->func.env);
            newenv->bindings = bindings;

            Value *res;
            for (Value *e = f->func.body; is_pair(e); e = cdr(e)) {
                res = eval(car(e), newenv);
            }

            return res;
        } else if (is_builtin(f)) {
            return f->builtin.f(evlis(cdr(v), env));
        } else {
            fprintf(stderr, "not a function: ");
            fprint(stderr, car(v));
            exit(1);
        }
    } else if (is_sym(v)) {
        Value *res = lookup(v, env);

        if (res != undefined) {
            return res;
        } else {
            fprintf(stderr, "unbound variable: %s\n", v->sym);
            exit(1);
        }
    } else {
        return v;
    }
}

int
is_eq(Value *x, Value *y)
{
    return x == y;
}

void
arity(Value *args, int expected, char *name)
{
    int actual = length(args);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %d arguments, got %d\n", name, expected, actual);
        exit(1);
    }
}

void
varity(Value *args, int min, char *name)
{
    int actual = length(args);

    if (actual < min) {
        fprintf(stderr, "%s: expected %d or more arguments, got %d\n", name, min, actual);
        exit(1);
    }
}

int
allints(Value *l)
{
    for (; l != NULL; l = cdr(l)) {
        if (!is_int(car(l))) {
            return 0;
        }
    }

    return 1;
}

#define builtin1(name) \
    Value *builtin_##name(Value *args) { \
        arity(args, 1, #name); \
        return name(car(args)); \
    }

#define builtin2(name) \
    Value *builtin_##name(Value *args) { \
        arity(args, 2, #name); \
        return name(car(args), cadr(args)); \
    }

#define pred1(name) \
    Value *builtin_is_##name(Value *args) { \
        arity(args, 1, #name); \
        return is_##name(car(args)) ? s_t : NULL; \
    }

#define pred2(name) \
    Value *builtin_is_##name(Value *args) { \
        arity(args, 2, #name); \
        return is_##name(car(args), cadr(args)) ? s_t : NULL; \
    }

#define op(op, name, init) \
    Value *builtin_##name(Value *args) { \
        if (!allints(args)) return NULL; \
        long long res = init; \
        for (; args != NULL; args = cdr(args)) { \
            res = res op car(args)->n; \
        } \
        return mkint(res); \
    }

#define comp(op, name) \
    Value *builtin_##name(Value *args) { \
        if (!allints(args)) return NULL; \
        if (length(args) == 0) return t; \
        Value *last = car(args); \
        for (args = cdr(args); args != NULL; args = cdr(args)) { \
            if (!(last->n op car(args)->n)) { \
                return NULL; \
            } \
            last = car(args); \
        } \
        return t; \
    }

builtin1(car)
builtin1(cdr)
builtin2(cons)

Value *
builtin_length(Value *args)
{
    arity(args, 1, "length");
    return mkint(length(car(args)));
}

pred1(nil)
pred1(sym)
pred1(int)
pred1(pair)
pred1(func)
pred1(builtin)
pred2(eq)

op(+, plus, 0)
op(-, minus, 0)
op(*, times, 1)

Value *
builtin_divide(Value *args)
{
    varity(args, 1, "/");

    if (!allints(args)) return NULL;

    if (car(args)->n == 0) {
        fprintf(stderr, "division by zero\n");
        exit(1);
    } else if (length(args) == 1) {
        // In CL and Scheme, (/ 5) => 1/5, but we don't support
        // rationals, so we round down.
        return mkint(0);
    }

    long long res = car(args)->n;
    for (args = cdr(args); args != NULL; args = cdr(args)) {
        res /= car(args)->n;
    }

    return mkint(res);
}

comp(>, gt)
comp(<, lt)
comp(>=, ge)
comp(<=, le)

#define symbol(name) s_##name = intern(#name)
#define def_builtin(name) def(intern(#name), mkbuiltin(#name, builtin_##name), globals)
#define def_pred(name) def(intern(#name "?"), mkbuiltin(#name "?", builtin_is_##name), globals)
#define def_op(op, name) def(intern(#op), mkbuiltin(#op, builtin_##name), globals)

int
main(int argc, char *argv[])
{
    undefined = alloc(UNDEFINED);

    globals = clone(NULL);

    symbol(t); t = s_t;
    def(t, t, globals);
    def(intern("nil"), NULL, globals);

    symbol(quote);
    symbol(cond);
    symbol(fn);
    symbol(def);

    def_builtin(car);
    def_builtin(cdr);
    def_builtin(cons);
    def_builtin(length);

    def_pred(nil);
    def_pred(sym);
    def_pred(int);
    def_pred(pair);
    def_pred(func);
    def_pred(builtin);

    def_pred(eq);

    def_op(+, plus);
    def_op(-, minus);
    def_op(*, times);
    def_op(/, divide);

    def_op(>, gt);
    def_op(<, lt);
    def_op(>=, ge);
    def_op(<=, le);

    while (peek(stdin) != EOF) {
        Value *v = read1(stdin);
        v = eval(v, globals);
        print(v);
    }
    return 0;
}
