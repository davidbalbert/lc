enum Type {
    SYM,
    INT,
    LIST
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
        char *sym;
        int n;
        List list;
    };
};

void *xalloc(size_t size);

Value *intern(char *s);
Value *integer(int n);
Value *cons(Value *car, Value *cdr);

int is_nil(Value *v);
int is_int(Value *v);
int is_sym(Value *v);
int is_list(Value *v);

void print(Value *v);
void fprint(FILE *stream, Value *v);

Value *cons(Value *car, Value *cdr);
Value *car(Value *v);
Value *cdr(Value *v);
Value *cadr(Value *v);
Value *cddr(Value *v);
Value *caddr(Value *v);
Value *caaddr(Value *v);

