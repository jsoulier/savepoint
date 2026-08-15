#pragma once

#include <savepoint/fwd.hpp>
#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
#include <savepoint/traits.hpp>
#include <savepoint/visitor.hpp>

#include <algorithm>
#include <format>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

/**
 * @brief Visit implementation for serializing std::unique_ptr and std::shared_ptr.
 * 
 * Pointers data and whether they are null are serialized. As such, pointers are
 * allowed to be nullptr and will be handled accordingly. Polymorphics are also
 * supported by storing type information alongside the aforementioned data. When
 * reading, the correct derived type will be instantiated and deserialized.
 * 
 * Raw pointers are unsupported, not because they couldn't be, but because it's
 * not needed and avoids potential pitfalls.
 * 
 * @tparam T The type of the pointer.
 * @param visitor The visitor.
 * @param item The pointer.
 * @see SavepointPolymorph
 * @see SAVEPOINT_POLYMORPH
 */
template<SavepointIsStdPointer T>
void Visit(SavepointVisitor& visitor, T& item)
{
    using ValueT = typename T::element_type;
    if constexpr (std::is_polymorphic_v<ValueT>)
    {
        static_assert(std::is_base_of_v<SavepointPolymorph, ValueT>, "Missing SavepointPolymorph inheritance");
    }
    if (visitor.IsReading())
    {
        bool hasPointer = false;
        visitor(hasPointer);
        if (!hasPointer)
        {
            if (item)
            {
                SavepointLog("Nulled an allocated pointer since visitor contained a nullptr");
                item.reset();
            }
            return;
        }
        if (!item)
        {
            if constexpr (std::is_base_of_v<SavepointPolymorph, ValueT>)
            {
                item.reset(dynamic_cast<ValueT*>(SavepointReadPolymorph(visitor)));
                return;
            }
            else if constexpr (std::is_default_constructible_v<ValueT>)
            {
                item.reset(new ValueT());
                if (!item)
                {
                    SavepointLog("Failed to allocate pointer");
                    visitor.SetError();
                    return;
                }
            }
            else
            {
                // Don't static_assert because it'll fail on already instantiated derived classes with abstract parents
                SavepointLog("No method to create pointer");
                visitor.SetError();
                return;
            }
        }
        else
        {
            // Not using the derived interface but we still need to strip away that information
            if constexpr (std::is_base_of_v<SavepointPolymorph, ValueT>)
            {
                std::string string;
                visitor(string);
            }
        }
        visitor(*item);
    }
    else
    {
        bool hasPointer = item.get() != nullptr;
        visitor(hasPointer);
        if (hasPointer)
        {
            if constexpr (std::is_base_of_v<SavepointPolymorph, ValueT>)
            {
                SavepointWritePolymorph(item.get(), visitor);
            }
            else
            {
                visitor(*item);
            }
        }
    }
}

/**
 * @brief Visit implementation for serializing an std::pair or std::tuple.
 * 
 * @tparam T The type of the tuple.
 * @param visitor The visitor.
 * @param item The tuple.
 */
template<SavepointIsTuple T>
void Visit(SavepointVisitor& visitor, T& item)
{
    std::apply([&](auto&... elems)
    {
        // Const casts required because maps use const for value_type::first_type
        (visitor(const_cast<std::remove_const_t<std::remove_reference_t<decltype(elems)>>&>(elems)), ...);
    },
    item);
}

/**
 * @brief Visit implementation for serializing an std::optional.
 * 
 * @tparam T The type of the optional.
 * @param visitor The visitor.
 * @param item The optional.
 */
template<SavepointIsOptional T>
void Visit(SavepointVisitor& visitor, T& item)
{
    if (visitor.IsReading())
    {
        bool hasValue = false;
        visitor(hasValue);
        if (!hasValue)
        {
            item.reset();
            return;
        }
        if (!item.has_value())
        {
            item.emplace();
        }
        visitor(item.value());
    }
    else
    {
        bool hasValue = item.has_value();
        visitor(hasValue);
        if (hasValue)
        {
            visitor(item.value());
        }
    }
}

/**
 * @brief Visit implementation for serializing containers.
 * 
 * @tparam T The type of the container.
 * @param visitor The visitor.
 * @param item The pointer.
 */
template<std::ranges::range T>
void Visit(SavepointVisitor& visitor, T& item)
{
    using ValueT = typename T::value_type;
    int size = item.size();
    if constexpr (SavepointIsDynamicRange<T>)
    {
        if (visitor.IsReading() && size)
        {
            item.clear();
        }
    }
    visitor(size);
    if (visitor.IsReading())
    {
        // Can detect when we read garbage and would iterate forever
        if (size > visitor.GetSize())
        {
            SavepointLog("Tried to read past visitor");
            visitor.SetError();
            return;
        }
        if constexpr (SavepointIsCopyableRange<T> && SavepointIsResizableRange<T>)
        {
            item.resize(size);
            visitor(std::ranges::data(item), size);
        }
        else if constexpr (SavepointIsDynamicRange<T>)
        {
            auto inserter = std::inserter(item, std::ranges::end(item));
            for (int i = 0; i < size; i++)
            {
                // TODO: mutable iterators
                ValueT element;
                visitor(element);
                *inserter++ = std::move(element);
            }
        }
        else if constexpr (SavepointIsStaticRange<T>)
        {
            int maxSize = std::ranges::size(item);
            if (size > maxSize)
            {
                SavepointLog(std::format("Fixed range is too small: {} < {}", maxSize, size));
            }
            if (size < maxSize)
            {
                SavepointLog(std::format("Fixed range will be truncated: {} < {}", maxSize, size));
            }
            maxSize = std::min(size, maxSize);
            if constexpr (SavepointIsCopyableRange<T>)
            {
                visitor(std::ranges::data(item), maxSize);
            }
            else
            {
                for (int i = 0; i < maxSize; i++)
                {
                    visitor(item[i]);
                }
            }
            for (; maxSize < size; maxSize++)
            {
                visitor.Skip<ValueT>();
            }
        }
        else
        {
            // Required for write-only containers (e.g. views)
            SavepointLog("Unknown range");
            visitor.SetError();
        }
    }
    else
    {
        if constexpr (SavepointIsCopyableRange<T>)
        {
            visitor(std::ranges::data(item), size);
        }
        else
        {
            for (auto& element : item)
            {
                visitor(element);
            }
        }
    }
}

/**
 * @brief Visit implementation for serializing standard random number engines.
 *
 * @tparam T The type of the random number engine.
 * @param visitor The visitor.
 * @param item The random number engine.
 * @snippet examples/14_random.cpp 14_random
 */
template<SavepointIsRandom T>
void Visit(SavepointVisitor& visitor, T& item)
{
    std::string state;
    if (visitor.IsWriting())
    {
        std::stringstream stream;
        stream << item;
        state = stream.str();
    }
    visitor(state);
    if (visitor.IsReading() && !visitor.HasError())
    {
        std::stringstream stream{state};
        if (!(stream >> item))
        {
            SavepointLog("Failed to read random number engine");
            visitor.SetError();
        }
    }
}
