#ifndef _RFS_H_
#define _RFS_H_

#include <thread>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include "buffer.h"
#include "fs.h"

namespace VFS::RFS {

    class rfs : public fs {

    protected:
        struct packet_t {
            char signature[CFG_PACKET_SIGNATURE_SIZE]{};
            uint64_t size = {};
            uint64_t p_count = {};
            int8_t cmd = {};
            char hash[CFG_PACKET_HASH_SIZE]{};
            char flags[CFG_FLAGS_BUFFER_SIZE]{};
        } __attribute__((packed));

        struct payload_header_t {
            char hash[CFG_PACKET_HASH_SIZE]{};
            uint64_t index{};
            uint64_t size{};
            uint8_t mf : 1{};
        } __attribute__((packed));

        struct payload_t {
            payload_header_t header;
            char payload[CFG_PAYLOAD_SIZE]{};
        } __attribute__((packed));

        enum type_t {
            internal = -1,
            external = -2,
            ping = -3
        };

        struct pcontainer_t {
            packet_t info;
            std::vector<payload_t>* payloads = new std::vector<payload_t>();

            ~pcontainer_t() { delete payloads; }
        };

    public:
        rfs() = default;
        ~rfs() override = default;
        rfs(const rfs&) = delete;
        rfs(rfs&&) = delete;

    public:
        __attribute__((unused)) virtual void init() noexcept = 0;
        __attribute__((unused)) virtual void run() noexcept = 0;
        __attribute__((unused)) virtual void define_fd() noexcept = 0;


    protected:
        void serialize_packet(packet_t&, char*) noexcept;
        void deserialize_packet(packet_t&, char*) noexcept;
        [[nodiscard]] uint8_t process_packet(const packet_t&) const noexcept;

        void serialize_payload(payload_t&, char*) noexcept;
        void deserialize_payload(payload_t&, char*) noexcept;
        [[nodiscard]] uint8_t process_payload(const packet_t&, const payload_t&) const noexcept;

        void print_packet(const packet_t&) const noexcept;
        void print_payload(const payload_t&) const noexcept;
        std::shared_ptr<char[]> generate_hash() noexcept;

    public:
        uint64_t retain_payloads(std::shared_ptr<char[]>&, std::vector<payload_t>&) noexcept;
        std::unique_ptr<pcontainer_t> generate_container(uint8_t cmd, std::vector<std::string>& args, std::shared_ptr<std::byte[]>& payload, uint64_t size) noexcept;

    public:
        std::mutex m_send;
        std::mutex m_recieve;
        std::mutex m_lock;
        std::mutex m_ping;

        constexpr static size_t BUFFER_SIZE = sizeof(packet_t) + sizeof(payload_t);
    };
}

#endif // _RFS_H_
