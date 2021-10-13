#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime.h"

char *
type_name(Type t)
{
    switch (t) {
    case INT:
        return "int";
    case SYM:
        return "symbol";
    case LIST:
        return "list";
    default:
        fprintf(stderr, "unknown type: %d\n", t);
        exit(1);
    }
}

void type_assert(Value *v, Type t, Value *context)
{
    if (v->type != t) {
        if (context) {
            fprint(stderr, context);
            fprintf(stderr, "\n");
        }

        fprintf(stderr, "expected %s but got", type_name(t));
        fprint(stderr, v);
        fprintf(stderr, "\n");
        exit(1);
    }
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

        return integer(atoi(buf));
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

        return intern(buf);
    } else {
        fprintf(stderr, "unexpected character: %c\n", c);
        exit(1);
    }
}

Value *
reverse(Value *list)
{
    type_assert(list, LIST, NULL);

    Value *res = NULL;
    while (list != NULL) {
        res = cons(car(list), res);
        list = cdr(list);
    }
    return res;
}

Value *
readall(FILE *stream)
{
    Value *res = NULL;

    while (peek(stream) != EOF) {
        skipspace(stream);
        Value *val = read1(stream);
        res = cons(val, res);
    }

    return reverse(res);
}

void
compile_value(Value *v)
{
    switch (v->type) {
    case INT:
        printf("integer(%d)", v->n);
        break;
    case SYM:
        printf("intern(\"%s\")", v->sym);
        break;
    case LIST:
        fprintf(stderr, "can't compile lists yet");
        break;
    default:
        fprintf(stderr, "unknown type: %d\n", v->type);
        exit(1);
    }
}

int
can_statically_initialize(Value *v)
{
    return is_nil(v) || is_int(v);
}

void
compile_static_global(Value *name, Value *v)
{
    if (is_nil(v)) {
        printf("Value *%s = NULL;\n", name->sym);
    } else if (is_int(v)) {
        printf("Value _%s = { .type = INT, .n = %d };\n", name->sym, v->n);
        printf("Value *%s = &_%s;\n", name->sym, name->sym);
    } else {
        fprintf(stderr, "can't statically initialize %s\n", type_name(v->type));
        exit(1);
    }
}

void
compile_lazy_global(Value *name)
{
    printf("Value *%s = NULL;\n", name->sym);
}

void
compile_global(Value *expr)
{
    if (is_list(expr) && car(expr) == intern("def")) {
        Value *sym = cadr(expr);
        Value *val = caddr(expr);

        type_assert(sym, SYM, expr);

        if (can_statically_initialize(val)) {
            compile_static_global(sym, val);
        } else {
            compile_lazy_global(sym);
        }
    }
}

void
compile(Value *exprs)
{
    printf("#include <stdio.h>\n");
    printf("\n");
    printf("#include \"runtime.h\"\n");
    printf("\n");

    Value *l = exprs;
    while (l != NULL) {
        compile_global(car(l));
        l = cdr(l);
    }

    printf("\n");

    printf("int\n");
    printf("main(int argc, char *argv[])\n");
    printf("{\n");
    printf("    return 0;\n");
    printf("}\n");
}

int main(int argc, char const *argv[])
{
    compile(readall(stdin));
    return 0;
}
