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
