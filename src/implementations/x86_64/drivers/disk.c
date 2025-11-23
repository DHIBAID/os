#include "disk.h"

#include "../../../interface/printf.h"
#include "../../../interface/string.h"
#include "../../kernel/memory.h"

// Global filesystem info and current directory cluster (definitions)
FAT32_Info global_fat32;
uint32_t current_dir_cluster = 0;
int fat32_initialized = 0;

int disk_read(uint32_t lba, uint32_t sectors, void* buffer) {
    if (sectors == 0) return -1;

    uint16_t* buf = (uint16_t*)buffer;

    for (uint32_t i = 0; i < sectors; i++) {
        uint32_t current_lba = lba + i;

        // Wait for drive to be ready
        if (ata_wait_bsy() != 0) {
            return -1;
        }

        // Select drive (LBA mode, drive 0)
        outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((current_lba >> 24) & 0x0F));

        // Small delay after drive select
        for (volatile int d = 0; d < 400; d++);

        // Set sector count
        outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);

        // Set LBA
        outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)current_lba);
        outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)(current_lba >> 8));
        outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)(current_lba >> 16));

        // Send read command
        outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        // Wait for data to be ready
        int drq_result = ata_wait_drq();
        if (drq_result != 0) {
            if (drq_result == -2) {
                print_str("Disk error at LBA ");
                print_dec(current_lba);
                print_str(", error: 0x");
                print_hex(inb(ATA_PRIMARY_IO + ATA_REG_ERROR));
                print_str("\n");
            }
            return -1;
        }

        // Read sector data (256 words = 512 bytes)
        for (int j = 0; j < 256; j++) {
            buf[i * 256 + j] = inw(ATA_PRIMARY_IO + ATA_REG_DATA);
        }
    }

    return 0;
}

int fat32_read_mbr(MBR* mbr) {
    if (!mbr) return -1;
    return disk_read(0, 1, mbr);
}

int fat32_find_partition(MBR* mbr, uint32_t* partition_lba) {
    if (!mbr || !partition_lba) return -1;

    // Check MBR signature
    if (mbr->signature != 0xAA55) {
        print_str("Invalid MBR signature\n");
        return -1;
    }

    // Search for FAT32 partition
    for (int i = 0; i < 4; i++) {
        uint8_t type = mbr->partitions[i].partition_type;
        if (type == FAT32_PARTITION_TYPE_LBA || type == FAT32_PARTITION_TYPE_CHS) {
            *partition_lba = mbr->partitions[i].lba_first;
            return 0;
        }
    }

    print_str("No FAT32 partition found\n");
    return -1;
}
int fat32_read_boot_sector(uint32_t partition_lba, FAT32_BootSector* boot_sector) {
    if (!boot_sector) return -1;
    return disk_read(partition_lba, 1, boot_sector);
}

int fat32_init(FAT32_Info* info) {
    if (!info) return -1;

    // Read MBR
    MBR mbr;
    if (fat32_read_mbr(&mbr) != 0) {
        print_str("Failed to read MBR\n");
        return -1;
    }

    // Find FAT32 partition
    uint32_t partition_lba;
    if (fat32_find_partition(&mbr, &partition_lba) != 0) {
        print_str("Failed to find FAT32 partition\n");
        return -1;
    }

    // Read boot sector
    FAT32_BootSector boot_sector;
    if (fat32_read_boot_sector(partition_lba, &boot_sector) != 0) {
        print_str("Failed to read boot sector\n");
        return -1;
    }

    // Fill in FAT32_Info structure
    info->partition_start_lba = partition_lba;
    info->bytes_per_sector = boot_sector.bytes_per_sector;
    info->sectors_per_cluster = boot_sector.sectors_per_cluster;
    info->fat_size = boot_sector.fat_size_32;
    info->root_cluster = boot_sector.root_cluster;
    info->num_fats = boot_sector.num_fats;

    // Calculate FAT and data region locations
    info->fat_begin_lba = partition_lba + boot_sector.reserved_sectors;
    info->data_begin_lba = info->fat_begin_lba + (info->num_fats * info->fat_size);

    print_str("FAT32 initialized successfully\n");
    print_str("  Partition LBA: ");
    print_dec(info->partition_start_lba);
    print_str("\n  FAT begin LBA: ");
    print_dec(info->fat_begin_lba);
    print_str("\n  Data begin LBA: ");
    print_dec(info->data_begin_lba);
    print_str("\n  Root cluster: ");
    print_dec(info->root_cluster);
    print_str("\n");

    return 0;
}

uint32_t fat32_cluster_to_lba(FAT32_Info* info, uint32_t cluster) {
    if (!info) return 0;
    return info->data_begin_lba + ((cluster - 2) * info->sectors_per_cluster);
}

int fat32_read_cluster(FAT32_Info* info, uint32_t cluster, void* buffer) {
    if (!info || !buffer) return -1;

    uint32_t lba = fat32_cluster_to_lba(info, cluster);
    return disk_read(lba, info->sectors_per_cluster, buffer);
}

uint32_t fat32_get_next_cluster(FAT32_Info* info, uint32_t cluster) {
    if (!info) return 0;

    // Calculate FAT entry location
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = info->fat_begin_lba + (fat_offset / info->bytes_per_sector);
    uint32_t entry_offset = fat_offset % info->bytes_per_sector;

    // Read FAT sector
    uint8_t buffer[512];
    if (disk_read(fat_sector, 1, buffer) != 0) {
        return 0;
    }

    // Extract next cluster (mask off upper 4 bits)
    uint32_t next_cluster = *(uint32_t*)&buffer[entry_offset] & 0x0FFFFFFF;
    return next_cluster;
}

int fat32_read_directory(FAT32_Info* info, uint32_t cluster, FAT32_DirectoryEntry* entries, size_t max_entries) {
    if (!info || !entries || max_entries == 0) return -1;

    uint8_t cluster_buffer[4096];  // Assume max cluster size
    uint32_t current_cluster = cluster;
    size_t entry_count = 0;
    int end_of_directory = 0;

    while (!fat32_is_end_of_chain(current_cluster) && entry_count < max_entries && !end_of_directory) {
        // Read cluster
        if (fat32_read_cluster(info, current_cluster, cluster_buffer) != 0) {
            return -1;
        }

        // Parse directory entries
        size_t bytes_per_cluster = info->sectors_per_cluster * info->bytes_per_sector;
        size_t entries_per_cluster = bytes_per_cluster / sizeof(FAT32_DirectoryEntry);

        for (size_t i = 0; i < entries_per_cluster && entry_count < max_entries; i++) {
            FAT32_DirectoryEntry* entry = (FAT32_DirectoryEntry*)&cluster_buffer[i * sizeof(FAT32_DirectoryEntry)];

            // Skip free entries and long filename entries
            if (entry->name[0] == 0x00) {
                end_of_directory = 1;
                break;  // End of directory
            }
            if (entry->name[0] == 0xE5) continue;                     // Deleted entry
            if (entry->attributes == FAT32_ATTR_LONG_NAME) continue;  // Long filename

            // Copy valid entry
            memcpy(&entries[entry_count], entry, sizeof(FAT32_DirectoryEntry));
            entry_count++;
        }

        // Get next cluster
        if (!end_of_directory) {
            uint32_t next = fat32_get_next_cluster(info, current_cluster);
            // Check for invalid cluster values
            if (next == 0 || next == current_cluster) {
                break;  // Invalid or stuck, exit loop
            }
            current_cluster = next;
        }
    }

    return entry_count;
}

int fat32_find_file(FAT32_Info* info, const char* filename, FAT32_DirectoryEntry* entry) {
    if (!info || !filename || !entry) return -1;

    // Parse filename to FAT32 8.3 format
    char fat_name[12];
    fat32_parse_filename(filename, fat_name);

    // Read root directory
    FAT32_DirectoryEntry entries[128];
    int count = fat32_read_directory(info, info->root_cluster, entries, 128);

    if (count < 0) return -1;

    // Search for file
    for (int i = 0; i < count; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            memcpy(entry, &entries[i], sizeof(FAT32_DirectoryEntry));
            return 0;
        }
    }

    return -1;  // File not found
}

void fat32_parse_filename(const char* input, char* output) {
    // Initialize output with spaces
    for (int i = 0; i < 11; i++) {
        output[i] = ' ';
    }
    output[11] = '\0';

    int i = 0, o = 0;

    // Parse name part (up to 8 characters)
    while (input[i] && input[i] != '.' && o < 8) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            output[o++] = input[i++] - 32;  // Convert to uppercase
        } else {
            output[o++] = input[i++];
        }
    }

    // Skip to extension
    while (input[i] && input[i] != '.') i++;
    if (input[i] == '.') i++;

    // Parse extension (up to 3 characters)
    o = 8;
    while (input[i] && o < 11) {
        if (input[i] >= 'a' && input[i] <= 'z') {
            output[o++] = input[i++] - 32;  // Convert to uppercase
        } else {
            output[o++] = input[i++];
        }
    }
}

int fat32_open_file(FAT32_Info* info, const char* filename, FAT32_File* file) {
    if (!info || !filename || !file) return -1;

    FAT32_DirectoryEntry entry;
    if (fat32_find_file(info, filename, &entry) != 0) {
        return -1;  // File not found
    }

    // Parse filename for storage
    fat32_parse_filename(filename, file->filename);

    // Fill file structure
    file->file_size = entry.file_size;
    file->first_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    file->current_cluster = file->first_cluster;
    file->position = 0;
    file->attributes = entry.attributes;

    return 0;
}

int fat32_read_file(FAT32_Info* info, FAT32_File* file, void* buffer, size_t size) {
    if (!info || !file || !buffer || size == 0) return -1;

    uint8_t* buf = (uint8_t*)buffer;
    size_t bytes_read = 0;
    uint32_t cluster_size = info->sectors_per_cluster * info->bytes_per_sector;
    uint8_t cluster_buffer[4096];

    while (bytes_read < size && file->position < file->file_size) {
        // Calculate position within current cluster
        uint32_t cluster_offset = file->position % cluster_size;
        uint32_t bytes_remaining_in_cluster = cluster_size - cluster_offset;
        uint32_t bytes_remaining_in_file = file->file_size - file->position;
        uint32_t bytes_to_read = size - bytes_read;

        // Limit to cluster, file, and request size
        if (bytes_to_read > bytes_remaining_in_cluster) {
            bytes_to_read = bytes_remaining_in_cluster;
        }
        if (bytes_to_read > bytes_remaining_in_file) {
            bytes_to_read = bytes_remaining_in_file;
        }

        // Read cluster if at start of cluster
        if (cluster_offset == 0 || bytes_read == 0) {
            if (fat32_read_cluster(info, file->current_cluster, cluster_buffer) != 0) {
                return -1;
            }
        }

        // Copy data
        memcpy(&buf[bytes_read], &cluster_buffer[cluster_offset], bytes_to_read);
        bytes_read += bytes_to_read;
        file->position += bytes_to_read;

        // Move to next cluster if needed
        if (file->position % cluster_size == 0 && file->position < file->file_size) {
            file->current_cluster = fat32_get_next_cluster(info, file->current_cluster);
            if (fat32_is_end_of_chain(file->current_cluster)) {
                break;
            }
        }
    }

    return bytes_read;
}

void fat32_close_file(FAT32_File* file) {
    if (!file) return;

    // Reset file structure
    file->filename[0] = '\0';
    file->file_size = 0;
    file->first_cluster = 0;
    file->current_cluster = 0;
    file->position = 0;
    file->attributes = 0;
}

int fat32_is_end_of_chain(uint32_t cluster) {
    return cluster >= FAT32_END_OF_CHAIN_MIN && cluster <= FAT32_END_OF_CHAIN_MAX;
}

void fat32_print_boot_sector(FAT32_BootSector* boot_sector) {
    if (!boot_sector) return;

    print_str("FAT32 Boot Sector:\n");
    print_str("  Bytes per sector: ");
    print_dec(boot_sector->bytes_per_sector);
    print_str("\n  Sectors per cluster: ");
    print_dec(boot_sector->sectors_per_cluster);
    print_str("\n  Reserved sectors: ");
    print_dec(boot_sector->reserved_sectors);
    print_str("\n  Number of FATs: ");
    print_dec(boot_sector->num_fats);
    print_str("\n  Total sectors: ");
    print_dec(boot_sector->total_sectors_32);
    print_str("\n  FAT size: ");
    print_dec(boot_sector->fat_size_32);
    print_str(" sectors\n  Root cluster: ");
    print_dec(boot_sector->root_cluster);
    print_str("\n");
}

void fat32_print_directory_entry(FAT32_DirectoryEntry* entry) {
    if (!entry) return;

    // Print filename
    print_str("  ");
    for (int i = 0; i < 8; i++) {
        if (entry->name[i] != ' ') {
            print_char(entry->name[i]);
        }
    }
    if (entry->name[8] != ' ') {
        print_char('.');
        for (int i = 8; i < 11; i++) {
            if (entry->name[i] != ' ') {
                print_char(entry->name[i]);
            }
        }
    }

    // Print attributes
    print_str(" [");
    if (entry->attributes & FAT32_ATTR_DIRECTORY) print_char('D');
    if (entry->attributes & FAT32_ATTR_READ_ONLY) print_char('R');
    if (entry->attributes & FAT32_ATTR_HIDDEN) print_char('H');
    if (entry->attributes & FAT32_ATTR_SYSTEM) print_char('S');
    if (entry->attributes & FAT32_ATTR_ARCHIVE) print_char('A');
    print_str("]");

    // Print size and cluster
    if (!(entry->attributes & FAT32_ATTR_DIRECTORY)) {
        print_str(" Size: ");
        print_dec(entry->file_size);
        print_str(" bytes");
    }
    uint32_t cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
    print_str(" Cluster: ");
    print_dec(cluster);
    print_str("\n");
}

// Helper function to find a file/directory in a specific cluster
int fat32_find_file_in_cluster(FAT32_Info* info, uint32_t cluster, const char* filename, FAT32_DirectoryEntry* entry) {
    if (!info || !filename || !entry) return -1;

    // Parse filename to FAT32 8.3 format
    char fat_name[12];
    fat32_parse_filename(filename, fat_name);

    // Read directory
    FAT32_DirectoryEntry entries[128];
    int count = fat32_read_directory(info, cluster, entries, 128);

    if (count < 0) return -1;

    // Search for file
    for (int i = 0; i < count; i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            memcpy(entry, &entries[i], sizeof(FAT32_DirectoryEntry));
            return 0;
        }
    }

    return -1;  // File not found
}

void list_dir(FAT32_Info* info, uint32_t cluster) {
    if (!info) return;

    FAT32_DirectoryEntry entries[128];
    int count = fat32_read_directory(info, cluster, entries, 128);
    if (count < 0) {
        print_str("Failed to read directory.\n");
        return;
    }

    if (count == 0) {
        print_str("Empty directory\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        fat32_print_directory_entry(&entries[i]);
    }
}

void change_directory(char* path) {
    // Initialize FAT32 if not already done
    if (!fat32_initialized) {
        if (fat32_init(&global_fat32) != 0) {
            print_str("FAT32 initialization failed.\n");
            return;
        }
        current_dir_cluster = global_fat32.root_cluster;
        fat32_initialized = 1;
    }

    // Handle empty path or "." (stay in current directory)
    if (!path || strcmp(path, "") == 0 || strcmp(path, ".") == 0 || strcmp(path, "./") == 0) {
        return;
    }

    // Handle "/" (root directory)
    if (strcmp(path, "/") == 0) {
        current_dir_cluster = global_fat32.root_cluster;
        strcpy(currentDirectory, "/");
        return;
    }

    // Handle ".." or "../" (parent directory)
    if (strcmp(path, "..") == 0 || strcmp(path, "../") == 0) {
        // If already at root, stay there
        if (strcmp(currentDirectory, "/") == 0) {
            return;
        }

        // Update path string - remove last directory component
        int last_slash = -1;
        for (int i = 0; currentDirectory[i] != '\0'; i++) {
            if (currentDirectory[i] == '/') {
                last_slash = i;
            }
        }

        // Build parent path
        char parent_path[256];
        if (last_slash > 0) {
            // Copy everything up to the last slash
            for (int i = 0; i < last_slash; i++) {
                parent_path[i] = currentDirectory[i];
            }
            parent_path[last_slash] = '\0';
        } else {
            // Parent is root
            strcpy(parent_path, "/");
        }

        // Navigate from root to find the parent directory
        if (strcmp(parent_path, "/") == 0) {
            current_dir_cluster = global_fat32.root_cluster;
            strcpy(currentDirectory, "/");
        } else {
            // Parse the parent path and navigate step by step
            uint32_t search_cluster = global_fat32.root_cluster;
            char temp_path[256] = "/";

            // Skip the leading '/'
            int path_idx = 1;
            while (parent_path[path_idx] != '\0') {
                // Extract next directory component
                char component[12];
                int comp_idx = 0;
                while (parent_path[path_idx] != '/' && parent_path[path_idx] != '\0' && comp_idx < 11) {
                    component[comp_idx++] = parent_path[path_idx++];
                }
                component[comp_idx] = '\0';

                if (comp_idx > 0) {
                    // Find this component in current search cluster
                    FAT32_DirectoryEntry found_entry;
                    if (fat32_find_file_in_cluster(&global_fat32, search_cluster, component, &found_entry) != 0) {
                        print_str("cd: invalid parent path\n");
                        return;
                    }

                    search_cluster = ((uint32_t)found_entry.first_cluster_high << 16) | found_entry.first_cluster_low;
                }

                // Skip the '/' if present
                if (parent_path[path_idx] == '/') {
                    path_idx++;
                }
            }

            current_dir_cluster = search_cluster;
            strcpy(currentDirectory, parent_path);
        }
        return;
    }

    // Handle subdirectory navigation
    FAT32_DirectoryEntry dir_entry;
    if (fat32_find_file_in_cluster(&global_fat32, current_dir_cluster, path, &dir_entry) != 0) {
        print_str("cd: ");
        print_str(path);
        print_str(": No such directory\n");
        return;
    }

    // Check if it's a directory
    if (!(dir_entry.attributes & FAT32_ATTR_DIRECTORY)) {
        print_str("cd: ");
        print_str(path);
        print_str(": Not a directory\n");
        return;
    }

    // Get directory cluster
    uint32_t dir_cluster = ((uint32_t)dir_entry.first_cluster_high << 16) | dir_entry.first_cluster_low;
    current_dir_cluster = dir_cluster;

    // Update currentDirectory path
    if (strcmp(currentDirectory, "/") != 0) {
        strcat(currentDirectory, "/");
    }

    // Add directory name (convert from FAT32 format)
    int path_len = strlen(currentDirectory);
    int j = 0;
    for (int i = 0; i < 8 && dir_entry.name[i] != ' '; i++) {
        currentDirectory[path_len + j] = dir_entry.name[i];
        j++;
    }
    currentDirectory[path_len + j] = '\0';
}

void list_subdirectory(FAT32_Info* info, const char* dirname) {
    FAT32_DirectoryEntry entry;

    // Find the directory
    if (fat32_find_file(info, dirname, &entry) != 0) {
        print_str("Directory not found\n");
        return;
    }

    // Check if it's actually a directory
    if (!(entry.attributes & FAT32_ATTR_DIRECTORY)) {
        print_str("Not a directory\n");
        return;
    }

    // Get directory's cluster
    uint32_t dir_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;

    // Read and display entries
    FAT32_DirectoryEntry entries[128];
    int count = fat32_read_directory(info, dir_cluster, entries, 128);

    if (count < 0) {
        print_str("Failed to read directory\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        fat32_print_directory_entry(&entries[i]);
    }
}