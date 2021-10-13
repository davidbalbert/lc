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
        }

        fprintf(stderr, "expected %s but got ", type_name(t));
        fprint(stderr, v);
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

int
contains(Value *list, Value *v)
{
    type_assert(list, LIST, NULL);

    while (list != NULL) {
        if (car(list) == v) {
            return 1;
        }
        list = cdr(list);
    }
    return 0;
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

Value *globals = NULL;

void
codegen_global_decl(Value *expr)
{
    if (is_list(expr) && car(expr) == intern("def")) {
        Value *sym = cadr(expr);
        type_assert(sym, SYM, expr);

        globals = cons(sym, globals);
        printf("Value *%s;\n", sym->sym);
    }
}

void
codegen_value(Value *v)
{
    if (is_nil(v)) {
        printf("NULL");
    } else if (is_int(v)) {
        printf("integer(%d)", v->n);
    } else if (is_sym(v)) {
        printf("intern(\"%s\")", v->sym);
    } else if (is_list(v)) {
        printf("cons(");
        codegen_value(car(v));
        printf(", ");
        codegen_value(cdr(v));
        printf(")");
    } else {
        fprintf(stderr, "unknown type: %d\n", v->type);
        exit(1);
    }
}

void
codegen_global_init(Value *expr)
{
    if (!is_list(expr) || car(expr) != intern("def")) {
        return;
    }

    Value *sym = cadr(expr);
    Value *val = caddr(expr);

    type_assert(sym, SYM, expr);

    printf("    %s = ", sym->sym);

    if (is_sym(val)) {
        if (contains(globals, val)) {
            printf("%s", val->sym);
        } else {
            fprintf(stderr, "undefined variable: %s\n", val->sym);
            exit(1);
        }
    } else if (is_list(val) && car(val) == intern("quote")) {
        codegen_value(cadr(val));
    } else if (is_list(val)) {
        fprintf(stderr, "undefined function: %s\n", sym->sym);
        exit(1);
    } else {
        codegen_value(val);
    }

    printf(";\n");
}

void
codegen(Value *exprs)
{
    printf("#include <stdio.h>\n");
    printf("\n");
    printf("#include \"runtime.h\"\n");
    printf("\n");

    for (Value *l = exprs; l != NULL; l = cdr(l)) {
        codegen_global_decl(car(l));
    }

    printf("\n");

    printf("int\n");
    printf("main(int argc, char *argv[])\n");
    printf("{\n");

    for (Value *l = exprs; l != NULL; l = cdr(l)) {
        codegen_global_init(car(l));
    }

    printf("    return 0;\n");
    printf("}\n");
}

int main(int argc, char const *argv[])
{
    codegen(readall(stdin));
    return 0;
}
