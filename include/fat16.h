#pragma once

#include "sprintf.h" // strcmp_(), strcpy_(), memcpy_()

#define FAT_ADDR        0x60000 // Also set in bootloader.asm (FAT_SEGMENT:FAT_OFFSET)
#define ROOT_DIR_ADDR   0x80000 // Also set in bootloader.asm (ROOT_DIR_SEGMENT:ROOT_DIR_OFFSET)
#define END_OF_CHAIN    0xFFF8
#define FILE_NAME_SIZE  11      // FAT12: 8.3 file name. The first 8 characters are the name and the last 3 are the extension.

// BIOS Parameter Block 
struct BPB {
    uint8_t     bootjmp[3];
    char        oem_name[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sectors;
    uint8_t     fat_count;
    uint16_t    root_entry_count;
    uint16_t    total_sectors;
    uint8_t     media_type;
    uint16_t    sectors_per_fat;
    uint16_t    sectors_per_track;
    uint16_t    head_side_count;
    uint32_t    hidden_sector_count;
    uint32_t    large_sector_count;
    
    //extended fat12 and fat16 stuff
	uint8_t		drive_number;
    
}__attribute__((packed));


struct Root_Entry {
    char        name[11];
    uint8_t     attributes;
    uint8_t     reserved_WNT;
    uint8_t     creation_time_sec;
    uint16_t    creation_time;
    uint16_t    creation_date;
    uint16_t    last_access;
    uint16_t    current_cluster_high;   //  For FAT 12 and FAT 16 this is always zero.
    uint16_t    last_mofication_time;
    uint16_t    last_mofication_date;
    uint16_t    current_cluster_low;    // The low 16 bits of this entry's first cluster number. Use this number to find the first cluster for this entry.
    uint32_t    size;                   // The size of the file in bytes.
       
}__attribute__((packed));


// attributes
#define READ_ONLY       0x01
#define HIDDEN          0x02
#define SYSTEM          0x04
#define VOLUME_ID       0x08
#define DIRECTORY       0x10
#define ARCHIVE         0x20
#define LONG_FILE_NAME  0x0F

// Gemini generated function
uint8_t is_valid_fat16_char(unsigned char c) {
    // Reject control characters (0x00-0x1F) and DEL (0x7F)
    if (c <= 0x1F || c == 0x7F) {
        return 0;
    }

    // Reject characters explicitly forbidden by the FAT specification
    const char *invalid_chars = "\"*/:<>?\\|+,;=[] ";
    for (uint8_t i = 0; invalid_chars[i] != '\0'; i++)
        if (c == invalid_chars[i]) {
            return 0;        
        }

    return 1;
}

// Gemini generated function
uint8_t valid_fat16_filename(char *filename) {
    if (filename == NULL || filename[0] == '\0') {
        return 0;
    }

    int name_len = 0;
    int ext_len = 0;
    uint8_t has_dot = 0;


    for (size_t i = 0; filename[i] != '\0'; i++) {

        if (filename[i] == '.') {
            // Only one dot is allowed, and the base name cannot be empty
            if (has_dot || name_len == 0) {
                return 0;
            }
            has_dot = 1;
            continue;
        }

        if (!is_valid_fat16_char(filename[i])) {
            return 0;
        }

        if (!has_dot) {
            name_len++;
            if (name_len > 8) {
                return 0; // Base name exceeds 8 characters
            }
        } else {
            ext_len++;
            if (ext_len > 3) {
                return 0; // Extension exceeds 3 characters
            }
        }
    }

    // If a dot was present, there must be at least 1 extension character
    if (!has_dot || (has_dot && ext_len == 0)) {
        return 0;
    }

    return 1;
}

// Gemini generated function
uint8_t valid_fat16_dirname(char *dir_name) {
    
    // 1. Check for NULL or empty string
    if (dir_name == NULL || dir_name[0] == '\0') {
        return 0;
    }

    uint8_t len = strlen_(dir_name);

    // 2. Standard FAT16 base name cannot exceed 8 characters
    if (len > 8) {
        return 0;
    }

    // 3. FAT16 filenames/directories cannot begin with a space (0x20)
    if (dir_name[0] == ' ') {
        return 0;
    }

    // 4. Check each character against FAT16 invalid character rules
    // Forbidden ASCII symbols: " * / : < > ? \ | + , . ; = [ ]  
    for (uint8_t i = 0; i < len; i++) 
        if (!is_valid_fat16_char(dir_name[i]) || dir_name[i] == '.') 
            return 0;

    return 1;
}




// Put in FAT standard 8.3 format
// The first 8 characters are the name and the last 3 are the extension
void fat_standard(char *name, char *fat_name) {
    
    uint8_t i = 0, j = 0;
   
    // First test fot special dot entries
    if (!strcmp_(name,".")) {
        strcpy_(fat_name, ".          ");
        return;
    }
    else if (!strcmp_(name,"..")) {
        strcpy_(fat_name, "..         ");
        return;
    }
    
    // Normal names
    //+1 includes dot (.)
    while (i < FILE_NAME_SIZE + 1 && name[i] != '\0' && name[i] != '.') {
        fat_name[i] = name[i];
        i++;
    }
    
    if (name[i] == '.') {   // File
        for (j = i; j < 8; j++)
            fat_name[j] = ' ';
        
        fat_name[8] = name[++i];
        
        if (name[i + 1] == '\0') {          // One char extension
            fat_name[9] = ' ';
            fat_name[10] = ' ';        
        }
        else {
            fat_name[9] = name[++i];
            
            if (name[i + 1] == '\0')        // Two chars extension
                fat_name[10] = ' ';
            else {
                fat_name[10] = name[++i];   // Three chars extension
            }        
        }       
    }
    else {  // Directory
        for (j = i; j < 11; j++)
            fat_name[j] = ' ';
    }
}
