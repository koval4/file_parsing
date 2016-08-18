#ifndef FILEREADER_H
#define FILEREADER_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <array>
#include <string>
#include <fstream>
#include <algorithm>
#include <functional>
#include <tuple>
#include <utility>
#include <boost/variant.hpp>

template <typename F, typename T, size_t... index>
decltype(auto) apply_impl(F&& func, T&& tuple, std::index_sequence<index...>) {
    return func(std::get<index>(std::forward<T>(tuple))...);
}

template <typename F, typename T>
decltype(auto) apply(F&& func, T&& tuple) {
    constexpr auto size = std::tuple_size<std::decay_t<T> >::value;
    return apply_impl(
        std::move(func),
        std::move(tuple),
        std::make_index_sequence<size>()
    );
}

template <typename F, typename T, size_t... index>
decltype(auto) for_each_impl(T&& tuple, F&& fn, std::index_sequence<index...>) {
    return std::make_tuple(fn(std::get<index>(tuple))...);
}

template <typename F, typename... T>
decltype(auto) for_each(const std::tuple<T...>& tuple, F&& fn) {
    constexpr auto size = std::tuple_size<std::decay_t<decltype(tuple)> >::value;
    return for_each_impl(
        tuple,
        std::move(fn),
        std::make_index_sequence<size>()
    );
}

template <typename T>
struct converter {
    using type = T;
    std::string name;
    std::function<T(std::string&)> fn;

    converter(
        const std::string& name,
        const std::function<T(std::string&)>& fn)
        : name (name)
        , fn(fn) {}
};

template <typename T>
using converter_variant = boost::variant<T, converter<T> >;

template <typename T>
struct converter_visitor
    : public boost::static_visitor<converter_variant<T> > {
    std::string name;
    std::string& source;

    converter_visitor(
        const std::string& name,
        std::string& source)
        : name(name)
        , source(source) {}

    converter_variant<T> operator()(const T& obj) const {
        return converter_variant<T>{obj};
    }

    converter_variant<T> operator()(const converter<T>& converter) {
        if (converter.name == name)
            return converter.fn(source);
        else return converter;
    }
};

template <typename T>
struct unwrap_visitor
    : public boost::static_visitor<T> {
    T operator()(const T& obj) const {
        return obj;
    }

    T operator()(const converter<T>&) const {
        // TODO: add nice exception derived from std::exception
        throw "Cannot unwrap this shit";
    }
};

template <typename... Types>
decltype(auto) build_visitors(
    const std::string& name,
    std::string& source,
    const std::tuple<converter_variant<Types>... >&) {
    return std::make_tuple(converter_visitor<Types>{name, source}...);
}

template <typename... Types, size_t... index>
decltype(auto) apply_visitors_impl(
    const std::tuple<converter_variant<Types>... >& converters,
    std::tuple<converter_visitor<Types>...>& visitors,
    std::index_sequence<index...>) {
    return std::make_tuple(boost::apply_visitor(std::get<index>(visitors), std::get<index>(converters))...);
}

template <typename... Types>
decltype(auto) apply_visitors(
    const std::tuple<converter_variant<Types>... >& converters,
    std::tuple<converter_visitor<Types>...>& visitors) {
    constexpr auto converters_size = std::tuple_size<std::decay_t<decltype(converters)> >::value;
    constexpr auto visitors_size = std::tuple_size<std::decay_t<decltype(visitors)> >::value;
    static_assert(converters_size == visitors_size, "converters size doesn't matches visitors size!");
    return apply_visitors_impl(converters, visitors, std::make_index_sequence<converters_size>());
}

template <typename... Types>
decltype(auto) wrap_variant(
    const std::tuple<converter<Types>...>& converters) {
    return for_each(converters, [] (auto converter) {
            return converter_variant<typename decltype(converter)::type>{converter};
        });
}

template <typename... Types>
decltype(auto) build_unwrap_visitors(
    const std::tuple<converter_variant<Types>...>&) {
    return std::make_tuple(unwrap_visitor<Types>{}...);
}

template <typename... Types, size_t... index>
decltype(auto) apply_unwrap_impl(
    const std::tuple<converter_variant<Types>...>& converters,
    const std::tuple<unwrap_visitor<Types>...>& visitors,
    std::index_sequence<index...>) {
    return std::make_tuple(boost::apply_visitor(std::get<index>(visitors), std::get<index>(converters))...);
}

template <typename... Types>
decltype(auto) apply_unwrap(
    const std::tuple<converter_variant<Types>...>& converters,
    const std::tuple<unwrap_visitor<Types>...>& visitors) {
    constexpr auto converters_size = std::tuple_size<std::decay_t<decltype(converters)> >::value;
    constexpr auto visitors_size = std::tuple_size<std::decay_t<decltype(visitors)> >::value;
    static_assert(converters_size == visitors_size, "converters size doesn't matches visitors size!");
    return apply_unwrap_impl(converters, visitors, std::make_index_sequence<converters_size>());
}

template <typename... Types, size_t... index>
decltype(auto) unwrap_variant_impl(
    const std::tuple<converter_variant<Types>... >& converters,
    std::index_sequence<index...>) {
    return std::make_tuple(boost::get<Types>(std::get<index>(converters))...);
}

template <typename... Types>
decltype(auto) unwrap_variant(
    const std::tuple<converter_variant<Types>... >& converters) {
    constexpr auto size = std::tuple_size<std::decay_t<decltype(converters)> >::value;
    //return unwrap_variant_impl(converters, std::make_index_sequence<size>());
    //return for_each(converters, [] (auto converter) { return boost::get<0>(converter); });
    auto visitors = build_unwrap_visitors(converters);
    return apply_unwrap(converters, visitors);
}

template <typename... Types>
decltype(auto) read_object_impl(
    std::tuple<converter_variant<Types>...>&& converters,
    std::string& source) {
    if (source.empty())
        return std::move(converters);
    else {
        auto assign_pos = source.find_first_of('=');
        // TODO: add name trimming
        auto name = source.substr(0, assign_pos);
        name = name.substr(name.find_first_not_of(" \t\r\n"));
        name = name.substr(0, name.find_first_of(" \t\r\n"));
        source.erase(0, assign_pos + 1);
        auto visitors = build_visitors(name, source, converters);
        return read_object_impl(apply_visitors(converters, visitors), source);
    }
}

template <typename T, typename Factory, typename... Args>
T read_object(
    std::tuple<converter<Args>... >&& properties,
    Factory&& factory,
    std::string& source) {
    return apply(std::move(factory), unwrap_variant(read_object_impl(
        wrap_variant(properties),
        source
    )));
}

/*
 * new {
 *     name = some_name,
 *     number = 42,
 *     list_of_numbers = {1, 2, 3}
 * }
 *
 * read_object(std::make_tuple(
 *  converter<std::string>{"name", read_string},
 *  converter<int>{"number", read_int},
 *  converter<std::vector<int>>{"list_of_numbers", std::bind(read_list, read_int, _1)}),
 *  source);
 *
 */

#endif
