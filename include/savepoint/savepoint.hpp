#pragma once

#include <savepoint/fwd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @brief The log function signature.
 * 
 * @param string The log message.
 */
using SavepointLogFunction = std::function<void(std::string_view string)>;

/**
 * @brief Set the log function used by SavepointLog. Defaults to stderr.
 * 
 * @param function The log function.
 */
void SavepointSetLogFunction(const SavepointLogFunction& function);

/**
 * @brief Invoke the currently set log function.
 * 
 * @param string The log message.
 */
void SavepointLog(std::string_view string);

/**
 * @brief Used to specify a Savepoint version.
 * 
 * For versioning members and performing quick comparisons, a version wrapper
 * is provided. The version consists of a major, minor, and patch version (with
 * decreasing significance). Versions are packed into a u32 so comparisons are
 * cheap. Versions can also be assigned and compared at compile time.
 * 
 * @snippet examples/9_version.cpp 9_version
 * @see Savepoint
 */
class SavepointVersion
{
public:
    /**
     * @brief Default initializes the version to 0.0.0.
     */
    constexpr SavepointVersion()
        : Value{0}
    {
    }

    /**
     * @brief Initialize the version to major.minor.patch.
     * 
     * @param major The major version.
     * @param minor The minor version.
     * @param patch The patch version.
     */
    constexpr SavepointVersion(uint32_t major, uint32_t minor, uint32_t patch)
        : Value{major << 24 | minor << 16 | patch}
    {
    }

    /**
     * @brief Get the major version.
     * 
     * @return The major version.
     */
    constexpr uint32_t GetMajor() const
    {
        return (Value >> 24) & 0xFF;
    }

    /**
     * @brief Get the minor version.
     * 
     * @return The minor version.
     */
    constexpr uint32_t GetMinor() const
    {
        return (Value >> 16) & 0xFF;
    }

    /**
     * @brief Get the patch version.
     * 
     * @return The patch version.
     */
    constexpr uint32_t GetPatch() const
    {
        return Value & 0xFFFF;
    }

    /**
     * @brief Get the version as a string in the format major.minor.patch.
     * 
     * @return The version as a string.
     */
    std::string GetString() const
    {
        return std::format("{}.{}.{}", GetMajor(), GetMinor(), GetPatch());
    }

    /**
     * @brief Compare the version to another version.
     * 
     * @param other The other version.
     * @return True if the comparison evaluated to true.
     */
    constexpr auto operator<=>(const SavepointVersion& other) const = default;

private:
    uint32_t Value;
};

/**
 * @brief The current savepoint version.
 */
static constexpr SavepointVersion kSavepointVersion{0, 0, 0};

/**
 * @brief Used to uniquely identify a Savepoint entry.
 * 
 * @see SavepointEntity
 */
class SavepointID
{
    friend class Savepoint;

    SavepointID(int value)
        : Value{value}
    {
    }

public:
    /**
     * @brief Reserved value for an invalid ID.
     */
    static constexpr int kInvalidID = std::numeric_limits<int>::max();

    /**
     * @brief Default initialize an invalid ID.
     */
    constexpr SavepointID()
        : Value{kInvalidID}
    {
    }

    /**
     * @brief Compare the ID to another ID.
     * 
     * @param other The other ID.
     * @return True if the comparison evaluated to true.
     */
    constexpr auto operator<=>(const SavepointID& other) const = default;

    /**
     * @brief Check if an ID is valid.
     * 
     * @return True if the ID is valid.
     */
    constexpr bool IsValid() const
    {
        return Value != SavepointID{}.Value;
    }

    /**
     * @brief Get the raw ID value.
     *
     * @return The raw ID value.
     */
    constexpr int GetValue() const
    {
        return Value;
    }

private:
    int Value;
};

namespace std
{

/**
 * @brief Hash implementation for a SavepointID.
 */
template <>
struct hash<SavepointID>
{
    /**
     * @brief Hash a SavepointID.
     * 
     * @param id The ID.
     * @return The hash.
     */
    size_t operator()(const SavepointID& id) const noexcept
    {
        return std::hash<uint32_t>{}(id.GetValue());
    }
};

}

/**
 * @brief Used to uniquely identify a Savepoint entry.
 * 
 * For objects that don't have unique locations, a base class is provided to
 * ensure the object gets a unique entry. When users write their object to the
 * Savepoint, Savepoint will use the base class to insert or update an entry.
 * Users should not modify the base class themselves.
 * 
 * @snippet examples/2_basic_usage.cpp 2_basic_usage
 * @see Savepoint
 * @see SavepointID
 */
class SavepointEntity
{
    friend class Savepoint;

public:
    /**
     * @brief Get the unique entity ID.
     * 
     * A limitation of Savepoint is that you can't serialize references to other entities.
     * Instead of a reference, it will create a copy and unassociate the reference when
     * deserialized. If you need to maintain references, you can serialize the ID instead.
     * 
     * @see SavepointID
     * @return The unique ID.
     */
    SavepointID GetID() const
    {
        return ID;
    }

private:
    SavepointID ID;
};

/**
 * @brief Serves as the base class for the user's base class.
 * 
 * To support polymorphic types, Savepoint offers a base class you can use.
 * Savepoint checks if visited objects inherit from SavepointPoly and if so, 
 * serializes the object alongside its type information. When Savepoint reads
 * the type information out, it knows to instantiate the derived class.
 * 
 * @snippet examples/8_polymorphic_types.cpp 8_polymorphic_types
 * @see SAVEPOINT_POLY
 */
class SavepointPoly
{
public:
    /**
     * @brief Default destructor.
     */
    virtual ~SavepointPoly() = default;

    /**
     * @brief The Visit method to be called from SavepointVisitor.
     * 
     * @param visitor The visitor.
     * @see SavepointVisitor
     */
    virtual void Visit(SavepointVisitor& visitor) {}

    /**
     * @brief Get the class name string of the underlying object.
     * 
     * @return The class name string.
     */
    virtual std::string_view GetClassName() const = 0;
};

/**
 * @brief Helper for concrete derived classes to implement SavepointPoly methods.
 * 
 * Implements GetClassName and automatically registers a factory function
 * for the derived class. It allows a SavepointVisitor to to create an instance of
 * the derived class whilst only knowing its class name.
 * 
 * @param T The class type.
 * @see SavepointPoly
 */
#define SAVEPOINT_POLY(T) \
    private: \
        struct SavepointRegistrar \
        { \
            static SavepointPoly* Function() \
            { \
                return new T(); \
            } \
            SavepointRegistrar() \
            { \
                SavepointAddPolyFunction(#T, Function); \
            } \
        }; \
        static inline SavepointRegistrar SavepointRegistrar; \
    public: \
        std::string_view GetClassName() const override \
        { \
            return #T; \
        } \

/** @cond INTERNAL */

// Poly factory function signature
using SavepointPolyFunction = SavepointPoly*(*)();

// Register a factory function for poly types
void SavepointAddPolyFunction(std::string_view string, const SavepointPolyFunction function);

// Get a factory function for poly types
SavepointPolyFunction SavepointGetPolyFunction(std::string_view string);

// Read the visitor to get the poly factory and create the poly instance
SavepointPoly* SavepointReadPoly(SavepointVisitor& visitor);

// Write the poly instance to the visitor
void SavepointWritePoly(SavepointPoly* poly, SavepointVisitor& visitor);

// Type-erased function for populating a visitor's debug information
using SavepointDebugFunction = void(*)(SavepointVisitor& visitor);

// Register a debug function. Does nothing for ID collisions
void SavepointAddDebugFunction(uint32_t id, std::string_view name, const SavepointDebugFunction function);

// Get a debug function registered to an ID
SavepointDebugFunction SavepointGetDebugFunction(uint32_t id);

// Get the type name registered to an ID
std::string_view SavepointGetDebugTypeName(uint32_t id);

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

template<typename T, typename Poly>
concept SavepointIsPolyPointer = SavepointIsStdPointer<T> && std::is_base_of_v<Poly, typename T::element_type>;

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
concept SavepointIsVisitable = !std::is_same_v<T, SavepointVisitor> && !std::is_base_of_v<SavepointPoly, T>;
template<typename T>
concept SavepointHasFreeVisit = requires(SavepointVisitor visitor, T item) { { Visit(visitor, item) }; };
template<typename T>
concept SavepointHasMemberVisit = requires(SavepointVisitor visitor, T item) { { item.Visit(visitor) }; };

// std::memcpy
template<typename T>
concept SavepointIsCopyable = !SavepointIsPointer<T> && !SavepointHasFreeVisit<T> && !SavepointHasMemberVisit<T> && std::is_trivially_copyable_v<T>;
template<typename T>
concept SavepointIsCopyableRange = std::ranges::contiguous_range<T> && SavepointIsCopyable<std::remove_const_t<std::ranges::range_value_t<T>>>;

template<typename T>
concept SavepointIsEntity = std::is_base_of_v<SavepointEntity, T> || SavepointIsPolyPointer<T, SavepointEntity>;

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

/**
 * @brief Register a type so that Savepoint can deserialize it for debugging.
 * 
 * Savepoint works by forcing the user to provide a type for reads/writes. Since the
 * schema is built directly into the type, we need that type to visualize the contents of
 * the database (unless you wanted to read binary). As such, you must (currently) register
 * your types. 
 * 
 * The type's name is hashed and stored in the visitor as a key. If the type's name changes,
 * you can keep it backwards compatible by overriding SavepointTypeName. 
 *
 * template<>
 * struct SavepointTypeName<PlayerButWithACharacterSuffix>
 * {
 *     static constexpr std::string_view kValue = "Player";
 * };
 *
 * @todo Delete when C++26 reflection is supported
 * @param T The class type. Must be default constructible.
 */
#define SAVEPOINT_TYPE(T) \
    static const bool k##T##SavepointTypeRegistrar = [] \
    { \
        SavepointAddDebugFunction(SavepointTypeID<T>(), SavepointTypeName<T>::kValue, [](SavepointVisitor& visitor) \
        { \
            T item{}; \
            visitor(item); \
        }); \
        return true; \
    }(); \

#ifdef SAVEPOINT_DEBUGGER

/**
 * @brief Debug representation of a SavepointVisitor::Visit.
 */
class SavepointDebugNode
{
    friend class SavepointVisitor;

public:
    SavepointDebugNode()
        : Depth{0}
    {
    }

    /**
     * @brief Get how deeply nested the node is (zero at the root). Useful for indentation
     *
     * @return The depth.
     */
    int GetDepth() const
    {
        return Depth;
    }

    /**
     * @brief Get the node's type name. 
     *
     * @return The type name.
     */
    std::string_view GetTypeName() const
    {
        return Type;
    }

    /**
     * @brief Get the node's value. Empty if the node is a leaf.
     *
     * @return The value.
     */
    const std::string& GetValue() const
    {
        return Value;
    }

    /**
     * @brief Check if a node has no children.
     *
     * @return True if the node has no children.
     */
    bool GetIsLeaf() const
    {
        return !Value.empty();
    }

private:
    int Depth;
    std::string Type;
    std::string Value;
};

#endif

/**
 * @brief Implementation of the Visitor pattern for serialization.
 *
 * The visitor is used to serialize objects. It uses a simplified version of the
 * [pattern](https://refactoring.guru/design-patterns/visitor) with two operating modes:
 * 1. Reading from the Savepoint.
 * 2. Writing to the Savepoint.
 * 
 * Visitors are a structured blob of binary data. They consist of:
 * 1. A SavepointVersion representing the user's application build version.
 * 2. A SavepointVersion representing the Savepoint build version (reserved for future use).
 * 3. A u32 type hash identifying the type of the object.
 * 4. The object data.
 *
 * When writing, we store the current build versions. When reading, we load the
 * versions used in the previous write. By comparing these versions to the build
 * versions, we can determine what members are safe to deserialize.
 * 
 * @snippet examples/4_nested_types.cpp 4_nested_types
 */
class SavepointVisitor
{
private:
    // Pooling to allow thread safety without having to reallocate the writers
    struct VisitorState
    {
        VisitorState()
        {
            Clear();
        }

        void Clear()
        {
            Version = {};
            TypeID = 0;
            Error = false;
            Writer.clear();
            Reader = {};
            Offset = 0;
            DebugClear();
        }

        void DebugClear()
        {
#ifdef SAVEPOINT_DEBUGGER
            Debug.clear();
            Depth = 0;
#endif
        }

        SavepointVersion Version;
        uint32_t TypeID;
        bool Error;
        std::vector<uint8_t> Writer;
        std::span<uint8_t> Reader;
        int Offset;
#ifdef SAVEPOINT_DEBUGGER
        std::vector<SavepointDebugNode> Debug;
        int Depth;
#endif
    };

public:
    /**
     * @brief Default initializes the visitor.
     */
    SavepointVisitor()
    {
        if (!States.empty())
        {
            State = std::move(States.back());
            States.pop_back();
        }
    }

    /**
     * @brief Returns the visitor state to the current thread's cache.
     */
    ~SavepointVisitor()
    {
        State.Clear();
        States.push_back(std::move(State));
    }

    SavepointVisitor(const SavepointVisitor& other) = delete;
    SavepointVisitor& operator=(const SavepointVisitor& other) = delete;
    SavepointVisitor(SavepointVisitor&& other) = delete;
    SavepointVisitor& operator=(SavepointVisitor&& other) = delete;

    /**
     * @brief Prepare a visitor for writing to bytes.
     *
     * @tparam T The type about to be written.
     * @param version The version of the application to be written.
     * @see SavepointTypeID
     */
    template<typename T>
    void Begin(SavepointVersion version)
    {
        State.Version = version;
        State.TypeID = SavepointTypeID<T>();
        State.Error = false;
        State.Writer.resize(sizeof(version) * 2 + sizeof(State.TypeID));
        State.Reader = {};
        State.Offset = 0;
        std::memcpy(State.Writer.data(), &version, sizeof(version));
        std::memcpy(State.Writer.data() + sizeof(version), &kSavepointVersion, sizeof(version));
        std::memcpy(State.Writer.data() + sizeof(version) * 2, &State.TypeID, sizeof(State.TypeID));
        State.DebugClear();
    }

    /**
     * @brief Prepare a visitor for reading from bytes.
     *
     * @param data The data as bytes.
     * @param size The number of bytes.
     */
    void Begin(const void* data, int size)
    {
        State.Version = SavepointVersion{};
        State.TypeID = 0;
        State.Error = false;
        State.Reader = {static_cast<uint8_t*>(const_cast<void*>(data)), size_t(size)};
        State.Writer.clear();
        State.Offset = 0;
        operator()(State.Version);
        Skip<SavepointVersion>();
        operator()(State.TypeID);
        State.DebugClear();
    }

    /**
     * @brief Serialize to/from an item's raw bytes.
     * 
     * Perform a simple memcpy from the visitor to the item's raw bytes (and
     * vice versa) using the size of the item. If the item cannot be deserialized,
     * it will be default initialized using args, assuming args are provided.
     * 
     * @tparam T The type to serialize.
     * @tparam Args The types of the args.
     * @param item The item to serialize.
     * @param version The version required to deserialize.
     * @param args The args for default initialization.
     */
    template<SavepointIsCopyable T, typename... Args>
    void operator()(T& item, SavepointVersion version = {}, Args&&... args)
    {
        if (IsReading())
        {
            // Required for write-only containers (e.g. views)
            if constexpr (std::is_const_v<T>)
            {
                SavepointLog("Tried to read into a const");
                SetError();
                return;
            }
            else
            {
                if (HasError() || State.Version < version)
                {
                    if constexpr (sizeof...(Args) > 0)
                    {
                        item = T{std::forward<Args>(args)...};
                    }
                    return;
                }
                if (sizeof(T) > GetSize())
                {
                    SavepointLog(std::format("Tried to read past visitor: {} -> {}", State.Version.GetString(), version.GetString()));
                    SetError();
                    if constexpr (sizeof...(Args) > 0)
                    {
                        item = T{std::forward<Args>(args)...};
                    }
                    return;
                }
                std::memcpy(std::addressof(item), State.Reader.data() + State.Offset, sizeof(T));
                State.Offset += sizeof(T);
            }
        }
        else
        {
            if (HasError())
            {
                return;
            }
            State.Writer.resize(State.Writer.size() + sizeof(T));
            std::memcpy(State.Writer.data() + State.Writer.size() - sizeof(T), std::addressof(item), sizeof(T));
        }
        DebugLeaf(item);
    }

private:
    template<typename T, typename... Args>
    bool TryVisit(T& item, SavepointVersion version = {}, Args&&... args)
    {
        if (IsReading())
        {
            if (HasError() || State.Version < version)
            {
                if constexpr (sizeof...(Args) > 0)
                {
                    item = T{std::forward<Args>(args)...};
                }
                return false;
            }
        }
        return !HasError();
    }

public:
    /**
     * @brief Visit using the implementation from Visit.
     *
     * If the item cannot be deserialized, it will be default initialized using
     * args, assuming args are provided.
     * 
     * @tparam T The type to serialize.
     * @tparam Args The types of the args.
     * @param item The item to serialize.
     * @param version The version required to deserialize.
     * @param args The args for default initialization.
     */
    template<SavepointHasFreeVisit T, typename... Args>
    void operator()(T& item, SavepointVersion version = {}, Args&&... args)
    {
        if (TryVisit(item, version, std::forward<Args>(args)...))
        {
            DebugPush(item);
            Visit(*this, item);
            DebugPop();
        }
    }

    /**
     * @brief Visit using the implementation from T::Visit.
     * 
     * If the item cannot be deserialized, it will be default initialized using
     * args, assuming args are provided.
     * 
     * @tparam T The type to serialize.
     * @tparam Args The types of the args.
     * @param item The item to serialize.
     * @param version The version required to deserialize.
     * @param args The args for default initialization.
     */
    template<SavepointHasMemberVisit T, typename... Args>
    void operator()(T& item, SavepointVersion version = {}, Args&&... args)
    {
        if (TryVisit(item, version, std::forward<Args>(args)...))
        {
            DebugPush(item);
            item.Visit(*this);
            DebugPop();
        }
    }

    /**
     * @brief Visit a contiguous block of copyable elements.
     *
     * @tparam T The element type.
     * @param data The pointer to the elements.
     * @param size The number of elements.
     */
    template<SavepointIsCopyable T>
    void operator()(T* data, int size)
    {
        if (HasError())
        {
            return;
        }
        int bytes = size * sizeof(T);
        if (IsReading())
        {
            // Required for write-only containers (e.g. views)
            if constexpr (std::is_const_v<T>)
            {
                SavepointLog("Tried to read into a const");
                SetError();
                return;
            }
            else
            {
                if (bytes > GetSize())
                {
                    SavepointLog(std::format("Tried to read past visitor: {}", State.Version.GetString()));
                    SetError();
                    return;
                }
                if (bytes)
                {
                    std::memcpy(data, State.Reader.data() + State.Offset, bytes);
                    State.Offset += bytes;
                }
            }
        }
        else
        {
            if (bytes)
            {
                State.Writer.resize(State.Writer.size() + bytes);
                std::memcpy(State.Writer.data() + State.Writer.size() - bytes, data, bytes);
            }
        }
#ifdef SAVEPOINT_DEBUGGER
        if constexpr (std::is_same_v<std::remove_cv_t<T>, char>)
        {
            DebugLeaf(std::string_view{data, size_t(size)});
        }
        else
        {
            for (int i = 0; i < size; i++)
            {
                DebugLeaf(data[i]);
            }
        }
#endif
    }

    /**
     * @brief Skip bytes.
     *
     * @tparam T The type to skip.
     */
    template<SavepointIsCopyable T>
    void Skip()
    {
        if (HasError())
        {
            return;
        }
        if (IsReading())
        {
            if (sizeof(T) > GetSize())
            {
                SavepointLog(std::format("Tried to skip past visitor: {}", State.Version.GetString()));
                SetError();
                return;
            }
            State.Offset += sizeof(T);
        }
        else
        {
            State.Writer.resize(State.Writer.size() + sizeof(T));
        }
    }

    /**
     * @brief Disable reading and writing.
     * 
     * If a serialization error is detected, you can use SetError to disable a read or write on the Savepoint.
     * 
     * @see HasError
     * @snippet examples/11_error_handling.cpp 11_error_handling
     */
    void SetError()
    {
        State.Error = true;
    }

    /**
     * @brief Check if an error is set.
     * 
     * @return True if an error is set.
     * @see SetError
     */
    bool HasError() const
    {
        return State.Error;
    }

    /**
     * @brief Check if a visitor is reading.
     * 
     * @return True if the visitor is reading.
     */
    bool IsReading() const
    {
        return !State.Reader.empty();
    }

    /**
     * @brief Check if a visitor is writing.
     * 
     * @return True if the visitor is writing.
     */
    bool IsWriting() const
    {
        return !IsReading();
    }

    /**
     * @brief Get the application version.
     * 
     * @return The application version.
     */
    SavepointVersion GetVersion() const
    {
        return State.Version;
    }

    /**
     * @brief Get the type ID of the item held by the visitor.
     *
     * Zero when reading a Savepoint written before kSavepointTypeIDVersion.
     *
     * @return The type ID.
     * @see SavepointTypeID
     */
    uint32_t GetTypeID() const
    {
        return State.TypeID;
    }

#ifdef SAVEPOINT_DEBUGGER

     /**
      * @brief Deserialize the visitor by using the visitor's type hash.
      * 
      * Uses the visitor's type hash deserialize the visitor using the root type.
      * Requires the user to have called SAVEPOINT_TYPE on the stored type.
      * 
      * @return The debug nodes.
     * @see SAVEPOINT_TYPE
      */
    const std::vector<SavepointDebugNode>& GetDebugNodes()
    {
        if (SavepointDebugFunction function = SavepointGetDebugFunction(State.TypeID))
        {
            function(*this);
        }
        else
        {
            SavepointDebugNode node;
            node.Type = std::format("Failed to parse debug information, missing SAVEPOINT_TYPE. {} bytes", State.TypeID, GetSize());
            node.Value = "<unknown>";
            State.Debug.push_back(std::move(node));
        }
        return State.Debug;
    }

#endif

    /**
     * @brief Add a non-leaf node to the debug nodes. Adds 1 level of nesting.
     * 
     * @tparam T The type to add.
     * @param item The item to add.
     */
    template<typename T>
    void DebugPush(T& item)
    {
#ifdef SAVEPOINT_DEBUGGER
        SavepointDebugNode node;
        node.Depth = State.Depth;
        if constexpr (std::is_base_of_v<SavepointPoly, T>)
        {
            node.Type = item.GetClassName();
        }
        else
        {
            node.Type = SavepointTypeName<T>::kValue;
        }
        State.Debug.push_back(std::move(node));
        State.Depth++;
#endif
    }

    /**
     * @brief Remove 1 level of nesting from the debug nodes.
     */
    void DebugPop()
    {
#ifdef SAVEPOINT_DEBUGGER
        State.Depth--;
#endif
    }

    /**
     * @brief Add a leaf node to the debug nodes.
     *
     * @tparam T The type to add.
     * @param item The item to add.
     */
    template<typename T>
    void DebugLeaf(const T& item)
    {
#ifdef SAVEPOINT_DEBUGGER
        SavepointDebugNode node;
        node.Depth = State.Depth;
        node.Type = SavepointTypeName<T>::kValue;
        if constexpr (std::is_convertible_v<T, std::string_view>)
        {
            node.Value = std::format("\"{}\"", std::string_view{item});
        }
        else if constexpr (std::is_enum_v<T>)
        {
            node.Value = std::format("{}", std::to_underlying(item));
        }
        else if constexpr (std::formattable<T, char>)
        {
            node.Value = std::format("{}", item);
        }
        else
        {
            node.Value = std::format("<{} bytes>", sizeof(T));
        }
        State.Debug.push_back(std::move(node));
#endif
    }

    /**
     * @brief Get the number of bytes to write or the remaining to read.
     * 
     * @return The number of bytes.
     */
    int GetSize() const
    {
        if (HasError())
        {
            return 0;
        }
        if (IsReading())
        {
            return State.Reader.size() - std::min(State.Offset, int(State.Reader.size()));
        }
        else
        {
            return State.Writer.size();
        }
    }

    /**
     * @brief Check if a visitor has no remaining bytes.
     * 
     * @return True if no remaining bytes.
     */
    bool IsEmpty()
    {
        return GetSize() == 0;
    }

    /**
     * @brief Get the data as bytes.
     * 
     * @return The data as bytes.
     */
    const void* GetData() const
    {
        return State.Writer.data();
    }

private:
    inline static thread_local std::vector<VisitorState> States;
    VisitorState State;
};

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
 * @see SavepointPoly
 * @see SAVEPOINT_POLY
 */
template<SavepointIsStdPointer T>
void Visit(SavepointVisitor& visitor, T& item)
{
    using ValueT = typename T::element_type;
    if constexpr (std::is_polymorphic_v<ValueT>)
    {
        static_assert(std::is_base_of_v<SavepointPoly, ValueT>, "Missing SavepointPoly inheritance");
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
            if constexpr (std::is_base_of_v<SavepointPoly, ValueT>)
            {
                item.reset(dynamic_cast<ValueT*>(SavepointReadPoly(visitor)));
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
            if constexpr (std::is_base_of_v<SavepointPoly, ValueT>)
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
            if constexpr (std::is_base_of_v<SavepointPoly, ValueT>)
            {
                SavepointWritePoly(item.get(), visitor);
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

/**
 * @brief The statuses returned by Savepoint.
 */
enum class SavepointStatus
{
    Failed,   /**< Failed to open a Savepoint for any reason */
    Existing, /**< Opened an existing Savepoint */
    New,      /**< Created a new Savepoint */
};

/**
 * @brief The implementation for Savepoint's file operations.
 */
enum class SavepointDriver : uint8_t
{
    Null,    /**< Noop. */
    SQLite3, /**< Backed by sqlite3. */
};

/** @cond INTERNAL */

using SavepointReadDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadAllEntityDataFunction = std::function<void(const void* data, int size, int)>;
using SavepointReadAllTile2DDataFunction = std::function<void(const void* data, int size, int x, int y)>;
using SavepointReadAllTile3DDataFunction = std::function<void(const void* data, int size, int x, int y, int z)>;
using SavepointReadTile2DDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadTile3DDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadAllLevelsFunction = std::function<void(int level)>;

class ISavepointDriver
{
public:
    virtual ~ISavepointDriver() = default;
    virtual SavepointStatus Open(std::string_view path, bool threadSafe, int maxWait) = 0;
    virtual bool IsOpen() = 0;
    virtual void Write(const void* data, int size) = 0;
    virtual void Write(const void* data, int size, int level) = 0;
    virtual int Insert(const void* data, int size, int level) = 0;
    virtual bool Update(const void* data, int size, int id, int level) = 0;
    virtual void Write(const void* data, int size, int x, int y, int level) = 0;
    virtual void Write(const void* data, int size, int x, int y, int z, int level) = 0;
    virtual void Read(const SavepointReadDataFunction& function) = 0;
    virtual void Read(const SavepointReadDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadAllEntityDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadAllTile2DDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadAllTile3DDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y) = 0;
    virtual void Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z) = 0;
    virtual void Read(const SavepointReadAllLevelsFunction& function) = 0;
    virtual void Delete(int id) = 0;
    virtual void Close() = 0;
    virtual void Save() = 0;
    virtual void Clear() = 0;
};

/** @endcond */

/**
 * @brief The read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 */
template<typename T>
using SavepointReadFunction = std::function<void(T& item)>;

/**
 * @brief The entity read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 * @see SavepointID
 */
template<typename T>
using SavepointReadEntityFunction = std::function<void(T& item)>;

/**
 * @brief The 2D tile read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 * @param x The x location.
 * @param y The y location.
 */
template<typename T>
using SavepointReadTile2DFunction = std::function<void(T& item, int x, int y)>;

/**
 * @brief The 3D tile read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 * @param x The x location.
 * @param y The y location.
 * @param z The z location.
 */
template<typename T>
using SavepointReadTile3DFunction = std::function<void(T& item, int x, int y, int z)>;

#ifdef SAVEPOINT_DEBUGGER

/**
 * @brief The entity debug nodes function signature.
 *
 * @param debug The debug nodes.
 * @param id The entity ID.
 */
using SavepointDebugEntityFunction = std::function<void(const std::vector<SavepointDebugNode>& nodes, SavepointID id)>;

/**
 * @brief The 2D tile debug nodes function signature.
 *
 * @param debug The debug nodes.
 * @param x The x location.
 * @param y The y location.
 */
using SavepointDebugTile2DFunction = std::function<void(const std::vector<SavepointDebugNode>& nodes, int x, int y)>;

/**
 * @brief The 3D tile debug nodes function signature.
 *
 * @param debug The debug nodes.
 * @param x The x location.
 * @param y The y location.
 * @param z The z location.
 */
using SavepointDebugTile3DFunction = std::function<void(const std::vector<SavepointDebugNode>& nodes, int x, int y, int z)>;

#endif

/**
 * @brief The connection handle to a Savepoint file.
 * 
 * @snippet examples/2_basic_usage.cpp 2_basic_usage
 */
class Savepoint
{
public:
    /**
     * @brief Default initializes the connection.
     */
    Savepoint() = default;

    /**
     * @brief If connected, closes the connection.
     */
    ~Savepoint();

    /**
     * @brief Deleted copy constructor.
     */
    Savepoint(const Savepoint& other) = delete;
    
    /**
     * @brief Deleted copy assignment operator.
     */
    Savepoint& operator=(const Savepoint& other) = delete;
    
    /**
     * @brief Deleted move constructor.
     */
    Savepoint(Savepoint&& other) = delete;
    
    /**
     * @brief Deleted move assignment operator.
     */
    Savepoint& operator=(Savepoint&& other) = delete;

    /**
     * @brief Opens a new connection.
     * 
     * @param driver The driver to use for file operations.
     * @param path The path to the Savepoint file.
     * @param version The user's application version.
     * @param threadSafe True if the Savepoint instance will be used from multiple threads.
     * @param maxWait The number of milliseconds to wait when the database is busy.
     * @return The result of the attempt to open a connection.
     * @see Save
     * @see Close
     */
    SavepointStatus Open(SavepointDriver driver, std::string_view path, SavepointVersion version, bool threadSafe = false, int maxWait = 0);

    /**
     * @brief Write a singleton to the Savepoint.
     * 
     * For storing information such as date and time, the user can write a
     * singleton with the assumption that only one entry exists.
     * 
     * @tparam T The type to write.
     * @param item The item to write.
     */
    template<SavepointIsVisitable T>
    void Write(T& item)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        visitor.Begin<T>(Version);
        visitor(item);
        if (visitor.HasError())
        {
            return;
        }
        Driver->Write(visitor.GetData(), visitor.GetSize());
    }

    /**
     * @brief Write a singleton to the specified level.
     *
     * Writes an item using the level as its unique location. If an entry
     * already exists at the level, the entry will be overridden.
     *
     * @tparam T The type to write.
     * @param item The item to write.
     * @param level The level.
     * @snippet examples/15_reserved_ids.cpp 15_reserved_ids
     */
    template<SavepointIsVisitable T> requires (!SavepointIsEntity<T>)
    void Write(T& item, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        visitor.Begin<T>(Version);
        visitor(item);
        if (visitor.HasError())
        {
            return;
        }
        Driver->Write(visitor.GetData(), visitor.GetSize(), level);
    }

    /**
     * @brief Write an entity to the Savepoint.
     * 
     * For items without a unique location, an ID can be used to ensure the item
     * gets a unique entry. If the ID is invalid, the ID will be written to and
     * a new entry will be inserted. If the ID is valid, an existing entry will
     * be updated (including the level).
     * 
     * @tparam T The type to write.
     * @param item The item to write.
     * @param level The level.
     * @see SavepointEntity
     */
    template<SavepointIsEntity T>
    void Write(T& item, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        visitor.Begin<T>(Version);
        visitor(item);
        SavepointID& id = GetID(item);
        if (visitor.HasError())
        {
            if (id.IsValid())
            {
                SavepointLog(std::format("Failed to write entity: id={}, level={}", id.Value, level));
            }
            else
            {
                SavepointLog(std::format("Failed to write entity: level={}", level));
            }
            return;
        }
        if (!id.IsValid())
        {
            // Not an error. Inserting a new entry
            id.Value = Driver->Insert(visitor.GetData(), visitor.GetSize(), level);
        }
        else if (!Driver->Update(visitor.GetData(), visitor.GetSize(), id.Value, level))
        {
            // Update failed so try inserting
            id.Value = Driver->Insert(visitor.GetData(), visitor.GetSize(), level);
        }
    }

    /**
     * @brief Write a 2D tile to the Savepoint.
     * 
     * Writes a tile to a entry using a key of x, y, and level. If an entry
     * already exists, the entry will be overridden.
     * 
     * @tparam T The type to write.
     * @param item The item to write.
     * @param x The x location.
     * @param y The y location.
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Write(T& item, int x, int y, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        visitor.Begin<T>(Version);
        visitor(item);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to write tile: x={}, y={}, level={}", x, y, level));
            return;
        }
        Driver->Write(visitor.GetData(), visitor.GetSize(), x, y, level);
    }

    /**
     * @brief Write a 3D tile to the Savepoint.
     * 
     * Writes a tile to a entry using a key of x, y, z, and level. If an entry
     * already exists, the entry will be overridden.
     * 
     * @tparam T The type to write.
     * @param item The item to write.
     * @param x The x location.
     * @param y The y location.
     * @param z The z location.
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Write(T& item, int x, int y, int z, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        visitor.Begin<T>(Version);
        visitor(item);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to write tile: x={}, y={}, z={}, level={}", x, y, z, level));
            return;
        }
        Driver->Write(visitor.GetData(), visitor.GetSize(), x, y, z, level);
    }

    /**
     * @brief Read a singleton from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param item The item to read.
     * @return True if the singleton exists.
     */
    template<SavepointIsVisitable T>
    bool Read(T& item)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool exists = false;
        Driver->Read([&visitor,&item, &exists](const void* data, int size)
        {
            visitor.Begin(data, size);
            visitor(item);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            exists = true;
        });
        if (visitor.HasError())
        {
            SavepointLog("Failed to read singleton");
            return false;
        }
        return exists;
    }

    /**
     * @brief Read a singleton from the specified level.
     *
     * @tparam T The type to read.
     * @param item The item to read.
     * @param level The level.
     * @return True if the singleton exists at the level.
     * @snippet examples/15_reserved_ids.cpp 15_reserved_ids
     */
    template<SavepointIsVisitable T> requires (!SavepointIsEntity<T>)
    bool Read(T& item, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool exists = false;
        Driver->Read([&visitor,&item, &exists](const void* data, int size)
        {
            visitor.Begin(data, size);
            visitor(item);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            exists = true;
        }, level);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read singleton: level={}", level));
            return false;
        }
        return exists;
    }

    /**
     * @brief Read all entities in the specified level from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param function The function to use.
     * @param level The level.
     * @see SavepointEntity
     */
    template<SavepointIsVisitable T>
    void Read(const SavepointReadEntityFunction<T>& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor,&function, level](const void* data, int size, int id)
        {
            T item;
            visitor.Begin(data, size);
            visitor(item);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read entity: id={}, level={}", id, level));
                return;
            }
            GetID(item).Value = id;
            function(item);
        }, level);
    }

    /**
     * @brief Read all 2D tiles in the specified level from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param function The function to use.
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Read(const SavepointReadTile2DFunction<T>& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor,&function, level](const void* data, int size, int x, int y)
        {
            T item;
            visitor.Begin(data, size);
            visitor(item);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, level={}", x, y, level));
                return;
            }
            function(item, x, y);
        }, level);
    }

    /**
     * @brief Read all 3D tiles in the specified level from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param function The function to use.
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Read(const SavepointReadTile3DFunction<T>& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor,&function, level](const void* data, int size, int x, int y, int z)
        {
            T item;
            visitor.Begin(data, size);
            visitor(item);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, z={}, level={}", x, y, z, level));
                return;
            }
            function(item, x, y, z);
        }, level);
    }

    /**
     * @brief Read a 2D tile at the specified level and location from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param tile The tile to read.
     * @param x The x location.
     * @param y The y location.
     * @param level The level.
     * @return True if the tile exists.
     */
    template<SavepointIsVisitable T>
    bool Read(T& tile, int x, int y, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool exists = false;
        Driver->Read([&visitor,&tile, &exists](const void* data, int size)
        {
            visitor.Begin(data, size);
            visitor(tile);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            exists = true;
        }, level, x, y);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read tile: x={}, y={}, level={}", x, y, level));
            exists = false;
        }
        return exists;
    }

    /**
     * @brief Read a 3D tile at the specified level and location from the Savepoint.
     * 
     * @tparam T The type to read.
     * @param tile The tile to read.
     * @param x The x location.
     * @param y The y location.
     * @param z The z location.
     * @param level The level.
     * @return True if the tile exists.
     */
    template<SavepointIsVisitable T>
    bool Read(T& tile, int x, int y, int z, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool exists = false;
        Driver->Read([&visitor,&tile, &exists](const void* data, int size)
        {
            visitor.Begin(data, size);
            visitor(tile);
            if (!visitor.IsEmpty())
            {
                SavepointLog("Visitor has unread data");
                visitor.SetError();
            }
            exists = true;
        }, level, x, y, z);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read tile: x={}, y={}, z={}, level={}", x, y, z, level));
            exists = false;
        }
        return exists;
    }

    /**
     * @brief Get all the levels from the Savepoint. Duplicates are removed.
     * 
     * @return The levels.
     */
    std::vector<int> GetLevels()
    {
        if (!Driver || !Driver->IsOpen())
        {
            return {};
        }
        std::vector<int> levels;
        Driver->Read([&levels](int level)
        {
            levels.push_back(level);
        });
        return levels;
    }

    /**
     * @brief Deletes an entity from the Savepoint.
     * 
     * @tparam T The type to delete
     * @param item The item to delete.
     * @see SavepointEntity
     */
    template<SavepointIsEntity T>
    void Delete(T& item)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointID& id = GetID(item);
        if (id.IsValid())
        {
            Driver->Delete(id.Value);
            id = SavepointID{};
        }
    }
    
    /**
     * @brief Closes the connection. Does NOT call Savepoint::Save.
     * 
     * @see Save
     * @see Close
     */
    void Close();
    
    /**
     * @brief Save all pending changes.
     * 
     * Commits the current transaction and starts a new one. The next time
     * Savepoint::Open is called, it will return SavepointStatus::Existing
     * instead of SavepointStatus::New.
     * 
     * @see Open
     */
    void Save();

    /**
     * @brief Remove all entities and tiles from the Savepoint.
     */
    void Clear();

#ifdef SAVEPOINT_DEBUGGER

    /**
     * @brief Read the singleton's debug nodes.
     *
     * @param nodes The debug nodes.
     * @return True if the singleton exists.
     */
    bool ReadDebug(std::vector<SavepointDebugNode>& nodes)
    {
        return ReadDebugImpl(nodes, [this](const SavepointReadDataFunction& function)
        {
            Driver->Read(function);
        });
    }

    /**
     * @brief Read the singleton's debug nodes for a specific level.
     *
     * @param nodes The tree describing the singleton.
     * @param level The level.
     * @return True if the singleton exists.
     */
    bool ReadDebug(std::vector<SavepointDebugNode>& nodes, int level)
    {
        return ReadDebugImpl(nodes, [this, level](const SavepointReadDataFunction& function)
        {
            Driver->Read(function, level);
        });
    }

    /**
     * @brief Read all entity debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugEntityFunction& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function](const void* data, int size, int id)
        {
            visitor.Begin(data, size);
            function(visitor.GetDebugNodes(), SavepointID{id});
        }, level);
    }

    /**
     * @brief Read all 2D tile debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugTile2DFunction& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function](const void* data, int size, int x, int y)
        {
            visitor.Begin(data, size);
            function(visitor.GetDebugNodes(), x, y);
        }, level);
    }

    /**
     * @brief Read all 3D tile debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugTile3DFunction& function, int level)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function](const void* data, int size, int x, int y, int z)
        {
            visitor.Begin(data, size);
            function(visitor.GetDebugNodes(), x, y, z);
        }, level);
    }

private:
    template<typename T>
    bool ReadDebugImpl(std::vector<SavepointDebugNode>& nodes, const T& read)
    {
        if (!Driver || !Driver->IsOpen())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool exists = false;
        read([&visitor, &nodes, &exists](const void* data, int size)
        {
            visitor.Begin(data, size);
            nodes = visitor.GetDebugNodes();
            exists = true;
        });
        return exists;
    }
#endif

private:
    template<SavepointIsEntity T>
    static constexpr SavepointID& GetID(T& item)
    {
        if constexpr (SavepointIsStdPointer<T>)
        {
            return item->ID;
        }
        else
        {
            return item.ID;
        }
    }

    SavepointVersion Version;
    std::unique_ptr<ISavepointDriver> Driver;
};
