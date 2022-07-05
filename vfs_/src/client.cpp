#include "../include/client.h"
#include <memory>

using namespace VFS::RFS;

client::client(const char* addr, const int32_t& port) {
    conn.m_addr = addr;
    conn.m_port = port;
    set_state(CFG_SOCK_OPEN);
    set_recieved_ping(0);
    client::init();
}

client::~client() {
    set_state(CFG_SOCK_CLOSE);
}

void client::init() noexcept {
    define_fd();
    add_hint();
    connect();
    std::string addr = conn.m_addr;

    BUFFER << LOG_str(log::INFO, "You have successfully connected to the mp_vfs: [address : " + addr + "]");
    recv_ = std::thread(&client::run, this);
    recv_.detach();

    #if !defined _DEBUG_
    ping_ = std::thread(&client::ping, this);
    ping_.detach();
    #endif
}

void client::define_fd() noexcept {
    if((conn.m_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        BUFFER << LOG_str(log::ERROR_, "Socket[SOCK_STREAM] could not be created");
        return;
    }
}

void client::add_hint() noexcept {
    conn.hint.sin_family = AF_INET;
    conn.hint.sin_port   = htons(conn.m_port);

    unsigned long res = inet_addr(conn.m_addr);
    if(res == INADDR_NONE) {
        BUFFER << LOG_str(log::ERROR_, "Address specified is not valid");
        return;
    } else conn.hint.sin_addr = in_addr{(in_addr_t)res};
}

void client::connect() noexcept {
    if(::connect(conn.m_socket_fd, (sockaddr*)&conn.hint, sizeof(conn.hint)) == -1) {
        LOG(log::ERROR_, "Could not connect to mp_vfs, please try again or check IP/ Port specified. Make sure server, is initialised");
    }
}

void client::run() noexcept {
    while(info.state == CFG_SOCK_OPEN) {
        receive_from_server();
    }
    if(disconnected) {
        std::vector<std::string> args;
        vfs::get_vfs()->umnt_disk(args);
    }
    close(conn.m_socket_fd);
    BUFFER << (LOG_str(log::SERVER, "Disconnected from server, either it has crashed or closed off it's connection"));
}

void client::handle_send(const char* str_cmd, uint8_t cmd, std::vector<std::string>& args) noexcept {
    std::shared_ptr<std::byte[]> payload = nullptr;
    uint64_t size = get_payload(str_cmd, args, payload);
    std::unique_ptr<pcontainer_t> container = generate_container(cmd, args, payload, size);

    // Serialize packet.
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    serialize_packet(container->info, buffer);

    // sending info packet towards server, with command info related to input. Formatted ispl.
    send(buffer, sizeof(packet_t));

    if(container->info.p_count > 0) {
        double data_sent = 0;
        double data_size = container->info.size;
        data_size -= (sizeof(packet_t) * 8);
        m_ping.lock();
        #if _DEBUG_
            LOG(log::INFO, "Ping stopped");
        #endif
        
        for(int i = 0; i < container->info.p_count; i++) {
            memset(buffer, 0, BUFFER_SIZE);
            // Serialize payload
            serialize_payload(container->payloads->at(i), buffer);
            // send payload per fragment.
            send(buffer, sizeof(payload_t));
            data_sent += container->payloads->at(i).header.size;

            lib_::printProgress((double)data_sent/data_size);
        }
        if(data_sent != 0) {
            lib_::printProgress((double)data_sent/data_size);
            printf("\n");
        }
        m_ping.unlock();
        #if _DEBUG_
            LOG(log::INFO, "Ping started");
        #endif
    }
    //packet has been sent.
}

void client::receive_from_server() noexcept {
    int master_sock = conn.m_socket_fd;
    fd_set master_set;
    struct timeval tv{};

    FD_ZERO(&master_set);
    FD_SET(conn.m_socket_fd, &master_set);
    tv.tv_sec = SOCKET_ACCEPT_TIME;

    select(master_sock + 1, &master_set, NULL, NULL, &tv);
        
    if(FD_ISSET(master_sock, &master_set)) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        receive(buffer, sizeof(packet_t));
            
        std::unique_ptr<pcontainer_t> container = std::make_unique<pcontainer_t>();
        deserialize_packet(container->info, buffer);

        if(container->info.cmd == (int8_t)type_t::ping) {
            set_recieved_ping(1);
            return;
        }

        if(process_packet(container->info) == 0 && info.state == CFG_SOCK_OPEN) {
            set_state(CFG_SOCK_CLOSE);
            disconnected = 1;
            return;
        }
           
        if(container->info.p_count > 0) {
            double data_sent = 0;
            double data_size = container->info.size;
            data_size -= (sizeof(packet_t) * 8);
            bool ext = (container->info.cmd == (int8_t)type_t::external);
            m_ping.lock();
            #if _DEBUG_
                LOG(log::INFO, "Ping stopped");
            #endif
            
            payload_t tmp_payload;
            for(int i = 0; i < container->info.p_count; i++) {
                memset(buffer, 0, BUFFER_SIZE);
                receive(buffer, sizeof(payload_t));
                deserialize_payload(tmp_payload, buffer);
                
                if(process_payload(container->info, tmp_payload) == 0) {
                    set_state(CFG_SOCK_CLOSE);
                    disconnected = 1;
                    return;
                }
                container->payloads->push_back(tmp_payload);
                data_sent += container->payloads->at(i).header.size;

                if(ext)
                    lib_::printProgress((double)data_sent/data_size);
            }
            if(data_sent != 0 && ext) {
                lib_::printProgress((double)data_sent/data_size);
                printf("\n");
            }
            m_ping.unlock();
            #if _DEBUG_
                LOG(log::INFO, "Ping started");
            #endif
        }

        std::thread handle_data = std::thread(&client::interpret_input, this, std::move(container));
        handle_data.detach();
    }
}

void client::interpret_input(std::unique_ptr<pcontainer_t> container) noexcept {
    BUFFER.hold_buffer();

    uint64_t payload_size = 1;

    for(int i = 0; i < container->info.p_count; i++)
        payload_size += container->payloads->at(i).header.size;

    std::unique_ptr<char[]> buffer = std::unique_ptr<char[]>(new char[payload_size]);
    memset(buffer.get(), 0, payload_size);

    uint64_t data_copied = 0;
    for(int i = 0; i < container->info.p_count; i++) {
        memcpy(&(buffer[data_copied]), container->payloads->at(i).payload, container->payloads->at(i).header.size);
        data_copied += container->payloads->at(i).header.size;
    }

    buffer[payload_size - 1] = '\n';
    output_data(*container, buffer.get(), payload_size);
    BUFFER.release_buffer();
}

void client::output_data(const pcontainer_t& container, char* buffer, int64_t size) noexcept {
    if(container.info.cmd == (int8_t)type_t::external) {

        std::shared_ptr<std::byte[]> bytes = std::shared_ptr<std::byte[]>(new std::byte[size]);
        memcpy(bytes.get(), buffer, size);
        std::string filename = container.info.flags;
        filename[filename.size() - 1] = '\0';

        store_ext_file_buffer(filename.c_str(), bytes, (uint64_t)size);
    } else if(container.info.cmd == (int8_t)type_t::internal) {
        BUFFER.append(buffer, size);
        memset(buffer, 0, size);
        BUFFER.print_stream();
    }
}

void client::send(const char* buffer, size_t buffer_size) noexcept {
    m_send.lock();
    int number_of_bytes = {};
    size_t bytes_sent = {};

    while(number_of_bytes < buffer_size && (bytes_sent = ::send(conn.m_socket_fd, buffer, (buffer_size - number_of_bytes), MSG_NOSIGNAL))) {
        number_of_bytes += bytes_sent;
        buffer += bytes_sent;

        if(bytes_sent == -1) {
            BUFFER << LOG_str(log::WARNING, "input could not be sent towards remote mp_vfs");
            m_send.unlock();
            return;
        }
    }
    m_send.unlock();
}

uint8_t client::receive(char* buffer, size_t bytes) noexcept {
    m_recieve.lock();
    int number_of_bytes = {};
    size_t bytes_received = {};

    while(number_of_bytes < bytes && (bytes_received = recv(conn.m_socket_fd, buffer, (bytes - number_of_bytes), MSG_NOSIGNAL)) > 0) {
        number_of_bytes += bytes_received;
        buffer += bytes_received;

        if(bytes_received == -1) {
            BUFFER << (LOG_str(log::WARNING, "client was unable to read incoming data from mounted server"));
            m_recieve.unlock();
            return -1;
        }
    }
    m_recieve.unlock();
    return 1;
}

uint64_t client::get_payload(const char* cmd, std::vector<std::string>& args, std::shared_ptr<std::byte[]>& payload) noexcept {
    uint64_t size = {};
    if(strcmp(cmd, "cp") == 0) {
        if(strcmp(args[0].c_str(), "imp") == 0)
            size = fs::get_ext_file_buffer(args[1].c_str(), payload);
    }

    if(payload == nullptr) {
        payload = std::shared_ptr<std::byte[]>(new std::byte);
    }
    return size;
}

void client::ping() noexcept {
    std::vector<std::string> args;
    int8_t unrecievedAmt = 0;

    while(info.state == CFG_SOCK_OPEN) {
        sleep(1);

        if(recieved_ping == 1) {
            m_ping.lock();
            ping_server();
            set_recieved_ping(0);

            #if _DEBUG_
                LOG(log::INFO, "Ping");
            #endif
            
            unrecievedAmt ^= unrecievedAmt;
        } else unrecievedAmt++;

        if(unrecievedAmt == 10) {
            BUFFER << LOG_str(log::WARNING, "Have not recieved ping from server, disconnecting..");
            std::vector<std::string> args;
            vfs::get_vfs()->umnt_disk(args);
            set_state(CFG_SOCK_CLOSE);
        }
        m_ping.unlock();
    }
}

void client::ping_server() noexcept {
    std::string ping = "PING";
    std::vector<std::string> flags;

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    std::shared_ptr<std::byte[]> bytes = std::shared_ptr<std::byte[]>(new std::byte[ping.length()]);
    std::memcpy(bytes.get(), ping.data(), ping.length());

    std::unique_ptr<pcontainer_t> container = generate_container((int8_t)type_t::ping, flags, bytes, ping.length());
    serialize_packet(container->info, buffer);
    send(buffer, sizeof(packet_t));
}

void client::set_recieved_ping(int8_t val) noexcept {
    m_lock.lock();
    this->recieved_ping = val;
    m_lock.unlock();
}

void client::set_state(int8_t val) noexcept {
    m_lock.lock();
    this->info.state = val;
    m_lock.unlock();
}
