#pragma once

#include <savepoint/debug.hpp>
#include <savepoint/fwd.hpp>
#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
#include <savepoint/profile.hpp>
#include <savepoint/traits.hpp>
#include <savepoint/version.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iosfwd>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @brief Flags describing the visitor data.
 */
enum class SavepointVisitorFlags : uint32_t
{
    None = 0,             /**< No flags. */
    BigEndian = 1u << 0,  /**< Written on a big-endian platform. */
    // 31 bits reserved for future use
};

/**
 * @brief Implementation of the Visitor pattern for serialization.
 *
 * The visitor is used to serialize objects. It uses a simplified version of the
 * [pattern](https://refactoring.guru/design-patterns/visitor) with two operating modes:
 * 1. Reading from the Savepoint.
 * 2. Writing to the Savepoint.
 * 
 * Visitors are a structured blob of binary data. They consist of:
 * 1. SavepointVisitorFlags describing the data.
 * 2. A SavepointVersion representing the user's application build version.
 * 3. A SavepointVersion representing the Savepoint build version (reserved for future use).
 * 4. A u32 type hash identifying the type of the object.
 * 5. The object data.
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
#ifdef SAVEPOINT_DEBUGGER
            Backtraces = 0;
#endif
        }

        void Clear()
        {
            Version = {};
            Flags = SavepointVisitorFlags::None;
            TypeID = 0;
            Error = false;
            Reader = {};
            Data.clear();
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
        SavepointVisitorFlags Flags;
        uint32_t TypeID;
        bool Error;
        std::vector<uint8_t> Data;
        std::span<uint8_t> Reader;
        int Offset;
#ifdef SAVEPOINT_DEBUGGER
        std::vector<SavepointDebugNode> Debug;
        int Depth;
        int Backtraces;
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
     * The type's name is hashed and stored as its identifier. If the C++ type name
     * changes, backwards compatibility can be preserved by specializing
     * SavepointTypeName:
     *
     * @code
     * template<>
     * struct SavepointTypeName<PlayerButWithACharacterSuffix>
     * {
     *     static constexpr std::string_view kValue = "Player";
     * };
     * @endcode
     *
     * @tparam T The type about to be written.
     * @param item The item to write.
     * @param version The version of the application to be written.
     * @see SavepointTypeID
     * @see SavepointTypeName
     */
    template<typename T>
    void Begin(T& item, SavepointVersion version)
    {
#ifdef SAVEPOINT_DEBUGGER
        // C++ magic to ensure the type's debug information is registered at startup
        SavepointDebugRegistrar<T>::kRegistered;
#endif

        SAVEPOINT_PROFILE_SCOPE();
        State.Version = version;
        State.Flags = SavepointVisitorFlags::None;
        if (std::endian::native == std::endian::big)
        {
            State.Flags = SavepointVisitorFlags::BigEndian;
        }
        State.TypeID = SavepointTypeID<T>();
        State.Error = false;
        State.Reader = {};
        State.Data.clear();
        State.Offset = 0;
        SavepointVersion savepointVersion = kSavepointVersion;
        operator()(State.Flags);
        operator()(State.Version);
        operator()(savepointVersion);
        operator()(State.TypeID);
        State.DebugClear();
        operator()(item);
    }

    /**
     * @brief Prepare a visitor for reading from bytes.
     *
     * @param data The data as bytes.
     * @param size The number of bytes.
     */
    void Begin(const void* data, int size)
    {
        State.Reader = {};
        State.Data.clear();
        State.Error = false;
        if (size < 0)
        {
            SavepointLog("Visitor size was less than zero");
            SetError();
            return;
        }
        State.Data.resize(size);
        if (size)
        {
            std::memcpy(State.Data.data(), data, size);
        }
        BeginInternal();
    }

    /**
     * @brief Prepare a visitor for reading from bytes and deserialize the item.
     *
     * @tparam T The type to read.
     * @param data The data as bytes.
     * @param size The number of bytes.
     * @param item The item to read into.
     */
    template<typename T>
    void Begin(const void* data, int size, T& item)
    {
        SAVEPOINT_PROFILE_SCOPE();
        Begin(data, size);
        operator()(item);
        if (!IsEmpty())
        {
            SavepointLog("Visitor has unread data");
            SetError();
        }
    }

private:
    void BeginInternal()
    {
        SAVEPOINT_PROFILE_SCOPE();
        State.Version = SavepointVersion{};
        State.Flags = SavepointVisitorFlags::None;
        State.TypeID = 0;
        State.Error = false;
        State.Reader = State.Data;
        State.Offset = 0;
        State.DebugClear();
        using VisitorFlags = typename SavepointPortableTypeConverter<SavepointVisitorFlags>::Type;
        if (sizeof(VisitorFlags) > State.Data.size())
        {
            SavepointLog("Tried to read past visitor flags");
            SetError();
            return;
        }

        // The endianness has to be captured before all reads (include state.Flags)
        const uint8_t* flags = State.Reader.data();
        if ((flags[sizeof(VisitorFlags) - 1] & std::to_underlying(SavepointVisitorFlags::BigEndian)) != 0)
        {
            State.Flags = SavepointVisitorFlags::BigEndian;
        }

        operator()(State.Flags);
        operator()(State.Version);
        Skip<uint32_t>(); // kSavepointVersion
        operator()(State.TypeID);
        State.DebugClear();
    }

public:
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
        using ConverterType = SavepointPortableTypeConverter<std::remove_cv_t<T>>;
        using PortableType = typename ConverterType::Type;
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
                if (sizeof(PortableType) > GetSize())
                {
                    SavepointLog(std::format("Tried to read past visitor: {} -> {}", State.Version.GetString(), version.GetString()));
                    SetError();
                    if constexpr (sizeof...(Args) > 0)
                    {
                        item = T{std::forward<Args>(args)...};
                    }
                    return;
                }
                PortableType value;
                Memcpy(std::addressof(value), State.Reader.data() + State.Offset, 1, sizeof(value));
                State.Offset += sizeof(value);
                item = ConverterType::Read(value);
            }
        }
        else
        {
            if (HasError())
            {
                return;
            }
            PortableType value = ConverterType::Write(item);
            State.Data.resize(State.Data.size() + sizeof(value));
            Memcpy(State.Data.data() + State.Data.size() - sizeof(value), std::addressof(value), 1, sizeof(value));
        }
        DebugLeaf(item);
    }

    /**
     * @brief Visit a contiguous block of portable elements.
     *
     * @tparam T The element type.
     * @param data The pointer to the elements.
     * @param size The number of elements.
     */
    template<SavepointIsCopyable T>
    void operator()(T* data, int size)
    {
        using ConverterType = SavepointPortableTypeConverter<std::remove_cv_t<T>>;
        using PortableType = typename ConverterType::Type;
        static constexpr bool kIsPortable = sizeof(PortableType) == sizeof(T);
        if (HasError())
        {
            return;
        }
        int bytes = size * sizeof(PortableType);
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
                    if constexpr (kIsPortable)
                    {
                        Memcpy(data, State.Reader.data() + State.Offset, size, sizeof(PortableType));
                        State.Offset += bytes;
                    }
                    else
                    {
                        for (int i = 0; i < size; i++)
                        {
                            PortableType value;
                            Memcpy(std::addressof(value), State.Reader.data() + State.Offset, 1, sizeof(value));
                            State.Offset += sizeof(value);
                            data[i] = ConverterType::Read(value);
                        }
                    }
                }
            }
        }
        else if (bytes)
        {
            State.Data.resize(State.Data.size() + bytes);
            uint8_t* destination = State.Data.data() + State.Data.size() - bytes;
            if constexpr (kIsPortable)
            {
                Memcpy(destination, data, size, sizeof(PortableType));
            }
            else
            {
                for (int i = 0; i < size; i++)
                {
                    PortableType value = ConverterType::Write(data[i]);
                    Memcpy(destination + i * sizeof(value), std::addressof(value), 1, sizeof(value));
                }
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

private:
    // Memcpy accounts for endianness differences
    void Memcpy(void* destination, const void* source, int elements, int size) const
    {
        if (size == 1 || GetEndianness() == std::endian::native)
        {
            std::memcpy(destination, source, elements * size);
        }
        else
        {
            const uint8_t* src = static_cast<const uint8_t*>(source);
            uint8_t* dst = static_cast<uint8_t*>(destination);
            for (int offset = 0; offset < elements * size; offset += size)
            {
                std::reverse_copy(src + offset, src + offset + size, dst + offset);
            }
        }
    }

    // Helper for SavepointVisitor::Visit and free Visit functions
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
     * @brief Skip an element.
     *
     * @tparam T The type to skip.
     */
    template<typename T>
    void Skip()
    {
        if (HasError())
        {
            return;
        }
        if constexpr (SavepointIsCopyable<T>)
        {
            using ConverterType = SavepointPortableTypeConverter<std::remove_cv_t<T>>;
            using PortableType = typename ConverterType::Type;
            if (IsReading())
            {
                if (sizeof(PortableType) > GetSize())
                {
                    SavepointLog(std::format("Tried to skip past visitor: {}", State.Version.GetString()));
                    SetError();
                    return;
                }
                State.Offset += sizeof(PortableType);
            }
            else
            {
                State.Data.resize(State.Data.size() + sizeof(PortableType));
            }
        }
        else
        {
            T element;
            (*this)(element);
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
#ifdef SAVEPOINT_DEBUGGER
        static constexpr int kBacktraces = 10;
        if (!State.Error && IsReading() && State.Backtraces < kBacktraces)
        {
            State.Backtraces++;
            std::string result = std::format("Backtrace: stopped at byte {} of {}", State.Offset, State.Reader.size());
            for (const SavepointDebugNode& node : State.Debug)
            {
                result += std::format("\n{:{}}{}", "", (node.GetDepth() + 1) * 2, node.GetTypeName());
                if (node.GetIsLeaf())
                {
                    result += std::format(" = {}", node.GetValue());
                }
            }
            if (State.Backtraces == kBacktraces)
            {
                result += std::format("\nHit max of {} backtraces", kBacktraces);
            }
            SavepointLog(result);
        }
#endif
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
     * @brief Get the endianness of the visitor's data.
     *
     * @return The endianness.
     */
    std::endian GetEndianness() const
    {
        if ((std::to_underlying(State.Flags) & std::to_underlying(SavepointVisitorFlags::BigEndian)) != 0)
        {
            return std::endian::big;
        }
        else
        {
            return std::endian::little;
        }
    }

    /**
     * @brief Get the type ID of the item held by the visitor.
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
      * Requires the application to have read or written the stored type.
      *
      * @return The debug nodes.
      * @see SavepointTypeID
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
            node.Type = std::format("Failed to parse debug information: {}. {} bytes", State.TypeID, GetSize());
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
        if (IsWriting())
        {
            return;
        }
        SavepointDebugNode node;
        node.Depth = State.Depth;
        if constexpr (std::is_base_of_v<SavepointPolymorph, T>)
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
        if (IsWriting())
        {
            return;
        }
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
        if (IsWriting())
        {
            return;
        }
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
            return State.Data.size();
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
        return State.Data.data();
    }

    /**
     * @brief Clear the visitor.
     */
    void Clear()
    {
        State.Reader = {};
        State.Data.clear();
    }

    /**
     * @brief Shrink the visitor to the required amount of memory.
     */
    void ShrinkToFit()
    {
        State.Data.shrink_to_fit();
    }

private:
    inline static thread_local std::vector<VisitorState> States;
    VisitorState State;
};
