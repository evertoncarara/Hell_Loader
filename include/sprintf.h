#pragma once

#include <stdarg.h>




// https://www.geeksforgeeks.org/cpp/implementing-of-strtok-function-in-c/
char* strtok_ (char* s, char d) {

    // Stores the state of string
    static char* input = 0;
    static char result[50];

    // Initialize the input string
    if (s != 0)
        input = s;

    // Case for final token
    if (input == 0)
        return 0;


    // Stores the extracted string
    //char* result = new char[strlen(input) + 1];
    int i = 0;

    // Start extracting string and
    // store it in array
    for (; input[i] != '\0'; i++) {

        // If delimiter is not reached
        // then add the current character
        // to result[i]
        if (input[i] != d)
            result[i] = input[i];

        // Else store the string formed
        else {
            result[i] = '\0';
            input = input + i + 1;
            return result;
        }
    }

    // Case when loop ends
    result[i] = '\0';
    input = 0;

    // Return the resultant pointer
    // to the string
    return result;
}




/*  Copy len characters from src to dest, and return dest
        - libgcc
            https://github.com/gcc-mirror/gcc/blob/master/libgcc/memcpy.c  */    
void *memcpy_(void *dest, const void *src, unsigned int len) {

    char *d = dest;
    const char *s = src;
    
    while (len--)
        *d++ = *s++;
    
    return dest;
}


/* Compare strings s and t
    return 
        < 0 if s < t
        0 if s == t
        > 0 if s > t 
    The C programming Language
        By Brian W. Kernighan and Dennis M. Ritchie  */
int strcmp_(char *s, char *t) {

    int i;
    
    for (i = 0; s[i] == t[i]; i++)
        if (s[i] == '\0')
            return 0;

    return s[i] - t[i];
}


/*  Copy t to s; array subscript version
    The C programming Language
        By Brian W. Kernighan and Dennis M. Ritchie  */
int strcpy_(char *s, char *t) {

    int i;
 
    i = 0;
    
        while ((s[i] = t[i]))
            i++;
            
    return i;
}


/*  Return length of sn
    The C programming Language
        By Brian W. Kernighan and Dennis M. Ritchie  */
int strlen_(char s[]) {

    int i = 0;
    
    while (s[i] != '\0')
        ++i;
    
    return i;
}


/*  Return the absolute value of I.  
    GNU C Library implementation  */
int abs_ (int i) {

  return i < 0 ? -i : i;
}


/*  Reverse string s in place.  
    The C programming Language
        By Brian W. Kernighan and Dennis M. Ritchie  */
void reverse(char s[]) {
    
    int c, i, j;

    for (i = 0, j = strlen_(s) - 1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/*  Convert n to characters in s.  
    The C programming Language
        By Brian W. Kernighan and Dennis M. Ritchie  */
int itoa_ (int n, char s[]) {

    int i, sign;
    
    if ((sign = n) < 0)         /* record sign */
        n = -n;                 /* make n positive */
 
    i = 0;
 
    /* generate digits in reverse order */
    do { 
        s[i++] = abs_(n % 10) + '0';    /* get next digit */
    } while ((n /= 10) != 0);            /* delete it */
 
    if (sign < 0)
        s[i++] = '-';
 
    s[i] = '\0';
        
    reverse(s);
    
    return i;
}

// Helper function to convert integer to a two-digit hexadecimal string
int itoh (int n, char *s, char c) {

    const char hex_upper[] = "0123456789ABCDEF";
    const char hex_lower[] = "0123456789abcdef";
    const char *t;
    int i = 0;
    
    if (c == 'x')
        t = hex_lower;
    else
        t = hex_upper;        
    
    do {
        s[i++] = t[n & 0xF];    
        n = (unsigned)n >> 4;        
    } while (n);

    if (i == 1) // Conversion with at least 2 digits
        s[i++] = '0';
    
    s[i] = '\0';
        
    reverse(s);    
    
    return i; // Number of characters written
}


// Scans string for formatters (%d / %s / %c / %X / %%) and replaces them 
// with data from argument list
void sprintf_(char* buf, const char* format, ...) {
    
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
}


int atoi_(char *str) {

    // Initialize res to 0
    int res = 0;
    int i = 0;
    int base = 10;
    int digit;

    if (strlen_(str) > 1) {
        if (str[0] == '0' && str[1] == 'x') { // Hex integer ?
            base = 16;
            i = 2;
        }
    }

    // Iterate through the string str and compute res
    while (str[i] != '\0') {
        if (str[i] >= '0' && str[i] <= '9')                 
            digit = (str[i] - '0');
        else if (base == 16 && str[i] >= 'a' && str[i] <= 'f')
            digit = (str[i] - 'a' + 10);
        else if (base == 16 && str[i] >= 'A' && str[i] <= 'F')
            digit = (str[i] - 'A' + 10);
        else {
            //printf_("ERROR atoi_: invalid digit %c\n",str[i]);
            return -1;
        }

        res = res * base + digit;
        i++;
    }
    
    return res;
}
