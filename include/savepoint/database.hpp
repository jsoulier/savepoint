#pragma once

#include <savepoint/driver.hpp>
#include <savepoint/entity.hpp>
#include <savepoint/id.hpp>
#include <savepoint/log.hpp>
#include <savepoint/profile.hpp>
#include <savepoint/traits.hpp>
#include <savepoint/version.hpp>
#include <savepoint/visitor.hpp>

#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
 * @return If true, continue reading.
 * @see SavepointID
 */
template<typename T>
using SavepointReadEntityFunction = std::function<bool(T& item)>;

/**
 * @brief The 2D tile read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 * @param x The x location.
 * @param y The y location.
 * @return If true, continue reading.
 */
template<typename T>
using SavepointReadTile2DFunction = std::function<bool(T& item, int x, int y)>;

/**
 * @brief The 3D tile read function signature.
 * 
 * @tparam T The type to read.
 * @param item The read item.
 * @param x The x location.
 * @param y The y location.
 * @param z The z location.
 * @return If true, continue reading.
 */
template<typename T>
using SavepointReadTile3DFunction = std::function<bool(T& item, int x, int y, int z)>;

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
    Savepoint();

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
     * @brief Take the connection and close the other.
     *
     * @param other The Savepoint to move from.
     */
    Savepoint(Savepoint&& other);

    /**
     * @brief Take the connection and close the other.
     *
     * @param other The Savepoint to move from.
     * @return This Savepoint.
     */
    Savepoint& operator=(Savepoint&& other);

    /**
     * @brief Opens a new connection.
     *
     * @param path The path to the Savepoint file.
     * @param version The user's application version.
     * @param flags The flags to use (driver, read and/or write, thread safety).
     * @return The result of the attempt to open a connection.
     * @see Save
     * @see Close
     */
    SavepointStatus Open(const std::string& path, SavepointVersion version, SavepointFlags flags = SavepointFlags::SQLite3 | SavepointFlags::ReadWrite);

    /**
     * @brief Check if the Savepoint is open.
     *
     * @return True if the Savepoint is open.
     */
    [[nodiscard]] bool IsOpen() const;

    /**
     * @brief Get the name of the underlying driver.
     *
     * @return The driver name or an empty string.
     * @see ISavepointDriver
     */
    [[nodiscard]] std::string_view GetDriverName() const;

    /**
     * @brief Write a singleton to the Savepoint.
     *
     * For storing information such as date and time, the user can write a
     * singleton with the assumption that only one entry exists.
     *
     * @tparam T The type to write.
     * @param item The item to write.
     * @return True if the item was written.
     */
    template<SavepointIsVisitable T>
    bool Write(T& item)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanWrite())
        {
            return false;
        }
        SavepointVisitor visitor;
        visitor.Begin(item, Version);
        if (visitor.HasError())
        {
            SavepointLog("Failed to write singleton");
            return false;
        }
        return Driver->Write(visitor.GetData(), visitor.GetSize());
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
     * @return True if the item was written.
     * @snippet examples/15_reserved_ids.cpp 15_reserved_ids
     */
    template<SavepointIsVisitable T> requires (!SavepointIsEntity<T>)
    bool Write(T& item, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanWrite())
        {
            return false;
        }
        SavepointVisitor visitor;
        visitor.Begin(item, Version);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to write singleton: level={}", level));
            return false;
        }
        return Driver->Write(visitor.GetData(), visitor.GetSize(), level);
    }

    /**
     * @brief Write an entity to the Savepoint.
     *
     * For items without a unique location, an ID can be used to ensure the item
     * gets a unique entry. If the ID is invalid, a new ID will be reserved and
     * a new entry will be inserted. If the ID is valid, an existing entry will
     * be updated (including the level) or inserted with that ID.
     *
     * @tparam T The type to write.
     * @param item The item to write.
     * @param level The level.
     * @return True if the entity was written.
     * @see SavepointEntity
     * @see Reserve
     */
    template<SavepointIsEntity T>
    bool Write(T& item, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanWrite())
        {
            return false;
        }
        SavepointVisitor visitor;
        visitor.Begin(item, Version);
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
            return false;
        }
        if (!id.IsValid())
        {
            // Not an error. New entities get a new ID
            id.Value = Driver->Reserve();
        }
        if (!Driver->Write(visitor.GetData(), visitor.GetSize(), id.Value, level))
        {
            SavepointLog(std::format("Failed to write entity: id={}, level={}", id.Value, level));
            return false;
        }
        return true;
    }

    /**
     * @brief Give an entity an ID without writing it.
     *
     * @tparam T The type to reserve.
     * @param item The item to give an ID.
     * @return True if the entity has an ID.
     * @see SavepointEntity
     */
    template<SavepointIsEntity T>
    bool Reserve(T& item)
    {
        SAVEPOINT_PROFILE_SCOPE();
        SavepointID& id = GetID(item);
        if (id.IsValid())
        {
            return true;
        }
        if (!CanWrite())
        {
            return false;
        }
        id.Value = Driver->Reserve();
        return id.IsValid();
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
     * @return True if the tile was written.
     */
    template<SavepointIsVisitable T>
    bool Write(T& item, int x, int y, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanWrite())
        {
            return false;
        }
        SavepointVisitor visitor;
        visitor.Begin(item, Version);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to write tile: x={}, y={}, level={}", x, y, level));
            return false;
        }
        return Driver->Write(visitor.GetData(), visitor.GetSize(), x, y, level);
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
     * @return True if the tile was written.
     */
    template<SavepointIsVisitable T>
    bool Write(T& item, int x, int y, int z, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanWrite())
        {
            return false;
        }
        SavepointVisitor visitor;
        visitor.Begin(item, Version);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to write tile: x={}, y={}, z={}, level={}", x, y, z, level));
            return false;
        }
        return Driver->Write(visitor.GetData(), visitor.GetSize(), x, y, z, level);
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool success = false;
        Driver->Read([&visitor, &item, &success](const void* data, int size)
        {
            visitor.Begin(data, size, item);
            success = true;
        });
        if (visitor.HasError())
        {
            SavepointLog("Failed to read singleton");
            return false;
        }
        return success;
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool success = false;
        Driver->Read([&visitor, &item, &success](const void* data, int size)
        {
            visitor.Begin(data, size, item);
            success = true;
        }, level);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read singleton: level={}", level));
            return false;
        }
        return success;
    }

    /**
     * @brief Read an entity by its ID from the Savepoint.
     *
     * @tparam T The type to read.
     * @param item The item to read.
     * @param id The entity ID.
     * @return True if the entity exists.
     * @see SavepointEntity
     */
    template<SavepointIsEntity T>
    bool Read(T& item, SavepointID id)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead() || !id.IsValid())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool success = false;
        Driver->ReadEntity([&visitor, &item, &success](const void* data, int size)
        {
            visitor.Begin(data, size, item);
            success = true;
        }, id.Value);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read entity: id={}", id.Value));
            return false;
        }
        if (success)
        {
            GetID(item) = id;
        }
        return success;
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function, level](const void* data, int size, int64_t id)
        {
            T item;
            visitor.Begin(data, size, item);
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read entity: id={}, level={}", id, level));
                return true;
            }
            GetID(item).Value = id;
            return function(item);
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function, level](const void* data, int size, int x, int y)
        {
            T item;
            visitor.Begin(data, size, item);
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, level={}", x, y, level));
                return true;
            }
            return function(item, x, y);
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function, level](const void* data, int size, int x, int y, int z)
        {
            T item;
            visitor.Begin(data, size, item);
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, z={}, level={}", x, y, z, level));
                return true;
            }
            return function(item, x, y, z);
        }, level);
    }

    /**
     * @brief Read all 2D tiles inside a region of the specified level from the Savepoint.
     *
     * @tparam T The type to read.
     * @param function The function to use.
     * @param minX The minimum x location (inclusive).
     * @param minY The minimum y location (inclusive).
     * @param maxX The maximum x location (inclusive).
     * @param maxY The maximum y location (inclusive).
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Read(const SavepointReadTile2DFunction<T>& function, int minX, int minY, int maxX, int maxY, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function, level](const void* data, int size, int x, int y)
        {
            T item;
            visitor.Begin(data, size, item);
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, level={}", x, y, level));
                return true;
            }
            return function(item, x, y);
        }, level, minX, minY, maxX, maxY);
    }

    /**
     * @brief Read all 3D tiles inside a region of the specified level from the Savepoint.
     *
     * @tparam T The type to read.
     * @param function The function to use.
     * @param minX The minimum x location (inclusive).
     * @param minY The minimum y location (inclusive).
     * @param minZ The minimum z location (inclusive).
     * @param maxX The maximum x location (inclusive).
     * @param maxY The maximum y location (inclusive).
     * @param maxZ The maximum z location (inclusive).
     * @param level The level.
     */
    template<SavepointIsVisitable T>
    void Read(const SavepointReadTile3DFunction<T>& function, int minX, int minY, int minZ, int maxX, int maxY, int maxZ, int level)
    {
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return;
        }
        SavepointVisitor visitor;
        Driver->Read([&visitor, &function, level](const void* data, int size, int x, int y, int z)
        {
            T item;
            visitor.Begin(data, size, item);
            if (visitor.HasError())
            {
                SavepointLog(std::format("Failed to read tile: x={}, y={}, z={}, level={}", x, y, z, level));
                return true;
            }
            return function(item, x, y, z);
        }, level, minX, minY, minZ, maxX, maxY, maxZ);
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool success = false;
        Driver->Read([&visitor, &tile, &success](const void* data, int size)
        {
            visitor.Begin(data, size, tile);
            success = true;
        }, level, x, y);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read tile: x={}, y={}, level={}", x, y, level));
            success = false;
        }
        return success;
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
        SAVEPOINT_PROFILE_SCOPE();
        if (!CanRead())
        {
            return false;
        }
        SavepointVisitor visitor;
        bool success = false;
        Driver->Read([&visitor, &tile, &success](const void* data, int size)
        {
            visitor.Begin(data, size, tile);
            success = true;
        }, level, x, y, z);
        if (visitor.HasError())
        {
            SavepointLog(std::format("Failed to read tile: x={}, y={}, z={}, level={}", x, y, z, level));
            success = false;
        }
        return success;
    }

    /**
     * @brief Get all the levels from the Savepoint. Duplicates are removed.
     * 
     * @return The levels.
     */
    [[nodiscard]] std::vector<int> GetLevels();

    /**
     * @brief Check if an entity exists in the Savepoint.
     *
     * @param id The entity ID.
     * @return True if the entity exists.
     * @see SavepointEntity
     */
    [[nodiscard]] bool Contains(SavepointID id);

    /**
     * @brief Check if a 2D tile exists in the Savepoint.
     *
     * @param x The x location.
     * @param y The y location.
     * @param level The level.
     * @return True if the tile exists.
     */
    [[nodiscard]] bool Contains(int x, int y, int level);

    /**
     * @brief Check if a 3D tile exists in the Savepoint.
     *
     * @param x The x location.
     * @param y The y location.
     * @param z The z location.
     * @param level The level.
     * @return True if the tile exists.
     */
    [[nodiscard]] bool Contains(int x, int y, int z, int level);

    /**
     * @brief Get the number of entities in the specified level.
     *
     * @param level The level.
     * @return The number of entities.
     */
    [[nodiscard]] int GetEntityCount(int level);

    /**
     * @brief Get the number of 2D tiles in the specified level.
     *
     * @param level The level.
     * @return The number of 2D tiles.
     */
    [[nodiscard]] int GetTile2DCount(int level);

    /**
     * @brief Get the number of 3D tiles in the specified level.
     *
     * @param level The level.
     * @return The number of 3D tiles.
     */
    [[nodiscard]] int GetTile3DCount(int level);

    /**
     * @brief Deletes an entity from the Savepoint and invalidates its ID.
     *
     * @tparam T The type to delete
     * @param item The item to delete.
     * @return True if the entity was deleted.
     * @see SavepointEntity
     */
    template<SavepointIsEntity T>
    bool Delete(T& item)
    {
        SAVEPOINT_PROFILE_SCOPE();
        SavepointID& id = GetID(item);
        bool success = Delete(id);
        id = SavepointID{};
        return success;
    }

    /**
     * @brief Deletes the singleton from the Savepoint.
     *
     * @return True if the singleton was deleted.
     */
    bool Delete();

    /**
     * @brief Deletes the singleton at the specified level from the Savepoint.
     *
     * Entities and tiles in the level are kept.
     *
     * @param level The level.
     * @return True if the singleton was deleted.
     */
    bool Delete(int level);

    /**
     * @brief Deletes an entity from the Savepoint.
     *
     * @param id The entity ID.
     * @return True if the entity was deleted.
     * @see SavepointEntity
     */
    bool Delete(SavepointID id);

    /**
     * @brief Deletes a 2D tile from the Savepoint.
     *
     * @param x The x location.
     * @param y The y location.
     * @param level The level.
     * @return True if the tile was deleted.
     */
    bool Delete(int x, int y, int level);

    /**
     * @brief Deletes a 3D tile from the Savepoint.
     *
     * @param x The x location.
     * @param y The y location.
     * @param z The z location.
     * @param level The level.
     * @return True if the tile was deleted.
     */
    bool Delete(int x, int y, int z, int level);

    /**
     * @brief Deletes the singleton, entities, and tiles in the specified level.
     *
     * @param level The level.
     * @return True if anything was deleted.
     */
    bool DeleteLevel(int level);

    /**
     * @brief Closes the connection. Does NOT call Savepoint::Save.
     *
     * @return True if the connection was closed.
     * @see Save
     * @see Close
     */
    bool Close();

    /**
     * @brief Save all pending changes.
     *
     * Commits the current transaction and starts a new one. The next time
     * Savepoint::Open is called, it will return SavepointStatus::Existing
     * instead of SavepointStatus::New.
     * 
     * @see Open
     */
    bool Save();

    /**
     * @brief Discard all pending changes since the last save.
     *
     * @return True if the rollback was completed.
     * @see Save
     */
    bool Rollback();

    /**
     * @brief Create a backup of the last save.
     *
     * @param path The path to the backup file.
     * @return True if the backup was created.
     * @see Save
     */
    bool Backup(const std::string& path);

    /**
     * @brief Remove all entities, tiles, and level singletons from the Savepoint.
     *
     * @return True if the Savepoint was cleared.
     */
    bool Clear();

#ifdef SAVEPOINT_DEBUGGER

    /**
     * @brief Read the singleton's debug nodes.
     *
     * @param nodes The debug nodes.
     * @return True if the singleton exists.
     */
    bool ReadDebug(std::vector<SavepointDebugNode>& nodes);

    /**
     * @brief Read the singleton's debug nodes for a specific level.
     *
     * @param nodes The tree describing the singleton.
     * @param level The level.
     * @return True if the singleton exists.
     */
    bool ReadDebug(std::vector<SavepointDebugNode>& nodes, int level);

    /**
     * @brief Read all entity debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugEntityFunction& function, int level);

    /**
     * @brief Read all 2D tile debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugTile2DFunction& function, int level);

    /**
     * @brief Read all 3D tile debug nodes for a specific level.
     *
     * @param function The function to use.
     * @param level The level.
     */
    void ReadDebug(const SavepointDebugTile3DFunction& function, int level);

#endif

private:
    [[nodiscard]] bool CanRead() const;
    [[nodiscard]] bool CanWrite() const;

    // TODO: Bad hack 
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
    SavepointFlags Flags;
    std::unique_ptr<ISavepointDriver> Driver;
};
