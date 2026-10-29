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


// Return the file directory entry or NULL if not found
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

    for (i = 0, j = 0; i < entries && j < count; i++)
        if (fat16[i] == 0) 
            clusters[j++] = i;
        
            
    if (j < count) {
        printf_("get_free_clusters: only %d free clusters found. Required: %d\n", j,count);
        return -1;
    }
        
    return 0;
}


void copy_file(uint8_t *fat, struct BPB *bpb, uint16_t current_dir_cluster, struct Root_Entry *dir_entry, char *src, char *dst, uint32_t temp_ram_area) {
    
    uint16_t next_cluster_read, next_cluster_write;
    uint16_t clusters[100];
    uint16_t cluster_count;
    struct Root_Entry *file_entry, *search_dir;
    uint16_t i;
    uint32_t fsize;
    uint16_t *fat16 = (uint16_t *)fat;
    char fat_std[50];
    char token[50], *next;
    
    
    
    /*** Searches for file entry on source directory ***/
    
    strcpy_(token, strtok_(src, '/')); // Initialize tokenizer
    search_dir = dir_entry;
    next_cluster_read = current_dir_cluster;
    

    while ((next = strtok_(0, '/'))) {
        printf_("search for '%s'\n",token);
        fat_standard(upper_case(token), fat_std);
        search_dir = find_file(search_dir, bpb->root_entry_count, fat_std);
        printf_("search_dir: %X\n",search_dir);        
        next_cluster_read = search_dir->current_cluster_low;
        printf_("next_cluster_read: %X\n",next_cluster_read);
        load_file(fat, next_cluster_read, temp_ram_area, bpb, temp_ram_area);
        search_dir = (struct Root_Entry *)temp_ram_area;
        strcpy_(token, next);
    }
    printf_("found source directory: %X\n",search_dir);
    printf_("next_cluster_read: %X\n",next_cluster_read);
    printf_("token: %s\n", token);
    printf_("pwd_top: %d\n", pwd_top());
    pwd_print();
    puts_("\n");
           
    fat_standard(upper_case(token), fat_std);
    
    file_entry = find_file(search_dir, bpb->root_entry_count, fat_std);
    if (file_entry == 0) {
        printf_("File '%s' not found bastard!!!\n", token);
        return;
    }
    
    if (file_entry->attributes != ARCHIVE) {
        printf_("'%s' is not a file bastard!!!\n", token);
        return;    
    }
    
    // Computes the file size in clusters
    fsize = file_entry->size;
    cluster_count = ((fsize / bpb->bytes_per_sector) + 1) / bpb->sectors_per_cluster + 1;
    get_free_clusters(fat, bpb, cluster_count, clusters);
    
    // Copy file cluster by cluster and update FAT on RAM
    for (i = 0, next_cluster_read = file_entry->current_cluster_low; i < cluster_count; i++) {
        next_cluster_read = load_clusters (fat, next_cluster_read, 1, temp_ram_area, bpb);      
        next_cluster_write = (i == cluster_count - 1) ? END_OF_CHAIN : clusters[i + 1];     
        write_cluster (clusters[i], temp_ram_area, bpb);
        
        // Set next cluster on FAT
        fat16[clusters[i]] = next_cluster_write; 
    }

  
    update_fat_disk (fat, bpb, temp_ram_area);
  
   
    /*** Searches for file entry on destination directory ***/
    
    strcpy_(token, strtok_(dst, '/')); // Initialize tokenizer
    search_dir = dir_entry;
    next_cluster_read = current_dir_cluster;      // Start indicating root directory
    
    while ((next = strtok_(0, '/'))) {
        //printf_("search for '%s'\n",token);
        fat_standard(upper_case(token), fat_std);
        search_dir = find_file(search_dir, bpb->root_entry_count, fat_std);
        //printf_("search_dir: %X\n",search_dir);        
        next_cluster_read = search_dir->current_cluster_low;
        //printf_("next_cluster_read: %X\n",next_cluster_read);
        load_file(fat, next_cluster_read, temp_ram_area, bpb, temp_ram_area);
        search_dir = (struct Root_Entry *)temp_ram_area;
        strcpy_(token, next);
    }
    printf_("found destination directory: %X\n",search_dir);
    printf_("next_cluster_read: %X\n",next_cluster_read);
    printf_("root directory: %X\n", dir_entry);
    printf_("token: %s\n", token);


    // Find entry on destination directory
    file_entry = search_dir;
    for (i = 0; i < bpb->root_entry_count; i++, file_entry++) {
        if (file_entry->name[0] == 0 || file_entry->name[0] == (char)0xE5)
            break;
    }
    
    if (i == bpb->root_entry_count) {
        puts_("ERROR: no entry available in the current directory\n");
        return;
    }
    
    
    // Set file attributes
    fat_standard(upper_case(token), file_entry->name);
    file_entry->attributes = ARCHIVE;
    file_entry->size = fsize;
    file_entry->current_cluster_low = clusters[0];
    printf_("search_dir: %X\n",search_dir);
    
    update_dir_entry_disk((uint8_t *)search_dir, bpb, next_cluster_read, temp_ram_area);
}

