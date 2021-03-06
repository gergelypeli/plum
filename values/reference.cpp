#include "../plum.h"


ReferenceOperationValue::ReferenceOperationValue(OperationType o, Value *l, TypeMatch &match)
    :GenericOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), l) {
}



PointerOperationValue::PointerOperationValue(OperationType o, Value *l, TypeMatch &match)
    :ReferenceOperationValue(o, l, match) {
}

