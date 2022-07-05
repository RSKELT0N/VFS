#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <mutex>
#include <stdio.h>
#include <cstring>

#include "config.h"
#include "lib.h"

#define BUFFER     (*VFS::buffer::get_buffer().get())

namespace VFS {

    class buffer {
    private:
        buffer();

    public:
        ~buffer() = default;;
        buffer(buffer&&) = delete;
        buffer(const buffer&) = delete;

    public:
        static std::shared_ptr<buffer> get_buffer() noexcept;
        buffer& operator<<(uint64_t) noexcept;
        buffer& operator<<(const char*) noexcept;

        void hold_buffer() noexcept;
        void release_buffer() noexcept;
        void retain_buffer(char*& store) const noexcept;
        void retain_buffer(std::shared_ptr<std::byte[]>&) const noexcept;
        void print_stream() const noexcept;
        void append(char*, size_t) const noexcept;

    private:
        std::unique_ptr<std::mutex> mLock;
        static std::shared_ptr<buffer> mBuf_p;

    public:
        std::unique_ptr<std::vector<char>> mStream;
    };
}


#endif // _BUFFER_H_
