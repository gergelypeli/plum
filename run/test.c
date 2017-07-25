#include <stdio.h>
#include <stdlib.h>
#include "../utf8.c"
#include "../arch/heap.h"

static int allocation_count = 0;

long star(long a, long b) {
    return a * b;
}

long plus(long a, long b) {
    return a + b;
}

void print(long a) {
    printf("%ld\n", a);
}

void printu8(char a) {
    printf("%c\n", a);
}

#define ALEN(x) *(long *)((x) + ARRAY_LENGTH_OFFSET)
#define ARES(x) *(long *)((x) + ARRAY_RESERVATION_OFFSET)
#define AITE(x) ((x) + ARRAY_ITEMS_OFFSET)
#define HREF(x) *(long *)((x) + HEAP_REFCOUNT_OFFSET)

void prints(void *s) {
    if (s) {
        long character_length = ALEN(s);
        char *bytes = malloc(character_length * 3);
        int byte_length = encode_utf8_raw(AITE(s), character_length, bytes);
        printf("%.*s\n", byte_length, bytes);
        free(bytes);
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

void *memalloc(long size) {
    allocation_count += 1;
    void *x = malloc(size);
    //printf("malloc %p\n", x);
    return x;
}

void memfree(void *m) {
    allocation_count -= 1;
    //printf("free %p\n", m);
    free(m);
}

void *memrealloc(void *m, long size) {
    void *x = realloc(m, size);
    return x;
}


void *allocate_array(long length, long size) {
    void *array = malloc(HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    HREF(array) = 1;
    ARES(array) = length;
    allocation_count += 1;
    return array;
}


void *reallocate_array(void *array, long length, long size) {
    array = realloc(array + HEAP_HEADER_OFFSET, HEAP_HEADER_SIZE + ARRAY_HEADER_SIZE + length * size) - HEAP_HEADER_OFFSET;
    ARES(array) = length;
    return array;
}


void *decode_utf8(void *byte_array) {
    if (!byte_array)
        return NULL;

    long byte_length = ALEN(byte_array);
    char *bytes = AITE(byte_array);

    void *character_array = allocate_array(byte_length, 2);
    unsigned short *characters = AITE(character_array);
    
    long character_length = decode_utf8_raw(bytes, byte_length, characters);
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

    long byte_length = encode_utf8_raw(characters, character_length, bytes);
    ALEN(byte_array) = byte_length;
    
    return reallocate_array(byte_array, byte_length, 1);
}

#define STRINGIFY(fmt, val) \
    char byte_array[ARRAY_HEADER_SIZE + 20]; \
    ALEN(byte_array) = snprintf(byte_array + ARRAY_HEADER_SIZE, sizeof(byte_array) - ARRAY_HEADER_SIZE, fmt, val); \
    return decode_utf8(byte_array);

void *stringify_integer(long x) {
    STRINGIFY("%ld", x);
}

extern void start();

int main() {
    start();
    
    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
}
