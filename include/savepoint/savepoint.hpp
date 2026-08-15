#pragma once

#include <savepoint/debug.hpp>
#include <savepoint/driver.hpp>
#include <savepoint/entity.hpp>
#include <savepoint/fwd.hpp>
#include <savepoint/id.hpp>
#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
#include <savepoint/std.hpp>
#include <savepoint/traits.hpp>
#include <savepoint/version.hpp>
#include <savepoint/visitor.hpp>

#include <format>
#include <functional>
#include <memory>
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
        return ReadDebugInternal(nodes, [this](const SavepointReadDataFunction& function)
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
        return ReadDebugInternal(nodes, [this, level](const SavepointReadDataFunction& function)
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
    bool ReadDebugInternal(std::vector<SavepointDebugNode>& nodes, const T& read)
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
