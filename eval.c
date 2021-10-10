#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    ATOM,
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
        char *atom;
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

#define ERRLEN 256
char *
xsprintf(char *fmt, ...)
{
    char *err = xalloc(ERRLEN);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, ERRLEN, fmt, ap);
    va_end(ap);
    return err;
}

Value *
alloc(Type t)
{
    Value *v = xalloc(sizeof(Value));
    v->type = t;
    return v;
}

int
is_nil(Value *v) {
    return v == NULL;
}

int
is_atom(Value *v) {
    return !is_nil(v) && v->type == ATOM;
}

int
is_list(Value *v) {
    return !is_nil(v) && v->type == PAIR;
}

Value *
car(Value *v, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (is_nil(v)) {
        return NULL;
    } else if (is_list(v)) {
        return v->list.car;
    } else {
        *errp = xsprintf("car: expected list");
        return NULL;
    }
}

Value *
cdr(Value *v, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (is_nil(v)) {
        return NULL;
    } else if (is_list(v)) {
        return v->list.cdr;
    } else {
        *errp = xsprintf("cdr: expected list");
        return NULL;
    }
}

Value *
caar(Value *v, char **errp)
{
    return car(car(v, errp), errp);
}

Value *
cadr(Value *v, char **errp)
{
    return car(cdr(v, errp), errp);
}

Value *
cadar(Value *v, char **errp)
{
    return car(cdr(car(v, errp), errp), errp);
}

Value *
caddr(Value *v, char **eerp)
{
    return car(cdr(cdr(v, eerp), eerp), eerp);
}

Value *
caddar(Value *v, char **errp)
{
    return car(cdr(cdr(car(v, errp), errp), errp), errp);
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
        assert(is_atom(sym));

        if (strcmp(sym->atom, s) == 0) {
            return sym;
        }

        l = l->list.cdr;
    }

    Value *v = alloc(ATOM);
    v->atom = xsprintf(s);

    symtab = cons(v, symtab);

    return v;
}

void
print0(Value *v, int depth)
{
    if (is_nil(v)) {
        printf("()");
    } else if (is_atom(v)) {
        printf("%s", v->atom);
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

void
skipspace(FILE *stream)
{
    int c;
    while ((c = peek(stream)) != EOF && isspace(c)) {
        fgetc(stream);
    }
}

Value *read_(FILE *stream, char **errp);

Value *
readlist(FILE *stream, char **errp)
{
    if (*errp) {
        return NULL;
    }

    skipspace(stream);

    int c = peek(stream);
    if (c == EOF) {
        *errp = xsprintf("expected value or ')' but got EOF");
        return NULL;
    } else if (c == ')') {
        fgetc(stream);
        return NULL;
    } else {
        Value *car = read_(stream, errp);
        if (*errp) {
            return NULL;
        }

        skipspace(stream);

        Value *cdr;
        if (peek(stream) == '.') {
            fgetc(stream);
            cdr = read_(stream, errp);
            if (*errp) {
                return NULL;
            }

            skipspace(stream);

            if (fgetc(stream) != ')') {
                *errp = xsprintf("expected ')'");
                return NULL;
            }
        } else {
            cdr = readlist(stream, errp);

            if (*errp) {
                return NULL;
            }
        }

        return cons(car, cdr);
    }
}

#define MAX_ATOM_LEN 256

// Named read_ to not conflict with read(2)
Value *
read_(FILE *stream, char **errp)
{
    if (*errp) {
        return NULL;
    }

    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(') {
        Value *l = readlist(stream, errp);

        if (*errp) {
            return NULL;
        } else {
            return l;
        }
    } else if (isalpha(c)) {
        char *buf = xalloc(MAX_ATOM_LEN+1);
        int i = 0;
        while (isalpha(c) && i < MAX_ATOM_LEN) {
            buf[i++] = c;
            c = fgetc(stream);
        }
        buf[i] = '\0';

        if (c != EOF) {
            if (ungetc(c, stream) == EOF) {
                *errp = xsprintf("couldn't ungetc (read atom)");
                return NULL;
            }
        }

        return intern(buf);
    } else {
        *errp = xsprintf("unexpected character '%c' (%d)", c, c);
        return NULL;
    }
}

Value *
assoc(Value *v, Value *l, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (!is_list(l) && !is_nil(l)) {
        *errp = xsprintf("assoc: expected list");
        return NULL;
    }

    while (!is_nil(l)) {
        Value *pair = car(l, errp);
        if (*errp) {
            return NULL;
        }

        Value *key = car(pair, errp);
        if (*errp) {
            return NULL;
        }

        if (key == v) {
            return cadr(pair, errp);
        }

        l = cdr(l, errp);
        if (*errp) {
            return NULL;
        }
    }

    return NULL;
}

Value *
append(Value *x, Value *y, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (is_nil(x)) {
        return y;
    } else {
        Value *a = car(x, errp);
        if (*errp) {
            return NULL;
        }

        Value *d = append(cdr(x, errp), y, errp);
        if (*errp) {
            return NULL;
        }

        return cons(car(x, errp), d);
    }
}

Value *
zip(Value *x, Value *y, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (is_nil(x) && is_nil(y)) {
        return NULL;
    } else if (is_list(x) && is_list(y)) {
        Value *a = car(x, errp);
        if (*errp) {
            return NULL;
        }

        Value *b = car(y, errp);
        if (*errp) {
            return NULL;
        }

        Value *d = zip(cdr(x, errp), cdr(y, errp), errp);
        if (*errp) {
            return NULL;
        }

        return cons(cons(a, cons(b, NULL)), d);
    } else if (is_nil(x) || is_nil(y)){
        *errp = xsprintf("zip: lists not the same length");
        return NULL;
    } else {
        *errp = xsprintf("zip: expected list");
        return NULL;
    }
}

Value *eval(Value *v, Value *env, char **errp);

Value *
evcon(Value *conditions, Value *env, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (!is_list(conditions) && !is_nil(conditions)) {
        *errp = xsprintf("evcon: expected list");
        return NULL;
    }

    if (eval(caar(conditions, errp), env, errp)) {
        return eval(cadar(conditions, errp), env, errp);
    } else {
        return evcon(cdr(conditions, errp), env, errp);
    }
}

Value *
evlis(Value *params, Value *env, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (!is_list(params) && !is_nil(params)) {
        *errp = xsprintf("evlis: expected list");
        return NULL;
    }

    if (is_nil(params)) {
        return NULL;
    } else {
        Value *a = eval(car(params, errp), env, errp);
        if (*errp) {
            return NULL;
        }

        Value *d = evlis(cdr(params, errp), env, errp);
        if (*errp) {
            return NULL;
        }

        return cons(a, d);
    }
}

Value *
eval(Value *v, Value *env, char **errp)
{
    if (*errp) {
        return NULL;
    }

    if (is_atom(v)) {
        Value *l = assoc(v, env, errp);
        if (*errp) {
            return NULL;
        }

        if (l) {
            return l;
        } else {
            *errp = xsprintf("unbound variable: %s", v->atom);
            return NULL;
        }
    } else if (is_atom(car(v, errp))) {
        if (car(v, errp) == intern("quote")) {
            return cadr(v, errp);
        } else if (car(v, errp) == intern("atom")) {
            return is_atom(eval(cadr(v, errp), env, errp)) ? intern("t") : NULL;
        } else if (car(v, errp) == intern("eq")) {
            Value *a = eval(cadr(v, errp), env, errp);
            Value *b = eval(caddr(v, errp), env, errp);

            if (*errp) {
                return NULL;
            }

            return a == b ? intern("t") : NULL;
        } else if (car(v, errp) == intern("car")) {
            return car(eval(cadr(v, errp), env, errp), errp);
        } else if (car(v, errp) == intern("cdr")) {
            return cdr(eval(cadr(v, errp), env, errp), errp);
        } else if (car(v, errp) == intern("cons")) {
            Value *a = eval(cadr(v, errp), env, errp);
            Value *b = eval(caddr(v, errp), env, errp);
            if (*errp) {
                return NULL;
            }

            return cons(a, b);
        } else if (car(v, errp) == intern("cond")) {
            return evcon(cdr(v, errp), env, errp);
        } else {
            Value *f = assoc(car(v, errp), env, errp);
            if (*errp) {
                return NULL;
            }

            if (!is_list(f)) {
                *errp = xsprintf("unbound function: %s", car(v, errp)->atom);
                return NULL;
            }

            return eval(cons(f, cdr(v, errp)), env, errp);
        }
    } else if (caar(v, errp) == intern("label")) {
        return eval(
            cons(caddar(v, errp), cdr(v, errp)),
            cons(cons(cadar(v, errp), cons(car(v, errp), NULL)), env),
            errp);
    } else if (caar(v, errp) == intern("lambda")) {
        Value *bindings = zip(cadar(v, errp), evlis(cdr(v, errp), env, errp), errp);
        if (*errp) {
            return NULL;
        }

        Value *env1 = append(bindings, env, errp);
        if (*errp) {
            return NULL;
        }

        return eval(caddar(v, errp), env1, errp);
    }

    print(v);
    *errp = xsprintf("eval: unhandled case");
    return NULL;
}

void
handle(char **errp)
{
    fprintf(stderr, "error: %s\n", *errp);
    free(*errp);
    *errp = NULL;
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
        Value *v = read_(stdin, &err);

        if (err != NULL) {
            handle(&err);
            continue;
        }

        v = eval(v, env, &err);

        if (err != NULL) {
            handle(&err);
            continue;
        }

        print(v);
    }
    return 0;
}
