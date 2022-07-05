#include "../include/terminal.h"

using namespace VFS;

vfs* terminal::m_vfs;
terminal* terminal::m_terminal;

void remote_interpret_cmd(vfs::system_cmd& cmd, std::vector<std::string>& args, char*& payload, uint64_t size, int8_t options) noexcept {
    terminal::get_instance()->translate_remote_cmd(cmd, args);
    terminal::get_instance()->interpret_cmd(cmd, args, payload, size, options);
}

terminal::terminal() {
    m_vfs = vfs::get_vfs();
    sys_lock = std::make_unique<std::mutex>();
    m_syscmds = std::make_shared<std::unordered_map<std::string, cmd_t>>();
    m_mnted_system = reinterpret_cast<vfs::system_t **>((*m_vfs).get_mnted_system().get());

    cmd_map();
}

terminal::~terminal() {
    delete m_vfs;
}

terminal* terminal::get_instance() noexcept {
    if(!m_terminal) {
        m_terminal = new terminal();
    }

    return m_terminal;
}

void terminal::run() noexcept {
    BUFFER << LOG_str(log::INFO, "Terminal: defined, /help for cmd list");
    BUFFER << "-------------------------------------------------\n";
    std::string line;

    while(1) {
        
        BUFFER.print_stream();
        printf("%s", ">> ");
        BUFFER.release_buffer();

        std::getline(std::cin, line);
        printf("%s%s", "\x1b[A", "\33[2K");

        if(strcmp(line.c_str(), "/exit") == 0) {
            if(m_vfs->is_mnted() && dynamic_cast<RFS::rfs*>(m_vfs->get_mnted_system()->mp_fs.get())) {
                std::vector<std::string> args;
                m_vfs->umnt_disk(args);
                continue;
            } else return;
        }

        if(!line.empty()) {
            if(line[0] != ' ') {
                BUFFER.hold_buffer();
                input(line.c_str());
            }
        }
    }
}

void terminal::input(const char* line) noexcept {
    std::vector<std::string> args = lib_::split(line, SEPARATOR);
    vfs::system_cmd command = validate_cmd(args);

    args.erase(args.begin()); // remove initial to retrieve only args
    interpret_cmd(command, args);
}


vfs::system_cmd terminal::validate_cmd(std::vector<std::string> &parts) noexcept {
    if(m_syscmds->find(parts[0]) == m_syscmds->end())
        return vfs::system_cmd::invalid;

    return (*this.*m_syscmds->find(parts[0])->second.valid)(parts) == vfs::system_cmd::invalid
    ? vfs::system_cmd::invalid : m_syscmds->find(parts[0])->second.cmd_name;
}

void terminal::interpret_cmd(vfs::system_cmd& cmd, std::vector<std::string>& args, char*& payload, uint64_t size, int8_t options) noexcept {

    if(cmd == vfs::system_cmd::invalid) {
        BUFFER << "command is invalid, use /help\n";
        return;
    }

    (*this.*m_syscmds->find(vfs::syscmd_str[(int)cmd])->second.funct)(cmd, args, payload, size, options);
}

void terminal::translate_remote_cmd(vfs::system_cmd& cmd, std::vector<std::string>& args) noexcept {
    if(cmd == vfs::system_cmd::cp) {
        if(strcmp(args[0].c_str(), "imp") == 0) {
            args.erase(args.begin());
            args.erase(args.begin());
            cmd = vfs::system_cmd::touch;
        } else if(strcmp(args[0].c_str(), "exp") == 0) {
            args.erase(args.begin());
            args.erase(args.end());
            cmd = vfs::system_cmd::cat;
        }
    }
}

void terminal::print_help(vfs::system_cmd cmd, std::vector<std::string> args, char*&, uint64_t size, int8_t options) noexcept {
    BUFFER << "\n--------- Help ---------\n";
    uint32_t j = 0;
    for(auto i = m_vfs->get_sys_cmds()->begin(); i != m_vfs->get_sys_cmds()->end(); i++) {
        BUFFER << vfs::syscmd_str[j] << " -> " << i->desc << "\n";
        j++;
    }

    BUFFER << "\nmp_vfs - must start with '/'\n";
    BUFFER << "---------  End  ---------\n";
}

void terminal::clear_scr(vfs::system_cmd cmd, std::vector<std::string> args, char*& payload, uint64_t size, int8_t options) noexcept {
    printf(CLEAR_SCR);
}

void terminal::map_vfs_funct(vfs::system_cmd cmd, std::vector<std::string> args, char*& payload, uint64_t size, int8_t options) noexcept {
    sys_lock->lock();
    if(m_vfs->is_mnted() && strcmp(m_vfs->get_mnted_system()->fs_type, "rfs") == 0) {
        sys_lock->unlock();
        map_sys_funct(cmd, args, payload, size);
    } else m_vfs->control_vfs(args);
    sys_lock->unlock();
}

void terminal::map_sys_funct(vfs::system_cmd cmd, std::vector<std::string> args, char*& payload, uint64_t size, int8_t options) noexcept {
    if(!m_vfs->is_mnted()) {
        BUFFER << LOG_str(log::WARNING, "Please mount a system before carrying out a sys call");
        return;
    }
    sys_lock->lock();
    (m_vfs->*m_vfs->get_mnted_system()->access)(cmd, args, payload, size, options);
    sys_lock->unlock();
}


vfs::system_cmd terminal::valid_vfs(std::vector<std::string>& parts) noexcept {
    if(parts.size() > 6) return vfs::system_cmd::invalid;
    if(parts.size() == 1) return vfs::system_cmd::invalid;

    switch(lib_::hash(parts[1].c_str())) {
        case lib_::hash("ls"):     if(parts.size() > 2)                       return vfs::system_cmd::invalid; break;
        case lib_::hash("ifs"):    if(parts.size() != 4 && parts.size() != 5) return vfs::system_cmd::invalid; break;
        case lib_::hash("rfs"):    if(parts.size() != 4 && parts.size() != 6) return vfs::system_cmd::invalid; break;
        case lib_::hash("mnt"):    if(parts.size() != 3)                      return vfs::system_cmd::invalid; break;
        case lib_::hash("umnt"):   if(parts.size() != 2)                      return vfs::system_cmd::invalid; break;
        case lib_::hash("server"): if(parts.size() != 2 && parts.size() != 3) return vfs::system_cmd::invalid; break;
        default: return vfs::system_cmd::invalid;
    }
    return vfs::system_cmd::vfs_;
}

void terminal::cmd_map() noexcept {
    (*m_syscmds)["/vfs"]   = cmd_t{vfs::system_cmd::vfs_,  &terminal::valid_vfs,   &terminal::map_vfs_funct};
    (*m_syscmds)["ls"]     = cmd_t{vfs::system_cmd::ls,    &terminal::valid_ls,    &terminal::map_sys_funct};
    (*m_syscmds)["mkdir"]  = cmd_t{vfs::system_cmd::mkdir, &terminal::valid_mkdir, &terminal::map_sys_funct};
    (*m_syscmds)["cd"]     = cmd_t{vfs::system_cmd::cd,    &terminal::valid_cd,    &terminal::map_sys_funct};
    (*m_syscmds)["rm"]     = cmd_t{vfs::system_cmd::rm,    &terminal::valid_rm,    &terminal::map_sys_funct};
    (*m_syscmds)["touch"]  = cmd_t{vfs::system_cmd::touch, &terminal::valid_touch, &terminal::map_sys_funct};
    (*m_syscmds)["mv"]     = cmd_t{vfs::system_cmd::mv,    &terminal::valid_mv,    &terminal::map_sys_funct};
    (*m_syscmds)["cp"]     = cmd_t{vfs::system_cmd::cp,    &terminal::valid_cp,    &terminal::map_sys_funct};
    (*m_syscmds)["cat"]    = cmd_t{vfs::system_cmd::cat,   &terminal::valid_cat,   &terminal::map_sys_funct};
    (*m_syscmds)["/help"]  = cmd_t{vfs::system_cmd::help,  &terminal::valid_help,  &terminal::print_help};
    (*m_syscmds)["/clear"] = cmd_t{vfs::system_cmd::clear, &terminal::valid_clear, &terminal::clear_scr};
}

vfs::system_cmd terminal::valid_cp(std::vector<std::string>& parts) noexcept {
    if(parts.size() == 1)
        return vfs::system_cmd::invalid;

    if(strcmp(parts[1].c_str(), "imp") == 0 || strcmp(parts[1].c_str(), "exp") == 0) {
        if(parts.size() == 4)
            return vfs::system_cmd::cp;
    } else if(parts.size() == 3)
        return vfs::system_cmd::cp;

    return vfs::system_cmd::invalid;
}


vfs::system_cmd terminal::valid_ls(std::vector<std::string>& parts)    noexcept { return (parts.size() != 1)   ? vfs::system_cmd::invalid : vfs::system_cmd::ls;      }
vfs::system_cmd terminal::valid_mkdir(std::vector<std::string>& parts) noexcept { return (parts.size() != 2)   ? vfs::system_cmd::invalid : vfs::system_cmd::mkdir;   }
vfs::system_cmd terminal::valid_cd(std::vector<std::string>& parts)    noexcept { return (parts.size() != 2)   ? vfs::system_cmd::invalid : vfs::system_cmd::cd;      }
vfs::system_cmd terminal::valid_rm(std::vector<std::string>& parts)    noexcept { return (parts.size() < 2)    ? vfs::system_cmd::invalid : vfs::system_cmd::rm;      }
vfs::system_cmd terminal::valid_touch(std::vector<std::string>& parts) noexcept { return (parts.size() >  1)   ? vfs::system_cmd::touch   : vfs::system_cmd::invalid; }
vfs::system_cmd terminal::valid_mv(std::vector<std::string>& parts)    noexcept { return (parts.size() != 3)   ? vfs::system_cmd::invalid : vfs::system_cmd::mv;      }
vfs::system_cmd terminal::valid_cat(std::vector<std::string>& parts)   noexcept { return (parts.size() == 2)   ? vfs::system_cmd::cat     : vfs::system_cmd::invalid; }
vfs::system_cmd terminal::valid_help(std::vector<std::string>& parts)  noexcept { return (parts.size() == 1)   ? vfs::system_cmd::help    : vfs::system_cmd::invalid; }
vfs::system_cmd terminal::valid_clear(std::vector<std::string>& parts) noexcept { return (parts.size() == 1)   ? vfs::system_cmd::clear   : vfs::system_cmd::invalid; }
