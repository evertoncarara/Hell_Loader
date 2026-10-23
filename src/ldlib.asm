
### SP was not set for protected mode. Still using the real mode address because real mode bios interrupts are called from protected mode

### word = 16 bits!!!! ###

### This code is position independent ###

.code16      # tell the assembler that we're using 16 bit mode
.intel_syntax noprefix
.global entry
.global bios_extended_read_drive
.global bios_extended_write_drive
.global wait_for_key
.global kbhit
.global get_key
.global cpu_reset
.global run_linux
.global run_dos
.global get_mmap
.global get_time
.global set_time


#
# MACRO: Switch to 16-bit real mode
#
.macro ENTER_REAL_MODE

# By appending _\@ to your label name, GAS creates a globally unique identifier 
# every time the macro expands (e.g., loop_0, loop_1)

    call get_eip_\@
get_eip_\@:
    pop ebx                  # ebx has the ip address pointing to the get_eip label
      
    
    # Absolute indirect far jump from the stack jump from the stack!
    # Jump to 16-bit protected mode segment (GDT offset: 0x18)                               
    push 0x18               # Set the GDT segment descriptor (16-bits Code Descriptor)
    lea eax, [ebx + (returning2reality_\@ - get_eip_\@)]  # eax points the returning2reality label
    push eax                # Set the offset in GDT segment
    retf                    # Pops both values and performs a far jump (jmp far 0x18:returning2reality)    
                            # Jump to returning2reality label
  
  
#
# 16-bit protected mode
#
returning2reality_\@:
.code16

    # Disable protected mode bit in cr0
    mov eax, cr0
    and al, ~1
    mov cr0, eax
     
    # Absolute indirect far jump from the stack jump from the stack!                                
    # Jump to real mode
    push 0x00           # Set CS = 0 (in real mode cs = 0)
    lea ax, [bx + (back2reality_\@ - get_eip_\@)] # ax points the back2reality label
    push ax             # Set the offset in CS (target label)
    retf                # Pops both values and performs a far jump (jmp far 0x00:back2reality)
                        # Jump to back2reality label
     
    
back2reality_\@:

    # Setup segments for real mode
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    # Enable interrupts
    sti
     
.endm



#
# MACRO: Switch to 32-bit protected_mode
#
.macro ENTER_PROTECTED_MODE

    cli

    # Set Protection Enable bit in Control Register 0
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

# By appending _\@ to your label name, GAS creates a globally unique identifier 
# every time the macro expands (e.g., loop_0, loop_1)
 
    call get_ip_\@
get_ip_\@:
    pop bx                  # bx has the ip address pointing to the get_ip label

    # Execute absolute indirect far jump to 32-bits code (protected mode) from the stack!
    # Jump to 32-bit protected mode segment (GDT offset: 0x08) 
    push 0x08           # Set the GDT segment descriptor (32-bits Code Descriptor)
    lea ax, [bx + (back2protected_\@ - get_ip_\@)]    # ax points the back2protected label
    push ax             # Set the offset in GDT segment
    retf                # Pops both values and performs a far jump (jmp far 0x08:back2protected)
                        # Jump to back2protected label

back2protected_\@:
.code32
    # Update data segment registers with data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax 
    mov ss, ax
    


.endm



entry:

# Gets the ip (instruction pointer) to compute variables absolute address in runtime (relative addressing not supported)
    call get_ip
get_ip:
    pop bx                  # bx has the ip address pointing to the get_ip label

#
#  Absloute address is computed in runtime: bx + (target - get_ip)   
#

#
# Entering the realms of x86 protected mode...
#
    # Disable interrupts during transition
    cli

    # Setup initial real mode segments
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    
    # Hide the Cursor 
    mov ah, 0x01
    mov ch, 32    # Line out of screen
    int 0x10
    
    # Enable A20 gate (support for high memory addresses starting at 0x100000)
    # BIOS INT 0x15 Function 2401 - Enable A20 (https://wiki.osdev.org/A20_Line)
    mov ax, 0x2401 
    int 0x15

    # Load Global Descriptor Table (GDT) using relative addressing
    lea ax, [bx + (gdt_start - get_ip)]         # ax points the gdt_start                    
    lea di, [bx + (gdt_descriptor - get_ip)]    # di points the gdt_descriptor 
    mov [di + 2], ax                # Set the second 'field' of gdt_descriptor (gdt_start)
    lgdt [di]

    ENTER_PROTECTED_MODE

    call main               # C code application
 
    jmp $



#
# Returns AL = 1 if a key is waiting, AL = 0 otherwise
#   (https://wiki.osdev.org/I8042_PS/2_Controller)
kbhit:
    in al, 0x64             # Read the PS/2 controller status
    and al, 0x01            # Check if the output buffer is full
    ret 


#
# Read the scancode from the data port
#
get_key:    
    in al, 0x60             
    ret 

#
# CPU reset (https://wiki.osdev.org/I8042_PS/2_Controller)
#
cpu_reset:
.code32
 
wait_empty:
    in   al, 0x64           # Read the PS/2 controller status
    test al, 2              # Wait for a empty Input Buffer
    jne  wait_empty
  
    mov  al, 0xFE           #Send 0xFE to the keyboard controller.
    out  0x64, al


#
# Detects the memory of a PC is by using the INT 0x15, EAX = 0xE820
#   https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15,_EAX_=_0xE820
get_mmap:
.code32
    push ebp
    mov  ebp, esp

    push ecx
    push edx
    push ebx
    push esp
    push esi
    push edi 

    ENTER_REAL_MODE

    mov di, [ebp + 8]       #
    
	mov si, 0               # read entry count
    mov ebx, 0              # ebx must be 0 to start    

read_entry:	
    cmp si, word ptr[ebp + 12]
	je end_loop

	mov edx, 0x0534D4150	# Place "SMAP" into edx
	mov eax, 0xE820         # Function 0xE820 to get memory map
	mov dword ptr es:[di + 20], 1	# force a valid ACPI 3.X entry | allows us to get additional information (extended attributes)
	mov ecx, 24		        # ask for 24 bytes
	int 0x15
    inc si                  # updata entry count (the CF flag is not affected by inc)
    
    jc end_list	            # carry set after int 0x15 means "end of list already reached"
                       	
	mov edx, 0x0534D4150	# Some BIOSes apparently trash this register?
	cmp eax, edx		    # on success, eax must have been reset to "SMAP"
	jne fail
		
	add di, 24              # next entry
	
	test ebx, ebx		    # if ebx resets to 0, list is complete
	je end_list		        # 
	
	jmp read_entry
	
end_list:	
end_loop:	
	mov ax, 0
	jmp return
	
fail:
    mov ax, -1
    jmp return
   
return:
    mov bx, [ebp + 16]
    mov word ptr[bx], ax
    
    ENTER_PROTECTED_MODE
    
    mov ax, si
    
    pop edi 
    pop esi
    pop esp
    pop ebx
    pop edx
    pop ecx
    
    pop ebp
    ret 


#    
# Get current time
#
get_time:
.code32
    push ebp
    mov  ebp, esp
    pusha                   # Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
   
    ENTER_REAL_MODE 
       
    mov ah, 0x02            # Function 02h: Get Real-Time Clock Time
    int 0x1A                # Call BIOS time service 
    
    mov bx, [ebp + 8]       #
    mov byte ptr [bx], ch   # Hour
    mov bx, [ebp + 12]      #
    mov byte ptr [bx], cl   # Minute
    mov bx, [ebp + 16]      #
    mov byte ptr [bx], dh   # Second  
    
    ENTER_PROTECTED_MODE
    
    popa                    # Restore EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
    pop ebp
    ret 


#    
# Set time
#
set_time:
.code32
    push ebp
    mov  ebp, esp
    pusha                   # Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
   
    ENTER_REAL_MODE 
       
    mov ch, [ebp + 8]       # Hour
    mov cl, [ebp + 12]      # Minute
    mov dh, [ebp + 16]      # Second
    
    
    mov ah, 0x03            # BIOS function: Set RTC Time
    int 0x1A                # Call BIOS time service 
       
    ENTER_PROTECTED_MODE
    
    popa                    # Restore EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
    pop ebp
    ret 




#    
# Reads one or more disk sectors into a buffer
#
bios_extended_read_drive:
.code32
    push ebp
    mov  ebp, esp
    pusha                   # Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
   
    ENTER_REAL_MODE 
   
# Gets the ip (instruction pointer) to compute variables absolute address in runtime (relative addressing not supported)
    call get_ip_brd
get_ip_brd:
    pop bx                  # bx has the ip address pointing to the get_ip label    
    
    lea si, [bx + (dap - get_ip_brd)]         # ds:si points to the Disk Address Packet
    mov ax, [ebp + 8]       # LBA sector high word
    mov [si + 10], ax       # Write to dap.lba
    mov ax, [ebp + 12]      # LBA sector low word
    mov [si + 8], ax        # Write to dap.lba                       
    mov ax, [ebp + 16]      # Number of sectors to read
    mov [si + 2], ax        # Write to dap.sectors 
    mov ax, [ebp + 20]      # Target segment
    mov [si + 6], ax        # Write to dap.segment
    mov ax, [ebp + 24]      # Buffer Address Pointer
    mov [si + 4], ax        # Write to dap.offset

    
    mov ah, 0x42            # BIOS Extended Read Sectors
    mov dl, 0x80            # Drive number (0x80 for first hard drive)
    int 0x13                # Call BIOS disk services
    
    ENTER_PROTECTED_MODE
    
    popa                    # Restore EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
    pop ebp
    ret 
    
    
#    
# Write to disk sectors
#
bios_extended_write_drive:
.code32
    push ebp
    mov  ebp, esp
    pusha                   # Save EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
   
    ENTER_REAL_MODE 
   
# Gets the ip (instruction pointer) to compute variables absolute address in runtime (relative addressing not supported)
    call get_ip_bwd
get_ip_bwd:
    pop bx                  # bx has the ip address pointing to the get_ip label    
    
    lea si, [bx + (dap - get_ip_bwd)]         # ds:si points to the Disk Address Packet
    mov ax, [ebp + 8]       # LBA sector high word
    mov [si + 10], ax       # Write to dap.lba
    mov ax, [ebp + 12]      # LBA sector low word
    mov [si + 8], ax        # Write to dap.lba                       
    mov ax, [ebp + 16]      # Number of sectors to read
    mov [si + 2], ax        # Write to dap.sectors 
    mov ax, [ebp + 20]      # Target segment
    mov [si + 6], ax        # Write to dap.segment
    mov ax, [ebp + 24]      # Buffer Address Pointer
    mov [si + 4], ax        # Write to dap.offset

    
    mov ah, 0x43            # BIOS Extended Read Sectors
    mov dl, 0x80            # Drive number (0x80 for first hard drive)
    int 0x13                # Call BIOS disk services
    
    ENTER_PROTECTED_MODE
    
    popa                    # Restore EAX, ECX, EDX, EBX, ESP, EBP, ESI, and EDI
    pop ebp
    ret 
        

#    
# Execute real mode kernel
#   1.12. Running the Kernel (https://www.kernel.org/doc/html/v5.6/x86/boot.html)
run_linux:
.code32
    push ebp
    mov  ebp, esp
   
    ENTER_REAL_MODE 
    
    cli                     # Enter with interrupts disabled!
    
    /* Set up the real-mode kernel stack */
    mov ax, [ebp + 8]       # Segment
    mov bx, [ebp + 12]      # HEAP_END_ADDR
    mov cx, [ebp + 16]      # Segment offset
    mov ss, ax
    mov sp, bx
    
    # At entry, ds = es = ss should point to the start of the real-mode kernel code,
    # to guard against bugs in the kernel, it is recommended that the boot loader sets fs = gs = ds = es = ss.
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    /* Run the kernel */
    add ax, cx          # Segment + segment offset
    push ax
    mov ax, 0x0000
    push ax
    retf                # jmp ax:0x0000
    




run_dos:
.code32
    push ebp
    mov  ebp, esp
   
    ENTER_REAL_MODE    
    
    /* Set up the real-mode kernel stack */
    mov ch, [ebp + 8]       # Media type
    mov dl, [ebp + 12]      # Drive number
    mov ax, [ebp + 16]      # First data sector (high word)
    mov bx, [ebp + 20]      # First data sector (high word)
    
    sti                     # Enter with interrupts enabled!
    
    /* Run the kernel */
    jmp 0x70:0x00



#
# Disk Address Packet (DAP) structure
#
dap:
    .byte   0x10               # DAP Size (16 bytes)
    .byte   0x00               # Unused (must be 0)
    .word   0x0000             # Number of sectors to read
    .word   0x0000             # Offset 
    .word   0x0000             # Segment
    .quad   0x0000000000000000 # Starting LBA sector


#
# Global Descriptor Table Data Structures
#
gdt_start:
    # Entry 0 (offset 0)    : Null Descriptor (Mandatory)
    .quad 0x0000000000000000

    # Entry 1 (offset 0x8)  : 32-bits Code Descriptor: base=0, limit=0xFFFFF, Type=Code/Execute/Read
    # Flags: G=1 (4KB granularity), D=1 (32-bit protected mode)
    .word 0xFFFF        # Limit 0:15
    .word 0x0000        # Base 0:15
    .byte 0x00          # Base 16:23
    .byte 0x9A          # Access byte (present, ring 0, code segment, executable, direction 0, readable)
    .byte 0xCF          # Flags byte (granularity 4KiB pages, 32-bit pmode) & Limit 16:19  
    .byte 0x00          # Base 24:31

    # Entry 2 (offset 0x10)  : 32-bits Data Descriptor: base=0, limit=0xFFFFF, Type=Data/Read/Write
    .word 0xFFFF        # Limit 0:15
    .word 0x0000        # Base 0:15
    .byte 0x00          # Base 16:23
    .byte 0x92          # Access byte (present, ring 0, data segment, executable, direction 0, writable)
    .byte 0xCF          # Flags byte (granularity 4KiB pages, 32-bit pmode) & Limit 16:19
    .byte 0x00          # Base 24:31
    
    # Entry 3 (offset 0x18)  : 16-bits code Descriptor
    .word 0xFFFF        # limit (bits 0-15) = 0xFFFFF
    .word 0             # base (bits 0-15) = 0x0
    .byte 0             # base (bits 16-23)
    .byte 0b10011010    # access byte (present, ring 0, code segment, executable, direction 0, readable)
    .byte 0b00001111    # Flags byte (granularity 1Byte pages, 16-bit pmode) & Limit 16:19
    .byte 0             # base high

    # Entry 4 (offset 0x20)  : 16-bits data Descriptor
    .word 0xFFFF        # limit (bits 0-15) = 0xFFFFF
    .word 0             # base (bits 0-15) = 0x0
    .byte 0             # base (bits 16-23)
    .byte 0b10010010    # Access byte (present, ring 0, data segment, executable, direction 0, writable)
    .byte 0b00001111    # Flags byte (granularity 1Byte pages, 16-bit pmode) & Limit 16:19
    .byte 0             # base high    
gdt_end:


gdt_descriptor:
    .word gdt_end - gdt_start - 1   # GDT Size minus 1
    .long 0                         # GDT address (set in runtime)


