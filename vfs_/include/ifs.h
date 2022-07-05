#ifndef _IFS_H_
#define _IFS_H_

#include <vector>
#include <string>

#include "fs.h"
#include "buffer.h"

namespace VFS::IFS {

    class ifs : public fs {
    public:
        ifs() = default;
        ~ifs() override = default;
        ifs(const ifs&) = delete;
        ifs(ifs&&) = delete;

    public:
        __attribute__((unused)) virtual void cd(const char* pth) noexcept = 0;
        __attribute__((unused)) virtual void mkdir(const char* dir) noexcept = 0;
        __attribute__((unused)) virtual void rm(std::vector<std::string>& tokens) noexcept = 0;
        __attribute__((unused)) virtual void mv(std::vector<std::string>& tokens) noexcept = 0;
        __attribute__((unused)) virtual void cp(const char* src, const char* dst) noexcept = 0;
        __attribute__((unused)) virtual void cp_imp(const char* src, const char* dst) noexcept = 0;
        __attribute__((unused)) virtual void cp_exp(const char* src, const char* dst) noexcept = 0;
        __attribute__((unused)) virtual void touch(std::vector<std::string>& tokens, char* data, uint64_t size) noexcept = 0;
        __attribute__((unused)) virtual void cat(const char* path, int8_t export_ = 0) noexcept = 0;
        __attribute__((unused)) virtual void ls() noexcept = 0;
    };
}

#endif //_IFS_H_