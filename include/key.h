#pragma once

// Keyboard scan codes
#define F1_KEY      59
#define F2_KEY      60
#define UP_KEY      72
#define DOWN_KEY    80
#define LEFT_KEY    0x4B
#define RIGHT_KEY   0x4D



// Assembly functions on bootloader.asm
char kbhit();
char get_key();

const char ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

int getchar() {
    
    int key;
    
    for(;;) {
        if (kbhit()) {
            key = get_key();
            if (key > 0)
                return (int)ascii[key];
        }
    }   
}
