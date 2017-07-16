
class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
    }
    
    virtual Value *matched(Value *) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;
            
        Value *cpivot = convertible(pivot_ts, pivot);
        
        if (cpivot || pivot_ts == VOID_TS)
            return matched(cpivot);
        else
            return NULL;
    }
};


class Variable: public Identifier {
public:
    TypeSpec var_ts;
    int offset;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        var_ts = vts;
    }
    
    virtual Value *matched(Value *cpivot) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot);
    }
    
    virtual void allocate() {
        offset = outer->reserve(var_ts.measure());
    }
    
    virtual Storage get_storage(Storage s) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // for now, variables are all in memory
            
        return Storage(MEMORY, s.address + offset);
    }
};


class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    TypeSpec ret_ts;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        ret_ts = rts;
        
        is_sysv = false;
    }

    virtual Value *matched(Value *cpivot) {
        return make_function_value(this, cpivot);
    }

    virtual TypeSpec get_return_typespec() {
        return ret_ts;
    }
    
    virtual unsigned get_argument_count() {
        return arg_tss.size();
    }
    
    virtual TypeSpec get_argument_typespec(unsigned i) {
        //std::cerr << "Returning typespec for argument " << i << ".\n";
        return arg_tss[i];
    }

    virtual int get_argument_index(std::string keyword) {  // FIXME
        for (unsigned i = 0; i < arg_names.size(); i++)
            if (arg_names[i] == keyword)
                return i;
                
        return (unsigned)-1;
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, name);  // TODO: mangle import name!
    }

    virtual void allocate() {
        x64_label.allocate();
    }
};


class IntegerOperation: public Identifier {
public:
    NumericOperation operation;
    
    IntegerOperation(std::string n, TypeSpec t, NumericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_integer_operation_value(operation, pivot_ts, cpivot);
    }
};


class BooleanOperation: public Identifier {
public:
    NumericOperation operation;
    
    BooleanOperation(std::string n, TypeSpec t, NumericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_boolean_operation_value(operation, cpivot);
    }
};


class BooleanIf: public Identifier {
public:
    BooleanIf()
        :Identifier("if", BOOLEAN_TS) {
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_boolean_if_value(cpivot);
    }
};
