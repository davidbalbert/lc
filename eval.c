#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
    ATOM,
    PAIR,
    ERROR
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
        char *err;
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

// char *
// xsprintf(const char *fmt, ...)
// {
//     va_list ap;
//     char *buf = xalloc(ERRLEN);
//     va_start(ap, fmt);
//     vsnprintf(buf, ERRLEN, fmt, ap);
//     va_end(ap);
//     return buf;
// }


#define ERRLEN 1024

Value *
errorf(char *fmt, ...)
{
    char *s = xalloc(ERRLEN);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s, ERRLEN, fmt, ap);
    va_end(ap);

    Value *err = alloc(ERROR);
    err->err = s;

    return err;
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

int
is_err(Value *v) {
    return !is_nil(v) && v->type == ERROR;
}

Value *
car(Value *v)
{
    if (is_err(v)) {
        return v;
    }

    if (is_nil(v)) {
        return NULL;
    } else if (is_list(v)) {
        return v->list.car;
    } else {
        return errorf("car: expected list");
    }
}

Value *
cdr(Value *v)
{
    if (is_err(v)) {
        return v;
    }

    if (is_nil(v)) {
        return NULL;
    } else if (is_list(v)) {
        return v->list.cdr;
    } else {
        return errorf("cdr: expected list");
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
    if (is_err(car)) {
        return car;
    } else if (is_err(cdr)) {
        return cdr;
    }

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

    size_t len = strlen(s);
    char *s1 = xalloc(len + 1);
    v->atom = strncpy(s1, s, len);

    symtab = cons(v, symtab);

    return v;
}

void
print0(Value *v, int depth)
{
    if (is_err(v)) {
        fprintf(stderr, "error: %s\n", v->err);
        return;
    }

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

Value *read_(FILE *stream);

Value *
readlist(FILE *stream)
{
    skipspace(stream);

    int c = peek(stream);
    if (c == EOF) {
        return errorf("expected value or ')' but got EOF");
    } else if (c == ')') {
        fgetc(stream);
        return NULL;
    } else {
        Value *car = read_(stream);
        if (is_err(car)) {
            return car;
        }

        skipspace(stream);

        Value *cdr;
        if (peek(stream) == '.') {
            fgetc(stream);
            cdr = read_(stream);
            if (is_err(cdr)) {
                return cdr;
            }

            skipspace(stream);

            if (fgetc(stream) != ')') {
                return errorf("expected ')'");
            }
        } else {
            cdr = readlist(stream);

            if (is_err(cdr)) {
                return cdr;
            }
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
            if (ungetc(c, stream) == EOF) {
                return errorf("couldn't ungetc (read atom)");
            }
        }

        return intern(buf);
    } else {
        return errorf("unexpected character '%c' (%d)", c, c);
    }
}

Value *
assoc(Value *v, Value *l)
{
    if (is_err(v)) {
        return v;
    } else if (is_err(l)) {
        return l;
    }

    if (!is_list(l) && !is_nil(l)) {
        return errorf("assoc: expected list");
    }

    while (!is_nil(l)) {
        if (caar(l) == v) {
            return cadar(l);
        }

        l = cdr(l);
        if (is_err(l)) {
            return l;
        }
    }

    return NULL;
}

Value *
append(Value *x, Value *y)
{
    if (is_err(x)) {
        return x;
    } else if (is_err(y)) {
        return y;
    }

    if (is_nil(x)) {
        return y;
    } else {
        return cons(car(x), append(cdr(x), y));
    }
}

Value *
zip(Value *x, Value *y)
{
    if (is_err(x)) {
        return x;
    } else if (is_err(y)) {
        return y;
    }

    if (is_nil(x) && is_nil(y)) {
        return NULL;
    } else if (is_list(x) && is_list(y)) {
        return cons(cons(car(x), cons(car(y), NULL)), zip(cdr(x), cdr(y)));
    } else if (is_nil(x) || is_nil(y)){
        return errorf("zip: lists not the same length");
    } else {
        return errorf("zip: expected list");
    }
}

Value *eval(Value *v, Value *env);

Value *
evcon(Value *conditions, Value *env)
{
    if (is_err(conditions)) {
        return conditions;
    } else if (is_err(env)) {
        return env;
    }

    if (!is_list(conditions) && !is_nil(conditions)) {
        return errorf("evcon: expected list");
    }

    if (eval(caar(conditions), env)) {
        return eval(cadar(conditions), env);
    } else {
        return evcon(cdr(conditions), env);
    }
}

Value *
evlis(Value *params, Value *env)
{
    if (is_err(params)) {
        return params;
    } else if (is_err(env)) {
        return env;
    }

    if (!is_list(params) && !is_nil(params)) {
        return errorf("evlis: expected list");
    }

    if (is_nil(params)) {
        return NULL;
    } else {
        return cons(eval(car(params), env), evlis(cdr(params), env));
    }
}

Value *
eq(Value *x, Value *y)
{
    if (is_err(x)) {
        return x;
    } else if (is_err(y)) {
        return y;
    }

    if (x == y) {
        return intern("t");
    } else {
        return NULL;
    }
}

Value *
eval(Value *v, Value *env)
{
    if (is_err(v)) {
        return v;
    } else if (is_err(env)) {
        return env;
    }

    if (is_atom(v)) {
        Value *res = assoc(v, env);

        if (res) {
            return res;
        } else {
            return errorf("unbound variable: %s", v->atom);
        }
    } else if (is_atom(car(v))) {
        if (car(v) == intern("quote")) {
            return cadr(v);
        } else if (car(v) == intern("atom")) {
            return is_atom(eval(cadr(v), env)) ? intern("t") : NULL;
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

            if (is_err(f)) {
                return f;
            } else if (!is_list(f)) {
                return errorf("unbound function: %s", car(v)->atom);
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
    return errorf("eval: unhandled case");
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
