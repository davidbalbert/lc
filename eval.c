#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

enum Type {
    ATOM,
    PAIR,
    NIL
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

Value *
alloc(Type t)
{
    Value *v = xalloc(sizeof(Value));
    v->type = t;
    return v;
}

int
is_nil(Value *v) {
    return v->type == NIL;
}

int
is_atom(Value *v) {
    return v->type == ATOM;
}

int
is_list(Value *v) {
    return v->type == PAIR;
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

#define MAX_ATOM_LEN 256

Value *
read0(FILE *stream, int inlist)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(' || inlist) {
        printf("-- list(%d)\n", inlist);

        if (inlist) {
            c = ungetc(c, stream);
            if (c == EOF) {
                fprintf(stderr, "couldn't ungetc\n");
                exit(1);
            }
        }

        skipspace(stream);

        if (peek(stream) == ')' && !inlist) {
            fgetc(stream);
            return alloc(NIL);
        }

        Value *car = read0(stream, 1);
        if (car == NULL) {
            return NULL;
        }


        printf("-- car: ");
        print(car);

        Value *cdr = read0(stream, 1);
        if (cdr == NULL) {
            return NULL;
        }

        printf("-- cdr: ");
        print(cdr);

        return cons(car, cdr);
    } else if (c == ')' && inlist) {
        printf("-- end of list\n");
        return alloc(NIL);
    } else if (isalpha(c)) {
        char *buf = xalloc(MAX_ATOM_LEN+1);
        int i = 0;
        while (isalpha(c) && i < MAX_ATOM_LEN) {
            buf[i++] = c;
            c = fgetc(stream);
        }
        buf[i] = '\0';

        if (c != EOF) {
            ungetc(c, stream);
        }

        return intern(buf);
    } else {
        fprintf(stderr, "unexpected character: `%c'\n", c);
        return NULL;
    }
}

// Named read_ to not conflict with read(2)
Value *
read_(FILE *stream)
{
    return read0(stream, 0);
}


int
main(int argc, char *argv[])
{
    Value *v = read_(stdin);

    if (v != NULL) {
        print(v);
    }

    return 0;
}
