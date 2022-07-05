#include "../include/rfs.h"
#include <memory>

using namespace VFS::RFS;

std::unique_ptr<rfs::pcontainer_t> rfs::generate_container(uint8_t cmd, std::vector<std::string>& args, std::shared_ptr<std::byte[]>& payload, uint64_t size) noexcept {
        m_lock.lock();
        std::unique_ptr<pcontainer_t> con = std::make_unique<pcontainer_t>();
        std::shared_ptr<char[]> hash = generate_hash();
        memcpy(con->info.hash, hash.get(), CFG_PACKET_HASH_SIZE);

        std::string flags = {};

        for(const auto& a : args)
            flags += a + " ";

        flags[flags.size() - 2] += '\0';
        
        uint64_t load_size = size;
        uint64_t amount_of_payloads = 0;
        size_t data_read = {};

        if(load_size > 0) {
    
            amount_of_payloads = load_size / CFG_PAYLOAD_SIZE;

            if(load_size > CFG_PAYLOAD_SIZE)
                if(load_size % CFG_PAYLOAD_SIZE)
                    amount_of_payloads++;

            if(amount_of_payloads == 0)
                amount_of_payloads += 1;

            std::unique_ptr<payload_t> tmp = std::make_unique<payload_t>();
            memcpy(tmp->header.hash, con->info.hash, CFG_PACKET_HASH_SIZE);
            tmp->header.size = CFG_PAYLOAD_SIZE;
            tmp->header.mf = 0x1;
            for(uint64_t i = 0; i < (amount_of_payloads - 1); i++) {
                tmp->header.index = i;
                memcpy(tmp->payload, payload.get() + data_read, CFG_PAYLOAD_SIZE);

                con->payloads->push_back(*tmp);
                data_read += CFG_PAYLOAD_SIZE;
            }

            uint64_t remaining_data = load_size - data_read;

            tmp->header.mf = 0x0;
            tmp->header.size = remaining_data;
            tmp->header.index = amount_of_payloads - 1;

            memcpy(tmp->payload, payload.get() + data_read, remaining_data);
            con->payloads->push_back(*tmp);

            data_read += load_size - data_read;
        }
        memcpy(con->info.signature, CFG_PACKET_SIGNATURE, CFG_PACKET_SIGNATURE_SIZE);
        con->info.cmd = cmd;

        if(flags.size() < CFG_FLAGS_BUFFER_SIZE) {
            memcpy(con->info.flags, flags.c_str(), flags.size());
        } else memcpy(con->info.flags, flags.c_str(), CFG_FLAGS_BUFFER_SIZE);
        
        con->info.p_count = amount_of_payloads;
        con->info.size = load_size + (sizeof(packet_t) * 8);

        m_lock.unlock();

        return std::move(con);
}

uint64_t rfs::retain_payloads(std::shared_ptr<char[]>& buffer, std::vector<payload_t>& pylds) noexcept {
    uint64_t size = 0;
    for(int i = 0; i < pylds.size(); i++)
        size += pylds[i].header.size;

    if(size == 0)
        return 0;

    buffer = std::shared_ptr<char[]>(new char[size]);
    memset(buffer.get(), 0, size);
    
    uint64_t data_copied = 0;
    for(int i = 0; i < pylds.size(); i++) {
        memcpy(buffer.get() + data_copied, pylds[i].payload, pylds[i].header.size);
        data_copied += pylds[i].header.size;
    }
    buffer[data_copied] = '\0';
    return size;
}


void rfs::serialize_packet(packet_t& pckt, char* buffer) noexcept {
    // Store signature.
    char* sign_p = (char*)buffer;
    int sign_c = 0;

    while(sign_c < CFG_PACKET_SIGNATURE_SIZE) {
        *sign_p = pckt.signature[sign_c];
        sign_p++;
        sign_c++;
    }
    // Store size.
    uint64_t* size_p = (uint64_t*)sign_p;
    *size_p = pckt.size; size_p++;
    // Store p count.
    uint64_t* p_count_p = (uint64_t*)size_p;
    *p_count_p = pckt.p_count; p_count_p++;
    // Store cmd int.
    uint8_t* int_p = (uint8_t*)p_count_p;
    *int_p = pckt.cmd; int_p++;
    // Store hash char.
    char* hash_p = (char*)int_p;
    int hash_c = 0;
    while(hash_c < CFG_PACKET_HASH_SIZE) {
        *hash_p = pckt.hash[hash_c]; 
        hash_p++;
        hash_c++;
    }
    // Store flags char.
    char* char_p = (char*)hash_p;
    int flag_c = 0;

    while(flag_c < CFG_FLAGS_BUFFER_SIZE) {
        *char_p = pckt.flags[flag_c];
        char_p++;
        flag_c++;
    }
}

void rfs::deserialize_packet(packet_t& pckt, char* buffer) noexcept {
    // Store signature.
    char* sign_p = (char*)buffer;
    int sign_c = 0;

    while(sign_c < CFG_PACKET_SIGNATURE_SIZE) {
        pckt.signature[sign_c] = *sign_p;
        sign_p++;
        sign_c++;
    }
    // Store size into struct.
    uint64_t* size_p = (uint64_t*)sign_p;
    pckt.size = *size_p; size_p++;
    // Store p count.
    uint64_t* p_count_p = (uint64_t*)size_p;
    pckt.p_count = *p_count_p; p_count_p++;
    // Store cmd into struct.
    uint8_t* int_p = (uint8_t*)p_count_p;
    pckt.cmd = *int_p; int_p++;
    // Store hash char.
    char* hash_p = (char*)int_p;
    int hash_c = 0;
    while(hash_c < CFG_PACKET_HASH_SIZE) {
        pckt.hash[hash_c] = *hash_p; 
        hash_p++;
        hash_c++;
    }
    // Store flags into struct.
    char* char_p = (char*)hash_p;
    int flag_c = 0;

    while(flag_c < CFG_FLAGS_BUFFER_SIZE) {
        pckt.flags[flag_c] = *char_p;
        char_p++;
        flag_c++;
    }
}

void rfs::serialize_payload(payload_t& pyld, char* buffer) noexcept {
    // Store payload header.
    payload_header_t* header_p = (payload_header_t*)buffer;
    *header_p = pyld.header; header_p++;

    //Store payload.
    char* char_p = (char*)header_p;
    int pyld_c = 0;

    while(pyld_c < CFG_PAYLOAD_SIZE) {
        *char_p = pyld.payload[pyld_c];
        char_p++;
        pyld_c++;
    }
}

void rfs::deserialize_payload(payload_t& pyld, char* buffer) noexcept {
    // Store payload header.
    payload_header_t* header_p = (payload_header_t*)buffer;
    pyld.header = *header_p; header_p++;
    //Store payload into struct.
    char* char_p = (char*)header_p;
    int pyld_c = 0;

    while(pyld_c < CFG_PAYLOAD_SIZE) {
        pyld.payload[pyld_c] = *char_p;
        char_p++;
        pyld_c++;
    }
}

uint8_t rfs::process_payload(const packet_t& pckt, const payload_t& pyld) const noexcept {
    uint8_t return_val = 1;

    if(strncmp(pyld.header.hash, pckt.hash, CFG_PACKET_HASH_SIZE) != 0)
        return_val = 0;

    if(pyld.header.size > pckt.size)
        return_val = 0;

    if(pyld.header.index > pckt.p_count)
        return_val = 0;

    
    return return_val;
}

uint8_t rfs::process_packet(const packet_t& pckt) const noexcept {
    uint8_t return_val = 1;

    if(strncmp(pckt.signature, CFG_PACKET_SIGNATURE, CFG_PACKET_SIGNATURE_SIZE) != 0)
        return_val = 0;

    return return_val;
}

void rfs::print_packet(const packet_t& pckt) const noexcept {
    char buffer[2];
    lib_::itoa_(pckt.cmd, buffer, 10, 2);
    std::cout << "\n---- INFO ----\n -> cmd: [" << buffer << "]\n -> flags: [" << pckt.flags << "]\n -> Payload count: [" << pckt.p_count << "]\n--------------\n";
}

void rfs::print_payload(const payload_t& pyld) const noexcept {
    std::cout << "\n---- PAYLOAD ----\n -> index : [" << pyld.header.index << "]\n -> size : [" << pyld.header.size << "]\n -> payload : [";
    std::string stream = "";
    for(int i = 0; i < CFG_PAYLOAD_SIZE; i++) {
        stream += pyld.payload[i];
    }
    std::cout << stream.c_str();
    std::cout << "]\n-----------------\n";
}

std::shared_ptr<char[]> rfs::generate_hash() noexcept {
    std::shared_ptr<char[]> hash = std::shared_ptr<char[]>(new char[CFG_PACKET_HASH_SIZE]);


    for(int i = 0; i < CFG_PACKET_HASH_SIZE; i++) {
        int r = rand() % 122 + 36;
        hash[i] = (char)r;
    }
    return hash;
}
