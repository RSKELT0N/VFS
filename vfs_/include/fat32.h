#ifndef _FAT32_H_
#define _FAT32_H_

#include <string.h>
#include <utility>
#include <vector>
#include <unordered_set>

#include "ifs.h"
#include "disk.h"
#include "lib.h"
#include "vfs.h"

#define abs_(a,b)            ((a) < (b) ? (b) - (a) : (a) - (b))
#define min_(a,b)            ((a) < (b) ? (a) : (b))
#define DISK_NAME_LENGTH     (uint8_t)10
#define DIR_NAME_LENGTH      (uint8_t)10
#define UNDEF_START_CLUSTER  0
#define DIRECTORY            1
#define NON_DIRECTORY        0

namespace VFS::IFS {

    class fat32 : public ifs {

    public:
        enum clu_values_t {
            UNALLOCATED_CLUSTER = 0x00000000,
            BAD_CLUSTER         = 0xFF700000,
            EOF_CLUSTER         = 0xFF800000,
            ALLOCATED_CLUSTER   = 0x00000001
        } __attribute__((packed));

    private:

        struct metadata_t {
            char disk_name[DISK_NAME_LENGTH] = {};
            int64_t disk_size = {};
            uint32_t superblock_size = {};
            uint64_t fat_table_size = {};
            int64_t user_size = {};
            uint32_t cluster_size = {};
            uint32_t cluster_n = {};
        } __attribute__((packed));

        typedef struct __attribute__((packed)) {
            metadata_t data = {};
            uint32_t superblock_addr = {};
            uint32_t fat_table_addr = {};
            uint32_t root_dir_addr = {};
        } superblock_t;

        typedef struct __attribute__((packed)) {
            char dir_name[DIR_NAME_LENGTH] = {};
            uint32_t dir_entry_amt = {};
            uint32_t start_cluster_index = {};
            uint32_t parent_cluster_index = {};
        } dir_header_t;

        typedef struct __attribute__((packed)) {
            char dir_entry_name[DIR_NAME_LENGTH] = {};
            uint32_t start_cluster_index = {};

            uint64_t dir_entry_size = {};
            uint8_t is_directory = {};
        } dir_entry_t;

         struct dir_t {
            dir_header_t dir_header = {};
            std::shared_ptr<dir_entry_t[]> dir_entries = {};

            ~dir_t() = default;
        } __attribute__((packed));

        struct dir_entr_ret_t {
            std::shared_ptr<dir_t> m_dir = nullptr;
            dir_entry_t* m_entry = nullptr;

            ~dir_entr_ret_t() = default;
            dir_entr_ret_t(std::shared_ptr<dir_t> dir, dir_entry_t* entry) : m_dir(std::move(dir)), m_entry(std::move(entry)) { };
        } __attribute__((packed));

    public:
        explicit fat32(const char* disk_name);
        ~fat32() override = default;

        fat32(const fat32& tmp) = delete;
        fat32(fat32&& tmp) = delete;

    public:
        void ls() noexcept override;
        void cd(const char* pth)  noexcept override;
        void mkdir(const char* dir) noexcept override;
        void rm(std::vector<std::string>& tokens) noexcept override;
        void mv(std::vector<std::string>& tokens) noexcept override;
        void cp(const char* src, const char* dst) noexcept override;
        void cp_imp(const char* src, const char* dst) noexcept override;
        void cp_exp(const char* src, const char* dst) noexcept override;
        void cat(const char* path, int8_t export_ = 0) noexcept override;
        void touch(std::vector<std::string>& tokens, char* payload, uint64_t size) noexcept override;

    private:
        void set_up() noexcept;
        void init() noexcept;
        void load() noexcept;
        static int8_t check_config() noexcept;
        int8_t dir_equal(std::shared_ptr<dir_t>&, std::shared_ptr<dir_t>&) noexcept;

        void create_disk() noexcept;
        void add_new_entry(std::shared_ptr<dir_t>& dir, const char* name, const uint32_t& start_clu, const uint64_t& size, const uint8_t& is_dir) noexcept;

        void define_superblock() noexcept;
        void define_fat_table() noexcept;
        std::shared_ptr<dir_t> init_dir(const uint32_t& start_cl, const uint32_t& parent_clu, const char* name) noexcept;

        void store_superblock() noexcept;
        void store_fat_table() noexcept;
        void store_dir(std::shared_ptr<dir_t>& directory) noexcept;
        void save_dir(std::shared_ptr<dir_t>& directory) noexcept;

        void load_superblock() noexcept;
        void load_fat_table() noexcept;

        int32_t store_file(std::shared_ptr<std::byte[]>& path, uint64_t data_size) noexcept;

        uint32_t insert_dir(std::shared_ptr<dir_t>& curr_dir, const char* dir_name) noexcept;
        void insert_int_file(std::shared_ptr<dir_t>& dir, std::shared_ptr<std::byte[]>& buffer, const char* name, size_t size) noexcept;
        void insert_ext_file(std::shared_ptr<dir_t>& dir, const char* path, const char* name) noexcept;

        void delete_entry(std::unique_ptr<dir_entr_ret_t>& entry) noexcept;
        void delete_dir(std::shared_ptr<dir_t>& dir) noexcept;

        void cp_dir(std::shared_ptr<dir_t>& src, std::shared_ptr<dir_t>& dst) noexcept;

        std::shared_ptr<dir_t> read_dir(const uint32_t& start_clu) noexcept;
        size_t read_file(std::shared_ptr<dir_t>& dir, const char* entry_name, std::shared_ptr<std::byte[]>& buffer) noexcept;

        [[nodiscard]] uint32_t attain_clu() const noexcept;
        [[nodiscard]] uint32_t n_free_clusters(const uint32_t& req) const noexcept;
        std::unique_ptr<std::vector<uint32_t>> get_list_of_clu(const uint32_t& start_clu) noexcept;
        void rm_entr_mem(std::shared_ptr<dir_t>& dir, const char* name) noexcept;

        fat32::dir_entry_t* find_entry(std::shared_ptr<dir_t>& dir, const char* path, uint8_t shd_exst) const noexcept;
        std::unique_ptr<dir_entr_ret_t> parsePath(std::vector<std::string>& paths, uint8_t shd_exst) noexcept;

    public:
        void print_fat_table() const noexcept;
        void print_dir(dir_t& dir) noexcept;
        void print_super_block() const noexcept;

    private:
        const char* DISK_NAME;
        const char* PATH_TO_DISK;

        static constexpr uint64_t USER_SPACE   = CFG_USER_SPACE_SIZE;
        static constexpr uint32_t CLUSTER_SIZE = CFG_CLUSTER_SIZE;
        static constexpr uint64_t CLUSTER_AMT  = USER_SPACE / CLUSTER_SIZE;

        static constexpr uint64_t STORAGE_SIZE           = (sizeof(superblock_t) + (sizeof(uint32_t) * CLUSTER_AMT)) + USER_SPACE;
        static constexpr uint32_t SUPERBLOCK_START_ADDR  = 0x00000000;
        static constexpr uint32_t FAT_TABLE_START_ADDR   = sizeof(superblock_t);
        static constexpr uint32_t FAT_TABLE_SIZE         = sizeof(uint32_t) * CLUSTER_AMT;
        static constexpr uint32_t ROOT_START_ADDR        = FAT_TABLE_START_ADDR + FAT_TABLE_SIZE;
        static constexpr uint32_t SUPERBLOCK_SIZE        = sizeof(superblock_t);

    private:
        superblock_t m_superblock;
        std::shared_ptr<dir_t> m_root;
        std::shared_ptr<dir_t> m_curr_dir;
        std::unique_ptr<diskdriver> m_disk;
        std::unique_ptr<uint32_t[]> m_fat_table;
        std::unique_ptr<std::unordered_set<uint32_t>> m_free_clusters;
    };
}

#endif //_FAT32_H_