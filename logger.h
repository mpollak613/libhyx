// <hyx/logger.h> -*- C++ -*-
// Copyright (C) 2023 Michael Pollak
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef HYX_LOGGER_H
#define HYX_LOGGER_H

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <hyx/header_string.h>
#include <iostream>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <syncstream>
#include <type_traits>

namespace hyx {
    template<typename... Args>
    struct format_string_with_location {
        template<typename T>
            requires std::convertible_to<const T&, std::format_string<Args...>>
        consteval format_string_with_location(const T& s, const std::source_location& l = std::source_location::current()) : fstr(s), loc(l)
        {
        }

        std::format_string<Args...> fstr;
        std::source_location loc;
    };

    class log_level {
    public:
        consteval log_level(const std::string_view lbl) noexcept : label(lbl) {}

        [[nodiscard]] auto to_string_view() const noexcept
        {
            return label;
        }

    private:
        std::string_view label;
    };

    inline namespace logger_literals {
        inline constinit static const log_level trace{"TRACE"};
        inline constinit static const log_level debug{"DEBUG"};
        inline constinit static const log_level info{"INFO"};
        inline constinit static const log_level warning{"WARNING"};
        inline constinit static const log_level error{"ERROR"};
        inline constinit static const log_level fatal{"FATAL"};

        inline consteval log_level operator""_lvl(const char* str, std::size_t len) noexcept
        {
            return {{str, len}};
        }
    } // namespace logger_literals

    class logger {
    public:
        logger() noexcept = default;

        // not copyable or movable
        explicit logger(const logger&) = delete;
        explicit logger(logger&&) = delete;
        logger& operator=(const logger&) = delete;
        logger& operator=(logger&&) = delete;

        ~logger() = default;

        template<typename... Args>
        explicit logger(std::ostream& os, const header_string<std::type_identity_t<Args>...> fmt = "", Args&&... args) : sink_(os), header(std::format(fmt, std::forward<Args>(args)...)) {}

        template<typename... Args>
        explicit logger(std::ofstream& ofs, const header_string<std::type_identity_t<Args>...> fmt = "", Args&&... args) : sink_(ofs), header(std::format(fmt, std::forward<Args>(args)...)) {}

        template<typename... Args>
        explicit logger(const std::filesystem::path& path, const header_string<std::type_identity_t<Args>...> fmt = "", Args&&... args) : file_sink_(path, std::ios_base::app), sink_(file_sink_), header(std::format(fmt, std::forward<Args>(args)...))
        {
            if (path.filename().empty()) {
                throw std::invalid_argument("log output path does not contain a filename");
            }
        }

        template<typename... Args>
        void operator()(const format_string_with_location<std::type_identity_t<Args>...>& fmt, Args&&... args)
        {
            logger::operator()(logger_literals::info, fmt, std::forward<Args>(args)...);
        }

        template<typename... Args>
        void operator()(log_level lvl, const format_string_with_location<std::type_identity_t<Args>...>& fmt, Args&&... args)
        {
            // first output the header
            format_scanner fc(header, std::ostream_iterator<char>(sink_), lvl, fmt.loc);
            fc.scan();

            // now we can output log-site data
            sink_ << std::format(fmt.fstr, std::forward<Args>(args)...);

            // we need both to ensure osyncstream outputs on each call
            sink_.flush();
            sink_.emit();
        }

        void disable()
        {
            sink_.setstate(std::ios::failbit);
        }

        void enable()
        {
            sink_.clear();
        }

    private:
        template<typename OutIt>
            requires(std::output_iterator<OutIt, const char&>)
        class format_scanner : public basic_scanner {
        public:
            constexpr format_scanner(std::string_view fmt, OutIt out, log_level lvl, const std::source_location& sl) : basic_scanner(fmt), out_(out), lvl_(lvl), sl_(sl) {}

        private:
            OutIt out_;
            log_level lvl_;
            std::source_location sl_;

            constexpr void on_event(iterator end) override
            {
                std::unique_copy(begin(), end, out_, [](auto a, auto b) { return (a == '[' && b == '[') || (a == ']' && b == ']'); });
            }

            constexpr void consume_spec(detail::spec_id id) override
            {
                const std::string_view fmt{"{:"s.append(std::string_view{ctx_.begin(), ctx_.subend()}).append("}")};

                switch (id) {
                    using enum hyx::detail::spec_id;
                case lvl:
                    std::vformat_to(out_, fmt, std::make_format_args(lvl_.to_string_view()));
                    break;
                case sys:
                    std::vformat_to(out_, fmt, std::make_format_args(std::chrono::system_clock::now()));
                    break;
                case utc:
                    std::vformat_to(out_, fmt, std::make_format_args(std::chrono::utc_clock::now()));
                    break;
                case tai:
                    std::vformat_to(out_, fmt, std::make_format_args(std::chrono::tai_clock::now()));
                    break;
                case gps:
                    std::vformat_to(out_, fmt, std::make_format_args(std::chrono::gps_clock::now()));
                    break;
                case file:
                    std::vformat_to(out_, fmt, std::make_format_args(std::chrono::file_clock::now()));
                    break;
                case line:
                    std::vformat_to(out_, fmt, std::make_format_args(sl_.line()));
                    break;
                case column:
                    std::vformat_to(out_, fmt, std::make_format_args(sl_.column()));
                    break;
                case file_name:
                    std::vformat_to(out_, fmt, std::make_format_args(sl_.file_name()));
                    break;
                case function_name:
                    std::vformat_to(out_, fmt, std::make_format_args(sl_.function_name()));
                    break;
                }
            }
        };

        std::ofstream file_sink_{};
        std::osyncstream sink_{std::clog};
        std::string header;
    };
} // namespace hyx

#endif // !HYX_LOGGER_H
