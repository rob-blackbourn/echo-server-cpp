#ifndef __utils_hpp
#define __utils_hpp

#include <deque>
#include <iostream>
#include <set>
#include <span>
#include <vector>


template <typename InputIt>
inline std::ostream& stream_out_collection(
    std::ostream& os,
    const std::string& opening_bracket,
    const std::string& closing_bracket,
    const std::string& item_separator,
    const InputIt& first,
    const InputIt& last)
{
    os << opening_bracket;

    for (auto item = first; item != last; ++item)
    {
        if (item != first)
            os << item_separator;
        os << *item;
    }
    os << closing_bracket;

    return os;
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::deque<T>& collection)
{
    return stream_out_collection(os, "(", ")", ",", collection.begin(), collection.end());
}

template<typename T>
inline std::string to_string(const std::deque<T>& collection)
{
    std::stringstream ss;
    ss << collection;
    return ss.str();
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::set<T>& collection)
{
    return stream_out_collection(os, "{", "}", ",", collection.begin(), collection.end());
}

template<typename T>
inline std::string to_string(const std::set<T>& collection)
{
    std::stringstream ss;
    ss << collection;
    return ss.str();
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::vector<T>& collection)
{
    return stream_out_collection(os, "[", "]", ",", collection.begin(), collection.end());
}

template<typename T>
inline std::string to_string(const std::vector<T>& collection)
{
    std::stringstream ss;
    ss << collection;
    return ss.str();
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::span<T>& collection)
{
    return stream_out_collection(os, "<", ">", ",", collection.begin(), collection.end());
}

template<typename T>
inline std::string to_string(const std::span<T>& collection)
{
    std::stringstream ss;
    ss << collection;
    return ss.str();
}

#endif // __utils_hpp