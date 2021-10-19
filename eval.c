#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    SYMBOL,
    STRING,
    INTEGER,
    PAIR,
    BUILTIN,
    FUNCTION,
};
typedef enum Type Type;

typedef struct Value Value;

struct Buf {
    char *s;
    size_t len;
    size_t cap;
};
typedef struct Buf Buf;

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

typedef Value *(*Imp)(Value *args);

struct Builtin {
    char *name;
    Imp imp;
};
typedef struct Builtin Builtin;

struct Value {
    Type type;
    union {
        char *sym;
        Buf *str;
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

void *
xrealloc(void *p, size_t size)
{
    p = realloc(p, size);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

Buf *
binit(char *s)
{
    Buf *b = xalloc(sizeof(Buf));
    b->len = strlen(s);
    b->cap = b->len + 1;
    b->s = xalloc(b->cap);
    strncpy(b->s, s, b->len);
    return b;
}

Buf *
bappend(Buf *b, char *s)
{
    size_t len = strlen(s);
    if (b->len + len + 1 > b->cap) {
        b->cap = b->len + len + 1;
        b->s = xrealloc(b->s, b->cap);
    }
    strncpy(b->s + b->len, s, len);
    b->len += len;
    return b;
}

Buf *
bputc(Buf *b, char c)
{
    if (b->len + 1 > b->cap) {
        b->cap *= 2;
        b->s = xrealloc(b->s, b->cap);
    }
    b->s[b->len++] = c;
    return b;
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
is_symbol(Value *v) {
    return !is_nil(v) && v->type == SYMBOL;
}

int
is_string(Value *v) {
    return !is_nil(v) && v->type == STRING;
}

int
is_integer(Value *v) {
    return !is_nil(v) && v->type == INTEGER;
}

int
is_pair(Value *v) {
    return !is_nil(v) && v->type == PAIR;
}

int
is_function(Value *v) {
    return !is_nil(v) && v->type == FUNCTION;
}

int
is_builtin(Value *v) {
    return !is_nil(v) && v->type == BUILTIN;
}

int
is_procedure(Value *v) {
    return is_function(v) || is_builtin(v);
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
    Value *v = alloc(INTEGER);
    v->n = n;
    return v;
}

Value *
mkstring(Buf *b)
{
    Value *v = alloc(STRING);
    v->str = b;
    return v;
}

Value *
mkfunc(Value *params, Value *body, Env *env)
{
    Value *v = alloc(FUNCTION);
    v->func.name = NULL;
    v->func.params = params;
    v->func.body = body;
    v->func.env = env;
    return v;
}

Value *
mkbuiltin(char *name, Imp imp)
{
    Value *v = alloc(BUILTIN);
    v->builtin.name = name;
    v->builtin.imp = imp;
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
        assert(is_symbol(sym));

        if (strcmp(sym->sym, s) == 0) {
            return sym;
        }

        l = l->pair.cdr;
    }

    Value *v = alloc(SYMBOL);

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
    } else if (is_symbol(v)) {
        fprintf(stream, "%s", v->sym);
    } else if (is_integer(v)) {
        fprintf(stream, "%lld", v->n);
    } else if (is_string(v)) {
        fprintf(stream, "\"%s\"", v->str->s);
    } else if (is_function(v)) {
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
    } else if (c == '"') {
        Buf *b = binit("");

        while (1) {
            c = fgetc(stream);
            if (c == EOF) {
                fprintf(stderr, "unterminated string\n");
                exit(1);
            } else if (c == '"') {
                break;
            } else if (c == '\\') {
                c = fgetc(stream);
                if (c == EOF) {
                    fprintf(stderr, "unterminated string\n");
                    exit(1);
                } else if (c == 'n') {
                    c = '\n';
                } else if (c == 't') {
                    c = '\t';
                } else if (c == 'r') {
                    c = '\r';
                } else if (c == '\\') {
                    c = '\\';
                } else if (c == '"') {
                    c = '"';
                } else {
                    fprintf(stderr, "unknown escape sequence '\\%c'\n", c);
                    exit(1);
                }
            }

            bputc(b, c);
        }

        return mkstring(b);
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

int
is_eq(Value *x, Value *y)
{
    return x == y;
}

int
is_eqv(Value *x, Value *y)
{
    if (is_integer(x) && is_integer(y)) {
        return x->n == y->n;
    } else {
        return is_eq(x, y);
    }
}

int
is_equal(Value *x, Value *y)
{
    if (is_pair(x) && is_pair(y)) {
        return is_equal(car(x), car(y)) && is_equal(cdr(x), cdr(y));
    } else {
        return is_eqv(x, y);
    }
}

Value *
assoc(Value *v, Value *l)
{
    if (!is_pair(l) && !is_nil(l)) {
        fprintf(stderr, "assoc: expected list\n");
        exit(1);
    }

    while (!is_nil(l)) {
        if (is_equal(caar(l), v)) {
            return car(l);
        }

        l = cdr(l);
    }

    return NULL;
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
    if (is_symbol(params)) {
        return;
    }

    int nargs = 0;
    int varargs = 0;

    for (Value *p = params; is_pair(p); p = cdr(p)) {
        nargs += 1;

        if (is_symbol(cdr(p))) {
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
    } else if (is_symbol(x)) {
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
        if (v) {
            return v;
        }
    }

    return NULL;
}

void
setname(Value *name, Value *value)
{
    if (is_function(value)) {
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

Value *
set(Value *name, Value *value, Env *env)
{
    Value *binding = lookup(name, env);
    if (binding == NULL) {
        fprintf(stderr, "set: undefined variable: %s\n", name->sym);
        exit(1);
    }

    binding->pair.cdr = cons(value, NULL);

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

Value *
evlet(Value *bindings, Env *env)
{
    assert(is_pair(bindings) || is_nil(bindings));

    if (is_nil(bindings)) {
        return NULL;
    } else {
        Value *b = car(bindings);
        Value *name = car(b);
        Value *value = eval(cadr(b), env);

        return cons(cons(name, cons(value, NULL)), evlet(cdr(bindings), env));
    }
}

Value *
evletstar(Value *bindings, Env *env)
{
    assert(is_pair(bindings) || is_nil(bindings));

    if (is_nil(bindings)) {
        return NULL;
    } else {
        Value *b = car(bindings);
        Value *name = car(b);
        Value *value = eval(cadr(b), env);

        env = clone(env);
        def(name, value, env);

        return cons(cons(name, cons(value, NULL)), evletstar(cdr(bindings), env));
    }
}

Env *globals = NULL;

Value *s_t;
Value *t; // s_t
Value *s_fn;
Value *s_quote;
Value *s_cond;
Value *s_def;
Value *s_set;
Value *s_let;
Value *s_letstar;

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

        if (!is_symbol(name)) {
            fprintf(stderr, "def: expected symbol\n");
            exit(1);
        }

        Value *old = lookup(name, globals);
        if (old) {
            fprintf(stderr, "def: symbol already defined: ");
            fprint(stderr, name);
            exit(1);
        }

        return def(name, val, globals);
    } else if (is_pair(v) && car(v) == s_set) {
        Value *lvar = cadr(v);
        Value *val = eval(caddr(v), env);

        if (!is_symbol(lvar)) {
            fprintf(stderr, "set: expected symbol\n");
            exit(1);
        }

        return set(lvar, val, env);
    } else if (is_pair(v) && car(v) == s_let) {
        Value *bindings = cadr(v);
        Value *exprs = cddr(v);

        if (!is_pair(bindings)) {
            fprintf(stderr, "let: expected list, got: \n");
            fprint(stderr, bindings);
            exit(1);
        }

        Env *newenv = clone(env);
        newenv->bindings = evlet(bindings, env);

        Value *res = NULL;
        for (Value *e = exprs; is_pair(e); e = cdr(e)) {
            res = eval(car(e), newenv);
        }

        return res;
    } else if (is_pair(v) && car(v) == s_letstar) {
        Value *bindings = cadr(v);
        Value *exprs = cddr(v);

        if (!is_pair(bindings)) {
            fprintf(stderr, "let*: expected list, got: \n");
            fprint(stderr, bindings);
            exit(1);
        }

        Env *newenv = clone(env);
        newenv->bindings = evletstar(bindings, env);

        Value *res = NULL;
        for (Value *e = exprs; is_pair(e); e = cdr(e)) {
            res = eval(car(e), newenv);
        }

        return res;
    } else if (is_pair(v)) {
        Value *f = eval(car(v), env);

        if (is_function(f)) {
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
            return f->builtin.imp(evlis(cdr(v), env));
        } else {
            fprintf(stderr, "not a function: ");
            fprint(stderr, car(v));
            exit(1);
        }
    } else if (is_symbol(v)) {
        Value *binding = lookup(v, env);

        if (binding) {
            return cadr(binding);
        } else {
            fprintf(stderr, "unbound variable: %s\n", v->sym);
            exit(1);
        }
    } else {
        return v;
    }
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
        if (!is_integer(car(l))) {
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
        if (!allints(args)) { return NULL; } \
        int len = length(args); \
        if (len == 0) { return mkint(init op init); } \
        if (len == 1) { return mkint(init op car(args)->n); } \
        long long res = car(args)->n; \
        for (args = cdr(args); args != NULL; args = cdr(args)) { \
            res = res op car(args)->n; \
        } \
        return mkint(res); \
    }

#define comp(op, name) \
    Value *builtin_##name(Value *args) { \
        if (!allints(args)) { return NULL; } \
        if (length(args) == 0) { return t; } \
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
pred1(symbol)
pred1(string)
pred1(integer)
pred1(pair)
pred1(function)
pred1(builtin)
pred1(procedure)

pred2(eq)
pred2(eqv)
pred2(equal)

op(+, plus, 0)
op(-, minus, 0)
op(*, times, 1)
op(/, divide_, 1)

Value *
builtin_divide(Value *args)
{
    varity(args, 1, "/");

    for (Value *a = args; is_pair(a); a = cdr(a)) {
        if (car(a)->n == 0) {
            fprintf(stderr, "/: division by zero\n");
            exit(1);
        }
    }

    return builtin_divide_(args);
}

comp(>, gt)
comp(<, lt)
comp(>=, ge)
comp(<=, le)
// == for the c operator, but = in def_op below for Lisp.
// different from is_eq, which is eq?
comp(==, eq)

#define symbol(name) s_##name = intern(#name)
#define def_builtin(name) def(intern(#name), mkbuiltin(#name, builtin_##name), globals)
#define def_pred(name) def(intern(#name "?"), mkbuiltin(#name "?", builtin_is_##name), globals)
#define def_op(op, name) def(intern(#op), mkbuiltin(#op, builtin_##name), globals)

int
main(int argc, char *argv[])
{
    globals = clone(NULL);

    symbol(t); t = s_t; // return t seems more ergonomic and clear than return s_t
    def(t, t, globals);
    def(intern("nil"), NULL, globals);

    symbol(quote);
    symbol(cond);
    symbol(fn);
    symbol(def);
    symbol(set);
    symbol(let);
    s_letstar = intern("let*");

    def_builtin(car);
    def_builtin(cdr);
    def_builtin(cons);
    def_builtin(length);

    def_pred(nil);
    def_pred(symbol);
    def_pred(string);
    def_pred(integer);
    def_pred(pair);
    def_pred(function);
    def_pred(builtin);
    def_pred(procedure);

    def_pred(eq);
    def_pred(eqv);
    def_pred(equal);

    def_op(+, plus);
    def_op(-, minus);
    def_op(*, times);
    def_op(/, divide);

    def_op(>, gt);
    def_op(<, lt);
    def_op(>=, ge);
    def_op(<=, le);
    // = for Lisp land, but == above in comp for the C operator.
    def_op(=, eq);

    while (peek(stdin) != EOF) {
        Value *v = read1(stdin);
        v = eval(v, globals);
        print(v);
    }
    return 0;
}
