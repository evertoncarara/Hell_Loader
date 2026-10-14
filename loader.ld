SECTIONS
{
    .text : {
        build/ldlib.o(.text)
        build/loader.o(.text)
      /*  *(.text.*)  */
    }
    
    .data : {
        *(.data)
    }
    
    .rodata : {
      *(.rodata*)
    }
    
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    
     
    /* Explicitly discard standard sections I don't want */
    /DISCARD/ :
    {        
        *(.eh_frame)  /* Exclude exception handling frames */
        *(.comment)   /* Exclude compiler version strings */
    } /* As these section were exluced, they will not be dumped by: objcopy -O binary*/
    
}
