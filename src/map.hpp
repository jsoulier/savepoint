#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

template<typename ReturnT, typename MapT, typename KeyT, typename ProjectionT = std::identity>
static ReturnT GetOr(MapT& map, const KeyT& key, ProjectionT projection = {})
{
    auto it = map.find(key);
    if (it != map.end())
    {
        return std::invoke(projection, it->second);
    }
    else
    {
        return {};
    }
}

struct Hash
{
    using is_transparent = void;

    size_t operator()(std::string_view string) const
    {
        return std::hash<std::string_view>{}(string);
    }

    size_t operator()(const std::string& string) const
    {
        return std::hash<std::string_view>{}(string);
    }

    template<typename KeyT>
    size_t operator()(const KeyT& key) const
    {
        return std::hash<KeyT>{}(key);
    }
};

template<typename KeyT, typename ValueT>
using Map = std::unordered_map<KeyT, ValueT, Hash, std::equal_to<>>;
