#include "../plum.h"


ReferenceType::ReferenceType(std::string name)
    :Type(name, Metatypes { identity_metatype }, value_metatype) {
}

Allocation ReferenceType::measure(TypeMatch tm) {
    return Allocation(REFERENCE_SIZE);
}

void ReferenceType::store(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    switch (s.where * t.where) {
    case NOWHERE_REGISTER:
        std::cerr << "Reference must be initialized!\n";
        throw TYPE_ERROR;
    case NOWHERE_STACK:
        std::cerr << "Reference must be initialized!\n";
        throw TYPE_ERROR;
    
    case REGISTER_NOWHERE:
        tm[1].decref(s.reg, x64);
        return;
    case REGISTER_REGISTER:
        if (s.reg != t.reg)
            x64->op(MOVQ, t.reg, s.reg);
        return;
    case REGISTER_STACK:
        x64->op(PUSHQ, s.reg);
        return;
    case REGISTER_MEMORY:
        x64->op(XCHGQ, t.address, s.reg);
        tm[1].decref(s.reg, x64);
        return;

    case STACK_NOWHERE:
        x64->op(POPQ, R10);
        tm[1].decref(R10, x64);
        return;
    case STACK_REGISTER:
        x64->op(POPQ, t.reg);
        return;
    case STACK_STACK:
        return;
    case STACK_MEMORY:
        x64->op(POPQ, R10);
        x64->op(XCHGQ, R10, t.address);
        tm[1].decref(R10, x64);
        return;

    case MEMORY_NOWHERE:
        return;
    case MEMORY_REGISTER:
        x64->op(MOVQ, t.reg, s.address);
        tm[1].incref(t.reg, x64);
        return;
    case MEMORY_STACK:
        x64->op(MOVQ, R10, s.address);
        tm[1].incref(R10, x64);
        x64->op(PUSHQ, R10);
        return;
    case MEMORY_MEMORY:  // must work with self-assignment
        x64->op(MOVQ, R10, s.address);
        tm[1].incref(R10, x64);
        x64->op(XCHGQ, R10, t.address);
        tm[1].decref(R10, x64);
        return;
        
    case BSTACK_NOWHERE:
        x64->op(ADDQ, RSP, REFERENCE_SIZE);
        return;
    case BSTACK_REGISTER:
        x64->op(POPQ, t.reg);
        tm[1].incref(t.reg, x64);
        return;
    case BSTACK_STACK:
        x64->op(MOVQ, R10, Address(RSP, 0));
        tm[1].incref(R10, x64);
        return;
    case BSTACK_MEMORY:
        x64->op(POPQ, R10);
        tm[1].incref(R10, x64);
        x64->op(XCHGQ, t.address, R10);
        tm[1].decref(R10, x64);
        return;
    case BSTACK_BSTACK:
        return;

    default:
        Type::store(tm, s, t, x64);
    }
}

void ReferenceType::create(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    // Assume the target MEMORY is uninitialized
    
    switch (s.where * t.where) {
    case NOWHERE_MEMORY:
        std::cerr << "Reference must be initialized!\n";
        throw TYPE_ERROR;
    case REGISTER_MEMORY:
        x64->op(MOVQ, t.address, s.reg);
        return;
    case STACK_MEMORY:
        x64->op(POPQ, t.address);
        return;
    case MEMORY_MEMORY:
        x64->op(MOVQ, R10, s.address);
        tm[1].incref(R10, x64);
        x64->op(MOVQ, t.address, R10);
        return;
    case BSTACK_MEMORY:
        x64->op(POPQ, R10);
        tm[1].incref(R10, x64);
        x64->op(MOVQ, t.address, R10);
        return;
    default:
        throw INTERNAL_ERROR;
    }
}

void ReferenceType::destroy(TypeMatch tm, Storage s, X64 *x64) {
    if (s.where == MEMORY) {
        x64->op(MOVQ, R10, s.address);
        tm[1].decref(R10, x64);
    }
    else
        throw INTERNAL_ERROR;
}

void ReferenceType::equal(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    // No need to handle STACK here, GenericOperationValue takes care of it
    
    switch (s.where * t.where) {
    case REGISTER_REGISTER:
        tm[1].decref(s.reg, x64);
        tm[1].decref(t.reg, x64);
        x64->op(CMPQ, s.reg, t.reg);
        break;
    case REGISTER_MEMORY:
        tm[1].decref(s.reg, x64);
        x64->op(CMPQ, s.reg, t.address);
        break;

    case MEMORY_REGISTER:
        tm[1].decref(t.reg, x64);
        x64->op(CMPQ, s.address, t.reg);
        break;
    case MEMORY_MEMORY:
        x64->op(MOVQ, R10, s.address);
        x64->op(CMPQ, R10, t.address);
        break;
        
    default:
        throw INTERNAL_ERROR;
    }
}

void ReferenceType::compare(TypeMatch tm, Storage s, Storage t, X64 *x64) {
    equal(tm, s, t, x64);
    x64->runtime->r10bcompar(true);
}

void ReferenceType::streamify(TypeMatch tm, X64 *x64) {
    tm[1].streamify(x64);
}

StorageWhere ReferenceType::where(TypeMatch tm, AsWhat as_what) {
    return (
        as_what == AS_VALUE ? REGISTER :
        as_what == AS_VARIABLE ? MEMORY :
        as_what == AS_ARGUMENT ? MEMORY :
        //as_what == AS_PIVOT_ARGUMENT ? MEMORY :
        as_what == AS_LVALUE_ARGUMENT ? ALIAS :
        throw INTERNAL_ERROR
    );
}

Storage ReferenceType::optimal_value_storage(TypeMatch tm, Regs preferred) {
    if (preferred.has_gpr())
        return Storage(REGISTER, preferred.get_gpr());
    else
        return Storage(STACK);
}

Value *ReferenceType::lookup_inner(TypeMatch tm, std::string n, Value *v, Scope *s) {
    //std::cerr << "Ref inner lookup " << tm << " " << n << ".\n";
    Value *value = Type::lookup_inner(tm, n, v, s);
    
    if (value)
        return value;

    return tm[1].lookup_inner(n, v, s);
}

Value *ReferenceType::lookup_initializer(TypeMatch tm, std::string name, Scope *scope) {
    return NULL;
}

Value *ReferenceType::lookup_matcher(TypeMatch tm, std::string name, Value *pivot, Scope *scope) {
    return tm[1].lookup_matcher(name, pivot, scope);
}

devector<VirtualEntry *> ReferenceType::get_virtual_table(TypeMatch tm) {
    return tm[1].get_virtual_table();
}

Label ReferenceType::get_virtual_table_label(TypeMatch tm, X64 *x64) {
    return tm[1].get_virtual_table_label(x64);
}

Label ReferenceType::get_interface_table_label(TypeMatch tm, X64 *x64) {
    return tm[1].get_interface_table_label(x64);
}

Value *ReferenceType::autoconv(TypeMatch tm, Type *target, Value *orig, TypeSpec &ifts) {
    return tm[1].autoconv(target, orig, ifts);
}

void ReferenceType::type_info(TypeMatch tm, X64 *x64) {
    unsigned ts_index = x64->once->type_info(tm[1]);
    x64->dwarf->pointer_type_info(tm[0].symbolize(), ts_index);
}


PointerType::PointerType(std::string name)
    :ReferenceType(name) {
}
