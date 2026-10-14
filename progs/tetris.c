#include <stdint.h>
#include "printf.h"

#define W 10
#define H 20

char kbhit();
char get_key();

void usleep(uint32_t t) {
    
    volatile uint32_t n;
    
    for (uint32_t i = 0; i < 10; i++) {
        n = t;        
        while(n--);
    }
}

uint8_t rand_() {
    static unsigned int i = 10;
    //uint8_t *array = (uint8_t *)0x500;

    //return array[i++];
    return i++;
}

const char ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// 7 Tetromino shapes defined in 4x4 grids (flattened)
const char shapes[7][16] = {
    {0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0}, // I
    {0,0,0,0, 0,2,2,0, 0,2,2,0, 0,0,0,0}, // O
    {0,0,0,0, 0,3,3,0, 3,3,0,0, 0,0,0,0}, // Z
    {0,0,0,0, 4,4,0,0, 0,4,4,0, 0,0,0,0}, // S
    {0,0,0,0, 5,5,5,0, 0,0,5,0, 0,0,0,0}, // J
    {0,0,0,0, 6,6,6,0, 6,0,0,0, 0,0,0,0}, // L
    {0,0,0,0, 7,7,7,0, 0,7,0,0, 0,0,0,0}  // T
};

// ANSI color mappings for the pieces
const char* colors[] = { "\033[0m", "\033[36m", "\033[33m", "\033[31m", "\033[32m", "\033[34m", "\033[35m", "\033[37m" };

char board[H][W] = {0}, piece[16];
int px, py, score = 0, over = 0;



// Verify piece collisions
int check_collision(int nx, int ny, char* p) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (p[y * 4 + x]) {
                int tx = nx + x, ty = ny + y;
                if (tx < 0 || tx >= W || ty >= H) return 1;
                if (ty >= 0 && board[ty][tx]) return 1;
            }
        }
    }
    return 0;
}

// Rotate piece 90 degrees clockwise
void rotate() {
    char temp[16];
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            temp[x * 4 + (3 - y)] = piece[y * 4 + x];
    if (!check_collision(px, py, temp)) 
        for (int i = 0; i < 16; i++) piece[i] = temp[i];
}

// Spawn a random new piece
void spawn() {
    int idx = rand_() % 7;
    for (int i = 0; i < 16; i++) piece[i] = shapes[idx][i];
    px = W / 2 - 2; py = -1;
    if (check_collision(px, py, piece)) over = 1;
}

// Permanently burn piece to board and check rows
void lock_and_clear() {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (piece[y * 4 + x] && (py + y) >= 0)
                board[py + y][px + x] = piece[y * 4 + x];
        }
    }
    for (int y = H - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < W; x++) if (!board[y][x]) full = 0;
        if (full) {
            score += 100;
            for (int ty = y; ty > 0; ty--)
                for (int tx = 0; tx < W; tx++) board[ty][tx] = board[ty - 1][tx];
            for (int tx = 0; tx < W; tx++) board[0][tx] = 0;
            y++; // Recheck same row index
        }
    }
}

// Draw the screen frame
void draw() {
    printf_("\033[H"); // Reset cursor home
    printf_("Score: %d\n", score);
    for (int x = 0; x < W + 12; x++) printf_("#");
    printf_("\n");

    for (int y = 0; y < H; y++) {
        printf_("#");
        for (int x = 0; x < W; x++) {
            int p_idx = 0;
            if (x >= px && x < px + 4 && y >= py && y < py + 4)
                p_idx = piece[(y - py) * 4 + (x - px)];
            
            if (p_idx) printf_("%s[]", colors[p_idx]);
            else if (board[y][x]) printf_("%s[]", colors[(int)board[y][x]]);
            else printf_("  ");
        }
        //printf_("\033[0m#\n");
        printf_("#\n");
    }
    for (int x = 0; x < W + 12; x++) printf_("#");
    printf_("\n");
}

int main() {


    spawn();

    int timer = 0;
    unsigned char scan_code;
    while (!over) {
        if (kbhit()) {
            scan_code = get_key();
            if (scan_code > 0) {
                char ch = ascii[scan_code];
                if (ch == 'a' && !check_collision(px - 1, py, piece)) px--;
                if (ch == 'd' && !check_collision(px + 1, py, piece)) px++;
                if (ch == 's' && !check_collision(px, py + 1, piece)) py++;
                if (ch == 'w') rotate();
                if (ch == 'q') break;
            }
        }

        if (++timer > 30) { // Tick speed
            if (!check_collision(px, py + 1, piece)) {
                py++;
            } else {
                lock_and_clear();
                spawn();
            }
            timer = 0;
        }

        draw();
        usleep(20000); // Frame limiter (~60FPS)
    }


    printf_("Game Over! Final Score: %d\n", score);
    return 0;
}
