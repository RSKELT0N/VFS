#ifndef _FS_H_
#define _FS_H_


#include <string>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "log.h"

namespace VFS {
    class fs {
    public:
        fs() = default;
        virtual ~fs() = default;

    public:
        std::string convert_size(const uint64_t&) const noexcept;
        FILE* get_file_handlr(const char* file_path, char* type = (char*)"rb+") noexcept;
        long get_file_size(const char* file_path) noexcept;
        uint64_t get_ext_file_buffer(const char* file_path, std::shared_ptr<std::byte[]>&) noexcept;
        void store_ext_file_buffer(const char* file_path, std::shared_ptr<std::byte[]>&, uint64_t size) noexcept;
    };
}

#endif // _FS_H_