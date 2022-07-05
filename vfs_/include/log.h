#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>

#define LOG_INSTANCE       (Log::getInstance().get())
#define LOG(type, msg)     (log::getInstance().get()->logger(type, __LINE__, msg, ""))
#define LOG_str(type, msg) (log::getInstance().get()->logger_str(type, __LINE__, msg, "").c_str())

namespace VFS {

    class log {
    public:
        enum Type {
            WARNING,
            INFO,
            SERVER,
            DEBUG,
            ERROR_
        };

        struct Colours     {
            const std::string RED     = "\033[1;31m";
            const std::string GREEN   = "\033[1;92m";
            const std::string YELLOW  = "\033[1;93m";
            const std::string BLUE    = "\033[1;34m";
            const std::string MAGENTA = "\033[1;95m";
            const std::string CYAN    = "\033[1;96m";
            const std::string WHITE   = "\033[1;37m";
            const std::string RESET   = "\033[1;0m";
        } colour;

        std::unordered_map<log::Type, std::string, std::hash<int>> typeMap = {
                {std::make_pair(log::WARNING, "WARNING")},
                {std::make_pair(log::INFO, "INFO")},
                {std::make_pair(log::SERVER, "SERVER")},
                {std::make_pair(log::DEBUG, "DEBUG")},
                {std::make_pair(log::ERROR_, "ERROR")} };

    public:
        ~log() = default;
        log() = default;
        void operator=(const log&) = delete;
        static std::shared_ptr<log> getInstance();

    public:
        void logger(Type, int, const std::string&, std::string);
        std::string logger_str(Type, int, const std::string&, std::string);

    private:
        static std::shared_ptr<log> logInstance;

    };
}

#endif //LOG_LOG_H
