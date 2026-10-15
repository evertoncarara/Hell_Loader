#include "sprintf.h"

#define MAX         100

struct Stack {
    char array[MAX][50];
    int top;
};

static struct Stack stack;

void pwd_init() {
    stack.top = 0;
}

void pwd_push(char *str) {
    if (stack.top < MAX - 2)
        strcpy_(stack.array[stack.top++], str);
}

char *pwd_pop() {      
    if (stack.top > 0)
        return stack.array[--stack.top];
    else
        return 0;
}

void pwd_print() {
    
    for (int8_t i = 0 ; i < stack.top; i++)
        printf_("%s/",stack.array[i]);
}

