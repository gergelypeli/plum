class RaisingDummy: public Declaration {
public:
    Unwound unwound;

    RaisingDummy(Unwound u);

    virtual bool may_omit_finalization();
    virtual void set_outer_scope(Scope *os);
};

class Autoconvertible {
public:
    virtual Label get_autoconv_table_label(TypeMatch tm, X64 *x64);
    virtual std::vector<AutoconvEntry> get_autoconv_table(TypeMatch tm);
};

class Methodlike {
public:
    virtual Label get_method_label(X64 *x64);
    virtual std::string get_method_name();
};

class PartialInitializable {
public:
    virtual std::vector<std::string> get_partial_initializable_names();
};

class VirtualEntry {
public:
    virtual void compile(TypeMatch tm, X64 *x64);
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
    virtual ~VirtualEntry();
};

class AutoconvVirtualEntry: public VirtualEntry {
public:
    Autoconvertible *autoconvertible;

    AutoconvVirtualEntry(Autoconvertible *a);

    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
};

class FfwdVirtualEntry: public VirtualEntry {
public:
    Allocation offset;

    FfwdVirtualEntry(Allocation o);

    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
};

class MethodVirtualEntry: public VirtualEntry {
public:
    Methodlike *method;

    MethodVirtualEntry(Methodlike *m);

    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
};

class PatchMethodVirtualEntry: public VirtualEntry {
public:
    Label trampoline_label;
    Methodlike *method;
    int offset;

    PatchMethodVirtualEntry(Methodlike *m, int o);

    virtual void compile(TypeMatch tm, X64 *x64);
    virtual Label get_virtual_entry_label(TypeMatch tm, X64 *x64);
    virtual std::ostream &out_virtual_entry(std::ostream &os, TypeMatch tm);
};
