#pragma once

#include <savepoint/fwd.hpp>
#include <savepoint/status.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

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
