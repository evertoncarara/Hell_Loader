#pragma once

#include "sprintf.h"
#include "screen.h"

void printf_(const char* format, ...) {
    
    char buf[200];
    va_list args;
    va_start(args, format);

    const char* p = format;
    char* buf_ptr = buf;
    
    int i;
    char c, *s;
    
    while (*p) {
        if (*p == '%') {
            switch (*++p) { // Move to the specifier character
                case 'd': // Integer
                    i = va_arg(args, int);
                    buf_ptr += itoa_(i, buf_ptr);
                    break;
                
                case 's':  // String
                    s = va_arg(args, char*);
                    buf_ptr += strcpy_(buf_ptr, s);
                    break;
                     
                case 'c':  // Character
                    c = (char)va_arg(args, int); // char is promoted to int in varargs
                    *buf_ptr++ = c;
                    break;
                
                case '%': // Percent sign
                    *buf_ptr++ = '%';
                    break;
                    
                case 'X': // Hexadecimal
                case 'x':
                    i = va_arg(args, int);
                    buf_ptr += itoh(i, buf_ptr, *p);
                    break;                
                               
                default: // Unsupported format
                    *buf_ptr++ = '%';
                    *buf_ptr++ = *p;
                    break;
            }
        }
        else {
            *buf_ptr++ = *p; // Copy regular character
        }
        
        p++;
    }

    *buf_ptr = '\0'; // Null-terminate the buffer
    va_end(args);
    
    puts_(buf);
}

