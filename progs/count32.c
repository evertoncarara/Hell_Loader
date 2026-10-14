#include "sprintf.h"
#include "printf.h"

int main() {
   
    unsigned int count;
    
    for (count = 0; ; count++) {
        printf_("%d\r", count);        
    }
   
    return 0;
}
