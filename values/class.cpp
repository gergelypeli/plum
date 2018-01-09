
class ClassInitializerValue: public Value {
public:
    ClassType *class_type;
    std::vector<std::unique_ptr<Value>> values;
    std::vector<TypeSpec> member_tss;
    std::vector<std::string> member_names;
    std::vector<Storage> var_storages;
    
    ClassInitializerValue(TypeMatch &match)
        :Value(match[0].prefix(reference_type)) {
        class_type = dynamic_cast<ClassType *>(match[0][0]);
        member_tss = class_type->get_member_tss(match);
        member_names = class_type->get_member_names();
    }
    
    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        ArgInfos infos;

        // Separate loop, so reallocations won't screw us
        for (unsigned i = 0; i < member_tss.size(); i++)
            values.push_back(NULL);
        
        for (unsigned i = 0; i < member_tss.size(); i++)
            infos.push_back(ArgInfo { member_names[i].c_str(), &member_tss[i], scope, &values[i] });
        
        return check_arguments(args, kwargs, infos);
    }

    virtual Regs precompile(Regs preferred) {
        for (auto &v : values)
            if (v)
                v->precompile(preferred);
            
        return Regs::all();  // We're too complex to care
    }
    
    virtual Storage compile(X64 *x64) {
        // FIXME: the initialization must be atomic, not incremental like this!

        unsigned heap_size = ts.unprefix(reference_type).measure(MEMORY);
        x64->op(MOVQ, RAX, heap_size);
        std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, x64->empty_function_label);  // TODO: fill finalizer
        x64->alloc_RAX_RBX();
        x64->op(PUSHQ, RAX);

        x64->op(LEARIP, RBX, ts.get_virtual_table_label());
        x64->op(MOVQ, Address(RAX, 0), RBX);
        
        x64->unwind->push(this);
        
        for (unsigned i = 0; i < values.size(); i++) {
            Variable *var = class_type->member_variables[i];
            TypeSpec var_ts = var->var_ts;
            Value *v = values[i].get();
            
            Storage s;
            int offset = 0;
            
            if (v) {
                // Must leave RAX free
                v->compile_and_store(x64, Storage(STACK));
                s = Storage(STACK);
                offset = var_ts.measure(STACK);
            }

            x64->op(MOVQ, RAX, Address(RSP, offset));
            Storage t = var->get_storage(Storage(MEMORY, Address(RAX, 0)));
            std::cerr << "XXX Loading RAX from RSP+" << offset << ".\n";
            std::cerr << "XXX Storing member from " << s << " to " << t << ".\n";
            
            var_ts.create(s, t, x64);
            
            var_storages.push_back(t);  // This is the RAX-based address
        }
        
        x64->unwind->pop(this);

        return Storage(STACK);
    }
    
    virtual Scope *unwind(X64 *x64) {
        x64->op(PUSHQ, RAX);
        
        for (int i = var_storages.size() - 1; i >= 0; i--) {
            x64->op(MOVQ, RAX, Address(RSP, 8));
            unwind_destroy_var(class_type->member_variables[i]->var_ts, var_storages[i], x64);
        }

        x64->op(POPQ, RAX);
        x64->memfree(RAX);
        
        x64->op(POPQ, RAX);

        return NULL;
    }
};

