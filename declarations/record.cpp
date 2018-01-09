
class RecordType: public Type {
public:
    Scope *inner_scope;
    
    std::vector<Variable *> member_variables;
    std::vector<TypeSpec> member_tss;  // rvalues, for the initializer arguments
    std::vector<std::string> member_names;

    RecordType(std::string n, int pc)
        :Type(n, pc) {
        inner_scope = NULL;
    }

    virtual void set_inner_scope(Scope *is) {
        inner_scope = is;
        
        for (auto &c : inner_scope->contents) {
            Variable *v = dynamic_cast<Variable *>(c.get());
            
            if (v) {
                member_variables.push_back(v);
                member_tss.push_back(v->var_ts.rvalue());
                member_names.push_back(v->name);
            }
        }
        
        std::cerr << "Record " << name << " has " << member_variables.size() << " member variables.\n";
    }
    
    virtual unsigned measure(TypeSpecIter tsi, StorageWhere where) {
        switch (where) {
        case MEMORY:
            return inner_scope->get_size(tsi);
        case STACK:
            return stack_size(inner_scope->get_size(tsi));
        default:
            return Type::measure(tsi, where);
        }
    }

    virtual void store(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        //case REGISTER_STACK:  // for the sake of String, FIXME: check size and stuff!
        //    x64->op(PUSHQ, s.reg);
        //    return;
        case STACK_NOWHERE:
            destroy(tsi, Storage(MEMORY, Address(RSP, 0)), x64);
            x64->op(ADDQ, RSP, measure(tsi, STACK));
            return;
        case STACK_STACK:
            return;
        case STACK_MEMORY:
            store(tsi, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tsi, s, Storage(), x64);
            return;
        case MEMORY_NOWHERE:
            return;
        case MEMORY_STACK:
            x64->op(SUBQ, RSP, measure(tsi, STACK));
            create(tsi, s, Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->store(tsi, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            Type::store(tsi, s, t, x64);
        }
    }

    virtual void create(TypeSpecIter tsi, Storage s, Storage t, X64 *x64) {
        switch (s.where * t.where) {
        case NOWHERE_STACK:
            x64->op(SUBQ, RSP, measure(tsi, STACK));
            create(tsi, Storage(), Storage(MEMORY, Address(RSP, 0)), x64);
            return;
        case NOWHERE_MEMORY:
            for (auto &var : member_variables)
                var->create(tsi, Storage(), Storage(MEMORY, t.address), x64);
            return;
        //case REGISTER_MEMORY:  // for the sake of String, FIXME: check sizes
        //    x64->op(MOVQ, t.address, s.reg);
        //    return;
        case STACK_MEMORY:
            create(tsi, Storage(MEMORY, Address(RSP, 0)), t, x64);
            store(tsi, s, Storage(), x64);
            return;
        case MEMORY_MEMORY:  // duplicates data
            for (auto &var : member_variables)
                var->create(tsi, Storage(MEMORY, s.address), Storage(MEMORY, t.address), x64);
            return;
        default:
            throw INTERNAL_ERROR;
        }
    }

    virtual void destroy(TypeSpecIter tsi, Storage s, X64 *x64) {
        if (s.where == MEMORY)
            for (auto &var : member_variables)  // FIXME: reverse!
                var->destroy(tsi, Storage(MEMORY, s.address), x64);
        else
            throw INTERNAL_ERROR;
    }

    virtual StorageWhere where(TypeSpecIter tsi, bool is_arg, bool is_lvalue) {
        return (is_arg ? (is_lvalue ? ALIAS : MEMORY) : (is_lvalue ? MEMORY : STACK));
    }
    
    virtual Storage boolval(TypeSpecIter tsi, Storage s, X64 *x64, bool probe) {
        Address address;
        
        switch (s.where) {
        case STACK:
            address = Address(RSP, 0);
            break;
        case MEMORY:
            address = s.address;
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        Label done;
        
        for (auto &var : member_variables) {
            Storage t = var->boolval(tsi, Storage(MEMORY, address), x64, true);
            
            if (t.where == FLAGS && t.bitset == SETNE)
                x64->op(JNE, done);
            else
                throw INTERNAL_ERROR;
        }

        x64->code_label(done);
        
        if (!probe) {
            x64->op(SETNE, BL);
            x64->op(PUSHQ, RBX);
            destroy(tsi, Storage(MEMORY, Address(RSP, 8)), x64);
            x64->op(POPQ, RBX);
            x64->op(ADDQ, RSP, measure(tsi, STACK));
            x64->op(CMPB, BL, 0);
        }
        
        return Storage(FLAGS, SETNE);
    }
    
    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        int stack_size = measure(tsi, STACK);

        StorageWhereWhere stw = s.where * t.where;  // s and t may be overwritten

        switch (stw) {
        case STACK_STACK:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            s = Storage(MEMORY, Address(RBP, 8 + stack_size));
            t = Storage(MEMORY, Address(RBP, 8));
            break;
        case STACK_MEMORY:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            s = Storage(MEMORY, Address(RBP, 8));
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, RBP);
            x64->op(MOVQ, RBP, RSP);
            t = Storage(MEMORY, Address(RBP, 8));
            break;
        case MEMORY_MEMORY:
            for (auto &var : member_variables) 
                var->compare(tsi, s, t, x64, less, greater);
            return;
        default:
            throw INTERNAL_ERROR;
        }

        Label xless, xgreater, xend, xclean;

        for (auto &var : member_variables) 
            var->compare(tsi, s, t, x64, xless, xgreater);
            
        x64->op(LEARIP, RBX, xend);
        x64->op(JMP, xclean);
        
        x64->code_label(xless);
        x64->op(LEARIP, RBX, less);
        x64->op(JMP, xclean);
        
        x64->code_label(xgreater);
        x64->op(LEARIP, RBX, greater);
        
        x64->code_label(xclean);
        x64->op(PUSHQ, RBX);

        switch (stw) {
        case STACK_STACK:
            destroy(tsi, t + 8, x64);
            destroy(tsi, s + 8, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, 2 * stack_size);
            break;
        case STACK_MEMORY:
            destroy(tsi, s + 8, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, stack_size);
            break;
        case MEMORY_STACK:
            destroy(tsi, t + 8, x64);
            x64->op(POPQ, RBX);
            x64->op(POPQ, RBP);
            x64->op(ADDQ, RSP, stack_size);
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(JMP, RBX);
        x64->code_label(xend);
    }

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string n, Scope *scope) {
        TypeSpec ts(tsi);

        // NOTE: initializers must only appear in code scopes, and there all types
        // must be concrete, not having free parameters. Also, the automatic variable is
        // put in the local scope, so there will be no pivot for it to derive any
        // type parameters from. 
        
        if (n == "{}") {
            // Anonymous initializer
            TypeMatch match = type_parameters_to_match(ts);
            return make_record_initializer_value(match);
        }
        else {
            // Named initializer
            //Value *dv = make_declaration_by_type("<new>", ts, scope);
            Value *pre = make_record_preinitializer_value(ts.lvalue());

            Value *value = inner_scope->lookup(n, pre);

            // FIXME: check if the method is Void!
            if (value)
                return make_record_postinitializer_value(value);
            
            std::cerr << "Can't initialize record as " << n << "!\n";
        
            // OK, we gonna leak dv here, because it's just not possible to delete it.
            //   error: possible problem detected in invocation of delete operator
            //   error: ‘dv’ has incomplete type
            //   note: neither the destructor nor the class-specific operator delete
            //     will be called, even if they are declared when the class is defined
            // Thanks, C++!
        
            return NULL;
        }
    }
    
    virtual Scope *get_inner_scope(TypeSpecIter tsi) {
        return inner_scope;
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
};


class StringType: public RecordType {
public:
    StringType(std::string n)
        :RecordType(n, 0) {
    }
    
    virtual void compare(TypeSpecIter tsi, Storage s, Storage t, X64 *x64, Label less, Label greater) {
        Label strcmp_label = x64->once(compile_strcmp);

        switch (s.where * t.where) {
        case STACK_STACK:
            x64->op(XCHGQ, RAX, Address(RSP, 8));
            x64->op(XCHGQ, RDX, Address(RSP, 0));
            break;
        case STACK_MEMORY:
            x64->op(PUSHQ, RDX);
            x64->op(XCHGQ, RAX, Address(RSP, 8));
            x64->op(MOVQ, RDX, t.address);
            break;
        case MEMORY_STACK:
            x64->op(PUSHQ, RDX);
            x64->op(XCHGQ, RAX, Address(RSP, 8));
            x64->op(MOVQ, RDX, s.address);
            x64->op(XCHGQ, RAX, RDX);
            break;
        case MEMORY_MEMORY:
            x64->op(PUSHQ, RAX);
            x64->op(PUSHQ, RDX);
            if (t.address.base != RAX) {
                x64->op(MOVQ, RAX, s.address);
                x64->op(MOVQ, RDX, t.address);
            }
            else if (s.address.base != RDX) {
                x64->op(MOVQ, RDX, t.address);
                x64->op(MOVQ, RAX, s.address);
            }
            else {
                x64->op(MOVQ, RAX, t.address);
                x64->op(MOVQ, RDX, s.address);
                x64->op(XCHGQ, RAX, RDX);
            }
            break;
        default:
            throw INTERNAL_ERROR;
        }

        x64->op(PUSHQ, RCX);
        x64->op(PUSHQ, RSI);
        x64->op(PUSHQ, RDI);
        x64->op(CALL, strcmp_label);  // result numerically in RBX, preserved by decref, below
        x64->op(POPQ, RDI);
        x64->op(POPQ, RSI);
        x64->op(POPQ, RCX);
        
        if (t.where == STACK)
            x64->decref(RDX);
            
        if (s.where == STACK)
            x64->decref(RAX);
            
        x64->op(POPQ, RDX);
        x64->op(POPQ, RAX);
        
        x64->op(CMPQ, RBX, 0);
        x64->op(JL, less);
        x64->op(JG, greater);
    }

    static void compile_strcmp(X64 *x64) {
        // Expects RAX and RDX with the arguments, clobbers RCX, RSI, RDI, returns RBX.
        Label equal, less, greater, s_longer, begin;
        x64->op(MOVQ, RBX, 0);  // assume equality
        
        x64->op(CMPQ, RAX, RDX);
        x64->op(JE, equal);
        x64->op(CMPQ, RAX, 0);
        x64->op(JE, less);
        x64->op(CMPQ, RDX, 0);
        x64->op(JE, greater);

        x64->op(MOVQ, RCX, x64->array_length_address(RAX));
        x64->op(CMPQ, RCX, x64->array_length_address(RDX));
        x64->op(JE, begin);
        x64->op(JA, s_longer);
        
        x64->op(MOVQ, RBX, -1);  // t is longer, on common equality t is greater
        x64->op(JMP, begin);

        x64->code_label(s_longer);
        x64->op(MOVQ, RBX, 1);  // s is longer, on common equality s is greater
        x64->op(MOVQ, RCX, x64->array_length_address(RDX));
        
        x64->code_label(begin);
        x64->op(CMPQ, RCX, 0);
        x64->op(JE, equal);
        x64->op(LEA, RSI, x64->array_elems_address(RAX));
        x64->op(LEA, RDI, x64->array_elems_address(RDX));
        x64->op(REPECMPSW);
        x64->op(JE, equal);
        x64->op(JA, greater);
        
        x64->code_label(less);
        x64->op(MOVQ, RBX, -1);
        x64->op(JMP, equal);
        
        x64->code_label(greater);
        x64->op(MOVQ, RBX, 1);
                
        x64->code_label(equal);  // common parts are equal, RBX determines the result
        x64->op(RET);
    }
};


class StackType: public RecordType {
public:
    std::unique_ptr<DataScope> inner_scope;

    StackType(std::string name)
        :RecordType(name, 1) {
        //inner_scope.reset(new DataScope);
        //inner_scope->set_pivot_type_hint(TypeSpec { reference_type, this, any_type });
    }
    
    //virtual Scope *get_inner_scope(TypeSpecIter tsi) {
    //    return inner_scope.get();
    //}

    virtual Value *lookup_initializer(TypeSpecIter tsi, std::string name, Scope *scope) {
        TypeSpec ats = TypeSpec(tsi).unprefix(stack_type).prefix(array_type);
        
        Value *array_initializer = ats.lookup_initializer(name, scope);
        
        if (array_initializer)
            return make_wrapper_value(VOID_TS, TypeSpec(tsi), NO_TS, array_initializer);

        std::cerr << "No Stack initializer called " << name << "!\n";
        return NULL;
    }
};

