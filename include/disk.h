#pragma once

// Assembly function on boot.asm
void bios_extended_read_drive(uint16_t lba_high, uint16_t lba_low, uint16_t n, uint16_t segment, uint16_t offset);
void bios_extended_write_drive(uint16_t lba_high, uint16_t lba_low, uint16_t n, uint16_t segment, uint16_t offset);


void linear2segmented(uint32_t addr, uint16_t *segment, uint16_t *offset) {
    
    *segment = (addr >> 4) & 0xF000;
    *offset  = addr - (*segment << 4);
}


// Returns the next cluster
uint16_t load_clusters(uint8_t *fat, uint16_t init_cluster, uint16_t count, uint32_t addr, struct BPB *bpb) {

    uint16_t current_cluster = init_cluster;
    uint32_t lba;
    uint16_t lba_high, lba_low;
    uint32_t current_load_addr = addr;
    uint16_t segment, offset;
    uint32_t first_data_sector = bpb->hidden_sector_count + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat) + (bpb->root_entry_count * 32) / bpb->bytes_per_sector;    
    uint16_t *fat16 = (uint16_t *)fat;

    // Read cout file clusters one by one using FAT16
    while(count--) {                                   
        
        lba = first_data_sector + (current_cluster - 2) * bpb->sectors_per_cluster; // -2: First two entries in FAT are reserved
        linear2segmented(current_load_addr, &segment, &offset);        
        lba_high = lba >> 16;
        lba_low = lba & 0x0000FFFF;
        bios_extended_read_drive(lba_high, lba_low, bpb->sectors_per_cluster, segment, offset);
        current_load_addr += bpb->sectors_per_cluster * bpb->bytes_per_sector;
            
        // Get next cluster
        current_cluster = fat16[current_cluster];              
    }
    
    // Return the next current_cluster
    return current_cluster;
}


// Write one cluster and set FAT with the next cluster
void write_cluster(uint8_t *fat, uint16_t init_cluster, uint16_t next_cluster, uint32_t addr, struct BPB *bpb) {

    uint16_t current_cluster = init_cluster;
    uint32_t lba;
    uint16_t lba_high, lba_low;
    uint16_t segment, offset;
    uint32_t first_data_sector = bpb->hidden_sector_count + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat) + (bpb->root_entry_count * 32) / bpb->bytes_per_sector;    
    uint16_t *fat16 = (uint16_t *)fat;
                               
        
    lba = first_data_sector + (current_cluster - 2) * bpb->sectors_per_cluster; // -2: First two entries in FAT are reserved
    linear2segmented(addr, &segment, &offset);        
    lba_high = lba >> 16;
    lba_low = lba & 0x0000FFFF;
    bios_extended_write_drive(lba_high, lba_low, bpb->sectors_per_cluster, segment, offset);

        
    // Set next cluster on FAT
    fat16[current_cluster] = next_cluster;        
}


// Write back FAT on disk
void update_fat_disk(uint8_t *fat, struct BPB *bpb) {
    
    // When using BIOS INT 13h extended write functions (such as AH=43h), 
    // the maximum number of sectors that can be transferred in a single call is 127
    const uint8_t max_write_sectors = 127;
    
    uint16_t lba_high, lba_low;
    uint16_t segment, offset;    
    uint32_t lba;    
    uint8_t count = bpb->sectors_per_fat / max_write_sectors;
    uint8_t rem_sectors = bpb->sectors_per_fat % max_write_sectors;
    uint8_t *fat_temp;
    
    
    // Upadate FATs
    for (uint8_t j = 0; j < bpb->fat_count; j++) {
    
        lba = bpb->hidden_sector_count + bpb->reserved_sectors + (j * bpb->sectors_per_fat);     
        fat_temp = fat;
        
        // Write chuncks of max_write_sectors
        for (uint8_t i = 0; i < count; i++, lba += max_write_sectors, fat_temp += max_write_sectors * bpb->bytes_per_sector) {
            linear2segmented((uint32_t)fat_temp, &segment, &offset);        
            lba_high = lba >> 16;
            lba_low = lba & 0x0000FFFF;
            bios_extended_write_drive(lba_high, lba_low, max_write_sectors, segment, offset);
        }
        
        // Write the remaining sectors
        linear2segmented((uint32_t)fat_temp, &segment, &offset);        
        lba_high = lba >> 16;
        lba_low = lba & 0x0000FFFF;
        bios_extended_write_drive(lba_high, lba_low, rem_sectors, segment, offset);
    }
}


// Write back FAT on disk
void update_dir_entry_disk(uint8_t *dir, struct BPB *bpb, uint32_t temp_ram_area) {
       
    uint16_t lba_high, lba_low;
    uint16_t segment, offset;    
    uint32_t lba;    
    uint16_t dir_sectors = (bpb->root_entry_count * 32) / bpb->bytes_per_sector;
    
    // BIOS cant access memory above 1MiB
    memcpy_((uint8_t *)temp_ram_area, (uint8_t *)dir, bpb->root_entry_count * 32);
    
    // Set the initial sector of root directory 
    lba = bpb->hidden_sector_count + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);     
        
    // Write sectors
    linear2segmented(temp_ram_area, &segment, &offset);        
    lba_high = lba >> 16;
    lba_low = lba & 0x0000FFFF;
    bios_extended_write_drive(lba_high, lba_low, dir_sectors, segment, offset);    
}


// Write back FAT on disk
void load_root_dir(uint8_t *dir, struct BPB *bpb, uint32_t temp_ram_area) {
       
    uint16_t lba_high, lba_low;
    uint16_t segment, offset;    
    uint32_t lba;    
    uint16_t dir_sectors = (bpb->root_entry_count * 32) / bpb->bytes_per_sector;
            
    // Set the initial sector of root directory   
    lba = bpb->hidden_sector_count + bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);     
        
    // Read sectors
    linear2segmented(temp_ram_area, &segment, &offset);        
    lba_high = lba >> 16;
    lba_low = lba & 0x0000FFFF;
    bios_extended_read_drive(lba_high, lba_low, dir_sectors, segment, offset);
    
    // BIOS cant access memory above 1MiB
    memcpy_(dir, (uint8_t *)temp_ram_area, bpb->root_entry_count * 32);    
}

