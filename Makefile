BUILD_DIR=build
SRC_DIR=src
PRG_DIR=progs


# -c: Compile or assemble the source files, but do not linker
# -m16: Generate code for a 16-bit environment
# -ffreestanding: A freestanding environment is one in which the standard library may not exist, and arraysram startup may not necessarily be at main. The most obvious example is an OS kernel. This is equivalent to -fno-hosted.
# -masm=intel: Output assembly instructions using intel synthax
# -fno-zero-initialized-in-bss: Ensures that variables initialized to zero are allocated into .bss
# -ffunction-sections flag tells the GCC compiler to place each function into its own unique code section instead of combining them all into a single monolithic .text section
# --param=min-pagesize=0: Tell GCC that memory at address 0 is valid (avoid gcc 12 bug)
# --32: assemble the source file for the 32-bit Intel i386 architecture
CFLAGS = -c -ffreestanding -masm=intel -fno-zero-initialized-in-bss --param=min-pagesize=0 -Wall -Wextra  -I./include #-Wpedantic

# -m elf_i386: compatibility with gcc -m16 output format
LDFLAGS = -m elf_i386

all: always bootstrap loader tetris bootable_disk 



bootstrap:
	as --32 $(SRC_DIR)/boot_mbr.asm -o $(BUILD_DIR)/boot_mbr.o -I./$(SRC_DIR)
	as --32 $(SRC_DIR)/bootloader.asm -o $(BUILD_DIR)/bootloader.o -I./$(SRC_DIR)
	ld $(LDFLAGS) -T boot_mbr.ld $(BUILD_DIR)/boot_mbr.o -o $(BUILD_DIR)/boot_mbr.elf 
	ld $(LDFLAGS) -T bootloader.ld $(BUILD_DIR)/bootloader.o -o $(BUILD_DIR)/bootloader.elf 
	objcopy --dump-section .text=$(BUILD_DIR)/boot_mbr.bin $(BUILD_DIR)/boot_mbr.elf
	objcopy --dump-section .text=$(BUILD_DIR)/bootloader.bin $(BUILD_DIR)/bootloader.elf
	objdump -M intel,data16,addr16 -D $(BUILD_DIR)/boot_mbr.elf > $(BUILD_DIR)/boot_mbr.asm
	objdump -M intel,data16,addr16 -D $(BUILD_DIR)/bootloader.elf > $(BUILD_DIR)/bootloader.asm	

loader:
	as --32 $(SRC_DIR)/ldlib.asm -o $(BUILD_DIR)/ldlib.o
	gcc $(CFLAGS) -O2 -m32 -march=i386 -ffunction-sections $(SRC_DIR)/loader.c -o $(BUILD_DIR)/loader.o
	ld $(LDFLAGS) -e entry -T loader.ld -o $(BUILD_DIR)/loader.elf $(BUILD_DIR)/ldlib.o $(BUILD_DIR)/loader.o  
	objcopy -O binary $(BUILD_DIR)/loader.elf $(BUILD_DIR)/loader.bin
	objdump -M intel  -D $(BUILD_DIR)/loader.elf > $(BUILD_DIR)/loader.asm

arrays:
	gcc $(CFLAGS) -O1 -m32 -march=i386 -ffunction-sections $(PRG_DIR)/arrays.c -o $(BUILD_DIR)/arrays.o
	ld $(LDFLAGS) -e main -T linker.ld -o $(BUILD_DIR)/arrays.elf $(BUILD_DIR)/arrays.o  
	objcopy -O binary $(BUILD_DIR)/arrays.elf $(BUILD_DIR)/arrays.bin
	objdump -M intel  -D $(BUILD_DIR)/arrays.elf > $(BUILD_DIR)/arrays.asm
	
count32:
	gcc $(CFLAGS) -m32 -march=i386 -ffunction-sections $(PRG_DIR)/count32.c -o $(BUILD_DIR)/count32.o
	ld $(LDFLAGS) -e main -T linker.ld -o $(BUILD_DIR)/count32.elf $(BUILD_DIR)/count32.o  
	objcopy -O binary $(BUILD_DIR)/count32.elf $(BUILD_DIR)/count32.bin
	objdump -M intel  -D $(BUILD_DIR)/count32.elf > $(BUILD_DIR)/count32.asm

hello32:
	gcc $(CFLAGS) -m32 -march=i386 -ffunction-sections $(PRG_DIR)/hello32.c -o $(BUILD_DIR)/hello32.o
	ld $(LDFLAGS) -e main -T linker.ld -o $(BUILD_DIR)/hello32.elf $(BUILD_DIR)/hello32.o  
	objcopy -O binary $(BUILD_DIR)/hello32.elf $(BUILD_DIR)/hello32.bin
	objdump -M intel  -D $(BUILD_DIR)/hello32.elf > $(BUILD_DIR)/hello32.asm

snake:
	as --32 $(SRC_DIR)/key.asm -o $(BUILD_DIR)/key.o
	gcc $(CFLAGS) -O1 -m32 -march=i386 -ffunction-sections $(PRG_DIR)/snake.c -o $(BUILD_DIR)/snake.o
	ld $(LDFLAGS) -e main -T linker.ld -o $(BUILD_DIR)/snake.elf $(BUILD_DIR)/key.o $(BUILD_DIR)/snake.o  
	objcopy -O binary $(BUILD_DIR)/snake.elf $(BUILD_DIR)/snake.bin
	objdump -M intel  -D $(BUILD_DIR)/snake.elf > $(BUILD_DIR)/snake.asm

tetris:
	as --32 $(SRC_DIR)/key.asm -o $(BUILD_DIR)/key.o
	gcc $(CFLAGS) -O1 -m32 -march=i386 -ffunction-sections $(PRG_DIR)/tetris.c -o $(BUILD_DIR)/tetris.o
	ld $(LDFLAGS) -e main -T linker.ld -o $(BUILD_DIR)/tetris.elf $(BUILD_DIR)/key.o $(BUILD_DIR)/tetris.o  
	objcopy -O binary $(BUILD_DIR)/tetris.elf $(BUILD_DIR)/tetris.bin
	objdump -M intel  -D $(BUILD_DIR)/tetris.elf > $(BUILD_DIR)/tetris.asm


DOS=dos622
# seek=63: FAT16 hidden sectors (0x3F)
bootable_disk:
	cp $(BUILD_DIR)/boot_mbr.bin $(BUILD_DIR)/disk.img
	dd if=$(BUILD_DIR)/bootloader.bin of=$(BUILD_DIR)/disk.img bs=512 seek=63 conv=notrunc
# F8 FF FF FF: this mark must be in the begining of first FAT16 entry (bootloader.ld)

    # Modify boot_mbr.as and bootloader.asm for different HD sizes
	truncate -s 1G $(BUILD_DIR)/disk.img
#	mkfs.fat -F 16 $(BUILD_DIR)/disk.img
# @@offset: partition offset pointing directly to the target FAT partition start
# MS-DOS (io.sys and msdos.sys must be the first two entries on root directory)
	mcopy -i $(BUILD_DIR)/disk.img@@32256 msdos/$(DOS)/IO.SYS "::io.sys"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 msdos/$(DOS)/MSDOS.SYS "::msdos.sys"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 msdos/$(DOS)/AUTOEXEC.BAT  "::autoexec.bat"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 msdos/$(DOS)/CONFIG.SYS "::config.sys"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/$(DOS)/dos_full "::/dos"

# Linux 
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s linux "::/linux"

# Hell loader (if HD >= 256MiB loader must be in the begining, otherwise bootloader overflows when searching the next cluster)
	mcopy -i $(BUILD_DIR)/disk.img@@32256 $(BUILD_DIR)/loader.bin "::loader.bin"

# MS-DOS games
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/dd1 "::/dd1"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/dd2 "::/dd2"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/ootw "::/ootw"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/prince "::/prince"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/prince2 "::/prince2"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/karateka "::/karateka"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/lemmings "::/lemmings"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/bthorne "::/bthorne"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/monkey "::/monkey"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/monkey2 "::/monkey2"	
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/zorro "::/zorro"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/rockstar "::/rockstar"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/srse "::/srse"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/srod2 "::/srod2"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/wof "::/wof"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/ninja2 "::/ninja2"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/carnage "::/carnage"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/syndi "::/syndi"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/flash2 "::/flash2"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/atlantis "::/atlantis"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/indi "::/indi"	
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/wipeout "::/wipeout"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/rally "::/rally"
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/games/countdow "::/countdow"
	
# Windows 3.11 
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/win311 "::/windows"

# Borland Turbo C 2.0 (1988)
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/TurboC/TC "::/turboc"

# Borland C++ 3.1 (1992)
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s msdos/borlandc "::/borlandc"

# Terminal progs/games
	mcopy -i $(BUILD_DIR)/disk.img@@32256 -s progs "::/progs"

	mdir -i $(BUILD_DIR)/disk.img@@32256 

run:
	qemu-system-i386 -drive format=raw,file=$(BUILD_DIR)/disk.img -m 512M 

run_kernel:
	qemu-system-i386 -kernel linux/bzimage.img -initrd linux/initrd.cio
	#qemu-system-i386 -kernel linux/bzimage -initrd linux/initrdfs -append rdinit=/etc/init.d/rc

#
# Always
#
always:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f *.o *.bin *.elf
	rm -rf $(BUILD_DIR)
