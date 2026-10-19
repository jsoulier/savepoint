#pragma once

#include <savepoint/log.hpp>
#include <savepoint/stream.hpp>
#include <savepoint/visitor.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

/**
 * @brief Convenient wrapper for serializing objects to and from a file.
 */
class SavepointFile
{
public:
    /**
     * @brief Create a Savepoint file.
     *
     * @param path The file to read or write.
     */
    explicit SavepointFile(std::string path = {})
        : Path{std::move(path)}
    {
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
        std::ifstream stream{Path, std::ios::binary};
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
    bool Write(T& item, SavepointVersion version) const
    {
        SavepointVisitor visitor;
        visitor.Begin(item, version);
        if (visitor.HasError())
        {
            return false;
        }
        std::string path = Path + ".tmp";
        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        stream << visitor;
        stream.close();
        std::error_code error;
        if (!stream)
        {
            SavepointLog(std::format("Failed to write file: {}", path));
            std::filesystem::remove(path, error);
            return false;
        }
        std::filesystem::rename(path, Path, error);
        if (error)
        {
            SavepointLog(std::format("Failed to replace file: {}, {}", Path, error.message()));
            std::filesystem::remove(path, error);
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
        return std::filesystem::remove(Path);
    }

private:
    std::string Path;
};
