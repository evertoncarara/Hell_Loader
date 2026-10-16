#include <stdint.h>
#include "fat16.h"
#include "disk.h"
#include "file.h"
#include "key.h"
#include "sprintf.h"
#include "printf.h"
#include "linux.h"
#include "dos.h"
#include "pwd.h"  
#include "doskey.h"
#include "more.h"


// To load above address 0x100000 (Enable A20 gate), data must be first loaded 
// to some memory area achievable in real mode, due to limitations of bios_extended_read_drive 
// and then copied to high memory area
#define HIGH_MEMORY         0x1000000
#define CURRENT_DIR_ADDR    0x90000     // 9000:0000 (segment 9)
#define TEMP_LOAD_ADDR      0x10000     // 1000:0000 (segment 1)


// Commands
#define UNKNOWN     0
#define LS          1
#define CD          2    
#define RST         3        
#define MORE        4   
#define LINUX       5  
#define LOAD        6   
#define RUN         7    
#define DUMP        8   
#define DOS         9    
#define PWD         10    
#define HISTORY     11
#define CLEAR       12
#define MEM         13
#define GET_TIME    14
#define SET_TIME    15
#define MEM_WRITE   16        

// https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15,_EAX_=_0xE820
struct SMAP_entry_t {

	uint32_t BaseL;     // base address uint64_t
	uint32_t BaseH;
	uint32_t LengthL;   // length uint64_t
	uint32_t LengthH;
	uint32_t Type;      // entry Type
	uint32_t ACPI;      // extended

}__attribute__((packed)) ;



// Assembly functions on ldlib.asm
void bios_extended_read_drive(uint16_t lba_high, uint16_t lba_low, uint16_t n, uint16_t segment, uint16_t offset);
void cpu_reset();
int16_t get_mmap(struct SMAP_entry_t *smap, uint16_t entries);
void get_time(uint8_t *hour, uint8_t *minute, uint8_t *second);
void set_time(uint8_t hour, uint8_t minute, uint8_t second);



void mem_report() {
    struct SMAP_entry_t *smap = (struct SMAP_entry_t *)0x1000;
    int16_t entries;
    uint32_t total = 0;
    
    entries = get_mmap(smap, 20);
    
    if (entries < 0) {
        puts_("mem_report fail\n");
        return;    
    }
    
    puts_("Memory report\n");
    puts_("-------------\n");     
    for (uint16_t i = 0; i < entries; i++, smap++) {    
        if (smap->Type == 1)
            printf_("Type: USABLE   - ");
        else if (smap->Type == 2)
            printf_("Type: RESERVED - ");
        else
            printf_("Type: %X - ", smap->Type);
        
        printf_("[0x%X%X] - %d KiB (0x%X%X)\n", smap->BaseH, smap->BaseL, smap->LengthL/1024, smap->LengthH, smap->LengthL);
        
        if (smap->Type == 1)
            total += smap->LengthL;
    }
    printf_("Total USABLE: %d MiB\n\n", total / (1024*1024));
}


void prompt(char *subdir_entry) {
    
    static unsigned int count = 0;
    
    //printf_("%d:\\%s> ",count++, subdir_entry);
    printf_("%d:%s> ",count++, subdir_entry);
    prompt_cursor();
}


uint16_t get_token(char *str, uint16_t n, char *token) {

    uint16_t i = 0;

    while(str[n] == ' ')    // Removes leading spaces
        n++;   
    
    while(str[n] != ' ' && str[n] != '\0') {
        token[i++] = str[n++];
    }

    token[i] = '\0';

    // Skip next spaces
    while(str[n] == ' ') 
        n++;

    return n;
}


void run(uint32_t addr) {

    void (*func_ptr)(void) = (void (*)(void))addr;
        
    func_ptr();
}


void more(uint32_t addr, uint32_t fsize) {

    char *ptr = (char *)addr;
    char key;
    int32_t print_lines = LINES;
    
    // Print one page
    more_init();

    do {
        
        if (print_lines == LINES)   // Print one page ?
            more_push(ptr);         // Saves the beginning of the page
               
        // Print lines
        while (print_lines > 0 && fsize > 0) {                       
            printf_("%c",*ptr);
            if (*ptr == '\n')
                print_lines--;   
        
            ptr++;
            if ((uint32_t)(ptr - addr) > fsize) { // End of file ?
                puts_("\n");
                return;
            }
        
        }
    
        key = getchar();
        
        // Naivigation
        switch(key) {                              
            
            // Advance one line
            case '\n':
            case 's': 
                print_lines = 1;        
            break;
            
            // Advance one page
            case 'd':
            case ' ':
                print_lines = LINES;    // Print one page
            break;
           
            // Return one page
            case 'a':
                if(more_top() > 1) {
                    more_pop();         // Remove the begin of the current page     
                    ptr = more_pop();   // Retrieves the begin of the previous page
                    print_lines = LINES;    // Print one page
                }
            break;
        };    
        
    } while (key != 'q');
    
    puts_("\n");
}


void time() {

    uint8_t hour, minute, second;
    get_time(&hour, &minute, &second);
    printf_("%X:%X:%X\n",hour, minute, second);
}



uint8_t get_command(char *cmd) {
    
    if (!strcmp_(cmd, "ls") || !strcmp_(cmd, "dir")) return LS;
    if (!strcmp_(cmd, "clear") || !strcmp_(cmd, "cls")) return CLEAR;
    if (!strcmp_(cmd, "cd"))        return CD;
    if (!strcmp_(cmd, "rst"))       return RST;
    if (!strcmp_(cmd, "more"))      return MORE;
    if (!strcmp_(cmd, "linux"))     return LINUX;
    if (!strcmp_(cmd, "load"))      return LOAD;
    if (!strcmp_(cmd, "run"))       return RUN;
    if (!strcmp_(cmd, "dump"))      return DUMP;
    if (!strcmp_(cmd, "dos"))       return DOS;
    if (!strcmp_(cmd, "pwd"))       return PWD;
    if (!strcmp_(cmd, "history"))   return HISTORY;
    if (!strcmp_(cmd, "mem"))       return MEM;
    if (!strcmp_(cmd, "time"))      return GET_TIME;
    if (!strcmp_(cmd, "stime"))     return SET_TIME;
    if (!strcmp_(cmd, "mwrite"))    return MEM_WRITE;
    
    return UNKNOWN;    
}



// String returned without '\n'
char *keyboard_read(char *str, uint32_t n) {
        
    int8_t scan_code;    
    uint32_t i = 0;
    
    while(1) {
        if (kbhit()) {        
            scan_code = get_key();
        
            if (scan_code > 0) {        // Key pressed                                               
                                
                switch(scan_code) {
                    case F1_KEY:
                        text_color();
                    break;
                    
                    case F2_KEY:
                        background_color();
                    break;
                    
                    case UP_KEY:
                    case DOWN_KEY:
                        clear_line();                    
                        strcpy_(str, doskey_read(scan_code));
                        puts_(str);
                        i = strlen_(str);
                    break;
                    
                    /*case LEFT_KEY:
                        if (i > 0) {
                            i--;
                            CURSOR-=2;
                        }                        
                    break;*/
                    
                    // Normal keys
                    default:
                        str[i] = ascii[scan_code];
                        printf_("%c", str[i]);                              
                               
                        show_cursor();
                        
                        if (str[i] == '\b') {   // Backspace ?
                            
                            if (i > 0)
                                i--;
                        }
                        else if (str[i] == '\n') {
                            str[i] = '\0';      // Removes ENTER
                            doskey_insert(str);
                            return str;
                        }
                        else if (i == (n - 2)) { // Buffer full ?
                            str[++i] = '\0';
                            puts_("\n");
                            return str;
                        }
                        else
                            i++;
                };
            }
        }
    }
}


void dump(uint32_t addr) {
    
    uint32_t c;
    int32_t i;
    uint8_t *ptr = (uint8_t *)addr;
    uint8_t *ptr0;              // Points the beginning of the current page
    uint8_t *ptr_char;
    char ad[10];
    uint8_t lines;              // Number of lines to print
    char key;
    const uint8_t bytes = 16;   // Number of bytes printed per line
    char target[20];            // Jump target address    
    char toggle = 0;            // Toggle between text or hex mode
        
    // Print page by page or line by line
    lines = LINES;
    ptr0 = ptr;         // ptr0 points the beginning of the current page
    do {
        
            
        for (;lines > 0; lines--) {
            // Print address        
            sprintf_(ad, "%X", ptr);
            if (strlen_(ad) < 8) {  // Pad address with 0
                puts_("[");            
                for (i = strlen_(ad); i < 8; i++)
                    puts_("0");
                printf_("%X]: ", ptr);
            } 
            else // 8 digits address
                printf_("[%X]: ", ptr);
            
            // Print bytes
            ptr_char = ptr;
            for (c = 0; c < bytes; c++) {
                printf_("%X ",*ptr);
                ptr++;        
            }

            // Print characters
            puts_(" |");
            for (c = 0; c < bytes; c++) {
                if (*ptr_char >= 32) // Printable character ?
                    printf_("%c",*ptr_char);
                else
                    puts_(".");
                ptr_char++;        
            }
            puts_("|");
            puts_("\n");
        }
        
        key = getchar();        
        
        // Naivigation
        switch(key) {        
            
            // Advance one line
            case 's':
            case '\n':   
                ptr0 += bytes;          // Set the next line as beginning of the current page
                ptr = ptr0 + (LINES - 1) * bytes;   // Set ptr at the address in the end of page
                CURSOR = (LINES - 1) * BYTES_LINE;  // Place the cursor on the last line of the page   
                scroll_up();                                                 
                lines = 1;              // Print a new line        
            break;
            
            // Return one line
            case 'w':
                ptr0 -= bytes;          // Set the previous line as beginning of the current page
                ptr = ptr0;             // Set ptr at the beginning of the current page
                CURSOR = 0;   
                scroll_down();                
                lines = 1;              // Print a new page
            break;
            
            // Advance one page
            case 'd':
            case ' ': 
                ptr0 += bytes * LINES;  // Advance to the beginning of the next page
                ptr = ptr0;             // Set ptr at the beginning of the current page
                CURSOR = 0;                
                lines = LINES;          // Print a new page
            break;           
            
            // Return one page
            case 'a':            
                ptr0 -= bytes * LINES;  // Return to the beginning of the previous page
                ptr = ptr0;             // Set ptr at the beginning of the current page
                CURSOR = 0;
                lines = LINES;          // Print a new page
            break;
            
            // Jump to address
            case 'j':
                CURSOR = (LINES - 1) * BYTES_LINE;  // Place the cursor on the last line of the page
                printf_("                                                                               \r");
                puts_(": Jump to ");
                keyboard_read(target, sizeof(target));
                ptr0 = ptr = (uint8_t *)atoi_(target);
                lines = LINES;                      // Print a new page
            break;
            
            // Toggle to text mode
            case 't':
                clear_screen();
                                
                if (toggle == 0) {                    
                                    
                    for (ptr_char = ptr0; CURSOR < LINES * BYTES_LINE; ptr_char++) 
                        printf_("%c", *ptr_char);
                    
                    lines = 0;
                }
                else {
                    ptr = ptr0;
                    lines = LINES;
                }
                    
                toggle = !toggle;
            break;
                           
        };
            
    
    } while(key != 'q');
    
    CURSOR = (LINES - 1) * BYTES_LINE; // Place the cursor on the last line of the page                
    puts_("\n");    
}


void mem_write(char *cmd_line) {

    uint8_t bytes[20], *ptr;
    uint16_t n = 0;
    char token[20];
    uint8_t i = 0;
   
    n = get_token(cmd_line, n, token);
    
    if (strlen_(token) == 0) {
        puts_("Sintax: mwrite <address> <bytes sequence>\n");
        return;
    }
    
    ptr = (uint8_t *)atoi_(token);
    
    do {
        n = get_token(cmd_line, n, token);        
        
        if (strlen_(token) == 0)
            break;
        
        bytes[i++] = atoi_(token);        
    } while(i < sizeof(bytes));
    
    if(i > 0)
        for(uint8_t j = 0; j < i; j++, ptr++) {
            *ptr = bytes[j];                     
            printf_("MEM[%X] = %X\n", (uint32_t)ptr, *ptr);
        }
    else
        puts_("Sintax: mwrite <address> <bytes sequence>\n");    
    

}


void stime(char *cmd_line) {
                
    uint8_t hour, minute, second;
    uint8_t n = 0;
    char token[5];
    
    n = get_token(cmd_line, n, token);
    hour = atoi_(token);
    
    if (strlen_(token) == 0) {
        puts_("Sintax: stime <hour> <minute> <second>\n");
        return;
    }
    
    n = get_token(cmd_line, n, token);
    minute = atoi_(token);
    
    if (strlen_(token) == 0) {
        puts_("Sintax: stime <hour> <minute> <second>\n");
        return;
    }
    
    n = get_token(cmd_line, n, token);
    second = atoi_(token);
    
    if (strlen_(token) == 0) {
        puts_("Sintax: stime <hour> <minute> <second>\n");
        return;
    }
    
    set_time(hour, minute, second);
    time(); 
}


void main() {
    
    // Assume bootstap is STILL loaded
    struct BPB *bpb = (struct BPB *)0x7C00; 
             
    // Assume FAT12 and root directory are STILL loaded after boot (they are stored successive)
    uint8_t *fat = (void *)FAT_ADDR; // Load from bootstap
    struct Root_Entry *root_entry = (struct Root_Entry *)ROOT_DIR_ADDR; // Defined in bootloader.asm
    
    
    clear_screen();
    puts_("\t\t\t\t\t*** Wellcome to hell loader!!! ***\n\n");
                                                                                                                   
    time();
    puts_("\n");
    
    mem_report();


    
    uint16_t init_cluster;
    uint32_t fsize;
    void (*func_ptr)(void) = (void (*)(void))TEMP_LOAD_ADDR;
    char cmd_line[100];
    char linux_img[20], ramdiskfs[20];
    char token[100];
    uint32_t addr;
    char file_name[20], fat_name[20];
    uint16_t n;
    struct Root_Entry *subdir_entry = root_entry;
    char current_dir[20];
    current_dir[0] = '\0';
    
    // Initialize sub-directory names stack
    pwd_init();
    
    // Initialize command history buffer
    doskey_init();
    
    strcpy_(current_dir, "root");   
    prompt(current_dir);  
    
    // Command line interface
    while(1) {
        
        keyboard_read(cmd_line, sizeof(cmd_line));

        n = 0;
        n = get_token(cmd_line, n, token);        
            
        // Commands first
        switch (get_command(token)) {
            case LS:
                list_files(subdir_entry, bpb->root_entry_count);
            break;
            
            case CD:
                get_token(cmd_line, n, file_name);
                fat_standard(file_name, fat_name);   
            
                init_cluster = find_file(subdir_entry, bpb->root_entry_count, fat_name, &fsize);
                
                if (init_cluster && fsize == 0) {   // Directory size == 0
                    load_file(fat, init_cluster, CURRENT_DIR_ADDR, bpb, TEMP_LOAD_ADDR);
                    subdir_entry = (struct Root_Entry *)CURRENT_DIR_ADDR;
                    
                    if (!strcmp_(file_name, "..")) {
                        strcpy_(current_dir, pwd_pop());
                    }
                    else {
                        pwd_push(current_dir);
                        strcpy_(current_dir, file_name);                
                    }               
                }
                else if (fsize > 0 ) {
                    printf_("'%s' is not directory bastard!!!\n",file_name);
                }
                else if (!strcmp_(file_name, "..")) {
                    subdir_entry = (struct Root_Entry *)ROOT_DIR_ADDR;
                    if (pwd_top() > 0) // current_dir is not root ?
                        strcpy_(current_dir, pwd_pop());
                }
                else
                    printf_("'%s' not found bastard!!!\n",file_name);
            break;
        
            case CLEAR:
                clear_screen();
            break;
            
            case RST:
                cpu_reset();
            break;
            
            case MORE:
                get_token(cmd_line, n, file_name);
                fat_standard(file_name, fat_name);
                                               
                if ( (init_cluster = find_file(subdir_entry, bpb->root_entry_count, fat_name, &fsize)) ) {                               
                    load_file(fat, init_cluster, HIGH_MEMORY, bpb, TEMP_LOAD_ADDR);
                    more(HIGH_MEMORY, fsize);
                }
                else 
                    printf_("File '%s' not found bastard!!!\n", file_name);           
            break;
            
            case LINUX:
                //n = get_token(cmd_line, n, linux_img);
                //n = get_token(cmd_line, n, ramdiskfs);
                strcpy_(linux_img,"BZIMAGE IMG");
                strcpy_(ramdiskfs,"INITRD  IMG");
                load_linux(fat, subdir_entry, bpb, linux_img, ramdiskfs);
            break;
        
            case LOAD:
                n = get_token(cmd_line, n, file_name);
                fat_standard(file_name, fat_name);
                
                if (strlen_(file_name) == 0) {
                    puts_("Sintax: load <file> <address>\n");
                    break;
                }
                
                n = get_token(cmd_line, n, token);
                addr = atoi_(token);
                
                if (strlen_(token) == 0) {
                    puts_("Sintax: load <file> <address>\n");
                    break;
                }
                
                if ( (init_cluster = find_file(subdir_entry, bpb->root_entry_count, fat_name, &fsize)) ) {                               
                    if (load_file(fat, init_cluster, addr, bpb, TEMP_LOAD_ADDR) == 0) {
                        printf_("%s: loaded %d bytes at [%X]\n",file_name, fsize, TEMP_LOAD_ADDR);                
                        printf_("%s: copied %d bytes to [%X]\n",file_name, fsize, addr);
                    }
                }
                else 
                    printf_("File %s not found bastard!!!\n", file_name);                
            break;
            
            case RUN:
                n = get_token(cmd_line, n, token);
                
                if (strlen_(token) == 0) {
                    puts_("Sintax: run <address>\n");
                    break;
                }
                
                addr = atoi_(token);        
                run(addr);
            break;
            
            case DUMP:
                n = get_token(cmd_line, n, token);
                
                if (strlen_(token) == 0) {
                    puts_("Sintax: dump <address>\n");
                    break;
                }
                
                addr = atoi_(token);        
                dump(addr);
            break;
            
            case DOS:
                dos(root_entry, bpb);
            break;
            
            case PWD:
                pwd_print();
                printf_("%s>\n",current_dir);
            break;
            
            case HISTORY:
                doskey_history();
            break;
        
            case MEM:
                mem_report();
            break;
            
            case GET_TIME:
                time();
            break;
            
            case SET_TIME:
                stime(&cmd_line[n]); // n: skips command 'stime'              
            break;
            
            case MEM_WRITE:                
                mem_write(&cmd_line[n]);// n: skips the command 'mwrite'
            break;
            
            // Then test for executables
            default:
                fat_standard(cmd_line, fat_name);            
                
                if ( (init_cluster = find_file(subdir_entry, bpb->root_entry_count, fat_name, &fsize)) ) {                               
                    
                    if (fsize > 0 && load_file(fat, init_cluster, HIGH_MEMORY, bpb, TEMP_LOAD_ADDR) == 0) {                    
                        printf_("%s: loaded %d bytes at [%X]\n",cmd_line, fsize, TEMP_LOAD_ADDR);                
                        printf_("%s: copied %d bytes to [%X]\n",cmd_line, fsize, HIGH_MEMORY);
                        func_ptr = (void (*)(void))HIGH_MEMORY;

                        // Fires loaded program
                        func_ptr();  
                    }
                    else
                        printf_("%s: is a directory bastard!!!\n", cmd_line);       
                }
                else if (cmd_line[0] != '\0')    // cmd_line is not empty
                    printf_("%s: command not found bastard!!!\n", cmd_line);                
            break;
        };

        prompt(current_dir); 
    }      
}


