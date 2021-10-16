#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    SYM,
    PAIR,
};
typedef enum Type Type;

typedef struct Value Value;
typedef struct List List;

struct List {
    Value *car;
    Value *cdr;
};

struct Value {
    Type type;
    union {
        char *sym;
        List list;
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
is_list(Value *v) {
    return !is_nil(v) && v->type == PAIR;
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
    Value *v = alloc(PAIR);
    v->list.car = car;
    v->list.cdr = cdr;
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
print0(Value *v, int depth)
{
    if (is_nil(v)) {
        printf("()");
    } else if (is_sym(v)) {
        printf("%s", v->sym);
    } else {
        printf("(");
        print0(v->list.car, depth+1);

        Value *cdr = v->list.cdr;
        while (is_list(cdr)) {
            printf(" ");
            print0(cdr->list.car, depth+1);
            cdr = cdr->list.cdr;
        }

        if (!is_nil(cdr)) {
            printf(" . ");
            print0(cdr, depth+1);
        }

        printf(")");
    }

    if (depth == 0) {
        printf("\n");
    }
}

void
print(Value *v)
{
    print0(v, 0);
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

Value *read_(FILE *stream);

Value *
readlist(FILE *stream)
{
    skipspace(stream);

    int c = peek(stream);
    if (c == EOF) {
        fprintf(stderr, "expected value or ')' but got EOF\n");
        exit(1);
    } else if (c == ')') {
        fgetc(stream);
        return NULL;
    } else {
        Value *car = read_(stream);

        skipspace(stream);

        Value *cdr;
        if (peek(stream) == '.') {
            fgetc(stream);
            cdr = read_(stream);

            skipspace(stream);

            if (fgetc(stream) != ')') {
                fprintf(stderr, "expected ')'\n");
                exit(1);
            }
        } else {
            cdr = readlist(stream);
        }

        return cons(car, cdr);
    }
}

#define MAX_ATOM_LEN 256

// Named read_ to not conflict with read(2)
Value *
read_(FILE *stream)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(') {
        return readlist(stream);
    } else if (isalpha(c)) {
        char *buf = xalloc(MAX_ATOM_LEN+1);
        int i = 0;
        while (isalpha(c) && i < MAX_ATOM_LEN) {
            buf[i++] = c;
            c = fgetc(stream);
        }
        buf[i] = '\0';

        if (c != EOF) {
            xungetc(c, stream);
        }

        return intern(buf);
    } else {
        fprintf(stderr, "unexpected character '%c' (%d)\n", c, c);
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
    if (is_sym(v)) {
        Value *res = assoc(v, env);

        if (res) {
            return res;
        } else {
            fprintf(stderr, "unbound variable: %s\n", v->sym);
            exit(1);
        }
    } else if (is_sym(car(v))) {
        if (car(v) == intern("quote")) {
            return cadr(v);
        } else if (car(v) == intern("atom")) {
            return is_sym(eval(cadr(v), env)) ? intern("t") : NULL;
        } else if (car(v) == intern("eq")) {
            return eq(eval(cadr(v), env), eval(caddr(v), env));
        } else if (car(v) == intern("car")) {
            return car(eval(cadr(v), env));
        } else if (car(v) == intern("cdr")) {
            return cdr(eval(cadr(v), env));
        } else if (car(v) == intern("cons")) {
            return cons(eval(cadr(v), env), eval(caddr(v), env));
        } else if (car(v) == intern("cond")) {
            return evcon(cdr(v), env);
        } else {
            Value *f = assoc(car(v), env);

            if (!is_list(f)) {
                fprintf(stderr, "unbound function: %s\n", car(v)->sym);
                exit(1);
            }

            return eval(cons(f, cdr(v)), env);
        }
    } else if (caar(v) == intern("label")) {
        return eval(
            cons(caddar(v), cdr(v)),
            cons(cons(cadar(v), cons(car(v), NULL)), env));
    } else if (caar(v) == intern("lambda")) {
        Value *bindings = zip(cadar(v), evlis(cdr(v), env));

        return eval(caddar(v), append(bindings, env));
    }

    // print(v);
    fprintf(stderr, "eval: unhandled case\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    char *err = NULL;
    Value *env = NULL;

    env = cons(cons(intern("foo"), cons(intern("bar"), NULL)), env);
    env = cons(cons(intern("a"), cons(cons(intern("x"), cons(intern("y"), cons(intern("z"), NULL))), NULL)), env);
    env = cons(cons(intern("cadr"), cons(cons(intern("lambda"), cons(cons(intern("l"), NULL), cons(cons(intern("car"), cons(cons(intern("cdr"), cons(intern("l"), NULL)), NULL)), NULL))), NULL)), env);

    while (peek(stdin) != EOF) {
        Value *v = read_(stdin);
        v = eval(v, env);
        print(v);
    }
    return 0;
}
