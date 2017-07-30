#include <stdio.h>
#include <stdlib.h>
#include "../utf8.c"
#include "../arch/heap.h"

static int allocation_count = 0;


// Exported helpers

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


void die() {
    fprintf(stderr, "I have a bad feeling about this...\n");
    abort();
}


// Internal helpers

#define ALEN(x) *(long *)((x) + ARRAY_LENGTH_OFFSET)
#define ARES(x) *(long *)((x) + ARRAY_RESERVATION_OFFSET)
#define AITE(x) ((x) + ARRAY_ITEMS_OFFSET)
#define HREF(x) *(long *)((x) + HEAP_REFCOUNT_OFFSET)


void *allocate_array(long length, long size) {
    void *array = memalloc(HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    HREF(array) = 1;
    ARES(array) = length;
    return array;
}


void *reallocate_array(void *array, long length, long size) {
    if (HREF(array) != 1)
        fprintf(stderr, "Oops, reallocating an array with %ld references!\n", HREF(array));

    array = memrealloc(array + HEAP_HEADER_OFFSET, HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    ARES(array) = length;
    return array;
}


void *append_decode_utf8(void *character_array, char *bytes, long byte_length) {
    long character_length = ALEN(character_array);
    long character_reserve = ARES(character_array);
    
    if (character_reserve - character_length < byte_length)
        character_array = reallocate_array(character_array, character_length + byte_length, 2);
        
    unsigned short *characters = AITE(character_array);
    ALEN(character_array) += decode_utf8_buffer(bytes, byte_length, characters + character_length);
    
    return character_array;
}


// Library functions

void print(long a) {
    printf("%ld\n", a);
}


void printu8(char a) {
    printf("%c\n", a);
}


void prints(void *s) {
    if (s) {
        long character_length = ALEN(s);
        char bytes[character_length * 3];
        int byte_length = encode_utf8_buffer(AITE(s), character_length, bytes);
        printf("%.*s\n", byte_length, bytes);
    }
    else
        printf("(null)\n");
}


void printb(void *s) {
    if (s) {
        long byte_length = ALEN(s);
        char *bytes = AITE(s);
        printf("%.*s\n", (int)byte_length, bytes);
    }
    else
        printf("(null)\n");
}


void *decode_utf8(void *byte_array) {
    if (!byte_array)
        return NULL;

    long byte_length = ALEN(byte_array);
    char *bytes = AITE(byte_array);

    void *character_array = allocate_array(byte_length, 2);
    unsigned short *characters = AITE(character_array);
    
    long character_length = decode_utf8_buffer(bytes, byte_length, characters);
    ALEN(character_array) = character_length;
    
    return reallocate_array(character_array, character_length, 2);
}


void *encode_utf8(void *character_array) {
    if (!character_array)
        return NULL;

    long character_length = ALEN(character_array);
    unsigned short *characters = AITE(character_array);
    
    void *byte_array = allocate_array(character_length * 3, 1);
    char *bytes = AITE(byte_array);

    long byte_length = encode_utf8_buffer(characters, character_length, bytes);
    ALEN(byte_array) = byte_length;
    
    return reallocate_array(byte_array, byte_length, 1);
}


// FIXME: snprintf for double is locale-dependent!
#define STRINGIFY(fmt, val) \
    char byte_array[ARRAY_HEADER_SIZE + 20]; \
    ALEN(byte_array) = snprintf(byte_array + ARRAY_HEADER_SIZE, sizeof(byte_array) - ARRAY_HEADER_SIZE, fmt, val); \
    return decode_utf8(byte_array);


void *stringify_integer(long x) {
    STRINGIFY("%ld", x);
}


void streamify_integer(long x, void **character_array_lvalue) {
    char byte_array[30];
    int byte_length = snprintf(byte_array, sizeof(byte_array), "%ld", x);
    
    void *character_array = *character_array_lvalue;
    character_array = append_decode_utf8(character_array, byte_array, byte_length);
    *character_array_lvalue = character_array;
}


// Entry point

extern void start();

int main() {
    start();
    
    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
}
