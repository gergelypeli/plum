
int rbtree_elem_size(TypeSpec elem_ts) {
    return ::stack_size(elem_ts.measure(MEMORY).concretize()) + RBNODE_HEADER_SIZE;
}


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_alloc_rbtree(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - reservation
    int elem_size = rbtree_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_rbtree_alloc");
    
    alloc_container(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    x64->op(RET);
}


void compile_realloc_rbtree(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = rbtree_elem_size(elem_ts);

    x64->code_label_local(label, "x_rbtree_realloc");

    realloc_container(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_grow_rbtree(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough
    Label realloc_label = x64->once->compile(compile_realloc_rbtree, elem_ts);

    x64->code_label_local(label, "x_rbtree_grow");
    //x64->log("x_rbtree_grow");
    grow_container(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


// Register usage:
// RAX - index of current node
// RBX - return of operation
// RCX, RDX - child indexes
// RSI - address of the tree
// RDI - address of key (input), dark soul (output during removal)

void compile_left_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_left_fix");
    // RSI - tree, RAX - node
    // RBX - result
    // RCX, RDX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree left fix.");
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);  // RBX - red left child

    x64->op(MOVQ, RCX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, outer_nonred);  // RCX - outer red
    
    x64->code_label(outer_red);  // Red-promoting right rotate
    //x64->log("Rbtree red-promoting right rotate.");
    x64->op(MOVQ, RDX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_LEFT_OFFSET), RDX);
    
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_RIGHT_OFFSET), RAX);
    
    x64->op(ANDQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken outer
    x64->op(RET);  // the red left child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, RDX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(RSI, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping left rotate.");
    x64->op(MOVQ, RCX, Address(RSI, RDX, RBNODE_LEFT_OFFSET));  // TODO
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_RIGHT_OFFSET), RCX);
    
    x64->op(MOVQ, Address(RSI, RDX, RBNODE_LEFT_OFFSET), RBX);
    
    x64->op(MOVQ, RCX, RBX);
    x64->op(MOVQ, RBX, RDX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red left child
    
    x64->code_label(ok);
    //x64->log("Rbtree left fix ok.");
    x64->op(MOVQ, RBX, RAX);
    x64->op(RET);
}


void compile_right_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_right_fix");
    // RSI - tree, RAX - node
    // RBX - result
    // RCX, RDX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree right fix.");
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);  // RBX - red right child

    x64->op(MOVQ, RCX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, outer_nonred);  // RCX - outer red
    
    x64->code_label(outer_red);  // Red-promoting left rotate
    //x64->log("Rbtree red-promoting left rotate.");
    x64->op(MOVQ, RDX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_RIGHT_OFFSET), RDX);
    
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_LEFT_OFFSET), RAX);
    
    x64->op(ANDQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken outer
    x64->op(RET);  // the red right child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, RDX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(RSI, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping right rotate.");
    x64->op(MOVQ, RCX, Address(RSI, RDX, RBNODE_RIGHT_OFFSET));  // TODO
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_LEFT_OFFSET), RCX);
    
    x64->op(MOVQ, Address(RSI, RDX, RBNODE_RIGHT_OFFSET), RBX);
    
    x64->op(MOVQ, RCX, RBX);
    x64->op(MOVQ, RBX, RDX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red right child
    
    x64->code_label(ok);
    //x64->log("Rbtree right fix ok.");
    x64->op(MOVQ, RBX, RAX);
    x64->op(RET);
}


void compile_other_fix(Label label, X64 *x64) {
    Label redden_side, materialize;
    Label left_fix = x64->once->compile(compile_left_fix);
    Label right_fix = x64->once->compile(compile_right_fix);

    {
        x64->code_label_local(label, "rbtree_other_fix");
        // RSI - tree, RAX - node, RBX - child, RDI - dark soul
        // RBX - result, RDI - dark soul
        // RCX, RDX - clob
        Label no_dark_soul, right, fixed;
        
        x64->op(TESTQ, RDI, 1);
        x64->op(JE, no_dark_soul);
        
        x64->op(CALL, redden_side);
        
        x64->op(CALL, materialize);
        
        x64->op(CMPQ, RBX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
        x64->op(JNE, right);
        
        // Left fix
        x64->op(CALL, left_fix);
        x64->op(CMPQ, RBX, RAX);
        x64->op(JE, fixed);
        
        // Got rotated right
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, left_fix);  // run again on the new right (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(RSI, RBX, RBNODE_RIGHT_OFFSET)); // put on the right of the new root
        x64->op(JMP, fixed);

        x64->code_label(right);
        
        // Right fix
        x64->op(CALL, right_fix);
        x64->op(CMPQ, RBX, RAX);
        x64->op(JE, fixed);
        
        // Got rotated left
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, right_fix);  // run again on the new left (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(RSI, RBX, RBNODE_LEFT_OFFSET)); // put on the left of the new root

        x64->code_label(fixed);

        x64->op(XCHGQ, RBX, RAX);
        x64->op(CALL, materialize);  // on the new root
        x64->op(XCHGQ, RBX, RAX);
        x64->op(RET);
        
        x64->code_label(no_dark_soul);
        x64->op(MOVQ, RBX, RAX);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(redden_side, "rbtree_redden_side");
        // RSI - tree, RBX - child
        // RCX - clob
        Label black;
    
        //x64->log("Rbtree redden side.");
        x64->op(TESTQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(JE, black);
    
        // Redden the children, there must be two, because black height is positive
        x64->op(MOVQ, RCX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
        x64->op(ORQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(MOVQ, RCX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
        x64->op(ORQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(RET);
    
        x64->code_label(black);
        x64->op(ORQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(materialize, "_materialize");
        // RSI - tree, RAX - node, RDI - dark soul
        // RDI - dark soul
        Label end;
    
        //x64->log("Rbtree materialize.");
        x64->op(TESTQ, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(JE, end);
    
        x64->op(TESTQ, RDI, 1);
        x64->op(JE, end);
    
        x64->op(ANDQ, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET), -2);
        x64->op(MOVQ, RDI, 0);
    
        x64->code_label(end);
        x64->op(RET);
    }
}


void compile_allocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_allocate");
    // In: RSI - tree, RBX - node size
    // Out: RAX - node
    // Clob: RBX
    Label no_vacancy, no_reservation, init, no_last, end;

    //x64->log("Rbtree allocate.");
    x64->op(MOVQ, RAX, Address(RSI, RBTREE_VACANT_OFFSET));
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, no_vacancy);
    
    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(RSI, RBTREE_VACANT_OFFSET), RBX);
    x64->op(JMP, init);
    
    x64->code_label(no_vacancy);
    x64->op(MOVQ, RAX, Address(RSI, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, RAX, Address(RSI, RBTREE_RESERVATION_OFFSET));
    x64->op(JE, no_reservation);
    
    x64->op(INCQ, Address(RSI, RBTREE_LENGTH_OFFSET));
    x64->op(IMUL2Q, RAX, RBX);
    x64->op(ADDQ, RAX, RBTREE_HEADER_SIZE);
    x64->op(JMP, init);
    
    x64->code_label(no_reservation);
    x64->die("Rbtree full!");
    
    x64->code_label(init);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_RIGHT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_NEXT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, RBX, Address(RSI, RBTREE_LAST_OFFSET));
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET), RBX);
    x64->op(ORQ, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET), 1);  // red
    
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_last);
    
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_NEXT_OFFSET), RAX);
    x64->op(JMP, end);
    
    x64->code_label(no_last);
    x64->op(MOVQ, Address(RSI, RBTREE_FIRST_OFFSET), RAX);
    
    x64->code_label(end);
    x64->op(MOVQ, Address(RSI, RBTREE_LAST_OFFSET), RAX);
    x64->op(RET);
}


void compile_deallocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_deallocate");
    // In: RSI - tree, RAX - node
    // Clob: RBX, RCX
    Label no_prev, prev_ok, no_next, next_ok;
    //x64->log("Rbtree deallocate.");
    
    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET));
    x64->op(ANDQ, RBX, -2);
    x64->op(MOVQ, RCX, Address(RSI, RAX, RBNODE_NEXT_OFFSET));

    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_prev);
    
    x64->op(MOVQ, Address(RSI, RBX, RBNODE_NEXT_OFFSET), RCX);
    x64->op(JMP, prev_ok);
    
    x64->code_label(no_prev);
    x64->op(MOVQ, Address(RSI, RBTREE_FIRST_OFFSET), RCX);
    
    x64->code_label(prev_ok);
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, no_next);
    
    // set prev while keeping the color
    x64->op(SHRQ, RBX, 1);
    x64->op(SHRQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);  // color to CF
    x64->op(RCLQ, RBX, 1);
    x64->op(MOVQ, Address(RSI, RCX, RBNODE_PREV_IS_RED_OFFSET), RBX);
    x64->op(JMP, next_ok);
    
    x64->code_label(no_next);
    x64->op(MOVQ, Address(RSI, RBTREE_LAST_OFFSET), RBX);
    
    x64->code_label(next_ok);
    x64->op(MOVQ, RBX, Address(RSI, RBTREE_VACANT_OFFSET));
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_NEXT_OFFSET), RBX);
    x64->op(MOVQ, Address(RSI, RBTREE_VACANT_OFFSET), RAX);
    
    x64->op(DECQ, Address(RSI, RBTREE_LENGTH_OFFSET));
    x64->op(RET);
}


void compile_has(Label label, TypeSpec elem_ts, X64 *x64) {
    // RSI - tree
    // RAX - node / return bool
    // RDI - key
    x64->code_label_local(label, "rbtree_has");

    Label loop, no, less, greater;

    x64->code_label(loop);
    //x64->log("Has loop.");
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, no);

    Storage ks(MEMORY, Address(RDI, 0));
    Storage vs(MEMORY, Address(RSI, RAX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, less, greater);
    
    x64->op(MOVQ, RAX, 1);
    x64->op(RET);
    
    x64->code_label(less);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(JMP, loop);
    
    x64->code_label(greater);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(JMP, loop);
    
    x64->code_label(no);
    x64->op(MOVQ, RAX, 0);
    x64->op(RET);
}


void compile_add(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects RSI - tree, RAX - index, RDI - key
    // Returns RBX - new index
    // Clobbers RCX, RDX
    x64->code_label_local(label, "rbtree_add");
    
    Label less, greater, no;
    Label left_fix = x64->once->compile(compile_left_fix);
    Label right_fix = x64->once->compile(compile_right_fix);
    Label allocate = x64->once->compile(compile_allocate);
    int key_size = ::stack_size(elem_ts.measure(MEMORY).concretize());
    int node_size = key_size + RBNODE_HEADER_SIZE;
    
    //x64->log("Rbtree add.");
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(RDI, 0));
    Storage vs(MEMORY, Address(RSI, RAX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, less, greater);
    
    // Found the value, nothing to do
    //x64->log("Rbtree add found.");
    x64->op(MOVQ, RBX, RAX);
    x64->op(RET);
    
    x64->code_label(less);
    //x64->log("Rbtree add left.");
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RAX);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_LEFT_OFFSET), RBX);
    x64->op(CALL, left_fix);
    x64->op(RET);
    
    x64->code_label(greater);
    //x64->log("Rbtree add right.");
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RAX);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_RIGHT_OFFSET), RBX);
    x64->op(CALL, right_fix);
    x64->op(RET);
    
    x64->code_label(no);
    //x64->log("Rbtree add missing.");
    x64->op(MOVQ, RBX, node_size);
    x64->op(CALL, allocate);  // from RSI to RAX
    elem_ts.create(ks, vs, x64);
    x64->op(MOVQ, RBX, RAX);
    x64->op(RET);
}


void compile_remove(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects RSI - tree, RAX - index, RDI - key / dark soul
    // Returns RBX - new index
    // Clobbers RCX, RDX
    x64->code_label_local(label, "rbtree_remove");
    
    Label no, remove_left, remove_right;
    Label deallocate = x64->once->compile(compile_deallocate);
    Label other_fix = x64->once->compile(compile_other_fix);
    
    //x64->log("Rbtree remove.");
    x64->op(CMPQ, RAX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(RDI, 0));  // can't use STACK, that would be popped!
    Storage vs(MEMORY, Address(RSI, RAX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, remove_left, remove_right);
    
    // Found the value, remove it
    Label no_left, no_right, no_children, internal_loop, was_red;
    //x64->log("Rbtree remove found.");
    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_left);
    
    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_right);
    
    // Find the smallest greater element as replacement
    //x64->log("Rbtree remove found internal.");
    x64->code_label(internal_loop);
    x64->op(MOVQ, RCX, RBX);
    x64->op(MOVQ, RBX, Address(RSI, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JNE, internal_loop);
    
    Storage rs(MEMORY, Address(RSI, RCX, RBNODE_VALUE_OFFSET));
    elem_ts.store(rs, vs, x64);
    
    x64->op(LEA, RDI, Address(RSI, RCX, RBNODE_VALUE_OFFSET));
    x64->op(JMP, remove_left);  // Yes, this is a goto :-)
    
    x64->code_label(no_right);
    // A single red left child can be the replacement
    //x64->log("Rbtree remove found left only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, deallocate);  // At RAX
    x64->op(POPQ, RBX);  // return the left child
    x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken
    x64->op(MOVQ, RDI, 0);  // no dark soul
    x64->op(RET);
        
    x64->code_label(no_left);
    x64->op(MOVQ, RDX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, no_children);
    
    // A single red right child can be the replacement
    //x64->log("Rbtree remove found right only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, deallocate);  // At RAX
    x64->op(POPQ, RBX);  // return the right child
    x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken
    x64->op(MOVQ, RDI, 0);  // no dark soul
    x64->op(RET);
    
    // No children, just remove
    x64->code_label(no_children);
    //x64->log("Rbtree remove found leaf.");
    elem_ts.destroy(vs, x64);
    x64->op(CALL, deallocate);
    x64->op(MOVQ, RDI, 0);  // assume no dark soul
    x64->op(TESTQ, Address(RSI, RAX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JNE, was_red);
    
    //x64->log("Rbtree remove found leaf releasing dark soul.");
    x64->op(MOVQ, RDI, 1);  // well
    
    x64->code_label(was_red);
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(RET);
    
    // Descend to the left
    x64->code_label(remove_left);
    //x64->log("Rbtree remove left.");
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RAX);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_LEFT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Descend to the right
    x64->code_label(remove_right);
    //x64->log("Rbtree remove right.");
    x64->op(PUSHQ, RAX);
    x64->op(MOVQ, RAX, Address(RSI, RAX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RAX);
    x64->op(MOVQ, Address(RSI, RAX, RBNODE_RIGHT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(RSI, RAX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Not found
    x64->code_label(no);
    //x64->log("Rbtree remove missing.");
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(MOVQ, RDI, 0);  // no dark soul
    x64->op(RET);
}


void compile_next(Label label, X64 *x64) {
    // Expects RSI - tree, RAX - it
    // Returns RAX - new it or 0, RBX new index
    // Clobbers RCX, RDX
    x64->code_label_local(label, "rbtree_next");
    Label terminate, find_leftmost, loop_leftmost, loop_check, loop, right_step, stepped, no_right;
    
    x64->op(MOVQ, RBX, Address(RSI, RBTREE_ROOT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, terminate);
    
    x64->op(MOVQ, RCX, 1);  // mask
    x64->op(CMPQ, RAX, 0);
    x64->op(JE, find_leftmost);
    
    x64->op(MOVQ, RDX, -2);  // inverse full mask
    x64->op(PUSHQ, RBNODE_NIL);  // last_left_index
    x64->op(PUSHQ, 0);  // last_left_mask
    x64->op(JMP, loop_check);
    
    x64->code_label(loop);
    x64->op(TESTQ, RAX, RCX);  // the mask bit is then a direction
    x64->op(JNE, right_step);
    
    x64->op(MOVQ, Address(RSP, 8), RBX);
    x64->op(MOVQ, Address(RSP, 0), RCX);
    x64->op(MOVQ, RBX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
    x64->op(JMP, stepped);
    
    x64->code_label(right_step);
    x64->op(MOVQ, RBX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
    
    x64->code_label(stepped);
    x64->op(SHLQ, RCX, 1);
    x64->op(SHLQ, RDX, 1);
    
    x64->code_label(loop_check);
    x64->op(TESTQ, RAX, RDX);  // are there bits set above mask?
    x64->op(JNE, loop);
    
    // Found the previous node
    x64->op(MOVQ, RBX, Address(RSI, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_right);
    
    x64->op(SHLQ, RCX, 1);
    x64->op(ADDQ, RSP, 16);
    x64->op(JMP, find_leftmost);
    
    x64->code_label(no_right);
    x64->op(POPQ, RCX);  // last_left_mask
    x64->op(POPQ, RBX);  // last_left_index
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, terminate);
    
    // Backtrack to followup ancestor
    x64->op(MOVQ, RDX, RCX);
    x64->op(DECQ, RDX);
    x64->op(ANDQ, RAX, RDX);
    x64->op(ORQ, RAX, RCX);
    x64->op(RET);
    
    x64->code_label(loop_leftmost);
    x64->op(MOVQ, RBX, RDX);
    x64->op(SHLQ, RCX, 1);
    
    x64->code_label(find_leftmost);
    x64->op(MOVQ, RDX, Address(RSI, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JNE, loop_leftmost);
    
    x64->op(ORQ, RAX, RCX);
    x64->op(RET);
    
    x64->code_label(terminate);
    x64->op(MOVQ, RAX, 0);
    x64->op(RET);
}


class RbtreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc_rbtree, elem_ts);
        
        x64->op(MOVQ, RAX, 0);
        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeReservedValue(TypeSpec ts)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = right->precompile(preferred);
        return clob.add(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_alloc_rbtree, elem_ts);

        right->compile_and_store(x64, Storage(REGISTER, RAX));

        x64->op(CALL, alloc_label);
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeInitializerValue: public ContainerInitializerValue {
public:
    RbtreeInitializerValue(TypeSpec ts)
        :ContainerInitializerValue(ts) {
    }

    virtual Storage compile(X64 *x64) {
        // This won't use the base class subcompile method, because that's inappropriate here.
        Label alloc_label = x64->once->compile(compile_alloc_rbtree, elem_ts);
        Label add_label = x64->once->compile(compile_add, elem_ts);
        int stack_size = ::stack_size(elem_ts.measure(STACK).concretize());
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, alloc_label);
        //x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), elems.size());
        x64->op(PUSHQ, RAX);
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));

            x64->op(MOVQ, RDI, RSP);  // save key address for stack usage
            x64->op(MOVQ, RSI, Address(RSP, stack_size));  // Rbtree without incref
            x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));

            x64->op(CALL, add_label);

            x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
            x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken root
        
            elem_ts.store(Storage(STACK), Storage(), x64);
        }
        
        return Storage(STACK);
    }
};


class RbtreeLengthValue: public GenericValue {
public:
    Register reg;
    
    RbtreeLengthValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, INTEGER_TS, l) {
        reg = NOREG;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        reg = clob.get_any();
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);

        switch (ls.where) {
        case REGISTER:
            x64->decref(ls.reg);
            x64->op(MOVQ, ls.reg, Address(ls.reg, RBTREE_LENGTH_OFFSET));
            return Storage(REGISTER, ls.reg);
        case MEMORY:
            x64->op(MOVQ, reg, ls.address);
            x64->op(MOVQ, reg, Address(reg, RBTREE_LENGTH_OFFSET));
            return Storage(REGISTER, reg);
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class RbtreeHasValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeHasValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1].varvalue(), BOOLEAN_TS, pivot) {
        elem_ts = match[1].varvalue();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY).concretize());
        Label has = x64->once->compile(compile_has, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(MOVQ, RDI, RSP);  // save key address for stack usage
        x64->op(MOVQ, RSI, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));
        
        x64->op(CALL, has);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeAddValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeAddValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1].varvalue(), VOID_TS, pivot) {
        elem_ts = match[1].varvalue();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY).concretize());
        Label add = x64->once->compile(compile_add, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDI, RSP);  // save key address for stack usage
        x64->op(MOVQ, RSI, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));

        x64->op(CALL, add);

        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(RSI, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken root
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class RbtreeRemoveValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeRemoveValue(Value *pivot, TypeMatch &match)
        :GenericValue(match[1], VOID_TS, pivot) {
        elem_ts = match[1].varvalue();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI).add(RDI);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY).concretize());
        Label remove = x64->once->compile(compile_remove, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDI, RSP);  // save key address for stack usage
        x64->op(MOVQ, RSI, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RAX, Address(RSI, RBTREE_ROOT_OFFSET));

        x64->op(CALL, remove);

        x64->op(MOVQ, Address(RSI, RBTREE_ROOT_OFFSET), RBX);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class RbtreeAutogrowValue: public ContainerAutogrowValue {
public:
    RbtreeAutogrowValue(Value *l, TypeMatch &match)
        :ContainerAutogrowValue(l, match) {
    }
    
    virtual Storage compile(X64 *x64) {
        return subcompile(RBTREE_RESERVATION_OFFSET, RBTREE_LENGTH_OFFSET, compile_grow_rbtree, x64);
    }
};


// Iteration

class RbtreeElemByAgeIterValue: public SimpleRecordValue {
public:
    RbtreeElemByAgeIterValue(Value *l, TypeMatch &match)
        :SimpleRecordValue(typesubst(SAME_RBTREEELEMBYAGEITER_TS, match), l) {
    }

    virtual Storage compile(X64 *x64) {
        left->compile_and_store(x64, Storage(STACK));
        
        x64->op(POPQ, RBX);
        x64->op(PUSHQ, Address(RBX, RBTREE_FIRST_OFFSET));
        x64->op(PUSHQ, RBX);
        
        return Storage(STACK);
    }
};


class RbtreeNextElemByAgeValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;

    RbtreeNextElemByAgeValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[1].varvalue(), l) {
        is_down = false;  // TODO: get as argument for backward iteration!
        elem_ts = match[1].varvalue();
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob.add(RAX);
        
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);  // iterator
        Register reg = (clob & ~ls.regs()).get_any();
        Label ok;
        
        switch (ls.where) {
        case MEMORY:
            x64->op(MOVQ, RBX, ls.address + REFERENCE_SIZE);  // offset
            x64->op(MOVQ, reg, ls.address); // tree reference without incref
            x64->op(CMPQ, RBX, RBNODE_NIL);
            x64->op(JNE, ok);
            
            x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
            x64->unwind->initiate(dummy, x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, RBX);
            
            if (is_down) {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_PREV_IS_RED_OFFSET));
                x64->op(ANDQ, RBX, -2);  // remove color bit
            }
            else {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_NEXT_OFFSET));
            }
            
            x64->op(MOVQ, ls.address + REFERENCE_SIZE, RBX);
            
            return Storage(MEMORY, Address(reg, RBNODE_VALUE_OFFSET));
        default:
            throw INTERNAL_ERROR;
        }
    }
};


class RbtreeElemByOrderIterValue: public ContainerIterValue {
public:
    RbtreeElemByOrderIterValue(Value *l, TypeMatch &match)
        :ContainerIterValue(typesubst(SAME_RBTREEELEMBYORDERITER_TS, match), l) {
    }
};


class RbtreeNextElemByOrderValue: public GenericValue {
public:
    Declaration *dummy;
    Regs clob;
    TypeSpec elem_ts;

    RbtreeNextElemByOrderValue(Value *l, TypeMatch &match)
        :GenericValue(VOID_TS, match[1].varvalue(), l) {
        elem_ts = match[1].varvalue();
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        dummy = new Declaration;
        scope->add(dummy);
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        clob.add(RAX).add(RCX).add(RDX).add(RSI);
        
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        Label next_label = x64->once->compile(compile_next);
        Label ok;

        left->compile_and_store(x64, Storage(ALISTACK));  // iterator

        x64->op(MOVQ, RCX, Address(RSP, 0));
        x64->op(MOVQ, RAX, Address(RCX, REFERENCE_SIZE));  // it
        x64->op(MOVQ, RSI, Address(RCX, 0)); // tree reference without incref

        x64->op(CALL, next_label);
        
        x64->op(POPQ, RCX);
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);

        x64->op(MOVB, EXCEPTION_ADDRESS, DONE_EXCEPTION);
        x64->unwind->initiate(dummy, x64);
        
        x64->code_label(ok);
        x64->op(MOVQ, Address(RCX, REFERENCE_SIZE), RAX);  // save it
        x64->op(LEA, RAX, Address(RSI, RBX, RBNODE_VALUE_OFFSET));

        return Storage(MEMORY, Address(RAX, 0));
    }
};
