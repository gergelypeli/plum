
class Identifier: public Declaration {
public:
    std::string name;
    TypeSpec pivot_ts;

    Identifier(std::string n, TypeSpec pts) {
        name = n;
        pivot_ts = pts;
        
        if (pts == VOID_TS)
            throw INTERNAL_ERROR;
    }

    virtual Value *matched(Value *, TypeMatch &match) {
        std::cerr << "Unmatchable identifier!\n";
        throw INTERNAL_ERROR;
    }
    
    virtual Value *match(std::string n, Value *pivot) {
        if (n != name)
            return NULL;
            
        TypeMatch match;
            
        if (pivot_ts == NO_TS) {
            if (!pivot)
                return matched(NULL, match);
            else
                return NULL;
        }
        else {
            if (typematch(pivot_ts, pivot, match))
                return matched(pivot, match);
            else
                return NULL;
        }
    }
};


class Identity: public Identifier {
public:
    Identity(std::string name, TypeSpec pts)
        :Identifier(name, pts) {
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return cpivot;
    }
};


class Cast: public Identifier {
public:
    TypeSpec cast_ts;
    
    Cast(std::string name, TypeSpec pts, TypeSpec cts)
        :Identifier(name, pts) {
        cast_ts = cts;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_cast_value(cpivot, typesubst(cast_ts, match));
    }
};


class Variable: public Identifier {
public:
    TypeSpec var_ts;
    Allocation offset;
    int virtual_index;
    bool xxx_is_allocated;
    StorageWhere where;
    
    Variable(std::string name, TypeSpec pts, TypeSpec vts)
        :Identifier(name, pts) {
        //offset = 0;
        virtual_index = -1;
        var_ts = vts;
        
        if (var_ts == NO_TS)
            throw INTERNAL_ERROR;
            
        xxx_is_allocated = false;
        where = var_ts.where(false);
    }
    
    virtual void be_argument() {
        where = var_ts.where(true);
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // cpivot may be NULL if this is a local variable
        if (var_ts[0] == lvalue_type && var_ts[1] == role_type)
            return make_role_value(this, cpivot, match);
        else
            return make_variable_value(this, cpivot, match);
    }
    
    virtual void allocate() {
        if (xxx_is_allocated)
            throw INTERNAL_ERROR;
            
        Allocation a = (
            where == MEMORY ? var_ts.measure() :
            where == ALIAS ? Allocation(ALIAS_SIZE) :
            throw INTERNAL_ERROR
        );
        
        if (a.count1 || a.count2 || a.count3)
            std::cerr << "Hohoho, allocating variable " << name << " with size " << a << ".\n";
        
        offset = outer_scope->reserve(a);
        //std::cerr << "Allocated variable " << name << " to " << offset << ".\n";

        if (var_ts[0] == lvalue_type && var_ts[1] == role_type) {
            virtual_index = outer_scope->virtual_reserve(var_ts.get_virtual_table());
        }
        
        xxx_is_allocated = true;
        //std::cerr << "Variable " << name << " offset is " << offset << "\n";
    }
    
    virtual Storage get_storage(TypeMatch tm, Storage s) {
        if (!xxx_is_allocated)
            throw INTERNAL_ERROR;

        if (s.where != MEMORY)
            throw INTERNAL_ERROR;  // all variable containers must use MEMORY
        
        return Storage(where, s.address + offset.concretize(tm));
    }

    virtual Storage get_local_storage() {
        // Without pivot as a function local variable
        return get_storage(TypeMatch(), Storage(MEMORY, Address(RBP, 0)));
    }
    
    virtual void finalize(X64 *x64) {
        // This method is only called on local variables, and it's an overload,
        // so it can't get tsi, but must use a dummy
        Identifier::finalize(x64);  // Place label
        //std::cerr << "Finalizing variable " << name << ".\n";
        
        var_ts.destroy(get_local_storage(), x64);
    }
    
    virtual void create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(var_ts, tm);
        int o = offset.concretize(tm);
        ts.create(s.where == NOWHERE ? s : s + o, t + o, x64);
    }

    virtual void store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
        TypeSpec ts = typesubst(var_ts, tm);
        int o = offset.concretize(tm);
        ts.store(s + o, t + o, x64);
    }

    virtual void destroy(TypeMatch tm, Storage s, X64 *x64) {
        TypeSpec ts = typesubst(var_ts, tm);
        int o = offset.concretize(tm);
        ts.destroy(s + o, x64);
    }
    
    virtual Storage boolval(TypeMatch tm, Storage s, X64 *x64, bool probe) {
        TypeSpec ts = typesubst(var_ts, tm);
        int o = offset.concretize(tm);
        return ts.boolval(s + o, x64, probe);
    }

    virtual void compare(TypeMatch tm, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        TypeSpec ts = typesubst(var_ts, tm);
        int o = offset.concretize(tm);
        ts.compare(s + o, t + o, x64, less, greater);
    }
};


class PartialVariable: public Variable {
public:
    std::set<std::string> initialized_member_names;
    std::vector<Variable *> member_variables;
    
    PartialVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_partial_variable_value(this, cpivot, match);
    }

    virtual void set_member_variables(std::vector<Variable *> mv) {
        member_variables = mv;
    }
    
    virtual void be_initialized(std::string name) {
        initialized_member_names.insert(name);
    }
    
    virtual bool is_initialized(std::string name) {
        return initialized_member_names.count(name) == 1;
    }
    
    virtual Variable *var_initialized(std::string name) {
        for (Variable *v : member_variables)
            if (v->name == name)
                return v;
                
        return NULL;
    }
    
    virtual bool is_complete() {
        return initialized_member_names.size() == member_variables.size();
    }
};


class RetroVariable: public Variable {
public:
    RetroVariable(std::string name, TypeSpec pts, TypeSpec vts)
        :Variable(name, pts, vts) {
    }

    virtual void finalize(X64 *x64) {
        // These are not finalized
    }
};


class Function: public Identifier {
public:
    std::vector<TypeSpec> arg_tss;
    std::vector<std::string> arg_names;
    std::vector<TypeSpec> res_tss;
    TreenumerationType *exception_type;
    int virtual_index;
    bool is_interface_function;
    bool is_initializer_function;

    Label x64_label;
    bool is_sysv;
    
    Function(std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Identifier(n, pts) {
        arg_tss = ats;
        arg_names = ans;
        res_tss = rts;
        exception_type = et;
        virtual_index = -1;
        is_interface_function = false;
        is_initializer_function = false;
        
        is_sysv = false;
    }

    virtual void be_interface_function() {
        is_interface_function = true;
    }

    virtual void be_initializer_function() {
        is_initializer_function = true;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        // TODO: do this properly!
        
        if (is_interface_function) {
            std::cerr << "Oops, interface function " << name << " was called instead of an implementation!\n";
            throw INTERNAL_ERROR;
        }
        
        return make_function_call_value(this, cpivot, match);
    }

    virtual std::vector<TypeSpec> get_result_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : res_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual TypeSpec get_pivot_typespec(TypeMatch &match) {
        return typesubst(pivot_ts, match);
    }
    
    virtual std::vector<TypeSpec> get_argument_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : arg_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_argument_names() {
        return arg_names;
    }

    virtual void allocate() {
        if (outer_scope->is_virtual_scope() && !is_initializer_function) {  // FIXME
            std::vector<Function *> vt;
            vt.push_back(this);
            virtual_index = outer_scope->virtual_reserve(vt);
        }
    }
};


class ImportedFunction: public Function {
public:
    static std::vector<ImportedFunction *> to_be_imported;
    
    static void import_all(X64 *x64) {
        for (auto i : to_be_imported)
            i->import(x64);
    }
    
    std::string import_name;
    
    ImportedFunction(std::string in, std::string n, TypeSpec pts, std::vector<TypeSpec> ats, std::vector<std::string> ans, std::vector<TypeSpec> rts, TreenumerationType *et)
        :Function(n, pts, ats, ans, rts, et) {
        import_name = in;
        to_be_imported.push_back(this);
    }

    virtual void import(X64 *x64) {
        is_sysv = true;
        x64->code_label_import(x64_label, import_name);
    }
};

std::vector<ImportedFunction *> ImportedFunction::to_be_imported;


typedef Value *(*GenericValueFactory)(OperationType, Value *, TypeMatch &);

class GenericOperation: public Identifier {
public:
    GenericValueFactory factory;
    OperationType operation;
    
    GenericOperation(std::string n, TypeSpec t, GenericValueFactory f, OperationType o)
        :Identifier(n, t) {
        factory = f;
        operation = o;
    }

    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return factory(operation, cpivot, match);
    }
};


template <typename T>
class TemplateOperation: public Identifier {
public:
    OperationType operation;

    TemplateOperation(std::string n, TypeSpec t, OperationType o)
        :Identifier(n, t) {
        operation = o;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return new T(operation, cpivot, match);
    }
};


template <typename T>
class TemplateIdentifier: public Identifier {
public:
    TemplateIdentifier(std::string n, TypeSpec t)
        :Identifier(n, t) {
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return new T(cpivot, match);
    }
};


class RecordWrapperIdentifier: public Identifier {
public:
    TypeSpec result_ts;
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    std::string arg_operation_name;
    
    RecordWrapperIdentifier(std::string n,
        TypeSpec pivot_ts, TypeSpec pcts,
        TypeSpec rts, std::string on,
        std::string aon = "")
        :Identifier(n, pivot_ts) {
        result_ts = rts;
        pivot_cast_ts = pcts;
        operation_name = on;
        arg_operation_name = aon;
    }
    
    virtual Value *matched(Value *pivot, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
        
        TypeSpec rts = typesubst(result_ts, match);
        TypeSpec pcts = typesubst(pivot_cast_ts, match);
            
        Value *wrapper = make_record_wrapper_value(pivot, pcts, rts, operation_name, arg_operation_name);
        
        return wrapper;
    }
};


class ClassWrapperIdentifier: public Identifier {
public:
    TypeSpec pivot_cast_ts;
    std::string operation_name;
    bool autogrow;
    
    ClassWrapperIdentifier(std::string n, TypeSpec pivot_ts, TypeSpec pcts, std::string on, bool ag = false)
        :Identifier(n, pivot_ts) {
        pivot_cast_ts = pcts;
        operation_name = on;
        autogrow = ag;
    }
    
    virtual Value *matched(Value *pivot, TypeMatch &match) {
        if (!pivot)
            throw INTERNAL_ERROR;
        
        Scope *is = pivot_ts.get_inner_scope();
        if (is->contents.size() < 1)
            throw INTERNAL_ERROR;
            
        Variable *v = dynamic_cast<Variable *>(is->contents[0].get());
        if (!v)
            throw INTERNAL_ERROR;
        
        Value *member = make_variable_value(v, pivot, match);
        
        if (autogrow) {
            TypeSpec mts = get_typespec(member);
            member = mts.lookup_inner("autogrow", member);
            if (!member) {
                std::cerr << "No autogrow for " << mts << "!\n";
                throw INTERNAL_ERROR;
            }
        }
        
        Value *operation = get_typespec(member).lookup_inner(operation_name, member);
        if (!operation) {
            std::cerr << "No operation " << operation_name << " in " << get_typespec(member) << "!\n";
            throw INTERNAL_ERROR;
        }
        
        return operation;
    }
};


class Yield: public Identifier {
public:
    EvalScope *eval_scope;
    
    Yield(std::string n, EvalScope *es)
        :Identifier(n, NO_TS) {
        eval_scope = es;
    }
    
    virtual Value *matched(Value *cpivot, TypeMatch &match) {
        return make_yield_value(eval_scope);
    }
};
