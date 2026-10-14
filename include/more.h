struct MoreStack {
    char *array[1000];
    int top;
};

static struct MoreStack more_stack;

void more_init() {
    more_stack.top = 0;
}

void more_push(char *ptr) {
    more_stack.array[more_stack.top++] = ptr;
}

char *more_pop() {      
    return more_stack.array[--more_stack.top];
}

int more_top() { 
    return more_stack.top;
}

