#include <stdio.h>

long star(long a, long b) {
    return a * b;
}

long plus(long a, long b) {
    return a + b;
}

void print(long a) {
    printf("%ld\n", a);
}

extern void start();

int main(int argc, char **argv) {
    start();
}
