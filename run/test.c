#include <stdio.h>
#include <stdlib.h>

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

void prints(const char *s) {
    if (s) {
        int len = *(int *)s;
        printf("%.*s\n", len, s + 8);
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

extern void start();

int main() {
    start();
    
    if (allocation_count)
        printf("Oops, the allocation count is %d!\n", allocation_count);
}
