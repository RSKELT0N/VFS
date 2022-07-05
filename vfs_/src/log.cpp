#include "../include/log.h"
#include <memory>

using namespace VFS;

std::shared_ptr<VFS::log> VFS::log::logInstance = NULL;

std::shared_ptr<VFS::log> log::getInstance()
{
    if (logInstance == nullptr) {
        logInstance = std::shared_ptr<VFS::log>(new VFS::log());
    }
    return logInstance;
}

void log::logger(Type type, int lineNumber, const std::string& log, std::string col)
{
    std::stringstream ss;
    switch (type)
    {
    case log::WARNING:
        col = log::colour.YELLOW;
        break;
    case log::INFO:
        col = log::colour.GREEN;
        break;
    case log::SERVER:
        col = log::colour.MAGENTA;
        break;
    case log::DEBUG:
        col = log::colour.BLUE;
        break;
    case log::ERROR_:
        col = log::colour.RED;
        break;
    }

    lineNumber = 0;

    ss << colour.RESET;
    ss << "[ " << col << typeMap[type] << colour.RESET << " ] ";
    ss << log;
    ss << colour.RESET;

    std::cout << ss.str() << "\n";

    if(type == Type::ERROR_)
        exit(EXIT_FAILURE);
}

std::string log::logger_str(Type type, int lineNumber, const std::string& log, std::string col) {
    std::stringstream ss;
    switch (type)
    {
    case log::WARNING:
        col = log::colour.YELLOW;
        break;
    case log::INFO:
        col = log::colour.GREEN;
        break;
    case log::SERVER:
        col = log::colour.MAGENTA;
        break;
    case log::DEBUG:
        col = log::colour.BLUE;
        break;
    case log::ERROR_:
        col = log::colour.RED;
        break;
    }

    lineNumber = 0;

    ss << colour.RESET;
    ss << "[ " << col << typeMap.find(type)->second << colour.RESET << " ] ";
    ss << log;
    ss << colour.RESET;
    ss << "\n";

    if(type == Type::ERROR_)
        exit(EXIT_FAILURE);
    return ss.str();
}
