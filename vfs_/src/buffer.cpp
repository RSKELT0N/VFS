#include "../include/buffer.h"
#include <memory>

using namespace VFS;

std::shared_ptr<buffer> buffer::mBuf_p;

buffer::buffer() {
    this->mLock = std::make_unique<std::mutex>();
    this->mStream = std::make_unique<std::vector<char>>();
}

std::shared_ptr<buffer> buffer::get_buffer() noexcept {
    if(!mBuf_p) {
        mBuf_p = std::shared_ptr<buffer>(new buffer);
    }
    
    return mBuf_p;
}

void buffer::hold_buffer() noexcept {
     if(!mStream->empty()) {
         printf("\r");
         print_stream();
         mStream->clear();
     }
    mLock->lock();
}

void buffer::release_buffer() noexcept {
    mLock->unlock();
    mStream->clear();
    mStream.reset();
    mStream = std::make_unique<std::vector<char>>();
}

void buffer::retain_buffer(char*& store) const noexcept {
    store = (char*)malloc(sizeof(char) * mStream->size());
    std::copy(mStream->begin(), mStream->end(), store);
}

void buffer::retain_buffer(std::shared_ptr<std::byte[]>& store) const noexcept {
    store = std::shared_ptr<std::byte[]>(new std::byte[mStream->size()]);
    std::copy(mStream->begin(), mStream->end(), (char*)store.get());
}


buffer& buffer::operator<<(const char* str) noexcept {
    for(int i = 0; i < strlen(str); i++) {
        mStream->push_back(str[i]);
    }

    return *(this);
}

buffer& buffer::operator<<(uint64_t val) noexcept {
    int amt = lib_::countDigit(val);
    char buffer[amt];

    lib_::itoa_(val, buffer, 10, amt);
    buffer[amt] = '\0';

    *this << buffer;

    return (*this);
}

void buffer::append(char* str, size_t len) const noexcept {
    for(int i = 0; i < len; i++) {
        mStream->push_back(str[i]);
    }
}

void buffer::print_stream() const noexcept {
    for(int i = 0; i < mStream->size(); i++) {
        printf("%c", (*mStream)[i]);
    }
}

