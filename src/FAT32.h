#ifndef _FAT32_H_
#define _FAT32_H_

#include "VFS.h"
#include "Disk.h"

#define KB(__size__)              (__size__ * 1024)
#define MB(__size__)              (__size__ * (1024 * 1024))
#define CLUSTER_ADDR(__CLUSTER__) (KB(2) * __CLUSTER__)

#define DISK_NAME_LENGTH (uint8_t)10
#define DIR_NAME_LENGTH  (uint8_t)10


class FAT32 : public IFS {

public:
     enum clu_values_t {
        UNALLOCATED_CLUSTER = 0x0000,
        BAD_CLUSTER         = 0xFFF7,
        EOF_CLUSTER         = 0xFFF8,
        ALLOCATED_CLUSTER   = 0x0001
    };

private:

    struct metadata_t {
        const char* disk_name;
        uint32_t disk_size;
        uint32_t cluster_size;
        uint32_t cluster_n;

        metadata_t(const char* name, uint32_t& dsk_size, uint32_t& clu_size, uint32_t& clu_n);
    } __attribute__((packed));

    typedef struct __attribute__((packed)) {
        metadata_t data;
        uint32_t superblock_addr;
        uint32_t fat_table_addr;
        uint32_t root_dir_addr;
    } superblock_t;

    typedef struct __attribute__((packed)) {
        char dir_name[DIR_NAME_LENGTH];
        uint32_t dir_entry_amt;
        uint32_t start_cluster_index;
        uint32_t parent_cluster_index;
    } dir_header_t;

    typedef struct __attribute__((packed)) {
        char dir_entry_name[DIR_NAME_LENGTH];
        uint32_t start_cluster_index;

        uint32_t dir_entry_size;
        uint8_t is_directory;
    } dir_entry_t;

    typedef struct __attribute__((packed)) {
        dir_header_t dir_header;
        dir_entry_t* dir_entries;
    } dir_t;

public:
    FAT32();
    FAT32(const char* disk_name);
    ~FAT32();

    FAT32(const FAT32& tmp) = delete;
    FAT32(FAT32&& tmp) = delete;

public:
     void cd(const path& pth) const noexcept override;
     void mkdir(char* dir) const noexcept override;
     void rm(char* file) noexcept override;
     void rm(char *file, const char* args, ...) noexcept override;
     void cp(const path& src, const path& dst) noexcept override;

private:
    void init() noexcept;
    void set_up() noexcept;
    void load() noexcept;

    void define_superblock() noexcept;
    void define_fat_table() noexcept;


private:
    DiskDriver *disk;
    std::vector<uint16_t>* fat_table;
    dir_t* root;

private:
     const char* DISK_NAME;
    static constexpr const char* DEFAULT_DISK = "disk.dat";

    static constexpr uint32_t STORAGE_SIZE = MB(100);
    static constexpr uint32_t CLUSTER_SIZE = KB(2);
    static constexpr uint32_t CLUSTER_N    = STORAGE_SIZE - (sizeof(superblock_t) + sizeof(fat_table)) / CLUSTER_SIZE;

    static constexpr uint32_t SUPERBLOCK_START_ADDR = 0x0000;
    static constexpr uint32_t FAT_TABLE_START_ADDR  = sizeof(superblock_t);
    static constexpr uint32_t ROOT_START_ADDR       = FAT_TABLE_START_ADDR + sizeof(FAT_TABLE_START_ADDR);
};

#endif //_FAT32_H_