
int rbtree_elem_size(TypeSpec elem_ts) {
    return elem_ts.measure_stack() + RBNODE_HEADER_SIZE;
}


// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RSI points to the tree, then RSI + RAX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

void compile_rbtree_alloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - reservation
    int elem_size = rbtree_elem_size(elem_ts);
    Label finalizer_label = elem_ts.prefix(rbtree_type).get_finalizer_label(x64);
    
    x64->code_label_local(label, "x_rbtree_alloc");
    
    container_alloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, finalizer_label, x64);

    x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), 0);
    x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBNODE_NIL);
    
    x64->op(RET);
}


void compile_rbtree_realloc(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    int elem_size = rbtree_elem_size(elem_ts);

    x64->code_label_local(label, "x_rbtree_realloc");

    container_realloc(RBTREE_HEADER_SIZE, elem_size, RBTREE_RESERVATION_OFFSET, x64);

    x64->op(RET);
}


void compile_rbtree_grow(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - array, RBX - new reservation
    // Double the reservation until it's enough
    Label realloc_label = x64->once->compile(compile_rbtree_realloc, elem_ts);

    x64->code_label_local(label, "x_rbtree_grow");
    //x64->log("x_rbtree_grow");
    container_grow(RBTREE_RESERVATION_OFFSET, RBTREE_MINIMUM_RESERVATION, realloc_label, x64);
    
    x64->op(RET);
}


// Register usage:
// ROOTX - index of current node
// RBX - return of operation
// THISX, THATX - child indexes
// SELFX - address of the tree
// KEYX - address of key (input), dark soul (output during removal)

#define SELFX RSI
#define KEYX  RDI
#define ROOTX RAX
#define THISX RCX
#define THATX RDX

void compile_rbtree_left_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_left_fix");
    // SELFX - tree, ROOTX - node
    // RBX - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree left fix.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);  // RBX - red left child

    x64->op(MOVQ, THISX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, outer_nonred);  // THISX - outer red
    
    x64->code_label(outer_red);  // Red-promoting right rotate
    //x64->log("Rbtree red-promoting right rotate.");
    x64->op(MOVQ, THATX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);
    
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET), ROOTX);
    
    x64->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    x64->op(RET);  // the red left child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, THATX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, THATX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping left rotate.");
    x64->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_LEFT_OFFSET));  // TODO
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET), THISX);
    
    x64->op(MOVQ, Address(SELFX, THATX, RBNODE_LEFT_OFFSET), RBX);
    
    x64->op(MOVQ, THISX, RBX);
    x64->op(MOVQ, RBX, THATX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red left child
    
    x64->code_label(ok);
    //x64->log("Rbtree left fix ok.");
    x64->op(MOVQ, RBX, ROOTX);
    x64->op(RET);
}


void compile_rbtree_right_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_right_fix");
    // SELFX - tree, ROOTX - node
    // RBX - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //x64->log("Rbtree right fix.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);  // RBX - red right child

    x64->op(MOVQ, THISX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, outer_nonred);  // THISX - outer red
    
    x64->code_label(outer_red);  // Red-promoting left rotate
    //x64->log("Rbtree red-promoting left rotate.");
    x64->op(MOVQ, THATX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_LEFT_OFFSET), ROOTX);
    
    x64->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    x64->op(RET);  // the red right child is the new root
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, THATX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, THATX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(JE, ok);
    
    //x64->log("Rbtree red-swapping right rotate.");
    x64->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET));  // TODO
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_LEFT_OFFSET), THISX);
    
    x64->op(MOVQ, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET), RBX);
    
    x64->op(MOVQ, THISX, RBX);
    x64->op(MOVQ, RBX, THATX);
    x64->op(JMP, outer_red);  // the red inner grandchild is the new red right child
    
    x64->code_label(ok);
    //x64->log("Rbtree right fix ok.");
    x64->op(MOVQ, RBX, ROOTX);
    x64->op(RET);
}


void compile_rbtree_other_fix(Label label, X64 *x64) {
    Label redden_side, materialize;
    Label left_fix = x64->once->compile(compile_rbtree_left_fix);
    Label right_fix = x64->once->compile(compile_rbtree_right_fix);

    {
        x64->code_label_local(label, "rbtree_other_fix");
        // SELFX - tree, ROOTX - node, RBX - child, KEYX - dark soul
        // RBX - result, KEYX - dark soul
        // THISX, THATX - clob
        Label no_dark_soul, right, fixed;
        
        x64->op(CMPQ, KEYX, 0);
        x64->op(JE, no_dark_soul);
        
        x64->op(CALL, redden_side);
        
        x64->op(CALL, materialize);
        
        x64->op(CMPQ, RBX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
        x64->op(JNE, right);
        
        // Left fix
        x64->op(CALL, left_fix);
        x64->op(CMPQ, RBX, ROOTX);
        x64->op(JE, fixed);
        
        // Got rotated right
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, left_fix);  // run again on the new right (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET)); // put on the right of the new root
        x64->op(JMP, fixed);

        x64->code_label(right);
        
        // Right fix
        x64->op(CALL, right_fix);
        x64->op(CMPQ, RBX, ROOTX);
        x64->op(JE, fixed);
        
        // Got rotated left
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, right_fix);  // run again on the new left (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(SELFX, RBX, RBNODE_LEFT_OFFSET)); // put on the left of the new root

        x64->code_label(fixed);

        x64->op(XCHGQ, RBX, ROOTX);
        x64->op(CALL, materialize);  // on the new root
        x64->op(XCHGQ, RBX, ROOTX);
        x64->op(RET);
        
        x64->code_label(no_dark_soul);
        x64->op(MOVQ, RBX, ROOTX);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(redden_side, "rbtree_redden_side");
        // SELFX - tree, RBX - child
        // THISX - clob
        Label black;
    
        //x64->log("Rbtree redden side.");
        x64->op(TESTQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(JE, black);
    
        // Redden the children, there must be two, because black height is positive
        x64->op(MOVQ, THISX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
        x64->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(MOVQ, THISX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
        x64->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        x64->op(RET);
    
        x64->code_label(black);
        x64->op(ORQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
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
    // In: SELFX - tree, RBX - node size
    // Out: ROOTX - node or NIL
    // Clob: RBX
    Label no_vacancy, no_reservation, init, no_last, end;

    //x64->log("Rbtree allocate.");
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_VACANT_OFFSET));
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, no_vacancy);
    
    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(SELFX, RBTREE_VACANT_OFFSET), RBX);
    x64->op(JMP, init);
    
    x64->code_label(no_vacancy);
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, ROOTX, Address(SELFX, RBTREE_RESERVATION_OFFSET));
    x64->op(JE, no_reservation);
    
    x64->op(INCQ, Address(SELFX, RBTREE_LENGTH_OFFSET));
    x64->op(IMUL2Q, ROOTX, RBX);
    x64->op(ADDQ, ROOTX, RBTREE_HEADER_SIZE);
    x64->op(JMP, init);
    
    x64->code_label(no_reservation);
    x64->op(MOVQ, ROOTX, RBNODE_NIL);
    x64->op(RET);
    
    x64->code_label(init);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, RBX, Address(SELFX, RBTREE_LAST_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBX);
    x64->op(ORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);  // red
    
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_last);
    
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_NEXT_OFFSET), ROOTX);
    x64->op(JMP, end);
    
    x64->code_label(no_last);
    x64->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), ROOTX);
    
    x64->code_label(end);
    x64->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), ROOTX);
    x64->op(RET);
}


void compile_rbtree_deallocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_deallocate");
    // In: SELFX - tree, ROOTX - node
    // Clob: RBX, THISX
    Label no_prev, prev_ok, no_next, next_ok;
    //x64->log("Rbtree deallocate.");
    
    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    x64->op(ANDQ, RBX, ~RBNODE_RED_BIT);
    x64->op(MOVQ, THISX, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));

    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_prev);
    
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_NEXT_OFFSET), THISX);
    x64->op(JMP, prev_ok);
    
    x64->code_label(no_prev);
    x64->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), THISX);
    
    x64->code_label(prev_ok);
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, no_next);
    
    // set prev while keeping the color
    x64->op(SHRQ, RBX, 1);
    x64->op(SHRQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), 1);  // color to CF
    x64->op(RCLQ, RBX, 1);
    x64->op(MOVQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBX);
    x64->op(JMP, next_ok);
    
    x64->code_label(no_next);
    x64->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), RBX);
    
    x64->code_label(next_ok);
    x64->op(MOVQ, RBX, Address(SELFX, RBTREE_VACANT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), RBX);
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
        
    elem_ts.compare(ks, vs, x64, less, greater);
    
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
    // Returns RBX - new subtree root, KEYX - index with uninitialized value
    // Clobbers THISX, THATX
    x64->code_label_local(label, "rbtree_add");
    
    Label less, greater, no;
    Label left_fix = x64->once->compile(compile_rbtree_left_fix);
    Label right_fix = x64->once->compile(compile_rbtree_right_fix);
    Label allocate = x64->once->compile(compile_rbtree_allocate);
    int key_size = elem_ts.measure_stack();
    int node_size = key_size + RBNODE_HEADER_SIZE;
    
    //x64->log("Rbtree add.");
    x64->op(CMPQ, ROOTX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(KEYX, 0));
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;

    elem_ts.compare(ks, vs, x64, less, greater);
    
    // Found the value, destroy to make place for the new one
    //x64->log("Rbtree add found.");
    elem_ts.destroy(vs, x64);
    x64->op(MOVQ, RBX, ROOTX);
    x64->op(MOVQ, KEYX, ROOTX);
    x64->op(RET);
    
    x64->code_label(less);
    //x64->log("Rbtree add left.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), RBX);
    x64->op(CALL, left_fix);
    x64->op(RET);
    
    x64->code_label(greater);
    //x64->log("Rbtree add right.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), RBX);
    x64->op(CALL, right_fix);
    x64->op(RET);
    
    x64->code_label(no);
    //x64->log("Rbtree add missing.");
    x64->op(MOVQ, RBX, node_size);
    x64->op(CALL, allocate);  // from SELFX to ROOTX (may be NIL)
    x64->op(MOVQ, RBX, ROOTX);
    x64->op(MOVQ, KEYX, ROOTX);
    x64->op(RET);
}


void compile_rbtree_remove(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects SELFX - tree, ROOTX - index, KEYX - key / dark soul
    // Returns RBX - new index
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

    elem_ts.compare(ks, vs, x64, remove_left, remove_right);
    
    // Found the value, remove it
    Label no_left, no_right, no_children, was_red;
    //x64->log("Rbtree remove found.");
    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_left);
    
    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_right);
    
    // Has two children, find the smallest greater element to swap it with.
    // This violates the ordering for a moment, but since we're about to remove one of
    // them, it will be fine soon.
    // Keep the values at the same index, this saves us a value swap, which is nice for
    // keeping the age ordering, and even nicer for finalizer callbacks.
    
    // RBX already contains the right child, then traverse to the leftmost descendant
    Label leftward_loop, leftward_cond, same_color, child_swap, swapped;
    x64->op(MOVQ, THISX, RBNODE_NIL);  // replacement's parent
    x64->op(JMP, leftward_cond);
    
    //x64->log("Rbtree remove found internal.");
    x64->code_label(leftward_loop);
    x64->op(MOVQ, THISX, RBX);
    x64->op(MOVQ, RBX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->code_label(leftward_cond);
    x64->op(CMPQ, Address(SELFX, RBX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(JNE, leftward_loop);
    
    // RBX - replacement node, THISX - replacement's parent or NIL
    // Swap the current (ROOTX) and replacement (RBX) node tree links
    
    // Swap left child
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(XCHGQ, THATX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);

    // Swap right child
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(XCHGQ, THATX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    // Swap color
    x64->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    x64->op(XORQ, THATX, Address(SELFX, RBX, RBNODE_PRED_OFFSET));
    x64->op(TESTQ, THATX, RBNODE_RED_BIT);
    x64->op(JE, same_color);  // xored == 0
    
    // The color bits differ, so complement both of them
    x64->op(XORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    x64->op(XORQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    
    x64->code_label(same_color);
    // Make the replacement parent point to the current node
    x64->op(CMPQ, THISX, RBNODE_NIL);
    x64->op(JE, child_swap);
    
    // The replacement is not the child of current, so just adjust the parent's left link
    x64->op(MOVQ, Address(SELFX, THISX, RBNODE_LEFT_OFFSET), ROOTX);
    x64->op(JMP, swapped);
    
    // The replacement is the child of current, so adjust the replacement's right link
    x64->code_label(child_swap);
    x64->op(MOVQ, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET), ROOTX);
    
    // The replacement took the place of the current node, so continue searching for
    // the same value on the right, because that's where we swapped it to.
    x64->code_label(swapped);
    x64->op(MOVQ, ROOTX, RBX);
    x64->op(JMP, remove_right);
    
    x64->code_label(no_right);
    // A single red left child can be the replacement
    //x64->log("Rbtree remove found left only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, deallocate);  // At ROOTX
    x64->op(POPQ, RBX);  // return the left child
    x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
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
    x64->op(POPQ, RBX);  // return the right child
    x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
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
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(RET);
    
    // Descend to the left
    x64->code_label(remove_left);
    //x64->log("Rbtree remove left.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Descend to the right
    x64->code_label(remove_right);
    //x64->log("Rbtree remove right.");
    x64->op(PUSHQ, ROOTX);
    x64->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, ROOTX);
    x64->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Not found
    x64->code_label(no);
    //x64->log("Rbtree remove missing.");
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(MOVQ, KEYX, 0);  // no dark soul
    x64->op(RET);
}


void compile_rbtree_next(Label label, X64 *x64) {
    // Expects SELFX - tree, RAX - it
    // Returns RAX - new it or 0, RBX new index
    // Clobbers RCX, RDX
    x64->code_label_local(label, "rbtree_next");
    Label terminate, find_leftmost, loop_leftmost, loop_check, loop, right_step, stepped, no_right;
    
    x64->op(MOVQ, RBX, Address(SELFX, RBTREE_ROOT_OFFSET));
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
    x64->op(MOVQ, RBX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(JMP, stepped);
    
    x64->code_label(right_step);
    x64->op(MOVQ, RBX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
    
    x64->code_label(stepped);
    x64->op(SHLQ, RCX, 1);
    x64->op(SHLQ, RDX, 1);
    
    x64->code_label(loop_check);
    x64->op(TESTQ, RAX, RDX);  // are there bits set above mask?
    x64->op(JNE, loop);
    
    // Found the previous node
    x64->op(MOVQ, RBX, Address(SELFX, RBX, RBNODE_RIGHT_OFFSET));
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
    x64->op(MOVQ, RDX, Address(SELFX, RBX, RBNODE_LEFT_OFFSET));
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
        :GenericValue(NO_TS, ts, NULL) {
        elem_ts = container_elem_ts(ts);
    }

    virtual Regs precompile(Regs preferred) {
        return Regs(RAX);
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
        
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
        return clob | RAX;
    }

    virtual Storage compile(X64 *x64) {
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);

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

    virtual Regs precompile(Regs preferred) {
        Regs clob = ContainerInitializerValue::precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        // This won't use the base class subcompile method, because that's inappropriate here.
        Label alloc_label = x64->once->compile(compile_rbtree_alloc, elem_ts);
        Label add_label = x64->once->compile(compile_rbtree_add, elem_ts);
        int stack_size = elem_ts.measure_stack();
    
        x64->op(MOVQ, RAX, elems.size());
        x64->op(CALL, alloc_label);
        //x64->op(MOVQ, Address(RAX, RBTREE_LENGTH_OFFSET), elems.size());
        x64->op(PUSHQ, RAX);
        
        for (auto &elem : elems) {
            elem->compile_and_store(x64, Storage(STACK));

            x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
            x64->op(MOVQ, SELFX, Address(RSP, stack_size));  // Rbtree without incref
            x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

            x64->op(CALL, add_label);

            x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
            x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
        
            elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);
        }
        
        return Storage(STACK);
    }
};


class RbtreeLengthValue: public GenericValue {
public:
    Register reg;
    
    RbtreeLengthValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, INTEGER_TS, l) {
        reg = NOREG;
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob = clob | RAX;
        
        reg = clob.get_any();
            
        return clob;
    }

    virtual Storage compile(X64 *x64) {
        ls = left->compile(x64);

        switch (ls.where) {
        case REGISTER:
            x64->runtime->decref(ls.reg);
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
        :GenericValue(match[1], BOOLEAN_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label has = x64->once->compile(compile_rbtree_has, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        
        x64->op(CALL, has);
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(SETNE, AL);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, AL);
    }
};


class RbtreeAddValue: public ContainerGrowableValue {
public:
    TypeSpec elem_ts;
    
    RbtreeAddValue(Value *pivot, TypeMatch &match)
        :ContainerGrowableValue(match[1], VOID_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label add = x64->once->compile(compile_rbtree_add, elem_ts);
        Label ok;
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, add);

        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root
        
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(JNE, ok);
        
        // Non-autogrowing Rbtree-s only raise a CONTAINER_FULL exception, if the operation
        // actually tried to increase the size, not when an existing node is updated.
        if (raising_dummy) {
            raise("CONTAINER_FULL", x64);
        }
        else
            x64->runtime->die("Rbtree full even if autogrowing!");
        
        x64->code_label(ok);
        elem_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class RbtreeRemoveValue: public ContainerShrinkableValue {
public:
    TypeSpec elem_ts;
    
    RbtreeRemoveValue(Value *pivot, TypeMatch &match)
        :ContainerShrinkableValue(match[1], VOID_TS, pivot) {
        elem_ts = match[1];
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = left->precompile(preferred) | right->precompile(preferred);
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        int key_size = elem_ts.measure_stack();
        Label remove = x64->once->compile(compile_rbtree_remove, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, KEYX, RSP);  // save key address for stack usage
        x64->op(MOVQ, SELFX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, remove);

        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        
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
        return subcompile(RBTREE_RESERVATION_OFFSET, RBTREE_LENGTH_OFFSET, compile_rbtree_grow, x64);
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


class RbtreeNextElemByAgeValue: public GenericValue, public Raiser {
public:
    Regs clob;
    bool is_down;
    TypeSpec elem_ts;

    RbtreeNextElemByAgeValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, match[1], l) {
        is_down = false;  // TODO: get as argument for backward iteration!
        elem_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        if (!check_raise(iterator_done_exception_type, scope))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        if (!clob.has_any())
            clob = clob | RAX;
        
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
            
            raise("ITERATOR_DONE", x64);
            
            x64->code_label(ok);
            x64->op(ADDQ, reg, RBX);
            
            if (is_down) {
                x64->op(MOVQ, RBX, Address(reg, RBNODE_PRED_OFFSET));
                x64->op(ANDQ, RBX, ~RBNODE_RED_BIT);  // remove color bit
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


class RbtreeNextElemByOrderValue: public GenericValue, public Raiser {
public:
    Regs clob;
    TypeSpec elem_ts;

    RbtreeNextElemByOrderValue(Value *l, TypeMatch &match)
        :GenericValue(NO_TS, match[1], l) {
        elem_ts = match[1];
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        if (!check_arguments(args, kwargs, {}))
            return false;

        if (!check_raise(iterator_done_exception_type, scope))
            return false;
        
        return true;
    }

    virtual Regs precompile(Regs preferred) {
        clob = left->precompile(preferred);
        
        return clob | RAX | RCX | RDX | SELFX;
    }

    virtual Storage compile(X64 *x64) {
        Label next_label = x64->once->compile(compile_rbtree_next);
        Label ok;

        left->compile_and_store(x64, Storage(ALISTACK));  // iterator

        x64->op(MOVQ, RCX, Address(RSP, 0));
        x64->op(MOVQ, RAX, Address(RCX, REFERENCE_SIZE));  // it
        x64->op(MOVQ, SELFX, Address(RCX, 0)); // tree reference without incref

        x64->op(CALL, next_label);
        
        x64->op(POPQ, RCX);
        x64->op(CMPQ, RAX, 0);
        x64->op(JNE, ok);

        raise("ITERATOR_DONE", x64);
        
        x64->code_label(ok);
        x64->op(MOVQ, Address(RCX, REFERENCE_SIZE), RAX);  // save it
        x64->op(LEA, RAX, Address(SELFX, RBX, RBNODE_VALUE_OFFSET));

        return Storage(MEMORY, Address(RAX, 0));
    }
};


// Map and friends

class MapAddValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts, value_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapAddValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l->lookup_inner("wrapped")->lookup_inner("autogrow"));
        key_ts = match[1];
        value_ts = match[2];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        // To help subclasses tweaking these
        key_arg_ts = key_ts;
        value_arg_ts = value_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        // Autogrow stuff
        Args fake_args;
        Kwargs fake_kwargs;
        if (!pivot->check(fake_args, fake_kwargs, scope))
            return false;

        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key },
            { "value", &value_arg_ts, scope, &value }  // disabled if NO_TS
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        
        if (value)
            clob = clob | value->precompile(preferred);
        
        // We build on this in WeakValueMap::precreate
        return clob | SELFX | ROOTX | KEYX | THISX | THATX | COMPARE_CLOB;
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        // To be overridden
    }

    virtual void prevalue(Address alias_addr, X64 *x64) {
        // To be overridden
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(ALISTACK));  // Push the address of the rbtree ref
        key->compile_and_store(x64, Storage(STACK));
        
        if (value)
            value->compile_and_store(x64, Storage(STACK));

        int key_size = key_ts.measure_stack();  // NOTE: as it's in an Item, it is rounded up
        int key_stack_size = key_arg_ts.measure_stack();
        int value_stack_size = value_arg_ts != NO_TS ? value_arg_ts.measure_stack() : 0;
        
        Label add_label = x64->once->compile(compile_rbtree_add, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size + value_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, 0));

        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, value_stack_size));

        x64->op(CALL, add_label);
        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(SELFX, RBX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken root

        // NOTE: we abuse the fact that Item contains index first, and value second,
        // and since they're parametric types, their sizes will be rounded up.
        if (value) {
            prevalue(Address(RSP, key_stack_size + value_stack_size), x64);
            value_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size)), x64);
        }
        
        prekey(Address(RSP, key_stack_size), x64);
        key_ts.create(Storage(STACK), Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET)), x64);

        pivot->ts.store(Storage(ALISTACK), Storage(), x64);
        
        return Storage();
    }
};


class MapRemoveValue: public Value {
public:
    TypeSpec key_ts, item_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key;

    MapRemoveValue(Value *l, TypeMatch &match)
        :Value(VOID_TS) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | SELFX | KEYX | ROOTX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage compile(X64 *x64) {
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));

        int key_stack_size = key_arg_ts.measure_stack();
        
        Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));  // NOTE: only the index part is present of the Item

        x64->op(CALL, remove_label);
        x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);

        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};


class MapIndexValue: public Value {
public:
    TypeSpec key_ts, value_ts, item_ts, key_arg_ts;
    std::unique_ptr<Value> pivot, key, value;

    MapIndexValue(Value *l, TypeMatch &match)
        :Value(match[2]) {
        pivot.reset(l);
        key_ts = match[1];
        item_ts = match[0].unprefix(weakref_type).reprefix(map_type, item_type);
        
        key_arg_ts = key_ts;
    }

    virtual bool check(Args &args, Kwargs &kwargs, Scope *scope) {
        return check_arguments(args, kwargs, {
            { "key", &key_arg_ts, scope, &key }
        });
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob = pivot->precompile(preferred) | key->precompile(preferred);
        return clob | SELFX | KEYX | ROOTX | THISX | THATX | COMPARE_CLOB;
    }

    virtual Storage postresult(X64 *x64) {
        int key_size = key_ts.measure_stack();  // in an Item it's rounded up

        Label ok;
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        x64->op(JNE, ok);

        x64->runtime->die("Map missing!");  // TODO

        x64->code_label(ok);
        
        return Storage(MEMORY, Address(SELFX, KEYX, RBNODE_VALUE_OFFSET + key_size));
    }

    virtual Storage compile(X64 *x64) {
        int key_stack_size = key_arg_ts.measure_stack();
        
        pivot->compile_and_store(x64, Storage(STACK));
        key->compile_and_store(x64, Storage(STACK));
        
        Label has_label = x64->once->compile(compile_rbtree_has, item_ts);

        x64->op(MOVQ, RBX, Address(RSP, key_stack_size));
        x64->op(MOVQ, SELFX, Address(RBX, CLASS_MEMBERS_OFFSET));
        x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
        x64->op(LEA, KEYX, Address(RSP, 0));

        x64->op(CALL, has_label);  // KEYX is the index of the found item, or NIL
        
        key_arg_ts.store(Storage(STACK), Storage(), x64);
        pivot->ts.store(Storage(STACK), Storage(), x64);
        
        return postresult(x64);
    }
};


// Weak map helpers

static void compile_process_fcb(Label label, TypeSpec item_ts, X64 *x64) {
    x64->code_label_local(label, "xy_weakmap_callback");
    // RAX - fcb, RCX - payload1, RDX - payload2
    // We may clobber all registers
    // FIXME: make sure these registers can be safely moved to the RB-pseudoregisters!

    std::stringstream ss;
    ss << item_ts << " callback";
    x64->runtime->log(ss.str().c_str());  // "WeakMap callback.");
    
    Label remove_label = x64->once->compile(compile_rbtree_remove, item_ts);

    x64->op(MOVQ, SELFX, Address(RCX, 0));  // load current rbtree ref
    x64->op(MOVQ, ROOTX, Address(SELFX, RBTREE_ROOT_OFFSET));
    x64->op(LEA, KEYX, Address(SELFX, RDX, RBNODE_VALUE_OFFSET));

    x64->op(CALL, remove_label);
    
    x64->op(MOVQ, Address(SELFX, RBTREE_ROOT_OFFSET), RBX);
    
    x64->op(RET);
}


static void alloc_fcb(TypeSpec item_ts, Address alias_addr, X64 *x64) {
    // Allocate and push a FCB pointer. We may clobber RAX, RBX, RCX, RDX.
    Label callback_label = x64->once->compile(compile_process_fcb, item_ts);
    
    x64->op(MOVQ, RAX, Address(RSP, 0));  // referred heap object
    x64->op(LEARIP, RBX, callback_label);  // callback
    x64->op(MOVQ, RCX, alias_addr);  // payload1, the rbtree ref address, RSP based
    x64->op(MOVQ, RDX, KEYX);  // payload2, the rbnode index
    
    x64->op(CALL, x64->runtime->alloc_fcb_label);
    
    x64->op(PUSHQ, RAX);  // last minute weakanchor
}


// WeakValueMap

TypeMatch &wvmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAME_SAMEID2_WEAKANCHOR_MAP_WEAKREF_TS, match);
    tm[1] = match[1];
    tm[2] = match[2].prefix(weakanchor_type);
    tm[3] = NO_TS;
    return tm;
}


class WeakValueMapAddValue: public MapAddValue {
public:
    WeakValueMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wvmatch(match)) {
        value_arg_ts = value_ts.reprefix(weakanchor_type, weakref_type);
    }

    virtual void prevalue(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
    }
};


class WeakValueMapRemoveValue: public MapRemoveValue {
public:
    WeakValueMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wvmatch(match)) {
    }
};


class WeakValueMapIndexValue: public MapIndexValue {
public:
    WeakValueMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wvmatch(match)) {
        ts = ts.reprefix(weakanchor_type, weakref_type);
    }
};


// WeakIndexMap

TypeMatch &wimatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_WEAKANCHOR_SAME2_MAP_WEAKREF_TS, match);
    tm[1] = match[1].prefix(weakanchor_type);
    tm[2] = match[2];
    tm[3] = NO_TS;
    return tm;
}


class WeakIndexMapAddValue: public MapAddValue {
public:
    WeakIndexMapAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
    }
};


class WeakIndexMapRemoveValue: public MapRemoveValue {
public:
    WeakIndexMapRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


class WeakIndexMapIndexValue: public MapIndexValue {
public:
    WeakIndexMapIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wimatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


// WeakSet

TypeMatch &wsmatch(TypeMatch &match) {
    static TypeMatch tm;
    tm[0] = typesubst(SAMEID_WEAKANCHOR_VOID_MAP_WEAKREF_TS, match);
    tm[1] = match[1].prefix(weakanchor_type);
    tm[2] = VOID_TS;
    tm[3] = NO_TS;
    return tm;
}


class WeakSetAddValue: public MapAddValue {
public:
    WeakSetAddValue(Value *l, TypeMatch &match)
        :MapAddValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
        value_arg_ts = NO_TS;
    }

    virtual void prekey(Address alias_addr, X64 *x64) {
        alloc_fcb(item_ts, alias_addr, x64);
    }
};


class WeakSetRemoveValue: public MapRemoveValue {
public:
    WeakSetRemoveValue(Value *l, TypeMatch &match)
        :MapRemoveValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
    }
};


class WeakSetIndexValue: public MapIndexValue {
public:
    WeakSetIndexValue(Value *l, TypeMatch &match)
        :MapIndexValue(l, wsmatch(match)) {
        key_arg_ts = key_ts.reprefix(weakanchor_type, weakref_type);
        ts = BOOLEAN_TS;
    }

    virtual Storage postresult(X64 *x64) {
        x64->op(CMPQ, KEYX, RBNODE_NIL);
        
        return Storage(FLAGS, SETNE);
    }
};

#undef SELFX
#undef KEYX
#undef ROOTX
#undef THISX
#undef THATX
