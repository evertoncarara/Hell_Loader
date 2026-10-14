#
# FAT16 header
# 
jmp short start
nop

# BIOS Parameter Block
bpb_oem:                    .ascii "MSWIN4.1"   # 8 bytes
bpb_bytes_per_sector:       .word 512
bpb_sectors_per_cluster:    .byte 0x20
bpb_reserved_sectors:       .word 0x0001
bpb_fat_count:              .byte 2
bpb_root_entry_count:       .word 512
bpb_total_sectors:          .word 0x0000        # 0x4000 * 512 = 8MiB
bdp_media_type:             .byte 0xF8          # F0 = 3.5" floppy disk, F8 = hd
bpb_sectors_per_fat:        .word 0x00FC            # 8 sectors/fat
bpb_sectors_per_track:      .word 0x003F
bpb_head_side_count:        .word 0x0020
bpb_hidden_sector_count:    .long 0x0000003F
bpb_large_sector_count:     .long 0x001F77E1

# extended boot record
ebr_drive_number:           .byte 0x00             # 0x00 floppy, 0x80 hdd, useless
#                            .byte 0             # reserved
#   ebr_signature:              .byte 0x29
#   ebr_volume_id:              .byte 0x12, 0x34, 0x56, 0x78   # serial number, value doesn't matter
#   ebr_volume_label:           .ascii "HD BOOT    "   # 11 bytes, padded with spaces
#   ebr_system_id:              .ascii "FAT16   "      # 8 bytes
