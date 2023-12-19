/* hyx_logger.h
 *
 * Copyright 2023 Michael Pollak. All rights reserved.
 */

#ifndef HYX_LOGGER_H
#define HYX_LOGGER_H

#include <chrono>
#include <concepts>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <string.h>
#include <string_view>
#include <syncstream>
#include <type_traits>

namespace hyx {
    class logger {
    private:
        struct string_with_location {
            std::string_view format;
            std::source_location location;

            template<typename T>
                requires std::convertible_to<const T&, std::string_view>
            consteval string_with_location(const T& fmt, const std::source_location& loc = std::source_location::current()) : format(fmt), location(loc)
            {
            }
        };

        // log level base type (using CRTP)
        template<typename ConcreteLogLevel>
        struct log_level_t {
        public:
            // we use string_with_location to get the caller's location through implicit conversion instead of the local location when header gets implicitly cast
            template<typename... Args>
            constexpr void operator()(const string_with_location& strwloc, Args&&... args) const
            {
                header(static_cast<ConcreteLogLevel>(*this), strwloc.location) << std::vformat(strwloc.format, std::make_format_args(args...)) << hyx::logger::emit;
            }

        protected:
            // log_level_t needs to get inherited
            log_level_t() = default;
            log_level_t(const log_level_t&) = default;
            log_level_t(log_level_t&&) = default;
        };

        template<typename ConcreteLogLevel>
            requires std::derived_from<ConcreteLogLevel, log_level_t<ConcreteLogLevel>>
        class log_function_guard {
        public:
            log_function_guard(const ConcreteLogLevel ll, const std::source_location& loc = std::source_location::current()) : log_level(ll), source_location(loc)
            {
                header(this->log_level, this->source_location) << this->source_location.function_name() << ": Start\n"
                                                               << hyx::logger::emit;
            }

            log_function_guard(const log_function_guard&) = delete;
            log_function_guard& operator=(const log_function_guard&) = delete;

            ~log_function_guard()
            {
                header(this->log_level, this->source_location) << this->source_location.function_name() << ": End\n"
                                                               << hyx::logger::emit;
            }

        private:
            const ConcreteLogLevel log_level;
            const std::source_location& source_location;
        };

        // log level types (using CRTP from log_level_t)
        struct trace_t : public log_level_t<trace_t> {};
        struct debug_t : public log_level_t<debug_t> {};
        struct info_t : public log_level_t<info_t> {};
        struct warning_t : public log_level_t<warning_t> {};
        struct error_t : public log_level_t<error_t> {};
        struct fatal_t : public log_level_t<fatal_t> {};

        template<typename T>
            requires std::derived_from<T, log_level_t<T>>
        struct log_level_to_string {
            constexpr static auto value = []() {
                // return a string literal for the given type
                if constexpr (std::is_same_v<T, trace_t>) {
                    return "TRACE";
                }
                else if constexpr (std::is_same_v<T, debug_t>) {
                    return "DEBUG";
                }
                else if constexpr (std::is_same_v<T, info_t>) {
                    return "INFO";
                }
                else if constexpr (std::is_same_v<T, warning_t>) {
                    return "WARNING";
                }
                else if constexpr (std::is_same_v<T, error_t>) {
                    return "ERROR";
                }
                else if constexpr (std::is_same_v<T, fatal_t>) {
                    return "FATAL";
                }
                else {
                    throw std::invalid_argument("Unknown log level");
                }
            }();
        };

        template<typename T>
            requires std::derived_from<T, log_level_t<T>>
        constexpr static auto log_level_to_string_v = log_level_to_string<T>::value;

        class header {
        private:
            void create_header(const std::string_view level, const std::source_location& source)
            {
                // default header format
                // --------------30--------------[----9----]: -?-@-?-: -?-
                // utc[LEVEL]: source@line: prefix
                *this << std::format("{:%FT%TZ}[{:^9}]: {}@{}: {}", std::chrono::utc_clock::now(), level, std::filesystem::path(source.file_name()).filename().string(), source.line(), logger::get_prefix());
            }

        public:
            template<typename T>
                requires std::derived_from<T, log_level_t<T>>
            /*implicit*/ header(const T& log_level, const std::source_location& source = std::source_location::current())
            {
                create_header(log_level_to_string_v<std::remove_cvref_t<decltype(log_level)>>, source);
            }
        };

    public:
        // log level objects to access
        trace_t trace;
        debug_t debug;
        info_t info;
        warning_t warning;
        error_t error;
        fatal_t fatal;

        // not copyable
        logger(const logger& rhs) = delete;

        ~logger();

        void swap_to(const std::filesystem::path& logpath);

        void swap_to(std::ostream& new_out_stream);

        void push_prefix(const std::string_view pre);

        void pop_prefix();

        void disable();

        void enable();

        static std::string get_prefix()
        {
            return prefix;
        }

        explicit logger(const std::filesystem::path& logpath);

        explicit logger() {}

        friend const header& operator<<(const header& head, const auto& out)
        {
            out_stream << out;
            return head;
        }

        friend const header& operator<<(const header& head, const header& (*func)(const header&))
        {
            func(head);
            return head;
        }

        friend const header& operator<<(const header& head, std::ostream& (*ostream_func)(std::ostream&))
        {
            ostream_func(out_stream);
            return head;
        }

        static const header& emit(const header& head)
        {
            out_stream.emit();
            return head;
        }

    private:
        void open(const std::filesystem::path& log_path)
        {
            using namespace std::string_literals;

            if (!std::filesystem::exists(log_path.parent_path()) && !std::filesystem::create_directories(log_path.parent_path())) {
                throw std::runtime_error("Could not create log file @ "s + log_path.string());
            }

            // turn off buffering
            fstream.rdbuf()->pubsetbuf(0, 0);
            fstream.open(log_path, std::ios::app);
        }

        // TODO: do we need fstream here? can we create temp streams and hand them to out_stream?
        std::ofstream fstream;
        static std::osyncstream out_stream;
        static std::string prefix;
    };

    // external global logger
    extern logger logger;
} // namespace hyx

#endif // !HYX_LOGGER_H
