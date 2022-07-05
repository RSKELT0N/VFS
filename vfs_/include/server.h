#ifndef _SERVER_H_
#define _SERVER_H_

#include <vector>
#include <thread>
#include <utility>

#include "vfs.h"
#include "rfs.h"

#ifndef _WIN32
    #include <sys/time.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <unistd.h>
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
#endif

#define SOCKET_ACCEPT_TIME    (time_t)3

#define EMPH_START            (uint32_t)49152
#define EMPH_END              (uint32_t)65535
#define PORT_RANGE(__port__)  (__port__ < EMPH_START || __port__ >  EMPH_END) ? 1 : 0

namespace VFS::RFS {

    class server : public rfs {

    private:
        struct connect_t {
            uint32_t m_port = {};
            int m_socket_fd = {};
            sockaddr_in hint = {};
            int opt = 1;
        };

        struct info_t {
            uint8_t state = {};
            uint8_t users_c = {};
            uint8_t max_usr_c = {};
        }info;

        struct client_t {
            uint8_t state = {};
            int sock_fd = {};
            sockaddr_in hint = {};
            const char* ip = {};
            int8_t recieved_ping = {};


            std::thread thrd = {};
            std::thread ping = {};

            ~client_t() {
                    state = CFG_SOCK_CLOSE;
                    close(sock_fd);
                }
        };

    public:
        server();
        ~server();
        server(const server&) = delete;
        server(server&&)      = delete;

    private:
        void init() noexcept override;
        void define_fd() noexcept override;

    public:
        void run() noexcept override;
        void receive(client_t&) noexcept;
        void recv_(char* buffer, client_t&, size_t bytes) noexcept;
        void send(const char* buffer, client_t&, size_t buffer_size) noexcept;
        void interpret_input(const std::shared_ptr<pcontainer_t>&, client_t*) noexcept;
        void send_to_client(client_t&, type_t cmd, const std::string& ext_filename = "") noexcept;

    private:
        void bind_sock() noexcept;
        void set_sockopt() noexcept;
        void mark_listener() noexcept;
        void set_state(int8_t) noexcept;
        void ping_client(client_t*) noexcept;
        void set_state(client_t&, int8_t) noexcept;
        void ping(std::shared_ptr<client_t>) noexcept;
        void remove_client(client_t* client) noexcept;
        void handle(const std::shared_ptr<client_t>&) noexcept;
        void set_recieved_ping(client_t&, int8_t) noexcept;
        std::string find_ip(const sockaddr_in& sock) const noexcept;
        void add_client(const uint32_t& sock, const sockaddr_in& hint) noexcept;

    private:
        std::thread run_;
        std::unique_ptr<server::connect_t> conn;
        std::unique_ptr<std::vector<std::pair<uint32_t, client_t*>>> clients;
    };

}

#endif // _SERVER_H_