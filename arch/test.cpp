#include "ia32.h"

int main() {
    IA32 ia32;
    
    ia32.init("mymodule");
    ia32.op(MOVD, EAX, 0x12345678);
    ia32.done("mymodule");
}
