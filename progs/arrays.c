#include "sprintf.h"
#include "printf.h"

char array_g[] = {0x10,0x11,0x12,0x13,0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
int size_g;

int main() {
   
    puts_("Hello from arrays!!!!\n");
    size_g = 17;
    char array_l[] = {0x20,0x21,0x22,0x23,0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30};
    int size_l = 17;
    
    puts_("array_g: ");
    for (int i = 0; i < size_g; i++) {
        printf_("%X ", array_g[i]);        
    }
    puts_("\n");
    
    puts_("array_l: ");
    for (int i = 0; i < size_l; i++) {
        printf_("%X ", array_l[i]);        
    }
    puts_("\n");
    
    return 0;
}




