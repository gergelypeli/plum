
class ClassPreinitializerValue: public Value {
public:
    ClassPreinitializerValue(TypeSpec rts)
        :Value(rts.prefix(partial_type)) {
    }
    
    virtual Regs precompile(Regs preferred) {
        return Regs().add(RAX);
    }
    
    virtual Storage compile(X64 *x64) {
        TypeSpec class_ts = ts.unprefix(partial_type).unprefix(reference_type);
        unsigned heap_size = class_ts.measure(MEMORY);
        
        x64->op(MOVQ, RAX, heap_size);
        std::cerr << "XXX Allocating " << heap_size << " on the heap.\n";
        x64->op(LEARIP, RBX, x64->empty_function_label);  // TODO: fill finalizer
        x64->alloc_RAX_RBX();

        x64->op(LEARIP, RBX, class_ts.get_virtual_table_label());
        x64->op(MOVQ, Address(RAX, 0), RBX);
        
        return Storage(REGISTER, RAX);
    }
};

