void compile_rbtree_left_fix(Label label, Cx *cx);
void compile_rbtree_right_fix(Label label, Cx *cx);
void compile_rbtree_other_fix(Label label, Cx *cx);
void compile_rbtree_occupy(Label label, Cx *cx);
void compile_rbtree_vacate(Label label, Cx *cx);
void compile_rbtree_has(Label label, TypeSpec elem_ts, Cx *cx);
void compile_rbtree_add(Label label, TypeSpec elem_ts, Cx *cx);
void compile_rbtree_remove(Label label, TypeSpec elem_ts, Cx *cx);
void compile_rbtree_next(Label label, Cx *cx);
