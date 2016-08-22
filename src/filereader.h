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
constexpr decltype(auto) for_each_impl(T&& tuple, F&& fn, std::index_sequence<index...>) {
    return std::make_tuple(fn(std::get<index>(tuple))...);
}

template <typename F, typename... T>
constexpr decltype(auto) for_each(const std::tuple<T...>& tuple, F&& fn) {
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
        if (converter.name == name) {
            auto result = converter.fn(source);
            auto separator = source.find_first_of(',');
            auto end = source.find_first_of('}');
            if (separator != std::string::npos && separator < end)
                source.erase(0, separator + 1);
            return result;
        } else return converter;
    }
};

template <typename... Types>
constexpr decltype(auto) build_visitors(
    const std::string& name,
    std::string& source,
    const std::tuple<converter_variant<Types>... >&) {
    return std::make_tuple(converter_visitor<Types>{name, source}...);
}

template <typename... Types, size_t... index>
constexpr decltype(auto) apply_visitors_impl(
    const std::tuple<converter_variant<Types>... >& converters,
    std::tuple<converter_visitor<Types>...>& visitors,
    std::index_sequence<index...>) {
    return std::make_tuple(boost::apply_visitor(std::get<index>(visitors), std::get<index>(converters))...);
}

template <typename... Types>
constexpr decltype(auto) apply_visitors(
    const std::tuple<converter_variant<Types>... >& converters,
    std::tuple<converter_visitor<Types>...>& visitors) {
    constexpr auto converters_size = std::tuple_size<std::decay_t<decltype(converters)> >::value;
    constexpr auto visitors_size = std::tuple_size<std::decay_t<decltype(visitors)> >::value;
    static_assert(converters_size == visitors_size, "converters size doesn't matches visitors size!");
    return apply_visitors_impl(converters, visitors, std::make_index_sequence<converters_size>());
}

template <typename... Types>
constexpr decltype(auto) wrap_variant(
    std::tuple<converter<Types>...> converters) {
    return for_each(converters, [] (auto converter) {
        return converter_variant<typename decltype(converter)::type>{converter};
    });
}

template <typename... Types, size_t... index>
constexpr decltype(auto) unwrap_variant_impl(
    std::tuple<converter_variant<Types>... > converters,
    std::index_sequence<index...>) {
    return std::make_tuple(boost::get<Types>(std::get<index>(converters))...);
}

template <typename... Types>
constexpr decltype(auto) unwrap_variant(
    std::tuple<converter_variant<Types>... > converters) {
    constexpr auto size = std::tuple_size<std::decay_t<decltype(converters)> >::value;
    return unwrap_variant_impl(converters, std::make_index_sequence<size>());
}

template <typename... Types>
decltype(auto) read_object_impl(
    std::string& source,
    std::tuple<converter_variant<Types>...> converters) {
    source.erase(0, source.find_first_not_of(" \r\n\t"));
    if (source.front() == '}')
        return converters;

    auto assign_pos = source.find_first_of('=');
    // TODO: add name trimming
    auto name = source.substr(0, assign_pos);
    name = name.substr(name.find_first_not_of(" \t\r\n"));
    name = name.substr(0, name.find_first_of(" \t\r\n"));
    source.erase(0, assign_pos + 1);

    auto visitors = build_visitors(name, source, converters);
    return read_object_impl(source, apply_visitors(converters, visitors));
}

template <typename T, typename Factory, typename... Args>
T read_object(
    std::string& source,
    Factory&& factory,
    std::tuple<converter<Args>... >&& properties) {
    source.erase(0, source.find_first_of('{') + 1);
    auto unwrapped = unwrap_variant(
        read_object_impl(source,
            wrap_variant(properties)));
    return apply(factory, unwrapped);
}

template <typename T>
std::vector<T> read_list(
    std::string& source,
    std::function<T(std::string&)> converter) {
    std::vector<T> elements;

    auto start = source.find_first_of('{');
    auto end = source.find_first_of('}');
    source.erase(0, start + 1);

    do {
        source.erase(0, source.find_first_not_of(" \n\t\r"));
        if (source.front() == '}')
            break;
        elements.push_back(converter(source));
        auto separator = source.find_first_of(',');
        end = source.find_first_of('}');
        if (separator != std::string::npos && separator < end)
            source.erase(0, separator + 1);
    } while (true);

    source.erase(0, end + 1);

    return elements;
}

std::string read_string(std::string& source) {
    auto start = source.find_first_of('"');
    if (start == std::string::npos)
        throw "String start not found";
    auto end = source.find_first_of('"', start + 1);
    if (end == std::string::npos)
        throw "String end not found";
    auto substr = source.substr(start + 1, end - start - 1);
    source.erase(0, end + 1);
    return substr;
}

int read_int(std::string& source) {
    auto pos = source.find_first_not_of(" \r\n\t");
    source.erase(0, pos);
    size_t end = 0;
    while (isdigit(source[end])) end++;
    auto number = std::stoi(source.substr(0, end));
    source.erase(0, end);
    return number;
}

/*
 * new {
 *     name = some_name,
 *     number = 42,
 *     list_of_numbers = {1, 2, 3}
 * }
 *
 * read_object(source, factory, std::make_tuple(
 *  converter<std::string>{"name", read_string},
 *  converter<int>{"number", read_int},
 *  converter<std::vector<int>>{"list_of_numbers", std::bind(read_list, read_int, _1)})
 *  );
 *
 */

#endif
