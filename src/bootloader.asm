.code16      # tell the assembler that we're using 16 bit mode
.intel_syntax noprefix
.global start


#
# FAT16 header
# 

.include "fat16_1GiB_part.h"
#.include "fat16_512MiB_part.h"


#
# Code goes here
#   https://github.com/nanobyte-dev/nanobyte_os/blob/videos/part3/src/bootloader/boot.asm



.equ LOADER_SEGMENT,    0x0000
.equ LOADER_OFFSET,     0x8000

# Also set in include/fat16.h (FAT_ADDR)
.equ FAT_SEGMENT,       0x6000
.equ FAT_OFFSET,        0x0000 

# Also set in include/fat16.h (ROOT_DIR_ADDR)
.equ ROOT_DIR_SEGMENT,  0x8000
.equ ROOT_DIR_OFFSET,   0x0000     

   




### When this code starts, dl has the boot drive number set by BIOS ###

start:
    mov [ebr_drive_number], dl
    
    # setup data segments
    mov ax, 0                   # can't set ds/es directly
    mov ds, ax
    mov es, ax
    
    # setup stack
    mov ss, ax
    mov sp, 0x7C00              # stack grows downwards from where we are loaded in memory

    # some BIOSes might start us at 07C0:0000 instead of 0000:7C00, make sure we are in the
    # expected location
    push es
    push offset after
    retf

after:

    # show loading message
    mov si, offset msg_loading
    call bios_puts

    # read drive parameters (sectors per track and head count),
    # instead of relying on data on formatted disk
    # es:di: 0000h:0000h (Highly recommended to explicitly initialize to zero to prevent bugs on certain BIOSes)
    # assume dl was not modified (set by BIOS with boot drive)
    xor di, di
    mov es, di      # es:di: 0000h:0000h
    push es
    mov ah, 0x08
    int 0x13
    jc drive_error
    pop es

    and cl, 0x3F                        # remove top 2 bits
    xor ch, ch
    mov [bpb_sectors_per_track], cx     # sector count

    inc dh
    mov [bpb_head_side_count], dh       # head count


    # Compute total FAT size in sectors (bpb_fat_count * bpb_sectors_per_fat)
    movzx ax, byte ptr [bpb_fat_count]
    mul word ptr [bpb_sectors_per_fat]  # ax = FAT size in sectors 
    mov [fat_sectors], ax               # store FAT size          
        
    # Reads only the first FAT from disk                               
read_fat_root_dir:
    mov bx, [bpb_sectors_per_fat]       # Retrieves the FAT size in sectors 
    mov [count], bx                     # count: total FAT sectors to read
    
    # Set the initial sector
    mov ax, [bpb_hidden_sector_count]   # Skip hidden sectors (due to partitonated hard disk)
    add ax, [bpb_reserved_sectors]      # Skip bootloader
    
    # Set memory location
    mov bx, FAT_SEGMENT                 #
    mov es, bx                          #
    mov bx, FAT_OFFSET                  # es:bx = buffer

    mov dx, 0                           # LBA high word (ax: LBA low word)
read_one_fat_sector:      
    mov cl, 1                           # read total sectors one-by-one
    cmp word ptr [count], 0             # Were all
    je after_read_fat                   # sectors read ?   
    call disk_read
    inc ax                              # next sector    
    dec word ptr [count]                # update sector count
    add bx, 512                         # 
    jnc read_one_fat_sector    

inc_fat_seg:
    mov cx, es
    add cx, 0x1000
    mov es, cx
    jmp read_one_fat_sector    


    # Compute root directory size in sectors (32 * number_of_entries) / bytes_per_sector)
after_read_fat:
    mov ax, [bpb_root_entry_count]
    shl ax, 5                           # ax *= 32
    div word ptr [bpb_bytes_per_sector] # ax = root directory size in sectors
    mov [root_dir_sectors], ax          # store root directory size

    test dx, dx                         # if dx != 0, add 1 (dx: div remainder)
    jz save_root_dir_sectors
    inc ax                              # division remainder != 0, add 1
                                        # this means we have a sector only partially filled with entries

save_root_dir_sectors:
    mov [count], ax                     # count: total root directory sectors to read

    # Set memory location
    mov bx, ROOT_DIR_SEGMENT            #
    mov es, bx                          #
    mov bx, ROOT_DIR_OFFSET             # es:bx = buffer
    mov cl, 1                           # read total sectors one-by-one
    
    # Set the initial sector
    mov ax, [bpb_hidden_sector_count]   # Skip hidden sectors (due to partitonated hard disk)
    add ax, [bpb_reserved_sectors]      # Skip bootloader
    add ax, [fat_sectors]               # ax points the initial root directory sector on disk
    
    mov dx, 0                           # LBA high word (ax: LBA low word)
read_one_root_dir_sector:      
    cmp word ptr [count], 0             # Were all
    je after_read_root_dir              # sectors read ?   
    call disk_read
    inc ax                              # next sector    
    add bx, 512                         # 
    dec word ptr [count]                # update sector count
    jmp read_one_root_dir_sector 


    # search for loader.bin
after_read_root_dir:
    mov di, ROOT_DIR_OFFSET             # di points the first root directory entry
    xor bx, bx                          # bx: entry count

search_loader:
    mov si, offset file_loader_bin
    mov cx, 11                          
    push di
    repe cmpsb                          # compare up to 11 characters
    pop di
    je found_loader

    add di, 32                          # next directory entry address (32: directory entry size)
    inc bx                              # next entry
    cmp bx, [bpb_root_entry_count]
    jl search_loader

    # loader.bin not found
    jmp loader_not_found_error

found_loader:    

    # di should have the address to the entry
#    mov bx, es:[di + 20]                   # The high 16 bits of first logical cluster field (offset 26)
    mov ax, es:[di + 26]                   # The low 16 bits of first logical cluster field (offset 26)
    mov [current_cluster], ax

    # read loader.bin and process FAT chain
    mov bx, LOADER_OFFSET

load_loader_loop:
        
    # Read current cluster
    mov cx, LOADER_SEGMENT
    mov es, cx
    
    # set dx to point the first FAT data sector on disk
    mov dx, [bpb_hidden_sector_count]   # Due to partitonated hard disk
    add dx, [bpb_reserved_sectors]
    add dx, [fat_sectors]
    add dx, [root_dir_sectors]
    push dx                
    
    mov ax, [current_cluster]
    add ax, -2                          # First two FAT clusters are reserved                         
    movzx cx, byte ptr [bpb_sectors_per_cluster]    
    mul cx                              # Converts current_cluster to sector
    pop cx                              # restore in cx the first FAT data sector on disk 
    add ax, cx                          # points the correspondent sector in FAT data area                   
    jnc call_disk_read
    inc dx                              # in case of carry when adding ax + cx

call_disk_read:
    mov cl, [bpb_sectors_per_cluster]
    call disk_read

    # increase the load memory address
    movzx ax, byte ptr [bpb_sectors_per_cluster]
    mul word ptr [bpb_bytes_per_sector]    
    add bx, ax                          # bx += bpb_sectors_per_cluster * bpb_bytes_per_sector


    # compute location of next cluster (indexes FAT in memory)
next_cluster:    
    mov ax, [current_cluster]
    shl ax, 1              # Convert to word offset. Each FAT16 entry has 2 bytes 
    mov si, FAT_OFFSET
    add si, ax
    mov ax, FAT_SEGMENT
    mov es, ax
    mov ax, word ptr es:[si]            # read entry from FAT table at index ax (memory)
    cmp ax, 0xFFF8                      # end of chain ?
    jae read_finish

    mov [current_cluster], ax           # updates current_cluster
    jmp load_loader_loop                # returns to read the next cluster

read_finish:
    
    # jump to loader

    mov ax, LOADER_SEGMENT              # set segment registers
    mov ds, ax
    mov es, ax

    jmp LOADER_SEGMENT:LOADER_OFFSET



#
# Error handlers
#
drive_error:
#    mov si, msg_read_failed
#    call bios_puts
    jmp $

loader_not_found_error:
    mov si, offset msg_loader_not_found
    call bios_puts
    hlt



#
# Prints a string to the screen
# Params:
#   - ds:si points to string
#
bios_puts:
    # save registers we will modify
    pusha

.loop:
    lodsb               # loads next character in al
    or al, al           # verify if next character is null?
    jz .done

    mov ah, 0x0E        # call bios interrupt
    mov bh, 0           # set page number to 0
    int 0x10

    jmp .loop

.done:
    popa   
    ret



#
# Reads sectors from a disk
# Parameters:
#   - dx:ax: LBA address
#   - cl: number of sectors to read (up to 128)
#   - es:bx: memory address where to store read data
#
disk_read:

    pusha
    
    # set Disk Address Packet (DAP) structure
    mov si, offset dap
    mov [si + 8], ax        # Write to dap.lba (low sector)
    mov [si + 10], dx       # Write to dap.lba (high sector)
    mov [si + 2], cl        # Write to dap.sectors 
    push es
    pop ax
    mov [si + 6], ax        # Write to dap.segment
    mov [si + 4], bx        # Write to dap.offset    

    mov ah, 0x42            # BIOS Extended Read Sectors
    mov dl, [ebr_drive_number]            
    int 0x13                # Call BIOS disk services

    popa
    ret

#
# Disk Address Packet (DAP) structure
#
dap:
    .byte   0x10               # DAP Size (16 bytes)
    .byte   0x00               # Unused (must be 0)
    .word   0x0000             # Number of sectors to read
    .word   0x0000             # Segment 
    .word   0x0000             # Offset 
    .quad   0x0000000000000000 # Starting LBA sector


msg_loading:            .asciz "Loading..."
#msg_read_failed:        .asciz "Read disk failed"
msg_loader_not_found:   .asciz "LOADER.BIN not found"
file_loader_bin:        .ascii "LOADER  BIN"
current_cluster:        .word 0
fat_sectors:            .word 0 # FAT size in sectors
root_dir_sectors:       .word 0 # root directory size in sectors
count:                  .word 0xABCD   # Initial value used as end of code/data mark







