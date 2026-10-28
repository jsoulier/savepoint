#pragma once

#include <savepoint/log.hpp>
#include <savepoint/stream.hpp>
#include <savepoint/visitor.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

/**
 * @brief Convenient wrapper for serializing objects to and from a file.
 */
class SavepointFile
{
    static constexpr std::string_view kTmpSuffix = ".tmp";

public:
    /**
     * @brief Create a Savepoint file.
     *
     * @param path The file to read or write.
     */
    explicit SavepointFile(std::string path = {})
        : Path{std::move(path) + std::string{kTmpSuffix}}
    {
    }

    /**
     * @brief Get the path to the file.
     *
     * @return The path.
     */
    [[nodiscard]] std::string_view GetPath() const
    {
        return std::string_view{Path}.substr(0, Path.size() - kTmpSuffix.size());
    }

    /**
     * @brief Get the path to the temporary file used while writing.
     *
     * @return The temporary path.
     */
    [[nodiscard]] std::string_view GetTmpPath() const
    {
        return Path;
    }

    /**
     * @brief Try to deserialize an object from the file.
     *
     * @tparam T The object type.
     * @param item The object to deserialize into.
     * @return True if the entire file was successfully deserialized.
     */
    template<typename T>
    bool TryRead(T& item) const
    {
        std::ifstream stream{std::filesystem::path{GetPath()}, std::ios::binary};
        if (!stream)
        {
            return false;
        }
        SavepointVisitor visitor;
        stream >> visitor;
        if (!stream)
        {
            return false;
        }
        visitor(item);
        return !visitor.HasError() && visitor.IsEmpty();
    }

    /**
     * @brief Serialize an object to the file.
     *
     * @tparam T The object type.
     * @param item The object to serialize.
     * @param version The application version to store.
     * @return True if the object was successfully serialized and written.
     */
    template<typename T>
    bool Write(T& item, SavepointVersion version = {}) const
    {
        SavepointVisitor visitor;
        visitor.Begin(item, version);
        if (visitor.HasError())
        {
            return false;
        }
        std::error_code error;
        if (!Write(visitor.GetData(), visitor.GetSize()))
        {
            SavepointLog(std::format("Failed to write file: {}", GetTmpPath()));
            std::filesystem::remove(GetTmpPath(), error);
            return false;
        }
        std::filesystem::rename(GetTmpPath(), GetPath(), error);
        if (error)
        {
            SavepointLog(std::format("Failed to replace file: {}, {}", GetPath(), error.message()));
            std::filesystem::remove(GetTmpPath(), error);
            return false;
        }
        return true;
    }

    /**
     * @brief Delete the file.
     *
     * @return True if the file was deleted.
     */
    bool Delete() const
    {
        return std::filesystem::remove(GetPath());
    }

private:
    // Avoid corruptions by syncing the write (only if sqlite3 is available)
    bool Write(const void* data, int size) const;

    std::string Path;
};
