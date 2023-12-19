// <hyx/header_string.h> -*- C++ -*-
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

#include <algorithm>
#include <chrono>
#include <concepts>
#include <format>
#include <hyx/type_sequence.h>
#include <iostream>
#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

// in the following code, comments will use these shorthands:
// (! == not previous ;; ? == wildcard, including previous ;; ... == context)

namespace hyx {
    using namespace std::literals;

    class scan_context {
    public:
        using iterator = std::string_view::const_iterator;
        using const_iterator = std::string_view::const_iterator;

        constexpr explicit scan_context(std::string_view fmt) noexcept : begin_(fmt.begin()), subend_(fmt.end()), end_(subend_)
        {
        }

        scan_context(const scan_context&) = delete;

        constexpr const_iterator begin() const noexcept
        {
            return begin_;
        }

        constexpr const_iterator subend() const noexcept
        {
            return subend_;
        }

        constexpr const_iterator end() const noexcept
        {
            return end_;
        }

        constexpr void advance_subend_to(const_iterator it) noexcept
        {
            subend_ = it;
        }

        constexpr void advance_to(const_iterator it) noexcept
        {
            begin_ = it;
        }

    private:
        iterator begin_;
        iterator subend_;
        const_iterator end_;
    };

    inline void _throw_format_error(const char* what)
    {
        throw std::format_error(what);
    }

    inline void _unmatched_right_bracket_in_format_string()
    {
        _throw_format_error("format error: unmatched ']' in format string");
    }

    inline void _unmatched_left_bracket_in_format_string()
    {
        _throw_format_error("format error: unmatched '[' in format string");
    }

    // returns next valid [ or ] iterator (e.g., not [[ or ]]) or end()
    constexpr std::contiguous_iterator auto find_next_valid_bracket(std::contiguous_iterator auto start, const std::contiguous_iterator auto end)
    {
        //  cases:
        //    ...[!... ok
        //    ...[[?... skip ([ next is ok since we want the right-most one)
        //    ...]!... ok
        //    ...]]!... skip
        //    ...]]]!... ok (use the left-most one)

        while (start != end) {
            if /* [... */ (*start == '[') {
                if /* [! */ (*std::ranges::next(start, 1, end) != '[') {
                    return start;
                }
                // else [[, advance twice (once here and once before looping)
                std::ranges::advance(start, 1, end);
            }
            else if /* ]... */ (*start == ']') {
                const auto next = std::ranges::next(start, 1, end);
                if /* ]! */ (*next != ']') {
                    return start;
                }
                // else ]]?
                if /* ]]] */ (*std::ranges::next(next, 1, end) == ']') {
                    return start;
                }
                // else ]]!, advance twice (once here and once before looping)
                std::ranges::advance(start, 1, end);
            }
            // else !..., advance to next character
            std::ranges::advance(start, 1, end);
        }

        return end;
    }

    namespace detail {
        enum class spec_id {
            // global
            lvl,

            // clock
            sys,
            utc,
            tai,
            gps,
            file,

            // source
            line,
            column,
            file_name,
            function_name
        };

        struct global_namespace : public std::string_view {
            consteval global_namespace() : std::string_view("::") {}
        };

        struct lvl : public std::string_view {
            consteval lvl() : std::string_view("lvl") {}

            constinit static const auto id{spec_id::lvl};
        };

        struct clock_namespace : public std::string_view {
            consteval clock_namespace() : std::string_view("cl::") {}
        };
        namespace cl {
            struct sys : public std::string_view {
                consteval sys() : std::string_view("sys") {}

                constinit static const auto id{spec_id::sys};
            };

            struct utc : public std::string_view {
                consteval utc() : std::string_view("utc") {}

                constinit static const auto id{spec_id::utc};
            };

            struct tai : public std::string_view {
                consteval tai() : std::string_view("tai") {}

                constinit static const auto id{spec_id::tai};
            };

            struct gps : public std::string_view {
                consteval gps() : std::string_view("gps") {}

                constinit static const auto id{spec_id::gps};
            };

            struct file : public std::string_view {
                consteval file() : std::string_view("file") {}

                constinit static const auto id{spec_id::file};
            };
        } // namespace cl

        struct source_namespace : public std::string_view {
            consteval source_namespace() : std::string_view("sl::") {}
        };
        namespace sl {
            struct line : public std::string_view {
                consteval line() : std::string_view("line") {}

                constinit static const auto id{spec_id::line};
            };

            struct column : public std::string_view {
                consteval column() : std::string_view("column") {}

                constinit static const auto id{spec_id::column};
            };

            struct file_name : public std::string_view {
                consteval file_name() : std::string_view("file_name") {}

                constinit static const auto id{spec_id::file_name};
            };

            struct function_name : public std::string_view {
                consteval function_name() : std::string_view("function_name") {}

                constinit static const auto id{spec_id::function_name};
            };
        } // namespace sl
    }     // namespace detail

    struct basic_scanner {
    public:
        using iterator = typename std::string_view::iterator;

        using GlobalSpecs = std::pair<detail::global_namespace, hyx::meta::type_sequence<detail::lvl>>;
        using ClockSpecs = std::pair<detail::clock_namespace, hyx::meta::type_sequence<detail::cl::sys, detail::cl::utc, detail::cl::tai, detail::cl::gps, detail::cl::file>>;
        using SourceSpecs = std::pair<detail::source_namespace, hyx::meta::type_sequence<detail::sl::line, detail::sl::column, detail::sl::file_name, detail::sl::function_name>>;
        using AvailableSpecs = hyx::meta::type_sequence<GlobalSpecs, ClockSpecs, SourceSpecs>;

        constexpr explicit basic_scanner(std::string_view str) : ctx_(str) {}

        constexpr iterator begin() const noexcept
        {
            return ctx_.begin();
        }

        constexpr iterator end() const noexcept
        {
            return ctx_.end();
        }

        constexpr void scan()
        {
            while (begin() != end()) {
                auto lb = find_next_valid_bracket(begin(), end());
                if (lb == end()) {
                    on_event(lb);
                    return;
                }
                else if (*lb == ']') {
                    _unmatched_right_bracket_in_format_string();
                }
                // else, lb == '['
                std::ranges::advance(lb, 1, end());

                const auto rb = find_next_valid_bracket(lb, end());
                if (*rb == '[' || rb == end()) {
                    _unmatched_left_bracket_in_format_string();
                }
                // else, rb == ']'

                // we only want to event things before the '['
                on_event(std::ranges::prev(lb));
                parse_namespace<AvailableSpecs>({lb, rb});
                // subend will always point to ']' (so we need to go one step further)
                ctx_.advance_to(std::ranges::next(ctx_.subend(), 1, end()));
            }
        }

        constexpr std::string_view as_string() const noexcept
        {
            return {begin(), end()};
        }

        // WARNING: for now, on_event needs to replace [[ and ]] with [ and ] when formatting
        constexpr virtual void on_event(iterator) {}

        constexpr virtual void consume_spec(detail::spec_id id) = 0;

        template<typename TypeSequence>
            requires(hyx::meta::is_empty_v<TypeSequence>)
        constexpr void parse_member([[maybe_unused]] std::string_view fmt)
        {
            _throw_format_error("format error: unknown namespace member spec");
        }

        template<typename TypeSequence>
        constexpr void parse_member(std::string_view fmt)
        {
            using Member = hyx::meta::front_t<TypeSequence>;

            if (constexpr Member mb{}; fmt.starts_with(mb)) {
                fmt.remove_prefix(mb.size());
                if (fmt.front() != ';') {
                    _throw_format_error("format error: missing semi-colon in namespace spec");
                }
                else {
                    // remove ';'
                    fmt.remove_prefix(1);
                    ctx_.advance_to(fmt.begin());
                    ctx_.advance_subend_to(fmt.end());
                    consume_spec(mb.id);
                }
            }
            else {
                parse_member<hyx::meta::pop_front_t<TypeSequence>>(fmt);
            }
        }

        template<typename TypeSequence>
            requires(hyx::meta::is_empty_v<TypeSequence>)
        constexpr void parse_namespace([[maybe_unused]] std::string_view fmt)
        {
            _throw_format_error("format error: unknown namespace spec");
        }

        template<typename TypeSequence>
        constexpr void parse_namespace(std::string_view fmt)
        {
            using Specs = hyx::meta::front_t<TypeSequence>;
            using Namespace = Specs::first_type;
            using MembersSequence = Specs::second_type;

            if (constexpr Namespace ns{}; fmt.starts_with(ns)) {
                fmt.remove_prefix(ns.size());
                parse_member<MembersSequence>(fmt);
            }
            else {
                parse_namespace<hyx::meta::pop_front_t<TypeSequence>>(fmt);
            }
        }

        scan_context ctx_;
    };

    class checking_scanner : public basic_scanner {
    public:
        constexpr checking_scanner(std::string_view fmt) : basic_scanner(fmt) {}

    private:
        constexpr void consume_spec(detail::spec_id id) override
        {
            std::format_parse_context pc{{ctx_.begin(), ctx_.subend()}};

            switch (id) {
            case detail::spec_id::sys:
                std::formatter<std::chrono::time_point<std::chrono::system_clock>>{}.parse(pc);
                break;
            case detail::spec_id::utc:
                std::formatter<std::chrono::time_point<std::chrono::utc_clock>>{}.parse(pc);
                break;
            case detail::spec_id::tai:
                std::formatter<std::chrono::time_point<std::chrono::tai_clock>>{}.parse(pc);
                break;
            case detail::spec_id::gps:
                std::formatter<std::chrono::time_point<std::chrono::gps_clock>>{}.parse(pc);
                break;
            case detail::spec_id::file:
                std::formatter<std::chrono::time_point<std::chrono::file_clock>>{}.parse(pc);
                break;
            case detail::spec_id::line:
                // same as below
            case detail::spec_id::column:
                // uint_least32_t as defined by the standard for line and column
                std::formatter<std::uint_least32_t>{}.parse(pc);
                break;
            case detail::spec_id::lvl:
                // same as below
            case detail::spec_id::file_name:
                // same as below
            case detail::spec_id::function_name:
                std::formatter<std::string>{}.parse(pc);
                break;
            }
        }
    };

    template<typename... Args>
    class header_string : public std::format_string<Args...> {
    public:
        template<typename T>
            requires std::convertible_to<const T&, std::string_view>
        consteval header_string(const T& s) : std::format_string<Args...>(s)
        {
            checking_scanner bs{s};
            bs.scan();
        }
    };
} // namespace hyx
