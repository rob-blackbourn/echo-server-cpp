#ifndef __utils_hpp
#define __utils_hpp

#include <deque>
#include <iostream>
#include <set>
#include <vector>

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::deque<T>& collection)
{
    os << "(";

    for (auto item = collection.begin(); item != collection.end(); ++item)
    {
        if (item != collection.begin())
            os << ",";
        os << *item;
    }
    os << ")";

    return os;
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::set<T>& collection)
{
    os << "{";

    for (auto item = collection.begin(); item != collection.end(); ++item)
    {
        if (item != collection.begin())
            os << ",";
        os << *item;
    }
    os << "}";

    return os;
}

template<typename T>
inline std::ostream& operator << (std::ostream& os, const std::vector<T>& collection)
{
    os << "[";

    for (auto item = collection.begin(); item != collection.end(); ++item)
    {
        if (item != collection.begin())
            os << ",";
        os << *item;
    }
    os << "]";

    return os;
}

inline std::ostream& operator << (std::ostream& os, bool b)
{
    return os << (b ? "<true>" : "<false>");
}

#endif // __utils_hpp