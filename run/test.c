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

void printu8(char a) {
    printf("%c\n", a);
}

void prints(const char *s) {
    int len = *(int *)s;
    printf("%*s\n", len, s + 8);
}

extern void start();

int main(int argc, char **argv) {
    start();
}
