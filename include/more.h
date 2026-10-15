#define MORE_STACK_SIZE 1000

struct MoreStack {
    char *array[MORE_STACK_SIZE];
    int top;
};

static struct MoreStack more_stack;

void more_init() {
    more_stack.top = 0;
}

void more_push(char *ptr) {
    if (more_stack.top < MORE_STACK_SIZE - 2)
        more_stack.array[more_stack.top++] = ptr;    
}

char *more_pop() {      
    if (more_stack.top > 0)
        return more_stack.array[--more_stack.top];
    else
        return 0;
}

int more_top() { 
    return more_stack.top;
}

