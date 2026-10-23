#pragma once

#include "printf.h"
#include "fat16.h"
#include "disk.h"
#include "key.h"



char *upper_case(char *str) {

    for (uint8_t i = 0; str[i] != '\0'; i++) 
        if (str[i] >= 'a' && str[i] <= 'z')
            str[i] -= 32; // To upper case

    return str;
}

struct Root_Entry *find_file(struct Root_Entry *dir_entry, uint16_t count, char *name) {
    
    uint16_t found = 1;
    struct Root_Entry *search_entry = (struct Root_Entry *)0;  
    
    upper_case(name);
    
    for(uint16_t entry = 0; entry < count; entry++, dir_entry++)  {
        
        found = 1;     
        for(uint8_t c = 0; c < FILE_NAME_SIZE; c++) {
            if (dir_entry->name[c] != name[c]) {
                found = 0;
                search_entry = (struct Root_Entry *)0;
                break;
            }
        }
        if (found) {
            search_entry = dir_entry;
            break;
        }

    }

    return search_entry;    
}


int load_file(uint8_t *fat, uint16_t init_cluster, uint32_t addr, struct BPB *bpb, uint32_t temp_ram_area) {

    uint16_t current_cluster = init_cluster;
    uint32_t current_load_addr = addr;
    uint32_t end;
    char key; 
     
    // Read the file cluster by cluster 
    while(current_cluster < END_OF_CHAIN) {    

        end = current_load_addr + bpb->sectors_per_cluster * bpb->bytes_per_sector;
        
        if (end >= 0x9FC00 && end <= 0x100000) {
            printf_("Overwriting BIOS and video memory area [%X - %X].\n",current_load_addr, end);
            printf_("Continue ? (y/n)\n");
            
            do {            
                key = getchar();
            } while (key != 'y' && key != 'n');
            
            if (key == 'n')
                return -1;
        }
        
        current_cluster = load_clusters(fat, current_cluster, 1, temp_ram_area, bpb);
        
        // BIOS cant access memory above 1MiB
        memcpy_((uint8_t *)current_load_addr, (uint8_t *)temp_ram_area, bpb->sectors_per_cluster * bpb->bytes_per_sector);
       
        current_load_addr += bpb->sectors_per_cluster * bpb->bytes_per_sector;        
    }
    
    return 0;
}


void write_file(uint8_t *fat, char *name, struct Root_Entry *dir, uint16_t *clusters, uint32_t addr, uint32_t count, struct BPB *bpb, uint32_t temp_ram_area) {

    uint16_t next_cluster;
    uint32_t current_read_addr = addr;
    uint32_t i;
     
    // Write the file cluster by cluster 
    for (i = 0; i < count; i++) {            

        // BIOS cant access memory above 1MiB
        memcpy_((uint8_t *)temp_ram_area, (uint8_t *)current_read_addr, bpb->sectors_per_cluster * bpb->bytes_per_sector);
        current_read_addr += bpb->sectors_per_cluster * bpb->bytes_per_sector;        
        
        next_cluster = (i == count - 1) ? END_OF_CHAIN : i + 1;   
        write_cluster(fat, clusters[i], next_cluster, temp_ram_area, bpb);
    }
    
    // Update FAT on disk
    // BIOS cant access memory above 1MiB
    memcpy_((uint8_t *)temp_ram_area, (uint8_t *)fat, bpb->sectors_per_fat * bpb->bytes_per_sector);
    update_fat_disk ((uint8_t *)temp_ram_area, bpb);
    
    // Searches an available entry in current directory
    struct Root_Entry *entry = dir;
    for (i = 0; i < bpb->root_entry_count; i++, entry++) {
        if (entry->name[0] == 0 || entry->name[0] == (char)0xE5)
            break;
    }
    
    fat_standard(name, entry->name);
    entry->attributes = ARCHIVE;
    entry->size = 0;
    entry->current_cluster_low = clusters[0];
    update_dir_entry_disk((uint8_t *)dir, bpb, temp_ram_area);   
}


void list_files(struct Root_Entry *dir_entry, uint16_t count) {
    
    char str[20];
    int8_t i, j;
    
    uint8_t hour, min;
    uint8_t mon, day;
    uint16_t year;
    uint16_t entry;
    uint32_t total_size = 0;       
       
    /*
    DIR_Name[0]: the value 0x00 indicates that the entry is not used and in addition, 
    there is no allocated entry after this one 
        https://elm-chan.org/docs/fat_e.html#dir_struct
    */   
    for(entry = 0; entry < count && dir_entry->name[0] != 0x00; entry++, dir_entry++)  {

        if (dir_entry->attributes == ARCHIVE || dir_entry->attributes == DIRECTORY) {
            
            hour = dir_entry->last_mofication_time >> 11;
            min = (dir_entry->last_mofication_time >> 5) & 0x3F;
            
            year = (dir_entry->last_mofication_date >> 9) + 1980;
            mon = (dir_entry->last_mofication_date >> 5) & 0x0F;
            day = dir_entry->last_mofication_date & 0x1F;
            
            // 'j' marks the dot position        
            for (i = FILE_NAME_SIZE - 1, j = 8; i >=0; i--) {
                str[i] = dir_entry->name[i];         
                if (str[i] == ' ')
                    j = i;
            }   
            str[FILE_NAME_SIZE + 1] = '\0';            
            
            if (dir_entry->attributes == ARCHIVE) {            
                str[j++] = '.';
                str[j++] = dir_entry->name[8];
                str[j++] = dir_entry->name[9];
                str[j++] = dir_entry->name[10];
                
                // Pad with spaces
                while(j < FILE_NAME_SIZE + 1)
                    str[j++] = ' '; 
                
                puts_(str);
                printf_("\t%d\t\t%d-%d-%d %d:%d\n",dir_entry->size, year, mon, day, hour, min);
            }
            else { // DIRECTORY               
                puts_(str);
                printf_("\t<DIR>\t\t%d-%d-%d %d:%d\n", year, mon, day, hour, min);
            }
                           
            
            total_size += dir_entry->size; 
            
            if ((entry + 1) % 23 == 0) // paged
                if (getchar() == 'q')
                    break;      
        }
    }
    printf_("\t%d files\t%d bytes\n", entry, total_size);
}


int get_free_clusters(uint8_t *fat, struct BPB *bpb, uint32_t count, uint16_t *clusters) {

    uint16_t *fat16 = (uint16_t *)fat; 
    uint32_t entries = (bpb->sectors_per_fat * bpb->bytes_per_sector) / 2; // Each FAT16 entry has 2 bytes
    uint32_t i, j;

    for(i = 0, j = 0; i < entries && j < count; i++)
        if (fat16[i] == 0) {
            clusters[j++] = i;
            printf_("free cluster: %d\n",i); 
        }
            
    if (j < count) {
        printf_("get_free_clusters: only %d free clusters found. Required: %d\n", j,count);
        return -1;
    }
        
    return 0;
}


/*void copy_file(uint8_t *fat, struct BPB *bpb, struct Root_Entry *dir_entry, char *src, char *dst, uint32_t temp_ram_area) {
    
    uint16_t init_cluster;
    uint32_t fsize;
    uint16_t clusters[100];
    
    init_cluster = find_file(dir_entry, bpb->root_entry_count, src, &fsize);
    if (init_cluster == 0) {
        printf_("File '%s' not found bastard!!!\n", src);
        return;
    }
    
    //get_free_clusters_test(fat, bpb, 5, clusters);
    
    //load_clusters(uint8_t *fat, uint16_t init_cluster, uint16_t count, uint32_t addr, struct BPB *bpb)
    //void write_cluster(uint8_t *fat, uint16_t init_cluster, uint16_t next_cluster, uint32_t addr, struct BPB *bpb) {
}*/

