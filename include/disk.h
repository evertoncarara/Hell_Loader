#pragma once

// Assembly function on boot.asm
void bios_extended_read_drive(uint16_t lba_high, uint16_t lba_low, uint16_t n, uint16_t segment, uint16_t offset);


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

