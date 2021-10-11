#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

enum Type {
    INT,
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
        int n;
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

#define INT_CHARLEN 10

Value *
read1(FILE *stream)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
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
    compile(read1(stdin));
    return 0;
}
