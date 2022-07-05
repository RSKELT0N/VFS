#include "../include/fat32.h"

using namespace VFS::IFS;

const uint32_t fat32::CLUSTER_SIZE;
const uint64_t fat32::CLUSTER_AMT;

fat32::fat32(const char* disk_name) : DISK_NAME(disk_name), PATH_TO_DISK(std::string("disks/" + std::string(DISK_NAME)).c_str()) {

    if(check_config() == -1) {
        LOG(log::ERROR_, "Please fix issues before creating disk, in config.php");
        return;
    }

    m_disk = std::make_unique<disk>();
    init();
}

int8_t fat32::check_config() noexcept {
    int8_t ret = {};

    if(USER_SPACE > CFG_MAX_USER_SPACE_SIZE) {
        LOG(log::WARNING, "User space must less than CFG_MAX_USER_SPACE_SIZE");
        ret = -1;
    } else if(USER_SPACE < CFG_MIN_USER_SPACE_SIZE) {
        LOG(log::WARNING, "User space must be greater than CFG_MIN_USER_SPACE_SIZE");
        ret = -1;
    }

    return ret;
}

void fat32::init() noexcept {
    if (access(PATH_TO_DISK, F_OK) == -1)
        set_up();
    else load();
}

void fat32::set_up() noexcept {
    define_superblock();
    define_fat_table();

    for(int i = CLUSTER_AMT - 1; i >= 0; i--)
        m_free_clusters->insert(i);

    m_root = init_dir(0, 0, "root");
    m_curr_dir = m_root;

    create_disk();
    store_superblock();
    store_fat_table();
    store_dir(m_root);
    
    BUFFER << (LOG_str(log::INFO, "file system has been initialised."));
    
    print_super_block();
#if _DEBUG_
    print_fat_table();
#endif // _DEBUG_
}

void fat32::create_disk() noexcept {
    m_disk->open(DISK_NAME, (const char*)"wb");
    m_disk->truncate(STORAGE_SIZE);
    rewind(((disk*)m_disk.get())->get_file());

    m_disk->close();

    m_disk->open(DISK_NAME, "rb+");
    BUFFER << LOG_str(log::INFO, "binary file '" + std::string(DISK_NAME) + "' has been created.");
}

void fat32::define_superblock() noexcept {
    fat32::metadata_t data{};
    strcpy(data.disk_name, DISK_NAME);
    data.cluster_size = CLUSTER_SIZE;
    data.disk_size = STORAGE_SIZE;
    data.cluster_n = CLUSTER_AMT;
    data.superblock_size = SUPERBLOCK_SIZE;
    data.fat_table_size = FAT_TABLE_SIZE;
    data.user_size = USER_SPACE;

    m_superblock.data = data;
    m_superblock.superblock_addr = SUPERBLOCK_START_ADDR;
    m_superblock.fat_table_addr = FAT_TABLE_START_ADDR;
    m_superblock.root_dir_addr = ROOT_START_ADDR;
}

void fat32::define_fat_table() noexcept {
    m_free_clusters = std::make_unique<std::unordered_set<uint32_t>>();
    m_fat_table = std::unique_ptr<uint32_t[]>(new uint32_t[CLUSTER_AMT]);
    memset((void*)m_fat_table.get(), UNALLOCATED_CLUSTER, (size_t)CLUSTER_AMT);
}

std::shared_ptr<fat32::dir_t> fat32::init_dir(const uint32_t & start_cl, const uint32_t & parent_clu, const char* name) noexcept {
    dir_header_t hdr;
    auto tmp = std::shared_ptr<fat32::dir_t>(new dir_t());
    strcpy(hdr.dir_name, name);
    hdr.start_cluster_index = start_cl;
    hdr.parent_cluster_index = parent_clu;
    hdr.dir_entry_amt = 2;

    tmp->dir_header = hdr;
    tmp->dir_entries = std::shared_ptr<dir_entry_t[]>(new dir_entry_t[tmp->dir_header.dir_entry_amt]);

    strcpy(tmp->dir_entries[0].dir_entry_name, ".");
    tmp->dir_entries[0].start_cluster_index = start_cl;
    tmp->dir_entries[0].is_directory = DIRECTORY;
    tmp->dir_entries[0].dir_entry_size = sizeof(tmp->dir_entries[0]);

    strcpy(tmp->dir_entries[1].dir_entry_name, "..");
    tmp->dir_entries[1].start_cluster_index = parent_clu;
    tmp->dir_entries[1].is_directory = DIRECTORY;
    tmp->dir_entries[1].dir_entry_size = sizeof(tmp->dir_entries[1]);

    return tmp;
}

void fat32::store_superblock() noexcept {
    m_disk->seek(m_superblock.superblock_addr);
    m_disk->write((void*)&m_superblock, sizeof(m_superblock), 1);
}

void fat32::store_fat_table() noexcept {
    m_disk->seek(m_superblock.fat_table_addr);
    m_disk->write(m_fat_table.get(), sizeof(uint32_t), (uint32_t)CLUSTER_AMT);
}

void fat32::store_dir(std::shared_ptr<dir_t>& directory)  noexcept {

    if (CLUSTER_SIZE < sizeof(directory->dir_header) || CLUSTER_SIZE < sizeof(dir_entry_t)) {
        BUFFER << (LOG_str(log::ERROR_, "Insufficient memory to store header data/dir entry for directory"));
        return;
    }

    uint32_t first_clu_entry_amt = (CLUSTER_SIZE - sizeof(directory->dir_header)) / sizeof(dir_entry_t);
    uint32_t remain_entries = (first_clu_entry_amt >= directory->dir_header.dir_entry_amt) ? 0 : (directory->dir_header.dir_entry_amt - (uint32_t)first_clu_entry_amt);
    uint32_t amt_of_entry_per_clu = 0;
    uint32_t entries_written = 0;

    if (!n_free_clusters(1)) {
        BUFFER << (LOG_str(log::ERROR_, "amount of clusters needed is not valid"));
        return;
    }

    uint32_t first_clu_index = attain_clu();

    directory->dir_header.start_cluster_index = first_clu_index;
    directory->dir_entries[0].start_cluster_index = first_clu_index;

    if (remain_entries > 0)
        amt_of_entry_per_clu = CLUSTER_SIZE / sizeof(dir_entry_t);

    m_disk->seek(m_superblock.root_dir_addr + (first_clu_index * CLUSTER_SIZE));
    m_disk->write((void*)&directory->dir_header, sizeof(dir_header_t), 1);

    uint16_t num_of_clu_needed;
    if (remain_entries < amt_of_entry_per_clu)
        num_of_clu_needed = 1;
    else num_of_clu_needed = (uint16_t)((double)remain_entries / (double)amt_of_entry_per_clu);

    for (int i = 0; i < min_(directory->dir_header.dir_entry_amt, first_clu_entry_amt); i++) {
        size_t addr_offset = (ROOT_START_ADDR + (first_clu_index * CLUSTER_SIZE) + sizeof(dir_header_t));

        m_disk->seek(addr_offset + (i * sizeof(dir_entry_t)));
        m_disk->write((void*)&directory->dir_entries[i], sizeof(dir_entry_t), 1);
        entries_written += 1;
    }

    if (remain_entries == 0) {
        m_fat_table[first_clu_index] = EOF_CLUSTER;
        store_fat_table();
        return;
    }

    if (remain_entries > amt_of_entry_per_clu) {
        if (remain_entries % amt_of_entry_per_clu > 0)
            num_of_clu_needed++;
    }

    if (!n_free_clusters(num_of_clu_needed)) {
        BUFFER << (LOG_str(log::WARNING, "remaining entries cannot be stored due to insufficient cluster amount"));
        BUFFER << (LOG_str(log::WARNING, "'" + std::string(directory->dir_header.dir_name) + "' directory cannot be stored within: '" + std::string(DISK_NAME) + "'"));
        m_fat_table[first_clu_index] = UNALLOCATED_CLUSTER;
        m_free_clusters->insert(first_clu_index);
        return;
    }

    auto clu_list = std::unique_ptr<uint32_t[]>(new uint32_t[num_of_clu_needed]);
    memset(clu_list.get(), 0, num_of_clu_needed);

    for (int i = 0; i < num_of_clu_needed; i++)
        clu_list[i] = attain_clu();

    for (int i = 0; i < num_of_clu_needed - 1; i++) {
        size_t addr_offset = (ROOT_START_ADDR + (clu_list[i] * CLUSTER_SIZE));

        m_disk->seek(addr_offset);
        m_disk->write((void*)&directory->dir_entries[entries_written], sizeof(dir_entry_t), amt_of_entry_per_clu);
        remain_entries -= amt_of_entry_per_clu;
        entries_written += amt_of_entry_per_clu;
    }

    m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[num_of_clu_needed - 1]));
    m_disk->write((void*)&directory->dir_entries[entries_written], sizeof(dir_entry_t), remain_entries);
    fflush(((disk*)m_disk.get())->get_file());

    m_fat_table[first_clu_index] = clu_list[0];
    for (int i = 0; i < num_of_clu_needed; i++)
        m_fat_table[clu_list[i]] = clu_list[i + 1];
    m_fat_table[clu_list[num_of_clu_needed - 1]] = EOF_CLUSTER;

    store_fat_table();
}

void fat32::save_dir(std::shared_ptr<dir_t>& directory) noexcept {
    std::unique_ptr<std::vector<uint32_t>> alloc_clu = get_list_of_clu(directory->dir_header.start_cluster_index);

    for (int i = 0; i < alloc_clu->size(); i++) {
        m_fat_table[(*alloc_clu)[i]] = UNALLOCATED_CLUSTER;
        m_free_clusters->insert((*alloc_clu)[i]);
    }

    store_dir(directory);
}

void fat32::load() noexcept {
    BUFFER << (LOG_str(log::INFO, "Loading disk into memory..."));
    m_disk->open(DISK_NAME, "rb+");
    load_superblock();
    define_fat_table();
    load_fat_table();
    m_root = read_dir(0);
    m_curr_dir = m_root;
    BUFFER << (LOG_str(log::INFO, "disk '" + std::string(DISK_NAME) + "' has been loaded"));
    print_super_block();
    #if _DEBUG_
        print_fat_table();
    #endif
}

void fat32::load_superblock() noexcept {
    m_disk->seek(SUPERBLOCK_START_ADDR);
    m_disk->read((void*)&m_superblock, sizeof(superblock_t), 1);
}

void fat32::load_fat_table() noexcept {
    m_disk->seek(m_superblock.fat_table_addr);
    m_disk->read(m_fat_table.get(), sizeof(uint32_t), CLUSTER_AMT);

    for(int i = m_superblock.data.cluster_n - 1; i >= 0; i--) {
        if(m_fat_table[i] == UNALLOCATED_CLUSTER)
            m_free_clusters->insert(i);
    }
}

uint32_t fat32::insert_dir(std::shared_ptr<dir_t>& curr_dir, const char* dir_name) noexcept {
    std::shared_ptr<dir_t> tmp;
    uint32_t ret;

    tmp = init_dir(UNDEF_START_CLUSTER, curr_dir->dir_header.start_cluster_index, dir_name);
    store_dir(tmp);

    add_new_entry(curr_dir, dir_name, tmp->dir_header.start_cluster_index, sizeof(dir_entry_t), 0x1);
    save_dir(curr_dir);

    ret = tmp->dir_header.start_cluster_index;
    return ret;
}

std::shared_ptr<fat32::dir_t> fat32::read_dir(const uint32_t & start_clu) noexcept {
    if (m_fat_table[start_clu] == UNALLOCATED_CLUSTER) {
        BUFFER << (LOG_str(log::WARNING, "specified cluster has not been allocated"));
        return nullptr;
    }
    //tmp dir_t*, return
    auto ret = std::make_shared<dir_t>();

    uint32_t dir_start_addr = ROOT_START_ADDR + (CLUSTER_SIZE * start_clu);

    //attain dir_header
    m_disk->seek(dir_start_addr);
    m_disk->read((void*)&ret->dir_header, sizeof(dir_header_t), 1);

    uint32_t first_clu_entry_amt = (CLUSTER_SIZE - sizeof(ret->dir_header)) / sizeof(dir_entry_t);
    uint32_t remain_entries = (first_clu_entry_amt >= ret->dir_header.dir_entry_amt) ? 0 : (ret->dir_header.dir_entry_amt - (uint32_t)first_clu_entry_amt);
    uint32_t entries_read = 0;

    //allocate memory to ret(dir_t) entries due to dir header data.
    ret->dir_entries = std::shared_ptr<dir_entry_t[]>(new dir_entry_t[ret->dir_header.dir_entry_amt]);

    for (int i = 0; i < min_(first_clu_entry_amt, ret->dir_header.dir_entry_amt); i++) {
        size_t addr_offset = dir_start_addr + sizeof(dir_header_t);

        m_disk->seek(addr_offset + (i * sizeof(dir_entry_t)));
        m_disk->read((void*)&ret->dir_entries[i], sizeof(dir_entry_t), 1);
        entries_read += 1;
    }


    uint32_t amt_of_entries_per_clu;
    if (remain_entries > 0) {
        amt_of_entries_per_clu = CLUSTER_SIZE / sizeof(dir_entry_t);
    } else return ret;

    uint32_t amt_of_clu_used = remain_entries / amt_of_entries_per_clu;

    if (remain_entries > amt_of_entries_per_clu) {
        if (remain_entries % amt_of_entries_per_clu > 0)
            amt_of_clu_used++;
    }

    //move towards next clu
    uint32_t curr_clu = start_clu;
    curr_clu = m_fat_table[curr_clu];

    for (int i = 0; i < amt_of_clu_used - 1; i++) {
        size_t addr_offset = ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu);

        m_disk->seek(addr_offset);
        m_disk->read((void*)&ret->dir_entries[entries_read], sizeof(dir_entry_t), amt_of_entries_per_clu);
        entries_read += amt_of_entries_per_clu;
        curr_clu = m_fat_table[curr_clu];
    }

    uint32_t remain_entries_in_lst_clu = abs_(ret->dir_header.dir_entry_amt, entries_read);

    m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu));
    m_disk->read((void*)&ret->dir_entries[entries_read], sizeof(dir_entry_t), remain_entries_in_lst_clu);
    fflush(((disk*)m_disk.get())->get_file());

    return ret;
}

size_t fat32::read_file(std::shared_ptr<dir_t>& dir, const char* entry_name, std::shared_ptr<std::byte[]>& buffer) noexcept {
    fat32::dir_entry_t* entry_ptr = find_entry(dir, entry_name, 1);
    uint64_t entry_size = entry_ptr->dir_entry_size;

    buffer = std::shared_ptr<std::byte[]>(new std::byte[entry_size]);
    memset(buffer.get(), 0, entry_size);

    if (m_fat_table[entry_ptr->start_cluster_index] == UNALLOCATED_CLUSTER) {
        BUFFER << (LOG_str(log::WARNING, "cluster specified has not been allocated, file could not be read"));
    }

    uint64_t curr_clu = entry_ptr->start_cluster_index;
    uint64_t data_read = 0;

    while(m_fat_table[curr_clu] != EOF_CLUSTER) {
        uint64_t addr_offset = ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu);

        m_disk->seek(addr_offset);
        m_disk->read(buffer.get() + data_read, sizeof(char), CLUSTER_SIZE);
        curr_clu = m_fat_table[curr_clu];
        data_read += CLUSTER_SIZE;
    }

    uint32_t remaining_data = abs_(entry_size, data_read);

    m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu));
    m_disk->read(buffer.get() + data_read, sizeof(std::byte), remaining_data);
    fflush(((disk*)m_disk.get())->get_file());
    buffer[data_read + remaining_data] = std::byte{0};

    return (size_t)entry_size;
}

int32_t fat32::store_file(std::shared_ptr<std::byte[]>& data, uint64_t data_size) noexcept {
    uint32_t sdata = data_size;
    uint32_t first_cluster = 0;
    size_t data_read = 0;
    
    uint32_t amt_of_clu_needed = 0;

    if (sdata < CLUSTER_SIZE)
        amt_of_clu_needed = 1;
    else amt_of_clu_needed = sdata / CLUSTER_SIZE;

     if (sdata > CLUSTER_SIZE) {
        if (sdata % CLUSTER_SIZE > 0)
            amt_of_clu_needed++;
    }

    if (amt_of_clu_needed == 1) {
        first_cluster = attain_clu();

        m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * first_cluster));
        m_disk->write(data.get(), sizeof(char), sdata);
        data_read += sdata;

        m_fat_table[first_cluster] = EOF_CLUSTER;
        return first_cluster;
    }

    if (!n_free_clusters(amt_of_clu_needed)) {
        BUFFER << (LOG_str(log::WARNING, "amount of cluster needed isn't available to store file"));
        return -1;
    }

    auto clu_list = std::unique_ptr<uint32_t[]>(new uint32_t[amt_of_clu_needed]);
    memset(clu_list.get(), 0, amt_of_clu_needed);

    for (int i = 0; i < amt_of_clu_needed; i++) {
        clu_list[i] = attain_clu();
    }

    first_cluster = clu_list[0];

    for (int i = 0; i < amt_of_clu_needed - 1; i++) {
        size_t off_adr = ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[i]);

        m_disk->seek(off_adr);
        m_disk->write(data.get() + data_read, CLUSTER_SIZE, 1);

        data_read += CLUSTER_SIZE;
    }

    size_t remaining_data = abs_(sdata, data_read);

    m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[amt_of_clu_needed - 1]));
    m_disk->write(data.get() + data_read, remaining_data, 1);
    fflush(((disk*)m_disk.get())->get_file());

    m_fat_table[first_cluster] = clu_list[0];
    for (int i = 0; i < amt_of_clu_needed; i++)
        m_fat_table[clu_list[i]] = clu_list[i + 1];
    m_fat_table[clu_list[amt_of_clu_needed - 1]] = EOF_CLUSTER;

    return first_cluster;
}

void fat32::insert_int_file(std::shared_ptr<dir_t>& dir, std::shared_ptr<std::byte[]>& buffer, const char* name, size_t size) noexcept {
    std::shared_ptr<std::byte[]> data = buffer;

    uint32_t start_clu = store_file(data, size);

    if (start_clu == -1) {
        BUFFER << (LOG_str(log::WARNING, "file could not be stored"));
        return;
    }

    add_new_entry(dir, name, start_clu, size, 0);
    save_dir(dir);
}

void fat32::insert_ext_file(std::shared_ptr<dir_t>& curr_dir, const char* path, const char* name) noexcept {
    if (access(path, F_OK) == -1) {
        BUFFER << (LOG_str(log::WARNING, "file specified does not exist"));
        return;
    }

    long size = get_file_size(path); 
    std::shared_ptr<std::byte[]> buffer = nullptr;

    get_ext_file_buffer(path, buffer);
    uint32_t start_clu = store_file(buffer, (uint64_t)size);

    if (start_clu == -1) {
        BUFFER << (LOG_str(log::WARNING, "file could not be stored"));
        return;
    }

    add_new_entry(curr_dir, name, start_clu, size, 0);
    save_dir(curr_dir);
}

void fat32::delete_entry(std::unique_ptr<dir_entr_ret_t>& entry) noexcept {

    std::unique_ptr<std::vector<uint32_t>> entry_alloc_clu = get_list_of_clu(entry->m_entry->start_cluster_index);

    for (int i = 0; i < entry_alloc_clu->size(); i++) {
        m_fat_table[(*entry_alloc_clu)[i]] = UNALLOCATED_CLUSTER;
        m_free_clusters->insert((*entry_alloc_clu)[i]);
    }

    rm_entr_mem(entry->m_dir, entry->m_entry->dir_entry_name);
}

void fat32::delete_dir(std::shared_ptr<dir_t>& dir) noexcept {
    for(int i = dir->dir_header.dir_entry_amt - 1; i >= 2; i--) { // i >= 2, as 0 = '.' and 1 = '..'
        auto tmp = std::shared_ptr<fat32::dir_t>();
        if(dir->dir_entries[i].is_directory) {
            tmp = read_dir(dir->dir_entries[i].start_cluster_index);
            delete_dir(tmp);
        }
        dir_entry_t* entry = &dir->dir_entries[i];
        std::unique_ptr<dir_entr_ret_t> entr = std::make_unique<dir_entr_ret_t>(dir, entry);
        delete_entry(entr);
    }
}

std::unique_ptr<fat32::dir_entr_ret_t> fat32::parsePath(std::vector<std::string>&path, uint8_t shd_exst) noexcept {
    std::unique_ptr<fat32::dir_entr_ret_t> ret = std::unique_ptr<dir_entr_ret_t>(new dir_entr_ret_t(nullptr, nullptr));

    auto curr_dir = m_curr_dir;
    fat32::dir_entry_t* tmp_entr;

    for (int i = 0; i < path.size() - 1; i++) {
         tmp_entr = find_entry(curr_dir, path[i].c_str(), 0x1);

        if (tmp_entr == nullptr && shd_exst) {
            return nullptr;
        }

        std::shared_ptr<dir_t> tmp = read_dir(tmp_entr->start_cluster_index);
        curr_dir = tmp;
    }

    tmp_entr = find_entry(curr_dir, path[path.size() - 1].c_str(), shd_exst);

    if (!tmp_entr && shd_exst == 1) {
        return nullptr;
    }

    if(tmp_entr && shd_exst == 0) {
        return nullptr;
    }

    ret->m_dir = curr_dir;
    ret->m_entry = tmp_entr;
    return ret;
}

uint32_t fat32::attain_clu() const noexcept {
    uint32_t rs = *m_free_clusters->begin();
    m_fat_table[rs] = ALLOCATED_CLUSTER;
    m_free_clusters->erase(m_free_clusters->begin());
    return rs;
}

uint32_t fat32::n_free_clusters(const uint32_t& req) const noexcept {
    return m_free_clusters->size() >= req ? 1 : 0;
}

std::unique_ptr<std::vector<uint32_t>> fat32::get_list_of_clu(const uint32_t & start_clu) noexcept {
    std::unique_ptr<std::vector<uint32_t>> alloc_clu = std::unique_ptr<std::vector<uint32_t>>(new std::vector<uint32_t>());

    uint32_t curr_clu = start_clu;
    alloc_clu->push_back(curr_clu);

    while (1) {
        uint32_t next_clu = m_fat_table[curr_clu];
        if (next_clu == EOF_CLUSTER)
            break;
        alloc_clu->push_back(next_clu);
        curr_clu = next_clu;
    }
    return std::move(alloc_clu);
}

void fat32::rm_entr_mem(std::shared_ptr<dir_t>& dir, const char* name) noexcept {

    std::shared_ptr<dir_entry_t[]> tmp = std::shared_ptr<dir_entry_t[]>(new dir_entry_t[dir->dir_header.dir_entry_amt - 1]);

    for (int i = 0; i < dir->dir_header.dir_entry_amt; i++) {
        if (strcmp(dir->dir_entries[i].dir_entry_name, name) == 0) {
            for (int j = i; j < dir->dir_header.dir_entry_amt - 1; j++) {
                tmp[j] = dir->dir_entries[j + 1];
            }
            break;
        }
        tmp[i] = dir->dir_entries[i];
    }

    dir->dir_entries = tmp;
    dir->dir_header.dir_entry_amt--;
}

void fat32::add_new_entry(std::shared_ptr<dir_t>& curr_dir, const char* name, const uint32_t& start_clu, const uint64_t& size, const uint8_t& is_dir) noexcept {

    curr_dir->dir_header.dir_entry_amt += 1;
    std::shared_ptr<dir_entry_t[]> tmp_entries = curr_dir->dir_entries;
    curr_dir->dir_entries = std::shared_ptr<dir_entry_t[]>(new dir_entry_t[curr_dir->dir_header.dir_entry_amt]);

    for (int i = 0; i < curr_dir->dir_header.dir_entry_amt - 1; i++)
        curr_dir->dir_entries[i] = tmp_entries[i];

    strcpy(curr_dir->dir_entries[curr_dir->dir_header.dir_entry_amt - 1].dir_entry_name, name);
    curr_dir->dir_entries[curr_dir->dir_header.dir_entry_amt - 1].start_cluster_index = start_clu;
    curr_dir->dir_entries[curr_dir->dir_header.dir_entry_amt - 1].is_directory = is_dir;
    curr_dir->dir_entries[curr_dir->dir_header.dir_entry_amt - 1].dir_entry_size = size;
}

void fat32::cp_dir(std::shared_ptr<dir_t>& src, std::shared_ptr<dir_t>& dst) noexcept {
    for(int i = src->dir_header.dir_entry_amt - 1; i >= 2; i--) { // i >= 2, as 0 = '.' and 1 = '..'
        std::shared_ptr<dir_t> tmp = nullptr;
        if(src->dir_entries[i].is_directory) {
            tmp = read_dir(src->dir_entries[i].start_cluster_index);
            uint32_t cp_clu = insert_dir(dst, tmp->dir_header.dir_name);
            std::shared_ptr<dir_t> dir_cp = read_dir(cp_clu);

            cp_dir(tmp, dir_cp);
            continue;
        }
        std::shared_ptr<std::byte[]> buffer = nullptr;
        size_t size = read_file(src, src->dir_entries[i].dir_entry_name, buffer);
        insert_int_file(dst, buffer, src->dir_entries[i].dir_entry_name, size);
    }
}

void fat32::mv(std::vector<std::string>& tokens) noexcept {
    std::vector<std::string> parts = lib_::split(tokens[0].c_str(), '/');
    std::unique_ptr<dir_entr_ret_t> src = parsePath(parts, 0x1);

    parts = lib_::split(tokens[1].c_str(), '/');
    std::unique_ptr<dir_entr_ret_t> dst = parsePath(parts, 0x0);
    const char* entr_name = parts[parts.size() - 1].c_str();

    if(!src || !dst) {
        BUFFER << (LOG_str(log::WARNING, "Either src or dst specified is invalid"));
        return;
    }

    if(src->m_entry->is_directory) {
        add_new_entry(dst->m_dir, entr_name, src->m_entry->start_cluster_index, src->m_entry->dir_entry_size, DIRECTORY);

        std::shared_ptr<dir_t> mv_dir = read_dir(dst->m_dir->dir_entries[dst->m_dir->dir_header.dir_entry_amt - 1].start_cluster_index);
        mv_dir->dir_entries[1].start_cluster_index = dst->m_dir->dir_header.start_cluster_index;
        save_dir(mv_dir);
    } else
        add_new_entry(dst->m_dir, entr_name, src->m_entry->start_cluster_index, src->m_entry->dir_entry_size, NON_DIRECTORY);

    rm_entr_mem(src->m_dir, src->m_entry->dir_entry_name);

    save_dir(src->m_dir);
    save_dir(dst->m_dir);
}

void fat32::cp(const char* src, const char* dst) noexcept {
    std::vector<std::string> parts = lib_::split(src, '/');
    std::unique_ptr<dir_entr_ret_t> dsrc = parsePath(parts, 0x1);

    parts = lib_::split(dst, '/');
    std::unique_ptr<dir_entr_ret_t> ddst = parsePath(parts, 0x0);
    const char* entr_name = parts[parts.size() - 1].c_str();

    if(!dsrc || !ddst) {
        BUFFER << (LOG_str(log::WARNING, "Either src or dst specified is invalid"));
        return;
    }

    if(dsrc->m_entry->is_directory) {
        std::shared_ptr<dir_t> src_dir = read_dir(dsrc->m_entry->start_cluster_index);
        uint32_t dir_clu = insert_dir(ddst->m_dir, entr_name);
        std::shared_ptr<dir_t> dst_dir = read_dir(dir_clu);

        cp_dir(src_dir, dst_dir);
    } else {
        std::shared_ptr<std::byte[]> buffer;
        size_t size = read_file(dsrc->m_dir, dsrc->m_entry->dir_entry_name, buffer);
        insert_int_file(ddst->m_dir, buffer, entr_name, size);
    }

    save_dir(ddst->m_dir);
}

void fat32::cp_imp(const char* src, const char* dst) noexcept {
    std::vector<std::string> parts = lib_::split(dst, '/');

    std::unique_ptr<dir_entr_ret_t> ddst = parsePath(parts, 0x0);
    const char* entr_name = parts[parts.size() - 1].c_str();

    if(!ddst) {
        BUFFER << (LOG_str(log::WARNING, "dst specified is invalid"));
        return;
    }

    insert_ext_file(ddst->m_dir, src, entr_name);
}

void fat32::cp_exp(const char* src, const char* dst) noexcept {
    std::vector<std::string> parts = lib_::split(src, '/');

    std::unique_ptr<dir_entr_ret_t> ssrc = parsePath(parts, 0x1);
    const char* entr_name = parts[parts.size() - 1].c_str();

    if(!ssrc) {
        BUFFER << (LOG_str(log::WARNING, "src specified is invalid"));
        return;
    }
    std::shared_ptr<std::byte[]> buffer;
    size_t size = read_file(ssrc->m_dir, parts[parts.size() - 1].c_str(), buffer);
    store_ext_file_buffer(dst, buffer, size);
}

void fat32::mkdir(const char* dir) noexcept {
    std::vector<std::string> tokens = lib_::split(dir, '/');
    std::unique_ptr<fat32::dir_entr_ret_t> ret = parsePath(tokens, 0x0);

    if (!ret) {
        BUFFER << (LOG_str(log::WARNING, "Path specified is invalid"));
        return;
    }
    insert_dir(ret->m_dir, tokens[tokens.size() - 1].c_str());
}

void fat32::cd(const char* pth) noexcept {
    std::vector<std::string> tokens = lib_::split(pth, '/');
    std::unique_ptr<fat32::dir_entr_ret_t> ret = parsePath(tokens, 0x1);

    if (!ret) {
        BUFFER << (LOG_str(log::WARNING, "entry does not exist"));
        return;
    }

    if (!ret->m_entry->is_directory) {
        BUFFER << (LOG_str(log::WARNING, "entry '" + std::string(ret->m_entry->dir_entry_name) + "' is not a directory"));
        return;
    }
    m_curr_dir = read_dir(ret->m_entry->start_cluster_index);
}

void fat32::rm(std::vector<std::string>&tokens) noexcept {
    for (int i = 0; i < tokens.size(); i++) {
        std::vector<std::string> parts = lib_::split(tokens[i].c_str(), '/');
        std::unique_ptr<dir_entr_ret_t> entry = parsePath(parts, 0x1);

        if (entry == nullptr) {
            BUFFER << (LOG_str(log::WARNING, "Path is not valid, either directory/file's specified are non-existant"));
            return;
        }

        if(!entry->m_entry->is_directory) {
            delete_entry(entry);
        } else {
            std::shared_ptr<dir_t> tmp = read_dir(entry->m_entry->start_cluster_index);
            delete_dir(tmp);
            delete_entry(entry);
        }
        save_dir(entry->m_dir);
    }
}

void fat32::touch(std::vector<std::string>& parts, char* payload, uint64_t size) noexcept {
    std::vector<std::string> tokens = lib_::split(parts[0].c_str(), '/');
    std::unique_ptr<dir_entr_ret_t> entr = parsePath(tokens, 0x0);
    const char* init_file_name = tokens[tokens.size() - 1].c_str();

    if(!entr) {
        BUFFER << (LOG_str(log::WARNING, "Path specified is invalid"));
        return;
    }

    if(size == 0 && parts.size() > 1) {
        std::string tmp = {};
        for(int i = 1; i < parts.size(); i++) {
            tmp += parts[i];
            tmp += " ";
        }
        tmp[tmp.size() - 1] = '\0';
        payload = const_cast<char*>(tmp.c_str());
        size = tmp.size();
    }

    std::shared_ptr<std::byte[]> buffer = std::shared_ptr<std::byte[]>(new std::byte[size]);
    memcpy(buffer.get(), payload, size);
    insert_int_file(entr->m_dir, buffer, init_file_name, size);
}

void fat32::cat(const char* path, int8_t export_) noexcept {
    std::vector<std::string> tokens = lib_::split(path, '/');
    std::unique_ptr<dir_entr_ret_t> entr = parsePath(tokens, 0x1);

    if(!entr) {
        BUFFER << (LOG_str(log::WARNING, "Path specified is invalid"));
        return;
    }
    std::shared_ptr<std::byte[]> buffer;
    size_t size = read_file(entr->m_dir, tokens[tokens.size() - 1].c_str(), buffer);
    std::unique_ptr<char[]> data = std::unique_ptr<char[]>(new char[size]);
    memcpy(data.get(), buffer.get(), size);

    std::string file_name = tokens[tokens.size() - 1];

    if(export_ == 0) {
        BUFFER << "\nFile: " << file_name.c_str() << "\nSize: " << size << "b\n------------\n";
        BUFFER.append(data.get(), size);
        BUFFER << "\n";
    } else {
        BUFFER.append(data.get(), size);
    }
}

void fat32::ls() noexcept {
    print_dir(*m_curr_dir);
}

fat32::dir_entry_t* fat32::find_entry(std::shared_ptr<dir_t>& dir, const char* entry, uint8_t shd_exst) const noexcept {
    dir_entry_t* ret = nullptr;
    for (int i = 0; i < dir.get()->dir_header.dir_entry_amt; i++) {
        if (strcmp(dir.get()->dir_entries[i].dir_entry_name, entry) == 0) {
            ret = &dir->dir_entries[i];
            break;
        }
    }

    if (shd_exst == 1 && ret == nullptr) {
        BUFFER << (LOG_str(log::WARNING, "entry '" + std::string(entry) + "', could not be found"));
    } else if (shd_exst == 0 && ret != nullptr) {
        BUFFER << (LOG_str(log::WARNING, "entry '" + std::string(entry) + "', already exists"));
    }

    return ret;
}

void fat32::print_super_block() const noexcept {
    char buffer[400];
    BUFFER << "\n   Super block\n ---------------\n\n";

    BUFFER << "  meta data\n-------------\n";
    BUFFER << " -> disk:            " << m_superblock.data.disk_name << "\n";
    BUFFER << " -> disk size:       " << convert_size(m_superblock.data.disk_size).c_str() << "\n";
    BUFFER << " -> Superblock size: " << convert_size(m_superblock.data.superblock_size).c_str() << "\n";
    BUFFER << " -> Fat table size:  " << convert_size(m_superblock.data.fat_table_size).c_str() << "\n";
    BUFFER << " -> User space:      " << convert_size(m_superblock.data.user_size).c_str() << "\n";
    BUFFER << " -> Cluster size:    " << convert_size(m_superblock.data.cluster_size).c_str() << "\n";
    BUFFER << " -> Cluster amount:  " << m_superblock.data.cluster_n << "\n";
    BUFFER << " -> Clusters free:   " << m_free_clusters->size() << "\n";

    BUFFER << "\n  Address space\n-----------------\n";


    sprintf(buffer, " -> [superblock : 0x%.8x]\n", m_superblock.superblock_addr);
    sprintf(buffer + strlen(buffer), " -> [fat_table  : 0x%.8x]\n", m_superblock.fat_table_addr);
    sprintf(buffer + strlen(buffer), " -> [user_space : 0x%.8x]\n", m_superblock.root_dir_addr);
    sprintf(buffer + strlen(buffer), "%s\n%s\n", "-----------------", "    End");

    BUFFER << (buffer);
}

void fat32::print_fat_table() const noexcept {
    printf("\n%s%s\n", "    Fat table\n", " --------------");
    for (int i = 0; i < CLUSTER_AMT; i++) {
        printf( "[%d : 0x%.8x]\n", i, m_fat_table[i]);
    }
}

void fat32::print_dir(dir_t & dir) noexcept {
    char buffer[1024 * 8];
    if ((dir_t*)&dir == NULL) {
        BUFFER << (LOG_str(log::WARNING, "specified directory to be printed is null"));
        return;
    }

    sprintf(buffer, "\r\nDirectory:        %s\n", dir.dir_header.dir_name);
    sprintf(buffer + strlen(buffer), "Start cluster:    %d\n", dir.dir_header.start_cluster_index);
    sprintf(buffer + strlen(buffer), "Parent cluster:   %d\n", dir.dir_header.parent_cluster_index);
    sprintf(buffer + strlen(buffer), "Entry amt:        %d\n", dir.dir_header.dir_entry_amt);

    sprintf(buffer + strlen(buffer), "\n %s%4s%s%4s%s\n%s\n", "size", "", "start cluster", "", "name", "-------------------------------");

    for (int i = 0; i < dir.dir_header.dir_entry_amt; i++) {
        sprintf(buffer + strlen(buffer), "%s%8s%02d%10s%s\n", convert_size(dir.dir_entries[i].dir_entry_size).c_str(), "", dir.dir_entries[i].start_cluster_index, "", dir.dir_entries[i].dir_entry_name);
    }
    
    sprintf(buffer + strlen(buffer), "-------------------------------");
    sprintf(buffer + strlen(buffer), "\n");

    BUFFER << (buffer);
}

int8_t fat32::dir_equal(std::shared_ptr<fat32::dir_t>& d1, std::shared_ptr<fat32::dir_t>& d2) noexcept {
    int8_t res = 1;

    if(strcmp(d1->dir_header.dir_name, d2->dir_header.dir_name) != 0)
        res = -1;

    if(d1->dir_header.start_cluster_index != d1->dir_header.start_cluster_index || d1->dir_header.parent_cluster_index != d1->dir_header.parent_cluster_index)
        res = -1;

    return res;
}