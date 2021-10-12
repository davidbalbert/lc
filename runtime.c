#include <stdio.h>

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
is_list(Value *v)
{
    return v != NULL && v->type == LIST;
}

static void
print0(Value *v, int depth)
{
    if (is_nil(v)) {
        printf("()");
    } else if (is_int(v)) {
        printf("%d", v->n);
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
