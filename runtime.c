#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "runtime.h"

static int
is_nil(Value *v)
{
    return v == NULL;
}

static int
is_int(Value *v)
{
    return v != NULL && v->type == INT;
}

static int
is_sym(Value *v)
{
    return v != NULL && v->type == SYM;
}

static int
is_list(Value *v)
{
    return v != NULL && v->type == LIST;
}

static void print0(Value *v, int nested);

static void
printlist(Value *v) {
    assert(is_list(v));

    printf("(");
    print0(v->list.car, 1);

    Value *cdr = v->list.cdr;
    while (is_list(cdr)) {
        printf(" ");
        print0(cdr->list.car, 1);
        cdr = cdr->list.cdr;
    }

    if (!is_nil(cdr)) {
        printf(" . ");
        print0(cdr, 1);
    }

    printf(")");
}

static void
print0(Value *v, int nested)
{
    if (is_nil(v)) {
        printf("()");
    } else if (is_int(v)) {
        printf("%d", v->n);
    } else if (is_sym(v)) {
        printf("%s", v->sym);
    } else if (is_list(v)) {
        printlist(v);
    } else {
        fprintf(stderr, "print: unknown type: %d\n", v->type);
        exit(1);
    }

    if (nested == 0) {
        printf("\n");
    }
}

void
print(Value *v)
{
    print0(v, 0);
}