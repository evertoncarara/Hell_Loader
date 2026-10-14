#include <stdint.h>
#include "sprintf.h"
#include "printf.h"

 // Hide terminal cursor
 //   printf_("\e[?25l");

char kbhit();
char get_key();

const char ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

uint8_t rand_() {
    static unsigned int i = 10;
    //uint8_t *array = (uint8_t *)0x500;

    //return array[i++];
    return i++;
}

void usleep(uint32_t t) {
    
    volatile uint32_t n;
    
    for (uint32_t i = 0; i < 1000; i++) {
        n = t;        
        while(n--);
    }
}

int x = 10, y = 10, fruitX = 5, fruitY = 5, score = 0, gameOver = 0;
int tailX[100], tailY[100];
int nTail = 0;
char dir;

void setup() {
    gameOver = 0;
    dir = 'd';
    x = 20;
    y = 10;
    fruitX = rand_() % 30;
    fruitY = rand_() % 20;
}

void draw() {

    // Clear screen 
    printf_("\e[H"); // \e[H (or \033[H): Moves the cursor to the top-left home position.

    for (int i = 0; i < 32; i++) printf_("#");
    printf_("\n");

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 32; j++) {
            if (j == 0 || j == 31) printf_("#");
            else if (i == y && j == x) printf_("O");
            else if (i == fruitY && j == fruitX) printf_("F");
            else {
                int print = 0;
                for (int k = 0; k < nTail; k++) {
                    if (tailX[k] == j && tailY[k] == i) {
                        printf_("o");
                        print = 1;
                    }
                }
                if (!print) printf_(" ");
            }
        }
        printf_("\n");
    }

    for (int i = 0; i < 32; i++) printf_("#");
    printf_("\nScore: %d\n", score);
}

void input() {
    
    char scan_code;
    
    if (kbhit()) {
        scan_code = get_key();
        
        if (scan_code > 0) {      
            switch (ascii[scan_code]) {
                case 'a': dir = 'l'; break;
                case 'd': dir = 'r'; break;
                case 'w': dir = 'u'; break;
                case 's': dir = 'd'; break;
                case 'q': gameOver = 1; break;
            }
        }
    }
    //dir = 'u';
}

void logic() {
    int prevX = tailX[0];
    int prevY = tailY[0];
    int prev2X, prev2Y;
    tailX[0] = x;
    tailY[0] = y;
    for (int i = 1; i < nTail; i++) {
        prev2X = tailX[i];
        prev2Y = tailY[i];
        tailX[i] = prevX;
        tailY[i] = prevY;
        prevX = prev2X;
        prevY = prev2Y;
    }
    switch (dir) {
        case 'l': x--; break;
        case 'r': x++; break;
        case 'u': y--; break;
        case 'd': y++; break;
    }
    if (x < 1 || x > 30 || y < 0 || y > 19) gameOver = 1;
    for (int i = 0; i < nTail; i++) {
        if (tailX[i] == x && tailY[i] == y) gameOver = 1;
    }
    if (x == fruitX && y == fruitY) {
        score += 10;
        fruitX = rand_() % 30;
        fruitY = rand_() % 20;
        nTail++;
    }
}

int main() {
    setup();
    while (!gameOver) {
        draw();
        input();
        logic();
        usleep(8000); // Control loop speed (100ms per frame)
    }

    printf_("EXIT\n");
    return 0;


}
