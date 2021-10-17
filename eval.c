#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    SYM,
    INT,
    LIST,
    FUNC
};
typedef enum Type Type;

typedef struct Value Value;
typedef struct List List;

struct List {
    Value *car;
    Value *cdr;
};

struct Func {
    Value *params;
    Value *body;
    Value *env;
};
typedef struct Func Func;

struct Value {
    Type type;
    union {
        char *sym;
        int n;
        List list;
        Func func;
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
is_list(Value *v) {
    return !is_nil(v) && v->type == LIST;
}

int
is_func(Value *v) {
    return !is_nil(v) && v->type == FUNC;
}

Value *
car(Value *v)
{
    if (is_list(v)) {
        return v->list.car;
    } else {
        return NULL;
    }
}

Value *
cdr(Value *v)
{
    if (is_list(v)) {
        return v->list.cdr;
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
caddar(Value *v)
{
    return car(cdr(cdr(car(v))));
}

Value *
cons(Value *car, Value *cdr)
{
    Value *v = alloc(LIST);
    v->list.car = car;
    v->list.cdr = cdr;
    return v;
}

Value *
mkint(int n)
{
    Value *v = alloc(INT);
    v->n = n;
    return v;
}

Value *
mkfunc(Value *params, Value *body, Value *env)
{
    Value *v = alloc(FUNC);
    v->func.params = params;
    v->func.body = body;
    v->func.env = env;
    return v;
}

Value *symtab = NULL;

Value *
intern(char *s)
{
    Value *l = symtab;

    assert(is_nil(l) || is_list(l));

    while (!is_nil(l)) {
        Value *sym = l->list.car;
        assert(is_sym(sym));

        if (strcmp(sym->sym, s) == 0) {
            return sym;
        }

        l = l->list.cdr;
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
        fprintf(stream, "()");
    } else if (is_sym(v)) {
        fprintf(stream, "%s", v->sym);
    } else if (is_int(v)) {
        fprintf(stream, "%d", v->n);
    } else if (is_func(v)) {
        fprintf(stream, "#<function>");
    } else if (is_list(v)){
        fprintf(stream, "(");
        fprint0(stream, v->list.car, depth+1);

        Value *cdr = v->list.cdr;
        while (is_list(cdr)) {
            fprintf(stream, " ");
            fprint0(stream, cdr->list.car, depth+1);
            cdr = cdr->list.cdr;
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

#define MAX_INTLEN 10
#define MAX_SYMLEN 1024

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
    } else if (isdigit(c)) {
        char buf[MAX_INTLEN+1];

        int i;
        for (i = 0; isdigit(c); i++) {
            buf[i] = c;
            c = fgetc(stream);

            if (i == MAX_INTLEN) {
                fprintf(stderr, "integer too long\n");
                exit(1);
            }
        }

        buf[i] = '\0';
        xungetc(c, stream);

        return mkint(atoi(buf));
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

Value *
assoc(Value *v, Value *l)
{
    if (!is_list(l) && !is_nil(l)) {
        fprintf(stderr, "assoc: expected list\n");
        exit(1);
    }

    while (!is_nil(l)) {
        if (caar(l) == v) {
            return cadar(l);
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

Value *
zip(Value *x, Value *y)
{
    if (is_nil(x) && is_nil(y)) {
        return NULL;
    } else if (is_list(x) && is_list(y)) {
        return cons(cons(car(x), cons(car(y), NULL)), zip(cdr(x), cdr(y)));
    } else if (is_nil(x) || is_nil(y)){
        fprintf(stderr, "zip: lists not the same length\n");
        exit(1);
    } else {
        fprintf(stderr, "zip: expected list\n");
        exit(1);
    }
}

Value *eval(Value *v, Value *env);

Value *
evcon(Value *conditions, Value *env)
{
    assert(is_list(conditions) || is_nil(conditions));

    if (eval(caar(conditions), env)) {
        return eval(cadar(conditions), env);
    } else {
        return evcon(cdr(conditions), env);
    }
}

Value *
evlis(Value *params, Value *env)
{
    assert(is_list(params) || is_nil(params));

    if (is_nil(params)) {
        return NULL;
    } else {
        return cons(eval(car(params), env), evlis(cdr(params), env));
    }
}

Value *
eq(Value *x, Value *y)
{
    if (x == y) {
        return intern("t");
    } else {
        return NULL;
    }
}

Value *
eval(Value *v, Value *env)
{
    if (is_list(v) && car(v) == intern("quote")) {
        return cadr(v);
    } else if (is_list(v) && car(v) == intern("atom")) {
        return is_sym(eval(cadr(v), env)) ? intern("t") : NULL;
    } else if (is_list(v) && car(v) == intern("eq")) {
        return eq(eval(cadr(v), env), eval(caddr(v), env));
    } else if (is_list(v) && car(v) == intern("car")) {
        return car(eval(cadr(v), env));
    } else if (is_list(v) && car(v) == intern("cdr")) {
        return cdr(eval(cadr(v), env));
    } else if (is_list(v) && car(v) == intern("cons")) {
        return cons(eval(cadr(v), env), eval(caddr(v), env));
    } else if (is_list(v) && car(v) == intern("cond")) {
        return evcon(cdr(v), env);
    } else if (is_list(v) && car(v) == intern("lambda")) {
        return mkfunc(cadr(v), cddr(v), env);
    } else if (is_list(v)) {
        Value *f = eval(car(v), env);

        if (is_func(f)) {
            Value *bindings = zip(f->func.params, evlis(cdr(v), env));
            Value *bound = append(bindings, f->func.env);

            Value *res;
            for (Value *e = f->func.body; is_list(e); e = cdr(e)) {
                res = eval(car(e), bound);
            }

            return res;
        } else {
            fprintf(stderr, "not a function: ");
            fprint(stderr, car(v));
            exit(1);
        }
    } else if (is_sym(v)) {
        Value *res = assoc(v, env);

        if (res) {
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
main(int argc, char *argv[])
{
    char *err = NULL;
    Value *env = NULL;

    env = cons(cons(intern("foo"), cons(intern("bar"), NULL)), env);
    env = cons(cons(intern("a"), cons(cons(intern("x"), cons(intern("y"), cons(intern("z"), NULL))), NULL)), env);

    while (peek(stdin) != EOF) {
        Value *v = read1(stdin);
        v = eval(v, env);
        print(v);
    }
    return 0;
}
