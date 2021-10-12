enum Type {
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
        int n;
        List list;
    };
};

void print(Value *v);