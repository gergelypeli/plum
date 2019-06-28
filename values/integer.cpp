#include "../plum.h"


IntegerOperationValue::IntegerOperationValue(OperationType o, Value *pivot, TypeMatch &match)
    :OptimizedOperationValue(o, op_arg_ts(o, match), op_ret_ts(o, match), pivot,
    is_assignment(o) ? PTR_SUBSET : GPR_SUBSET, GPR_SUBSET
    ) {
    int size = match[0].measure_raw();
    os = (
        size == 1 ? 0 :
        size == 2 ? 1 :
        size == 4 ? 2 :
        size == 8 ? 3 :
        throw INTERNAL_ERROR
    );

    if (operation != ASSIGN && operation != EQUAL && operation != NOT_EQUAL)
        is_unsigned = ptr_cast<BasicType>(match[0].rvalue()[0])->get_unsigned();
    else
        is_unsigned = false;
}

void IntegerOperationValue::exponentiation_by_squaring(Cx *cx, Register BASE, Register EXP, Register RES) {
    // RES = BASE ** EXP
    
    int xos = os == 0 ? 1 : os;  // No IMULB
    
    Label loop_label;
    Label skip_label;
    
    cx->op(MOVQ % os, RES, 1);
    
    cx->code_label(loop_label);
    cx->op(TESTQ % os, EXP, 1);
    cx->op(JE, skip_label);
    cx->op(IMUL2Q % xos, RES, BASE);
    
    cx->code_label(skip_label);
    cx->op(IMUL2Q % xos, BASE, BASE);
    cx->op(SHRQ % os, EXP, 1);
    cx->op(JNE, loop_label);
}

bool IntegerOperationValue::fits32(int value) {
    // We don't want to rely on the C++ truncation mechanism, so
    // as soon as the values don't fit in our types, stop keeping
    // them constants. Also, x64 immediate values can only be at
    // most signed dwords, so anything not fitting in that must
    // be dropped, too.
    
    return (
        is_unsigned ? (
            os == 0 ? value >= 0 && value <= 255 :
            os == 1 ? value >= 0 && value <= 65535 :
            value >= 0 && value <= MAX_SIGNED_DWORD
        ) : (
            os == 0 ? value >= -128 && value <= 127 :
            os == 1 ? value >= -32768 && value <= 32767 :
            value >= -1 - MAX_SIGNED_DWORD && value <= MAX_SIGNED_DWORD
        )
    );
}

Storage IntegerOperationValue::unary(Cx *cx, UnaryOp opcode) {
    subcompile(cx);

    if (auxls.where != REGISTER)
        throw INTERNAL_ERROR;
    
    switch (ls.where) {
    case CONSTANT: {
        int value = opcode % 3 == NEGQ ? -ls.value : ~ls.value;
        
        if (fits32(value))
            return Storage(CONSTANT, value);
            
        cx->op(MOVQ % os, auxls.reg, ls.value);
        cx->op(opcode % os, auxls.reg);
        return auxls;
    }
    case REGISTER:
        cx->op(opcode % os, ls.reg);
        return Storage(REGISTER, ls.reg);
    case MEMORY:
        cx->op(MOVQ % os, auxls.reg, ls.address);
        cx->op(opcode % os, auxls.reg);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_simple(Cx *cx, BinaryOp opcode) {
    bool commutative = (opcode % 3) != SUBQ;

    subcompile(cx);

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        int value = (
            (opcode % 3) == ADDQ ? ls.value + rs.value :
            (opcode % 3) == SUBQ ? ls.value - rs.value :
            (opcode % 3) == ANDQ ? ls.value & rs.value :
            (opcode % 3) == ORQ  ? ls.value | rs.value :
            (opcode % 3) == XORQ ? ls.value ^ rs.value :
            throw INTERNAL_ERROR
        );
        
        if (fits32(value))
            return Storage(CONSTANT, value);
        
        cx->op(MOVQ % os, auxls.reg, ls.value);
        cx->op(opcode % os, auxls.reg, rs.value);
        return auxls;
    }
    case CONSTANT_REGISTER:
        if (commutative) {
            cx->op(opcode % os, rs.reg, ls.value);
            return Storage(REGISTER, rs.reg);
        }
        else {
            cx->op(MOVQ % os, auxls.reg, ls.value);
            cx->op(opcode % os, auxls.reg, rs.reg);
            return auxls;
        }
    case CONSTANT_MEMORY:
        cx->op(MOVQ % os, auxls.reg, ls.value);
        cx->op(opcode % os, auxls.reg, rs.address);
        return auxls;
    case REGISTER_CONSTANT:
        cx->op(opcode % os, ls.reg, rs.value);
        return Storage(REGISTER, ls.reg);
    case REGISTER_REGISTER:
        cx->op(opcode % os, ls.reg, rs.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_MEMORY:
        cx->op(opcode % os, ls.reg, rs.address);
        return Storage(REGISTER, ls.reg);
    case MEMORY_CONSTANT:
        cx->op(MOVQ % os, auxls.reg, ls.address);
        cx->op(opcode % os, auxls.reg, rs.value);
        return auxls;
    case MEMORY_REGISTER:
        if (commutative) {
            cx->op(opcode % os, rs.reg, ls.address);
            return Storage(REGISTER, rs.reg);
        }
        else {
            cx->op(MOVQ % os, auxls.reg, ls.address);
            cx->op(opcode % os, auxls.reg, rs.reg);
            return auxls;
        }
    case MEMORY_MEMORY:
        cx->op(MOVQ % os, auxls.reg, ls.address);
        cx->op(opcode % os, auxls.reg, rs.address);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_multiply(Cx *cx) {
    subcompile(cx);

    // Yay, IMUL2 and IMUL3 truncates the result, so it can be used for
    // both signed and unsigned multiplication.
    // Use IMULW for bytes, and ignore the upper byte.
    int ios = os == 0 ? 1 : os;

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        int value = ls.value * rs.value;
        
        if (fits32(value))
            return Storage(CONSTANT, value);
            
        cx->op(MOVQ % os, auxls.reg, ls.value);
        cx->op(IMUL3Q % ios, auxls.reg, auxls.reg, rs.value);

        return auxls;
    }
    case CONSTANT_REGISTER:
        cx->op(IMUL3Q % ios, rs.reg, rs.reg, ls.value);
        return Storage(REGISTER, rs.reg);
    case CONSTANT_MEMORY:
        cx->op(IMUL3Q % ios, auxls.reg, rs.address, ls.value);
        return auxls;
    case REGISTER_CONSTANT:
        cx->op(IMUL3Q % ios, ls.reg, ls.reg, rs.value);
        return Storage(REGISTER, ls.reg);
    case REGISTER_REGISTER:
        cx->op(IMUL2Q % ios, ls.reg, rs.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_MEMORY:
        cx->op(IMUL2Q % ios, ls.reg, rs.address);
        return Storage(REGISTER, ls.reg);
    case MEMORY_CONSTANT:
        cx->op(IMUL3Q % ios, auxls.reg, ls.address, rs.value);
        return auxls;
    case MEMORY_REGISTER:
        cx->op(IMUL2Q % ios, rs.reg, ls.address);
        return Storage(REGISTER, rs.reg);
    case MEMORY_MEMORY:
        cx->op(MOVQ % os, auxls.reg, ls.address);
        cx->op(IMUL2Q % ios, auxls.reg, rs.address);
        return auxls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_divmod(Cx *cx, bool mod) {
    subcompile(cx);

    BinaryOp mov = MOVQ % os;
    DivModOp op = (is_unsigned ? (mod ? MODQ : DIVQ) : (mod ? IMODQ : IDIVQ)) % os;

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        int value = mod ? ls.value % rs.value : ls.value / rs.value;
        return Storage(CONSTANT, value);
    }
    case CONSTANT_REGISTER:
        cx->op(mov, auxls.reg, ls.value);
        cx->op(op, auxls.reg, rs.reg);
        return Storage(REGISTER, auxls.reg);
    case CONSTANT_MEMORY:
        cx->op(mov, auxls.reg, ls.value);
        cx->op(mov, R10, rs.address);
        cx->op(op, auxls.reg, R10);
        return Storage(REGISTER, auxls.reg);
    case REGISTER_CONSTANT:
        cx->op(mov, R10, rs.value);
        cx->op(op, ls.reg, R10);
        return Storage(REGISTER, ls.reg);
    case REGISTER_REGISTER:
        cx->op(op, ls.reg, rs.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_MEMORY:
        cx->op(mov, R10, rs.address);
        cx->op(op, ls.reg, R10);
        return Storage(REGISTER, ls.reg);
    case MEMORY_CONSTANT:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(mov, R10, rs.value);
        cx->op(op, auxls.reg, R10);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_REGISTER:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(op, auxls.reg, rs.reg);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_MEMORY:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(mov, R10, rs.address);
        cx->op(op, auxls.reg, R10);
        return Storage(REGISTER, auxls.reg);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_shift(Cx *cx, ShiftOp opcode) {
    subcompile(cx);

    BinaryOp mov = MOVQ % os;
    ShiftOp op = opcode % os;

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        // FIXME: implement all
        int value = (
            (opcode % 3) == RCLQ ? throw INTERNAL_ERROR :
            (opcode % 3) == RCRQ ? throw INTERNAL_ERROR :
            (opcode % 3) == ROLQ ? throw INTERNAL_ERROR :
            (opcode % 3) == RORQ ? throw INTERNAL_ERROR :
            (opcode % 3) == SALQ ? throw INTERNAL_ERROR :
            (opcode % 3) == SARQ ? throw INTERNAL_ERROR :
            (opcode % 3) == SHLQ ? ls.value << rs.value :
            (opcode % 3) == SHRQ ? ls.value >> rs.value :
            throw INTERNAL_ERROR
        );

        if (fits32(value))
            return Storage(CONSTANT, value);

        cx->op(mov, auxls.reg, ls.value);
        cx->op(op, auxls.reg, rs.value);
        return Storage(REGISTER, auxls.reg);
    }
    case CONSTANT_REGISTER:
        cx->op(mov, auxls.reg, ls.value);
        cx->op(op, auxls.reg, rs.reg);
        return Storage(REGISTER, auxls.reg);
    case CONSTANT_MEMORY:
        cx->op(mov, auxls.reg, ls.value);
        cx->op(mov, R10, rs.address);
        cx->op(op, auxls.reg, R10);
        return Storage(REGISTER, auxls.reg);
    case REGISTER_CONSTANT:
        cx->op(op, ls.reg, rs.value);
        return Storage(REGISTER, ls.reg);
    case REGISTER_REGISTER:
        cx->op(op, ls.reg, rs.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_MEMORY:
        cx->op(mov, R10, rs.address);
        cx->op(op, ls.reg, R10);
        return Storage(REGISTER, ls.reg);
    case MEMORY_CONSTANT:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(op, auxls.reg, rs.value);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_REGISTER:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(op, auxls.reg, rs.reg);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_MEMORY:
        cx->op(mov, auxls.reg, ls.address);
        cx->op(mov, R10, rs.address);
        cx->op(op, auxls.reg, R10);
        return Storage(REGISTER, auxls.reg);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_exponent(Cx *cx) {
    subcompile(cx);

    BinaryOp mov = MOVQ % os;

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        cx->op(mov, R10, ls.value);
        cx->op(mov, R11, rs.value);
        exponentiation_by_squaring(cx, R10, R11, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    }
    case CONSTANT_REGISTER:
        cx->op(mov, R10, ls.value);
        exponentiation_by_squaring(cx, R10, rs.reg, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    case CONSTANT_MEMORY:
        cx->op(mov, R10, ls.value);
        cx->op(mov, R11, rs.address);
        exponentiation_by_squaring(cx, R10, R11, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    case REGISTER_CONSTANT:
        cx->op(mov, R10, ls.reg);
        cx->op(mov, R11, rs.value);
        exponentiation_by_squaring(cx, R10, R11, ls.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_REGISTER:
        cx->op(mov, R10, ls.reg);
        exponentiation_by_squaring(cx, R10, rs.reg, ls.reg);
        return Storage(REGISTER, ls.reg);
    case REGISTER_MEMORY:
        cx->op(mov, R10, ls.reg);
        cx->op(mov, R11, rs.address);
        exponentiation_by_squaring(cx, R10, R11, ls.reg);
        return Storage(REGISTER, ls.reg);
    case MEMORY_CONSTANT:
        cx->op(mov, R10, ls.address);
        cx->op(mov, R11, rs.value);
        exponentiation_by_squaring(cx, R10, R11, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_REGISTER:
        cx->op(mov, R10, ls.address);
        exponentiation_by_squaring(cx, R10, rs.reg, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    case MEMORY_MEMORY:
        cx->op(mov, R10, ls.address);
        cx->op(mov, R11, rs.address);
        exponentiation_by_squaring(cx, R10, R11, auxls.reg);
        return Storage(REGISTER, auxls.reg);
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::binary_compare(Cx *cx, ConditionCode cc) {
    subcompile(cx);

    switch (ls.where * rs.where) {
    case CONSTANT_CONSTANT: {
        bool holds = (
            cc == CC_EQUAL ? ls.value == rs.value :
            cc == CC_NOT_EQUAL ? ls.value != rs.value :
            cc == CC_LESS || cc == CC_BELOW ? ls.value < rs.value :
            cc == CC_LESS_EQUAL || cc == CC_BELOW_EQUAL ? ls.value <= rs.value :
            cc == CC_GREATER || cc == CC_ABOVE ? ls.value > rs.value :
            cc == CC_GREATER_EQUAL || cc == CC_ABOVE_EQUAL ? ls.value >= rs.value :
            throw INTERNAL_ERROR
        );

        return Storage(CONSTANT, holds ? 1 : 0);
        }
    case CONSTANT_REGISTER:
        cx->op(CMPQ % os, rs.reg, ls.value);
        return Storage(FLAGS, swapped(cc));
    case CONSTANT_MEMORY:
        cx->op(CMPQ % os, rs.address, ls.value);
        return Storage(FLAGS, swapped(cc));
    case REGISTER_CONSTANT:
        cx->op(CMPQ % os, ls.reg, rs.value);
        return Storage(FLAGS, cc);
    case REGISTER_REGISTER:
        cx->op(CMPQ % os, ls.reg, rs.reg);
        return Storage(FLAGS, cc);
    case REGISTER_MEMORY:
        cx->op(CMPQ % os, ls.reg, rs.address);
        return Storage(FLAGS, cc);
    case MEMORY_CONSTANT:
        cx->op(CMPQ % os, ls.address, rs.value);
        return Storage(FLAGS, cc);
    case MEMORY_REGISTER:
        cx->op(CMPQ % os, ls.address, rs.reg);
        return Storage(FLAGS, cc);
    case MEMORY_MEMORY:
        cx->op(MOVQ % os, auxls.reg, ls.address);
        cx->op(CMPQ % os, auxls.reg, rs.address);
        return Storage(FLAGS, cc);
    default:
        throw INTERNAL_ERROR;
    }
}

// TODO: Hm... this seems a bit questionable...
// NOTE: for lvalue operations, reg is allocated so that it can be used
// for the left operand. Since it is MEMORY, it may be the base register of its
// address, and it may have been chosen to restore a spilled dynamic address.
// As such, unlike with rvalue operations, reg is not guaranteed to be
// distinct from ls.address.base. Since these operations return the left address,
// for a working register for values R10 must be used.

Storage IntegerOperationValue::assign_binary(Cx *cx, BinaryOp opcode) {
    subcompile(cx);
    
    Storage als = lmemory(cx);
        
    switch (rs.where) {
    case CONSTANT:
        cx->op(opcode % os, als.address, rs.value);
        return ls;
    case REGISTER:
        cx->op(opcode % os, als.address, rs.reg);
        return ls;
    case MEMORY:
        cx->op(MOVQ % os, R10, rs.address);
        cx->op(opcode % os, als.address, R10);
        return ls;
    default:
        throw INTERNAL_ERROR;
    }
}

Storage IntegerOperationValue::assign_multiply(Cx *cx) {
    subcompile(cx);

    Storage als = lmemory(cx);

    // Use IMULW for bytes, and ignore the upper byte.
    int ios = os == 0 ? 1 : os;

    switch (rs.where) {
    case CONSTANT:
        cx->op(IMUL3Q % ios, R10, als.address, rs.value);
        cx->op(MOVQ % os, als.address, R10);
        return ls;
    case REGISTER:
        cx->op(IMUL2Q % ios, rs.reg, als.address);
        cx->op(MOVQ % os, als.address, rs.reg);
        return ls;
    case MEMORY:
        cx->op(MOVQ % os, R10, als.address);
        cx->op(IMUL2Q % ios, R10, rs.address);
        cx->op(MOVQ % os, als.address, R10);
        return ls;
    default:
        throw INTERNAL_ERROR;
    }

    return ls;
}

Storage IntegerOperationValue::assign_divmod(Cx *cx, bool mod) {
    subcompile(cx);

    Storage als = lmemory(cx);

    BinaryOp mov = MOVQ % os;
    DivModOp op = (is_unsigned ? (mod ? MODQ : DIVQ) : (mod ? IMODQ : IDIVQ)) % os;
    
    cx->op(mov, R10, als.address);
    Register y;

    switch (rs.where) {
    case CONSTANT:
        cx->op(mov, R11, rs.value);
        y = R11;
        break;
    case REGISTER:
        y = rs.reg;
        break;
    case MEMORY:
        cx->op(mov, R11, rs.address);
        y = R11;
        break;
    default:
        throw INTERNAL_ERROR;
    }

    cx->op(op, R10, y);

    cx->op(mov, als.address, R10);
    return ls;
}

Storage IntegerOperationValue::assign_exponent(Cx *cx) {
    subcompile(cx);

    Storage als = lmemory(cx);

    BinaryOp mov = MOVQ % os;

    cx->op(mov, R10, als.address);

    switch (rs.where) {
    case CONSTANT:
        cx->op(PUSHQ, RAX);
        cx->op(mov, RAX, rs.value);
        exponentiation_by_squaring(cx, R10, RAX, R11);
        cx->op(POPQ, RAX);
        cx->op(mov, als.address, R11);
        break;
    case REGISTER:
        exponentiation_by_squaring(cx, R10, rs.reg, R11);
        cx->op(mov, als.address, R11);
        break;
    case MEMORY:
        cx->op(mov, R11, rs.address);
        cx->op(PUSHQ, RAX);
        cx->op(mov, RAX, R11);
        exponentiation_by_squaring(cx, R10, RAX, R11);
        cx->op(POPQ, RAX);
        cx->op(mov, als.address, R11);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    return ls;
}

Storage IntegerOperationValue::assign_shift(Cx *cx, ShiftOp opcode) {
    subcompile(cx);
    
    Storage als = lmemory(cx);

    BinaryOp mov = MOVQ % os;
    ShiftOp op = opcode % os;
    
    cx->op(mov, R10, als.address);

    switch (rs.where) {
    case CONSTANT:
        cx->op(op, R10, rs.value);
        break;
    case REGISTER:
        cx->op(op, R10, rs.reg);
        break;
    case MEMORY:
        cx->op(mov, R11, rs.address);
        cx->op(op, R10, R11);
        break;
    default:
        throw INTERNAL_ERROR;
    }

    cx->op(mov, als.address, R10);
    return ls;
}

Regs IntegerOperationValue::precompile(Regs preferred) {
    Regs clob = OptimizedOperationValue::precompile(preferred);

    return clob;
}

Storage IntegerOperationValue::compile(Cx *cx) {
    switch (operation) {
    case COMPLEMENT:
        return unary(cx, NOTQ);
    case NEGATE:
        return unary(cx, NEGQ);
    case ADD:
        return binary_simple(cx, ADDQ);
    case SUBTRACT:
        return binary_simple(cx, SUBQ);
    case MULTIPLY:
        return binary_multiply(cx);
    case DIVIDE:
        return binary_divmod(cx, false);
    case MODULO:
        return binary_divmod(cx, true);
    case OR:
        return binary_simple(cx, ORQ);
    case XOR:
        return binary_simple(cx, XORQ);
    case AND:
        return binary_simple(cx, ANDQ);
    case SHIFT_LEFT:
        return binary_shift(cx, SHLQ);
    case SHIFT_RIGHT:
        return binary_shift(cx, SHRQ);
    case EXPONENT:
        return binary_exponent(cx);
    case EQUAL:
        return binary_compare(cx, CC_EQUAL);
    case NOT_EQUAL:
        return binary_compare(cx, CC_NOT_EQUAL);
    case LESS:
        return binary_compare(cx, is_unsigned ? CC_BELOW : CC_LESS);
    case GREATER:
        return binary_compare(cx, is_unsigned ? CC_ABOVE : CC_GREATER);
    case LESS_EQUAL:
        return binary_compare(cx, is_unsigned ? CC_BELOW_EQUAL : CC_LESS_EQUAL);
    case GREATER_EQUAL:
        return binary_compare(cx, is_unsigned ? CC_ABOVE_EQUAL : CC_GREATER_EQUAL);
    case ASSIGN_ADD:
        return assign_binary(cx, ADDQ);
    case ASSIGN_SUBTRACT:
        return assign_binary(cx, SUBQ);
    case ASSIGN_MULTIPLY:
        return assign_multiply(cx);
    case ASSIGN_DIVIDE:
        return assign_divmod(cx, false);
    case ASSIGN_MODULO:
        return assign_divmod(cx, true);
    case ASSIGN_EXPONENT:
        return assign_exponent(cx);
    case ASSIGN_OR:
        return assign_binary(cx, ORQ);
    case ASSIGN_XOR:
        return assign_binary(cx, XORQ);
    case ASSIGN_AND:
        return assign_binary(cx, ANDQ);
    case ASSIGN_SHIFT_LEFT:
        return assign_shift(cx, SHLQ);
    case ASSIGN_SHIFT_RIGHT:
        return assign_shift(cx, SHRQ);
    default:
        return OptimizedOperationValue::compile(cx);
    }
}
