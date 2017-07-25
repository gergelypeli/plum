
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
        offset = 0;
        var_ts = vts;
    }
    
    virtual Value *matched(Value *cpivot) {
        // cpivot may be NULL if this is a local variable
        return make_variable_value(this, cpivot);
    }
    
    virtual void allocate() {
        offset = outer_scope->reserve(var_ts.measure());
        //std::cerr << "Variable " << name << " offset is " << offset << "\n";
    }
    
    virtual Storage get_storage(Storage s) {
        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // for now, variables are all in memory
            
        //std::cerr << "Variable " << name << " offset is now " << offset << "\n";
        return Storage(MEMORY, s.address + offset);
    }
    
    virtual void finalize(FinalizationType ft, Storage s, X64 *x64) {
        var_ts.destroy(get_storage(s), x64);
        
        Identifier::finalize(ft, s, x64);
    }
};


Variable *variable_cast(Declaration *decl) {
    return dynamic_cast<Variable *>(decl);
}


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

    virtual void allocate() {
    }
};


class ImportedFunction: public Function {
public:
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, TypeSpec rts)
        :Function(n, pts, ats, ans, rts) {
        import_name = in;
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, import_name);
    }
};


class IntegerOperation: public Identifier {
public:
    GenericOperation operation;
    
    IntegerOperation(std::string n, TypeSpec t, GenericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_integer_operation_value(operation, pivot_ts, cpivot);
    }
};


class BooleanOperation: public Identifier {
public:
    GenericOperation operation;
    
    BooleanOperation(std::string n, TypeSpec t, GenericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        //std::cerr << "YYY: " << cpivot->ts << "\n";
        if (operation == AND)
            return make_boolean_and_value(cpivot);
        else if (operation == OR)
            return make_boolean_or_value(cpivot);
        else
            return make_boolean_operation_value(operation, cpivot);
    }

    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;

        // Don't force a type conversion for and/or, those are polymorphic operations.
        Value *cpivot = (
            (operation == AND || operation == OR) && get_typespec(pivot) != VOID_TS ? pivot :
            (operation != AND && operation != OR) ? convertible(pivot_ts, pivot) :
            throw INTERNAL_ERROR
        );

        //std::cerr << "YYY: " << get_typespec(pivot) << " " << get_typespec(cpivot) << "\n";
            
        if (cpivot)
            return matched(cpivot);
        else
            return NULL;
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


class ArrayIndexing: public Identifier {
public:
    ArrayIndexing(TypeSpec t)
        :Identifier("index", t) {
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_array_item_value(pivot_ts, cpivot);
    }
};


class ArrayConcatenation: public Identifier {
public:
    ArrayConcatenation(TypeSpec t)
        :Identifier("plus", t) {
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_array_concatenation_value(pivot_ts, cpivot);
    }
};


class ArrayReallocation: public Identifier {
public:
    ArrayReallocation(TypeSpec t)
        :Identifier("realloc", t) {
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_array_realloc_value(pivot_ts, cpivot);
    }
};


class ReferenceOperation: public Identifier {
public:
    GenericOperation operation;

    ReferenceOperation(std::string n, TypeSpec t, GenericOperation o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot) {
        return make_reference_operation_value(operation, get_typespec(cpivot), cpivot);
    }
};
