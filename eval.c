#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// TODO: zero fill inside realloc
void *
xrealloc(void *p, size_t size)
{
    p = realloc(p, size);
    if (p == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return p;
}

typedef struct Header Header;
struct Header {
    Header *next; // the least significant bit is the mark bit
    size_t size; // size in multiples of sizeof(Header)
};

typedef struct Root Root;
struct Root {
    Header *h;
    Root *next;
};

Header base; // initialized to zero
Header *freep = &base; // circular linked list
Header *usedp;         // circular linked list
Root *roots;           // non-circular
void *stacktop;

void
gcinit0(void)
{
    freep->next = freep;
}

#define gcinit() do { void *x; stacktop = &x; gcinit0(); } while(0)

// TOOD: gcroot won't work for symtab, because intern does; symtab = cons("foo", symtab)
void
gcroot(void *p)
{
    Header *h = (Header *)p-1;
    Root *r = xalloc(sizeof(Root));

    r->h = h;
    r->next = roots;
    roots = r;
}

void
gcmark(void *p)
{
}

void
gcmarkall(void)
{

}

void
gcsweep(void)
{

}

void
gc()
{
    gcmarkall();
    gcsweep();
}

void
gcfree(Header *h)
{
    Header *p = freep;

    while (1) {
        if (p <= h && p->next >= h+h->size) {
            // if h abuts p->next, merge them
            if (h+h->size == p->next) {
                h->size += p->next->size;
                h->next = p->next->next;
            } else {
                h->next = p->next;
            }

            // if p abuts h, merge them too
            if (p+p->size == h) {
                p->size += h->size;
                p->next = h->next;
            } else {
                p->next = h;
            }

            freep = p;
            return;
        }

        p = p->next;
    }
}

#define MIN_ALLOC 4096

void
gcmore(size_t size)
{
    if (size < MIN_ALLOC) {
        size = MIN_ALLOC;
    }

    assert(size%sizeof(Header) == 0);

    // we may need to use aligned_alloc here
    Header *p = xalloc(size);

    assert((uintptr_t)p % sizeof(Header) == 0);

    gcfree(p);
}

void *
gcmalloc(size_t size)
{
    size_t nhead = (sizeof(Header) + size - 1) / sizeof(Header) + 1;

    Header *prev = freep;
    Header *p = freep->next;

    int gcd = 0;

    while (1) {
        if (p->size >= nhead) {
            if (p->size == nhead) {
                prev->next = p->next;
            } else {
                p->size -= nhead;
                p += p->size;
                p->size = nhead;
            }

            if (usedp == NULL) {
                usedp = p->next = p;
            } else {
                p->next = usedp->next;
                usedp->next = p;
            }

            return p+1;
        }

        if (p == freep && !gcd) {
            gc();
            gcd = 1;
        } else if (p == freep) {
            gcmore(nhead*sizeof(Header));
        }

        prev = p;
        p = p->next;
    }
}


enum Type {
    SYMBOL,
    STRING,
    INTEGER,
    PAIR,
    BUILTIN,
    FUNCTION,
    MACRO,
};
typedef enum Type Type;

typedef struct Value Value;

struct Buf {
    char *s;
    size_t len;
    size_t cap;
};
typedef struct Buf Buf;

struct Pair {
    Value *car;
    Value *cdr;
};
typedef struct Pair Pair;

typedef struct Env Env;
struct Env {
    Env *parent;
    Value *bindings;
};

struct Func {
    Value *name;
    Value *params;
    Value *body;
    Env *env;
};
typedef struct Func Func;

typedef Value *(*Imp)(Value *args);

struct Builtin {
    char *name;
    Imp imp;
};
typedef struct Builtin Builtin;

struct Value {
    Type type;
    union {
        char *sym;
        Buf *str;
        long long n;
        Pair pair;
        Builtin builtin;
        Func func; // also used for macros
    };
};

// s_nil can never appear in lisp land. It is read as NULL (empty list);
Value *s_nil;

Value *s_t;
Value *t; // s_t
Value *s_fn;
Value *s_macro;
Value *s_quote;
Value *s_quasiquote;
Value *s_unquote;
Value *s_unquote_splicing;
Value *s_if;
Value *s_def;
Value *s_set;
Value *s_car;
Value *s_cdr;

Buf *
binit(char *s)
{
    Buf *b = xalloc(sizeof(Buf));
    b->len = strlen(s);
    b->cap = b->len + 1;
    b->s = xalloc(b->cap);
    strncpy(b->s, s, b->len);
    return b;
}

Buf *
bappend(Buf *b, char *s)
{
    size_t len = strlen(s);
    if (b->len + len + 1 > b->cap) {
        b->cap = b->len + len + 1;
        b->s = xrealloc(b->s, b->cap);
    }
    strncpy(b->s + b->len, s, len);
    b->len += len;
    return b;
}

Buf *
bputc(Buf *b, char c)
{
    if (b->len + 1 > b->cap) {
        b->cap *= 2;
        b->s = xrealloc(b->s, b->cap);
    }
    b->s[b->len++] = c;
    return b;
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
is_symbol(Value *v) {
    return !is_nil(v) && v->type == SYMBOL;
}

int
is_string(Value *v) {
    return !is_nil(v) && v->type == STRING;
}

int
is_integer(Value *v) {
    return !is_nil(v) && v->type == INTEGER;
}

int
is_pair(Value *v) {
    return !is_nil(v) && v->type == PAIR;
}

int
is_builtin(Value *v) {
    return !is_nil(v) && v->type == BUILTIN;
}

int
is_function(Value *v) {
    return !is_nil(v) && v->type == FUNCTION;
}

int
is_procedure(Value *v) {
    return is_function(v) || is_builtin(v);
}

int
is_macro(Value *v) {
    return !is_nil(v) && v->type == MACRO;
}

Value *
car(Value *v)
{
    if (is_pair(v)) {
        return v->pair.car;
    } else {
        return NULL;
    }
}

Value *
cdr(Value *v)
{
    if (is_pair(v)) {
        return v->pair.cdr;
    } else {
        return NULL;
    }
}

Value *
caar(Value *v)
{
    return car(car(v));
}

Value *
cadr(Value *v)
{
    return car(cdr(v));
}

Value *
cddr(Value *v)
{
    return cdr(cdr(v));
}

Value *
cadar(Value *v)
{
    return car(cdr(car(v)));
}

Value *
caddr(Value *v)
{
    return car(cdr(cdr(v)));
}

Value *
cdddr(Value *v)
{
    return cdr(cdr(cdr(v)));
}

Value *
caddar(Value *v)
{
    return car(cdr(cdr(car(v))));
}

Value *
cons(Value *car, Value *cdr)
{
    Value *v = alloc(PAIR);
    v->pair.car = car;
    v->pair.cdr = cdr;
    return v;
}

Value *
mkint(long long n)
{
    Value *v = alloc(INTEGER);
    v->n = n;
    return v;
}

Value *
mkstring(Buf *b)
{
    Value *v = alloc(STRING);
    v->str = b;
    return v;
}

Value *
mkfunc(Type type, Value *params, Value *body, Env *env)
{
    assert(type == FUNCTION || type == MACRO);

    Value *v = alloc(type);
    v->func.name = NULL;
    v->func.params = params;
    v->func.body = body;
    v->func.env = env;
    return v;
}

Value *
mkbuiltin(char *name, Imp imp)
{
    Value *v = alloc(BUILTIN);
    v->builtin.name = name;
    v->builtin.imp = imp;
    return v;
}

Value *symtab = NULL;

Value *
intern(char *s)
{
    Value *l = symtab;

    assert(is_nil(l) || is_pair(l));

    while (!is_nil(l)) {
        Value *sym = l->pair.car;
        assert(is_symbol(sym));

        if (strcmp(sym->sym, s) == 0) {
            return sym;
        }

        l = l->pair.cdr;
    }

    Value *v = alloc(SYMBOL);

    size_t len = strlen(s);
    char *s1 = xalloc(len + 1);
    v->sym = strncpy(s1, s, len);

    symtab = cons(v, symtab);

    return v;
}

void
fprint0(FILE *stream, Value *v, int depth)
{
    if (is_nil(v)) {
        fprintf(stream, "nil");
    } else if (is_symbol(v)) {
        fprintf(stream, "%s", v->sym);
    } else if (is_integer(v)) {
        fprintf(stream, "%lld", v->n);
    } else if (is_string(v)) {
        fprintf(stream, "\"%s\"", v->str->s);
    } else if (is_builtin(v)) {
        fprintf(stream, "#<builtin %s>", v->builtin.name);
    } else if (is_function(v)) {
        fprintf(stream, "#<function ");
        if (v->func.name != NULL) {
            fprintf(stream, "%s", v->func.name->sym);
        } else {
            fprintf(stream, "(anonymous)");
        }
        fprintf(stream, ">");
    } else if (is_macro(v)) {
        fprintf(stream, "#<macro ");
        if (v->func.name != NULL) {
            fprintf(stream, "%s", v->func.name->sym);
        } else {
            fprintf(stream, "(anonymous)");
        }
        fprintf(stream, ">");
    } else if (is_pair(v)){
        fprintf(stream, "(");
        fprint0(stream, v->pair.car, depth+1);

        Value *cdr = v->pair.cdr;
        while (is_pair(cdr)) {
            fprintf(stream, " ");
            fprint0(stream, cdr->pair.car, depth+1);
            cdr = cdr->pair.cdr;
        }

        if (!is_nil(cdr)) {
            fprintf(stream, " . ");
            fprint0(stream, cdr, depth+1);
        }

        fprintf(stream, ")");
    } else {
        fprintf(stdout, "print: unknown type\n");
        exit(1);
    }

    if (depth == 0) {
        fprintf(stream, "\n");
    }
}

void
fprint(FILE *stream, Value *v)
{
    fprint0(stream, v, 0);
}

void
print(Value *v)
{
    fprint(stdout, v);
}

Value *
builtin_print(Value *args)
{
    for (Value *v = args; !is_nil(v); v = cdr(v)) {
        fprint(stdout, car(v));
    }

    return NULL;
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
    int c, comment = 0;

    while ((c = peek(stream)) != EOF) {
        if (c == ';') {
            comment = 1;
        }

        if (!isspace(c) && !comment) {
            return;
        }

        fgetc(stream);

        if (c == '\n') {
            comment = 0;
        }
    }
}

Value *read(FILE *stream);

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

        Value *cdr = read(stream);
        skipspace(stream);
        int c = fgetc(stream);
        if (c != ')') {
            fprintf(stderr, "expected ')' but got '%c'\n", c);
            exit(1);
        }

        return cdr;
    } else {
        Value *car = read(stream);
        Value *cdr = readlist(stream, 0);
        return cons(car, cdr);
    }
}

int
is_symchar(int c)
{
    return !isspace(c) && c != '(' && c != ')' && c != '.';
}

int
is_symstart(int c)
{
    return is_symchar(c) && !isdigit(c);
}

#define MAX_INTLEN 20 // includes optional leading -
#define MAX_SYMLEN 1024

long long
parseint(char *s)
{
    long long n = strtoll(s, NULL, 10);
    if (errno == ERANGE) {
        fprintf(stderr, "integer too big '%s'\n", s);
        exit(1);
    }
    return n;
}

Value *
read(FILE *stream)
{
    skipspace(stream);

    int c = fgetc(stream);
    if (c == EOF) {
        return NULL;
    } else if (c == '(') {
        return readlist(stream, 1);
    } else if (c == '\'') {
        return cons(s_quote, cons(read(stream), NULL));
    } else if (c == '`') {
        return cons(s_quasiquote, cons(read(stream), NULL));
    } else if (c == ',') {
        // TODO error check on peek and fgetc

        if (peek(stream) == '@') {
            fgetc(stream);
            return cons(s_unquote_splicing, cons(read(stream), NULL));
        } else {
            return cons(s_unquote, cons(read(stream), NULL));
        }
    } else if (c == '"') {
        Buf *b = binit("");

        while (1) {
            c = fgetc(stream);
            if (c == EOF) {
                fprintf(stderr, "unterminated string\n");
                exit(1);
            } else if (c == '"') {
                break;
            } else if (c == '\\') {
                c = fgetc(stream);
                if (c == EOF) {
                    fprintf(stderr, "unterminated string\n");
                    exit(1);
                } else if (c == 'n') {
                    c = '\n';
                } else if (c == 't') {
                    c = '\t';
                } else if (c == 'r') {
                    c = '\r';
                } else if (c == '\\') {
                    c = '\\';
                } else if (c == '"') {
                    c = '"';
                } else {
                    fprintf(stderr, "unknown escape sequence '\\%c'\n", c);
                    exit(1);
                }
            }

            bputc(b, c);
        }

        return mkstring(b);
    } else if ((c == '-' && isdigit(peek(stream))) || isdigit(c)) {
        char buf[MAX_INTLEN+1];

        int i = 0;
        do {
            if (i == MAX_INTLEN) {
                fprintf(stderr, "integer too long\n");
                exit(1);
            }
            buf[i++] = c;
            c = fgetc(stream);
        } while (isdigit(c));

        buf[i] = '\0';
        xungetc(c, stream);

        return mkint(parseint(buf));
    } else if (is_symstart(c)) {
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

        Value *sym = intern(buf);

        if (sym == s_nil) {
            return NULL;
        } else {
            return sym;
        }
    } else {
        fprintf(stderr, "unexpected character: %c\n", c);
        exit(1);
    }
}

int
is_eq(Value *x, Value *y)
{
    return x == y;
}

int
is_eqv(Value *x, Value *y)
{
    if (is_integer(x) && is_integer(y)) {
        return x->n == y->n;
    } else {
        return is_eq(x, y);
    }
}

int
is_equal(Value *x, Value *y)
{
    if (is_pair(x) && is_pair(y)) {
        return is_equal(car(x), car(y)) && is_equal(cdr(x), cdr(y));
    } else {
        return is_eqv(x, y);
    }
}

Value *
assoc(Value *v, Value *l)
{
    if (!is_pair(l) && !is_nil(l)) {
        fprintf(stderr, "assoc: expected list\n");
        exit(1);
    }

    while (!is_nil(l)) {
        if (is_equal(caar(l), v)) {
            return car(l);
        }

        l = cdr(l);
    }

    return NULL;
}

Value *
append(Value *x, Value *y)
{
    if (is_nil(x)) {
        return y;
    } else {
        return cons(car(x), append(cdr(x), y));
    }
}

int
length(Value *l)
{
    if (!is_pair(l)) {
        return 0;
    }

    int len = 0;
    for (; l != NULL; l = cdr(l)) {
        len++;
    }

    return len;
}

void
checkargs(Value *name, Value *params, Value *args)
{
    // if it's a symbol, capture everything
    if (is_symbol(params)) {
        return;
    }

    int nargs = 0;
    int varargs = 0;

    for (Value *p = params; is_pair(p); p = cdr(p)) {
        nargs += 1;

        if (is_symbol(cdr(p))) {
            varargs = 1;
        }
    }

    int len = length(args);

    if (len < nargs && varargs) {
        fprintf(stderr, "%s: expected %d or more arguments, got %d\n", name->sym, nargs, len);
        exit(1);
    } else if (len != nargs && !varargs) {
        fprintf(stderr, "%s: expected %d arguments, got %d\n", name->sym, nargs, len);
        exit(1);
    }
}

Value *
zipargs(Value *x, Value *y)
{
    if (is_nil(x) && is_nil(y)) {
        return NULL;
    } else if (is_symbol(x)) {
        return cons(cons(x, cons(y, NULL)), NULL);
    } else if (is_pair(x) && is_pair(y)) {
        return cons(cons(car(x), cons(car(y), NULL)), zipargs(cdr(x), cdr(y)));
    } else if (is_nil(x) || is_nil(y)) {
        fprintf(stderr, "zipargs: lists not the same length\n");
        exit(1);
    } else {
        fprintf(stderr, "zipargs: expected list\n");
        exit(1);
    }
}

Env *
clone(Env *env)
{
    Env *newenv = xalloc(sizeof(Env));
    newenv->parent = env;
    newenv->bindings = NULL;
    return newenv;
}

// Returns a binding, e.g. '(x 1)
Value *
lookup(Value *name, Env *env)
{
    Value *v;

    for (; env != NULL; env = env->parent) {
        v = assoc(name, env->bindings);
        if (v) {
            return v;
        }
    }

    return NULL;
}

// Returns a value instead of a binding. Can't differentiate
// between name being unbound and name being bound to nil.
Value *
lookupv(Value *name, Env *env)
{
    return cadr(lookup(name, env));
}

void
setname(Value *name, Value *value)
{
    if (is_function(value) || is_macro(value)) {
        value->func.name = name;
    }
}

Value *
def(Value *name, Value *value, Env *env)
{
    env->bindings = cons(cons(name, cons(value, NULL)), env->bindings);
    setname(name, value);
    return value;
}

Value **evalslot(Value *v, Env *env);

Value *
set(Value *lval, Value *value, Env *env)
{
    Value **slot = evalslot(lval, env);

    if (slot == NULL && is_symbol(lval)) {
        fprintf(stderr, "set: undefined variable: %s\n", lval->sym);
        exit(1);
    } else if (slot == NULL) {
        fprintf(stderr, "set: invalid location: ");
        fprint(stderr, lval);
        exit(1);
    }

    *slot = value;

    if (is_symbol(lval)) {
        setname(lval, value);
    }

    return value;
}

Value*
quotelist(Value *l)
{
    assert(is_pair(l) || is_nil(l));

    if (is_pair(l)) {
        return cons(cons(s_quote, cons(car(l), NULL)), quotelist(cdr(l)));
    } else {
        return NULL;
    }
}

Value *evlis(Value *params, Env *env);
Value *eval(Value *v, Env *env);

Value *
apply(Value *f, Value *args, Env *env)
{
    assert(is_function(f) || is_macro(f));

    checkargs(f->func.name, f->func.params, args);
    Value *bindings = zipargs(f->func.params, evlis(args, env));
    Env *newenv = clone(f->func.env);
    newenv->bindings = bindings;

    Value *res;
    for (Value *e = f->func.body; is_pair(e); e = cdr(e)) {
        res = eval(car(e), newenv);
    }

    return res;
}

Value *expand(Value *v, Env *env);

Value *
expandlist(Value *l, Env *env)
{
    if (is_pair(l)) {
        return cons(expand(car(l), env), expandlist(cdr(l), env));
    } else {
        return l;
    }
}

Value *
expand(Value *v, Env *env)
{
    if (is_pair(v)) {
        v = expandlist(v, env);

        // if car(v) is not a symbol, lookupv will return nil
        Value *macro = lookupv(car(v), env);
        if (!is_macro(macro)) {
            return v;
        }

        Value *args = quotelist(cdr(v));

        return expand(apply(macro, args, env), env);
    } else {
        return v;
    }
}

Value *
evif(Value *conditions, Env *env)
{
    if (is_nil(conditions)) {
        return NULL;
    } else if (length(conditions) == 1) {
        return eval(car(conditions), env);
    } else if (eval(car(conditions), env)) {
        return eval(cadr(conditions), env);
    } else {
        return evif(cddr(conditions), env);
    }
}

Value **
evifslot(Value *conditions, Env *env)
{
    if (is_nil(conditions)) {
        return NULL;
    } else if (length(conditions) == 1) {
        return evalslot(car(conditions), env);
    } else if (eval(car(conditions), env)) {
        return evalslot(cadr(conditions), env);
    } else {
        return evifslot(cddr(conditions), env);
    }
}

Value *
evlis(Value *params, Env *env)
{
    assert(is_pair(params) || is_nil(params));

    if (is_nil(params)) {
        return NULL;
    } else {
        return cons(eval(car(params), env), evlis(cdr(params), env));
    }
}

Value **
evalslot(Value *v, Env *env)
{
    if (is_pair(v) && car(v) == s_car) {
        Value *p = eval(cadr(v), env);

        if (is_pair(p)) {
            return &p->pair.car;
        } else {
            return NULL;
        }
    } else if (is_pair(v) && car(v) == s_cdr) {
        Value *p = eval(cadr(v), env);

        if (is_pair(p)) {
            return &p->pair.cdr;
        } else {
            return NULL;
        }
    } else if (is_pair(v) && car(v) == s_if) {
        return evifslot(cdr(v), env);
    } else if (is_pair(v) && car(v) == s_def) {
        eval(v, env);
        return evalslot(cadr(v), env);
    } else if (is_pair(v) && car(v) == s_set) {
        eval(v, env);
        return evalslot(cadr(v), env);
    } else if (is_pair(v)) {
        Value *f = eval(car(v), env);

        if (is_function(f)) {
            checkargs(f->func.name, f->func.params, cdr(v));
            Value *bindings = zipargs(f->func.params, evlis(cdr(v), env));
            Env *newenv = clone(f->func.env);
            newenv->bindings = bindings;

            Value **slot;
            for (Value *e = f->func.body; is_pair(e); e = cdr(e)) {
                if (is_pair(cdr(e))) {
                    eval(car(e), newenv);
                } else {
                    slot = evalslot(car(e), newenv);
                }
            }

            return slot;
        } else {
            return NULL;
        }
    } else if (is_symbol(v)) {
        Value *binding = lookup(v, env);
        if (binding == NULL) {
            return NULL;
        }

        return &binding->pair.cdr->pair.car;
    } else {
        return NULL;
    }
}

Value *
evalquasi(Value *v, Env *env)
{
    if (is_pair(v) && car(v) == s_unquote) {
        return eval(cadr(v), env);
    } else if (is_pair(v) && car(v) == s_unquote_splicing) {
        fprintf(stderr, "evalquasi: unquote-splicing not in list: ");
        fprint(stderr, v);
        exit(1);
    } else if (is_pair(v) && caar(v) == s_unquote_splicing) {
        Value *p = eval(cadar(v), env);

        if (!is_pair(p) && !is_nil(p)) {
            fprintf(stderr, "evalquasi: expected list, got: ");
            fprint(stderr, p);
            exit(1);
        }

        return append(p, evalquasi(cdr(v), env));
    } else if (is_pair(v)) {
        return cons(evalquasi(car(v), env), evalquasi(cdr(v), env));
    } else {
        return v;
    }
}

Env *globals = NULL;

Value *
eval(Value *v, Env *env)
{
    if (is_pair(v) && car(v) == s_quote) {
        return cadr(v);
    } else if (is_pair(v) && car(v) == s_quasiquote) {
        return evalquasi(cadr(v), env);
    } else if (is_pair(v) && car(v) == s_if) {
        return evif(cdr(v), env);
    } else if (is_pair(v) && car(v) == s_fn) {
        return mkfunc(FUNCTION, cadr(v), cddr(v), env);
    } else if (is_pair(v) && car(v) == s_macro) {
        return mkfunc(MACRO, cadr(v), cddr(v), env);
    } else if (is_pair(v) && car(v) == s_def && length(v) > 3) {
        // short form lambda definition, e.g. (def inc (x) (+ 1 x))
        return eval(cons(s_def, cons(cadr(v), cons(cons(s_fn, cons(caddr(v), cdddr(v))), NULL))), env);
    } else if (is_pair(v) && car(v) == s_def) {
        Value *name = cadr(v);
        Value *val = eval(caddr(v), env);

        if (!is_symbol(name)) {
            fprintf(stderr, "def: expected symbol\n");
            exit(1);
        }

        Value *old = lookup(name, globals);
        if (old) {
            fprintf(stderr, "def: symbol already defined: ");
            fprint(stderr, name);
            exit(1);
        }

        return def(name, val, globals);
    } else if (is_pair(v) && car(v) == s_set) {
        Value *lvar = cadr(v);
        Value *val = eval(caddr(v), env);

        return set(lvar, val, env);
    } else if (is_pair(v)) {
        Value *f = eval(car(v), env);

        if (is_function(f)) {
            return apply(f, cdr(v), env);
        } else if (is_builtin(f)) {
            return f->builtin.imp(evlis(cdr(v), env));
        } else if (is_macro(f)) {
            fprintf(stderr, "can't call a macro at runtime: ");
            fprint(stderr, car(v));
            exit(1);
        } else {
            fprintf(stderr, "not a function: ");
            fprint(stderr, car(v));
            exit(1);
        }
    } else if (is_symbol(v)) {
        Value *binding = lookup(v, env);

        if (binding) {
            return cadr(binding);
        } else {
            fprintf(stderr, "unbound variable: %s\n", v->sym);
            exit(1);
        }
    } else {
        return v;
    }
}

void
arity(Value *args, int expected, char *name)
{
    int actual = length(args);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %d arguments, got %d\n", name, expected, actual);
        exit(1);
    }
}

void
varity(Value *args, int min, char *name)
{
    int actual = length(args);

    if (actual < min) {
        fprintf(stderr, "%s: expected %d or more arguments, got %d\n", name, min, actual);
        exit(1);
    }
}

int
allints(Value *l)
{
    for (; l != NULL; l = cdr(l)) {
        if (!is_integer(car(l))) {
            return 0;
        }
    }

    return 1;
}

#define builtin1(name) \
    Value *builtin_##name(Value *args) { \
        arity(args, 1, #name); \
        return name(car(args)); \
    }

#define builtin2(name) \
    Value *builtin_##name(Value *args) { \
        arity(args, 2, #name); \
        return name(car(args), cadr(args)); \
    }

#define pred1(name) \
    Value *builtin_is_##name(Value *args) { \
        arity(args, 1, #name); \
        return is_##name(car(args)) ? s_t : NULL; \
    }

#define pred2(name) \
    Value *builtin_is_##name(Value *args) { \
        arity(args, 2, #name); \
        return is_##name(car(args), cadr(args)) ? s_t : NULL; \
    }

#define op(op, name, init) \
    Value *builtin_##name(Value *args) { \
        if (!allints(args)) { return NULL; } \
        int len = length(args); \
        if (len == 0) { return mkint(init op init); } \
        if (len == 1) { return mkint(init op car(args)->n); } \
        long long res = car(args)->n; \
        for (args = cdr(args); args != NULL; args = cdr(args)) { \
            res = res op car(args)->n; \
        } \
        return mkint(res); \
    }

#define comp(op, name) \
    Value *builtin_##name(Value *args) { \
        if (!allints(args)) { return NULL; } \
        if (length(args) == 0) { return t; } \
        Value *last = car(args); \
        for (args = cdr(args); args != NULL; args = cdr(args)) { \
            if (!(last->n op car(args)->n)) { \
                return NULL; \
            } \
            last = car(args); \
        } \
        return t; \
    }

builtin1(car)
builtin1(cdr)
builtin2(cons)

Value *
builtin_length(Value *args)
{
    arity(args, 1, "length");
    return mkint(length(car(args)));
}

pred1(nil)
pred1(symbol)
pred1(string)
pred1(integer)
pred1(pair)
pred1(function)
pred1(builtin)
pred1(procedure)

pred2(eq)
pred2(eqv)
pred2(equal)

op(+, plus, 0)
op(-, minus, 0)
op(*, times, 1)
op(/, divide_, 1)

Value *
builtin_divide(Value *args)
{
    varity(args, 1, "/");

    for (Value *a = args; is_pair(a); a = cdr(a)) {
        if (car(a)->n == 0) {
            fprintf(stderr, "/: division by zero\n");
            exit(1);
        }
    }

    return builtin_divide_(args);
}

comp(>, gt)
comp(<, lt)
comp(>=, ge)
comp(<=, le)
// == for the c operator, but = in def_op below for Lisp.
// different from is_eq, which is eq?
comp(==, eq)

Value *
load(char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "load: can't open %s\n", path);
        exit(1);
    }

    while (peek(f) != EOF) {
        Value *v = read(f);
        v = expand(v, globals);
        eval(v, globals);
    }

    fclose(f);

    return NULL;
}

Value *
builtin_load(Value *args)
{
    arity(args, 1, "load");

    Value *path = car(args);

    if (!is_string(path)) {
        fprintf(stderr, "load: path must be a string\n");
        exit(1);
    }

    return load(path->str->s);
}

#define symbol(name) s_##name = intern(#name)
#define def_builtin(name) def(intern(#name), mkbuiltin(#name, builtin_##name), globals)
#define def_pred(name) def(intern(#name "?"), mkbuiltin(#name "?", builtin_is_##name), globals)
#define def_op(op, name) def(intern(#op), mkbuiltin(#op, builtin_##name), globals)

int
main(int argc, char *argv[])
{
    gcinit();

    globals = clone(NULL);
    gcroot(globals);

    symbol(t); t = s_t; // return t seems more ergonomic and clear than return s_t
    def(t, t, globals);
    symbol(nil);

    symbol(car);
    symbol(cdr);
    symbol(quote);
    symbol(quasiquote);
    symbol(unquote);
    s_unquote_splicing = intern("unquote-splicing");
    symbol(if);
    symbol(fn);
    symbol(macro);
    symbol(def);
    symbol(set);
    symbol(car);
    symbol(cdr);

    def_builtin(car);
    def_builtin(cdr);
    def_builtin(cons);
    def_builtin(length);

    def_pred(nil);
    def_pred(symbol);
    def_pred(string);
    def_pred(integer);
    def_pred(pair);
    def_pred(function);
    def_pred(builtin);
    def_pred(procedure);

    def_pred(eq);
    def_pred(eqv);
    def_pred(equal);

    def_builtin(print);
    def_builtin(load);

    def_op(+, plus);
    def_op(-, minus);
    def_op(*, times);
    def_op(/, divide);

    def_op(>, gt);
    def_op(<, lt);
    def_op(>=, ge);
    def_op(<=, le);
    // = for Lisp land, but == above in comp for the C operator.
    def_op(=, eq);

    load("lib.lisp");

    while (peek(stdin) != EOF) {
        Value *v = read(stdin);
        v = expand(v, globals);
        v = eval(v, globals);
        print(v);
    }
    return 0;
}
