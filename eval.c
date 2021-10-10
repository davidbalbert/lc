#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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
char *eprintf(char *fmt, ...)
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
intern(char *s)
{
    Value *v = alloc(ATOM);
    v->atom = s;
    return v;
}

Value *
cons(Value *car, Value *cdr)
{
    Value *v = alloc(PAIR);
    v->list.car = car;
    v->list.cdr = cdr;
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
    skipspace(stream);

    int c = peek(stream);
    if (c == EOF) {
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
                *errp = eprintf("expected ')'");
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
                *errp = eprintf("couldn't ungetc (read atom)");
                return NULL;
            }
        }

        return intern(buf);
    } else {
        *errp = eprintf("unexpected character '%c' (%d)", c, c);
        return NULL;
    }
}

int
main(int argc, char *argv[])
{
    char *err = NULL;

    while (peek(stdin) != EOF) {
        Value *v = read_(stdin, &err);

        if (err != NULL) {
            fprintf(stderr, "error: %s\n", err);
            free(err);
            err = NULL;

            continue;
        }

        print(v);
    }

    return 0;
}
