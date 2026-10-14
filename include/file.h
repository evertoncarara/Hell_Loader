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

uint16_t find_file(struct Root_Entry *root_entry, uint16_t count, char *name, uint32_t *size) {
    
    uint16_t found = 1;  
    
    upper_case(name);
    
    for(uint16_t entry = 0; entry < count; entry++, root_entry++)  {
        
        found = 1;     
        for(uint8_t c = 0; c < FILE_NAME_SIZE; c++) {
            if (root_entry->name[c] != name[c]) {
                found = 0;
                break;
            }
        }
        if (found) {
            *size = root_entry->size;
            return root_entry->current_cluster_low;            
        }
    }

    return found;    
}

int load_file(uint8_t *fat, uint16_t init_cluster, uint32_t addr, struct BPB *bpb, uint32_t temp_load_area) {

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
        
        current_cluster = load_clusters(fat, current_cluster, 1, temp_load_area, bpb);
        
        // BIOS cant access memory above 1MiB
        memcpy_((uint8_t *)current_load_addr, (uint8_t *)temp_load_area, bpb->sectors_per_cluster * bpb->bytes_per_sector);
       
        current_load_addr += bpb->sectors_per_cluster * bpb->bytes_per_sector;        
    }
    
    return 0;
}


void list_files(struct Root_Entry *root_entry, uint16_t count) {
    
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
    for(entry = 0; entry < count && root_entry->name[0] != 0x00; entry++, root_entry++)  {

        if (root_entry->attributes == ARCHIVE || root_entry->attributes == DIRECTORY) {
            
            hour = root_entry->last_mofication_time >> 11;
            min = (root_entry->last_mofication_time >> 5) & 0x3F;
            
            year = (root_entry->last_mofication_date >> 9) + 1980;
            mon = (root_entry->last_mofication_date >> 5) & 0x0F;
            day = root_entry->last_mofication_date & 0x1F;
            
            // 'j' marks the dot position        
            for (i = FILE_NAME_SIZE - 1, j = 8; i >=0; i--) {
                str[i] = root_entry->name[i];         
                if (str[i] == ' ')
                    j = i;
            }   
            str[FILE_NAME_SIZE + 1] = '\0';            
            
            if (root_entry->attributes == ARCHIVE) {            
                str[j++] = '.';
                str[j++] = root_entry->name[8];
                str[j++] = root_entry->name[9];
                str[j++] = root_entry->name[10];
                
                // Pad with spaces
                while(j < FILE_NAME_SIZE + 1)
                    str[j++] = ' '; 
                
                puts_(str);
                printf_("\t%d\t\t%d-%d-%d %d:%d\n",root_entry->size, year, mon, day, hour, min);
            }
            else { // DIRECTORY               
                puts_(str);
                printf_("\t<DIR>\t\t%d-%d-%d %d:%d\n", year, mon, day, hour, min);
            }
                           
            
            total_size += root_entry->size; 
            
            if ((entry + 1) % 23 == 0) // paged
                if (getchar() == 'q')
                    break;      
        }
    }
    printf_("\t%d files\t%d bytes\n", entry, total_size);
}

