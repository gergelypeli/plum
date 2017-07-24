#include <stdio.h>
#include <stdlib.h>
#include "../utf8.c"

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

void prints(void *s) {
    if (s) {
        int len = *(int *)s;
        char *bytes = malloc(len * 3);
        int byte_length = encode_utf8_raw(s + 8, len, bytes);
        printf("%.*s\n", byte_length, bytes);
        free(bytes);
    }
    else
        printf("(null)\n");
}

void printb(void *s) {
    if (s) {
        int len = *(int *)s;
        char *bytes = s + 8;
        printf("%.*s\n", len, bytes);
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
    void *blob = malloc(8 + 8 + length * size);
    *(long *)blob = 1;
    void *array = blob + 8;
    *(long *)array = length;
    allocation_count += 1;
    return array;
}


void *reallocate_array(void *array, long length, long size) {
    array = realloc(array - 8, 8 + 8 + length * size) + 8;
    *(long *)array = length;
    return array;
}


void *decode_utf8(void *byte_array) {
    long byte_length = *(long *)byte_array;
    char *bytes = byte_array + 8;

    void *character_array = allocate_array(byte_length, 2);
    unsigned short *characters = character_array + 8;
    
    long character_length = decode_utf8_raw(bytes, byte_length, characters);
    
    return reallocate_array(character_array, character_length, 2);
}


void *encode_utf8(void *character_array) {
    long character_length = *(long *)character_array;
    unsigned short *characters = character_array + 8;
    
    void *byte_array = allocate_array(character_length * 3, 1);
    char *bytes = byte_array + 8;

    long byte_length = encode_utf8_raw(characters, character_length, bytes);
    
    return reallocate_array(byte_array, byte_length, 1);
}

#define STRINGIFY(fmt, val) \
    char byte_array[40]; \
    *(long *)byte_array = snprintf(byte_array + 8, sizeof(byte_array) - 8, fmt, val); \
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
