#pragma once

#include <savepoint/fwd.hpp>
#include <savepoint/status.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief Options for Savepoint.
 */
enum class SavepointFlags : uint32_t
{
    None = 0,                 /**< No options. */
    SQLite3 = 1u << 0,        /**< Use the sqlite3 driver if it's supported. */
    Null = 1u << 1,           /**< Use the noop driver if it's supported. */
    Read = 1u << 2,           /**< Enable reading from the database. */
    Write = 1u << 3,          /**< Enable writing to the database. Holds the file's write lock while open. Implies Read. */
    ReadWrite = Read | Write, /**< Enable reading and writing. */
    ThreadSafe = 1u << 4,     /**< The Savepoint will be used from multiple threads. */
};

/**
 * @brief Combine flags.
 *
 * @param lhs The first flags.
 * @param rhs The second flags.
 * @return The combined flags.
 */
constexpr SavepointFlags operator|(SavepointFlags lhs, SavepointFlags rhs)
{
    return SavepointFlags(std::to_underlying(lhs) | std::to_underlying(rhs));
}

/**
 * @brief Combine flags.
 *
 * @param lhs The flags to add to.
 * @param rhs The flags to add.
 * @return The combined flags.
 */
constexpr SavepointFlags& operator|=(SavepointFlags& lhs, SavepointFlags rhs)
{
    return lhs = lhs | rhs;
}

/**
 * @brief Intersect flags.
 *
 * @param lhs The first flags.
 * @param rhs The second flags.
 * @return The flags in both.
 */
constexpr SavepointFlags operator&(SavepointFlags lhs, SavepointFlags rhs)
{
    return SavepointFlags(std::to_underlying(lhs) & std::to_underlying(rhs));
}

/** @cond INTERNAL */

using SavepointReadDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadAllEntityDataFunction = std::function<void(const void* data, int size, int64_t id)>;
using SavepointReadAllTile2DDataFunction = std::function<void(const void* data, int size, int x, int y)>;
using SavepointReadAllTile3DDataFunction = std::function<void(const void* data, int size, int x, int y, int z)>;
using SavepointReadTile2DDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadTile3DDataFunction = std::function<void(const void* data, int size)>;
using SavepointReadAllLevelsFunction = std::function<void(int level)>;

class ISavepointDriver
{
public:
    virtual ~ISavepointDriver() = default;
    virtual std::string_view GetName() const = 0;
    virtual SavepointStatus Open(const std::string& path, SavepointFlags flags) = 0;
    virtual bool IsOpen() = 0;
    virtual bool Write(const void* data, int size) = 0;
    virtual bool Write(const void* data, int size, int level) = 0;
    virtual int64_t Insert(const void* data, int size, int level) = 0;
    virtual bool Update(const void* data, int size, int64_t id, int level) = 0;
    virtual bool Write(const void* data, int size, int x, int y, int level) = 0;
    virtual bool Write(const void* data, int size, int x, int y, int z, int level) = 0;
    virtual void Read(const SavepointReadDataFunction& function) = 0;
    virtual void Read(const SavepointReadDataFunction& function, int level) = 0;
    virtual void ReadEntity(const SavepointReadDataFunction& function, int64_t id) = 0;
    virtual void Read(const SavepointReadAllEntityDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadAllTile2DDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadAllTile3DDataFunction& function, int level) = 0;
    virtual void Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y) = 0;
    virtual void Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z) = 0;
    virtual void Read(const SavepointReadAllLevelsFunction& function) = 0;
    virtual void Read(const SavepointReadAllTile2DDataFunction& function, int level, int minX, int minY, int maxX, int maxY) = 0;
    virtual void Read(const SavepointReadAllTile3DDataFunction& function, int level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) = 0;
    virtual bool Contains(int64_t id) = 0;
    virtual bool Contains(int x, int y, int level) = 0;
    virtual bool Contains(int x, int y, int z, int level) = 0;
    virtual int CountEntities(int level) = 0;
    virtual int CountTiles2D(int level) = 0;
    virtual int CountTiles3D(int level) = 0;
    virtual bool Delete() = 0;
    virtual bool Delete(int level) = 0;
    virtual bool Delete(int64_t id) = 0;
    virtual bool Delete(int x, int y, int level) = 0;
    virtual bool Delete(int x, int y, int z, int level) = 0;
    virtual bool DeleteLevel(int level) = 0;
    virtual bool Close() = 0;
    virtual bool Save() = 0;
    virtual bool Rollback() = 0;
    virtual bool Backup(const std::string& path) = 0;
    virtual bool Clear() = 0;
};

/** @endcond */
