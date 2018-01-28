
// NOTE: node indexes stored in root, vacant, *.{left,right,prev,next} are tree-relative
// offsets, so if RAX points to the tree, then RAX + RCX points to the node. The NIL node
// value may be 0, which is an invalid offset, since the tree itself has a nonempty header.

// TODO: rewrite with RSI pointing to the tree, RDI the dark soul, and every other
// register a node index.

void compile_left_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_left_fix");
    // RAX - tree, RCX - node
    // RBX - result
    // RDX - clob
    Label ok, outer_nonred, outer_red;

    x64->log("Rbtree left fix.");
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);  // RBX - red left

    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, outer_nonred);
    
    x64->code_label(outer_red);  // Red-promoting right rotate
    x64->log("Rbtree red-promoting right rotate.");
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), RDX);
    
    x64->op(MOVQ, Address(RAX, RBX, RBNODE_RIGHT_OFFSET), RCX);
    
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(ANDQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken outer
    x64->op(RET);
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);
    
    x64->log("Rbtree red-swapping left rotate.");
    x64->op(PUSHQ, Address(RAX, RDX, RBNODE_LEFT_OFFSET));  // TODO
    x64->op(POPQ, Address(RAX, RBX, RBNODE_RIGHT_OFFSET));
    
    x64->op(MOVQ, Address(RAX, RDX, RBNODE_LEFT_OFFSET), RBX);
    x64->op(MOVQ, RBX, RDX);
    x64->op(JMP, outer_red);
    
    x64->code_label(ok);
    x64->log("Rbtree left fix ok.");
    x64->op(MOVQ, RBX, RCX);
    x64->op(RET);
}


void compile_right_fix(Label label, X64 *x64) {
    x64->code_label_local(label, "rbtree_right_fix");
    // RAX - tree, RCX - node
    // RBX - result
    // RDX - clob
    Label ok, outer_nonred, outer_red;

    x64->log("Rbtree right fix.");
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, ok);

    x64->op(TESTQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);  // RBX - red right

    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, outer_nonred);
    
    x64->op(TESTQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, outer_nonred);
    
    x64->code_label(outer_red);  // Red-promoting left rotate
    x64->log("Rbtree red-promoting left rotate.");
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), RDX);
    
    x64->op(MOVQ, Address(RAX, RBX, RBNODE_LEFT_OFFSET), RCX);
    
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_RIGHT_OFFSET));
    x64->op(ANDQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken outer
    x64->op(RET);
    
    x64->code_label(outer_nonred);
    x64->op(MOVQ, RDX, Address(RAX, RBX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, ok);
    
    x64->op(TESTQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JE, ok);
    
    x64->log("Rbtree red-swapping right rotate.");
    x64->op(PUSHQ, Address(RAX, RDX, RBNODE_RIGHT_OFFSET));  // TODO
    x64->op(POPQ, Address(RAX, RBX, RBNODE_LEFT_OFFSET));
    
    x64->op(MOVQ, Address(RAX, RDX, RBNODE_RIGHT_OFFSET), RBX);
    x64->op(MOVQ, RBX, RDX);
    x64->op(JMP, outer_red);
    
    x64->code_label(ok);
    x64->log("Rbtree right fix ok.");
    x64->op(MOVQ, RBX, RCX);
    x64->op(RET);
}


void compile_other_fix(Label label, X64 *x64) {
    Label redden, materialize;
    Label left_fix = x64->once->compile(compile_left_fix);
    Label right_fix = x64->once->compile(compile_right_fix);

    {
        x64->code_label_local(label, "rbtree_other_fix");
        // RAX - tree, RBX - child, RCX - node, RSI - immaterial
        Label no_black, right, fixed;
        
        x64->op(TESTQ, RSI, 1);
        x64->op(JE, no_black);
        
        x64->op(PUSHQ, RCX);
        x64->op(MOVQ, RCX, RBX);  // TODO: simplify!
        x64->op(CALL, redden);
        x64->op(POPQ, RCX);
        
        x64->op(CALL, materialize);
        
        x64->op(CMPQ, RBX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
        x64->op(JNE, right);
        
        // Left fix
        x64->op(CALL, left_fix);
        x64->op(CMPQ, RBX, RCX);
        x64->op(JE, fixed);
        
        // Got rotated
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, left_fix);  // run again on the new left (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(RAX, RBX, RBNODE_LEFT_OFFSET)); // put on the left of the new root
        x64->op(JMP, fixed);

        x64->code_label(right);
        
        // Right fix
        x64->op(CALL, right_fix);
        x64->op(CMPQ, RBX, RCX);
        x64->op(JE, fixed);
        
        // Got rotated
        x64->op(PUSHQ, RBX);  // the new root
        x64->op(CALL, right_fix);  // run again on the new right (old root)
        x64->op(XCHGQ, RBX, Address(RSP, 0));
        x64->op(POPQ, Address(RAX, RBX, RBNODE_RIGHT_OFFSET)); // put on the right of the new root

        x64->code_label(fixed);

        x64->op(XCHGQ, RBX, RCX);
        x64->op(CALL, materialize);  // on the new root
        x64->op(XCHGQ, RBX, RCX);
        x64->op(RET);
        
        x64->code_label(no_black);
        x64->op(MOVQ, RBX, RCX);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(redden, "_redden");
        // RAX - tree, RCX - node
        // RBX - clob
        Label black;
    
        x64->log("Rbtree redden.");
        x64->op(TESTQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(JE, black);
    
        // Redden the children, there must be two, because black height is positive
        x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
        x64->op(ORQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
        x64->op(ORQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(RET);
    
        x64->code_label(black);
        x64->op(ORQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(RET);
    }
    
    {
        x64->code_label_local(materialize, "_materialize");
        // RAX - tree, RCX - node, RSI - immaterial_black
        Label end;
    
        x64->log("Rbtree materialize.");
        x64->op(TESTQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
        x64->op(JE, end);
    
        x64->op(TESTQ, RSI, 1);
        x64->op(JE, end);
    
        x64->op(ANDQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), -2);
        x64->op(MOVQ, RSI, 0);
    
        x64->code_label(end);
        x64->op(RET);
    }
}


void compile_allocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_allocate");
    // In: RAX - tree, RBX - node size
    // Out: RCX - node
    // Clob: RBX
    Label no_vacancy, no_reservation, init, no_last, end;

    x64->log("Rbtree allocate.");
    x64->op(MOVQ, RCX, Address(RAX, RBTREE_VACANT_OFFSET));
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, no_vacancy);
    
    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RBX);
    x64->op(JMP, init);
    
    x64->code_label(no_vacancy);
    x64->op(MOVQ, RCX, Address(RAX, RBTREE_LENGTH_OFFSET));
    x64->op(CMPQ, RCX, Address(RAX, RBTREE_RESERVATION_OFFSET));
    x64->op(JE, no_reservation);
    
    x64->op(INCQ, Address(RAX, RBTREE_LENGTH_OFFSET));
    x64->op(IMUL2Q, RCX, RBX);
    x64->op(ADDQ, RCX, RBTREE_HEADER_SIZE);
    x64->op(JMP, init);
    
    x64->code_label(no_reservation);
    x64->die("Rbtree full!");
    
    x64->code_label(init);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), RBNODE_NIL);
    x64->op(MOVQ, RBX, Address(RAX, RBTREE_LAST_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), RBX);
    x64->op(ORQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);  // red
    
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_last);
    
    x64->op(MOVQ, Address(RAX, RBX, RBNODE_NEXT_OFFSET), RCX);
    x64->op(JMP, end);
    
    x64->code_label(no_last);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RCX);
    
    x64->code_label(end);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RCX);
    x64->op(RET);
}


void compile_deallocate(Label label, X64 *x64) {
    x64->code_label_local(label, "_deallocate");
    // In: RAX - tree
    // Out: RCX - node
    // Clob: RBX, RDX
    Label no_prev, prev_ok, no_next, next_ok;
    x64->log("Rbtree deallocate.");
    
    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET));
    x64->op(ANDQ, RBX, -2);
    x64->op(MOVQ, RDX, Address(RAX, RCX, RBNODE_NEXT_OFFSET));

    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_prev);
    
    x64->op(MOVQ, Address(RAX, RBX, RBNODE_NEXT_OFFSET), RDX);
    x64->op(JMP, prev_ok);
    
    x64->code_label(no_prev);
    x64->op(MOVQ, Address(RAX, RBTREE_FIRST_OFFSET), RDX);
    
    x64->code_label(prev_ok);
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, no_next);
    
    // set prev while keeping the color
    x64->op(SHRQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);  // color to CF
    x64->op(MOVQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), RBX);
    x64->op(RCRQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);  // color to MSB
    x64->op(ROLQ, Address(RAX, RDX, RBNODE_PREV_IS_RED_OFFSET), 1);  // color to LSB
    x64->op(JMP, next_ok);
    
    x64->code_label(no_next);
    x64->op(MOVQ, Address(RAX, RBTREE_LAST_OFFSET), RBX);
    
    x64->code_label(next_ok);
    x64->op(MOVQ, RBX, Address(RAX, RBTREE_VACANT_OFFSET));
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_NEXT_OFFSET), RBX);
    x64->op(MOVQ, Address(RAX, RBTREE_VACANT_OFFSET), RCX);
    
    x64->op(DECQ, Address(RAX, RBTREE_LENGTH_OFFSET));
    x64->op(RET);
}


void compile_has(Label label, TypeSpec elem_ts, X64 *x64) {
    // RAX - tree
    // RBX - clob
    // RCX - index, return bool
    // RDX - key
    x64->code_label_local(label, "rbtree_has");

    Label loop, no, less, greater;

    x64->code_label(loop);
    //x64->log("Has loop.");
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, no);

    Storage ks(MEMORY, Address(RDX, 0));
    Storage vs(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, less, greater);
    
    x64->op(MOVQ, RCX, 1);
    x64->op(RET);
    
    x64->code_label(less);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(JMP, loop);
    
    x64->code_label(greater);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(JMP, loop);
    
    x64->code_label(no);
    x64->op(MOVQ, RCX, 0);
    x64->op(RET);
}


void compile_add(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects RAX - tree, RCX - index, RDX - key (clobbered)
    // Returns RBX - new index
    x64->code_label_local(label, "rbtree_add");
    
    Label less, greater, no;
    Label left_fix = x64->once->compile(compile_left_fix);
    Label right_fix = x64->once->compile(compile_right_fix);
    Label allocate = x64->once->compile(compile_allocate);
    int key_size = ::stack_size(elem_ts.measure(MEMORY));
    int node_size = key_size + RBNODE_HEADER_SIZE;
    
    x64->log("Rbtree add.");
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(RDX, 0));
    Storage vs(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, less, greater);
    
    // Found the value, nothing to do
    x64->log("Rbtree add found.");
    x64->op(MOVQ, RBX, RCX);
    x64->op(RET);
    
    x64->code_label(less);
    x64->log("Rbtree add left.");
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RCX);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), RBX);
    x64->op(CALL, left_fix);
    x64->op(RET);
    
    x64->code_label(greater);
    x64->log("Rbtree add right.");
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RCX);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), RBX);
    x64->op(CALL, right_fix);
    x64->op(RET);
    
    x64->code_label(no);
    x64->log("Rbtree add missing.");
    x64->op(MOVQ, RBX, node_size);
    x64->op(CALL, allocate);  // from RAX to RCX
    elem_ts.create(ks, vs, x64);
    x64->op(MOVQ, RBX, RCX);
    x64->op(RET);
}


void compile_remove(Label label, TypeSpec elem_ts, X64 *x64) {
    // Expects RAX - tree, RCX - index, RDX - key (may be modified upon return)
    // Returns RBX - new index, RSI - immaterial_black
    x64->code_label_local(label, "rbtree_remove");
    
    Label no, remove_left, remove_right;
    Label deallocate = x64->once->compile(compile_deallocate);
    Label other_fix = x64->once->compile(compile_other_fix);
    
    x64->log("Rbtree remove.");
    x64->op(CMPQ, RCX, RBNODE_NIL);
    x64->op(JE, no);
    
    Storage ks(MEMORY, Address(RDX, 0));  // can't use STACK, that would be popped!
    Storage vs(MEMORY, Address(RAX, RCX, RBNODE_VALUE_OFFSET));
    elem_ts.compare(ks, vs, x64, remove_left, remove_right);
    
    // Found the value, remove it (RDX is no longer needed)
    Label no_left, no_right, no_children, internal_loop, was_red;
    x64->log("Rbtree remove found.");
    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_left);
    
    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JE, no_right);
    
    // Find the greatest smaller element as replacement
    x64->log("Rbtree remove found internal.");
    x64->code_label(internal_loop);
    x64->op(MOVQ, RDX, RBX);
    x64->op(MOVQ, RBX, Address(RAX, RDX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RBX, RBNODE_NIL);
    x64->op(JNE, internal_loop);
    
    Storage rs(MEMORY, Address(RAX, RDX, RBNODE_VALUE_OFFSET));
    elem_ts.store(rs, vs, x64);
    
    x64->op(LEA, RDX, Address(RAX, RDX, RBNODE_VALUE_OFFSET));
    x64->op(JMP, remove_left);  // Yes, this is a goto :-) XXXXXXXXXXXXXXXXXXXXX
    
    x64->code_label(no_right);
    // A single red left child can be the replacement
    x64->log("Rbtree remove found left only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, deallocate);  // At RCX
    x64->op(POPQ, RBX);  // return the left child
    x64->op(ANDQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken
    x64->op(RET);
        
    x64->code_label(no_left);
    x64->op(MOVQ, RDX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CMPQ, RDX, RBNODE_NIL);
    x64->op(JE, no_children);
    
    // A single red right child can be the replacement
    x64->log("Rbtree remove found right only.");
    elem_ts.destroy(vs, x64);
    x64->op(PUSHQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, deallocate);  // At RCX
    x64->op(POPQ, RBX);  // return the right child
    x64->op(ANDQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken
    x64->op(RET);
    
    // No children, just remove
    x64->code_label(no_children);
    x64->log("Rbtree remove found leaf.");
    elem_ts.destroy(vs, x64);
    x64->op(CALL, deallocate);
    x64->op(TESTQ, Address(RAX, RCX, RBNODE_PREV_IS_RED_OFFSET), 1);
    x64->op(JNE, was_red);
    
    x64->log("Rbtree remove found leaf immaterialize.");
    x64->op(MOVQ, RSI, 1);
    
    x64->code_label(was_red);
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(RET);
    
    // Descend to the left
    x64->code_label(remove_left);
    x64->log("Rbtree remove left.");
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RCX);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_LEFT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Descend to the right
    x64->code_label(remove_right);
    x64->log("Rbtree remove right.");
    x64->op(PUSHQ, RCX);
    x64->op(MOVQ, RCX, Address(RAX, RCX, RBNODE_RIGHT_OFFSET));
    x64->op(CALL, label);
    x64->op(POPQ, RCX);
    x64->op(MOVQ, Address(RAX, RCX, RBNODE_RIGHT_OFFSET), RBX);

    x64->op(MOVQ, RBX, Address(RAX, RCX, RBNODE_LEFT_OFFSET));
    x64->op(CALL, other_fix);
    x64->op(RET);
    
    // Not found
    x64->code_label(no);
    x64->log("Rbtree remove missing.");
    x64->op(MOVQ, RBX, RBNODE_NIL);
    x64->op(RET);
}


class RbtreeEmptyValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeEmptyValue(TypeSpec ts)
        :GenericValue(VOID_TS, ts, NULL) {
        elem_ts = ts.unprefix(reference_type).unprefix(rbtree_type).varvalue();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec ats = ts.unprefix(reference_type);
        Label rbtree_finalizer_label = ats.get_finalizer_label(x64);
        int elem_size = ::stack_size(elem_ts.measure(MEMORY));  // round to 8 bytes!
    
        x64->op(MOVQ, RAX, 0);
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, rbtree_finalizer_label);
    
        x64->alloc_rbtree_RAX_RBX_RCX();  // reservation, elem size, finalizer
        
        return Storage(REGISTER, RAX);
    }
};


class RbtreeReservedValue: public GenericValue {
public:
    TypeSpec elem_ts;
    
    RbtreeReservedValue(TypeSpec ts)
        :GenericValue(INTEGER_TS, ts, NULL) {
        elem_ts = ts.unprefix(reference_type).unprefix(rbtree_type).varvalue();
    }

    virtual Regs precompile(Regs preferred) {
        Regs clob;
        return clob.add(RAX).add(RBX).add(RCX);
    }

    virtual Storage compile(X64 *x64) {
        TypeSpec ats = ts.unprefix(reference_type);
        Label rbtree_finalizer_label = ats.get_finalizer_label(x64);
        int elem_size = ::stack_size(elem_ts.measure(MEMORY));  // round to 8 bytes!
    
        right->compile_and_store(x64, Storage(STACK));
    
        x64->op(POPQ, RAX);
        x64->op(MOVQ, RBX, elem_size);
        x64->op(LEARIP, RCX, rbtree_finalizer_label);
    
        x64->alloc_rbtree_RAX_RBX_RCX();  // reservation, elem size, finalizer
        
        return Storage(REGISTER, RAX);
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
        return clob.add(RAX).add(RBX).add(RCX).add(RDX);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        Label has = x64->once->compile(compile_has, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));
    
        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RCX, Address(RAX, RBTREE_ROOT_OFFSET));
        
        x64->op(CALL, has);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage(REGISTER, RCX);
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
        return clob.add(RAX).add(RBX).add(RCX).add(RDX);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        Label add = x64->once->compile(compile_add, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RCX, Address(RAX, RBTREE_ROOT_OFFSET));

        x64->op(CALL, add);

        x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBX);
        x64->op(ANDQ, Address(RAX, RBX, RBNODE_PREV_IS_RED_OFFSET), -2);  // blacken root
        
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
        return clob.add(RAX).add(RBX).add(RCX).add(RDX).add(RSI);
    }

    virtual Storage compile(X64 *x64) {
        int key_size = ::stack_size(elem_ts.measure(MEMORY));
        Label remove = x64->once->compile(compile_remove, elem_ts);
        
        compile_and_store_both(x64, Storage(STACK), Storage(STACK));

        x64->op(MOVQ, RDX, RSP);  // save key address for stack usage
        x64->op(MOVQ, RAX, Address(RSP, key_size));  // Rbtree without incref
        x64->op(MOVQ, RCX, Address(RAX, RBTREE_ROOT_OFFSET));
        x64->op(MOVQ, RSI, 0);

        x64->op(CALL, remove);

        x64->op(MOVQ, Address(RAX, RBTREE_ROOT_OFFSET), RBX);
        
        elem_ts.store(Storage(STACK), Storage(), x64);
        left->ts.store(Storage(STACK), Storage(), x64);
        
        return Storage();
    }
};

