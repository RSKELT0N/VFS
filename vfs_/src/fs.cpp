#include "../include/fs.h"

using namespace VFS;

std::string fs::convert_size(const uint64_t& bytes_) const noexcept {
    long double ret = 0;
    auto bytes = static_cast<long double>(bytes_);

    char buffer[10];
    memset(buffer, '\0', 10);

    if(bytes > ((long double)(1 << 30))) {
        ret = bytes / (1 << 30);
        sprintf(buffer, "%03.2LfGB", ret);
        goto end;
    }

    if(bytes > ((long double)(1 << 20))) {
        ret = bytes / (1 << 20);
        sprintf(buffer, "%03.2LfMB", ret);
        goto end;
    }

    if(bytes > ((long double)(1 << 10))) {
        ret = bytes / (1 << 10);
        sprintf(buffer, "%03.2LfKB", ret);
        goto end;
    }

    ret = bytes;
    if(buffer[0] == '\0')
        sprintf(buffer, "%03.2LfB", ret);

    end:

    return std::string(std::string(buffer));
}

FILE* fs::get_file_handlr(const char* file, char* type) noexcept {
    FILE* handlr = fopen(file, type);
    return handlr;
}

uint64_t fs::get_ext_file_buffer(const char* path, std::shared_ptr<std::byte[]>& payload) noexcept {
    FILE* file = get_file_handlr(path);

    if(file == nullptr) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    uint64_t fsize = ftell(file);
    rewind(file);


    payload = std::shared_ptr<std::byte[]>(new std::byte[fsize]);
    fread(payload.get(), sizeof(std::byte), fsize, file);
    fclose(file);
    return fsize;
}

long fs::get_file_size(const char* path) noexcept {
    FILE* file = get_file_handlr(path);
    fseek(file, 0L, SEEK_END);
    long sz = ftell(file);
    fclose(file);
    return sz;
}

void fs::store_ext_file_buffer(const char* path, std::shared_ptr<std::byte[]>& payload, uint64_t size) noexcept {
    FILE* file = get_file_handlr(path, (char*)"w+");

    ftruncate(fileno(file), size);
    rewind(file);
    fclose(file);

    FILE* file_ = get_file_handlr(path, (char*)"r+");
    fwrite(payload.get(), get_file_size(path), sizeof(char), file);
    fclose(file);
}