#pragma once

#include <savepoint/fwd.hpp>
#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
#include <savepoint/traits.hpp>
#include <savepoint/visitor.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

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
        if constexpr (std::is_base_of_v<SavepointPolymorph, ValueT>)
        {
            std::string className;
            visitor(className);
            if (visitor.HasError())
            {
                return;
            }
            if (!item || item->GetClassName() != className)
            {
                SavepointPolymorph* polymorph = SavepointCreatePolymorph(className);
                if (!polymorph)
                {
                    visitor.SetError();
                    return;
                }
                ValueT* value = dynamic_cast<ValueT*>(polymorph);
                if (!value)
                {
                    SavepointLog(std::format("Polymorph doesn't derive from {}: {}", SavepointTypeName<ValueT>::kValue, className));
                    delete polymorph;
                    visitor.SetError();
                    return;
                }
                item.reset(value);
            }
        }
        else if (!item)
        {
            if constexpr (std::is_default_constructible_v<ValueT>)
            {
                item.reset(new ValueT());
            }
            else
            {
                // Don't static_assert because it'll fail on already instantiated derived classes with abstract parents
                SavepointLog("No method to create pointer");
                visitor.SetError();
                return;
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
                std::string_view className = item->GetClassName();
                visitor(className);
            }
            visitor(*item);
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
 * @brief Visit implementation for serializing an std::variant.
 *
 * @tparam T The type of the variant.
 * @param visitor The visitor.
 * @param item The variant.
 */
template<SavepointIsVariant T>
void Visit(SavepointVisitor& visitor, T& item)
{
    if (visitor.IsWriting() && item.valueless_by_exception())
    {
        return;
    }
    int index = item.index();
    visitor(index);
    if (visitor.HasError())
    {
        return;
    }
    if (visitor.IsReading())
    {
        if (index < 0 || index >= std::variant_size_v<T>)
        {
            SavepointLog(std::format("Variant index is out of range: {}", index));
            visitor.SetError();
            return;
        }
        if (index != item.index())
        {
            [&]<size_t... Indices>(std::index_sequence<Indices...>)
            {
                auto emplace = [&]<size_t Index>()
                {
                    if (index == Index)
                    {
                        item.template emplace<Index>();
                    }
                };
                (emplace.template operator()<Indices>(), ...);
            }(std::make_index_sequence<std::variant_size_v<T>>{});
        }
    }
    std::visit([&visitor](auto& value)
    {
        visitor(value);
    },
    item);
}

/**
 * @brief Visit implementation for serializing an std::monostate. 
 *
 * @param visitor The visitor.
 * @param item The monostate.
 */
inline void Visit(SavepointVisitor& visitor, std::monostate& item)
{
}

/**
 * @brief Visit implementation for serializing an std::chrono::duration.
 *
 * @tparam T The type of the duration.
 * @param visitor The visitor.
 * @param item The duration.
 */
template<SavepointIsDuration T>
void Visit(SavepointVisitor& visitor, T& item)
{
    int64_t count = std::chrono::duration_cast<std::chrono::nanoseconds>(item).count();
    visitor(count);
    if (visitor.IsReading() && !visitor.HasError())
    {
        item = std::chrono::duration_cast<T>(std::chrono::nanoseconds{count});
    }
}

/**
 * @brief Visit implementation for serializing an std::chrono::time_point.
 *
 * @tparam T The type of the time point.
 * @param visitor The visitor.
 * @param item The time point.
 */
template<SavepointIsTimePoint T>
void Visit(SavepointVisitor& visitor, T& item)
{
    typename T::duration duration = item.time_since_epoch();
    visitor(duration);
    if (visitor.IsReading() && !visitor.HasError())
    {
        item = T{duration};
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
    if (visitor.HasError())
    {
        return;
    }
    if (size < 0)
    {
        SavepointLog("Container size was less than zero");
        visitor.SetError();
        return;
    }
    if (visitor.IsReading())
    {
        if constexpr (SavepointIsCopyableRange<T> && SavepointIsResizableRange<T>)
        {
            if (size > visitor.GetSize())
            {
                SavepointLog("Tried to read past visitor");
                visitor.SetError();
                return;
            }
            item.resize(size);
            visitor(std::ranges::data(item), size);
        }
        else if constexpr (SavepointIsDynamicRange<T>)
        {
            auto inserter = std::inserter(item, std::ranges::end(item));
            for (int i = 0; i < size && !visitor.HasError(); i++)
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
                SavepointLog(std::format("Fixed range will be truncated: {} > {}", size, maxSize));
            }
            if (size < maxSize)
            {
                SavepointLog(std::format("Fixed range will be partially read: {} < {}", size, maxSize));
            }
            maxSize = std::min(size, maxSize);
            if constexpr (SavepointIsCopyableRange<T>)
            {
                visitor(std::ranges::data(item), maxSize);
            }
            else
            {
                for (int i = 0; i < maxSize && !visitor.HasError(); i++)
                {
                    visitor(item[i]);
                }
            }
            for (; maxSize < size && !visitor.HasError(); maxSize++)
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
            // std::vector<bool> uses a proxy instead of a reference 
            using ReferenceT = std::ranges::range_reference_t<T>;
            if constexpr (std::is_reference_v<ReferenceT>)
            {
                for (auto& element : item)
                {
                    visitor(element);
                }
            }
            else
            {
                for (auto&& element : item)
                {
                    ValueT value = element;
                    visitor(value);
                }
            }
        }
    }
}
