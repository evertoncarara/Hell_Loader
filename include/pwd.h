#include "sprintf.h"

#define PWD_STACK_SIZE  100

struct PwdStack {
    char data[PWD_STACK_SIZE][50];
    int top;
};

static struct PwdStack pwd_stack;

void pwd_init() {
    pwd_stack.top = 0;
}

void pwd_push(char *str) {
    if (pwd_stack.top < PWD_STACK_SIZE - 2)
        strcpy_(pwd_stack.data[pwd_stack.top++], str);
}

char *pwd_pop() {      
    if (pwd_stack.top > 0)
        return pwd_stack.data[--pwd_stack.top];
    else
        return 0;
}

void pwd_print() {
    
    for (int8_t i = 0 ; i < pwd_stack.top; i++)
        printf_("%s>",pwd_stack.data[i]);
}

