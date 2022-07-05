#include <cstdio>
#include "../include/disk.h"

using namespace VFS;

disk::disk() {
    file = nullptr;
}

disk::~disk() {
    if(!file) {
        BUFFER << LOG_str(log::WARNING, "FILE object has not been initialised yet!");
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

FILE* disk::get_file() const noexcept {
    if(!file)
        BUFFER << LOG_str(log::WARNING, "FILE cannot be returned as it has not been initialised!");

    return file;
}

size_t& disk::get_addr() const noexcept {
    return (size_t&)addr;
}

diskdriver::ret_t disk::open(const char *pathname, const char *mode) {
    this->cmpl_path_to_file = "disks/" + std::string(pathname);
    this->disk_name = pathname;

    this->file = fopen(cmpl_path_to_file.c_str(), mode);

    if(file == NULL)
        LOG(log::ERROR_, "File descriptor could not be opened.");

    return file == nullptr ? ERROR : VALID;
}

diskdriver::ret_t disk::close() {
    if(file == nullptr)
        BUFFER << LOG_str(log::WARNING, "FD can't be closed, as it's not initialised");

    uint32_t val = fclose(get_file());

    return val == EOF ? ERROR : VALID;
}

diskdriver::ret_t disk::read(void* ptr, const size_t& size, const uint32_t& amt) {
    size_t ttl_amt = fread(ptr, size, amt, file);

    if(ttl_amt != amt)
        LOG(log::ERROR_, "Error reading disk at '" + std::string(std::to_string(addr)) + "'.");

    return ttl_amt == amt ? VALID : ERROR;
}

diskdriver::ret_t disk::write(const void *ptr, const size_t &size, const uint32_t &amt) {
    size_t ttl_amt = fwrite(ptr, size, amt, file);

    if(ttl_amt != amt)
        LOG(log::ERROR_, "Error writing disk at '" + std::string(std::to_string(addr)) + "'.");

    return ttl_amt == amt ? VALID : ERROR;
}

diskdriver::ret_t disk::seek(const long &offset) {
    int8_t val = fseek(file, offset, SEEK_SET);

    if(val == -1)
        LOG(log::ERROR_, "Error setting offset address from 'SEEK_SET' within disk.");

    return val == -1 ? ERROR : VALID;
}

diskdriver::ret_t disk::truncate(const off_t &size) {
    int8_t val = ftruncate(fileno(file), size);

    if(val == -1)
        LOG(log::ERROR_, "Error truncating the file");

    return val == -1 ? ERROR : VALID;
}

diskdriver::ret_t disk::rm() {
    int8_t val = std::remove(cmpl_path_to_file.c_str());
    if(val != 0)
        BUFFER << LOG_str(log::WARNING, "Cant remove file(" + cmpl_path_to_file + ")");

    return val == 0 ? VALID : ERROR;
}