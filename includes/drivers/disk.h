#ifndef DISK_H
#define DISK_H

#include <stddef.h>
#include <stdint.h>

#include "lib/util.h"

// FAT32 Partition Types
#define FAT32_PARTITION_TYPE_LBA 0x0C
#define FAT32_PARTITION_TYPE_CHS 0x0B

// FAT32 Cluster Markers
#define FAT32_END_OF_CHAIN_MIN 0x0FFFFFF8
#define FAT32_END_OF_CHAIN_MAX 0x0FFFFFFF
#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define FAT32_FREE_CLUSTER 0x00000000

// Directory Entry Attributes
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN 0x02
#define FAT32_ATTR_SYSTEM 0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE 0x20
#define FAT32_ATTR_LONG_NAME 0x0F

// ATA/IDE Port definitions
#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_DCR_AS 0x3F6
#define ATA_SECONDARY_IO 0x170
#define ATA_SECONDARY_DCR_AS 0x376

// ATA Registers
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_FEATURES 1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7

// ATA Commands
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_CACHE_FLUSH 0xE7

// ATA Status bits
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DSC 0x10
#define ATA_SR_DRQ 0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX 0x02
#define ATA_SR_ERR 0x01

// MBR Partition Entry (16 bytes)
typedef struct {
    uint8_t status;          // Boot indicator (0x80 = bootable, 0x00 = non-bootable)
    uint8_t chs_first[3];    // CHS address of first sector
    uint8_t partition_type;  // Partition type
    uint8_t chs_last[3];     // CHS address of last sector
    uint32_t lba_first;      // LBA of first sector in partition
    uint32_t sector_count;   // Number of sectors in partition
} __attribute__((packed)) MBR_PartitionEntry;

// Master Boot Record (512 bytes)
typedef struct {
    uint8_t bootloader[446];           // Boot code
    MBR_PartitionEntry partitions[4];  // Partition table (4 entries)
    uint16_t signature;                // Boot signature (0xAA55)
} __attribute__((packed)) MBR;

// FAT32 Boot Sector (512 bytes total)
typedef struct {
    // BIOS Parameter Block
    uint8_t jmp_boot[3];          // Jump instruction
    uint8_t oem_name[8];          // OEM name
    uint16_t bytes_per_sector;    // Bytes per sector (usually 512)
    uint8_t sectors_per_cluster;  // Sectors per cluster
    uint16_t reserved_sectors;    // Reserved sector count
    uint8_t num_fats;             // Number of FATs (usually 2)
    uint16_t root_entry_count;    // Root directory entries (0 for FAT32)
    uint16_t total_sectors_16;    // Total sectors (0 for FAT32)
    uint8_t media_type;           // Media descriptor
    uint16_t fat_size_16;         // FAT size in sectors (0 for FAT32)
    uint16_t sectors_per_track;   // Sectors per track
    uint16_t num_heads;           // Number of heads
    uint32_t hidden_sectors;      // Hidden sectors
    uint32_t total_sectors_32;    // Total sectors (for FAT32)

    // FAT32 Extended BPB
    uint32_t fat_size_32;            // FAT size in sectors
    uint16_t ext_flags;              // Extended flags
    uint16_t fs_version;             // Filesystem version
    uint32_t root_cluster;           // Root directory cluster
    uint16_t fs_info;                // FSInfo sector
    uint16_t backup_boot_sector;     // Backup boot sector
    uint8_t reserved[12];            // Reserved
    uint8_t drive_number;            // Drive number
    uint8_t reserved1;               // Reserved
    uint8_t boot_signature;          // Extended boot signature (0x29)
    uint32_t volume_id;              // Volume serial number
    uint8_t volume_label[11];        // Volume label
    uint8_t fs_type[8];              // Filesystem type ("FAT32   ")
    uint8_t boot_code[420];          // Boot code
    uint16_t boot_sector_signature;  // 0xAA55
} __attribute__((packed)) FAT32_BootSector;

// FAT32 Directory Entry (32 bytes)
typedef struct {
    uint8_t name[11];             // 8.3 filename (padded with spaces)
    uint8_t attributes;           // File attributes
    uint8_t reserved;             // Reserved for Windows NT
    uint8_t creation_time_ms;     // Creation time (milliseconds)
    uint16_t creation_time;       // Creation time
    uint16_t creation_date;       // Creation date
    uint16_t last_access_date;    // Last access date
    uint16_t first_cluster_high;  // High word of first cluster
    uint16_t last_modified_time;  // Last modified time
    uint16_t last_modified_date;  // Last modified date
    uint16_t first_cluster_low;   // Low word of first cluster
    uint32_t file_size;           // File size in bytes
} __attribute__((packed)) FAT32_DirectoryEntry;

// FAT32 Filesystem Information
typedef struct {
    uint32_t partition_start_lba;  // Starting LBA of FAT32 partition
    uint32_t fat_begin_lba;        // Starting LBA of FAT table
    uint32_t data_begin_lba;       // Starting LBA of data region
    uint16_t bytes_per_sector;     // Bytes per sector
    uint8_t sectors_per_cluster;   // Sectors per cluster
    uint32_t fat_size;             // Size of FAT in sectors
    uint32_t root_cluster;         // Root directory cluster number
    uint8_t num_fats;              // Number of FATs
} FAT32_Info;

// FAT32 File Handle
typedef struct {
    char filename[12];         // 8.3 filename with null terminator
    uint32_t file_size;        // File size in bytes
    uint32_t first_cluster;    // First cluster of file
    uint32_t current_cluster;  // Current cluster being read
    uint32_t position;         // Current position in file
    uint8_t attributes;        // File attributes
} FAT32_File;

// 1. Block Device Operations
int disk_read(uint32_t lba, uint32_t sectors, void* buffer);

// 2. MBR Operations
int fat32_read_mbr(MBR* mbr);
int fat32_find_partition(MBR* mbr, uint32_t* partition_lba);

// 3. Boot Sector Operations
int fat32_read_boot_sector(uint32_t partition_lba, FAT32_BootSector* boot_sector);
int fat32_init(FAT32_Info* info);

// 4. Cluster Operations
uint32_t fat32_cluster_to_lba(FAT32_Info* info, uint32_t cluster);
int fat32_read_cluster(FAT32_Info* info, uint32_t cluster, void* buffer);
uint32_t fat32_get_next_cluster(FAT32_Info* info, uint32_t cluster);

// 5. Directory Operations
int fat32_read_directory(FAT32_Info* info, uint32_t cluster, FAT32_DirectoryEntry* entries, size_t max_entries);
int fat32_find_file(FAT32_Info* info, const char* filename, FAT32_DirectoryEntry* entry);
void fat32_parse_filename(const char* input, char* output);

// 6. File Operations
int fat32_open_file(FAT32_Info* info, const char* filename, FAT32_File* file);
int fat32_read_file(FAT32_Info* info, FAT32_File* file, void* buffer, size_t size);
void fat32_close_file(FAT32_File* file);

// 7. Utility Functions
int fat32_is_end_of_chain(uint32_t cluster);
void fat32_print_boot_sector(FAT32_BootSector* boot_sector);
void fat32_print_directory_entry(FAT32_DirectoryEntry* entry);

// 8. Terminal Functions
void list_dir(FAT32_Info* info, uint32_t cluster);
void change_directory(char* path);
int fat32_find_file_in_cluster(FAT32_Info* info, uint32_t cluster, const char* filename, FAT32_DirectoryEntry* entry);

// 9. Global Variables
extern FAT32_Info global_fat32;
extern uint32_t current_dir_cluster;
extern int fat32_initialized;
extern char currentDirectory[256];

// Wait for disk to be ready
static int ata_wait_bsy() {
    int timeout = 10000000;
    while ((inb(ATA_PRIMARY_IO + ATA_REG_STATUS) & ATA_SR_BSY) && timeout-- > 0);
    return timeout > 0 ? 0 : -1;
}

static int ata_wait_drq() {
    int timeout = 10000000;
    uint8_t status;
    while (timeout-- > 0) {
        status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (status & ATA_SR_DRQ) return 0;   // Data ready
        if (status & ATA_SR_ERR) return -2;  // Error occurred
    }
    return -1;  // Timeout
}

// Disk write (PIO)
int disk_write(uint32_t lba, uint32_t sectors, const void* buffer);

// FAT/FS write helpers
int fat32_write_cluster(FAT32_Info* info, uint32_t cluster, const void* buffer);
uint32_t fat32_allocate_cluster(FAT32_Info* info);
int fat32_write_fat_entry(FAT32_Info* info, uint32_t cluster, uint32_t value);
int fat32_write_directory_entry(FAT32_Info* info, uint32_t dir_cluster, size_t entry_index, FAT32_DirectoryEntry* entry);
int fat32_find_free_directory_entry(FAT32_Info* info, uint32_t dir_cluster, uint32_t* out_cluster, size_t* out_index);

// High-level operations
int fat32_create_file(FAT32_Info* info, uint32_t dir_cluster, const char* name);
int fat32_delete_file(FAT32_Info* info, uint32_t dir_cluster, const char* name);
int fat32_create_directory(FAT32_Info* info, uint32_t dir_cluster, const char* name);
int fat32_remove_directory(FAT32_Info* info, uint32_t dir_cluster, const char* name);
int fat32_cat(FAT32_Info* info, const char* filename);

#endif  // DISK_H
