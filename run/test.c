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

void *decode_utf8(void *byte_array) {
    int byte_length = *(int *)byte_array;
    char *bytes = byte_array + 8;

    void *character_blob = malloc(8 + 8 + byte_length * 2);
    *(int *)character_blob = 1;
    void *character_array = character_blob + 8;
    unsigned short *characters = character_array + 8;
    
    int character_length = decode_utf8_raw(bytes, byte_length, characters);
    
    *(int *)character_array = character_length;
    character_blob = realloc(character_blob, 8 + 8 + character_length * 2);
    
    allocation_count += 1;
    return character_blob + 8;
}


void *encode_utf8(void *character_array) {
    int character_length = *(int *)character_array;
    unsigned short *characters = character_array + 8;
    
    void *byte_blob = malloc(8 + 8 + character_length * 3);
    *(int *)byte_blob = 1;
    void *byte_array = byte_blob + 8;
    char *bytes = byte_array + 8;

    int byte_length = encode_utf8_raw(characters, character_length, bytes);
    
    *(int *)byte_array = byte_length;
    byte_blob = realloc(byte_blob, 8 + 8 + byte_length);
    
    allocation_count += 1;
    return byte_blob + 8;
}


extern void start();

int main() {
    start();
    
    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
}
