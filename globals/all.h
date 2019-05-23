
template <class T, class S> T *ptr_cast(S *s) {
    return dynamic_cast<T *>(s);
}


// Makers
template <typename T>
Value *make() {
    return new T;
}

template <typename T, typename A>
Value *make(A a) {
    return new T(a);
}

template <typename T, typename A, typename B>
Value *make(A a, B b) {
    return new T(a, b);
}

template <typename T, typename A, typename B, typename C>
Value *make(A a, B b, C c) {
    return new T(a, b, c);
}

template <typename T, typename A, typename B, typename C, typename D>
Value *make(A a, B b, C c, D d) {
    return new T(a, b, c, d);
}

template <typename T, typename A, typename B, typename C, typename D, typename E>
Value *make(A a, B b, C c, D d, E e) {
    return new T(a, b, c, d, e);
}

template <typename T, typename A, typename B, typename C, typename D, typename E, typename F>
Value *make(A a, B b, C c, D d, E e, F f) {
    return new T(a, b, c, d, e, f);
}


#include "typespec.h"
#include "builtins.h"
#include "functions.h"
#include "typematch.h"
#include "runtime.h"
#include "modules.h"


// Streamification
void stream_preappend2(Address alias_addr, X64 *x64);
void streamify_ascii(std::string s, Address alias_addr, X64 *x64);


// Nosy stuff
void compile_nosytree_callback(Label label, TypeSpec elem_ts, X64 *x64);
//void compile_weakref_nosy_callback(Label label, X64 *x64);
void rbtree_fcb_action(Label action_label, TypeSpec elem_ts, X64 *x64);
void compile_rbtree_clone(Label label, TypeSpec elem_ts, X64 *x64);

// TODO
Storage preinitialize_class(TypeSpec class_ts, X64 *x64);

