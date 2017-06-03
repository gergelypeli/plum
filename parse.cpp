#include <fstream>
#include <vector>
#include <memory>
#include <sstream>

#include <util.h>


// Stage 1

bool is_paren(char c) {
    return (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
}


bool is_separator(char c) {
    return (c == ',');
}


bool is_quote(char c) {
    return (c == '\'' || c == '"');
}


std::vector<std::string> tokenize(std::string buffer) {
    std::vector<std::string> tokens;
    int i = 0;
    int indent = -1;
    
    while (buffer[i] != '\0') {
        char c = buffer[i];
        int start = i;
        
        if (c == '\n') {
            int start = i;
            
            do {
                i++;
                c = buffer[i];
            } while (c == ' ');
            
            if (c == '\n' || c == '#')
                continue;
                
            int n = i - start - 1;
            int ind = n / 4;
            int mod = n % 4;
            
            if (ind == indent && mod == 0) {
                tokens.push_back(" separate");  // May be removed by subsequent label
                continue;
            }
            else if (ind == indent && mod == 1) {
                continue;
            }
            else if (ind == indent + 1 && mod == 0) {
                tokens.push_back(" indent");
                indent++;
                continue;
            }
            else if (ind < indent && mod == 0) {
                for (int j=0; j < indent - ind; j++) {
                    tokens.push_back(" dedent");
                    indent--;
                }
                
                tokens.push_back(" separate");  // May be removed by subsequent label
                continue;
            }

            throw Error("Invalid indentation of %d spaces!", n);
        }
        else if (c == ' ') {
            i++;
            continue;
        }
        else if (c == '#') {
            do {
                i++;
                c = buffer[i];
            } while (c != '\n');
            
            continue;
        }
        else if (isdigit(c) || c == '.') {
            do {
                i++;
                c = buffer[i];
            } while (isdigit(c) || c == '_' || c == '.');
        }
        else if (isalpha(c) || c == '_' || c == ':') {
            bool prefixed = (c == ':');
            
            do {
                i++;
                c = buffer[i];
            } while (isalnum(c) || c == '_' || c == '.');
            
            if (!prefixed && (c == ':' || c == '?')) {
                if (c == ':' && tokens.back() == " separate")
                    tokens.pop_back();

                i++;
                c = buffer[i];
            }
        }
        else if (is_paren(c)) {
            i++;
        }
        else if (is_separator(c)) {
            i++;
        }
        else if (is_quote(c)) {
            char quote = c;
            
            do {
                if (c == '\\')
                    i++;
                    
                i++;
                c = buffer[i];
            } while (c != quote);
            
            i++;
        }
        else if (ispunct(c)) {
            do {
                i++;
                c = buffer[i];
            } while (ispunct(c) && !is_paren(c) && !is_separator(c) && !is_quote(c));
        }
        else {
            throw Error("Invalid input character %c!", c);
        }

        tokens.push_back(buffer.substr(start, i - start));
    }

    if (tokens.back() == " separate")
        tokens.pop_back();
    
    return tokens;
}


// Stage 2

enum Precedence {
    BASE,
    SEPARATING, // comma must end statements: :if x then: y else: z, ...
    LABELING,   // must be lower than DECLARING: each: i?
    ASSIGNING,  // must be lower than DECLARING: x? Int = 8
    DECLARING,
    LOGICAL_LOW,    // must be lower than COMPARING
    LOGICAL_HIGH,
    COMPARING,
    ADDITIVE,
    MULTIPLICATIVE,
    TEXTUAL,
    LITERAL
};


const char *print_precedence(Precedence p) {
    return (
        p == BASE ? "BASE" :
        p == SEPARATING ? "SEPARATING" :
        p == LABELING ? "LABELING" :
        p == ASSIGNING ? "ASSIGNING" :
        p == DECLARING ? "DECLARING" :
        p == LOGICAL_LOW ? "LOGICAL_LOW" :
        p == LOGICAL_HIGH ? "LOGICAL_HIGH" :
        p == COMPARING ? "COMPARING" :
        p == ADDITIVE ? "ADDITIVE" :
        p == MULTIPLICATIVE ? "MULTIPLICATIVE" :
        p == TEXTUAL ? "TEXTUAL" :
        p == LITERAL ? "LITERAL" :
        "???"
    );
}


bool is_right_associative(Precedence precedence) {
    return false;
}


enum OpType {
    OPEN, CLOSE,
    NUMBER, STRING, INITIALIZER,
    IDENTIFIER, LABEL, STATEMENT, DECLARATION,
    SEPARATOR
};


const char *print_op_type(OpType type) {
    return (
        type == OPEN ? "OPEN" :
        type == CLOSE ? "CLOSE" :
        type == NUMBER ? "NUMBER" :
        type == STRING ? "STRING" :
        type == INITIALIZER ? "INITIALIZER" :
        type == IDENTIFIER ? "IDENTIFIER" :
        type == LABEL ? "LABEL" :
        type == STATEMENT ? "STATEMENT" :
        type == DECLARATION ? "DECLARATION" :
        type == SEPARATOR ? "SEPARATOR" :
        "???"
    );
}


class Op {
public:
    OpType type;
    std::string text;
    Precedence back, fore;
    int left;
    int right;
    
    Op(OpType type, const std::string &text, Precedence back, Precedence fore) {
        this->type = type;
        this->text = text;
        this->back = back;
        this->fore = fore;
        left = -1;
        right = -1;
    }
};


enum Paren {
    PAREN, BRACKET, BRACE, BLOCK, UNIT
};


struct {
    const char *token;
    const char *text;
    Precedence precedence;
} operators[] = {
    { "+", "plus", ADDITIVE },
    { "-", "minus", ADDITIVE },
    { "*", "star", MULTIPLICATIVE },
    { "/", "slash", MULTIPLICATIVE },
    { "%", "percent", MULTIPLICATIVE },
    { "=", "assign", ASSIGNING },
    { "+=", "plus_assign", ASSIGNING },
    { "-=", "minus_assign", ASSIGNING },
    { "*=", "star_assign", ASSIGNING },
    { "/=", "slash_assign", ASSIGNING },
    { "%=", "percent_assign", ASSIGNING },
    { "==", "equal", COMPARING },
    { "!=", "not_equal", COMPARING },
    { "<", "less", COMPARING },
    { ">", "greater", COMPARING },
    { "<=", "less_equal", COMPARING },
    { ">=", "greater_equal", COMPARING },
    { "<>", "incomparable", COMPARING },
    { "!", "not", LOGICAL_HIGH },
    { "&", "and", LOGICAL_HIGH },
    { "|", "or", LOGICAL_LOW },
    { "^", "xor", LOGICAL_LOW }
    //{ "", "",  },
};


void print_op_node(std::vector<Op> &ops, int i, int indent, const char *prefix) {
    if (ops[i].left >= 0)
        print_op_node(ops, ops[i].left, indent + 2, "/ ");
    
    //std::cout << i << "(" << ops[i].left << "/" << ops[i].right << ")\n";
    for (int j=0; j<indent; j++)
        std::cout << " ";
        
    std::cout << prefix << "[" << print_op_type(ops[i].type) << "] " << ops[i].text << "\n";
    
    if (ops[i].right >= 0)
        print_op_node(ops, ops[i].right, indent + 2, "\\ ");
}


std::vector<Op> operate(std::vector<std::string> tokens) {
    std::vector<Op> ops;
    std::vector<Paren> parens;
    
    //ops.push_back(Op(OPEN, "", BASE, BASE));
    //parens.push_back(UNIT);
    if (tokens.front() != " indent")
        throw Error("Oops?");
    
    for (auto token : tokens) {
        OpType type;
        Precedence back, fore;
        std::string text;
        
        char c = token[0];
        
        if (isdigit(c) || c == '.') {
            type = NUMBER;
            back = fore = LITERAL;
            text = token;
        }
        else if (c == ':') {
            type = STATEMENT;
            back = LITERAL;
            fore = SEPARATING;
            text = token.substr(1);
        }
        else if (isalpha(c) || c == '_') {
            if (token.back() == ':') {
                type = LABEL;
                back = LABELING;
                fore = LABELING;
                text = token.substr(0, token.length() - 1);
            }
            else if (token.back() == '?') {
                type = DECLARATION;
                back = DECLARING;
                fore = DECLARING;
                text = token.substr(0, token.length() - 1);
            }
            else {
                type = IDENTIFIER;
                back = TEXTUAL;
                fore = TEXTUAL;
                text = token;
            }
        }
        else if (is_quote(c)) {
            type = STRING;
            back = LITERAL;
            fore = LITERAL;
            text = token.substr(1, token.length() - 1);
        }
        else if (c == ',') {
            type = SEPARATOR;
            back = SEPARATING;
            fore = SEPARATING;
        }
        else if (is_paren(c)) {
            if (c == '(') {
                parens.push_back(PAREN);
                
                type = OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (c == ')') {
                if (parens.back() != PAREN)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '[') {
                parens.push_back(BRACKET);
                
                type = IDENTIFIER;
                back = TEXTUAL;
                fore = BASE;
                text = "index";
            }
            else if (c == ']') {
                if (parens.back() != BRACKET)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (c == '{') {
                parens.push_back(BRACE);
                
                type = INITIALIZER;
                back = LITERAL;
                fore = BASE;
            }
            else if (c == '}') {
                if (parens.back() != BRACE)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
        }
        else if (c == ' ') {
            if (token == " indent") {
                parens.push_back(BLOCK);
                
                type = OPEN;
                back = LITERAL;
                fore = BASE;
            }
            else if (token == " dedent") {
                if (parens.back() != BLOCK)
                    throw Error("Mismatched %s!", token.c_str());
                    
                parens.pop_back();
                
                type = CLOSE;
                back = BASE;
                fore = LITERAL;
            }
            else if (token == " separate") {
                if (parens.back() != BLOCK && parens.back() != UNIT)
                    throw Error("Unclosed parentheses %s!", token.c_str());
                
                type = SEPARATOR;
                back = SEPARATING;
                fore = SEPARATING;
            }
            else
                throw Error("Invalid paren thingy %s!", token.c_str());
        }
        else {
            for (auto op : operators) {
                if (op.token == token) {
                    type = IDENTIFIER;
                    back = op.precedence;
                    fore = op.precedence;
                    text = op.text;
                    break;
                }
            }
            
            if (!type)
                throw Error("No operator %s!", token.c_str());
        }
        
        int n = ops.size();
        std::cout << "Token " << token << " => " << n << "\n";
        
        ops.push_back(Op(type, text, back, fore));
        int r = -1;
        
        for (int i = n - 1; i >= 0; i--) {
            if (ops[i].right != r)
                continue;
                
            r = i;

            if (ops[i].fore > back)  // || (ops[i].precedence == back && !is_right_associative(back)))
                continue;
            else if (ops[i].fore == back) {
                if (back == LITERAL)
                    throw Error("Literals can't follow each other!");
                else if (back == BASE)
                    ops[i].fore = ops[i].back;
                else
                    continue;
            }

            // ops[i] will be our parent, and we'll be its right child
            ops[n].left = ops[i].right;
            ops[i].right = n;
            break;
        }
    }
    
    if (parens.size() != 1)
        throw Error("Parens remained at the end!");
        
    print_op_node(ops, 0, 0, "- ");
    
    return ops;
}


// Stage 3

class Type;
class Declaration;
typedef std::vector<Type *> TypeSpec;
//typedef TypeSpec TypeList;

Type *type_type = NULL;
Type *void_type = NULL;
Type *function_type = NULL;
Type *integer_type = NULL;

TypeSpec TS_VOID;
std::string print_typespec(TypeSpec &ts);


struct Match {
    Declaration *decl;
    std::vector<TypeSpec> params;
    
    Match() {
        decl = NULL;
    }

    Match(Declaration *d) {
        decl = d;
    }
    
    Match(Declaration *d, std::vector<TypeSpec> p) {
        decl = d;
        params = p;
    }
};


class Declaration {
public:
    virtual Match match(std::string name, TypeSpec ts) {
        return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        throw Error("No return type!");
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        throw Error("No argument types!");
    }
};


class Variable: public Declaration {
public:
    std::string name;
    TypeSpec ts;
    
    Variable(std::string name, TypeSpec ts) {
        this->name = name;
        this->ts = ts;
    }
    
    virtual Match match(std::string name, TypeSpec ts) {
        if (name == this->name && ts == TS_VOID)
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return TS_VOID;
    }
};


class Function: public Declaration {
public:
    std::string name;
    TypeSpec ts;
    TypeSpec pivot_ts;
    TypeSpec argument_ts;
    
    Function(std::string name, TypeSpec ts, TypeSpec pivot_ts, TypeSpec argument_ts) {
        this->name = name;
        this->ts = ts;
        this->pivot_ts = pivot_ts;
        this->argument_ts = argument_ts;
    }
    
    virtual Match match(std::string name, TypeSpec ts) {
        std::cout << "XXX Function.match " << name << " " << print_typespec(ts) << "\n";
        if (name == this->name && ts == pivot_ts)  // TODO: convert
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return argument_ts;
    }
};


class Scope: virtual public Declaration {
public:
    std::vector<std::unique_ptr<Declaration>> contents;
    Scope *outer;
    
    Scope(Scope *outer = NULL) {
        this->outer = outer;
    }
    
    virtual void add(Declaration *decl) {
        contents.push_back(std::unique_ptr<Declaration>(decl));
    }
    
    virtual void remove(Declaration *decl) {
        if (contents.back().get() == decl) {
            contents.back().release();
            contents.pop_back();
        }
        else
            throw Error("Not the last decl to remove!");
    }
    
    virtual unsigned get_length() {
        return contents.size();
    }
    
    virtual Match lookup(std::string name, TypeSpec ts) {
        for (auto &decl : contents) {
            Match m = decl->match(name, ts);
            
            if (m.decl)
                return m;
        }

        return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        //TypeSpec ts;
        //ts.push_back();
        return TS_VOID;  // TODO
    }
};


class FunctionScope: public Scope {
public:
    FunctionScope(Scope *o)
        :Scope(o) {
    }
    
    virtual Match lookup(std::string name, TypeSpec ts) {
        if (contents.size() > 1) {
            Scope *s = dynamic_cast<Scope *>(contents[0].get());
            return s->lookup(name, ts);  // Look up function arguments
        }
        else
            return Match();
    }
};


class Type: virtual public Declaration {
public:
    std::string name;
    unsigned parameter_count;
    
    Type(std::string n, unsigned pc) {
        name = n;
        parameter_count = pc;
    }
    
    virtual unsigned get_parameter_count() {
        return parameter_count;
    }
    
    virtual int get_length() {
        return 1;
    }
    
    virtual std::string get_label() {
        return "";
    }
    
    virtual Match match(std::string name, TypeSpec ts) {
        if (name == this->name && ts == TS_VOID && parameter_count == 0)
            return Match(this);
        else
            return Match();
    }
    
    virtual TypeSpec get_return_ts(Match m) {
        if (m.params.size() != parameter_count)
            throw Error("Mismatching type parameter count!");
            
        TypeSpec ts;
        ts.push_back(type_type);
        ts.push_back(this);
        
        for (auto &param : m.params)
            for (auto type : param)
                ts.push_back(type);
        
        return ts;
    }
    
    virtual TypeSpec get_argument_ts(Match m) {
        return TS_VOID;  // FIXME
    }
};


int step_typespec(TypeSpec &ts, int i) {
    Type *t = ts[i];
    i++;
    
    for (unsigned j = 0; j < t->get_parameter_count(); j++)
        i = step_typespec(ts, i);
        
    return i;
}


std::string print_typespec(TypeSpec &ts) {
    std::stringstream s;
    s << "[";
    
    bool start = true;
    
    for (auto type : ts) {
        if (start)
            start = false;
        else
            s << ",";
            
        s << type->name;
    }
    
    s << "]";
    
    return s.str();
}


// FIXME: this is not this simple. For method calls and statements it's the first
// parameter, but for types it's all parameters.
TypeSpec get_pivot_typespec(TypeSpec &ts) {
    if (ts[0]->get_length() == 1)
        return ts;
        
    int start = 1;
    int end = step_typespec(ts, start);
    
    TypeSpec t;
    
    for (int i = start; i < end; i++)
        t.push_back(ts[i]);
        
    return t;
}


class LabelType: public Type {
public:
    std::string label;
    
    LabelType(std::string l)
        :Type("<Label>", 1) {
        label = l;
    }
    
    virtual std::string get_label() {
        return label;
    }
};


Type *make_label_type(std::string label) {
    return new LabelType(label);  // FIXME: cache!
}


class TupleType: public Type {
public:
    TupleType(int pc)
        :Type("<Tuple>", pc) {
        if (pc < 2)
            throw Error("A tuple type needs at least 2 parameters!");
    }
    
    virtual int get_length() {
        return parameter_count;
    }
};


Type *make_tuple_type(int pc) {
    return new TupleType(pc);  // FIXME: cache!
}


TypeSpec make_tuple_typespec(TypeSpec a, TypeSpec b) {
    if (a == TS_VOID)
        return b;
        
    if (b == TS_VOID)
        return a;
    
    int a_length = a[0]->get_length();
    int b_length = b[0]->get_length();
    
    TypeSpec ts;
    Type *t = make_tuple_type(a_length + b_length);
    ts.push_back(t);
    
    for (unsigned i = (a_length >= 2 ? 1 : 0); i < a.size(); i++)
        ts.push_back(a[i]);
        
    for (unsigned i = (b_length >= 2 ? 1 : 0); i < b.size(); i++)
        ts.push_back(b[i]);
    
    return ts;
}


class Expr {
public:
    Match match;
    TypeSpec ts;
    std::unique_ptr<Expr> left, right;
    Expr *parent;
    
    Expr(TypeSpec t, Expr *l, Expr *r)
        :left(l), right(r) {
        ts = t;
        
        if (l)
            l->parent = this;
            
        if (r)
            r->parent = this;
    }

    Expr(Match m, Expr *l, Expr *r)
        :left(l), right(r) {
        match = m;
        ts = m.decl->get_return_ts(m);

        if (l)
            l->parent = this;
            
        if (r)
            r->parent = this;
    }

};


Expr *resolve(std::vector<Op> ops, int i, Scope *scope) {
    if (i < 0)
        throw Error("Eiii!");
        
    Op &op = ops[i];
    
    if (op.type == OPEN) {
        Scope *inner = new Scope(scope);
        scope->add(inner);
        
        Expr *r = resolve(ops, op.right, inner);
        
        if (inner->get_length() == 0) {
            scope->remove(inner);
            return r;
        }
        else {
            //TypeSpec ts;
            //ts.push_back(inner);
            Match m(inner);
            
            Expr *e = new Expr(m, NULL, r);
            return e;
        }
    }
    else if (op.type == CLOSE) {
        return resolve(ops, op.left, scope);
    }
    else if (op.type == SEPARATOR) {
        Expr *l = resolve(ops, op.left, scope);
        Expr *r = resolve(ops, op.right, scope);
        Expr *e = new Expr(TS_VOID, l, r);  // TODO
        return e;
    }
    else if (op.type == STATEMENT) {
        if (op.text == "function") {
            Scope *inner = new FunctionScope(scope);
            scope->add(inner);

            Expr *r = resolve(ops, op.right, inner);

            //Expr *from = r->left->right;
            //Scope *args = dynamic_cast<Scope *>(from->match.decl);
            //std::cout << "XXX: " << print_typespec(r->ts) << "\n";
            TypeSpec ret_ts;
            
            if (r->left && r->left->left)
                ret_ts = r->left->left->ts;
            else {
                ret_ts.push_back(type_type);
                ret_ts.push_back(void_type);
            }
            
            std::cout << "Function ret_ts " << print_typespec(ret_ts) << "\n";
            //TypeSpec ts;
            
            //for (unsigned i = 1; i < ret->ts.size(); i++)
            //    ts.push_back(ret->ts[i]);
                
            Expr *e = new Expr(ret_ts, NULL, r);  // FIXME, a lot
            return e;
        }
        else {
            Expr *r = resolve(ops, op.right, scope);
            Expr *e = new Expr(TS_VOID, NULL, r);  // TODO
            return e;
        }
    }
    else if (op.type == DECLARATION) {
        Expr *r = resolve(ops, op.right, scope);
        
        if (r->ts.size() == 0)
            throw Error("Declaration needs a type!");

        Declaration *decl;

        if (r->ts[0] == type_type) {
            TypeSpec ts;
            for (unsigned i = 1; i < r->ts.size(); i++)
                ts.push_back(r->ts[i]);
        
            decl = new Variable(op.text, ts);
        }
        else if (r->ts[0] == function_type) {
            TypeSpec ts;
            for (unsigned i = 1; i < r->ts.size(); i++)
                ts.push_back(r->ts[i]);

            decl = new Function(op.text, ts, TS_VOID, TS_VOID);
        }
            
        scope->add(decl);
    
        Expr *e = new Expr(TS_VOID, NULL, r);  // TODO
        return e;
    }
    else if (op.type == LABEL) {
        // TODO: check if already labeled!
        Expr *l = op.left >= 0 ? resolve(ops, op.left, scope) : NULL;
        Expr *r = resolve(ops, op.right, scope);
        Type *lt = make_label_type(op.text);
        
        TypeSpec ts(r->ts);
        ts.insert(ts.begin(), lt);
        
        if (l)
            ts = make_tuple_typespec(l->ts, ts);
        
        Expr *e = new Expr(ts, l, r);  // TODO
        return e;
    }
    else if (op.type == IDENTIFIER) {
        Expr *l = op.left >= 0 ? resolve(ops, op.left, scope) : NULL;
        Expr *r = op.right >= 0 ? resolve(ops, op.right, scope) : NULL;
        
        TypeSpec lts = l ? l->ts : TS_VOID;
        TypeSpec rts = r ? r->ts : TS_VOID;
        TypeSpec ts = make_tuple_typespec(lts, rts);
        TypeSpec pts = get_pivot_typespec(ts);

        std::cout << "Looking up " << op.text << " with pivot type " << print_typespec(pts) << "\n";

        for (Scope *s = scope; s; s = s->outer) {
            Match m = s->lookup(op.text, pts);
        
            if (m.decl)
                return new Expr(m, l, r);
        }
        
        throw Error("No match for %s!", op.text.c_str());
    }
    else if (op.type == NUMBER) {
        TypeSpec ts;
        ts.push_back(integer_type);
        
        return new Expr(ts, NULL, NULL);  // TODO
    }
    else
        throw Error("Can't resolve this now %d!", op.type);
}


std::string read_source(const char *filename) {
    std::ifstream source(filename, std::ios::binary);
    
    if (!source.is_open())
        return "";
    
    std::string buffer((std::istreambuf_iterator<char>(source)),
                       (std::istreambuf_iterator<char>()));
                       
    buffer.push_back('\0');
    
    return buffer;
}


int main(int argc, char **argv) {
    try {
        std::string buffer = read_source(argv[1]);
        std::vector<std::string> tokens = tokenize(buffer);
        std::vector<Op> ops = operate(tokens);

        Scope *root_scope = new Scope();
        
        type_type = new Type("<Type>", 1);
        root_scope->add(type_type);

        function_type = new Type("<Function>", 1);
        root_scope->add(function_type);
        
        void_type = new Type("Void", 0);
        root_scope->add(void_type);
        TS_VOID.push_back(void_type);

        integer_type = new Type("Integer", 0);
        root_scope->add(integer_type);
        
        TypeSpec int_ts;
        int_ts.push_back(integer_type);
        Declaration *integer_add = new Function("plus", int_ts, int_ts, int_ts);
        root_scope->add(integer_add);
        
        Declaration *integer_print = new Function("print", TS_VOID, int_ts, TS_VOID);  // FIXME
        root_scope->add(integer_print);
        
        std::unique_ptr<Expr> root(resolve(ops, 0, root_scope));
        
        //for (auto token : tokens)
        //    std::cout << "" << token << "\n";
    }
    catch (Error &e) {
        std::cerr << e << "\n";
        throw;
    }
    
    return 0;
}
