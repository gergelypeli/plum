#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

#include "typedefs.h"
#include "text.h"
#include "heap.h"
#include "fpconv/fpconv.h"

#define ALENGTH(x) *(int64 *)((x) + LINEARRAY_LENGTH_OFFSET)
#define ARESERVATION(x) *(int64 *)((x) + LINEARRAY_RESERVATION_OFFSET)
#define AELEMENTS(x) ((x) + LINEARRAY_ELEMS_OFFSET)

#define HREFCOUNT(x) *(int64 *)((x) + HEAP_REFCOUNT_OFFSET)
#define HFINALIZER(x) *(int64 *)((x) + HEAP_FINALIZER_OFFSET)
#define HNEXT(x) *(int64 *)((x) + HEAP_NEXT_OFFSET)

#define RECORDMEMBER(obj, mtype) *(mtype *)(obj)
#define CLASSMEMBER(obj, mtype) *(mtype *)(obj + CLASS_MEMBERS_OFFSET)

#define SELEMENTS(x, s) (AELEMENTS((x)->ptr) + (s) * (x)->front)
#define SLENGTH(x) ((x)->length)

typedef void *Ref;

typedef struct {
    void *ptr;
    int64 front;
    int64 length;
} *Slice;

typedef struct {
    int64 exception;  // RAX
} Maybe;

typedef struct {
    int64 exception;  // RAX
    int64 value;      // RDX
} MaybeInteger;

typedef struct {
    int64 exception;  // RAX
    double value;     // XMM0
} MaybeFloat;


#define EXCNO(e) ((e) + ERRNO_TREENUM_OFFSET)

#define YES() ((Maybe) { NO_EXCEPTION })
#define NOT(x) ((Maybe) { (x) })

#define YES_INTEGER(x) ((MaybeInteger) { NO_EXCEPTION, (int64)(x) })
#define NOT_INTEGER(x) ((MaybeInteger) { (x), (int64)0 })

#define YES_FLOAT(x) ((MaybeFloat) { NO_EXCEPTION, (double)(x) })
#define NOT_FLOAT(x) ((MaybeFloat) { (x), (double)0.0 })


extern void empty_function();
extern void finalize_reference_array();
extern int64 refcount_balance;

static int allocation_count = 0;


// Memory management

void *C__malloc(int64 size) {
    allocation_count += 1;
    void *x = malloc(size);
    //fprintf(stderr, " -- malloc %p %lld\n", x, size);
    return x;
}


void *C__aligned_alloc(int64 alignment, int64 size) {
    allocation_count += 1;
    void *x = aligned_alloc(alignment, size);
    //fprintf(stderr, " -- aligned_alloc %p %lld\n", x, size);
    return x;
}


void C__free(void *m) {
    allocation_count -= 1;
    //fprintf(stderr, " -- free %p\n", m);
    free(m);
}


void *C__realloc(void *m, int64 size) {
    void *x = realloc(m, size);
    //fprintf(stderr, " -- realloc %p %lld %p\n", m, size, x);
    return x;
}


int C__mprotect(void *m, int64 size, int64 prot) {
    int x = mprotect(m, size, prot);
    //fprintf(stderr, " -- mprotect %p %lld %p\n", m, size, x);
    return x;
}


void *C__memcpy(void *dst, void *src, size_t n) {
    //fprintf(stderr, " -- memcpy %p %p %ld\n", dst, src, n);
    return memcpy(dst, src, n);
}


void *C__memmove(void *dst, void *src, size_t n) {
    //fprintf(stderr, " -- memmove %p %p %ld\n", dst, src, n);
    return memmove(dst, src, n);
}


// Internal helpers

void *allocate_basic_array(int64 length, int64 size) {
    void *array = C__malloc(HEAP_HEADER_SIZE + LINEARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    
    HNEXT(array) = 0;
    HREFCOUNT(array) = 1;
    //HWEAKREFCOUNT(array) = 0;
    HFINALIZER(array) = (int64)(void *)empty_function;  // TODO: this only works for basic types!
    
    ARESERVATION(array) = length;
    ALENGTH(array) = 0;
    
    refcount_balance += 1;
    
    return array;
}


void *allocate_string_array(int64 length) {
    int size = ADDRESS_SIZE;
    void *array = C__malloc(HEAP_HEADER_SIZE + LINEARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    
    HNEXT(array) = 0;
    HREFCOUNT(array) = 1;
    //HWEAKREFCOUNT(array) = 0;
    HFINALIZER(array) = (int64)(void *)finalize_reference_array;
    
    ARESERVATION(array) = length;
    ALENGTH(array) = 0;

    refcount_balance += 1;
    
    return array;
}


void *reallocate_array(void *array, int64 length, int64 size) {
    if (HREFCOUNT(array) != 1) {
        fprintf(stderr, "Oops, reallocating an array with %lld references!\n", HREFCOUNT(array));
        abort();
    }

    if (ALENGTH(array) > length) {
        fprintf(stderr, "Oops, reallocating an array with too large length!\n");
        abort();
    }

    array = C__realloc(array + HEAP_HEADER_OFFSET, HEAP_HEADER_SIZE + LINEARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    ARESERVATION(array) = length;
    return array;
}


void free_basic_array(void *array) {
    C__free(array + HEAP_HEADER_OFFSET);
}


void *preappend(void *character_array, int64 append_length) {
    int64 character_length = ALENGTH(character_array);
    int64 character_reserve = ARESERVATION(character_array);
    
    if (character_reserve - character_length < append_length)
        character_array = reallocate_array(character_array, character_length + append_length, 2);

    return character_array;
}


void *append_decode_utf8(void *character_array, char *bytes, int64 byte_length) {
    character_array = preappend(character_array, byte_length);  // upper limit
        
    unsigned16 *characters = AELEMENTS(character_array);
    int64 character_length = ALENGTH(character_array);
    int64 available_length = ARESERVATION(character_array) - character_length;
    
    int64 byte_count, character_count;
    decode_utf8_buffer(bytes, byte_length, characters + character_length, available_length, &byte_count, &character_count);
    ALENGTH(character_array) += character_count;
    
    return character_array;
}


void lvalue_append_decode_utf8(void **character_array_lvalue, char *byte_array, int64 byte_length) {
    void *character_array = *character_array_lvalue;
    character_array = append_decode_utf8(character_array, byte_array, byte_length);
    *character_array_lvalue = character_array;
}


// Exported helpers


void C__log(const char *message) {
    fprintf(stderr, "LOG: %s\n", message);
}


void C__logref(const char *message, unsigned64 ptr) {
    fprintf(stderr, "LOGREF: %s %016llx %lld\n", message, ptr, HREFCOUNT(ptr));
}


struct R { unsigned64 r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rsp, rdx, rcx, rbx, rax, rflags; };

void C__dump(const char *message, struct R *r) {
    fprintf(stderr, "DUMP: %s [%c%c%c%c%c%c]\n", message,
        (r->rflags & 1 ?    'C' : 'c'),
        (r->rflags & 4 ?    'P' : 'p'),
        (r->rflags & 16 ?   'A' : 'a'),
        (r->rflags & 64 ?   'Z' : 'z'),
        (r->rflags & 128 ?  'S' : 's'),
        (r->rflags & 2048 ? 'O' : 'o')
    );
    fprintf(stderr, "              ____    ____          ____    ____          ____    ____          ____    ____\n");
    fprintf(stderr, "      RAX=%016llx  RBX=%016llx  RCX=%016llx  RDX=%016llx\n", r->rax, r->rbx, r->rcx, r->rdx);
    fprintf(stderr, "      RSP=%016llx  RBP=%016llx  RSI=%016llx  RDI=%016llx\n", r->rsp + 32, r->rbp, r->rsi, r->rdi);
    fprintf(stderr, "      R8 =%016llx  R9 =%016llx  R10=%016llx  R11=%016llx\n", r->r8, r->r9, r->r10, r->r11);
    fprintf(stderr, "      R12=%016llx  R13=%016llx  R14=%016llx  R15=%016llx\n", r->r12, r->r13, r->r14, r->r15);
}


void C__die(const char *message) {
    fprintf(stderr, "Can't go on like this... %s\n", message);
    abort();
}


void C__dies(void *s) {
    int64 character_length = ALENGTH(s);
    char bytes[character_length * 3 + 1];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(s), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);

    bytes[byte_count] = '\0';
    fprintf(stderr, "DIE: %.*s\n", (int)byte_count, bytes);
    abort();
}


void C__die_uncaught(Ref name, int64 row) {
    int64 character_length = ALENGTH(name);
    char bytes[character_length * 3 + 1];
        
    int64 character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(name), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);
    bytes[byte_count] = '\0';
    
    printf("Uncaught exception %s on line %lld!\n", bytes, row);
    abort();
}


void C__streamify_integer(int64 x, void **character_array_lvalue) {
    char byte_array[30];
    int64 byte_length = snprintf(byte_array, sizeof(byte_array), "%lld", x);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void C__streamify_unteger(unsigned64 x, void **character_array_lvalue) {
    char byte_array[30];
    int64 byte_length = snprintf(byte_array, sizeof(byte_array), "%llu", x);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void C__streamify_boolean(unsigned char x, void **character_array_lvalue) {
    char *byte_array = (x ? "`true" : "`false");
    int64 byte_length = strlen(byte_array);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void C__streamify_float(double x, void **character_array_lvalue) {
    char byte_array[30];
    int64 byte_length;
    
    if (isnan(x)) {
        strcpy(byte_array, "`nan");
        byte_length = 4;
    }
    else if (isinf(x)) {
        strcpy(byte_array, x > 0.0 ? "`pinf" : "`ninf");
        byte_length = 5;
    }
    else {
        // Use a round-trippable decimal representation which is also the shortest possible
        // in 99.8% of the cases (Grisu2 algorithm by night-shift)
        byte_length = fpconv_dtoa(x, byte_array);
    }
    
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void C__streamify_pointer(Ref x, void **character_array_lvalue) {
    char byte_array[30];
    int64 byte_length = snprintf(byte_array, sizeof(byte_array), "%p", x);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void C__sort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    qsort(base, nmemb, size, compar);
}


void *C__string_regexp_match(void *subject_array, void *pattern_array) {
    //fprintf(stderr, "PCRE2 match begins.\n");
    //prints(subject_array);
    //prints(pattern_array);
    
    PCRE2_SPTR subject = AELEMENTS(subject_array);
    PCRE2_SIZE subject_length = ALENGTH(subject_array);
    
    PCRE2_SPTR pattern = AELEMENTS(pattern_array);
    PCRE2_SIZE pattern_length = ALENGTH(pattern_array);
    
    int errornumber;
    PCRE2_SIZE erroroffset;
    
    pcre2_code *re = pcre2_compile(
      pattern,
      pattern_length,
      0,                     /* default options */
      &errornumber,          /* for error number */
      &erroroffset,          /* for error offset */
      NULL);                 /* use default compile context */
      
    if (re == NULL) {
        fprintf(stderr, "PCRE2 error compiling pattern!\n");
        return NULL;
    }

    //fprintf(stderr, "PCRE2 compiled.\n");
    
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
    
    int rc = pcre2_match(
      re,                   /* the compiled pattern */
      subject,              /* the subject string */
      subject_length,       /* the length of the subject */
      0,                    /* start at offset 0 in the subject */
      0,                    /* default options */
      match_data,           /* block for storing the result */
      NULL);                /* use default match context */

    void *result_array = NULL;
    
    if (rc > 0) {
        //fprintf(stderr, "PCRE2 matched with rc=%d.\n", rc);
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        
        result_array = allocate_string_array(rc);
        void **result_refs = AELEMENTS(result_array);
        ALENGTH(result_array) = rc;
        unsigned16 *subject_characters = AELEMENTS(subject_array);
        
        for (int i = 0; i < rc; i++) {
            size_t start = ovector[2 * i];
            size_t len = ovector[2 * i + 1] - ovector[2 * i];
            //fprintf(stderr, "PCRE2 match %d start %lld length %lld.\n", i, start, len);
            
            void *target_array = allocate_basic_array(len, CHARACTER_SIZE);
            ALENGTH(target_array) = len;
            unsigned16 *target_characters = AELEMENTS(target_array);
            
            for (unsigned j = 0; j < len; j++)
                target_characters[j] = subject_characters[start + j];
                
            result_refs[i] = target_array;
        }
    }
        
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);
    
    return result_array;
}


// Library functions

bool Character__is_alnum(unsigned16 c) { return isalnum(c); }
bool Character__is_alpha(unsigned16 c) { return isalpha(c); }
bool Character__is_ascii(unsigned16 c) { return isascii(c); }
bool Character__is_blank(unsigned16 c) { return isblank(c); }
bool Character__is_cntrl(unsigned16 c) { return iscntrl(c); }
bool Character__is_digit(unsigned16 c) { return isdigit(c); }
bool Character__is_lower(unsigned16 c) { return islower(c); }
bool Character__is_punct(unsigned16 c) { return ispunct(c); }
bool Character__is_space(unsigned16 c) { return isspace(c); }
bool Character__is_upper(unsigned16 c) { return isupper(c); }
bool Character__is_xdigit(unsigned16 c) { return isxdigit(c); }


void Std__printi(int64 a) {
    printf("%lld\n", a);
}


void Std__printc(unsigned16 a) {
    printf("%c\n", a);
}


void Std__printd(double a) {
    printf("%g\n", a);
}


void Std__prints(void *s) {
    if (s) {
        int64 character_length = ALENGTH(s);
        char bytes[character_length * 3 + 1];
        
        int64 character_count, byte_count;
        encode_utf8_buffer(AELEMENTS(s), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);

        fwrite(bytes, 1, byte_count, stdout);
        fwrite("\n", 1, 1, stdout);
    }
    else
        printf("(null)\n");
}


void Std__printb(void *s) {
    if (s) {
        int64 byte_count = ALENGTH(s);
        char *bytes = AELEMENTS(s);
        fwrite(bytes, 1, byte_count, stdout);
        fwrite("\n", 1, 1, stdout);
    }
    else
        printf("(null)\n");
}


void Std__printp(void **x) {
    // This function uses an Lvalue, so invalid pointers can be passed
    printf("%p\n", *x);
}


static unsigned16 chr(Ref character_array, int64 index) {
    return *(unsigned16 *)(AELEMENTS(character_array) + CHARACTER_SIZE * index);
}


void Std__parse_ws(Ref character_array, int64 *position_lvalue) {
    while (*position_lvalue < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, *position_lvalue);
        
        if (!isspace(c))
            return;
            
        *position_lvalue += 1;
    }
}


MaybeInteger Std__parse_identifier(Ref character_array, int64 *position_lvalue) {
    int64 position = *position_lvalue;
    int64 start = position;
    
    if (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);
        
        if (!isalpha(c) && c != '_')
            return NOT_INTEGER(1);  // PARSE_ERROR
            
        position += 1;
    }
    else
        return NOT_INTEGER(1);  // PARSE_ERROR

    while (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);
        
        if (!isalnum(c) && c != '_')
            break;
            
        position += 1;
    }
    
    int length = position - start;
    Ref result_array = allocate_basic_array(length, 2);
    ALENGTH(result_array) = length;
    unsigned16 *characters = AELEMENTS(result_array);
    
    for (int i = 0; i < length; i++)
        characters[i] = chr(character_array, start + i);

    //fprintf(stderr, "parsed identifier of %d\n", length);
    
    *position_lvalue = position;
    
    return YES_INTEGER(result_array);
}


MaybeInteger Std__parse_integer(Ref character_array, int64 *position_lvalue) {
    int64 position = *position_lvalue;
    bool is_negative = false;
    
    if (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);
        
        if (c == '+') {
            position += 1;
        }
        else if (c == '-') {
            is_negative = true;
            position += 1;
        }
    }

    unsigned64 result;
    int64 character_count;
    
    //fprintf(stderr, "XXX parsing integer\n");
    bool ok = parse_unteger(AELEMENTS(character_array) + CHARACTER_SIZE * position, ALENGTH(character_array) - position, &result, &character_count);

    if (!ok)
        return NOT_INTEGER(1);
        
    //fprintf(stderr, "XXX parsed integer of %lld\n", is_negative ? -result : result);
    *position_lvalue = position + character_count;
    
    return YES_INTEGER(is_negative ? -result : result);
}


MaybeFloat Std__parse_float(Ref character_array, int64 *position_lvalue) {
    int64 position = *position_lvalue;
    
    double result;
    int64 character_count;
    
    bool ok = parse_float(AELEMENTS(character_array) + CHARACTER_SIZE * position, ALENGTH(character_array) - position, &result, &character_count);

    if (!ok)
        return NOT_FLOAT(1);

    *position_lvalue = position + character_count;
    
    return YES_FLOAT(result);
}


static int xdigit(char c) {
    return
        c >= 'A' && c <= 'F' ? c - 'A' + 10 :
        c >= 'a' && c <= 'f' ? c - 'a' + 10 :
        c - '0';
}


MaybeInteger Std__parse_jstring(Ref character_array, int64 *position_lvalue) {
    int64 position = *position_lvalue;
    
    if (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);
        
        if (c == '"') {
            position += 1;
        }
        else
            return NOT_INTEGER(1);
    }
    else
        return NOT_INTEGER(1);

    // First pass: determine result length
    bool seen_close = false;
    int64 start = position;
    int64 length = 0;
    
    while (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);

        if (c == '"') {
            seen_close = true;
            break;
        }
        else if (c == '\\') {
            position += 1;
            
            if (position < ALENGTH(character_array)) {
                unsigned16 d = chr(character_array, position);
                
                switch (d) {
                case '"': case '/': case '\\': case 'b': case 'f': case 'n': case 'r': case 't':
                    position += 1;
                    length += 1;
                    break;
                case 'u':
                    if (position + 4 < ALENGTH(character_array)) {
                        if (
                            isxdigit(chr(character_array, position + 1)) &&
                            isxdigit(chr(character_array, position + 2)) &&
                            isxdigit(chr(character_array, position + 3)) &&
                            isxdigit(chr(character_array, position + 4))
                        ) {
                            position += 5;
                            length += 1;
                        }
                        else
                            return NOT_INTEGER(1);
                    }
                    else
                        return NOT_INTEGER(1);
                    break;
                default:
                    return NOT_INTEGER(1);
                }
            }
            else
                return NOT_INTEGER(1);
        }
        else {
            position += 1;
            length += 1;
        }
    }

    if (!seen_close)
        return NOT_INTEGER(1);

    // Second pass: do the conversion
    Ref result_array = allocate_basic_array(length, 2);
    ALENGTH(result_array) = length;
    unsigned16 *characters = AELEMENTS(result_array);
    
    position = start;
    int64 count = 0;
    
    while (count < length) {
        unsigned16 c = chr(character_array, position);
        unsigned16 character;
        
        if (c == '\\') {
            position += 1;
            unsigned16 d = chr(character_array, position);
            
            if (d != 'u') {
                character = (
                    d == 'b' ? '\b' :
                    d == 'f' ? '\f' :
                    d == 'n' ? '\n' :
                    d == 'r' ? '\r' :
                    d == 't' ? '\t' :
                    d
                );
                
                position += 1;
            }
            else {
                character =
                    xdigit(chr(character_array, position + 1)) << 12 |
                    xdigit(chr(character_array, position + 2)) << 8 |
                    xdigit(chr(character_array, position + 3)) << 4 |
                    xdigit(chr(character_array, position + 4)) << 0;
            
                position += 5;
            }
        }
        else {
            position += 1;
            character = c;
        }
        
        characters[count] = character;
        count += 1;
    }
    
    position += 1;  // skip closing quote
    
    //fprintf(stderr, "XXX parsed integer of %lld\n", is_negative ? -result : result);
    *position_lvalue = position;
    
    return YES_INTEGER(result_array);
}


static char digitx(unsigned16 x) {
    return x >= 10 ? 'A' + x - 10 : '0' + x;
}


void Std__print_jstring(Ref character_array, Ref *stream_lvalue) {
    // First pass: determine result length
    int64 length = 2;  // quotes
    int64 position = 0;
    
    while (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);

        if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            length += 2;
        }
        else if (iscntrl(c)) {
            length += 6;
        }
        else {
            length += 1;
        }
        
        position += 1;
    }
        
    // Second pass: do the conversion
    Ref stream = preappend(*stream_lvalue, length);
    int64 count = ALENGTH(stream);
    ALENGTH(stream) += length;
    unsigned16 *characters = AELEMENTS(stream);
    position = 0;
    
    characters[count++] = '"';
    
    while (position < ALENGTH(character_array)) {
        unsigned16 c = chr(character_array, position);

        if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            characters[count++] = '\\';
            characters[count++] = (
                c == '\b' ? 'b' :
                c == '\f' ? 'f' :
                c == '\n' ? 'n' :
                c == '\r' ? 'r' :
                c == '\t' ? 't' :
                c
            );
        }
        else if (iscntrl(c)) {
            characters[count++] = '\\';
            characters[count++] = 'u';
            characters[count++] = digitx((c >> 12) & 0x000f);
            characters[count++] = digitx((c >>  8) & 0x000f);
            characters[count++] = digitx((c >>  4) & 0x000f);
            characters[count++] = digitx((c >>  0) & 0x000f);
        }
        else {
            characters[count++] = c;
        }

        position += 1;
    }

    characters[count++] = '"';
    
    *stream_lvalue = stream;
}


void *C__decode_utf8(Ref byte_array) {
    if (!byte_array)
        return NULL;

    int64 byte_length = ALENGTH(byte_array);
    char *bytes = AELEMENTS(byte_array);

    void *character_array = allocate_basic_array(byte_length, 2);
    unsigned16 *characters = AELEMENTS(character_array);
    
    int64 byte_count, character_count;
    decode_utf8_buffer(bytes, byte_length, characters, ARESERVATION(character_array), &byte_count, &character_count);
    ALENGTH(character_array) = character_count;
    
    return reallocate_array(character_array, character_count, 2);
}


void *C__encode_utf8(Ref character_array) {  //(Alias string_alias) {
    //void *character_array = *(void **)string_alias;
    //if (!character_array)
    //    return NULL;

    int64 character_length = ALENGTH(character_array);
    unsigned16 *characters = AELEMENTS(character_array);
    
    void *byte_array = allocate_basic_array(character_length * 3, 1);
    char *bytes = AELEMENTS(byte_array);
    int64 byte_length = character_length * 3;

    int64 character_count, byte_count;
    encode_utf8_buffer(characters, character_length, bytes, byte_length, &character_count, &byte_count);
    ALENGTH(byte_array) = byte_count;
    
    return reallocate_array(byte_array, byte_count, 1);
}


void *C__decode_utf8_slice(Slice byte_slice) {
    int64 byte_length = SLENGTH(byte_slice);
    char *bytes = SELEMENTS(byte_slice, 1);

    //fprintf(stderr, "decode_utf8_slice byte length %lld.\n", byte_length);

    void *character_array = allocate_basic_array(byte_length, 2);
    unsigned16 *characters = AELEMENTS(character_array);
    
    int64 byte_count, character_count;
    decode_utf8_buffer(bytes, byte_length, characters, ARESERVATION(character_array), &byte_count, &character_count);
    ALENGTH(character_array) = character_count;

    //fprintf(stderr, "decode_utf8_slice decoded %lld bytes to %lld characters.\n", byte_count, character_count);

    return reallocate_array(character_array, character_count, 2);
}


Maybe fs__Path__mkdir(Ref name_array /*Alias path_alias*/, int64 mode) {
    //void *name_array = RECORDMEMBER(path_alias, Ref);
    int64 character_length = ALENGTH(name_array);
    char bytes[character_length * 3 + 1];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(name_array), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);
    bytes[byte_count] = '\0';

    fprintf(stderr, "mkdir '%s' %llo\n", bytes, mode);
    int rc = mkdir(bytes, mode);
    int er = errno;

    fprintf(stderr, "mkdir ret %d\n", er);
    
    return rc == -1 ? NOT(EXCNO(er)) : YES();
}


Maybe fs__Path__rmdir(Ref name_array /*Alias path_alias*/) {
    //void *name_array = RECORDMEMBER(path_alias, Ref);
    int64 character_length = ALENGTH(name_array);
    char bytes[character_length * 3 + 1];
    
    int64 character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(name_array), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);
    bytes[byte_count] = '\0';

    fprintf(stderr, "rmdir '%s'\n", bytes);
    int rc = rmdir(bytes);
    int er = errno;
    
    fprintf(stderr, "rmdir ret %d\n", er);
    return rc == -1 ? NOT(EXCNO(er)) : YES();
}


MaybeInteger fs__File__read(Ref file_ref, Slice buffer_slice) {
    int fd = CLASSMEMBER(file_ref, int);
    int64 buffer_length = SLENGTH(buffer_slice);
    char *buffer_elements = SELEMENTS(buffer_slice, 1);
    
    int rc = read(fd, buffer_elements, buffer_length);
    int er = errno;
    
    fprintf(stderr, "read from %d returned %d\n", fd, rc);
    
    return rc == -1 ? NOT_INTEGER(EXCNO(er)) : YES_INTEGER(rc);
}


MaybeInteger C__reader_get(Ref reader_ref, int64 length) {
    int fd = CLASSMEMBER(reader_ref, int);
    void *byte_array = allocate_basic_array(length, 1);
    int64 buffer_length = length;
    char *buffer_elements = AELEMENTS(byte_array);
    
    int rc = read(fd, buffer_elements, buffer_length);
    int er = errno;
    
    fprintf(stderr, "get from %d returned %d\n", fd, rc);
    
    if (rc == -1) {
        free_basic_array(byte_array);
        return NOT_INTEGER(EXCNO(er));
    }

    byte_array = reallocate_array(byte_array, rc, 1);
    ALENGTH(byte_array) = rc;
    
    return YES_INTEGER(byte_array);
}


MaybeInteger C__reader_get_all(Ref reader_ref) {
    int length = 4096;
    int fd = CLASSMEMBER(reader_ref, int);
    void *byte_array = allocate_basic_array(length, 1);
    int64 buffer_length = length;
    char *buffer_elements = AELEMENTS(byte_array);
    
    int64 read_length = 0;

    while (1) {
        int rc = read(fd, buffer_elements + read_length, buffer_length - read_length);
        int er = errno;

        fprintf(stderr, "get_all from %d returned %d\n", fd, rc);
    
        if (rc == 0)
            break;
            
        if (rc == -1) {
            free_basic_array(byte_array);
            return NOT_INTEGER(EXCNO(er));
        }
        
        read_length += rc;
        
        if (read_length == buffer_length) {
            byte_array = reallocate_array(byte_array, read_length + 4096, 1);
            buffer_length = ALENGTH(byte_array);
            buffer_elements = AELEMENTS(byte_array);
        }
    }

    byte_array = reallocate_array(byte_array, read_length, 1);
    ALENGTH(byte_array) = read_length;

    return YES_INTEGER(byte_array);
}


// Entry point

extern void start();

int main() {
    // Must be using the C locale during the execution of the whole program,
    // setlocale is not allowed anywhere.
    
    setlinebuf(stdout);
    setlinebuf(stderr);
    
    start();

    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
        
    if (refcount_balance)
        printf("Oops, the refcount balance is %lld!\n", refcount_balance);
        
    return 0;
}
