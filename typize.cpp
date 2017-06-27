
// Stage 4

class Declaration;
class Type;
class Scope;
class FunctionReturnScope;
class FunctionHeadScope;
class FunctionBodyScope;
class FunctionScope;
class Variable;
class Function;

typedef std::vector<Type *> TypeSpec;
typedef TypeSpec::iterator TypeSpecIter;

Type *type_type = NULL;
Type *lvalue_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *boolean_type = NULL;
Type *integer_type = NULL;

TypeSpec BOGUS_TS;
TypeSpec VOID_TS;
TypeSpec BOOLEAN_TS;
TypeSpec INTEGER_TS;
TypeSpec LVALUE_INTEGER_TS;

typedef std::vector<std::unique_ptr<Expr>> Args;
typedef std::map<std::string, std::unique_ptr<Expr>> Kwargs;

bool operator>>(TypeSpec &this_ts, TypeSpec &that_ts);
unsigned measure(TypeSpec &ts);

TypeSpec rvalue(TypeSpec &ts) {
    TypeSpec t = ts;
    
    if (t[0] == lvalue_type)
        t.erase(t.begin());
        
    return t;
}

TypeSpec lvalue(TypeSpec &ts) {
    TypeSpec t = ts;
    
    if (t[0] != lvalue_type)
        t.insert(t.begin(), lvalue_type);
        
    return t;
}

enum ArithmeticOperation {
    COMPLEMENT, NEGATE,
    ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO, EXPONENT,
    OR, XOR, AND, SHIFT_LEFT, SHIFT_RIGHT, 
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, INCOMPARABLE,
    ASSIGN, ASSIGN_ADD, ASSIGN_SUBTRACT, ASSIGN_MULTIPLY, ASSIGN_DIVIDE, ASSIGN_MODULO, ASSIGN_EXPONENT,
    ASSIGN_OR, ASSIGN_XOR, ASSIGN_AND, ASSIGN_SHIFT_LEFT, ASSIGN_SHIFT_RIGHT
};

bool is_unary(ArithmeticOperation o) {
    return o == COMPLEMENT || o == NEGATE;
}

bool is_comparison(ArithmeticOperation o) {
    return o >= EQUAL && o <= INCOMPARABLE;
}

bool is_assignment(ArithmeticOperation o) {
    return o >= ASSIGN;
}

class Value;
Value *typize(Expr *expr, Scope *scope);
TypeSpec get_typespec(Value *v);
Value *make_function_head_value(FunctionHeadScope *s);
Value *make_function_body_value(FunctionBodyScope *s);
Value *make_function_return_value(FunctionReturnScope *s, Value *v);
Value *make_variable_value(Variable *decl, Value *pivot);
Value *make_function_value(Function *decl, Value *pivot);
Value *make_type_value(TypeSpec ts);
Value *make_block_value();
Value *make_function_definition_value(TypeSpec fn_ts, Value *ret, Value *head, Value *body, FunctionScope *fn_scope);
Value *make_declaration_value(std::string name);
Value *make_number_value(std::string text);
Value *make_integer_arithmetic_value(ArithmeticOperation ooperation, TypeSpec ts, Value *pivot);
Value *make_boolean_if_value(Value *pivot);


unsigned round_up(unsigned size) {
    return (size + 7) & ~7;
}


enum StorageWhere {
    // No result
    NOWHERE = 0,
    // Integer or pointer constant
    CONSTANT = 1,
    // The value is in EFLAGS with the specified condition
    FLAGS = 2,
    // The value is in the specified register
    REGISTER = 3,
    // The value is on the top of the stack
    STACK = 4,
    // The value is at the specified address
    MEMORY = 5

    // Not needed yet.
    // the value is at [[RBP + fo] + so], where [RBP + fo] is an outer frame pointer
    //FRAME_FRAME,

    // TODO: there will be multiple reference classes, this needs to be rethought.
    // TODO: and direct member access will only be allowed from methods, and
    // that will use a borrowed reference only.
    // the value is at [STACK + so], STACK is refcounted container (fo=0)
    //STACK_HEAP,
    // the value is at [RAX + so], RAX is refcounted container (fo=0)
    //REGISTER_HEAP,
    // the value is at [[RBP + fo] + so], where [RBP + fo] is refcounted container
    //FRAME_HEAP
};


struct Storage {
    StorageWhere where;
    int value;  // Must be 32-bit only, greater values must be loaded to registers.
    BitSetOp bitset;
    Register reg;
    Address address;
    
    Storage() {
        where = NOWHERE;
        value = 0;
        bitset = NOSET;
        reg = NOREG;
    }

    Storage(StorageWhere w) {
        if (w != STACK) {
            std::cerr << "Incomplete Storage!\n";
            throw INTERNAL_ERROR;
        }
        
        where = w;
        value = 0;
        bitset = NOSET;
        reg = NOREG;
    }

    Storage(StorageWhere w, int v) {
        if (w != CONSTANT) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = v;
        bitset = NOSET;
        reg = NOREG;
    }

    Storage(StorageWhere w, BitSetOp b) {
        if (w != FLAGS) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        bitset = b;
        reg = NOREG;
    }

    Storage(StorageWhere w, Register r) {
        if (w != REGISTER) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        bitset = NOSET;
        reg = r;
    }
    
    Storage(StorageWhere w, Address a) {
        if (w != MEMORY) {
            std::cerr << "Wrong Storage!\n";
            throw INTERNAL_ERROR;
        }

        where = w;
        value = 0;
        bitset = NOSET;
        reg = NOREG;
        address = a;
    }
    
};


std::ostream &operator<<(std::ostream &os, Storage &s) {
    if (s.where == NOWHERE)
        os << "NOWHERE";
    else if (s.where == CONSTANT)
        os << "CONSTANT(" << s.value << ")";
    else if (s.where == FLAGS)
        os << "FLAGS(" << s.bitset << ")";
    else if (s.where == REGISTER)
        os << "REGISTER(" << s.reg << ")";
    else if (s.where == STACK)
        os << "STACK";
    else if (s.where == MEMORY)
        os << "MEMORY(" << s.address.base << "+" << s.address.offset << ")";
    else
        os << "???";
        
    return os;
}


enum StorageWhereWhere {
    NOWHERE_NOWHERE=00, NOWHERE_CONSTANT=01, NOWHERE_FLAGS=02, NOWHERE_REGISTER=03, NOWHERE_STACK=04, NOWHERE_MEMORY=05,
    CONSTANT_NOWHERE=10, CONSTANT_CONSTANT=11, CONSTANT_FLAGS=12, CONSTANT_REGISTER=13, CONSTANT_STACK=14, CONSTANT_MEMORY=15,
    FLAGS_NOWHERE=20, FLAGS_CONSTANT=21, FLAGS_FLAGS=22, FLAGS_REGISTER=23, FLAGS_STACK=24, FLAGS_MEMORY=25,
    REGISTER_NOWHERE=30, REGISTER_CONSTANT=31, REGISTER_FLAGS=32, REGISTER_REGISTER=33, REGISTER_STACK=34, REGISTER_MEMORY=35,
    STACK_NOWHERE=40, STACK_CONSTANT=41, STACK_FLAGS=42, STACK_REGISTER=43, STACK_STACK=44, STACK_MEMORY=45,
    MEMORY_NOWHERE=50, MEMORY_CONSTANT=51, MEMORY_FLAGS=52, MEMORY_REGISTER=53, MEMORY_STACK=54, MEMORY_MEMORY=55
};

StorageWhereWhere operator*(StorageWhere l, StorageWhere r) {
    return (StorageWhereWhere)(l * 10 + r);
}

void store(TypeSpec &ts, Storage s, Storage t, X64 *x64);



#include "typize_declarations.cpp"

#include "typize_values.cpp"


struct {
    const char *name;
    ArithmeticOperation operation;
} integer_rvalue_operations[] = {
    { "unary_minus", NEGATE },
    { "unary_tilde", COMPLEMENT },
    { "exponent", EXPONENT },
    { "shift_left", SHIFT_LEFT },
    { "shift_right", SHIFT_RIGHT },
    { "star", MULTIPLY },
    { "slash", DIVIDE },
    { "percent", MODULO },
    { "and", AND },
    { "plus", ADD },
    { "minus", SUBTRACT },
    { "or", OR },
    { "xor", XOR },
    { "equal", EQUAL },
    { "not_equal", NOT_EQUAL },
    { "less", LESS },
    { "greater", GREATER },
    { "less_equal", LESS_EQUAL },
    { "greater_equal", GREATER_EQUAL },
    { "incomparable", INCOMPARABLE },
    //{ "compare",  },
}, integer_lvalue_operations[] = {
    { "assign", ASSIGN },
    { "plus_assign", ASSIGN_ADD },
    { "minus_assign", ASSIGN_SUBTRACT },
    { "star_assign", ASSIGN_MULTIPLY },
    { "slash-assign", ASSIGN_DIVIDE },
    { "percent_assign", ASSIGN_MODULO },
    { "and_assign", ASSIGN_AND },
    { "or_assign", ASSIGN_OR },
    { "xor_assign", ASSIGN_XOR },
    { "shift_left_assign", ASSIGN_SHIFT_LEFT },
    { "shift_right_assign", ASSIGN_SHIFT_RIGHT }
};

Scope *init_types() {
    Scope *root_scope = new Scope();
    
    type_type = new SpecialType("<Type>", 1);
    root_scope->add(type_type);

    lvalue_type = new LvalueType();
    root_scope->add(lvalue_type);

    function_type = new SpecialType("<Function>", 1);
    root_scope->add(function_type);
    
    void_type = new SpecialType("<Void>", 0);
    root_scope->add(void_type);

    boolean_type = new BasicType("Boolean", 1);
    root_scope->add(boolean_type);

    integer_type = new BasicType("Integer", 8);
    root_scope->add(integer_type);
    
    // BOGUS_TS will contain no Type pointers
    VOID_TS.push_back(void_type);
    BOOLEAN_TS.push_back(boolean_type);
    INTEGER_TS.push_back(integer_type);
    LVALUE_INTEGER_TS.push_back(lvalue_type);
    LVALUE_INTEGER_TS.push_back(integer_type);
    
    std::vector<TypeSpec> INTEGER_TSS = { INTEGER_TS };
    std::vector<TypeSpec> BOOLEAN_TSS = { BOOLEAN_TS };
    
    std::vector<std::string> value_names = { "value" };

    for (auto &item : integer_rvalue_operations)
        root_scope->add(new IntegerArithmeticOperation(item.name, INTEGER_TS, item.operation));

    for (auto &item : integer_lvalue_operations)
        root_scope->add(new IntegerArithmeticOperation(item.name, LVALUE_INTEGER_TS, item.operation));

    //root_scope->add(new BooleanIf());

    root_scope->add(new Function("print", VOID_TS, INTEGER_TSS, value_names, VOID_TS));

    return root_scope;
}


Value *typize(Expr *expr, Scope *scope) {
    if (expr->type == OPEN) {
        if (expr->pivot) {
            std::cerr << "An OPEN had a pivot argument!\n";
            throw INTERNAL_ERROR;
        }

        Value *v = make_block_value()->set_token(expr->token);
        v->check(expr->args, expr->kwargs, scope);
            
        return v;
    }
    else if (expr->type == STATEMENT) {
        if (expr->text == "function") {
            FunctionScope *fn_scope = new FunctionScope();
            scope->add(fn_scope);
            
            Expr *r = expr->pivot.get();
            Scope *rs = fn_scope->add_return_scope();
            Value *ret = r ? typize(r, rs) : NULL;
        
            Expr *h = expr->kwargs["from"].get();
            Scope *hs = fn_scope->add_head_scope();
            Value *head = h ? typize(h, hs) : NULL;
            
            Expr *b = expr->kwargs["as"].get();
            Scope *bs = fn_scope->add_body_scope();
            Value *body = b ? typize(b, bs) : NULL;
            
            TypeSpec fn_ts;
                
            if (ret) {
                if (ret->ts[0] != type_type) {
                    std::cerr << "Function return expression is not a type!\n";
                    throw TYPE_ERROR;
                }

                // Add anon return value variable, so funretscp can compute its size!
                TypeSpec var_ts = ret->ts;
                var_ts[0] = lvalue_type;
                Variable *decl = new Variable("<ret>", VOID_TS, var_ts);
                rs->add(decl);
                    
                fn_ts = ret->ts;
                fn_ts[0] = function_type;
            }
            else {
                fn_ts.push_back(function_type);
                fn_ts.push_back(void_type);
            }

            std::cerr << "Function ts " << fn_ts << "\n";
            
            Value *v = make_function_definition_value(fn_ts, ret, head, body, fn_scope)->set_token(expr->token);
            
            return v;
        }
        else if (expr->text == "return") {
            Expr *r = expr->pivot.get();
            Value *ret = r ? typize(r, scope) : NULL;  // TODO: statement scope? Or already have?

            Value *v = make_function_return_value(scope, ret)->set_token(expr->token);

            return v;
        }
        else if (expr->text == "if") {
            Expr *e = expr->pivot.get();
            Value *condition = e ? typize(e, scope) : NULL;
            TypeSpec cts = get_typespec(condition);
            
            if (!(cts >> BOOLEAN_TS)) {
                std::cerr << "Not a boolean condition!\n";
                throw TYPE_ERROR;
            }
            
            Value *v = make_boolean_if_value(condition)->set_token(expr->token);

            v->set_token(expr->token);
            bool ok = v->check(expr->args, expr->kwargs, scope);
                
            if (!ok) {
                std::cerr << "Argument problem for " << expr->token << "!\n";
                throw TYPE_ERROR;
            }

            return v;
        }
        else {
            std::cerr << "Unknown statement " << expr->token << "!\n";
            throw TYPE_ERROR;
        }
    }
    else if (expr->type == DECLARATION) {
        std::string name = expr->text;
        std::cerr << "Declaring " << name << ".\n";
        
        Value *v = make_declaration_value(name)->set_token(expr->token);
        bool ok = v->check(expr->args, expr->kwargs, scope);
        
        if (!ok) {
            std::cerr << "Couldn't declare " << name << "!\n";
            throw TYPE_ERROR;
        }

        std::cerr << "Declared " << name << ".\n";
        return v;
    }
    else if (expr->type == IDENTIFIER) {
        std::string name = expr->text;
        Value *p = expr->pivot ? typize(expr->pivot.get(), scope) : NULL;
        TypeSpec pts = get_typespec(p);
        
        std::cerr << "Looking up " << pts << " " << name << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            //std::cerr << "Trying a scope...\n";
            
            Value *v = s->lookup(expr->text, p);
        
            if (v) {
                v->set_token(expr->token);
                bool ok = v->check(expr->args, expr->kwargs, scope);
                
                if (!ok) {
                    std::cerr << "Argument problem for " << expr->token << "!\n";
                    throw TYPE_ERROR;
                }

                std::cerr << "Found " << pts << " " << name << " as a " << v->ts << ".\n";
                return v;
            }
        }
        
        std::cerr << "No match for " << pts << " " << name << " at " << expr->token << "!\n";
        throw TYPE_ERROR;
    }
    else if (expr->type == NUMBER) {
        return make_number_value(expr->text)->set_token(expr->token);  // TODO
    }
    else {
        std::cerr << "Can't typize this now: " << expr->token << "!\n";
        throw INTERNAL_ERROR;
    }
}
