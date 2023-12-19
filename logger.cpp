/* hyx_logger.cpp
 *
 * Copyright 2023 Michael Pollak. All rights reserved.
 */

#include "hyx_logger.h"

#include <chrono>
#include <concepts>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>
#include <syncstream>

// static declare
std::osyncstream hyx::logger::out_stream(std::clog);
std::string hyx::logger::prefix{};

// external global logger
namespace hyx {
    class logger logger;
} // namespace hyx

void hyx::logger::swap_to(const std::filesystem::path& logpath)
{
    this->fstream.close();
    this->fstream.clear();
    this->open(logpath);
    this->out_stream = std::osyncstream(this->fstream);
}

void hyx::logger::swap_to(std::ostream& new_out_stream)
{
    this->fstream.close();
    this->fstream.clear();
    this->out_stream = std::osyncstream(new_out_stream);
}

void hyx::logger::push_prefix(const std::string_view pre)
{
    prefix += pre;
    prefix += ": ";
}

void hyx::logger::pop_prefix()
{
    if (!prefix.empty()) {
        // remove the ending ": " so we can find the second to last
        prefix.pop_back();
        prefix.pop_back();

        // in the case we only have one prefix, there won't be any more ': '
        if (auto ending_idx = prefix.find_last_of(":"); ending_idx != std::string::npos)
            prefix = prefix.substr(0, ending_idx + 2);
        else
            prefix.clear();
    }
}

void hyx::logger::disable()
{
    this->out_stream.setstate(std::ios::failbit);
}

void hyx::logger::enable()
{
    this->out_stream.setstate(std::ios::goodbit);
}

hyx::logger::logger(const std::filesystem::path& logpath)
{
    this->open(logpath);
    this->out_stream = std::osyncstream(this->fstream);
}

hyx::logger::~logger()
{
    out_stream.emit();
}
