#include "../plum.h"


LvalueTupleValue::LvalueTupleValue()
    :Value(NO_TS) {
}

bool LvalueTupleValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (kwargs.size() > 0) {
        std::cerr << "Can't handle labels in tuples yet!\n";
        return NULL;
    }

    for (auto &arg : args) {
        Value *value = typize(arg.get(), scope);

        if (value->ts[0] != lvalue_type && value->ts[0] != uninitialized_type)
            return false;

        tss.push_back(value->ts);
        values.push_back(std::unique_ptr<Value>(value));
    }

    ts.pack_tuple(tss);
    
    return true;
}

std::vector<Storage> LvalueTupleValue::get_storages() {
    return storages;
}

Declaration *LvalueTupleValue::get_declaration(int i) {
    DeclarationValue *dv = ptr_cast<DeclarationValue>(values[i].get());
    
    return (dv ? declaration_get_decl(dv) : NULL);
}

TypeSpec LvalueTupleValue::fix_bare(int i, TypeSpec implicit_ts, Scope *scope) {
    // Used if a member is a bare declaration, and its type must be derived from the
    // right hand side of an unpacking
    DeclarationValue *dv = ptr_cast<DeclarationValue>(values[i].get());

    std::cerr << "Fixing bare declaration " << i << " with " << implicit_ts << ".\n";
    dv->fix_bare(implicit_ts, scope);

    tss[i] = dv->ts.unprefix(uninitialized_type);
    return dv->ts;
}

Regs LvalueTupleValue::precompile(Regs preferred) {
    Regs clob;
    
    for (auto &v : values)
        clob = clob | v->precompile_tail();

    return clob;
}

Storage LvalueTupleValue::compile(X64 *x64) {
    x64->unwind->push(this);

    for (unsigned i = 0; i < values.size(); i++) {
        // Since lvalues, these will be MEMORY or ALIAS
        Storage s = values[i]->compile(x64);
        if (s.where != MEMORY && s.where != ALIAS)
            throw INTERNAL_ERROR;

        if (s.regs().has_gpr()) {  // that is, non-RBP MEMORY
            StorageWhere where = stacked(tss[i].where(AS_ARGUMENT));
            if (where != ALISTACK)
                throw INTERNAL_ERROR;
            
            Storage t(where);
            values[i]->ts.store(s, t, x64);
            storages.push_back(t);
        }
        else {
            storages.push_back(s);
        }
    }
    
    x64->unwind->pop(this);

    return Storage();  // Well...
}

CodeScope *LvalueTupleValue::unwind(X64 *x64) {
    for (int i = storages.size() - 1; i >= 0; i--)
        tss[i].store(storages[i], Storage(), x64);  // dropping a MEMORY is a no-op
        
    return NULL;
}




UnpackingValue::UnpackingValue(Value *l)
    :Value(VOID_TS) {
    left.reset(ptr_cast<LvalueTupleValue>(l));
    
    if (!left)
        throw INTERNAL_ERROR;
        
    left_total = 0;
    scope = NULL;
}

bool UnpackingValue::check(Args &args, Kwargs &kwargs, Scope *scope) {
    if (args.size() != 1 || kwargs.size() != 0) {
        std::cerr << "Whacky unpacking!\n";
        return false;
    }
    
    Expr *expr = args[0].get();
    
    // Must put potential declarations on the left after all others, like CreateValue
    for (unsigned i = 0; i < left->values.size(); i++)
        declarations.push_back(left->get_declaration(i));
    
    for (int i = declarations.size() - 1; i >= 0; i--)
        if (declarations[i])
            scope->remove(declarations[i]);

    left_tss = left->tss;
    TSs arg_tss;
    
    for (auto &lts : left_tss) {
        if (lts == BARE_UNINITIALIZED_TS)
            arg_tss.push_back(ANY_TS);
        else if (lts[0] == lvalue_type || lts[0] == uninitialized_type)
            arg_tss.push_back(lts.unprefix(lts[0]));
        else
            throw INTERNAL_ERROR;
    }
        
    TypeSpec arg_ts;
    arg_ts.pack_tuple(arg_tss);
    
    Value *value = typize(expr, scope, &arg_ts);
    
    if (!value->ts.is_tuple()) {
        std::cerr << "Tuple value is needed in an unpacking!\n";
        return false;
    }
    
    value->ts.unpack_tuple(right_tss);
        
    for (unsigned i = 0; i < left_tss.size(); i++) {
        TypeSpec left_ts = left_tss[i];
        
        if (i >= right_tss.size()) {
            std::cerr << "Too few values to unpack!\n";
            return false;
        }
        
        TypeSpec &right_ts = right_tss[i];
        
        // TODO: this may be too strict, but we can't call typespec, because we don't
        // have a value for the right side, and we can't convert the type either.
        if (left_ts[0] == uninitialized_type) {
            if (left_ts == BARE_UNINITIALIZED_TS) {
                // Fix bare declaration, and place it in its scope
                left_ts = left->fix_bare(i, right_ts, scope);
                declarations[i] = left->get_declaration(i);
            }
            else {
                // Put typed declaration back to its scope
                scope->add(declarations[i]);
            }

            left_ts = left_ts.reprefix(uninitialized_type, lvalue_type);
        }
        
        if (right_ts != left_ts.rvalue()) {
            std::cerr << "Mismatching types in unpacking: " << left_ts << " = " << right_ts << "\n";
            return false;
        }
    }
    
    right.reset(value);
    this->scope = scope;
    return true;
}

Regs UnpackingValue::precompile(Regs preferred) {
    Regs clob = right->precompile_tail();
    clob = clob | left->precompile_tail();  // will be pushed
    
    return clob | RAX;
}

Storage UnpackingValue::compile(X64 *x64) {
    left->compile(x64);

    // A MultiLvalue only stores dynamic addresses as ALISTACK,
    // static addresses are kept as MEMORY or ALIAS, so we're safe for stack relocations.
    std::vector<Storage> left_storages = left->get_storages();
    std::vector<unsigned> left_sizes;
    
    for (auto &left_storage : left_storages) {
        StorageWhere where = left_storage.where;
        if (where == MEMORY || where == ALIAS) {
            left_sizes.push_back(0);
        }
        else if (where == ALISTACK) {
            left_sizes.push_back(ALIAS_SIZE);
            left_total += ALIAS_SIZE;
        }
        else
            throw INTERNAL_ERROR;
    }
    
    x64->unwind->push(this);
    right->compile(x64);
    x64->unwind->pop(this);

    std::vector<Storage> right_storages;
    std::vector<unsigned> right_sizes;
    int right_total = 0;
    
    for (auto &right_ts : right_tss) {
        StorageWhere where = stacked(right_ts.where(AS_ARGUMENT));  // TODO: always STACK?
        right_storages.push_back(Storage(where));
        int size = right_ts.measure_where(where);
        right_sizes.push_back(size);
        right_total += size;
    }

    int offset = right_total;
    int left_count = left_tss.size();

    for (int i = right_tss.size() - 1; i >= 0; i--) {
        Storage s, t;

        switch (i < left_count ? left_storages[i].where : NOWHERE) {
        case NOWHERE:
            t = Storage();
            break;
        case MEMORY:
            t = left_storages[i];
            break;
        case ALISTACK:
            x64->op(MOVQ, RAX, Address(RSP, offset));
            t = Storage(MEMORY, Address(RAX, 0));
            break;
        case ALIAS:
            x64->op(MOVQ, RAX, left_storages[i].address);
            t = Storage(MEMORY, Address(RAX, left_storages[i].value));
            break;
        default:
            throw INTERNAL_ERROR;
        }

        switch (right_storages[i].where) {
        case STACK:
            s = right_storages[i];
            break;
        default:
            throw INTERNAL_ERROR;
        }
        
        if (i >= left_count) {
            std::cerr << "Dropping multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
            right_tss[i].store(s, t, x64);
        }
        else if (left_tss[i][0] == uninitialized_type) {
            std::cerr << "Initializing multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
            right_tss[i].create(s, t, x64);
        }
        else {
            std::cerr << "Assigning multi member " << i << " from " << s << " to " << t << " occupying " << right_sizes[i] << " bytes.\n";
            right_tss[i].store(s, t, x64);
        }
        
        offset += (i < left_count ? left_sizes[i] : 0);
        offset -= right_sizes[i];
    }
    
    if (offset != left_total)
        throw INTERNAL_ERROR;
        
    // Drop potential ALIAS-es from the MultiLvalue.
    // The type itself does not know the number of such items, so we had to compute it.
    x64->op(ADDQ, RSP, left_total);
        
    return Storage();
}

CodeScope *UnpackingValue::unwind(X64 *x64) {
    if (left_total)
        x64->op(ADDQ, RSP, left_total);
        
    return NULL;
}

void UnpackingValue::escape_statement_variables() {
    scope->enter();
    
    for (int i = declarations.size() - 1; i >= 0 ; i--)
        if (declarations[i])
            scope->remove(declarations[i]);
            
    scope->leave();
            
    for (unsigned i = 0; i < declarations.size() ; i++)
        if (declarations[i])
            scope->outer_scope->add(declarations[i]);
}




ScalarConversionValue::ScalarConversionValue(Value *p)
    :Value(NO_TS) {
    orig.reset(p);
    
    orig->ts.unpack_tuple(tss);
        
    ts = tss[0];
}

Regs ScalarConversionValue::precompile(Regs preferred) {
    Regs clob = orig->precompile(preferred);
    return clob | RAX;
}

Storage ScalarConversionValue::compile(X64 *x64) {
    orig->compile(x64);
    
    Storage t;
    
    for (int i = tss.size() - 1; i >= 0; i--) {
        TypeSpec ts = tss[i];
        StorageWhere where = ts.where(AS_ARGUMENT);
        where = (where == MEMORY ? STACK : where == NOWHERE ? NOWHERE : throw INTERNAL_ERROR);
        Storage s(where);
        
        if (i > 0) {
            std::cerr << "Discarding multi member " << i << " from " << s << ".\n";
            ts.store(s, Storage(), x64);
        }
        else {
            std::cerr << "Keeping multi member " << i << " as " << s << ".\n";
            t = s;
        }
    }

    return t;
}
