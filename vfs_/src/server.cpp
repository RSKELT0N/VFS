#include"../include/server.h"
#include <memory>

using namespace VFS::RFS;

extern void remote_interpret_cmd(VFS::vfs::system_cmd& cmd, std::vector<std::string>& args, char*& payload, uint64_t size, int8_t options = 0) noexcept;

server::server() {
    if(PORT_RANGE(CFG_DEFAULT_PORT)) {
        LOG(log::ERROR_, "Port specified is out of range, must be between (49151 - 65536)");
    }

    conn = std::make_unique<server::connect_t>();
    clients = std::make_unique<std::vector<std::pair<uint32_t, client_t*>>>();

    conn->m_port     = CFG_DEFAULT_PORT;
    info.max_usr_c  = CFG_SOCK_LISTEN_AMT;
    this->set_state(CFG_SOCK_OPEN);

    init();
}

server::~server() {
    this->set_state(CFG_SOCK_CLOSE);
    BUFFER << LOG_str(log::INFO, "Socket has been closed off for conntection");

    while(info.users_c) {
        sleep(1);
    }
}

void server::init() noexcept {
    BUFFER << "\n-------  server  ------\n";
    define_fd();
    set_sockopt();
    bind_sock();
    mark_listener();

    char str[10];
    sprintf(str, "%d", conn->m_port);

    BUFFER << LOG_str(log::SERVER, "server has been initialised: [Port : " + std::string(str) + "]");
    BUFFER << "---------  End  -------\n";
    this->run_ = std::thread(&server::run, this);
    this->run_.detach();
}

void server::define_fd() noexcept {
    if((conn->m_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        LOG(log::ERROR_, "Socket couldnt be created");
    } else BUFFER << LOG_str(log::SERVER, "Socket[Socket Stream] has been created");
}

void server::set_sockopt() noexcept {
    int res = 0;

    #ifndef _WIN32
        setsockopt(conn->m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &conn->opt, sizeof(conn->opt));
    #else
        char int_str[1];
        setsockopt(conn->m_socket_fd, SOL_SOCKET, SO_REUSEADDR, ((const char*)itoa_(conn->opt, int_str, 10, 1)), sizeof(conn->opt));
    #endif

    BUFFER << LOG_str(log::SERVER, "Socket level has been set towards: SOL_SOCKET");
}

void server::bind_sock() noexcept {
    conn->hint.sin_family      = AF_INET;
    conn->hint.sin_addr.s_addr = INADDR_ANY;
    conn->hint.sin_port        = htons(conn->m_port);

    if(bind(conn->m_socket_fd, (sockaddr*)&conn->hint, sizeof(conn->hint)) ==  -1) {
        LOG(log::ERROR_, "Issue binding socket to hint structure. Ensure port specified is not currently open prior to defining server.");
    } else BUFFER << LOG_str(log::SERVER, "Socket has been bound to hint structure");
}

void server::mark_listener() noexcept {
    if(listen(conn->m_socket_fd, CFG_SOCK_LISTEN_AMT) == -1) {
        LOG(log::ERROR_, "Socket failed to set listener");
    } else BUFFER << LOG_str(log::SERVER, "Socket is set for listening");
}

void server::run() noexcept {
    int master_sock = conn->m_socket_fd;
    uint32_t socket;
    sockaddr_in client{};
    socklen_t clientSize = sizeof(client);

    fd_set master_set;
    struct timeval tv{};

    while(info.state == CFG_SOCK_OPEN) {
        FD_ZERO(&master_set);
        FD_SET(conn->m_socket_fd, &master_set);
        tv.tv_sec = SOCKET_ACCEPT_TIME;
        
        select(master_sock + 1, &master_set, nullptr, nullptr, &tv);
        
        if(FD_ISSET(master_sock, &master_set)) {
            if((socket = accept(master_sock,(sockaddr *)&client,&clientSize)) == -1) {
                BUFFER << LOG_str(log::WARNING, "client socket has failed to join");
                continue;
            }

            if(info.users_c == info.max_usr_c) {
                BUFFER << LOG_str(log::WARNING, "client: [" + find_ip(client) + "] has tried to join, server is full");
                close(socket);
                continue;
            }

            info.users_c++;
            add_client(socket, client);
            BUFFER << LOG_str(log::SERVER, "client: [" + find_ip(client) + "] has joined");
        }
    }
    close(master_sock);
}

void server::add_client(const uint32_t& sock, const sockaddr_in& hint) noexcept {
    std::shared_ptr<client_t> tmp = std::shared_ptr<client_t>(new client_t);

    tmp->sock_fd = (int)sock;
    tmp->hint = hint;
    tmp->ip = find_ip(tmp->hint).c_str();
    tmp->state = CFG_SOCK_OPEN;
    set_recieved_ping(*tmp, 1);

    clients->push_back(std::make_pair(clients->size(), tmp.get()));
    tmp->thrd = std::thread(&server::handle, this, tmp);
    tmp->thrd.detach();

    #if !defined _DEBUG_
    tmp->ping = std::thread(&server::ping, this, (tmp));
    tmp->ping.detach();
    #endif
    
    BUFFER.hold_buffer();

    BUFFER << "\r";
    if(vfs::get_vfs()->is_mnted()) {
        BUFFER << LOG_str(log::INFO, "rfs has a mounted disk");
        ((VFS::IFS::fat32*)vfs::get_vfs()->get_mnted_system()->mp_fs.get())->print_super_block();
    } else BUFFER << LOG_str(log::INFO, "rfs has no mounted disk");
    
    BUFFER << "\n";

    send_to_client(*tmp, type_t::internal);
    BUFFER.release_buffer();
}

void server::handle(const std::shared_ptr<client_t>& client) noexcept {
    while(info.state == CFG_SOCK_OPEN && client->state == CFG_SOCK_OPEN) {
        receive(*client);
    }
    BUFFER << LOG_str(log::SERVER, "client [" + find_ip(client->hint) + "] disconnected");
    remove_client(client.get());
}


void server::remove_client(client_t* client) noexcept {
    for(auto it = clients->begin(); it != clients->end(); it++) {
        if(it->second->sock_fd == client->sock_fd) {
            clients->erase(it);
            break;
        }
    }
    info.users_c--;
}

void server::receive(client_t& client) noexcept {
    fd_set master_set;
    struct timeval tv{};

    FD_ZERO(&master_set);
    FD_SET(client.sock_fd, &master_set);
    tv.tv_sec = SOCKET_ACCEPT_TIME;

    select(client.sock_fd + 1, &master_set, nullptr, nullptr, &tv);
        
    if(FD_ISSET(client.sock_fd, &master_set)) {
        int val = 0;
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        // declare container to store info and payload packets.
        std::shared_ptr<pcontainer_t> container =  std::shared_ptr<pcontainer_t>(new pcontainer_t);
        // recv info packet and store it within the container info address.
        recv_(buffer, client, sizeof(packet_t));
        // deserialize packet.
        deserialize_packet(container->info, buffer);

        if(container->info.cmd == (int8_t)type_t::ping) {
            set_recieved_ping(client, 1);
            return;
        }

        if(process_packet(container->info) == 0) {
            set_state(client, CFG_SOCK_CLOSE);
            return;
        }

        // check whether info has any payload.
        if(container->info.p_count > 0) {
            m_ping.lock();
            #if _DEBUG_
                LOG(log::INFO, "Ping stopped");
            #endif
            payload_t tmp;
            // check whether first payload has any mf flag set to 0x1 and then the last 0x0.
            for(int i = 0; i < container->info.p_count; i++) {
                // reset buffer array.
                memset(buffer, 0, BUFFER_SIZE);
                memset(tmp.payload, 0, CFG_PAYLOAD_SIZE);
                // recv a payload
                recv_(buffer, client, sizeof(payload_t));

                deserialize_payload(tmp, buffer);

                if(process_payload(container->info, tmp) == 0) {
                    set_state(client, CFG_SOCK_CLOSE);
                    return;
                }

                container->payloads->push_back(tmp);
            }
            m_ping.unlock();
            #if _DEBUG_
                LOG(log::INFO, "Ping started");
            #endif
        }

        std::thread handle_data = std::thread(&server::interpret_input, this, container, (client_t*)&client);
        handle_data.detach();
    }
}

void server::send_to_client(client_t& client, type_t cmd, const std::string& ext_filename) noexcept {
    uint64_t buffer_size = BUFFER.mStream->size();

    if(buffer_size == 0)
        return;

    std::shared_ptr<std::byte[]> stream;
    BUFFER.retain_buffer(stream);
    stream[buffer_size - 1] = std::byte{0};

    std::vector<std::string> flags;
    std::unique_ptr<pcontainer_t> container = nullptr;

    if(cmd == type_t::external)
        flags.emplace_back(ext_filename);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    container = generate_container((int8_t)cmd, flags, stream, buffer_size);
    serialize_packet(container->info, buffer);
    send(buffer, client, sizeof(packet_t));

    if(container->info.p_count > 0) {
        m_ping.lock();
        #if _DEBUG_
        LOG(log::INFO, "Ping stopped");
        #endif
        for(int i = 0; i < container->info.p_count; i++) {
            memset(buffer, 0, BUFFER_SIZE);
            serialize_payload(container->payloads->at(i), buffer);
            send(buffer, client, sizeof(payload_t));
        }
        m_ping.unlock();
        #if _DEBUG_
            LOG(log::INFO, "Ping started");
        #endif
    }

    BUFFER.release_buffer();
}

void server::interpret_input(const std::shared_ptr<pcontainer_t>& container, client_t* client) noexcept {
    BUFFER.hold_buffer();

    std::vector<std::string> args = lib_::split(container->info.flags, ' ');
    auto cmd = (vfs::system_cmd)container->info.cmd;

    std::shared_ptr<char[]> payload = nullptr;
    uint64_t size = {};
    if(container->info.p_count != 0) {
        size = retain_payloads(payload, *container->payloads);
    }

    type_t dest = internal;
    if(!args.empty())
        dest = (strcmp(args[0].c_str(), "exp") == 0) ? external : internal;
    int8_t exportData = (dest == type_t::external) ? 1 : 0;

    std::string ext_filename = (dest == external) ? args[args.size() - 1] : "";
    remote_interpret_cmd(cmd, args, (char*&)payload, size, exportData);
    send_to_client(*client, dest, ext_filename);

    #if _DEBUG_
        print_packet(container->info);
        for(int i = 0; i < container->payloads->size(); i++) {
            print_payload(container->payloads->at(i));
        }
    #endif // _DEBUG_
    
    BUFFER.release_buffer();
}

void server::send(const char* buffer, client_t& client, size_t buffer_size) noexcept {
    m_send.lock();
    int number_of_bytes = {};
    size_t bytes_sent = {};

    while(number_of_bytes < buffer_size && (bytes_sent = ::send(client.sock_fd, buffer, (buffer_size - number_of_bytes), MSG_NOSIGNAL))) {
        number_of_bytes += bytes_sent;
        buffer += bytes_sent;

        if(bytes_sent == -1) {
            BUFFER << LOG_str(log::ERROR_, "Issue sending information back towards client");
            m_send.unlock();
            return;
        }
    }
    m_send.unlock();
}

void server::recv_(char* buffer, client_t& client, size_t bytes) noexcept {
    m_recieve.lock();
    int number_of_bytes = {};
    size_t bytes_received = {};

    while(number_of_bytes < bytes && (bytes_received = recv(client.sock_fd, buffer, (bytes - number_of_bytes), MSG_NOSIGNAL)) > 0) {
        number_of_bytes += bytes_received;
        buffer += bytes_received;

        if(bytes_received == -1) {
            *buffer = '\0';
            BUFFER << LOG_str(log::ERROR_, "Issue receiving data from client");
        }
    }
    m_recieve.unlock();
}

std::string server::find_ip(const sockaddr_in& sock) const noexcept {
    unsigned long addr = sock.sin_addr.s_addr;
    char buffer[50];
    sprintf(buffer, "%d.%d.%d.%d",
            (int)(addr  & 0xff),
            (int)((addr & 0xff00) >> 8),
            (int)((addr & 0xff0000) >> 16),
            (int)((addr & 0xff000000) >> 24)); 
    return std::string(buffer);
}

void server::ping(std::shared_ptr<client_t> client) noexcept {
    int8_t unrecievedAmt = 0;

    while(info.state == CFG_SOCK_OPEN && client->state == CFG_SOCK_OPEN) {
        sleep(1);
        if(client->recieved_ping == 1) {
            m_ping.lock();
            ping_client(client.get());

            set_recieved_ping(*client, 0);
            #if _DEBUG_
                LOG(log::INFO, "Ping");
            #endif
            unrecievedAmt ^= unrecievedAmt;
        } else unrecievedAmt++;

        if(unrecievedAmt == 10) {
            BUFFER << LOG_str(log::WARNING, "client has not replied towards ping, disconnecting..");
            set_state(*client.get(), CFG_SOCK_CLOSE);
        }
        m_ping.unlock();
    }
}

void server::ping_client(client_t* client) noexcept {
    std::string ping = "PING";
    std::vector<std::string> flags;

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    std::shared_ptr<std::byte[]> bytes = std::shared_ptr<std::byte[]>(new std::byte[ping.length()]);
    std::memcpy(bytes.get(), ping.data(), ping.length());

    std::unique_ptr<pcontainer_t> container = generate_container((int8_t)type_t::ping, flags, bytes, ping.length());
    serialize_packet(container->info, buffer);
    send(buffer, *client, sizeof(packet_t));
}

void server::set_recieved_ping(client_t& client, int8_t val) noexcept {
    m_lock.lock();
    client.recieved_ping = val;
    m_lock.unlock();
}

void server::set_state(int8_t val) noexcept {
    m_lock.lock();
    this->info.state = val;
    m_lock.unlock();

}

void server::set_state(client_t& client, int8_t val) noexcept {
    m_lock.lock();
    client.state = val;
    m_lock.unlock();
}