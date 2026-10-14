#include <stdint.h>
#include "file.h"


struct setup_header {
    uint8_t        setup_sects;
    uint16_t    root_flags;
    uint32_t    syssize;
    uint16_t    ram_size;
    uint16_t    vid_mode;
    uint16_t    root_dev;
    uint16_t    boot_flag;
    uint16_t    jump;
    uint32_t    header;
    uint16_t    version;
    uint32_t    realmode_swtch;
    uint16_t    start_sys_seg;
    uint16_t    kernel_version;
    uint8_t     type_of_loader;
    uint8_t     loadflags;
    uint16_t    setup_move_size;
    uint32_t    code32_start;
    uint32_t    ramdisk_image;
    uint32_t    ramdisk_size;
    uint32_t    bootsect_kludge;
    uint16_t    heap_end_ptr;
    uint8_t     ext_loader_ver;
    uint8_t     ext_loader_type;
    uint32_t    cmd_line_ptr;
    uint32_t    initrd_addr_max;
    uint32_t    kernel_alignment;
    uint8_t     relocatable_kernel;
    uint8_t     min_alignment;
    uint16_t    xloadflags;
    uint32_t    cmdline_size;
    uint32_t    hardware_subarch;
    uint64_t    hardware_subarch_data;
    uint32_t    payload_offset;
    uint32_t    payload_length;
    uint64_t    setup_data;
    uint64_t    pref_address;
    uint32_t    init_size;
    uint32_t    handover_offset;
    uint32_t    kernel_info_offset;
} __attribute__((packed));


// Assembly functions on ldlib.asm
void run_linux(uint16_t segment, uint16_t heap_end, uint16_t seg_offset);


// Linux bootloader
int8_t load_linux(uint8_t *fat, struct Root_Entry *root_entry, struct BPB *bpb, char *linux_img, char *ramdiskfs) {
    
    // Linux boot addresses
    const uint32_t KERNEL_PROTECTED_MODE_ADDR   = 0x100000;   
    const uint32_t RAMDISK_IMAGE_ADDR           = 0x20000;
    const uint32_t RUN_LINUX_ADDR               = 0x10000;
    const uint32_t HEAP_END_ADDR                = 0xE000;      
    
    uint16_t init_cluster, clusters2sectors;
    uint32_t fsize;
    uint8_t *load_ptr;

    struct setup_header *kernel_header;
    
    if ( (init_cluster = find_file(root_entry, bpb->root_entry_count, linux_img, &fsize)) ) {                               

        // Read first cluster just to get the setup_sects
        load_clusters(fat, init_cluster, 1, RUN_LINUX_ADDR, bpb);
        kernel_header = (struct setup_header *)(RUN_LINUX_ADDR + 0x1F1); // Last bytes of kernel boot sector
    

        // Read boot sector and kernel setup
        // Linux kernel partionated in 512 bytes sectors
        if ((kernel_header->setup_sects + 1) % (bpb->sectors_per_cluster) != 0) {
            printf_("ERROR loading %s: kernel setup_sects + 1 (%d) not multiple of sectors_per_cluster (%d)\n", linux_img, kernel_header->setup_sects + 1, bpb->sectors_per_cluster); 
            for(;;);       
        }       

        clusters2sectors = (kernel_header->setup_sects + 1) / bpb->sectors_per_cluster;        
      

        init_cluster = load_clusters(fat, init_cluster, clusters2sectors, RUN_LINUX_ADDR, bpb);
        
        // init_cluster now is the first cluster of protected mode kernel code (skiped boot sector and kernel setup)  
     
        // 1.9. Sample Boot Configuartion (https://www.kernel.org/doc/html/v5.6/x86/boot.html)
        kernel_header->type_of_loader = 0xFF; // Special (0xFF = undefined)
        kernel_header->ramdisk_image = RAMDISK_IMAGE_ADDR;
        kernel_header->heap_end_ptr = HEAP_END_ADDR - 0x200;
        kernel_header->loadflags |= 0x80; /* CAN_USE_HEAP */
        kernel_header->cmd_line_ptr = RUN_LINUX_ADDR + HEAP_END_ADDR;
        strcpy_((char *)kernel_header->cmd_line_ptr, "auto"); // Fill in this field even if your boot loader does not support a command line, in which case you can point this to an empty string (or better yet, to the string “auto”.)
        //strcpy_((char *)kernel_header->cmd_line_ptr, "rdinit=/etc/init.d/rc"); // rdinit: Fist process to execute
 
             
        // Start protected mode kernel loading
        load_ptr = (uint8_t *)KERNEL_PROTECTED_MODE_ADDR;
        while(init_cluster < END_OF_CHAIN) {
            // Uses here RAMDISK_IMAGE_ADDR as temporary address,
            // to load kernel above 1MiB (protected mode)
            init_cluster = load_clusters(fat, init_cluster, 1, RAMDISK_IMAGE_ADDR, bpb);
            memcpy_(load_ptr, (uint8_t *)RAMDISK_IMAGE_ADDR, bpb->sectors_per_cluster * bpb->bytes_per_sector);
            load_ptr += bpb->sectors_per_cluster * bpb->bytes_per_sector;
        }
        printf_("%s file loaded!\n",linux_img);
      
    }
    else {
        printf_("%s not found bastard!!!\n", linux_img);
        return -1;    
    }
    

    if ( (init_cluster = find_file(root_entry, bpb->root_entry_count, ramdiskfs, &fsize)) ) {                                           
        
        kernel_header->ramdisk_size = fsize;
       
        // Start ramdiskfs loading
        load_ptr = (uint8_t *)RAMDISK_IMAGE_ADDR;
        while(init_cluster < END_OF_CHAIN) {
            init_cluster = load_clusters(fat, init_cluster, 1, (uint32_t)load_ptr, bpb);
            load_ptr += bpb->sectors_per_cluster * bpb->bytes_per_sector;
        }
        printf_("%s file loaded!\n",ramdiskfs);
    }
    else {
        printf_("%s not found bastard!!!\n", ramdiskfs);
        return -1;    
    }
    
    
    // 1.12. Running the Kernel (https://www.kernel.org/doc/html/v5.6/x86/boot.html)
    // The kernel is started by jumping to the kernel entry point, 
    // which is located at segment offset 0x20 from the start of the real mode kernel.
    uint16_t segment, offset;
    linear2segmented(RUN_LINUX_ADDR, &segment, &offset);
    run_linux(segment, HEAP_END_ADDR, 0x20);

    return 0;
}
