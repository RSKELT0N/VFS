#ifndef _TERMINAL_H_
#define _TERMINAL_H_

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "vfs.h"
#include "lib.h"
#include "buffer.h"

#define SEPARATOR (char)' '
#define CLEAR_SCR (const char*)"\033c"

namespace VFS {

    class terminal {

    public:

        typedef struct {
            vfs::system_cmd cmd_name = {};
            vfs::system_cmd (terminal::*valid)(std::vector<std::string>& parts){};
            void (terminal::*funct)(vfs::system_cmd cmd, std::vector<std::string> args, char*& payload, uint64_t size, int8_t options){};
        } cmd_t;

    private:
        terminal();
    public:
        ~terminal();
        terminal(const terminal&) = delete;
        terminal(terminal&&) = delete;

    public:
        void run() noexcept;
        static terminal* get_instance() noexcept;
        void interpret_cmd(vfs::system_cmd& cmd, std::vector<std::string>& args, char*& payload = (char*&)"", uint64_t size = 0, int8_t options = 0) noexcept;
        void translate_remote_cmd(vfs::system_cmd& cmd, std::vector<std::string>& args) noexcept;

    private:
        void input(const char* line) noexcept;

        void cmd_map() noexcept;
        vfs::system_cmd validate_cmd(std::vector<std::string>& parts) noexcept;

        void print_help(vfs::system_cmd, std::vector<std::string>,    char*&, uint64_t size = 0, int8_t options = 0)noexcept;
        void clear_scr(vfs::system_cmd, std::vector<std::string>,     char*&, uint64_t size = 0, int8_t options = 0) noexcept;
        void map_vfs_funct(vfs::system_cmd, std::vector<std::string>, char*&, uint64_t size = 0, int8_t options = 0) noexcept;
        void map_sys_funct(vfs::system_cmd, std::vector<std::string>, char*&, uint64_t size = 0, int8_t options = 0) noexcept;

    public:
        vfs::system_cmd valid_vfs(std::vector<std::string>& parts)   noexcept;
        vfs::system_cmd valid_ls(std::vector<std::string>& parts)    noexcept;
        vfs::system_cmd valid_mkdir(std::vector<std::string>& parts) noexcept;
        vfs::system_cmd valid_cd(std::vector<std::string>& parts)    noexcept;
        vfs::system_cmd valid_rm(std::vector<std::string>& parts)    noexcept;
        vfs::system_cmd valid_touch(std::vector<std::string>& parts) noexcept;
        vfs::system_cmd valid_mv(std::vector<std::string>& parts)    noexcept;
        vfs::system_cmd valid_cp(std::vector<std::string>& parts)    noexcept;
        vfs::system_cmd valid_cat(std::vector<std::string>& parts)   noexcept;
        vfs::system_cmd valid_help(std::vector<std::string>& parts)  noexcept;
        vfs::system_cmd valid_clear(std::vector<std::string>& parts) noexcept;

    public:
        static vfs* m_vfs;
        static terminal* m_terminal;
    private:
        std::string path;
        vfs::system_t** m_mnted_system;
        std::unique_ptr<std::mutex> sys_lock;
        std::shared_ptr<std::unordered_map<std::string, cmd_t>> m_syscmds;
    };
}

#endif // _TERMINAL_H_