SECTIONS
{
    . = 0x7C00;     /* Set the start address of section .text */
    .text : {
        *(.text)
        FILL(0x00);        
        . = 510;    /* Forward the location counter to the end of BOOTSTRAP to store the valid mark (0x55AA) as the last two sector bytes */ 
        BYTE(0x55)  /* Valid BOOTSTRAP */
        BYTE(0XAA)  /* mark */
        
        BYTE(0xF8) /* FAT16 mark */
        BYTE(0xFF)
        BYTE(0xFF)
        BYTE(0xFF)
    }    
}
