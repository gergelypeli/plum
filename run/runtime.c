#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

#include "../utf8.c"
#include "../arch/heap.h"

#define ALENGTH(x) *(long *)((x) + ARRAY_LENGTH_OFFSET)
#define ARESERVATION(x) *(long *)((x) + ARRAY_RESERVATION_OFFSET)
#define AELEMENTS(x) ((x) + ARRAY_ELEMS_OFFSET)

#define HREFCOUNT(x) *(long *)((x) + HEAP_REFCOUNT_OFFSET)
#define HWEAKREFCOUNT(x) *(long *)((x) + HEAP_WEAKREFCOUNT_OFFSET)
#define HFINALIZER(x) *(long *)((x) + HEAP_FINALIZER_OFFSET)
#define HNEXT(x) *(long *)((x) + HEAP_NEXT_OFFSET)

#define RECORDMEMBER(obj, mtype) *(mtype *)(obj)
#define CLASSMEMBER(obj, mtype) *(mtype *)(obj + CLASS_MEMBERS_OFFSET)

typedef void *Ref;
typedef void *Alias;

typedef struct {
    long valued;  // returned in RAX
    long raised;  // returned in RDX
} Varied;

#define VALUED(x) ((Varied) { x, NO_EXCEPTION })
#define RAISED(x) ((Varied) { 0, x + ERRNO_TREENUM_OFFSET })

extern void empty_function();
extern void finalize_reference_array();

static int allocation_count = 0;
static locale_t unfucked_locale;


// Memory management

void *memalloc(long size) {
    allocation_count += 1;
    void *x = malloc(size);
    //fprintf(stderr, " -- malloc %p %ld\n", x, size);
    return x;
}


void memfree(void *m) {
    allocation_count -= 1;
    //fprintf(stderr, " -- free %p\n", m);
    free(m);
}


void *memrealloc(void *m, long size) {
    void *x = realloc(m, size);
    //fprintf(stderr, " -- realloc %p %ld %p\n", m, size, x);
    return x;
}


// Internal helpers

void *allocate_basic_array(long length, long size) {
    void *array = memalloc(HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    
    HNEXT(array) = 0;
    HREFCOUNT(array) = 1;
    HWEAKREFCOUNT(array) = 0;
    HFINALIZER(array) = (long)(void *)empty_function;  // TODO: this only works for basic types!
    
    ARESERVATION(array) = length;
    ALENGTH(array) = 0;
    
    return array;
}


void *allocate_string_array(long length) {
    int size = ADDRESS_SIZE;
    void *array = memalloc(HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    
    HNEXT(array) = 0;
    HREFCOUNT(array) = 1;
    HWEAKREFCOUNT(array) = 0;
    HFINALIZER(array) = (long)(void *)finalize_reference_array;
    
    ARESERVATION(array) = length;
    ALENGTH(array) = 0;
    
    return array;
}


void *reallocate_array(void *array, long length, long size) {
    if (HREFCOUNT(array) != 1)
        fprintf(stderr, "Oops, reallocating an array with %ld references!\n", HREFCOUNT(array));

    array = memrealloc(array + HEAP_HEADER_OFFSET, HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    ARESERVATION(array) = length;
    return array;
}


void *append_decode_utf8(void *character_array, char *bytes, long byte_length) {
    long character_length = ALENGTH(character_array);
    long character_reserve = ARESERVATION(character_array);
    
    if (character_reserve - character_length < byte_length)
        character_array = reallocate_array(character_array, character_length + byte_length, 2);
        
    unsigned short *characters = AELEMENTS(character_array);
    long available_length = ARESERVATION(character_array) - ALENGTH(character_array);
    
    long byte_count, character_count;
    decode_utf8_buffer(bytes, byte_length, characters + character_length, available_length, &byte_count, &character_count);
    ALENGTH(character_array) += character_count;
    
    return character_array;
}


void lvalue_append_decode_utf8(void **character_array_lvalue, char *byte_array, long byte_length) {
    void *character_array = *character_array_lvalue;
    character_array = append_decode_utf8(character_array, byte_array, byte_length);
    *character_array_lvalue = character_array;
}


// Exported helpers


void logfunc(const char *message) {
    fprintf(stderr, "LOG: %s\n", message);
}


struct R { unsigned long r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rsp, rdx, rcx, rbx, rax, rflags; };

void dump(const char *message, struct R *r) {
    fprintf(stderr, "DUMP: %s [%c%c%c%c%c%c]\n", message,
        (r->rflags & 1 ?    'C' : 'c'),
        (r->rflags & 4 ?    'P' : 'p'),
        (r->rflags & 16 ?   'A' : 'a'),
        (r->rflags & 64 ?   'Z' : 'z'),
        (r->rflags & 128 ?  'S' : 's'),
        (r->rflags & 2048 ? 'O' : 'o')
    );
    fprintf(stderr, "              ____    ____          ____    ____          ____    ____          ____    ____\n");
    fprintf(stderr, "      RAX=%016lx  RBX=%016lx  RCX=%016lx  RDX=%016lx\n", r->rax, r->rbx, r->rcx, r->rdx);
    fprintf(stderr, "      RSP=%016lx  RBP=%016lx  RSI=%016lx  RDI=%016lx\n", r->rsp + 32, r->rbp, r->rsi, r->rdi);
    fprintf(stderr, "      R8 =%016lx  R9 =%016lx  R10=%016lx  R11=%016lx\n", r->r8, r->r9, r->r10, r->r11);
    fprintf(stderr, "      R12=%016lx  R13=%016lx  R14=%016lx  R15=%016lx\n", r->r12, r->r13, r->r14, r->r15);
}


void die(const char *message) {
    fprintf(stderr, "Can't go on like this... %s\n", message);
    abort();
}


void dies(void *s) {
    long character_length = ALENGTH(s);
    char bytes[character_length * 3 + 1];
    
    long character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(s), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);

    bytes[byte_count] = '\0';
    fprintf(stderr, "DIE: %.*s\n", (int)byte_count, bytes);
    abort();
}


void streamify_integer(long x, void **character_array_lvalue) {
    char byte_array[30];
    long byte_length = snprintf(byte_array, sizeof(byte_array), "%ld", x);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void streamify_unteger(unsigned long x, void **character_array_lvalue) {
    char byte_array[30];
    long byte_length = snprintf(byte_array, sizeof(byte_array), "%lu", x);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void streamify_boolean(unsigned char x, void **character_array_lvalue) {
    char *byte_array = (x ? "`true" : "`false");
    long byte_length = strlen(byte_array);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void streamify_float(double x, void **character_array_lvalue) {
    char byte_array[30];
    locale_t xxx = uselocale(unfucked_locale);
    long byte_length = snprintf(byte_array, sizeof(byte_array), "%g", x);
    uselocale(xxx);
    lvalue_append_decode_utf8(character_array_lvalue, byte_array, byte_length);
}


void sort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    qsort(base, nmemb, size, compar);
}


void *string_regexp_match(void *subject_array, void *pattern_array) {
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
        short *subject_characters = AELEMENTS(subject_array);
        
        for (int i = 0; i < rc; i++) {
            size_t start = ovector[2 * i];
            size_t len = ovector[2 * i + 1] - ovector[2 * i];
            //fprintf(stderr, "PCRE2 match %d start %ld length %ld.\n", i, start, len);
            
            void *target_array = allocate_basic_array(len, CHARACTER_SIZE);
            ALENGTH(target_array) = len;
            short *target_characters = AELEMENTS(target_array);
            
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

void printi(long a) {
    printf("%ld\n", a);
}


void printc(short a) {
    printf("%c\n", a);
}


void printd(double a) {
    printf("%g\n", a);
}


void printz(const char *s) {
    printf("%s\n", s);
}


void prints(void *s) {
    if (s) {
        long character_length = ALENGTH(s);
        char bytes[character_length * 3 + 1];
        
        long character_count, byte_count;
        encode_utf8_buffer(AELEMENTS(s), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);

        bytes[byte_count] = '\0';
        printf("%.*s\n", (int)byte_count, bytes);
    }
    else
        printf("(null)\n");
}


void printb(void *s) {
    if (s) {
        long byte_length = ALENGTH(s);
        char *bytes = AELEMENTS(s);
        printf("%.*s\n", (int)byte_length, bytes);
    }
    else
        printf("(null)\n");
}


void *decode_utf8(void *byte_array) {
    if (!byte_array)
        return NULL;

    long byte_length = ALENGTH(byte_array);
    char *bytes = AELEMENTS(byte_array);

    void *character_array = allocate_basic_array(byte_length, 2);
    unsigned short *characters = AELEMENTS(character_array);
    
    long byte_count, character_count;
    decode_utf8_buffer(bytes, byte_length, characters, ARESERVATION(character_array), &byte_count, &character_count);
    ALENGTH(character_array) = character_count;
    
    return reallocate_array(character_array, character_count, 2);
}


void *encode_utf8(void *string_alias) {
    void *character_array = *(void **)string_alias;
    if (!character_array)
        return NULL;

    long character_length = ALENGTH(character_array);
    unsigned short *characters = AELEMENTS(character_array);
    
    void *byte_array = allocate_basic_array(character_length * 3, 1);
    char *bytes = AELEMENTS(byte_array);
    long byte_length = character_length * 3;

    long character_count, byte_count;
    encode_utf8_buffer(characters, character_length, bytes, byte_length, &character_count, &byte_count);
    ALENGTH(byte_array) = byte_count;
    
    return reallocate_array(byte_array, byte_count, 1);
}


Varied path_mkdir(Alias path_alias, long mode) {
    void *name_array = RECORDMEMBER(path_alias, Ref);
    long character_length = ALENGTH(name_array);
    char bytes[character_length * 3 + 1];
    
    long character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(name_array), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);
    bytes[byte_count] = '\0';

    fprintf(stderr, "mkdir '%s' %lo\n", bytes, mode);
    int rc = mkdir(bytes, mode);
    int er = errno;

    fprintf(stderr, "mkdir ret %d\n", er);
    
    return rc == -1 ? RAISED(er) : VALUED(0);
}


Varied path_rmdir(Alias path_alias) {
    void *name_array = RECORDMEMBER(path_alias, Ref);
    long character_length = ALENGTH(name_array);
    char bytes[character_length * 3 + 1];
    
    long character_count, byte_count;
    encode_utf8_buffer(AELEMENTS(name_array), character_length, bytes, sizeof(bytes) - 1, &character_count, &byte_count);
    bytes[byte_count] = '\0';

    fprintf(stderr, "rmdir '%s'\n", bytes);
    int rc = rmdir(bytes);
    int er = errno;
    
    fprintf(stderr, "rmdir ret %d\n", er);
    return rc == -1 ? RAISED(er) : VALUED(0);
}


Varied reader_read(Ref reader_ref, Ref buffer_array) {
    int fd = CLASSMEMBER(reader_ref, int);
    long buffer_length = ALENGTH(buffer_array);
    long buffer_reservation = ARESERVATION(buffer_array);
    char *buffer_elements = AELEMENTS(buffer_array);
    
    int rc = read(fd, buffer_elements, buffer_reservation - buffer_length);
    int er = errno;
    
    fprintf(stderr, "read %d ret %d\n", fd, er);
    
    if (rc >= 0)
        ALENGTH(buffer_array) += rc;
    
    return rc == -1 ? RAISED(er) : VALUED(0);
}


// Entry point

extern void start();

int main() {
    unfucked_locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
    
    start();

    freelocale(unfucked_locale);

    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
}
