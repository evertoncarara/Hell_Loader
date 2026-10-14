#pragma once


#define VRAM_TEXT       0xB8000
#define LINES           25
#define COLUMNS         80
#define BYTES_LINE      2 * COLUMNS

// Cursor position shared between bootloader and programs
#define CURSOR          (*((volatile unsigned int *)0x7FFC)) // 0x7FFC: 4 bytes before the start of loader.bin (0x8000: set in bootloader.asm)

static char *vram = (char *)VRAM_TEXT;
static char text_attr = 0x4E;   // Background and text colors
static unsigned int prompt_end;


void scroll_up() {

    char *line;
    unsigned int i;    
    
    line = vram;
    // Copy the next line to the current 'line' 
    for(int l = 0; l < LINES - 1; l++, line+=BYTES_LINE)
        for(i = 0; i < BYTES_LINE; i++)
            line[i] = line[BYTES_LINE + i];    
 
    // Clear the last line
    for(i = 0; i < BYTES_LINE; i+=2) {
        line[i] = 0;    // Blank
        line[i + 1] = text_attr;        
    }         
}

void scroll_down() {

    char *line;
    unsigned int i;    
    
    line = vram + (BYTES_LINE * (LINES - 1));
    
    // Copy the next line to the current 'line' 
    for(int l = 0; l < LINES; l++, line-=BYTES_LINE) 
        for(i = 0; i < BYTES_LINE; i++) 
            line[BYTES_LINE + i] = line[i];          
      
}


// Change text color
void text_color() {

    char *line;
    unsigned int i;    
    
    line = vram;
    
    text_attr =  (line[1] & 0xF0) | ((text_attr + 1) & 0x0F);    
    
    for(int l = 0; l < LINES; l++, line+=BYTES_LINE)
        for(i = 1; i < BYTES_LINE; i+=2)
            line[i] = text_attr;    
}


// Change background color
void background_color() {

    char *line;
    unsigned int i;    
    
    line = vram;
    
    text_attr =  ((text_attr + 0x10) & 0xF0) | (line[1] & 0x0F);    
    
    for(int l = 0; l < LINES; l++, line+=BYTES_LINE)
        for(i = 1; i < BYTES_LINE; i+=2)
            line[i] = text_attr;    
}


void clear_screen() {

    unsigned int i;
  
    for (i = 0; i < LINES * BYTES_LINE; i+=2) {  
        vram[i] = 0;
        vram[i + 1] = 0x4E;
    }
    
    CURSOR = 0;
}

void clear_line() {

    while (CURSOR > prompt_end) {   // Do not erase prompt
        vram[CURSOR] = 0;           // Erase cursor
        CURSOR -= 2;                // Return cursor
    }
}

void puts_(char *str) {
    
    unsigned int i;
    int current_line;
    
    for(i = 0; str[i] != 0; i++) {
        
        switch (str[i]) {
            case '\n': 
                if (vram[CURSOR] == '_')
                    vram[CURSOR] = 0;   // Erase cursor
                
                current_line = CURSOR / ((unsigned int)BYTES_LINE); 
                if (current_line == LINES) {    // Last line ?
                    scroll_up();
                    CURSOR = (LINES) * BYTES_LINE;  // Cursor return
                }
                else
                    CURSOR = (current_line + 1) * BYTES_LINE;
            break;
                
            case '\r': 
                CURSOR = (CURSOR / ((unsigned int)BYTES_LINE)) * BYTES_LINE;
            break;
            
            case '\t': 
                CURSOR += 2 * 4;
            break;
            
            case '\e':
                if(str[i + 1] == '[' && str[i + 2] == '2' && str[i + 3] == 'J') {
                    clear_screen();
                    i += 3;
                }
                else if(str[i + 1] == '[' && str[i + 2] == 'H') {
                    CURSOR = 0; 
                    i += 2;
                }
            break;
            
            case '\b':                
                if (CURSOR > prompt_end) {  // Do not erase prompt
                    vram[CURSOR] = 0;       // Erase cursor
                    CURSOR -= 2;            // Return cursor
                }
            break;   
                
            default: // Print char 
                if (CURSOR < LINES * BYTES_LINE) {
                    vram[CURSOR] = str[i]; 
                    CURSOR += 2;
                }
                else { // Scroll
                    scroll_up();
                    CURSOR = (LINES - 1) * BYTES_LINE;
                    vram[CURSOR] = str[i]; 
                    CURSOR += 2;
                }
            break;
        }   
    }
}

void show_cursor() {
    
    vram[CURSOR] = '_';

}

void prompt_cursor() {
    
    show_cursor();
    prompt_end = CURSOR;   // Set the position where prompt ends
}









