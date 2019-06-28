#include "../plum.h"

#include "rbtree_registers.h"


void compile_rbtree_left_fix(Label label, Cx *cx) {
    cx->code_label_local(label, "rbtree_left_fix");
    cx->prologue();
    // SELFX - tree, ROOTX - node
    // R10 - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //cx->log("Rbtree left fix.");
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, ok);

    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, ok);

    cx->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, ok);  // R10 - red left child

    cx->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(CMPQ, THISX, RBNODE_NIL);
    cx->op(JE, outer_nonred);
    
    cx->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, outer_nonred);  // THISX - outer red
    
    cx->code_label(outer_red);  // Red-promoting right rotate
    //cx->log("Rbtree red-promoting right rotate.");
    cx->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);
    
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), ROOTX);
    
    cx->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    cx->epilogue();  // the red left child is the new root
    
    cx->code_label(outer_nonred);
    cx->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, THATX, RBNODE_NIL);
    cx->op(JE, ok);
    
    cx->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, ok);
    
    //cx->log("Rbtree red-swapping left rotate.");
    cx->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_LEFT_OFFSET));  // TODO
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), THISX);
    
    cx->op(MOVQ, Address(SELFX, THATX, RBNODE_LEFT_OFFSET), R10);
    
    cx->op(MOVQ, THISX, R10);
    cx->op(MOVQ, R10, THATX);
    cx->op(JMP, outer_red);  // the red inner grandchild is the new red left child
    
    cx->code_label(ok);
    //cx->log("Rbtree left fix ok.");
    cx->op(MOVQ, R10, ROOTX);
    cx->epilogue();
}


void compile_rbtree_right_fix(Label label, Cx *cx) {
    cx->code_label_local(label, "rbtree_right_fix");
    cx->prologue();
    // SELFX - tree, ROOTX - node
    // R10 - result
    // THISX, THATX - clob
    Label ok, outer_nonred, outer_red;

    //cx->log("Rbtree right fix.");
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, ok);

    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, ok);

    cx->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, ok);  // R10 - red right child

    cx->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, THISX, RBNODE_NIL);
    cx->op(JE, outer_nonred);
    
    cx->op(TESTQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, outer_nonred);  // THISX - outer red
    
    cx->code_label(outer_red);  // Red-promoting left rotate
    //cx->log("Rbtree red-promoting left rotate.");
    cx->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), ROOTX);
    
    cx->op(ANDQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken outer
    cx->epilogue();  // the red right child is the new root
    
    cx->code_label(outer_nonred);
    cx->op(MOVQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(CMPQ, THATX, RBNODE_NIL);
    cx->op(JE, ok);
    
    cx->op(TESTQ, Address(SELFX, THATX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JE, ok);
    
    //cx->log("Rbtree red-swapping right rotate.");
    cx->op(MOVQ, THISX, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET));  // TODO
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), THISX);
    
    cx->op(MOVQ, Address(SELFX, THATX, RBNODE_RIGHT_OFFSET), R10);
    
    cx->op(MOVQ, THISX, R10);
    cx->op(MOVQ, R10, THATX);
    cx->op(JMP, outer_red);  // the red inner grandchild is the new red right child
    
    cx->code_label(ok);
    //cx->log("Rbtree right fix ok.");
    cx->op(MOVQ, R10, ROOTX);
    cx->epilogue();
}


void compile_rbtree_other_fix(Label label, Cx *cx) {
    Label redden_side, materialize;
    Label left_fix = cx->once->compile(compile_rbtree_left_fix);
    Label right_fix = cx->once->compile(compile_rbtree_right_fix);

    {
        cx->code_label_local(label, "rbtree_other_fix");
        cx->prologue();
        // SELFX - tree, ROOTX - node, R10 - child, R11 - dark soul
        // R10 - result, R11 - dark soul
        // THISX, THATX - clob
        Label no_dark_soul, right, fixed;
        
        cx->op(CMPQ, R11, 0);
        cx->op(JE, no_dark_soul);
        
        cx->op(CALL, redden_side);
        
        cx->op(CALL, materialize);
        
        cx->op(CMPQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
        cx->op(JNE, right);
        
        // Left fix
        cx->op(CALL, left_fix);
        cx->op(CMPQ, R10, ROOTX);
        cx->op(JE, fixed);
        
        // Got rotated right
        cx->op(PUSHQ, R10);  // the new root
        cx->op(CALL, left_fix);  // run again on the new right (old root)
        cx->op(XCHGQ, R10, Address(RSP, 0));
        cx->op(POPQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET)); // put on the right of the new root
        cx->op(JMP, fixed);

        cx->code_label(right);
        
        // Right fix
        cx->op(CALL, right_fix);
        cx->op(CMPQ, R10, ROOTX);
        cx->op(JE, fixed);
        
        // Got rotated left
        cx->op(PUSHQ, R10);  // the new root
        cx->op(CALL, right_fix);  // run again on the new left (old root)
        cx->op(XCHGQ, R10, Address(RSP, 0));
        cx->op(POPQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET)); // put on the left of the new root

        cx->code_label(fixed);

        cx->op(XCHGQ, R10, ROOTX);
        cx->op(CALL, materialize);  // on the new root
        cx->op(XCHGQ, R10, ROOTX);
        cx->epilogue();
        
        cx->code_label(no_dark_soul);
        cx->op(MOVQ, R10, ROOTX);
        cx->epilogue();
    }
    
    {
        cx->code_label_local(redden_side, "rbtree_redden_side");
        cx->prologue();
        // SELFX - tree, R10 - child
        // THISX - clob
        Label black;
    
        //cx->log("Rbtree redden side.");
        cx->op(TESTQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        cx->op(JE, black);
    
        // Redden the children, there must be two, because black height is positive
        cx->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
        cx->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        cx->op(MOVQ, THISX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
        cx->op(ORQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        cx->epilogue();
    
        cx->code_label(black);
        cx->op(ORQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        cx->epilogue();
    }
    
    {
        cx->code_label_local(materialize, "_materialize");
        cx->prologue();
        // SELFX - tree, ROOTX - node, R11 - dark soul
        // R11 - dark soul
        Label end;
    
        //cx->log("Rbtree materialize.");
        cx->op(TESTQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
        cx->op(JE, end);
    
        cx->op(CMPQ, R11, 0);
        cx->op(JE, end);
    
        cx->op(ANDQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);
        cx->op(MOVQ, R11, 0);
    
        cx->code_label(end);
        cx->epilogue();
    }
}


void compile_rbtree_occupy(Label label, Cx *cx) {
    cx->code_label_local(label, "rbtree_occupy");
    cx->prologue();
    // In: SELFX - tree, R10 - node size
    // Out: ROOTX - node or NIL
    // Clob: R10
    Label no_vacancy, no_reservation, init, no_last, end;

    //cx->log("Rbtree occupy.");
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_VACANT_OFFSET));
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, no_vacancy);
    
    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));
    cx->op(MOVQ, Address(SELFX, RBTREE_VACANT_OFFSET), R10);
    cx->op(JMP, init);
    
    cx->code_label(no_vacancy);
    cx->op(MOVQ, ROOTX, Address(SELFX, RBTREE_LENGTH_OFFSET));
    cx->op(CMPQ, ROOTX, Address(SELFX, RBTREE_RESERVATION_OFFSET));
    cx->op(JE, no_reservation);
    
    cx->op(IMUL2Q, ROOTX, R10);
    cx->op(ADDQ, ROOTX, RBTREE_HEADER_SIZE);
    cx->op(JMP, init);
    
    cx->code_label(no_reservation);
    cx->op(MOVQ, ROOTX, RBNODE_NIL);
    cx->epilogue();
    
    cx->code_label(init);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), RBNODE_NIL);
    cx->op(MOVQ, R10, Address(SELFX, RBTREE_LAST_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), R10);
    cx->op(ORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);  // red
    
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, no_last);
    
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_NEXT_OFFSET), ROOTX);
    cx->op(JMP, end);
    
    cx->code_label(no_last);
    cx->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), ROOTX);
    
    cx->code_label(end);
    cx->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), ROOTX);
    cx->op(INCQ, Address(SELFX, RBTREE_LENGTH_OFFSET));
    cx->epilogue();
}


void compile_rbtree_vacate(Label label, Cx *cx) {
    cx->code_label_local(label, "rbtree_vacate");
    cx->prologue();
    // In: SELFX - tree, ROOTX - node
    // Clob: R10, R11, THISX
    Label no_prev, prev_ok, no_next, next_ok;
    //cx->log("Rbtree vacate.");
    
    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    cx->op(ANDQ, R10, ~RBNODE_RED_BIT);  // R10 - prev index
    cx->op(MOVQ, THISX, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET));  // THISX - next index

    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, no_prev);
    
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_NEXT_OFFSET), THISX);
    cx->op(JMP, prev_ok);
    
    cx->code_label(no_prev);
    cx->op(MOVQ, Address(SELFX, RBTREE_FIRST_OFFSET), THISX);
    
    cx->code_label(prev_ok);
    cx->op(CMPQ, THISX, RBNODE_NIL);
    cx->op(JE, no_next);
    
    // set prev while keeping the color
    cx->op(MOVQ, R11, Address(SELFX, THISX, RBNODE_PRED_OFFSET));
    cx->op(ANDQ, R11, RBNODE_RED_BIT);  // next index color bit only
    cx->op(ORQ, R10, R11);  // colored prev index
    cx->op(MOVQ, Address(SELFX, THISX, RBNODE_PRED_OFFSET), R10);
    cx->op(JMP, next_ok);
    
    cx->code_label(no_next);
    cx->op(MOVQ, Address(SELFX, RBTREE_LAST_OFFSET), R10);
    
    cx->code_label(next_ok);
    cx->op(MOVQ, R10, Address(SELFX, RBTREE_VACANT_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_NEXT_OFFSET), R10);
    cx->op(MOVQ, Address(SELFX, RBTREE_VACANT_OFFSET), ROOTX);
    
    cx->op(DECQ, Address(SELFX, RBTREE_LENGTH_OFFSET));
    cx->epilogue();
}


void compile_rbtree_has(Label label, TypeSpec elem_ts, Cx *cx) {
    // SELFX - tree
    // ROOTX - node
    // KEYX - key / found index or NIL
    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("has"));
    cx->prologue();

    Label loop, finish, less, greater;

    cx->code_label(loop);
    //cx->log("Has loop.");
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, finish);

    Storage ks(MEMORY, Address(KEYX, 0));
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;
        
    elem_ts.compare(ks, vs, cx);
    cx->op(JL, less);
    cx->op(JG, greater);
    
    cx->code_label(finish);
    cx->op(MOVQ, KEYX, ROOTX);  // Found index or NIL
    cx->epilogue();
    
    cx->code_label(less);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(JMP, loop);
    
    cx->code_label(greater);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(JMP, loop);
}


void compile_rbtree_add(Label label, TypeSpec elem_ts, Cx *cx) {
    // Expects SELFX - tree, ROOTX - index, KEYX - key
    // Returns R10 - new subtree root, KEYX - index with uninitialized value
    // Clobbers THISX, THATX
    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("add"));
    cx->prologue();
    
    Label less, greater, no;
    Label left_fix = cx->once->compile(compile_rbtree_left_fix);
    Label right_fix = cx->once->compile(compile_rbtree_right_fix);
    Label occupy = cx->once->compile(compile_rbtree_occupy);
    int elem_size = elem_ts.measure_stack();
    int node_size = elem_size + RBNODE_HEADER_SIZE;
    
    //cx->runtime->log("Rbtree add.");
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, no);
    
    Storage ks(MEMORY, Address(KEYX, 0));
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;

    elem_ts.compare(ks, vs, cx);
    cx->op(JL, less);
    cx->op(JG, greater);
    
    // Found the value, destroy to make place for the new one
    //cx->runtime->log("Rbtree add found.");
    elem_ts.destroy(vs, cx);
    cx->op(MOVQ, R10, ROOTX);
    cx->op(MOVQ, KEYX, ROOTX);
    cx->epilogue();
    
    cx->code_label(less);
    //cx->runtime->log("Rbtree add left.");
    cx->op(PUSHQ, ROOTX);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CALL, label);
    cx->op(POPQ, ROOTX);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), R10);
    cx->op(CALL, left_fix);
    cx->epilogue();
    
    cx->code_label(greater);
    //cx->runtime->log("Rbtree add right.");
    cx->op(PUSHQ, ROOTX);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CALL, label);
    cx->op(POPQ, ROOTX);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), R10);
    cx->op(CALL, right_fix);
    cx->epilogue();
    
    cx->code_label(no);
    //cx->runtime->log("Rbtree add missing.");
    cx->op(MOVQ, R10, node_size);
    cx->op(CALL, occupy);  // from SELFX to ROOTX (may be NIL)
    cx->op(MOVQ, R10, ROOTX);
    cx->op(MOVQ, KEYX, ROOTX);
    cx->epilogue();
}


void compile_rbtree_remove(Label label, TypeSpec elem_ts, Cx *cx) {
    // Expects SELFX - tree, ROOTX - index, KEYX - key
    // Returns R10 - new index, R11 - dark_soul, KEYX - index with destroyed value
    // Clobbers THISX, THATX
    cx->code_label_local(label, elem_ts.prefix(rbtree_type).symbolize("remove"));
    cx->prologue();
    
    Label no, remove_left, remove_right;
    Label vacate = cx->once->compile(compile_rbtree_vacate);
    Label other_fix = cx->once->compile(compile_rbtree_other_fix);
    
    //cx->log("Rbtree remove.");
    cx->op(CMPQ, ROOTX, RBNODE_NIL);
    cx->op(JE, no);
    
    Storage ks(MEMORY, Address(KEYX, 0));  // can't use STACK, that would be popped!
    Storage vs(MEMORY, Address(SELFX, ROOTX, RBNODE_VALUE_OFFSET));
    if (COMPARE_CLOB & Regs(ROOTX, SELFX, KEYX))
        throw INTERNAL_ERROR;

    elem_ts.compare(ks, vs, cx);
    cx->op(JL, remove_left);
    cx->op(JG, remove_right);
    
    // Found the value, remove it
    Label no_left, no_right, no_children, was_red;
    //cx->log("Rbtree remove found.");
    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, no_left);
    
    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, no_right);
    
    // Has two children, find the smallest greater element to swap it with.
    // This violates the ordering for a moment, but since we're about to remove one of
    // them, it will be fine soon.
    // Keep the values at the same index, this saves us a value swap, which is nice for
    // keeping the age ordering, and even nicer for finalizer callbacks.
    
    // R10 already contains the right child, then traverse to the leftmost descendant
    Label leftward_loop, leftward_cond, same_color, child_swap, swapped;
    cx->op(MOVQ, THISX, RBNODE_NIL);  // replacement's parent
    cx->op(JMP, leftward_cond);
    
    //cx->log("Rbtree remove found internal.");
    cx->code_label(leftward_loop);
    cx->op(MOVQ, THISX, R10);
    cx->op(MOVQ, R10, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->code_label(leftward_cond);
    cx->op(CMPQ, Address(SELFX, R10, RBNODE_LEFT_OFFSET), RBNODE_NIL);
    cx->op(JNE, leftward_loop);
    
    // R10 - replacement node, THISX - replacement's parent or NIL
    // Swap the current (ROOTX) and replacement (R10) node tree links
    
    // Swap left child
    cx->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(XCHGQ, THATX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), THATX);

    // Swap right child
    cx->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(XCHGQ, THATX, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), THATX);
    
    // Swap color
    cx->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET));
    cx->op(XORQ, THATX, Address(SELFX, R10, RBNODE_PRED_OFFSET));
    cx->op(TESTQ, THATX, RBNODE_RED_BIT);
    cx->op(JE, same_color);  // xored == 0
    
    // The color bits differ, so complement both of them
    cx->op(XORQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(XORQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    
    cx->code_label(same_color);
    // Make the replacement parent point to the current node
    cx->op(CMPQ, THISX, RBNODE_NIL);
    cx->op(JE, child_swap);
    
    // The replacement is not the child of current, so just adjust the parent's left link
    cx->op(MOVQ, Address(SELFX, THISX, RBNODE_LEFT_OFFSET), ROOTX);
    cx->op(JMP, swapped);
    
    // The replacement is the child of current, so adjust the replacement's right link
    cx->code_label(child_swap);
    cx->op(MOVQ, Address(SELFX, R10, RBNODE_RIGHT_OFFSET), ROOTX);
    
    // The replacement took the place of the current node, so continue searching for
    // the same value on the right, because that's where we swapped it to.
    cx->code_label(swapped);
    cx->op(MOVQ, ROOTX, R10);
    cx->op(JMP, remove_right);
    
    cx->code_label(no_right);
    // A single red left child can be the replacement
    //cx->log("Rbtree remove found left only.");
    elem_ts.destroy(vs, cx);
    cx->op(MOVQ, KEYX, ROOTX);
    cx->op(PUSHQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CALL, vacate);  // At ROOTX
    cx->op(POPQ, R10);  // return the left child
    cx->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
    cx->op(MOVQ, R11, 0);  // no dark soul
    cx->epilogue();
        
    cx->code_label(no_left);
    cx->op(MOVQ, THATX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, THATX, RBNODE_NIL);
    cx->op(JE, no_children);
    
    // A single red right child can be the replacement
    //cx->log("Rbtree remove found right only.");
    elem_ts.destroy(vs, cx);
    cx->op(MOVQ, KEYX, ROOTX);
    cx->op(PUSHQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CALL, vacate);  // At ROOTX
    cx->op(POPQ, R10);  // return the right child
    cx->op(ANDQ, Address(SELFX, R10, RBNODE_PRED_OFFSET), ~RBNODE_RED_BIT);  // blacken
    cx->op(MOVQ, R11, 0);  // no dark soul
    cx->epilogue();
    
    // No children, just remove
    cx->code_label(no_children);
    //cx->log("Rbtree remove found leaf.");
    elem_ts.destroy(vs, cx);
    cx->op(MOVQ, KEYX, ROOTX);
    cx->op(CALL, vacate);
    cx->op(MOVQ, R11, 0);  // assume no dark soul
    cx->op(TESTQ, Address(SELFX, ROOTX, RBNODE_PRED_OFFSET), RBNODE_RED_BIT);
    cx->op(JNE, was_red);
    
    //cx->log("Rbtree remove found leaf releasing dark soul.");
    cx->op(MOVQ, R11, 1);  // well
    
    cx->code_label(was_red);
    cx->op(MOVQ, R10, RBNODE_NIL);
    cx->epilogue();
    
    // Descend to the left
    cx->code_label(remove_left);
    //cx->log("Rbtree remove left.");
    cx->op(PUSHQ, ROOTX);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CALL, label);
    cx->op(POPQ, ROOTX);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET), R10);

    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CALL, other_fix);
    cx->epilogue();
    
    // Descend to the right
    cx->code_label(remove_right);
    //cx->log("Rbtree remove right.");
    cx->op(PUSHQ, ROOTX);
    cx->op(MOVQ, ROOTX, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET));
    cx->op(CALL, label);
    cx->op(POPQ, ROOTX);
    cx->op(MOVQ, Address(SELFX, ROOTX, RBNODE_RIGHT_OFFSET), R10);

    cx->op(MOVQ, R10, Address(SELFX, ROOTX, RBNODE_LEFT_OFFSET));
    cx->op(CALL, other_fix);
    cx->epilogue();
    
    // Not found
    cx->code_label(no);
    //cx->log("Rbtree remove missing.");
    cx->op(MOVQ, KEYX, RBNODE_NIL);
    cx->op(MOVQ, R10, RBNODE_NIL);
    cx->op(MOVQ, R11, 0);  // no dark soul
    cx->epilogue();
}


void compile_rbtree_next(Label label, Cx *cx) {
    // Expects SELFX - tree, RAX - it
    // Returns RAX - new it or 0, R10 new index
    // Clobbers RCX, RDX
    cx->code_label_local(label, "rbtree_next");
    cx->prologue();
    Label terminate, find_leftmost, loop_leftmost, loop_check, loop, right_step, stepped, no_right;
    
    cx->op(MOVQ, R10, Address(SELFX, RBTREE_ROOT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, terminate);
    
    cx->op(MOVQ, RCX, 1);  // mask
    cx->op(CMPQ, RAX, 0);
    cx->op(JE, find_leftmost);
    
    cx->op(MOVQ, RDX, -2);  // inverse full mask
    cx->op(PUSHQ, RBNODE_NIL);  // last_left_index
    cx->op(PUSHQ, 0);  // last_left_mask
    cx->op(JMP, loop_check);
    
    cx->code_label(loop);
    cx->op(TESTQ, RAX, RCX);  // the mask bit is then a direction
    cx->op(JNE, right_step);
    
    cx->op(MOVQ, Address(RSP, 8), R10);
    cx->op(MOVQ, Address(RSP, 0), RCX);
    cx->op(MOVQ, R10, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(JMP, stepped);
    
    cx->code_label(right_step);
    cx->op(MOVQ, R10, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    
    cx->code_label(stepped);
    cx->op(SHLQ, RCX, 1);
    cx->op(SHLQ, RDX, 1);
    
    cx->code_label(loop_check);
    cx->op(TESTQ, RAX, RDX);  // are there bits set above mask?
    cx->op(JNE, loop);
    
    // Found the previous node
    cx->op(MOVQ, R10, Address(SELFX, R10, RBNODE_RIGHT_OFFSET));
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, no_right);
    
    cx->op(SHLQ, RCX, 1);
    cx->op(ADDQ, RSP, 16);
    cx->op(JMP, find_leftmost);
    
    cx->code_label(no_right);
    cx->op(POPQ, RCX);  // last_left_mask
    cx->op(POPQ, R10);  // last_left_index
    cx->op(CMPQ, R10, RBNODE_NIL);
    cx->op(JE, terminate);
    
    // Backtrack to followup ancestor
    cx->op(MOVQ, RDX, RCX);
    cx->op(DECQ, RDX);
    cx->op(ANDQ, RAX, RDX);
    cx->op(ORQ, RAX, RCX);
    cx->epilogue();
    
    cx->code_label(loop_leftmost);
    cx->op(MOVQ, R10, RDX);
    cx->op(SHLQ, RCX, 1);
    
    cx->code_label(find_leftmost);
    cx->op(MOVQ, RDX, Address(SELFX, R10, RBNODE_LEFT_OFFSET));
    cx->op(CMPQ, RDX, RBNODE_NIL);
    cx->op(JNE, loop_leftmost);
    
    cx->op(ORQ, RAX, RCX);
    cx->epilogue();
    
    cx->code_label(terminate);
    cx->op(MOVQ, RAX, 0);
    cx->epilogue();
}

