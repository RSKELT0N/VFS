#ifndef _VFS_H_
#define _VFS_H_

#include <set>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <unordered_map>

#include "server.h"
#include "client.h"
#include "fat32.h"
#include "lib.h"

namespace VFS {

    struct vfs {

    public:
        enum system_cmd {
            vfs_,
            ls,
            mkdir,
            cd,
            rm,
            touch,
            cp,
            mv,
            cat,
            help,
            clear,
            invalid
        };

        struct flag_t {
            const char *name = {};
            void (vfs::*func)(std::vector <std::string> &) = NULL;
            const char *desc = {};
            flag_t(const char *nm, void(vfs::*ptr)(std::vector <std::string> &), const char *dsc) : name(nm), func(ptr),
                                                                                                    desc(dsc) {}
        };

        struct cmd_t {
            system_cmd cmd = {};
            std::vector <flag_t> flags = {};
            const char *desc = {};

            cmd_t() = default;
        };

        struct system_t {
            struct sock_conn_t {
                const char *addr = {};
                int32_t port = {};
            };

            const char *name;
            std::shared_ptr<VFS::fs> mp_fs = nullptr;
            const char *fs_type;
            sock_conn_t conn;

            void (vfs::*access)(system_cmd cmd, std::vector <std::string> &args, const char *, uint64_t size, int8_t options);

            ~system_t() = default;
            system_t(const char *nme, std::shared_ptr<VFS::fs> _fs, const char *type, void (vfs::*ptr)(vfs::system_cmd, std::vector <std::string> &, const char *, uint64_t, int8_t), sock_conn_t hint) : name(nme), mp_fs(_fs), fs_type(type), access(ptr), conn(hint) {};
        };

    private:
        vfs();
        vfs(const vfs &) = delete;
        vfs(vfs &&) = delete;

    public:
        ~vfs() = default;

    private:
        void mnt_disk(std::vector <std::string> &);
        void add_disk(std::vector <std::string> &);
        void rm_disk(std::vector <std::string> &);
        void add_remote(std::vector <std::string> &);
        void rm_remote(std::vector <std::string> &);
        void lst_disks(std::vector <std::string> &);
        void init_server(std::vector <std::string> &);
        void vfs_help() const noexcept;

    public:
        void init_sys_cmds() noexcept;
        void umnt_disk(std::vector <std::string> &);
        std::shared_ptr <fs> typetofs(const char *name, const char *fs_type) noexcept;
        void control_vfs(const std::vector <std::string> &) noexcept;
        void control_ifs(std::vector <std::string> &) noexcept;
        void control_rfs(std::vector <std::string> &) noexcept;
        void ifs_cmd_func(vfs::system_cmd cmd, std::vector <std::string> &args, const char *buffer, uint64_t size, int8_t options = 0) noexcept;
        void rfs_cmd_func(vfs::system_cmd cmd, std::vector <std::string> &args, const char *buffer, uint64_t size, int8_t options = 0) noexcept;

    public:
        void load_disks() noexcept;
        static vfs *get_vfs() noexcept;
        const bool is_mnted() const noexcept;
        std::shared_ptr <vfs::system_t> &get_mnted_system() noexcept;
        std::shared_ptr <std::vector<vfs::cmd_t>> get_sys_cmds() noexcept;

    private:
        static vfs* mp_vfs;
        std::unique_ptr <RFS::rfs> server;
        std::shared_ptr <system_t> mnted_system;
        std::shared_ptr <std::vector<vfs::cmd_t>> sys_cmds;
        std::unique_ptr <std::unordered_map<std::string, system_t>> disks;

    public:
        std::set <std::string> fs_types = {"fat32"};
        static constexpr const char *DEFAULT_FS = "fat32";
        static constexpr const char *syscmd_str[] = {"/vfs", "ls", "mkdir", "cd", "rm", "touch", "cp", "mv", "cat","/help", "/clear", "/exit", "invalid"};
    };
}

#endif //_VFS_H_