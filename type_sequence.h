// <hyx/type_sequence.h> -*- C++ -*-
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

#ifndef HYX_TYPE_SEQUENCE_H
#define HYX_TYPE_SEQUENCE_H

// https://stackoverflow.com/questions/2324658/how-to-determine-the-version-of-the-c-standard-used-by-the-compiler
// https://stackoverflow.com/questions/36612596/tuple-to-parameter-pack
// https://en.cppreference.com/w/cpp/types/disjunction

// #include <string> // not needed
// #include <type_traits> // not needed?
#include <utility>

namespace hyx {
    namespace meta {
        template<typename... Ts>
        struct type_sequence {};
    } // namespace meta

    namespace detail {
        using meta::type_sequence;

        // *********************************************************************
        // homogeneous type sequence impl
        // *********************************************************************

        template<std::size_t Count, typename IType, typename... Ts>
        struct _homogeneous_type_sequence_impl;

        template<std::size_t Count, typename IType, typename... Ts>
        struct _homogeneous_type_sequence_impl : _homogeneous_type_sequence_impl<Count - 1, IType, IType, Ts...> {
        };

        template<typename IType, typename... Ts>
        struct _homogeneous_type_sequence_impl<0, IType, Ts...> {
            using type = type_sequence<Ts...>;
        };

        // *********************************************************************
        // get impl
        // *********************************************************************

        template<std::size_t Pos, typename... Ts>
        struct _get_impl;

        template<std::size_t Pos, typename T, typename... Ts>
        struct _get_impl<Pos, type_sequence<T, Ts...>> : _get_impl<Pos - 1, type_sequence<Ts...>> {
        };

        template<typename T, typename... Ts>
        struct _get_impl<0, type_sequence<T, Ts...>> {
            using type = T;
        };

        // *********************************************************************
        // split impl
        // *********************************************************************

        template<std::size_t Pos, typename... Ts>
        struct _split_impl;

        template<std::size_t Pos, typename... Ts, typename U, typename... Us>
        struct _split_impl<Pos, type_sequence<Ts...>, type_sequence<U, Us...>> : _split_impl<Pos - 1, type_sequence<Ts..., U>, type_sequence<Us...>> {
        };

        template<typename... Ts, typename U, typename... Us>
        struct _split_impl<0, type_sequence<Ts...>, type_sequence<U, Us...>> {
            using first = type_sequence<Ts...>;
            using second = type_sequence<U, Us...>;
        };

        template<typename... Ts>
        struct _split_impl<0, type_sequence<Ts...>, type_sequence<>> {
            using first = type_sequence<Ts...>;
            using second = type_sequence<>;
        };

        // *********************************************************************
        // reverse impl
        // *********************************************************************

        template<typename... Ts>
        struct _reverse_impl;

        template<typename... Ts, typename U, typename... Us>
        struct _reverse_impl<type_sequence<Ts...>, type_sequence<U, Us...>> : _reverse_impl<type_sequence<U, Ts...>, type_sequence<Us...>> {
        };

        template<typename... Ts>
        struct _reverse_impl<type_sequence<Ts...>, type_sequence<>> {
            using type = type_sequence<Ts...>;
        };
    } // namespace detail

    namespace meta {
        // type used for testing
        // using short_bool_string_type = type_sequence<short, bool, std::string>;

        // *********************************************************************
        // constructs a homogeneous type_sequence
        // *********************************************************************

        template<std::size_t Count, typename IType>
        using make_homogeneous_type_sequence = typename detail::_homogeneous_type_sequence_impl<Count, IType>::type;

        // static_assert(std::is_same<type_sequence<int, int>, make_homogeneous_type_sequence<2, int>>());

        // *********************************************************************
        // cat: concats two or more type_sequences
        // *********************************************************************

        template<typename... Ts>
        struct cat;

        template<typename... Ts, typename... Us, typename... TypeSequences>
        struct cat<type_sequence<Ts...>, type_sequence<Us...>, TypeSequences...> : cat<type_sequence<Ts..., Us...>, TypeSequences...> {
        };

        template<typename... Ts, typename... Us>
        struct cat<type_sequence<Ts...>, type_sequence<Us...>> {
            using type = type_sequence<Ts..., Us...>;
        };

        template<typename... TypeSequences>
        using cat_t = typename cat<TypeSequences...>::type;

        // static__assert(std::is_same<type_sequence<short, bool>, cat_t<type_sequence<short>, type_sequence<bool>>>());
        // static__assert(std::is_same<type_sequence<short>, cat_t<type_sequence<short>, type_sequence<>>>());
        // static__assert(std::is_same<type_sequence<>, cat_t<type_sequence<>, type_sequence<>>>());
        // static__assert(std::is_same<type_sequence<short, bool, int>, cat_t<type_sequence<short>, type_sequence<bool>, type_sequence<int>>>());

        // *********************************************************************
        // size: returns the number of types
        // *********************************************************************

        template<typename... Ts>
        struct size;

        template<typename... Ts>
        struct size<type_sequence<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {
        };

#if __cplusplus >= 201703L
        template<typename... Ts>
        inline constexpr std::size_t size_v = size<Ts...>::value;
#endif

#if __cplusplus >= 201703L
        // static__assert(3 == size_v<short_bool_string_type>);
        // static__assert(0 == size_v<type_sequence<>>);
#endif

        // *********************************************************************
        // is_empty: checks whether the container is empty
        // *********************************************************************

        template<typename... Ts>
        struct is_empty;

        template<typename T, typename... Ts>
        struct is_empty<type_sequence<T, Ts...>> : std::false_type {
        };

        template<>
        struct is_empty<type_sequence<>> : std::true_type {
        };

#if __cplusplus >= 201703L
        template<typename TypeSequence>
        inline constexpr bool is_empty_v = is_empty<TypeSequence>::value;
#endif

        // static__assert(is_empty<type_sequence<>>::value);
        // static__assert(!is_empty<short_bool_string_type>::value);

        // *********************************************************************
        // get: gets the type at a location
        // *********************************************************************

        template<std::size_t Pos, typename TypeSequence>
#if __cplusplus >= 202002L
            requires(!is_empty<TypeSequence>::value)
#endif
        using get = typename detail::_get_impl<Pos, TypeSequence>::type;

        // static__assert(std::is_same<bool, get<1, short_bool_string_type>>());
        // static__assert(std::is_same<std::string, get<2, short_bool_string_type>>());
        // static__assert(std::is_same<int, get<0, type_sequence<int>>>());

        // *********************************************************************
        // front: access the first type
        // *********************************************************************

        template<typename TypeSequence>
#if __cplusplus >= 202002L
            requires(!is_empty<TypeSequence>::value)
#endif
        using front_t = get<0, TypeSequence>;

        // static__assert(std::is_same<short, front_t<short_bool_string_type>>());
        // static__assert(std::is_same<int, front_t<type_sequence<int>>>());
        // calling on an empty type_sequence is undefined

        // *********************************************************************
        // back: access the last type
        // *********************************************************************

        template<typename TypeSequence>
#if __cplusplus >= 202002L
            requires(!is_empty<TypeSequence>::value)
#endif
        using back_t = get<size<TypeSequence>::value - 1, TypeSequence>;

        // static__assert(std::is_same<std::string, back_t<short_bool_string_type>>());
        // static__assert(std::is_same<int, back_t<type_sequence<int>>>());
        // calling on an empty type_sequence is undefined

        // *********************************************************************
        // split: splits before Pos
        // *********************************************************************

        template<std::size_t Pos, typename... Ts>
        struct split;

        template<std::size_t Pos, typename... Ts>
        struct split<Pos, type_sequence<Ts...>> : detail::_split_impl<Pos, type_sequence<>, type_sequence<Ts...>> {
        };

#if __cplusplus >= 202002L
        // don't linear search for the end
        template<std::size_t Pos, typename... Ts>
            requires(Pos == sizeof...(Ts))
        struct split<Pos, type_sequence<Ts...>> : detail::_split_impl<0, type_sequence<Ts...>, type_sequence<>> {
        };
#endif

        template<std::size_t Pos, typename TypeSequence>
        using split_first_t = typename split<Pos, TypeSequence>::first;

        template<std::size_t Pos, typename TypeSequence>
        using split_second_t = typename split<Pos, TypeSequence>::second;

        // static__assert(std::is_same<type_sequence<short, bool>, split_first_t<2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<std::string>, split_second_t<2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<short>, split_first_t<1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<bool, std::string>, split_second_t<1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<>, split_first_t<0, short_bool_string_type>>());
        // static__assert(std::is_same<short_bool_string_type, split_second_t<0, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<>, split_first_t<0, type_sequence<>>>());
        // static__assert(std::is_same<type_sequence<>, split_second_t<0, type_sequence<>>>());

        // *********************************************************************
        // insert: inserts types
        // *********************************************************************

        template<std::size_t Pos, typename ITypeSequence, typename TypeSequence>
        using insert_range = cat<split_first_t<Pos, TypeSequence>, ITypeSequence, split_second_t<Pos, TypeSequence>>;

        template<std::size_t Pos, typename ITypeSequence, typename TypeSequence>
        using insert_range_t = typename insert_range<Pos, ITypeSequence, TypeSequence>::type;

        template<std::size_t Pos, typename IType, std::size_t Count, typename TypeSequence>
        using insert_count = insert_range<Pos, make_homogeneous_type_sequence<Count, IType>, TypeSequence>;

        template<std::size_t Pos, typename IType, std::size_t Count, typename TypeSequence>
        using insert_count_t = typename insert_count<Pos, IType, Count, TypeSequence>::type;

        template<std::size_t Pos, typename IType, typename TypeSequence>
        using insert = insert_range<Pos, type_sequence<IType>, TypeSequence>;

        template<std::size_t Pos, typename IType, typename TypeSequence>
        using insert_t = typename insert<Pos, IType, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<short, bool, std::string, int, int>, insert_count_t<3, int, 2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<short, bool, int, int, std::string>, insert_count_t<2, int, 2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<short, int, int, bool, std::string>, insert_count_t<1, int, 2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<int, int, short, bool, std::string>, insert_count_t<0, int, 2, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<int, int>, insert_count_t<0, int, 2, type_sequence<>>>());
        // static__assert(std::is_same<type_sequence<short, bool, std::string, int>, insert_count_t<3, int, 1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<short, bool, int, std::string>, insert_count_t<2, int, 1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<short, int, bool, std::string>, insert_count_t<1, int, 1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<int, short, bool, std::string>, insert_count_t<0, int, 1, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<int>, insert_count_t<0, int, 1, type_sequence<>>>());
        // static__assert(std::is_same<short_bool_string_type, insert_count_t<3, int, 0, short_bool_string_type>>());
        // static__assert(std::is_same<short_bool_string_type, insert_count_t<2, int, 0, short_bool_string_type>>());
        // static__assert(std::is_same<short_bool_string_type, insert_count_t<1, int, 0, short_bool_string_type>>());
        // static__assert(std::is_same<short_bool_string_type, insert_count_t<0, int, 0, short_bool_string_type>>());
        // static__assert(std::is_same<type_sequence<>, insert_count_t<0, int, 0, type_sequence<>>>());

        // *********************************************************************
        // pop front
        // *********************************************************************

        template<typename... Ts>
#if __cplusplus >= 202002L
            requires(!is_empty<type_sequence<Ts...>>::value)
#endif
        struct pop_front;

        template<typename T, typename... Ts>
        struct pop_front<type_sequence<T, Ts...>> {
            using type = type_sequence<Ts...>;
        };

        template<typename TypeSequence>
        using pop_front_t = typename pop_front<TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<bool, std::string>, pop_front_t<short_bool_string_type>>());

        // *********************************************************************
        // push front
        // *********************************************************************

        template<typename T, typename TypeSequence>
        using push_front_t = typename insert<0, T, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<int, short, bool, std::string>, push_front_t<int, short_bool_string_type>>());

        template<typename ITypeSequence, typename TypeSequence>
        using prepend_range_t = typename insert_range<0, ITypeSequence, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<short, bool, int>, prepend_range_t<type_sequence<short>, type_sequence<bool, int>>>());

        // *********************************************************************
        // push back
        // *********************************************************************

        template<typename T, typename TypeSequence>
        using push_back_t = typename insert<size<TypeSequence>::value, T, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<short, bool, int>, push_back_t<int, type_sequence<short, bool>>>());

        template<typename ITypeSequence, typename TypeSequence>
        using append_range_t = typename insert_range<size<TypeSequence>::value, ITypeSequence, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<short, bool, int>, append_range_t<type_sequence<int>, type_sequence<short, bool>>>());

        // *********************************************************************
        // reverse
        // *********************************************************************

        template<typename TypeSequence>
        using reverse_t = typename detail::_reverse_impl<type_sequence<>, TypeSequence>::type;

        // static__assert(std::is_same<type_sequence<std::string, bool, short>, reverse_t<short_bool_string_type>>());

        // *********************************************************************
        // contains
        // *********************************************************************

#if __cplusplus >= 201703L
        template<typename IType, typename... Ts>
        struct contains;

        template<typename IType, typename... Ts>
        struct contains<IType, type_sequence<Ts...>> : std::disjunction<std::is_same<IType, Ts>...> {
        };

        template<typename IType, typename TypeSequence>
        inline constexpr bool contains_v = contains<IType, TypeSequence>::value;
#endif

#if __cplusplus >= 201703L
        // static__assert(contains<bool, short_bool_string_type>::value);
        // static__assert(!contains<int, short_bool_string_type>::value);
#endif

    } // namespace meta

    namespace detail {
        using meta::contains;

        // *********************************************************************
        // unique impl
        // *********************************************************************

#if __cplusplus >= 201703L
        template<bool Contains, typename... Ts>
        struct _unique_impl;

        template<typename... Ts, typename U1, typename U2, typename... Us>
        struct _unique_impl<false, type_sequence<Ts...>, type_sequence<U1, U2, Us...>> : _unique_impl<contains<U2, type_sequence<U1, Ts...>>::value, type_sequence<U1, Ts...>, type_sequence<U2, Us...>> {
        };

        template<typename... Ts, typename U1, typename U2, typename... Us>
        struct _unique_impl<true, type_sequence<Ts...>, type_sequence<U1, U2, Us...>> : _unique_impl<contains<U2, type_sequence<Ts...>>::value, type_sequence<Ts...>, type_sequence<U2, Us...>> {
        };

        template<typename... Ts, typename U>
        struct _unique_impl<false, type_sequence<Ts...>, type_sequence<U>> {
            using type = type_sequence<Ts..., U>;
        };

        template<typename... Ts, typename U>
        struct _unique_impl<true, type_sequence<Ts...>, type_sequence<U>> {
            using type = type_sequence<Ts...>;
        };

        template<>
        struct _unique_impl<false, type_sequence<>, type_sequence<>> {
            using type = type_sequence<>;
        };
#endif
    } // namespace detail

    namespace meta {
        // *********************************************************************
        // unique
        // *********************************************************************

#if __cplusplus >= 201703L
        template<typename... Ts>
        struct unique;

        template<typename... Us>
        struct unique<type_sequence<Us...>> : detail::_unique_impl<false, type_sequence<>, type_sequence<Us...>> {
        };

        template<typename TypeSequence>
        using unique_t = typename unique<TypeSequence>::type;
#endif

#if __cplusplus >= 201703L
        // static__assert(std::is_same<type_sequence<int, bool>, unique_t<type_sequence<int, int, bool>>>());
        // static__assert(std::is_same<type_sequence<int>, unique_t<type_sequence<int, int>>>());
        // static__assert(std::is_same<type_sequence<int, bool>, unique_t<type_sequence<int, bool>>>());
        // static__assert(std::is_same<type_sequence<int>, unique_t<type_sequence<int>>>());
        // static__assert(std::is_same<type_sequence<>, unique_t<type_sequence<>>>());
        // static__assert(std::is_same<type_sequence<short, bool, std::string>, short_bool_string_type>());
#endif
    } // namespace meta
} // namespace hyx

#endif // !HYX_TYPE_SEQUENCE_H
