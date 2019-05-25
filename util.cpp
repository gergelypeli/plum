#include "plum.h"


unsigned stack_size(unsigned size) {
    return (size + 7) & ~7;
}


unsigned elem_size(unsigned size) {
    return size <= 2 ? size : size <= 4 ? 4 : size <= 8 ? 8 : stack_size(size);
}


// Allocation

Allocation::Allocation(int b, int c1, int c2, int c3) {
    bytes = b;
    count1 = c1;
    count2 = c2;
    count3 = c3;
}


int Allocation::concretize() {
    if (count1 || count2 || count3)
        throw INTERNAL_ERROR;
    else
        return bytes;
}

Allocation Allocation::stack_size() {
    int stack_bytes = (bytes >= 0 ? ::stack_size(bytes) : -::stack_size(-bytes));
    return { stack_bytes, count1, count2, count3 };
}


Allocation operator+(Allocation a, Allocation b) {
    return Allocation(a.bytes + b.bytes, a.count1 + b.count1, a.count2 + b.count2, a.count3 + b.count3);
}


Allocation operator*(Allocation a, int c) {
    return Allocation(a.bytes * c, a.count1 * c, a.count2 * c, a.count3 * c);
}


std::ostream &operator<<(std::ostream &os, const Allocation &a) {
    if (a.count1 || a.count2 || a.count3)
        os << "A(" << a.bytes << "," << a.count1 << "," << a.count2 << "," << a.count3 << ")";
    else
        os << "A(" << a.bytes << ")";
        
    return os;
}



PartialInfo::PartialInfo() {
}

void PartialInfo::set_member_names(std::vector<std::string> mn) {
    uninitialized_member_names.insert(mn.begin(), mn.end());
}

void PartialInfo::be_initialized(std::string name) {
    initialized_member_names.insert(name);
    uninitialized_member_names.erase(name);
}

bool PartialInfo::is_initialized(std::string name) {
    return initialized_member_names.count(name) == 1;
}

bool PartialInfo::is_uninitialized(std::string name) {
    return uninitialized_member_names.count(name) == 1;
}

bool PartialInfo::is_complete() {
    return uninitialized_member_names.size() == 0;
}

void PartialInfo::be_complete() {
    initialized_member_names.insert(uninitialized_member_names.begin(), uninitialized_member_names.end());
    uninitialized_member_names.clear();
}

bool PartialInfo::is_dirty() {
    return initialized_member_names.size() != 0;
}


// From https://stackoverflow.com/questions/874134/find-if-string-ends-with-another-string-in-c
bool desuffix(std::string &value, std::string const &ending)
{
    if (ending.size() > value.size())
        return false;
        
    if (!std::equal(ending.rbegin(), ending.rend(), value.rbegin()))
        return false;
        
    value = value.substr(0, value.size() - ending.size());
    return true;
}


bool deprefix(std::string &value, std::string const &beginning)
{
    if (beginning.size() > value.size())
        return false;
        
    if (!std::equal(beginning.begin(), beginning.end(), value.begin()))
        return false;
        
    value = value.substr(beginning.size());
    //std::cerr << "Deprefixed " << beginning << " into " << value << "\n";
    return true;
}


bool has_prefix(std::string &value, std::string const &beginning)
{
    if (beginning.size() > value.size())
        return false;
        
    if (!std::equal(beginning.begin(), beginning.end(), value.begin()))
        return false;
        
    return true;
}


std::string unqualify(std::string name) {
    auto i = name.rfind('.');
        
    if (i != std::string::npos)
        return name.substr(i + 1);
    else
        return name;
}


std::string get_working_path() {
   char temp[4096];  // No fucking standard, also see http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
   return ( getcwd(temp, 4096) ? std::string( temp ) : std::string("") );
}


std::ostream &operator<<(std::ostream &os, const std::ustring &x) {
    int charlen = x.size();
    char bytes[charlen * 3];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(x.data(), charlen, bytes, charlen * 3, &character_count, &byte_count);

    os.write(bytes, byte_count);
    return os;
}


std::vector<std::ustring> brace_split(std::ustring s) {
    std::vector<std::ustring> fragments;
    unsigned p = 0;
    bool inside = false;
    
    for (unsigned i = 0; i < s.size(); i++) {
        if (s[i] != (inside ? '}' : '{'))
            continue;
            
        fragments.push_back(s.substr(p, i - p));
        p = i + 1;
        inside = !inside;
    }
    
    fragments.push_back(s.substr(p, s.size() - p));
    
    return fragments;
}


bool is_unary(OperationType o) {
    return o == COMPLEMENT || o == NEGATE || o == GENERIC_UNARY;
}

bool is_comparison(OperationType o) {
    return o >= EQUAL && o <= COMPARE;
}

bool is_assignment(OperationType o) {
    return o >= ASSIGN;
}


// These must be uppercase for character interpolation
struct {
    std::string name;
    int code;
} character_codes[] = {
    { "NUL", 0 },
    { "SOH", 1 },
    { "STX", 2 },
    { "ETX", 3 },
    { "EOT", 4 },
    { "ENQ", 5 },
    { "ACK", 6 },
    { "BEL", 7 },
    { "BS", 8 },
    { "HT", 9 },
    { "LF", 10 },
    { "VT", 11 },
    { "FF", 12 },
    { "CR", 13 },
    { "SO", 14 },
    { "SI", 15 },
    { "DLE", 16 },
    { "DC1", 17 },
    { "DC2", 18 },
    { "DC3", 19 },
    { "DC4", 20 },
    { "NAK", 21 },
    { "SYN", 22 },
    { "ETB", 23 },
    { "CAN", 24 },
    { "EM", 25 },
    { "SUB", 26 },
    { "ESC", 27 },
    { "FS", 28 },
    { "GS", 29 },
    { "RS", 30 },
    { "US", 31 },

    { "QU", 34 },
    { "LB", 123 },
    { "RB", 125 },
    { "DEL", 127 },
};


int character_code(std::string name) {
    for (unsigned i = 0; i < sizeof(character_codes) / sizeof(character_codes[0]); i++)
        if (character_codes[i].name == name)
            return character_codes[i].code;
            
    return -1;
}


std::string character_name(int code) {
    for (unsigned i = 0; i < sizeof(character_codes) / sizeof(character_codes[0]); i++)
        if (character_codes[i].code == code)
            return character_codes[i].name;
        
    return "";    
}


int uni_code(std::string name) {
    if (name.size() != 5)
        return -1;
        
    if (name[0] != 'U')
        return -1;
        
    int code = 0;
    
    for (char c : { name[1], name[2], name[3], name[4] }) {
        c = tolower(c);
        int x = (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1);
        
        if (x < 0)
            return -1;
        
        code = code * 16 + x;
    }
    
    return code;
}


std::ustring decode_utf8(std::string text) {
    int bytelen = text.size();
    unsigned16 characters[bytelen];
    
    int64 character_count, byte_count;
    decode_utf8_buffer(text.data(), bytelen, characters, bytelen, &byte_count, &character_count);
    
    return std::ustring(characters, character_count);
}


std::string encode_utf8(std::ustring text) {
    int charlen = text.size();
    char bytes[charlen * 3];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(text.data(), charlen, bytes, charlen * 3, &character_count, &byte_count);
    
    return std::string(bytes, byte_count);
}


std::string encode_ascii(std::ustring text) {
    std::string out;
    
    for (unsigned16 c : text) {
        if (c < 32 || c > 126)
            return std::string();
        
        out.push_back(c);
    }
    
    return out;
}


std::vector<std::ustring> interpolate_characters(std::vector<std::ustring> in) {
    std::vector<std::ustring> out;
    bool is_literal = true;
    
    for (auto &f : in) {
        if (is_literal) {
            if (out.size() % 2 == 0) {
                // Literal fragments are at even indexes only
                out.push_back(std::ustring());
            }
            
            // May append to existing literal fragment
            for (auto c : f)
                out.back().push_back(c);
        }
        else {
            if (f.size() > 0 && isupper(f[0])) {
                // NOTE: currently all lookup returns an ASCII result
                std::string kw = encode_ascii(f);
                
                if (kw.empty()) {
                    std::cerr << "Interpolation keyword not ASCII: " << f << "!\n";
                    throw TYPE_ERROR;
                }
                
                int c = character_code(kw);
                
                if (c < 0)
                    c = uni_code(kw);
                
                if (c < 0) {
                    std::cerr << "Unknown interpolated character " << f << "!\n";
                    throw TYPE_ERROR;
                }
                
                // Append to previous literal fragment
                out.back().push_back(c);
            }
            else {
                // Create identifier fragment
                out.push_back(f);
            }
        }
        
        is_literal = !is_literal;
    }

    return out;
}
