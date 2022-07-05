#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <unistd.h>
#include <thread>

#include "vfs.h"
#include "rfs.h"
#include "lib.h"
#include "buffer.h"

#ifndef _WIN32
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace VFS::RFS {

class client : public rfs {

    public:
        struct info_t {
            uint8_t state = {};
        }info;

        struct connect_t {
            int32_t m_port = {};
            const char* m_addr = {};
            int m_socket_fd = {};
            sockaddr_in hint = {};
        }conn;

    public:
        client(const char* addr, const int32_t& port);

        ~client();
        client(client&&) = delete;
        client(const client&) = delete;

    public:
        void receive_from_server() noexcept;
        uint8_t receive(char* buffer, size_t bytes) noexcept;
        void send(const char*, size_t) noexcept;
        uint64_t get_payload(const char*, std::vector<std::string>&, std::shared_ptr<std::byte[]>&) noexcept;
        void handle_send(const char*, uint8_t, std::vector<std::string>&) noexcept;
        void output_data(const pcontainer_t&, char*, int64_t) noexcept;

    private:
        void add_hint() noexcept;
        void run() noexcept override;
        void init() noexcept override;
        void define_fd() noexcept override;

    private:
        void connect() noexcept;
        void interpret_input(std::unique_ptr<pcontainer_t>) noexcept;
        void ping() noexcept;
        void ping_server() noexcept;
        void set_recieved_ping(int8_t) noexcept;
        void set_state(int8_t) noexcept;

    private:
        std::thread recv_;
        std::thread ping_;
        int8_t recieved_ping;
        int8_t disconnected = {};
    };
}

#endif // _CLIENT_H_