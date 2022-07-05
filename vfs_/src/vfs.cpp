#include "../include/vfs.h"

using namespace VFS;

vfs* vfs::mp_vfs;

vfs::vfs() {
    sys_cmds     = std::make_shared<std::vector<vfs::cmd_t>>();
    disks        = std::make_unique<std::unordered_map<std::string, system_t>>();
    mnted_system = std::shared_ptr<system_t>(new system_t("", nullptr, "", NULL, {}));

    init_sys_cmds();
    BUFFER << LOG_str(log::INFO, "vfs: defined");
}

vfs* vfs::get_vfs() noexcept {
    if(!mp_vfs)
        mp_vfs = new vfs();

    return mp_vfs;
}

void vfs::umnt_disk(std::vector<std::string> &parts) {
    if(mnted_system->mp_fs != nullptr) {
        mnted_system->mp_fs.reset();
        mnted_system->mp_fs = nullptr;
        mnted_system->fs_type = "";
        mnted_system->name    = "";
    } else BUFFER << LOG_str(log::WARNING, "There is no system currently mounted");
}

void vfs::mnt_disk(std::vector<std::string>& parts) {
    if(disks->find(parts[1]) == disks->end()) {
        BUFFER << LOG_str(log::WARNING, "disk does not exist within the vfs");

        return;
    }

    if(mnted_system->mp_fs != nullptr) {
        BUFFER << LOG_str(log::WARNING, "Unmount the current system before mounting another");
        return;
    }

    BUFFER << "\r\n--------------------  " << parts[1].c_str() << "  --------------------\n";
    BUFFER << LOG_str(log::INFO, "Mounting '" + parts[1] + "' as primary mp_fs on the vfs");
    disks->find(parts[1])->second.mp_fs = typetofs(parts[1].c_str(), disks->find(parts[1])->second.fs_type);

    this->mnted_system->name    = parts[1].c_str();
    this->mnted_system->fs_type = disks->find(parts[1])->second.fs_type;
    this->mnted_system->mp_fs      = *&(disks->find(parts[1])->second.mp_fs);

    if(dynamic_cast<RFS::rfs*>(this->mnted_system->mp_fs.get()))
        this->mnted_system->access = &vfs::rfs_cmd_func;
    else this->mnted_system->access  = &vfs::ifs_cmd_func;
}

void vfs::add_disk(std::vector<std::string>& parts) {
    if(disks->find(parts[2]) != disks->end()) {
        BUFFER << LOG_str(log::WARNING, " There is an existing disk with that name already");
        return;
    }

    if(parts.size() == 4) { // added a fourth parameter to specify file system type
        if(fs_types.find(parts[3]) == fs_types.end()) {
            BUFFER << LOG_str(log::WARNING, "File system type does not exist");
            return;
        } else (*disks).insert(std::make_pair(parts[2], system_t{parts[2].c_str(), nullptr, parts[3].c_str(), nullptr, {}})); // default mp_fs
    } else (*disks).insert(std::make_pair(parts[2], system_t{parts[2].c_str(), nullptr, DEFAULT_FS, nullptr, {}})); // specified mp_fs
}

void vfs::rm_disk(std::vector<std::string>& parts) {
    fs* tmp = (disks->find(parts[2])->second.mp_fs.get());

    if(tmp) {
        if(tmp == mnted_system->mp_fs.get()){
            umnt_disk(parts); // erases mounted system information, if disk deleted is mounted.
        }
    }
          
    disks->erase(parts[2]); // deletes file system, on heap.
}

void vfs::add_remote(std::vector<std::string>& parts) {

    if(disks->find(parts[2]) != disks->end()) {
        BUFFER << LOG_str(log::WARNING, "There is an existing remote connection with that name already");
        return;
    }

    (*disks).insert(std::make_pair(parts[2], system_t{parts[2].c_str(), nullptr, "RFS::rfs", nullptr, system_t::sock_conn_t{parts[3].c_str(), atoi(parts[4].c_str())}})); // add RFS::rfs system with ip and address
}

void vfs::rm_remote(std::vector<std::string>& parts) {
    if(mnted_system->mp_fs)
        if(disks->find(parts[2])->second.mp_fs == mnted_system->mp_fs)
            umnt_disk(parts); // erases mounted system information, if RFS::rfs deleted is mounted.

    disks->erase(parts[2]); // deletes file system, on heap.
}

void vfs::lst_disks(std::vector<std::string>& parts) {
    BUFFER << "\r-----------------  vfs  ---------------\n";

    if(disks->empty()) {
        BUFFER << " -> there is no systems added\n";
        goto no_disks;
    }

    for(auto& disk : *disks) {
        if(strcmp(disk.second.fs_type, "RFS::rfs") == 0) {
            BUFFER << " -> (name)" << disk.first.c_str() << " : (address)" << disk.second.conn.addr << ", (port)" << disk.second.conn.port;
        } else BUFFER << " -> (name)" << disk.first.c_str() << " : (filesystem)" << disk.second.fs_type;

        if(strcmp(mnted_system->name, disk.first.c_str()) == 0) {
            BUFFER << " ~ [ Mounted ]";
        }

        BUFFER << "\n";
    }
    
    no_disks:
    BUFFER << "----------------  END  ----------------\n\n";
}

void vfs::init_server(std::vector<std::string>& args) {
    if(server == nullptr) {
        server = std::make_unique<RFS::server>();
        return;
    }

    server.reset();
    server = nullptr;
}

void vfs::init_sys_cmds() noexcept {
    sys_cmds->push_back({system_cmd::vfs_,
                         {flag_t{"ls", &vfs::lst_disks, "lists the current mounted systems                          | -> [/vfs ls]"},
                          flag_t{"ifs", &vfs::control_ifs, "controls internal file systems within the vfs             | -> [/vfs ifs add/rm <DISK_NAME> <FS_TYPE>]"},
                          flag_t{"rfs", &vfs::control_rfs, "controls remote file systems within the vfs               | -> [/vfs rfs add/rm <NAME> <IP> <PORT>]"},
                          flag_t{"mnt", &vfs::mnt_disk, "initialises the file system and mounts it towards the vfs | -> [/vfs mnt <DISK_NAME>]"},
                          flag_t{"umnt", &vfs::umnt_disk, "deletes file system data/disk from vfs                   | -> [/vfs umnt"},
                          flag_t{"server", &vfs::init_server, "toggles server initialisation for client connection on local host on specified port"}},
                         "allows the user to access control of the virtual file system"});


    sys_cmds->push_back({system_cmd::ls,     {}, "display the entries within the current working directory"});
    sys_cmds->push_back({system_cmd::mkdir,  {}, "create directory within current directory"});
    sys_cmds->push_back({system_cmd::cd,     {}, "changes the current directory within the file system"});
    sys_cmds->push_back({system_cmd::rm,     {}, "removes an entry within the file system"});
    sys_cmds->push_back({system_cmd::touch,  {}, "creates an entry within the file system"});
    sys_cmds->push_back({system_cmd::mv,     {}, "moves an entry towards a different directory"});
    sys_cmds->push_back({system_cmd::cp,     {}, "copies an entry within the specified directory"});
    sys_cmds->push_back({system_cmd::cat,    {}, "print bytes found at entry"});
    sys_cmds->push_back({system_cmd::help,   {}, "prints help page"});
    sys_cmds->push_back({system_cmd::clear,  {}, "clears bytes from screen"});
}

void vfs::control_vfs(const std::vector<std::string>& parts) noexcept { // checks vfs flags, if none print help. if not, carry out function.
    for(auto & sys_cmd : *sys_cmds) {
        if(vfs::system_cmd::vfs_ == sys_cmd.cmd) {
            for(int j = 0; j < sys_cmd.flags.size(); j++) {
                if(parts[0] == sys_cmd.flags.at(j).name) {
                    std::vector<std::string> tmp(parts);
                    (*this.*sys_cmd.flags.at(j).func)(tmp);
                    return;
                }
            }
        }
    }
    vfs_help();
}


void vfs::ifs_cmd_func(vfs::system_cmd cmd, std::vector<std::string>& args, const char* buffer, uint64_t size, int8_t options) noexcept {
    switch(cmd) {
        case vfs::system_cmd::mkdir: (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->mkdir(args[0].c_str()));           break;
        case vfs::system_cmd::cd:    (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->cd(args[0].c_str()));              break;
        case vfs::system_cmd::ls:    (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->ls());                             break;
        case vfs::system_cmd::rm:    (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->rm(args));                         break;
        case vfs::system_cmd::touch: (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->touch(args, (char*)buffer, size)); break;
        case vfs::system_cmd::mv:    (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->mv(args));                         break;
        case vfs::system_cmd::cat:   (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->cat(args[0].c_str(), options));             break;

        case vfs::system_cmd::cp:    if(args[0] == "imp")
                                        (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->cp_imp(args[1].c_str(), args[2].c_str()));
                                    else if(args[0] == "exp")
                                        (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->cp_exp(args[1].c_str(), args[2].c_str()));
                                    else (dynamic_cast<IFS::ifs*>(vfs::get_vfs()->get_mnted_system()->mp_fs.get())->cp(args[0].c_str(), args[1].c_str())); break;
        default: break;
    }
}

void vfs::rfs_cmd_func(vfs::system_cmd cmd, std::vector<std::string>& args, const char* buffer_, uint64_t size, int8_t options) noexcept {
    ((RFS::client*)(RFS::rfs*)vfs::get_vfs()->get_mnted_system()->mp_fs.get())->handle_send(syscmd_str[(uint8_t)cmd], (uint8_t)cmd, args);
}

void vfs::control_ifs(std::vector<std::string>& parts) noexcept {
    switch(lib_::hash(parts[1].c_str())) {
        case lib_::hash("add"): add_disk(parts); return;
        case lib_::hash("rm"):  rm_disk(parts);  return;
    }
}

void vfs::control_rfs(std::vector<std::string>& parts) noexcept {
    switch(lib_::hash(parts[1].c_str())) {
        case lib_::hash("add"): add_remote(parts); return;
        case lib_::hash("rm"):  rm_remote(parts);  return;
    }
}

void vfs::vfs_help() const noexcept {
    BUFFER << "------  vfs HELP  ------\n";
    for(auto & flag : sys_cmds->at(0).flags) {
        BUFFER << " -> " << flag.name << " - " << flag.desc << "\n";
    }
    BUFFER << "------  END  ------\n";
}

void vfs::load_disks() noexcept {
   std::string path = "disks/";
   std::vector<std::string> func = {"ifs", "add", ""};

   struct dirent *entry;
   DIR *dir = opendir(path.c_str());

   while((entry = readdir(dir)) != nullptr) {
       if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
           continue;
       func[2] = entry->d_name;
       vfs::get_vfs()->control_vfs(func);
   }

	closedir(dir);
}

std::shared_ptr<fs> vfs::typetofs(const char* name, const char *fs_type) noexcept {
    switch(lib_::hash(fs_type)) {
        case lib_::hash("fat32"): return std::make_shared<IFS::fat32>(name);
        case lib_::hash("rfs"): auto rm = disks->find(name); return std::make_shared<RFS::client>(rm->second.conn.addr, rm->second.conn.port);
    }
    return std::make_shared<IFS::fat32>(name);
}

const bool vfs::is_mnted() const noexcept {
    return this->mnted_system->mp_fs != nullptr;
}

std::shared_ptr<vfs::system_t>& vfs::get_mnted_system() noexcept {
    return mnted_system;
}

std::shared_ptr<std::vector<vfs::cmd_t>> vfs::get_sys_cmds() noexcept {
    return sys_cmds;
}
