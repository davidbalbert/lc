#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
        Value *car = read1(stream);

        skipspace(stream);

        Value *cdr;
        if (peek(stream) == '.') {
            fgetc(stream);
            cdr = read1(stream);

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


#define INT_CHARLEN 10

Value *
read1(FILE *stream)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(') {
        return readlist(stream);
    } else if (isdigit(c)) {
        char buf[INT_CHARLEN+1];

        int i = 0;

        do {
            buf[i++] = (char)c;
            c = fgetc(stream);

            if (i == INT_CHARLEN) {
                fprintf(stderr, "integer too long\n");
                exit(1);
            }
        } while (c != EOF && isdigit(c));

        xungetc(c, stream);

        buf[i] = '\0';

        Value *v = alloc(INT);
        v->n = atoi(buf);
        return v;
    } else {
        fprintf(stderr, "unexpected character: %c\n", c);
        return NULL;
    }

    return NULL;
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
