#include "FAT32.h"

 IFS::IFS() = default;
 
 FAT32::FAT32(const char *disk_name) {
     if(disk_name[0] == '\0')
         DISK_NAME = DEFAULT_DISK;
     else DISK_NAME = disk_name;

     std::string cmpl = "disks/" + std::string(DISK_NAME);
     PATH_TO_DISK = cmpl.c_str();

     if(STORAGE_SIZE > (1ULL << 32)) {
         LOG(Log::ERROR, "maximum storage can only be 4gb");
     }

     m_disk      = new Disk();
     m_fat_table = (uint32_t *)malloc(sizeof(uint32_t) * CLUSTER_AMT);
     m_root      = (dir_t*)malloc(sizeof(dir_t));
     m_curr_dir  = (dir_t*)malloc(sizeof(dir_t));

    init();
}

FAT32::~FAT32() {
    free(m_fat_table);
    m_disk->rm();
    delete m_disk;
    delete m_root;
    if(m_curr_dir != m_root)
        delete m_curr_dir;
}

void FAT32::init() noexcept {

     if(access(PATH_TO_DISK, F_OK) == -1)
         set_up();
     else load();
 }

 void FAT32::set_up() noexcept {
     define_superblock();
     define_fat_table();
     m_root = init_dir(0, 0, "root");
     m_curr_dir = m_root;

     create_disk();
     store_superblock();
     store_fat_table();
     store_dir(*m_root);
     LOG(Log::INFO, "file system has been initialised.");
 }

 void FAT32::create_disk() noexcept {
     m_disk->open(DISK_NAME, (const char*)"wb");
     m_disk->truncate(STORAGE_SIZE);
     rewind(m_disk->get_file());

     m_disk->close();

     m_disk->open(DISK_NAME, "rb+");
     LOG(Log::INFO, "binary file '" + std::string(DISK_NAME) + "' has been created.");
 }

 void FAT32::define_superblock() noexcept {
     FAT32::metadata_t data{};
     strcpy(data.disk_name, DISK_NAME);
     data.cluster_size = CLUSTER_SIZE;
     data.disk_size    = STORAGE_SIZE;
     data.cluster_n    = CLUSTER_AMT;
     data.user_size    = USER_SPACE;

     m_superblock.data = data;
     m_superblock.superblock_addr = SUPERBLOCK_START_ADDR;
     m_superblock.fat_table_addr  = FAT_TABLE_START_ADDR;
     m_superblock.root_dir_addr   = ROOT_START_ADDR;
 }

 void FAT32::define_fat_table() noexcept {
     memset((void*)m_fat_table, UNALLOCATED_CLUSTER, (size_t)FAT_TABLE_SIZE);
 }

 FAT32::dir_t* FAT32::init_dir(const uint32_t &start_cl, const uint32_t &parent_clu, const char* name) noexcept {
     dir_header_t hdr;
     auto* tmp = (dir_t*)malloc(sizeof(dir_t));
     strcpy(hdr.dir_name, name);
     hdr.start_cluster_index = 0;
     hdr.parent_cluster_index = 0;
     hdr.dir_entry_amt = 2;

     tmp->dir_header = hdr;
     tmp->dir_entries = (dir_entry_t*)malloc(sizeof(dir_entry_t) * tmp->dir_header.dir_entry_amt);

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

 void FAT32::store_superblock() noexcept {
     m_disk->seek(m_superblock.superblock_addr);
     m_disk->write((void*)&m_superblock, sizeof(m_superblock), 1);
     fflush(m_disk->get_file());
 }

 void FAT32::store_fat_table() noexcept {
     m_disk->seek(m_superblock.fat_table_addr);
     m_disk->write(m_fat_table, sizeof(uint32_t), (uint32_t)CLUSTER_AMT);
     fflush(m_disk->get_file());
 }

 void FAT32::store_dir(dir_t& directory)  noexcept {
     // ensuring header data is able to fit within a cluster size
     if(CLUSTER_SIZE < sizeof(directory.dir_header) || CLUSTER_SIZE < sizeof(dir_entry_t))
         LOG(Log::ERROR, "Insufficient memory to store header data/dir entry for directory");

     // //////////////////////////////////////////////////////////////////////////////////////////////
     // initialise variables for workout within cluster size

     uint16_t first_clu_entry_amt = (CLUSTER_SIZE - sizeof(directory.dir_header)) / sizeof(dir_entry_t);
     uint16_t remain_entries  = (first_clu_entry_amt >= directory.dir_header.dir_entry_amt) ? 0 : (directory.dir_header.dir_entry_amt - (uint32_t)first_clu_entry_amt);
     uint16_t amt_of_entry_per_clu = 0;
     uint16_t entries_written = 0;

     // //////////////////////////////////////////////////////////////////////////////////////////////
     // get first cluster and store header data within
     if(!n_free_clusters(1)) {
         LOG(Log::ERROR, "amount of clusters needed is not valid");
         return;
     }

     uint32_t first_clu_index = attain_clu();

     directory.dir_header.start_cluster_index = first_clu_index;

     if(remain_entries > 0)
         amt_of_entry_per_clu = CLUSTER_SIZE / sizeof(dir_entry_t);

     m_disk->seek(m_superblock.root_dir_addr + (first_clu_index * CLUSTER_SIZE));
     m_disk->write((void*)&directory.dir_header, sizeof(dir_header_t), 1);
     fflush(m_disk->get_file());

     uint16_t num_of_clu_needed;
     if(remain_entries < amt_of_entry_per_clu)
         num_of_clu_needed = 1;
     else num_of_clu_needed = (uint16_t)((double)remain_entries / (double)amt_of_entry_per_clu);

     // //////////////////////////////////////////////////////////////////////////////////////////////
     // write entries in first cluster index
     for(int i = 0; i < min(directory.dir_header.dir_entry_amt, first_clu_entry_amt); i++) {
         size_t addr_offset = (ROOT_START_ADDR + (first_clu_index * CLUSTER_SIZE) + sizeof(dir_header_t));

         m_disk->seek(addr_offset + (i * sizeof(dir_entry_t)));
         m_disk->write((void*)&directory.dir_entries[i], sizeof(dir_entry_t), 1);
         fflush(m_disk->get_file());
         entries_written += 1;
     }
     // /////////////////////////////////////////////////////////////////////////////////////////////
     // add remaining entries within available clusters, up until last cluster

     if(remain_entries == 0) {
         m_fat_table[first_clu_index] = EOF_CLUSTER;
         store_fat_table();
         return;
     }

     if(remain_entries > amt_of_entry_per_clu) {
         if(remain_entries % amt_of_entry_per_clu > 0)
             num_of_clu_needed++;
     }

     if(!n_free_clusters(num_of_clu_needed)) {
         LOG(Log::WARNING, "remaining entries cannot be stored due to insufficient cluster amount");
         LOG(Log::WARNING, "'" + std::string(directory.dir_header.dir_name) + "' directory cannot be stored within: '" + std::string(DISK_NAME) + "'");
         m_fat_table[first_clu_index] = UNALLOCATED_CLUSTER;
         return;
     }

     uint32_t* clu_list = (uint32_t*)malloc(sizeof(uint32_t) * num_of_clu_needed);
     memset(clu_list, 0, num_of_clu_needed);

     for(int i = 0; i < num_of_clu_needed; i++)
         clu_list[i] = attain_clu();

     for(int i = 0; i < num_of_clu_needed - 1; i++) {
         size_t addr_offset = (ROOT_START_ADDR + (clu_list[i] * CLUSTER_SIZE));

         m_disk->seek(addr_offset);
         m_disk->write((void*)&directory.dir_entries[entries_written], sizeof(dir_entry_t), amt_of_entry_per_clu);
         fflush(m_disk->get_file());
         remain_entries -= amt_of_entry_per_clu;
         entries_written += amt_of_entry_per_clu;
     }


     // //////////////////////////////////////////////////////////////////////////////////////////////
     // write remaining entries towards last cluster

     m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[num_of_clu_needed - 1]));
     m_disk->write((void*)&directory.dir_entries[entries_written], sizeof(dir_entry_t), remain_entries);
     fflush(m_disk->get_file());
     // /////////////////////////////////////////////////////////////////////////////////////////////
     // fill in fat_table

     m_fat_table[first_clu_index] = clu_list[0];
     for(int i = 0; i < num_of_clu_needed; i++)
         m_fat_table[clu_list[i]] = clu_list[i + 1];
     m_fat_table[clu_list[num_of_clu_needed - 1]] = EOF_CLUSTER;
     // /////////////////////////////////////////////////////////////////////////////////////////////
     // free heap allocated memory and store unsaved structures onto disk
     free(clu_list);
     store_fat_table();
 }

 void FAT32::load() noexcept {
     m_disk->open(DISK_NAME, "rb+");
     m_superblock = load_superblock();
     m_fat_table  = load_fat_table();
     m_root       = read_dir(0);
 }

 FAT32::superblock_t FAT32::load_superblock() noexcept {
     superblock_t ret;
     m_disk->seek(SUPERBLOCK_START_ADDR);
     m_disk->read((void*)&ret, sizeof(superblock_t), 1);
     fflush(m_disk->get_file());

     return ret;
 }

 uint32_t* FAT32::load_fat_table() noexcept {
     uint32_t* ret = (uint32_t*)malloc(sizeof(uint32_t) * CLUSTER_AMT);
     m_disk->seek(FAT_TABLE_START_ADDR);
     m_disk->read((void*)&ret, sizeof(FAT_TABLE_SIZE), 1);
     fflush(m_disk->get_file());

     return ret;
 }

 void FAT32::insert_dir(dir_t& curr_dir, const char *dir_name) noexcept {
     dir_t* tmp;

     tmp = init_dir(UNDEF_START_CLUSTER, curr_dir.dir_header.start_cluster_index, dir_name);
     store_dir(*tmp);

     add_new_entry(curr_dir, dir_name, tmp->dir_header.start_cluster_index, sizeof(tmp), 1);

     store_dir(curr_dir);
     delete tmp;
 }

 FAT32::dir_t* FAT32::read_dir(const uint32_t& start_clu) noexcept {
     if(m_fat_table[start_clu] == UNALLOCATED_CLUSTER) {
         LOG(Log::WARNING, "specified cluster has not been allocated");
         return nullptr;
     }

     //tmp dir_t*, return
     dir_t* ret = (dir_t*)malloc(sizeof(dir_t));

     uint32_t dir_start_addr = ROOT_START_ADDR + (CLUSTER_SIZE * start_clu);

     //attain dir_header
     m_disk->seek(dir_start_addr);
     m_disk->read((void*)&ret->dir_header, sizeof(dir_header_t), 1);
     fflush(m_disk->get_file());

     uint16_t first_clu_entry_amt = (CLUSTER_SIZE - sizeof(ret->dir_header)) / sizeof(dir_entry_t);
     uint16_t remain_entries  = (first_clu_entry_amt >= ret->dir_header.dir_entry_amt) ? 0 : (ret->dir_header.dir_entry_amt - (uint32_t)first_clu_entry_amt);
     uint16_t entries_read = 0;

     //allocate memory to ret(dir_t) entries due to dir header data.
     ret->dir_entries = (dir_entry_t*)malloc(sizeof(dir_entry_t) * ret->dir_header.dir_entry_amt);

     for(int i = 0; i < min(first_clu_entry_amt, ret->dir_header.dir_entry_amt); i++) {
         size_t addr_offset = dir_start_addr + sizeof(dir_header_t);

         m_disk->seek(addr_offset + (i * sizeof(dir_entry_t)));
         m_disk->read((void*)&ret->dir_entries[i], sizeof(dir_entry_t), 1);
         fflush(m_disk->get_file());
         entries_read += 1;
     }


     uint32_t amt_of_entries_per_clu;
     if(remain_entries > 0) {
         amt_of_entries_per_clu = CLUSTER_SIZE / sizeof(dir_entry_t);
     } else return ret;

     uint32_t amt_of_clu_used = remain_entries / amt_of_entries_per_clu;

     if(remain_entries > amt_of_entries_per_clu) {
         if(remain_entries % amt_of_entries_per_clu > 0)
             amt_of_clu_used++;
     }

     //move towards next clu
     uint32_t curr_clu = start_clu;
     curr_clu = m_fat_table[curr_clu];

     for(int i = 0; i < amt_of_clu_used - 1; i++) {
         size_t addr_offset = ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu);

         m_disk->seek(addr_offset);
         m_disk->read((void*)&ret->dir_entries[entries_read], sizeof(dir_entry_t), amt_of_entries_per_clu);
         fflush(m_disk->get_file());
         entries_read += amt_of_entries_per_clu;
         curr_clu = m_fat_table[curr_clu];
     }

     uint32_t remain_entries_in_lst_clu = abs(ret->dir_header.dir_entry_amt - entries_read);

     m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu));
     m_disk->read((void*)&ret->dir_entries[entries_read], sizeof(dir_entry_t), remain_entries_in_lst_clu);
     fflush(m_disk->get_file());

     return ret;
 }

std::string FAT32::read_file(dir_t& dir, const char* entry_name) noexcept {
     dir_entry_t* entry_ptr = find_entry(dir, entry_name);

     if(m_fat_table[entry_ptr->start_cluster_index] == UNALLOCATED_CLUSTER) {
         LOG(Log::WARNING, "cluster specified has not been allocated, file could not be read");
         return "";
     }

     uint32_t amt_of_clu_used = 0;
     size_t entry_size = entry_ptr->dir_entry_size;
     char buffer[entry_size];
     uint32_t data_read = 0;

     if(entry_size <= CLUSTER_SIZE)
        amt_of_clu_used = 1;
    else amt_of_clu_used = entry_size / CLUSTER_SIZE;

    if(entry_size % CLUSTER_SIZE > 0)
        amt_of_clu_used += 1;

    if(amt_of_clu_used == 1) {
        m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * entry_ptr->start_cluster_index));
        m_disk->read((void*)&buffer, sizeof(buffer), 1);
        fflush(m_disk->get_file());

        data_read += entry_size;

        return buffer;
    }

    uint32_t curr_clu = entry_ptr->start_cluster_index;

    for(int i = 0; i < amt_of_clu_used - 1; i++) {
        size_t addr_offset = ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu);

        m_disk->seek(addr_offset);
        m_disk->read((void*)&buffer[data_read], CLUSTER_SIZE, 1);
        fflush(m_disk->get_file());
        curr_clu = m_fat_table[curr_clu];
        data_read += CLUSTER_SIZE;
    }

    uint32_t remaining_data = abs(entry_size - data_read);

    m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu));
    m_disk->read((void*)&buffer[data_read], remaining_data, 1);
    fflush(m_disk->get_file());

    return std::string(buffer);
 }

 std::string FAT32::read_file(dir_entry_t &entry) noexcept {
     if(m_fat_table[entry.start_cluster_index] == UNALLOCATED_CLUSTER) {
         LOG(Log::WARNING, "cluster specified has not been allocated, file could not be read");
         return {""};
     }

     uint32_t amt_of_clu_used = 0;
     size_t entry_size = entry.dir_entry_size;
     char buffer[entry_size];
     uint32_t data_read = 0;

     if(entry_size <= CLUSTER_SIZE)
         amt_of_clu_used = 1;
     else amt_of_clu_used = entry_size / CLUSTER_SIZE;

     if(entry_size % CLUSTER_SIZE > 0)
         amt_of_clu_used += 1;

     if(amt_of_clu_used == 1) {
         m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * entry.start_cluster_index));
         m_disk->read((void*)&buffer, sizeof(buffer), 1);
         fflush(m_disk->get_file());

         data_read += entry_size;

         return buffer;
     }

     uint32_t curr_clu = entry.start_cluster_index;

     for(int i = 0; i < amt_of_clu_used - 1; i++) {
         size_t addr_offset = ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu);

         m_disk->seek(addr_offset);
         m_disk->read((void*)&buffer[data_read], CLUSTER_SIZE, 1);
         fflush(m_disk->get_file());
         curr_clu = m_fat_table[curr_clu];
         data_read += CLUSTER_SIZE;
     }

     uint32_t remaining_data = abs(entry_size - data_read);

     m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * curr_clu));
     m_disk->read((void*)&buffer[data_read], remaining_data, 1);
     fflush(m_disk->get_file());

     return {buffer};
 }

FAT32::file_ret FAT32::store_file(const char *path) noexcept {
     if(access(path, F_OK) == -1) {
         LOG(Log::WARNING, "file specified does not exist");
     }

     char buffer[CLUSTER_SIZE];
     uint32_t first_cluster = 0;
     FILE* file = get_file_handlr(path);
     off_t fsize = lseek(fileno(file), 0, SEEK_END);
     size_t data_read = 0;

     uint32_t amt_of_clu_needed = 0;

     if(fsize < CLUSTER_SIZE)
         amt_of_clu_needed = 1;
     else amt_of_clu_needed = fsize / CLUSTER_SIZE;

     if(amt_of_clu_needed == 1) {
         first_cluster = attain_clu();
         char buffer[fsize];

         fseek(file, 0, SEEK_SET);
         fread((void*)&buffer, sizeof(buffer), 1, file);
         data_read += fsize;
         fflush(file);

         m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * first_cluster));
         m_disk->write((void*)&buffer, sizeof(buffer), 1);
         fflush(m_disk->get_file());

         fclose(file);
         return file_ret(first_cluster, file);
     }

     if(fsize % CLUSTER_SIZE > 0)
         amt_of_clu_needed += 1;

     if(!n_free_clusters(amt_of_clu_needed)) {
         LOG(Log::WARNING, "amount of cluster needed isn't available to store file");
         fclose(file);
         return file_ret(-1, nullptr);
     }

     uint32_t* clu_list = (uint32_t*)malloc(sizeof(uint32_t) * amt_of_clu_needed);
     memset(clu_list, 0, amt_of_clu_needed);

     for(int i = 0; i < amt_of_clu_needed; i++)
         clu_list[i] = attain_clu();

     first_cluster = clu_list[0];

     for(int i = 0; i < amt_of_clu_needed - 1; i++) {
         size_t off_adr = ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[i]);

         fseek(file, data_read, SEEK_SET);
         fread((void*)&buffer, sizeof(buffer), 1, file);
         fflush(file);

         m_disk->seek(off_adr);
         m_disk->write((void*)&buffer, CLUSTER_SIZE, 1);
         fflush(m_disk->get_file());

         data_read += CLUSTER_SIZE;
     }

     size_t remaining_data = abs(fsize - data_read);

     fseek(file, data_read, SEEK_SET);
     fread((void*)&buffer, remaining_data, 1, file);
     fflush(file);

     m_disk->seek(ROOT_START_ADDR + (CLUSTER_SIZE * clu_list[amt_of_clu_needed - 1]));
     m_disk->write((void*)&buffer, remaining_data, 1);
     fflush(m_disk->get_file());

     m_fat_table[first_cluster] = clu_list[0];
     for(int i = 0; i < amt_of_clu_needed; i++)
         m_fat_table[clu_list[i]] = clu_list[i + 1];
     m_fat_table[clu_list[amt_of_clu_needed - 1]] = EOF_CLUSTER;

     free(clu_list);
     return file_ret(first_cluster, file);
 }

 void FAT32::insert_file(dir_t& curr_dir, const char* path, const char* name) noexcept {
     FAT32::file_ret file = store_file(path);

     if(file.start_cluster == -1) {
         LOG(Log::WARNING, "file could not be stored");
         return;
     }

    add_new_entry(curr_dir, name, file.start_cluster, lseek(fileno(file.fd), 0, SEEK_END), 1);
    store_dir(curr_dir);
    fclose(file.fd);
 }

 FILE* FAT32::get_file_handlr(const char* file) noexcept {
     FILE* handlr = fopen(file, "rb+");
     return handlr;
 }

 uint32_t FAT32::attain_clu() const noexcept {
     uint32_t rs = 0;
     for(int i = 0; i < CLUSTER_AMT; i++) {
         if(m_fat_table[i] == UNALLOCATED_CLUSTER) {
             rs = i;
             m_fat_table[rs] = ALLOCATED_CLUSTER;
             break;
         }
     }
     return rs;
 }

 uint32_t FAT32::n_free_clusters(const uint32_t& req) const noexcept {
     uint32_t amt = 0;
     for(int i = 0; i < CLUSTER_AMT; i++) {
         if(m_fat_table[i] == UNALLOCATED_CLUSTER)
            amt++;
         if(amt >= req)
             break;
     }
     return amt == req ? 1 : 0;
 }

 std::vector<uint32_t> FAT32::get_list_of_clu(const uint32_t &start_clu) noexcept {
     std::vector<uint32_t> alloc_clu;

     uint32_t curr_clu = start_clu;
     alloc_clu.push_back(curr_clu);

     while(1) {
         uint32_t next_clu = m_fat_table[curr_clu];
         if(next_clu == EOF_CLUSTER)
             break;
         alloc_clu.push_back(next_clu);
         curr_clu = next_clu;
     }
     return alloc_clu;
 }

 void FAT32::add_new_entry(dir_t& curr_dir, const char* name, const uint32_t& start_clu, const uint32_t& size, const uint8_t& is_dir) noexcept {

     curr_dir.dir_header.dir_entry_amt += 1;
     dir_entry_t* tmp_entries = curr_dir.dir_entries;
     curr_dir.dir_entries = (dir_entry_t*)malloc(sizeof(dir_entry_t) * curr_dir.dir_header.dir_entry_amt);

     for(int i = 0; i < curr_dir.dir_header.dir_entry_amt - 1; i++) {
         curr_dir.dir_entries[i] = tmp_entries[i];
     }
     strcpy(curr_dir.dir_entries[curr_dir.dir_header.dir_entry_amt-1].dir_entry_name, name);
     curr_dir.dir_entries[curr_dir.dir_header.dir_entry_amt-1].start_cluster_index = start_clu;
     curr_dir.dir_entries[curr_dir.dir_header.dir_entry_amt-1].is_directory = is_dir;
     curr_dir.dir_entries[curr_dir.dir_header.dir_entry_amt-1].dir_entry_size = size;

     std::vector<uint32_t> alloc_clu = get_list_of_clu(curr_dir.dir_header.start_cluster_index);

     for(int i = 0; i < alloc_clu.size(); i++) {
         m_fat_table[alloc_clu[i]] = UNALLOCATED_CLUSTER;
     }

     delete tmp_entries;
 }

 void FAT32::cp(const char* src, const char* dst) noexcept {

}

void FAT32::mkdir(const char *dir) noexcept {
    dir_entry_t* tmp = find_entry(*m_curr_dir, dir);

    if(tmp != nullptr) {
        LOG(Log::WARNING, "entry already exists with name specified");
        return;
    }

     insert_dir(*m_curr_dir, dir);
}

void FAT32::cd(const char* pth) noexcept {
    dir_entry_t* dir = find_entry(*m_curr_dir, pth);

    if(!dir->is_directory) {
        LOG(Log::WARNING, "entry specified is not a directory");
        return;
    }

    if(dir == nullptr) {
        LOG(Log::WARNING, "entry does not exist");
        return;
    }

    if(m_curr_dir != m_root)
        delete m_curr_dir;

    m_curr_dir = read_dir(dir->start_cluster_index);
}

void FAT32::rm(const char *file) noexcept {

}

void FAT32::rm(const char *file, const char *args, ...) noexcept {

}

void FAT32::ls() noexcept{
     printf("\n");
     print_dir(*m_curr_dir);
 }

FAT32::dir_entry_t* FAT32::find_entry(dir_t& dir, const char* entry) const noexcept {
    for(int i = 0; i < dir.dir_header.dir_entry_amt; i++)
        if(strcmp(dir.dir_entries[i].dir_entry_name, entry) == 0)
            return &dir.dir_entries[i];

    LOG(Log::WARNING, "'" + std::string(entry) + "' could not be found");
    return nullptr;
}

void FAT32::print_fat_table() const noexcept {
     printf("\n%s%s\n", "    Fat table\n", " --------------");
     for(int i = 0; i < CLUSTER_AMT; i++) {
         printf("[%d : 0x%.8x]\n", i, m_fat_table[i]);
     }
 }

 void FAT32::print_dir(dir_t &dir) const noexcept {
     if((dir_t*)&dir == NULL) {
         LOG(Log::WARNING, "specified directory to be printed is null");
         return;
     }

     printf("Directory:        %s\n", dir.dir_header.dir_name);
     printf("Start cluster:    %d\n", dir.dir_header.start_cluster_index);
     printf("Parent cluster:   %d\n", dir.dir_header.parent_cluster_index);
     printf("Entry amt:        %d\n", dir.dir_header.dir_entry_amt);

     printf(" %s%4s%s%4s%s\n%s\n", "size", "", "start cluster", "", "name", "-------------------------------");

     for(int i = 0; i < dir.dir_header.dir_entry_amt; i++) {
         printf("%db%10s%d%12s%s\n", dir.dir_entries[i].dir_entry_size, "", dir.dir_entries[i].start_cluster_index, "", dir.dir_entries[i].dir_entry_name);
     }
     printf("-------------------------------\n");
 }
