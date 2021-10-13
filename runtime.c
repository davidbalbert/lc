#include <assert.h>
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

static Value *
alloc(Type t)
{
    Value *v = xalloc(sizeof(Value));
    v->type = t;
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

    size_t len = strlen(s);

    Value *v = alloc(SYM);
    v->sym = xalloc(len + 1);
    strncpy(v->sym, s, len);

    symtab = cons(v, symtab);

    return v;
}

Value *
integer(int n)
{
    Value *v = alloc(INT);
    v->n = n;
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
is_nil(Value *v)
{
    return v == NULL;
}

int
is_int(Value *v)
{
    return v != NULL && v->type == INT;
}

int
is_sym(Value *v)
{
    return v != NULL && v->type == SYM;
}

int
is_list(Value *v)
{
    return v != NULL && v->type == LIST;
}

static void fprint0(FILE *stream, Value *v, int nested);

static void
printlist(FILE *stream, Value *v) {
    assert(is_list(v));

    fprintf(stream, "(");
    fprint0(stream, v->list.car, 1);

    Value *cdr = v->list.cdr;
    while (is_list(cdr)) {
        fprintf(stream, " ");
        fprint0(stream, cdr->list.car, 1);
        cdr = cdr->list.cdr;
    }

    if (!is_nil(cdr)) {
        fprintf(stream, " . ");
        fprint0(stream, cdr, 1);
    }

    fprintf(stream, ")");
}

static void
fprint0(FILE *stream, Value *v, int nested)
{
    if (is_nil(v)) {
        fprintf(stream, "()");
    } else if (is_int(v)) {
        fprintf(stream, "%d", v->n);
    } else if (is_sym(v)) {
        fprintf(stream, "%s", v->sym);
    } else if (is_list(v)) {
        printlist(stream, v);
    } else {
        fprintf(stderr, "print: unknown type: %d\n", v->type);
        exit(1);
    }

    if (nested == 0) {
        fprintf(stream, "\n");
    }
}

void
fprint(FILE *stream, Value *v) {
    fprint0(stream, v, 0);
}

void
print(Value *v)
{
    fprint(stdout, v);
}

Value *
car(Value *v)
{
    assert(is_list(v));
    return v->list.car;
}

Value *
cdr(Value *v)
{
    assert(is_list(v));
    return v->list.cdr;
}

Value *
cadr(Value *v)
{
    assert(is_list(v));
    return car(cdr(v));
}

Value *
caddr(Value *v)
{
    assert(is_list(v));
    return car(cdr(cdr(v)));
}
