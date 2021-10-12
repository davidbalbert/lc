#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime.h"

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

Value *
cons(Value *car, Value *cdr)
{
    Value *v = alloc(LIST);
    v->list.car = car;
    v->list.cdr = cdr;
    return v;
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

        Value *v = alloc(INT);
        v->n = atoi(buf);
        return v;
    } else if (is_symchar(c)) {
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

        int len = strlen(buf);

        Value *v = alloc(SYM);
        v->sym = xalloc(len+1);
        strncpy(v->sym, buf, len);
        return v;
    } else {
        fprintf(stderr, "unexpected character: %c\n", c);
        exit(1);
    }
}

void
compile(Value *v)
{
    if (v == NULL) {
        fprintf(stderr, "compile: null value\n");
        return;
    }

    assert(v->type == INT);

    printf("#include <stdio.h>\n");
    printf("\n");
    printf("int\n");
    printf("main(int argc, char *argv[])\n");
    printf("{\n");
    printf("    return %d;\n", v->n);
    printf("}\n");
}

int main(int argc, char const *argv[])
{
    // compile(read1(stdin));
    print(read1(stdin));
    return 0;
}
