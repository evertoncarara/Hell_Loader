### word = 16 bits!!!! ###


.code32     
.intel_syntax noprefix
.global kbhit
.global get_key



#
# Returns AL = 1 if a key is waiting, AL = 0 otherwise
#
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
