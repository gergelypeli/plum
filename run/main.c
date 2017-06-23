#include <stdio.h>

// Must abuse the x86-64 SysV ABI, the first 6 values are passed in registers

#define ABUSE long x1, long x2, long x3, long x4, long x5, long x6

void star(ABUSE, long a, long b, long ret) {
    ret = a * b;
}

void plus(ABUSE, long a, long b, long ret) {
    ret = a + b;
}

void print(ABUSE, long a) {
    printf("%ld\n", a);
}

extern void start();

int main(int argc, char **argv) {
    start();
}
