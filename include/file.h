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
       
       
    for(entry = 0; entry < count; entry++, dir_entry++)  {
    
        /*
            DIR_Name[0]: the value 0x00 indicates that the entry is not used and in addition, 
            there is no allocated entry after this one. The value 0xE5 indicates a removed entry. 
                https://elm-chan.org/docs/fat_e.html#dir_struct
        */
        if (dir_entry->name[0] == 0x00 || dir_entry->name[0] == (char)0xE5)
            continue;

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
        if (fat16[i] == 0) // free ?
            clusters[j++] = i;
        
            
    if (j < count) {
        printf_("get_free_clusters: only %d free clusters found. Required: %d\n", j,count);
        return -1;
    }
        
    return 0;
}


// Follows a directory path tokenizing it
//  Return:
//          - directory 'entry' pointer
//              - 'entry' points the file directory if file name is specified
//              - 'entry' points the parent directory of specified directory, when file name is not specified
//          - last 'token'' of string path
//          - 'last_cluster' correspondig to directory 'entry' pointer
struct Root_Entry *follow_path (uint8_t *fat, struct BPB *bpb, struct Root_Entry *current_dir_entry, uint16_t current_dir_cluster, char *name, uint32_t temp_ram_area, char *token, uint16_t *last_cluster) {

    uint16_t aux_cluster;
    struct Root_Entry *entry;
    char fat_std[50], *next;

    // Set the initial directory for file search
    if (current_dir_cluster == 0 || name[0] == '/') {    // current_dir_cluster == 0: current directory is root 
        load_root_dir((uint8_t *)temp_ram_area, bpb, temp_ram_area);
        entry = (struct Root_Entry *)temp_ram_area;
        aux_cluster = 0;
    }
    else {// current directory
        entry = current_dir_entry;
        aux_cluster = current_dir_cluster;    
    }

    // Follows the directory path to find the source/destination name
    strcpy_(token, strtok_(name, "/")); // Initialize tokenizer
    while ((next = strtok_(NULL, "/"))) {

        // Skip "./" on root directory
        if (!strcmp_(token,".") && current_dir_cluster == 0) {
            strcpy_(token, next);
            continue;
        }
        
        // Here 'token' should be a directory name
        fat_standard(upper_case(token), fat_std);
        entry = find_file(entry, bpb->root_entry_count, fat_std);
        
        if (entry == 0) {
            printf_("'%s' not found bastard!!!\n", token);
            return NULL;
        }
        
        if (entry->attributes != DIRECTORY) {
            printf_("'%s' is not a directory bastard!!!\n", token);
            return NULL;
        }
        
        aux_cluster = entry->current_cluster_low;
        
        // Load the next directory
        if (aux_cluster == 0) // root 
            load_root_dir((uint8_t *)temp_ram_area, bpb, temp_ram_area);
        else // ordinary directory
            load_file(fat, aux_cluster, temp_ram_area, bpb, temp_ram_area);
        
        // Point the loaded directory
        entry = (struct Root_Entry *)temp_ram_area;
        strcpy_(token, next);
    }
    
    // Return the last loaded cluster
    *last_cluster = aux_cluster;   
     return entry;
}

void copy_file (uint8_t *fat, struct BPB *bpb, struct Root_Entry *current_dir_entry, uint16_t current_dir_cluster, char *src, char *dst, uint32_t temp_ram_area) {
    
    uint16_t next_cluster_read, next_cluster_write, last_cluster;
    uint16_t clusters[50];
    uint16_t cluster_count;
    struct Root_Entry *entry, *dst_dir_entry, *aux_entry;
    uint16_t i;
    uint32_t src_fsize;
    uint16_t *fat16 = (uint16_t *)fat;
    char fat_std[50];
    char token[50], dst_name[12], src_file_name[12];


    /*** Searches for source file entry on source directory ***/

    entry = follow_path (fat, bpb, current_dir_entry, current_dir_cluster, src, temp_ram_area, token, &last_cluster);
    
    // Here 'token' must be the source file name
    fat_standard(upper_case(token), fat_std);
    strcpy_(src_file_name, token);
    
    // Searches the file in source directory
    entry = find_file(entry, bpb->root_entry_count, fat_std);
    if (entry == 0) {
        printf_("File '%s' not found bastard!!!\n", token);
        return;
    }
    
    if (entry->attributes != ARCHIVE) {
        printf_("'%s' is not a file bastard!!!\n", token);
        return;    
    }
    
    // Save file information because 'temp_ram_area', pointed by 'entry', will be modified below
    src_fsize = entry->size;
    next_cluster_read = entry->current_cluster_low;
    

    
    
    /*** Searches for destination file entry on destination directory ***/
    
    uint8_t dst_root_dir = 0;
    if (!strcmp_(dst, "/")) // destination directory: root
        dst_root_dir = 1;
    
    entry = follow_path (fat, bpb, current_dir_entry, current_dir_cluster, dst, temp_ram_area, token, &last_cluster);        
    
    // Here 'token' is a file or directory name
    strcpy_(dst_name, token);    
    printf_("dst_name: %s\n",dst_name);
    uint16_t dst_cluster = last_cluster;
      
    // If 'dst_name' is a file: 'entry' points the destination directory entry and 'dst_cluster' the correspondig cluster
    // If 'dst_name' is a directory (destination): 'entry' points its parent directory entry and 'dst_cluster' the correspondig cluster
    
    if (!strcmp_(dst_name, ".") && dst_cluster == 0) // destination directory: root
        dst_root_dir = 1;    
    
    fat_standard(upper_case(dst_name), fat_std);
    aux_entry = find_file(entry, bpb->root_entry_count, fat_std);
      
    if (aux_entry != 0 && aux_entry->attributes == ARCHIVE) { // destination file specified
        printf_("'%s' already exist on destination directory!\n",dst_name);
        puts_("Aborting copy.\n");
        return;    
    }
       
    if ((aux_entry != 0 && aux_entry->attributes == DIRECTORY) || dst_root_dir) { 
    
        // dst_name is parent directory
       
        // Load the destination directory 
        if (dst_root_dir || (aux_entry != 0 && aux_entry->current_cluster_low == 0)) {
            puts_("Destination is a directory: root\n");
            dst_cluster = 0;
            load_root_dir((uint8_t *)temp_ram_area, bpb, temp_ram_area);            
        }
        else { // Ordinary directory
            printf_("Destination is a directory: '%s'\n",dst_name);
            dst_cluster = aux_entry->current_cluster_low;
            load_file(fat, aux_entry->current_cluster_low, temp_ram_area, bpb, temp_ram_area);
        }            

        aux_entry = (struct Root_Entry *)temp_ram_area; 
        
        printf_("dst_cluster: %X\n",dst_cluster);
        printf_("aux_entry->current_cluster_low: %X\n",aux_entry->current_cluster_low);
        printf_("Searching for '%s' in directory '%s'\n",src_file_name, dst_name);
        
        // Verifies if the source file already exists in the destination directory
        fat_standard(upper_case(src_file_name), fat_std);
        entry = find_file(aux_entry, bpb->root_entry_count, fat_std);
        if (entry != 0 && entry->attributes == ARCHIVE) {
            printf_("'%s' already exist on destination directory!\n",src_file_name);
            puts_("Aborting copy.\n");
            return;    
        }
        
        // Add source file name when only destination directory is specified
        strcpy_(dst_name, src_file_name);        
        
    }
    else // dst_name not found
        printf_("'%s' doest not exist on destination directory\n",dst_name);
    
        
        
    // Here 'dst_cluster' has the destination directory cluster
    


    /*** Start file copying ***/
    
    puts_("Copying...\n");
    
    // Computes the file size in clusters
    cluster_count = ((src_fsize / bpb->bytes_per_sector) + 1) / bpb->sectors_per_cluster + 1;
    get_free_clusters(fat, bpb, cluster_count, clusters);
    
    // Copy file cluster by cluster and update FAT on RAM
    for (i = 0; i < cluster_count; i++) {
        next_cluster_read = load_clusters (fat, next_cluster_read, 1, temp_ram_area, bpb);      
        next_cluster_write = (i == cluster_count - 1) ? END_OF_CHAIN : clusters[i + 1];     
        write_cluster (clusters[i], temp_ram_area, bpb);
        
        // Set next cluster on FAT
        fat16[clusters[i]] = next_cluster_write; 
    }

    update_disk_fat (fat, bpb, temp_ram_area);
  
  
  
    /*** Add the file name in destination directory ***/
    
    if (dst_cluster == 0) // destination directory is root directory
        load_root_dir((uint8_t *)temp_ram_area, bpb, temp_ram_area);
    else
        load_file(fat, dst_cluster, temp_ram_area, bpb, temp_ram_area);
    
    dst_dir_entry = (struct Root_Entry *)temp_ram_area;
    
    // Find entry on destination directory
    for (i = 0, entry = dst_dir_entry; i < bpb->root_entry_count; i++, entry++) {
        if (entry->name[0] == 0 || entry->name[0] == (char)0xE5)
            break;
    }
    
    if (i == bpb->root_entry_count) {
        puts_("ERROR: no entry available in the current directory\n");
        return;
    }
    
    // Set destination file attributes
    fat_standard(upper_case(dst_name), entry->name);
    entry->attributes = ARCHIVE;
    entry->size = src_fsize;
    entry->current_cluster_low = clusters[0];
    
    update_disk_dir ((uint8_t *)dst_dir_entry, bpb, dst_cluster, temp_ram_area);
    
    // Update current directory on RAM
    if (dst_cluster == current_dir_cluster)
        memcpy_((uint8_t *)current_dir_entry, (uint8_t *)temp_ram_area, bpb->root_entry_count * 32); 
}


void make_dir (uint8_t *fat, struct BPB *bpb, struct Root_Entry *current_dir_entry, uint16_t current_dir_cluster, char *name, uint32_t temp_ram_area) {
    
    uint16_t dst_cluster;
    struct Root_Entry *entry;
    uint16_t i;
    uint16_t *fat16 = (uint16_t *)fat;
    char fat_std[50];

    // Verifies if the directory already exist in current directory    
    fat_standard(upper_case(name), fat_std);
    entry = find_file(current_dir_entry, bpb->root_entry_count, fat_std);
    if (entry != 0 && entry->attributes == DIRECTORY) {
        printf_("'%s' already exist bastard!!!\n",name);
        return;    
    }    
    
    get_free_clusters(fat, bpb, 1, &dst_cluster);
    
    // Set next cluster on FAT
    fat16[dst_cluster] = END_OF_CHAIN; 
    
    // Set the sub-directory special entries (dot entries)
    load_file(fat, dst_cluster, temp_ram_area, bpb, temp_ram_area);
    entry = (struct Root_Entry *)temp_ram_area;

    // Current directory entry
    strcpy_(entry->name, ".          ");
    entry->attributes = DIRECTORY;   
    entry->current_cluster_low = dst_cluster;
    entry++;
    
    // Parent directory entry
    strcpy_(entry->name, "..         ");
    entry->attributes = DIRECTORY;   
    entry->current_cluster_low = current_dir_cluster;
    
    // Update current directory on disk
    entry = (struct Root_Entry *)temp_ram_area;
    update_disk_dir ((uint8_t *)entry, bpb, dst_cluster, temp_ram_area);
        
    // Find entry in current directory
    for (i = 0, entry = current_dir_entry; i < bpb->root_entry_count; i++, entry++) {
        if (entry->name[0] == 0 || entry->name[0] == (char)0xE5)
            break;
    }
    
    if (i == bpb->root_entry_count) {
        puts_("ERROR: no entry available in the current directory\n");
        return;
    }
    
    // Set the new directory attributes
    fat_standard(upper_case(name), entry->name);
    entry->attributes = DIRECTORY;    
    entry->size = 0;    
    entry->current_cluster_low = dst_cluster;
    
    // Update current directory on disk
    update_disk_dir ((uint8_t *)current_dir_entry, bpb, current_dir_cluster, temp_ram_area);
}


void remove_file (uint8_t *fat, struct BPB *bpb, struct Root_Entry *current_dir_entry, uint16_t current_dir_cluster, char *name, uint32_t temp_ram_area) {
    
    struct Root_Entry *file_entry, *target_dir_entry;
    uint16_t *fat16 = (uint16_t *)fat;
    uint16_t next_cluster, free_cluster, target_cluster;
    char fat_std[50];
    char token[50];


    /*** Searches for target file entry on source directory ***/

    target_dir_entry = follow_path (fat, bpb, current_dir_entry, current_dir_cluster, name, temp_ram_area, token, &target_cluster);
    
    // Here 'token' must be the target file name
    fat_standard(upper_case(token), fat_std);
    
    // Searches the file in directory
    file_entry = find_file(target_dir_entry, bpb->root_entry_count, fat_std);
    if (file_entry == 0) {
        printf_("File '%s' not found bastard!!!\n", token);
        return;
    }
    
    if (file_entry->attributes != ARCHIVE) {
        printf_("'%s' is not a file bastard!!!\n", token);
        return;    
    }
    
    printf_("delete '%s'\n",file_entry->name);
    // To remove a file, set 0xE5 to the DIR_Name[0] to free the entry.
    //  https://elm-chan.org/docs/fat_e.html#dir_struct
    file_entry->name[0] = 0xE5;
    printf_("delete '%s'\n",file_entry->name);
    printf_("file_entry->current_cluster_low %X\n",file_entry->current_cluster_low);
    printf_("target_cluster: %X\n",target_cluster);
    printf_("target_dir_entry: %X\n",target_dir_entry);
    
    // Update current directory on disk
    update_disk_dir ((uint8_t *)target_dir_entry, bpb, target_cluster, temp_ram_area);
     
    // Update current directory on RAM
    if (target_cluster == current_dir_cluster)
        memcpy_((uint8_t *)current_dir_entry, (uint8_t *)target_dir_entry, bpb->root_entry_count * 32); 
        
    free_cluster = file_entry->current_cluster_low;
    do {
        next_cluster = fat16[free_cluster];
        printf_("free_cluster: %X\n",free_cluster);
        fat16[free_cluster] = 0;
        free_cluster = next_cluster; 
    } while(next_cluster < END_OF_CHAIN);

    
    update_disk_fat (fat, bpb, temp_ram_area);   
}









