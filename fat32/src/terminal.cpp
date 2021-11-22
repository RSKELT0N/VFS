#include "../include/terminal.h"

VFS* terminal::m_vfs;
terminal::cmd_environment terminal::m_env;

terminal::command_t valid_vfs(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_ls(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_mkdir(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_cd(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_help(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_clear(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_rm(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_touch(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_mv(std::vector<std::string>& parts) noexcept;
terminal::command_t valid_cp(std::vector<std::string>& parts) noexcept;


constexpr unsigned int hash(const char *s, int off = 0) {
    return !s[off] ? 5381 : (hash(s, off+1)*33) ^ s[off];
}

terminal::terminal() {
    m_vfs = new VFS();
    m_mnted_system = &(*m_vfs).get_mnted_system();
    m_cmds = new std::unordered_map<std::string, input_t>();
    m_env = terminal::EXTERNAL;
    path = "/";

    init_cmds();
}

terminal::~terminal() {
    delete m_cmds;
    m_vfs->~VFS();
    std::cout << "Deleted terminal\n";
}

void terminal::run() noexcept {
    input();
}

void terminal::input() noexcept {
    std::string line;
    printf("enter /help for cmd list\n-------------------"
           "--\n");

    while(1) {
        if(m_env == terminal::EXTERNAL)
            printf("%s", "-> ");
        else printf("%s> ", path.c_str());

        std::getline(std::cin, line);

        if(line.empty())
            continue;

        if(line == "/fat")
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->print_fat_table();

        if(line == "exit")
            return;

        std::vector<std::string> parts = split(line.c_str(), SEPARATOR);
        terminal::command_t command = validate_cmd(parts);
        terminal::cmd_environment cmd_env = cmdToEnv(command);

        if(cmd_env != m_env && cmd_env != terminal::HYBRID) {
            LOG(Log::WARNING, "Command is used within the wrong context");
            continue;
        } else determine_cmd(command, parts);
    }
}

void terminal::determine_cmd(command_t cmd, std::vector<std::string>& parts) noexcept {
    switch(cmd) {
        case terminal::help: {
            print_help();
            break;
        }
        case terminal::vfs: {
            if(parts.size() < 2) {
                m_vfs->vfs_help();
                break;
            }
            determine_flag(cmd, parts);
            break;
        }
        case terminal::clear: {
            clear_scr();
            break;
        }
        case terminal::mkdir: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->mkdir(parts[1].c_str());
            break;
        }
        case terminal::cd: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->cd(parts[1].c_str());
            break;
        }
        case terminal::ls: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->ls();
            break;
        }
        case terminal::rm: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->rm(parts);
            break;
        }
        case terminal::touch: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->touch(parts);
            break;
        }
        case terminal::mv: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->mv(parts);
            break;
        }
        case terminal::cat: {
            ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->cat(parts[1].c_str());
            break;
        }
        case terminal::cp: {
            if(parts[1] == "ext")
                ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->cp_ext(parts[2].c_str(), parts[3].c_str());
            else ((FAT32*)((VFS::system_t*)*m_mnted_system)->fs)->cp(parts[1].c_str(), parts[2].c_str());
            break;
        }


        case terminal::invalid: printf("command is not found\n"); break;
        default: break;
    }
}

terminal::cmd_environment terminal::cmdToEnv(command_t cmd) noexcept {
    switch(cmd) {
        case terminal::vfs:   return terminal::HYBRID;
        case terminal::help:  return terminal::EXTERNAL;
        case terminal::clear: return terminal::HYBRID;
        case terminal::mkdir: return terminal::INTERNAL;
        case terminal::ls:    return terminal::INTERNAL;
        case terminal::touch: return terminal::INTERNAL;
        case terminal::cd:    return terminal::INTERNAL;
        case terminal::rm:    return terminal::INTERNAL;
        case terminal::cp:    return terminal::INTERNAL;
        case terminal::mv:    return terminal::INTERNAL;
        default:              return m_env;
    }
}

void terminal::init_cmds() noexcept {
    (*m_cmds)["/help"]  = input_t{terminal::help,  "lists commands to enter", {}, &valid_help};
    (*m_cmds)["/vfs"]   = input_t{terminal::vfs,   "allows the user to access control of the virtual file system",{flag_t("add", wrap_add_disk), flag_t("rm", wrap_rm_disk), flag_t("mnt", wrap_mnt_disk), flag_t("ls", wrap_ls_disk), flag_t{"umnt", wrap_umnt_disk}},&valid_vfs};
    (*m_cmds)["/clear"] = input_t{terminal::clear, "clears screen", {}, &valid_clear};
    (*m_cmds)["ls"]     = input_t{terminal::ls,    "display the entries within the current working directory", {}, &valid_ls};
    (*m_cmds)["mkdir"]  = input_t{terminal::mkdir, "create directory within current directory", {}, &valid_mkdir};
    (*m_cmds)["cd"]     = input_t{terminal::cd,    "changes the current directory within the file system", {}, &valid_cd};
    (*m_cmds)["rm"]     = input_t{terminal::rm,    "removes an entry within the file system", {}, &valid_rm};
    (*m_cmds)["touch"]  = input_t{terminal::touch, "creates an entry within the file system", {}, &valid_touch};
    (*m_cmds)["mv"]     = input_t{terminal::mv,    "moves an entry towards a different directory", {}, &valid_mv};
    (*m_cmds)["cp"]     = input_t{terminal::cp,    "copies an entry within the specified directory", {}, &valid_cp};
    (*m_cmds)["cat"]    = input_t{terminal::cat,   "print bytes found at entry", {}, &valid_cp};
}

terminal::command_t terminal::validate_cmd(std::vector<std::string> &parts) noexcept {
    if(m_cmds->find(parts[0]) == m_cmds->end())
        return terminal::invalid;


    return m_cmds->find(parts[0])->second.valid(parts) == terminal::invalid
    ? terminal::invalid : m_cmds->find(parts[0])->second.cmd_name;
}

void terminal::determine_flag(terminal::command_t cmd, std::vector<std::string>& parts) noexcept {
    for(auto i = m_cmds->begin(); i != m_cmds->end(); i++) {
        if(cmd == i->second.cmd_name) {
            for(int j = 0; j < i->second.flags.size(); j++) {
                if(parts[1] == i->second.flags[j].name) {
                    i->second.flags[j].func_ptr(parts);
                    return;
                }
            }
        }
    }
}

std::vector<std::string> terminal::split(const char* line, char sep) noexcept {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string x;

    while ((getline(ss, x, sep))) {
        if (x != "")
            tokens.push_back(x);
    }

    return tokens;
}

void terminal::print_help() noexcept {
    printf("---------  %s  ---------\n", "Help");
    for(auto i = m_cmds->begin(); i != m_cmds->end(); i++) {
        printf(" -> %s - %s\n", i->first.c_str(), i->second.cmd_desc);
    }
    printf("---------  %s  ---------\n", "End");
}


void terminal::clear_scr() const noexcept {
    printf(CLEAR_SCR);
}

terminal::command_t valid_vfs(std::vector<std::string>& parts) noexcept {
    if(parts.size() > 4)
        return terminal::invalid;

    if(parts.size() == 1)
        return terminal::vfs;

    switch(hash(parts[1].c_str())) {
        case hash("ls"): {
            if(parts.size() > 2)
                return terminal::invalid;
            break;
        }
        case hash("add"): {
            if(parts.size() < 3 || parts.size() > 4)
                return terminal::invalid;
            break;
        }
        case hash("rm"): {
            if(parts.size() != 3)
                return terminal::invalid;
            break;
        }
        case hash("mnt"): {
            if(parts.size() != 3)
                return terminal::invalid;
            break;
        }
        case hash("umnt"): {
            if(parts.size() != 2)
                return  terminal::invalid;
            break;
        }
        default: return terminal::invalid;
    }
    return terminal::vfs;
}

terminal::command_t valid_ls(std::vector<std::string>& parts) noexcept {
    if(parts.size() != 1)
        return terminal::invalid;

    return terminal::ls;
}

terminal::command_t valid_mkdir(std::vector<std::string>& parts) noexcept {
    if(parts.size() != 2)
        return terminal::invalid;

    return terminal::mkdir;
}

terminal::command_t valid_cd(std::vector<std::string>& parts) noexcept {
    if(parts.size() != 2)
        return terminal::invalid;

    return terminal::cd;
}


terminal::command_t valid_rm(std::vector<std::string>& parts) noexcept {
    if(parts.size() <= 1)
        return terminal::invalid;

    return terminal::rm;
}

terminal::command_t valid_touch(std::vector<std::string>& parts) noexcept {
    if(parts.size() == 2)
        return terminal::touch;
    else return terminal::invalid;
}

terminal::command_t valid_mv(std::vector<std::string>& parts) noexcept {
    if(parts.size() < 3 || parts.size() > 4)
        return terminal::invalid;
    else return terminal::touch;
}

terminal::command_t valid_cp(std::vector<std::string>& parts) noexcept {
    if(parts.size() <= 4)
        return terminal::touch;
    else return terminal::invalid;
}

terminal::command_t valid_cat(std::vector<std::string>& parts) noexcept {
    if(parts.size() == 2)
        return terminal::cat;
    else return terminal::invalid;
}

terminal::command_t valid_help(std::vector<std::string>& parts) noexcept {
    if(parts.size() == 1)
        return terminal::help;

    return terminal::invalid;
}

terminal::command_t valid_clear(std::vector<std::string>& parts) noexcept {
    if(parts.size() != 1) {
        return terminal::invalid;
    } else return terminal::clear;
}

void terminal::wrap_add_disk(std::vector<std::string>& parts) {
    m_vfs->add_disk(parts);
}

void terminal::wrap_umnt_disk(std::vector<std::string>& parts) {
    m_vfs->umnt_disk(parts);
    m_env = terminal::EXTERNAL;
}

void terminal::wrap_mnt_disk(std::vector<std::string>& parts) {
    printf("%s  %s  %s", "\n--------------------", parts[2].c_str(), "--------------------\n");
    LOG(Log::INFO, "Mounting '" + parts[2] + "' as primary FS on the vfs");
    m_vfs->mnt_disk(parts);
    m_env = terminal::INTERNAL;
}

void terminal::wrap_rm_disk(std::vector<std::string>& parts) {
    m_vfs->rm_disk(parts);
}

void terminal::wrap_ls_disk(std::vector<std::string>& parts) {
    m_vfs->lst_disks(parts);
}

