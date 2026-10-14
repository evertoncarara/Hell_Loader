#include "printf.h" // printf_(), strcpy_()

#define BUFFER_SIZE 500

struct CircularBuffer {
    char data[BUFFER_SIZE][100];
    uint16_t head;       // Next write position
    uint16_t count;      // Number of elements currently in the buffer
    uint16_t i;  
};

struct CircularBuffer buffer;

// Initialize the buffer
void doskey_init() {
    buffer.head = 0;
    buffer.count = 0;
    buffer.i = 0;
}

void doskey_insert(char *cmd_line) {
    
    if (cmd_line[0] != '\0') {  // Do not insert empty strings
        strcpy_(buffer.data[buffer.head], cmd_line);
        buffer.head = (buffer.head + 1) % BUFFER_SIZE; // Wrap around index
        
        if (buffer.count < BUFFER_SIZE)
            buffer.count++;    
    }   
        
    buffer.i = buffer.head;    
}


char *doskey_read(char next) {
    static char cmd_line[100];
    static char last_key = 0;
    
    if (buffer.count > 0) {       
        
        if (last_key != 0) {            
            if (next == UP_KEY)
                buffer.i--;
            else    // DOWN_KEY
                buffer.i++;
        }
        else
            buffer.i = buffer.count - 1;
        
        if (buffer.i >= buffer.count)          // up key
            buffer.i =  buffer.count - 1;
        
        strcpy_(cmd_line, buffer.data[buffer.i]);        
            
        last_key = next;    
    }
    else
        cmd_line[0] = '\0';
    
    return cmd_line; 
}

void doskey_history() {

    uint16_t count = buffer.count;
    int16_t i;

    if (buffer.i == 0)
        i = buffer.count - 1;
    else
        i = buffer.i - 1;
    
    while (count-- > 0) {
        printf_("%d: %s\n", i % BUFFER_SIZE, buffer.data[i % BUFFER_SIZE]);
        i--;
        if (i < 0)
            i = buffer.count - 1;
   }
    
   puts_("\n");
}


