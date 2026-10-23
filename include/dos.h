#include <stdint.h>
#include "file.h"


// Assembly functions on ldlib.asm
void run_dos(uint8_t media_type, uint8_t drive_number, uint16_t first_data_sector_high, uint16_t first_data_sector_low);


// MS-DOS bootloader
int8_t dos(struct Root_Entry *root_entry, struct BPB *bpb) {

    uint16_t init_cluster;
    uint32_t lba;
    uint16_t lba_high, lba_low;
    uint16_t segment, offset;
    uint32_t first_data_sector = bpb->hidden_sector_count + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat) + (bpb->root_entry_count * 32) / bpb->bytes_per_sector;    
    struct Root_Entry *file_entry;

    // Search for io.sys and load the threee first sectors    
    file_entry = find_file(root_entry, bpb->root_entry_count, "IO      SYS");
    init_cluster = file_entry->current_cluster_low;
    
    if (init_cluster > 0) {                               
              
        // IO.SYS always loaded at 0x700                  
        lba = first_data_sector + (init_cluster - 2) * bpb->sectors_per_cluster; // -2: First two entries in FAT are reserved
        linear2segmented(0x700, &segment, &offset);
        lba_high = lba >> 16;
        lba_low = lba & 0x0000FFFF;
        bios_extended_read_drive(lba_high, lba_low, 3, segment, offset);
    }
    else {
        printf_("ERROR: file 'io.sys' not found bastard!!!.\n");
        return -1;    
    }


/*
;   https://github.com/microsoft/MS-DOS/blob/main/v4.0/src/BIOS/MSLOAD.ASM
; We load in the first directory sector and examine it to make sure the the
; BIOS and DOS are the first two directory entries.  If they are not found,
; the user is prompted to insert a new disk.  The directory sector is loaded
; into 00500h
*/    
    uint8_t *src = (uint8_t *)root_entry;
    uint8_t *dst = (uint8_t *)0x500;
    for (uint16_t i = 0; i < 512; i++)
        dst[i] = src[i];
    

/*
;   https://github.com/microsoft/MS-DOS/blob/main/v4.0/src/BIOS/MSLOAD.ASM
; We copy the disk parameter table into a local area.  We scan the table above
; for non-zero parameters.  Any we see get changed to their non-zero values.
;
;J.K. We copy the disk parameter table into a local area (overlayed into the
;code), and set the head settle time to 1, and End of Track to SECLIM given
;by FORMAT.
*/    
    
    src = (uint8_t *)0xFEFDE;
    dst = (uint8_t *)0x7C3E;
    for (uint8_t i = 0; i < 11; i++) 
        dst[i] = src[i];    
    
    dst[4] = bpb->sectors_per_track;
    
    dst = (uint8_t *)0x78;
    
    // Set address [0x78]: 0x00007C3E
    dst[3] = 0x00;
    dst[2] = 0x00;
    dst[1] = 0x7C;
    dst[0] = 0x3E;
    
    uint16_t first_data_sector_high = first_data_sector >> 16;
    uint16_t first_data_sector_low = first_data_sector & 0x0000FFFF;
    
    run_dos(bpb->media_type, bpb->drive_number, first_data_sector_high, first_data_sector_low);
    
    return 0;   // Never return!
}

