#ifndef _DISK_H_
#define _DISK_H_

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "config.h"
#include "diskdriver.h"
#include "buffer.h"

namespace VFS {

    class disk : public diskdriver {

    public:
        disk();
        ~disk() override;
        disk(disk&&) = delete;

    private:
        disk(const disk&);

    public:
        ret_t rm() override;
        ret_t close() override;
        ret_t seek(const long& offset) override;
        ret_t truncate(const off_t& size) override;
        ret_t open(const char* pathname, const char* mode) override;
        ret_t read(void* ptr, const size_t& size, const uint32_t& amt) override;
        ret_t write(const void* ptr, const size_t& size, const uint32_t& amt) override;

    public:
        [[nodiscard]] FILE* get_file() const noexcept;
        [[nodiscard]] size_t& get_addr() const noexcept;

    private:
        FILE* file;
        size_t addr;
        const char* disk_name;
        std::string cmpl_path_to_file;
    };
}

#endif //_DISK_H_