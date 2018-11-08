
void compile_rbtree_left_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_left_fix");
    // SELFX - tree, ROOTX - node
    // R10 - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree left fix.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);  // R10 - red left child

    x64->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, outer_nonred);  // THISX - outer red
    
    x64->code_label(outer_red);  // Red-promoting right rotate
    //x64->log("Rbtree red-promoting right rotate.");
    x64->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);
    
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), ROOTX);
    
    x64->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    x64->op(RET);  // the red left child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, THATX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping left rotate.");
    x64->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_LEFT_OFFSET));  // TODO
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), THISX);
    
    x64->op(MOVQ, Address(SELFX, THATX, RBNODE_LEFT_OFFSET), R10);
    
    x64->op(MOVQ, THISX, R10);
    x64->op(MOVQ, R10, THATX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red left child
    
    x64->code_label(ok);
    //x64->log("Rbtree left fix ok.");
    x64->op(MOVQ, R10, ROOTX);
    x64->op(RET);
}


void compile_rbtree_right_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_right_fix");
    // SELFX - tree, ROOTX - node
    // R10 - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree right fix.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);  // R10 - red right child

    x64->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, outer_nonred);  // THISX - outer red
    
    x64->code_label(outer_red);  // Red-promoting left rotate
    //x64->log("Rbtree red-promoting left rotate.");
    x64->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), ROOTX);
    
    x64->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    x64->op(RET);  // the red right child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, THATX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping right rotate.");
    x64->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET));  // TODO
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), THISX);
    
    x64->op(MOVQ, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET), R10);
    
    x64->op(MOVQ, THISX, R10);
    x64->op(MOVQ, R10, THATX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red right child
    
    x64->code_label(ok);
    //x64->log("Rbtree right fix ok.");
    x64->op(MOVQ, R10, ROOTX);
    x64->op(RET);
}


void compile_rbtree_other_fix(Label label, X64 *x64) {
    Label redden_side, materialize;
    Label left_fix = x64->once->compile(compile_rbtree_left_fix);
    Label right_fix = x64->once->compile(compile_rbtree_right_fix);

    {
        x64->code_label_local(label, "rbtree_other_fix");
        // SELFX - tree, ROOTX - node, R10 - child, KEYX - dark soul
        // R10 - result, KEYX - dark soul
        // THISX, THATX - clob
        Label no_dark_soul, right, fixed;
        
        x64->op(CMPQ, KEYX, 0);
        x64->op(JE, no_dark_soul);
        
        x64->op(CALL, redden_side);
        
        x64->op(CALL, materialize);
        
        x64->op(CMPQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
        x64->op(JNE, right);
        
        // Left fix
        x64->op(CALL, left_fix);
        x64->op(CMPQ, R10, ROOTX);
        x64->op(JE, fixed);
        
        // Got rotated right
        x64->op(PUSHQ, R10);  // the new root
        x64->op(CALL, left_fix);  // run again on the new right (old root)
        x64->op(XCHGQ, R10, Address(RSP, 0));
        x64->op(POPQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET)); // put on the right of the new root
        x64->op(JMP, fixed);

        x64->code_label(right);
        
        // Right fix
        x64->op(CALL, right_fix);
        x64->op(CMPQ, R10, ROOTX);
        x64->op(JE, fixed);
        
        // Got rotated left
        x64->op(PUSHQ, R10);  // the new root
        x64->op(CALL, right_fix);  // run again on the new left (old root)
        x64->op(XCHGQ, R10, Address(RSP, 0));
        x64->op(POPQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET)); // put on the left of the new root

        x64->code_label(fixed);

        x64->op(XCHGQ, R10, ROOTX);
        x64->op(CALL, materialize);  // on the new root
        x64->op(XCHGQ, R10, ROOTX);
        x64->op(RET);
        
        x64->code_label(no_dark_soul);
        x64->op(MOVQ, R10, ROOTX);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(redden_side, "rbtree_redden_side");
        // SELFX - tree, R10 - child
        // THISX - clob
        Label black;
    
        //x64->log("Rbtree redden side.");
        x64->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(JE, black);
    
        // Redden the children, there must be two, because black height is positive
        x64->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
        x64->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
        x64->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(RET);
    
        x64->code_label(black);
        x64->op(ORQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(materialize, "_materialize");
        // SELFX - tree, ROOTX - node, KEYX - dark soul
        // KEYX - dark soul
        Label end;
    
        //x64->log("Rbtree materialize.");
        x64->op(TESTQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(JE, end);
    
        x64->op(CMPQ, KEYX, 0);
        x64->op(JE, end);
    
        x64->op(ANDQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);
        x64->op(MOVQ, KEYX, 0);
    
        x64->code_label(end);
        x64->op(RET);
    }
}


void compile_rbtree_allocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_allocate");
    // In: SELFX - tree, R10 - node size
    // Out: ROOTX - node or NIL
    // Clob: R10
    Label no_vacancy, no_reservation, init, no_last, end;

    //x64->log("Rbtree allocate.");
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_VACANT_OFFSET));
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, no_vacancy);
    
    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(SELFX, RBTREE_VACANT_OFFSET), R10);
    x64->op(JMP, init);
    
    x64->code_label(no_vacancy);
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, ROOTX, Address(SELFX, RBTREE_RESERVATION_OFFSET));
    x64->op(JE, no_reservation);
    
    x64->op(IMUL2Q, ROOTX, R10);
    x64->op(ADDQ, ROOTX, RBTREE_HEADER_SIZE);
    x64->op(JMP, init);
    
    x64->code_label(no_reservation);
    x64->op(MOVQ, ROOTX, RBNODE_NIL);
    x64->op(RET);
    
    x64->code_label(init);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, R10, Address(SELFX, RBTREE_LAST_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), R10);
    x64->op(ORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);  // red
    
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, no_last);
    
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_NEXT_OFFSET), ROOTX);
    x64->op(JMP, end);
    
    x64->code_label(no_last);
    x64->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), ROOTX);
    
    x64->code_label(end);
    x64->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), ROOTX);
    x64->op(INCQ, Address(SELFX, RBTREE_LENGTH_OFFSET));
    x64->op(RET);
}


void compile_rbtree_deallocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_deallocate");
    // In: SELFX - tree, ROOTX - node
    // Clob: R10, THISX
    Label no_prev, prev_ok, no_next, next_ok;
    //x64->log("Rbtree deallocate.");
    
    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    x64->op(ANDQ, R10, ~RBNODE_RED_BIT);
    x64->op(MOVQ, THISX, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));

    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, no_prev);
    
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_NEXT_OFFSET), THISX);
    x64->op(JMP, prev_ok);
    
    x64->code_label(no_prev);
    x64->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), THISX);
    
    x64->code_label(prev_ok);
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, no_next);
    
    // set prev while keeping the color
    x64->op(SHRQ, R10, 1);
    x64->op(SHRQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), 1);  // color to CF
    x64->op(RCLQ, R10, 1);
    x64->op(MOVQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), R10);
    x64->op(JMP, next_ok);
    
    x64->code_label(no_next);
    x64->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), R10);
    
    x64->code_label(next_ok);
    x64->op(MOVQ, R10, Address(SELFX, RBTREE_VACANT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), R10);
    x64->op(MOVQ, Address(SELFX, RBTREE_VACANT_OFFSET), ROOTX);
    
    x64->op(DECQ, Address(SELFX, RBTREE_LENGTH_OFFSET));
    x64->op(RET);
}


void compile_rbtree_has(Label label, TypeSpec elem_ts, X64 *x64) {
    // SELFX - tree
    // ROOTX - node
    // KEYX - key / found index or NIL
    x64->code_label_local(label, "rbtree_has");

    Label loop, finish, less, greater;

    x64->code_label(loop);
    //x64->log("Has loop.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, finish);

    Storage ks(MEMORY, Address(KEYX, 0));
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;
        
    elem_ts.compare(ks, vs, x64);
    x64->op(JL, less);
    x64->op(JG, greater);
    
    x64->code_label(finish);
    x64->op(MOVQ, KEYX, ROOTX);  // Found index or NIL
    x64->op(RET);
    
    x64->code_label(less);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(JMP, loop);
    
    x64->code_label(greater);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(JMP, loop);
}


void compile_rbtree_add(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects SELFX - tree, ROOTX - index, KEYX - key
    // Returns R10 - new subtree root, KEYX - index with uninitialized value
    // Clobbers THISX, THATX
    x64->code_label_local(label, "rbtree_add");
    
    Label less, greater, no;
    Label left_fix = x64->once->compile(compile_rbtree_left_fix);
    Label right_fix = x64->once->compile(compile_rbtree_right_fix);
    Label allocate = x64->once->compile(compile_rbtree_allocate);
    int elem_size = elem_ts.measure_stack();
    int node_size = elem_size + RBNODE_HEADER_SIZE;
    
    //x64->runtime->log("Rbtree add.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(KEYX, 0));
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;

    elem_ts.compare(ks, vs, x64);
    x64->op(JL, less);
    x64->op(JG, greater);
    
    // Found the value, destroy to make place for the new one
    //x64->runtime->log("Rbtree add found.");
    elem_ts.destroy(vs, x64);
    x64->op(MOVQ, R10, ROOTX);
    x64->op(MOVQ, KEYX, ROOTX);
    x64->op(RET);
    
    x64->code_label(less);
    //x64->runtime->log("Rbtree add left.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), R10);
    x64->op(CALL, left_fix);
    x64->op(RET);
    
    x64->code_label(greater);
    //x64->runtime->log("Rbtree add right.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), R10);
    x64->op(CALL, right_fix);
    x64->op(RET);
    
    x64->code_label(no);
    //x64->runtime->log("Rbtree add missing.");
    x64->op(MOVQ, R10, node_size);
    x64->op(CALL, allocate);  // from SELFX to ROOTX (may be NIL)
    x64->op(MOVQ, R10, ROOTX);
    x64->op(MOVQ, KEYX, ROOTX);
    x64->op(RET);
}


void compile_rbtree_remove(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects SELFX - tree, ROOTX - index, KEYX - key / dark soul
    // Returns R10 - new index
    // Clobbers THISX, THATX
    x64->code_label_local(label, "rbtree_remove");
    
    Label no, remove_left, remove_right;
    Label deallocate = x64->once->compile(compile_rbtree_deallocate);
    Label other_fix = x64->once->compile(compile_rbtree_other_fix);
    
    //x64->log("Rbtree remove.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(KEYX, 0));  // can't use STACK, that would be popped!
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;

    elem_ts.compare(ks, vs, x64);
    x64->op(JL, remove_left);
    x64->op(JG, remove_right);
    
    // Found the value, remove it
    Label no_left, no_right, no_children, was_red;
    //x64->log("Rbtree remove found.");
    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, no_left);
    
    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, no_right);
    
    // Has two children, find the smallest greater element to swap it with.
    // This violates the ordering for a moment, but since we're about to remove one of
    // them, it will be fine soon.
    // Keep the values at the same index, this saves us a value swap, which is nice for
    // keeping the age ordering, and even nicer for finalizer callbacks.
    
    // R10 already contains the right child, then traverse to the leftmost descendant
    Label leftward_loop, leftward_cond, same_color, child_swap, swapped;
    x64->op(MOVQ, THISX, RBNODE_NIL);  // replacement's parent
    x64->op(JMP, leftward_cond);
    
    //x64->log("Rbtree remove found internal.");
    x64->code_label(leftward_loop);
    x64->op(MOVQ, THISX, R10);
    x64->op(MOVQ, R10, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->code_label(leftward_cond);
    x64->op(CMPQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(JNE, leftward_loop);
    
    // R10 - replacement node, THISX - replacement's parent or NIL
    // Swap the current (ROOTX) and replacement (R10) node tree links
    
    // Swap left child
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(XCHGQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);

    // Swap right child
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(XCHGQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    // Swap color
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    x64->op(XORQ, THATX, Address(SELFX, R10, RBNODE_PRED_OFFSET));
    x64->op(TESTQ, THATX, RBNODE_RED_BIT);
    x64->op(JE, same_color);  // xored == 0
    
    // The color bits differ, so complement both of them
    x64->op(XORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(XORQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    
    x64->code_label(same_color);
    // Make the replacement parent point to the current node
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, child_swap);
    
    // The replacement is not the child of current, so just adjust the parent's left link
    x64->op(MOVQ, Address(SELFX, THISX, RBNODE_LEFT_OFFSET), ROOTX);
    x64->op(JMP, swapped);
    
    // The replacement is the child of current, so adjust the replacement's right link
    x64->code_label(child_swap);
    x64->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), ROOTX);
    
    // The replacement took the place of the current node, so continue searching for
    // the same value on the right, because that's where we swapped it to.
    x64->code_label(swapped);
    x64->op(MOVQ, ROOTX, R10);
    x64->op(JMP, remove_right);
    
    x64->code_label(no_right);
    // A single red left child can be the replacement
    //x64->log("Rbtree remove found left only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, deallocate);  // At ROOTX
    x64->op(POPQ, R10);  // return the left child
    x64->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
    x64->op(MOVQ, KEYX, 0);  // no dark soul
    x64->op(RET);
        
    x64->code_label(no_left);
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, THATX, RBNODE_NIL);
    x64->op(JE, no_children);
    
    // A single red right child can be the replacement
    //x64->log("Rbtree remove found right only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, deallocate);  // At ROOTX
    x64->op(POPQ, R10);  // return the right child
    x64->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
    x64->op(MOVQ, KEYX, 0);  // no dark soul
    x64->op(RET);
    
    // No children, just remove
    x64->code_label(no_children);
    //x64->log("Rbtree remove found leaf.");
    elem_ts.destroy(vs, x64);
    x64->op(CALL, deallocate);
    x64->op(MOVQ, KEYX, 0);  // assume no dark soul
    x64->op(TESTQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JNE, was_red);
    
    //x64->log("Rbtree remove found leaf releasing dark soul.");
    x64->op(MOVQ, KEYX, 1);  // well
    
    x64->code_label(was_red);
    x64->op(MOVQ, R10, RBNODE_NIL);
    x64->op(RET);
    
    // Descend to the left
    x64->code_label(remove_left);
    //x64->log("Rbtree remove left.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), R10);

    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Descend to the right
    x64->code_label(remove_right);
    //x64->log("Rbtree remove right.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), R10);

    x64->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Not found
    x64->code_label(no);
    //x64->log("Rbtree remove missing.");
    x64->op(MOVQ, R10, RBNODE_NIL);
    x64->op(MOVQ, KEYX, 0);  // no dark soul
    x64->op(RET);
}


void compile_rbtree_next(Label label, X64 *x64) {
    // Expects SELFX - tree, RAX - it
    // Returns RAX - new it or 0, R10 new index
    // Clobbers RCX, RDX
    x64->code_label_local(label, "rbtree_next");
    Label terminate, find_leftmost, loop_leftmost, loop_check, loop, right_step, stepped, no_right;
    
    x64->op(MOVQ, R10, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
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
    
    x64->op(MOVQ, Address(RSP, 8), R10);
    x64->op(MOVQ, Address(RSP, 0), RCX);
    x64->op(MOVQ, R10, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(JMP, stepped);
    
    x64->code_label(right_step);
    x64->op(MOVQ, R10, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    
    x64->code_label(stepped);
    x64->op(SHLQ, RCX, 1);
    x64->op(SHLQ, RDX, 1);
    
    x64->code_label(loop_check);
    x64->op(TESTQ, RAX, RDX);  // are there bits set above mask?
    x64->op(JNE, loop);
    
    // Found the previous node
    x64->op(MOVQ, R10, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, no_right);
    
    x64->op(SHLQ, RCX, 1);
    x64->op(ADDQ, RSP, 16);
    x64->op(JMP, find_leftmost);
    
    x64->code_label(no_right);
    x64->op(POPQ, RCX);  // last_left_mask
    x64->op(POPQ, R10);  // last_left_index
    x64->op(CMPQ, R10, RBNODE_NIL);
    x64->op(JE, terminate);
    
    // Backtrack to followup ancestor
    x64->op(MOVQ, RDX, RCX);
    x64->op(DECQ, RDX);
    x64->op(ANDQ, RAX, RDX);
    x64->op(ORQ, RAX, RCX);
    x64->op(RET);
    
    x64->code_label(loop_leftmost);
    x64->op(MOVQ, R10, RDX);
    x64->op(SHLQ, RCX, 1);
    
    x64->code_label(find_leftmost);
    x64->op(MOVQ, RDX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JNE, loop_leftmost);
    
    x64->op(ORQ, RAX, RCX);
    x64->op(RET);
    
    x64->code_label(terminate);
    x64->op(MOVQ, RAX, 0);
    x64->op(RET);
}

