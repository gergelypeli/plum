

class ClassType: public HeapType {
public:
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    ClassType(std::string name, int pc)
        :HeapType(name, pc) {
    }

    virtual void complete_type() {
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->var_ts.rvalue());
                member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Class " << name << " has " << member_variables.size() << " member variables.\n";
    }

    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return inner_scope->get_size(tsi);
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        Type::store(tsi, s, t, x64);
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        // Assume the target MEMORY is uninitialized
        
        switch (s.where * t.where) {
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->create(tsi, Storage(), t, x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY) {
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tsi, s, x64);
        }
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter, bool is_arg, bool is_lvalue) {
        return (is_arg ? throw INTERNAL_ERROR : (is_lvalue ? MEMORY : throw INTERNAL_ERROR));
    }

    virtual Storage boolval(TypeSpecIter , Storage s, X64 *x64, bool probe) {
        throw INTERNAL_ERROR;
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (name == "{}") {
            std::cerr << "Classes can't be initialized anonymously!\n";
            return NULL;
        }
        else {
            // Named initializer
            TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
            Value *pre = make_class_preinitializer_value(rts);

            Value *value = inner_scope->lookup(name, pre);

            // FIXME: check if the method is Void!
            if (value)
                return make_cast_value(value, rts);
        }
        
        std::cerr << "Can't initialize class as " << name << "!\n";
        return NULL;
    }

    virtual DataScope *make_inner_scope(TypeSpec pts) {
        DataScope *is = HeapType::make_inner_scope(pts);
        
        is->be_virtual_scope();
        
        TypeSpec cts = { reference_type, this };
        is->set_meta_scope(class_metatype->get_inner_scope(cts.begin()));

        int vt_offset = is->reserve(CLASS_HEADER_SIZE);  // VT pointer
        if (vt_offset != CLASS_VT_OFFSET)  // sanity check
            throw INTERNAL_ERROR;
        
        return is;
    }

    virtual std::vector<TypeSpec> get_member_tss(TypeMatch &match) {
        std::vector<TypeSpec> tss;
        for (auto &ts : member_tss)
            tss.push_back(typesubst(ts, match));
        return tss;
    }
    
    virtual std::vector<std::string> get_member_names() {
        return member_names;
    }

    virtual std::vector<Variable *> get_member_variables() {
        return member_variables;
    }

    virtual std::vector<Function *> get_virtual_table(TypeSpecIter tsi) {
        return inner_scope->get_virtual_table();
    }

    virtual Label get_virtual_table_label(TypeSpecIter tsi, X64 *x64) {
        return x64->once->compile(compile_virtual_table, TypeSpec(tsi));
    }

    virtual Label get_finalizer_label(TypeSpecIter tsi, X64 *x64) {
        return x64->once->compile(compile_finalizer, TypeSpec(tsi));
    }
    
    static void compile_virtual_table(Label label, TypeSpec ts, X64 *x64) {
        std::vector<Function *> vt = ts.get_virtual_table();

        x64->data_align();
        x64->data_label_local(label, "x_virtual_table");  // FIXME: ambiguous name!

        for (auto f : vt) {
            if (f)
                x64->data_reference(f->x64_label);
            else
                x64->data_qword(0);  // data references are now 64-bit absolute addresses
        }
    }
    
    static void compile_finalizer(Label label, TypeSpec ts, X64 *x64) {
        x64->code_label_local(label, "x_finalizer");  // FIXME: ambiguous name!
        
        ts.destroy(Storage(MEMORY, Address(RAX, 0)), x64);

        x64->op(RET);
    }
};


class StackType: public ClassType {
public:
    StackType(std::string name)
        :ClassType(name, 1) {
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec ats = TypeSpec(tsi).unprefix(stack_type).prefix(array_type);
        Value *array_initializer = ats.lookup_initializer(name, scope);
        
        if (!array_initializer) {
            std::cerr << "No Stack initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
        Value *stack_preinitializer = make_class_preinitializer_value(rts);
        
        return make_class_wrapper_initializer_value(stack_preinitializer, array_initializer);
    }
};


class QueueType: public ClassType {
public:
    QueueType(std::string name)
        :ClassType(name, 1) {
    }
    
    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec cts = TypeSpec(tsi).unprefix(queue_type).prefix(circularray_type);
        Value *carray_initializer = cts.lookup_initializer(name, scope);
        
        if (!carray_initializer) {
            std::cerr << "No Queue initializer called " << name << "!\n";
            return NULL;
        }

        TypeSpec rts = TypeSpec(tsi).prefix(reference_type);
        Value *queue_preinitializer = make_class_preinitializer_value(rts);
        
        return make_class_wrapper_initializer_value(queue_preinitializer, carray_initializer);
    }
};


