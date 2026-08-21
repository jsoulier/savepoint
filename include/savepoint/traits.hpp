#pragma once

#include <savepoint/entity.hpp>
#include <savepoint/fwd.hpp>
#include <savepoint/polymorph.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

/** @cond INTERNAL */

// https://stackoverflow.com/questions/4384765/whats-the-difference-between-pretty-function-function-func
template<typename T>
static constexpr std::string_view SavepointFunctionName()
{
#if defined(_MSC_VER)
    return __FUNCSIG__;
#elif defined(__clang__) || defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#else
    return {};
#endif
}

static constexpr size_t kSavepointNamePrefix = SavepointFunctionName<void>().find("void");
static constexpr size_t kSavepointNameSuffix = SavepointFunctionName<void>().size() - kSavepointNamePrefix - 4;

// Try to extract the type name from the function signature
template<typename T>
static constexpr std::string_view SavepointTypeNameOf()
{
    std::string_view name = SavepointFunctionName<T>();
    if (name.empty())
    {
        return {};
    }
    name = name.substr(kSavepointNamePrefix, name.size() - kSavepointNamePrefix - kSavepointNameSuffix);
    for (std::string_view prefix : {"class ", "struct ", "enum ", "union "})
    {
        if (name.starts_with(prefix))
        {
            name.remove_prefix(prefix.size());
            break;
        }
    }
    return name;
}

// std::unique_ptr
template<typename T>
struct SavepointIsUniquePointerImpl : std::false_type {};
template<typename T, typename Deleter>
struct SavepointIsUniquePointerImpl<std::unique_ptr<T, Deleter>> : std::true_type {};
template<typename T>
concept SavepointIsUniquePointer = SavepointIsUniquePointerImpl<T>::value;

// std::shared_ptr
template<typename T>
struct SavepointIsSharedPointerImpl : std::false_type {};
template<typename T>
struct SavepointIsSharedPointerImpl<std::shared_ptr<T>> : std::true_type {};
template<typename T>
concept SavepointIsSharedPointer = SavepointIsSharedPointerImpl<T>::value;

// std::unique_ptr/std::shared_ptr
template<typename T>
concept SavepointIsStdPointer = SavepointIsUniquePointer<T> || SavepointIsSharedPointer<T>;
template<SavepointIsStdPointer T>
void Visit(SavepointVisitor& visitor, T& item);

template<typename T>
concept SavepointIsPointer = std::is_pointer_v<T> || SavepointIsStdPointer<T>;

template<typename T, typename Polymorph>
concept SavepointIsPolymorphPointer = SavepointIsStdPointer<T> && std::is_base_of_v<Polymorph, typename T::element_type>;

// std::tuple
template<typename T>
struct SavepointIsTupleImpl : std::false_type {};
template<typename First, typename Second>
struct SavepointIsTupleImpl<std::pair<First, Second>> : std::true_type {};
template<typename... Args>
struct SavepointIsTupleImpl<std::tuple<Args...>> : std::true_type {};
template<typename T>
concept SavepointIsTuple = SavepointIsTupleImpl<T>::value;
template<SavepointIsTuple T>
void Visit(SavepointVisitor& visitor, T& item);

// std::optional
template<typename T>
struct SavepointIsOptionalImpl : std::false_type {};
template<typename T>
struct SavepointIsOptionalImpl<std::optional<T>> : std::true_type {};
template<typename T>
concept SavepointIsOptional = SavepointIsOptionalImpl<T>::value;
template<SavepointIsOptional T>
void Visit(SavepointVisitor& visitor, T& item);

// <random>
template<typename T>
concept SavepointIsRandom = std::uniform_random_bit_generator<T> && requires(std::ostream& o, std::istream& i, T& item) { o << item; i >> item; };
template<SavepointIsRandom T>
void Visit(SavepointVisitor& visitor, T& item);

// <ranges>
template<typename T>
concept SavepointIsDynamicRange = requires(T item) { item.insert(std::ranges::end(item), std::declval<typename T::value_type>()); };
template<typename T>
concept SavepointIsStaticRange = !SavepointIsDynamicRange<T> && requires(T item) { item[0] = std::declval<typename T::value_type>(); };
template<typename T>
concept SavepointIsResizableRange = requires(T item, int size) { item.resize(size); };
template<std::ranges::range T>
void Visit(SavepointVisitor& visitor, T& item);

// SavepointVisitor::Visit/Visit
template<typename T>
concept SavepointIsVisitable = !std::is_same_v<T, SavepointVisitor> && !std::is_base_of_v<SavepointPolymorph, T>;
template<typename T>
concept SavepointHasFreeVisit = requires(SavepointVisitor visitor, T item) { { Visit(visitor, item) }; };
template<typename T>
concept SavepointHasMemberVisit = requires(SavepointVisitor visitor, T item) { { item.Visit(visitor) }; };

// For ensuring compatibility across platforms
template<typename T>
struct SavepointPortableTypeConverter
{
    using Type = T;

    static Type Write(T value)
    {
        return value;
    }

    static T Read(Type value)
    {
        return value;
    }
};

// For mapping integral types to a fixed width
template<typename T>
struct SavepointFixedWidth
{
    static_assert(sizeof(T) == 0, "Missing a SavepointFixedWidth specialization");
};

template<> struct SavepointFixedWidth<bool> { using Type = uint8_t; };
template<> struct SavepointFixedWidth<char> { using Type = int8_t; };
template<> struct SavepointFixedWidth<signed char> { using Type = int8_t; };
template<> struct SavepointFixedWidth<unsigned char> { using Type = uint8_t; };
template<> struct SavepointFixedWidth<char8_t> { using Type = uint8_t; };
template<> struct SavepointFixedWidth<short> { using Type = int16_t; };
template<> struct SavepointFixedWidth<unsigned short> { using Type = uint16_t; };
template<> struct SavepointFixedWidth<char16_t> { using Type = uint16_t; };
template<> struct SavepointFixedWidth<int> { using Type = int32_t; };
template<> struct SavepointFixedWidth<unsigned int> { using Type = uint32_t; };
template<> struct SavepointFixedWidth<char32_t> { using Type = uint32_t; };
template<> struct SavepointFixedWidth<wchar_t> { using Type = uint32_t; };
template<> struct SavepointFixedWidth<long> { using Type = int64_t; };
template<> struct SavepointFixedWidth<unsigned long> { using Type = uint64_t; };
template<> struct SavepointFixedWidth<long long> { using Type = int64_t; };
template<> struct SavepointFixedWidth<unsigned long long> { using Type = uint64_t; };

// Integers to their fixed-width equivalent
template<typename T> requires std::is_integral_v<T>
struct SavepointPortableTypeConverter<T>
{
    using Type = typename SavepointFixedWidth<T>::Type;

    static Type Write(T value)
    {
        return Type(value);
    }

    static T Read(Type value)
    {
        return T(value);
    }
};

// Enums to underlying integers
template<typename T> requires std::is_enum_v<T>
struct SavepointPortableTypeConverter<T>
{
    using ConverterType = SavepointPortableTypeConverter<std::underlying_type_t<T>>;
    using Type = typename ConverterType::Type;

    static Type Write(T value)
    {
        return ConverterType::Write(std::to_underlying(value));
    }

    static T Read(Type value)
    {
        return T(ConverterType::Read(value));
    }
};

// Portable arithmetic and enum types
template<typename T>
concept SavepointIsCopyable = !SavepointIsPointer<T> && !SavepointHasFreeVisit<T> && !SavepointHasMemberVisit<T> && (std::is_arithmetic_v<T> || std::is_enum_v<T>);
template<typename T>
concept SavepointIsCopyableRange = std::ranges::contiguous_range<T> && SavepointIsCopyable<std::remove_const_t<std::ranges::range_value_t<T>>>;

template<typename T>
concept SavepointIsEntity = std::is_base_of_v<SavepointEntity, T> || SavepointIsPolymorphPointer<T, SavepointEntity>;

// Type name with optional overrides for readability
template<typename T>
struct SavepointTypeName { static constexpr std::string_view kValue = SavepointTypeNameOf<T>(); };
template<typename T> requires (!std::is_same_v<T, std::remove_cvref_t<T>>)
struct SavepointTypeName<T> { static constexpr std::string_view kValue = SavepointTypeName<T>::kValue; };
template<typename T> requires SavepointIsStdPointer<T>
struct SavepointTypeName<T> { static constexpr std::string_view kValue = SavepointTypeName<typename T::element_type>::kValue; };
template<>
struct SavepointTypeName<std::string> { static constexpr std::string_view kValue = "std::string"; };
template<>
struct SavepointTypeName<std::string_view> { static constexpr std::string_view kValue = "std::string_view"; };

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
constexpr uint32_t SavepointTypeNameHash(std::string_view name)
{
    uint32_t hash = 2166136261u;
    for (char c : name)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

// Get the type as a hash
template<typename T>
constexpr uint32_t SavepointTypeID()
{
    static constexpr std::string_view name = SavepointTypeName<T>::kValue;
    if constexpr (name.empty())
    {
        return 0;
    }
    else
    {
        static constexpr uint32_t hash = SavepointTypeNameHash(name);
        return hash;
    }
}

/** @endcond */
