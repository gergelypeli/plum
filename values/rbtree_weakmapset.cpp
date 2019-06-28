#include "../plum.h"

#include "rbtree_registers.h"


// Type manipulations for unified case handling

int nosyvalue_offset(TypeSpec elem_ts) {
    if (elem_ts[0] == nosyvalue_type)
        return 0;
    else if (elem_ts[0] == item_type) {
        TypeMatch tm = elem_ts.match();
        
        if (tm[1][0] == nosyvalue_type)
            return 0;
        else if (tm[2][0] == nosyvalue_type)
            return tm[1].measure_stack();
        else
            throw INTERNAL_ERROR;
    }
    else
        throw INTERNAL_ERROR;
}


TypeSpec nosyvalue_heap_ts(TypeSpec elem_ts) {
    if (elem_ts[0] == nosyvalue_type)
        return elem_ts.unprefix(nosyvalue_type);
    else if (elem_ts[0] == item_type) {
        TypeMatch tm = elem_ts.match();
        
        if (tm[1][0] == nosyvalue_type)
            return tm[1].unprefix(nosyvalue_type);
        else if (tm[2][0] == nosyvalue_type)
            return tm[2].unprefix(nosyvalue_type);
        else
            throw INTERNAL_ERROR;
    }
    else
        throw INTERNAL_ERROR;
}


TypeSpec denosy_ts(TypeSpec ts) {
    if (ts[0] == nosyvalue_type)
        return ts.reprefix(nosyvalue_type, ptr_type);
    else
        return ts;
}


TypeSpec item_key_ts(TypeSpec ts) {
    if (ts[0] == item_type) {
        TypeMatch tm = ts.match();
        
        return tm[1];
    }
    else
        return ts;
}


TypeSpec item_value_ts(TypeSpec ts) {
    if (ts[0] == item_type) {
        TypeMatch tm = ts.match();
        
        return tm[2];
    }
    else
        return ts;
}


// Helper functions

void nosytree_fcb_action(Label action_label, TypeSpec elem_ts, Cx *cx) {
    // R10 - callback address, R11 - nosytree address
    int noffset = nosyvalue_offset(elem_ts);
    Label elem_check, elem_loop;
    
    cx->op(MOVQ, RAX, Address(R11, 0));
    cx->op(MOVQ, RCX, Address(RAX, RBTREE_FIRST_OFFSET));
    cx->op(JMP, elem_check);
    
    cx->code_label(elem_loop);
    cx->op(PUSHQ, Address(RAX, RCX, RBNODE_VALUE_OFFSET + noffset));  // object
    cx->op(PUSHQ, R10);  // callback
    cx->op(PUSHQ, R11);  // payload1
    cx->op(PUSHQ, RCX);  // payload2
    
    cx->op(CALL, action_label);  // clobbers all, returns nothing
    
    cx->op(POPQ, RCX);
    cx->op(POPQ, R11);
    cx->op(POPQ, R10);
    cx->op(POPQ, RAX);
    
    cx->op(MOVQ, RAX, Address(R11, 0));
    cx->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
    
    cx->code_label(elem_check);
    cx->op(CMPQ, RCX, RBNODE_NIL);
    cx->op(JNE, elem_loop);
}


void compile_nosytree_callback(Label label, TypeSpec elem_ts, Cx *cx) {
    Address payload1_arg_addr(RSP, ADDRESS_SIZE + RIP_SIZE + ADDRESS_SIZE);
    Address payload2_arg_addr(RSP, ADDRESS_SIZE + RIP_SIZE);
    
    Label remove_label = cx->once->compile(compile_rbtree_remove, elem_ts);

    cx->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("callback"));
    cx->prologue();
    
    std::stringstream ss;
    ss << elem_ts << " Nosytree callback";
    cx->runtime->log(ss.str());
    
    // arguments: payload1, payload2
    // We may clobber all registers

    cx->op(MOVQ, SELFX, payload1_arg_addr);  // rbtree ref address
    cx->op(MOVQ, SELFX, Address(SELFX, 0));  // load current rbtree ref
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(MOVQ, KEYX, payload2_arg_addr);  // elem index
    cx->op(LEA, KEYX, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET));  // use the elem key

    cx->op(CALL, remove_label);
    cx->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), R10);
    
    cx->epilogue();
}


void nosy_postadd(TypeSpec elem_ts, Storage ref_storage, Cx *cx) {
    // Add an FCB to the newly added rbtree elem, and decreases the reference count.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the newly added elem
    // Clobbers all registers.
    Label callback_label = cx->once->compile(compile_nosytree_callback, elem_ts);
    TypeSpec heap_ts = nosyvalue_heap_ts(elem_ts);
    int noffset = nosyvalue_offset(elem_ts);
    
    // This Rbtree Ref is the member of a Nosytree, on the heap, so must be ALISTACK.
    if (ref_storage.where != ALISTACK)
        throw INTERNAL_ERROR;
        
    cx->op(MOVQ, R11, Address(RSP, 0));
    
    cx->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(PUSHQ, R10);  // callback
    cx->op(PUSHQ, R11);  // payload1
    cx->op(PUSHQ, KEYX);  // payload2

    cx->op(CALL, cx->runtime->fcb_alloc_label);  // clobbers all
    cx->op(ADDQ, RSP, 3 * ADDRESS_SIZE);
    cx->op(POPQ, R10);  // object
    
    heap_ts.decref(R10, cx);
}


void nosy_postremove(TypeSpec elem_ts, Storage ref_storage, Cx *cx) {
    // Remove an FCB to the newly removed rbtree elem.
    // ref_storage - the storage of the rbtree ref
    // SELFX/KEYX - points to the just removed elem
    // Clobbers all registers.
    Label callback_label = cx->once->compile(compile_nosytree_callback, elem_ts);
    int noffset = nosyvalue_offset(elem_ts);

    // See above
    if (ref_storage.where != ALISTACK)
        throw INTERNAL_ERROR;
        
    cx->op(MOVQ, R11, Address(RSP, 0));
    
    cx->op(PUSHQ, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + noffset));  // object
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(PUSHQ, R10);  // callback
    cx->op(PUSHQ, R11);  // payload1
    cx->op(PUSHQ, KEYX);  // payload2

    cx->op(CALL, cx->runtime->fcb_free_label);  // clobbers all
    cx->op(ADDQ, RSP, 4 * ADDRESS_SIZE);
}


// Nosytree basics

void compile_nosytree_finalizer(Label label, TypeSpec elem_ts, Cx *cx) {
    Address ref_arg_addr(RSP, ADDRESS_SIZE + RIP_SIZE);
    
    cx->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("finalizer"));
    cx->prologue();
    cx->runtime->log("Nosytree finalized.");

    cx->op(MOVQ, RAX, ref_arg_addr);  // pointer arg
    
    Label callback_label = cx->once->compile(compile_nosytree_callback, elem_ts);
        
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(LEA, R11, Address(RAX, NOSYTREE_MEMBER_OFFSET));
    nosytree_fcb_action(cx->runtime->fcb_free_label, elem_ts, cx);  // clobbers all
        
    // If an iterator is referring to this rbtree, it must have increased all
    // reference counts to make sure they continue to point to a valid object.
    // Once we destroy the Rbtree Ref, only iterator(s) will be the owner(s)
    // of this rbtree.

    cx->op(MOVQ, RAX, ref_arg_addr);
    
    TypeSpec member_ts = elem_ts.prefix(rbtree_type).prefix(ref_type);
    member_ts.destroy(Storage(MEMORY, Address(RAX, NOSYTREE_MEMBER_OFFSET)), cx);
    
    cx->epilogue();
}


void alloc_nosytree(TypeSpec elem_ts, Cx *cx) {
    Label finalizer_label = cx->once->compile(compile_nosytree_finalizer, elem_ts);

    cx->op(PUSHQ, REFERENCE_SIZE);
    cx->op(LEA, R10, Address(finalizer_label, 0));
    cx->op(PUSHQ, R10);
    cx->runtime->heap_alloc();  // clobbers all
    cx->op(ADDQ, RSP, 2 * ADDRESS_SIZE);

    // Ref to Nosytree in RAX
}


void compile_nosytree_clone(Label label, TypeSpec elem_ts, Cx *cx) {
    // RAX - Nosytree Ref
    // Return a cloned Ref
    TypeSpec rbtree_heap_ts = elem_ts.prefix(rbtree_type);
    TypeSpec member_ts = rbtree_heap_ts.prefix(ref_type);
    TypeSpec container_heap_ts = member_ts.prefix(nosytree_type);
    
    Label callback_label = cx->once->compile(compile_nosytree_callback, elem_ts);
    Label clone_label = cx->once->compile(compile_rbtree_clone, elem_ts);
    
    cx->code_label_local(label, elem_ts.prefix(nosytree_type).symbolize("clone"));
    cx->prologue();
    cx->runtime->log("XXX Nosytree clone");

    cx->op(MOVQ, R10, Address(RAX, NOSYTREE_MEMBER_OFFSET));  // Rbtree ref
    rbtree_heap_ts.incref(R10, cx);
    cx->op(PUSHQ, R10);
    
    container_heap_ts.decref(RAX, cx);
    
    alloc_nosytree(elem_ts, cx);  // clobbers all
    
    cx->op(XCHGQ, RAX, Address(RSP, 0));  // push new nosy, pop old rbtree
    
    // Cloning the rbtree means that only the clone will be managed by FCB-s, so the
    // original must be referred by an iterator that increased all refcounts to make
    // sure they continue to point to valid objects until the end of the iteration.
    cx->op(CALL, clone_label);
    
    cx->op(POPQ, RBX);
    member_ts.create(Storage(REGISTER, RAX), Storage(MEMORY, Address(RBX, NOSYTREE_MEMBER_OFFSET)), cx);
    cx->op(PUSHQ, RBX);

    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(LEA, R11, Address(RBX, NOSYTREE_MEMBER_OFFSET));
    nosytree_fcb_action(cx->runtime->fcb_alloc_label, elem_ts, cx);
    
    cx->op(POPQ, RAX);
    cx->epilogue();
}



// Internally used access to the Rbtree Ref inside the Nosytree Ref

NosytreeMemberValue::NosytreeMemberValue(Value *p, TypeSpec ets, TypeSpec member_ts)
    :Value(member_ts) {
    pivot.reset(p);
    elem_ts = ets;
}

bool NosytreeMemberValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return Value::check(args, kwargs, scope);
}

void NosytreeMemberValue::need_rvalue() {
    GenericLvalue::need_rvalue();

    // If we're an rvalue, then our pivot is already rvalue-cast by NosytreeTemplateIdentifier
}

Regs NosytreeMemberValue::precompile(Regs preferred) {
    clob = pivot->precompile_tail();
    
    if (!rvalue_needed) {
        // Altering the member would clobber the heap vars
        clob = Regs::allregs() | Regs::heapvars();
    }
    else {
        clob.reserve_gpr(2);
    }

    return clob;
}

Storage NosytreeMemberValue::compile(Cx *cx) {
    Register r;
    
    if (!rvalue_needed) {
        Label clone_label = cx->once->compile(compile_nosytree_clone, elem_ts);

        Storage ps = pivot->compile_lvalue(cx);
        Storage aps = ps.access(0);

        container_cow(clone_label, aps, cx);  // leaves borrowed Ref in RAX
        r = RAX;

        cx->runtime->incref(r);
        
        cx->op(PUSHQ, r);
        cx->op(ADDQ, r, NOSYTREE_MEMBER_OFFSET);
        cx->op(PUSHQ, r);

        return Storage(ALISTACK);
    }
    else {
        // Until we can make sure the member can be borrowed, return a value copy
        Storage s = pivot->compile(cx);
    
        switch (s.where) {
        case REGISTER:
            r = (clob & ~Regs(s.reg)).get_gpr();
            cx->op(MOVQ, r, Address(s.reg, NOSYTREE_MEMBER_OFFSET));
            cx->runtime->incref(r);
            cx->runtime->decref(s.reg);
            return Storage(REGISTER, r);
        case MEMORY:
            r = clob.get_gpr();
            cx->op(MOVQ, r, s.address);
            cx->op(MOVQ, r, Address(r, NOSYTREE_MEMBER_OFFSET));
            cx->runtime->incref(r);
            return Storage(REGISTER, r);
        default:
            throw INTERNAL_ERROR;
        }
    }
}


// Wraps a newly created Rbtree in a Nosytree

WeaktreeValue::WeaktreeValue(Value *pivot, TypeSpec ets, TypeSpec cts)
    :Value(cts) {
    elem_ts = ets;
    member_value.reset(pivot);
}

bool WeaktreeValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    return member_value->check(args, kwargs, scope);  // for the Rbtree initializers
}

Regs WeaktreeValue::precompile(Regs preferred) {
    member_value->precompile_tail();
    return Regs::all();
}

Storage WeaktreeValue::compile(Cx *cx) {
    TypeSpec member_ts = elem_ts.prefix(rbtree_type).prefix(ref_type);
    Label callback_label = cx->once->compile(compile_nosytree_callback, elem_ts);

    member_value->compile_and_store(cx, Storage(STACK));

    alloc_nosytree(elem_ts, cx);  // clobbers all

    member_ts.create(Storage(STACK), Storage(MEMORY, Address(RAX, NOSYTREE_MEMBER_OFFSET)), cx);
    
    // Must set up the FCB-s
    cx->op(PUSHQ, RAX);
    
    cx->op(LEA, R10, Address(callback_label, 0));
    cx->op(LEA, R11, Address(RAX, NOSYTREE_MEMBER_OFFSET));
    nosytree_fcb_action(cx->runtime->fcb_alloc_label, elem_ts, cx);
    
    cx->op(POPQ, RAX);
    return Storage(REGISTER, RAX);
}


// Common operations on Rbtree-s with some nosy elements

NosyRbtreeLengthValue::NosyRbtreeLengthValue(Value *l, TypeSpec elem_ts)
    :RbtreeLengthValue(l, elem_ts) {
}



NosyRbtreeAddValue::NosyRbtreeAddValue(Value *l, TypeSpec elem_ts)
    :RbtreeAddValue(l, elem_ts, denosy_ts(elem_ts)) {
}

Regs NosyRbtreeAddValue::precompile(Regs preferred) {
    return RbtreeAddValue::precompile(preferred) | Regs::all();
}

Storage NosyRbtreeAddValue::postprocess(Storage ps, Cx *cx) {
    // Using the ps+SELFX+KEYX
    nosy_postadd(elem_ts, ps, cx);  // clobbers all
    
    return ps;
}



NosyRbtreeAddItemValue::NosyRbtreeAddItemValue(Value *l, TypeSpec elem_ts)
    :RbtreeAddItemValue(l, item_key_ts(elem_ts), item_value_ts(elem_ts), denosy_ts(item_key_ts(elem_ts)), denosy_ts(item_value_ts(elem_ts))) {
}

Regs NosyRbtreeAddItemValue::precompile(Regs preferred) {
    return RbtreeAddItemValue::precompile(preferred) | Regs::all();
}

Storage NosyRbtreeAddItemValue::postprocess(Storage ps, Cx *cx) {
    // Using the ps+SELFX+KEYX
    nosy_postadd(elem_ts, ps, cx);  // clobbers all
    
    return ps;
}



NosyRbtreeRemoveValue::NosyRbtreeRemoveValue(Value *l, TypeSpec elem_ts)
    :RbtreeRemoveValue(l, elem_ts, denosy_ts(item_key_ts(elem_ts))) {
}

Regs NosyRbtreeRemoveValue::precompile(Regs preferred) {
    return RbtreeRemoveValue::precompile(preferred) | Regs::all();
}

Storage NosyRbtreeRemoveValue::postprocess(Storage ps, Cx *cx) {
    // Using the ps+SELFX+KEYX
    nosy_postremove(elem_ts, ps, cx);  // clobbers all
    
    return ps;
}



NosyRbtreeHasValue::NosyRbtreeHasValue(Value *l, TypeSpec elem_ts)
    :RbtreeHasValue(l, elem_ts, denosy_ts(item_key_ts(elem_ts))) {
}



NosyRbtreeIndexValue::NosyRbtreeIndexValue(Value *l, TypeSpec elem_ts)
    :RbtreeIndexValue(l, item_key_ts(elem_ts), elem_ts, denosy_ts(item_key_ts(elem_ts)), denosy_ts(item_value_ts(elem_ts))) {
    // NOTE: We must not return an Lvalue to this NosyValue!
}
