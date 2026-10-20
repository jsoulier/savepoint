#include <sqlite3.h>

#include <savepoint/profile.hpp>
#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sqlite3.hpp"

static constexpr int kSavepointBusyTimeout = 16; // 16ms

static constexpr const char* kSQL =
    "CREATE TABLE IF NOT EXISTS status ("
    "    id INTEGER PRIMARY KEY"
    ");"
    "CREATE TABLE IF NOT EXISTS header ("
    "    id INTEGER PRIMARY KEY,"
    "    data BLOB NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS levels ("
    "    level INTEGER PRIMARY KEY,"
    "    data BLOB NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS entities ("
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    level INTEGER NOT NULL,"
    "    data BLOB NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS tiles_2d ("
    "    x INTEGER NOT NULL,"
    "    y INTEGER NOT NULL,"
    "    level INTEGER NOT NULL,"
    "    data BLOB NOT NULL,"
    "    PRIMARY KEY (x, y, level)"
    ");"
    "CREATE TABLE IF NOT EXISTS tiles_3d ("
    "    x INTEGER NOT NULL,"
    "    y INTEGER NOT NULL,"
    "    z INTEGER NOT NULL,"
    "    level INTEGER NOT NULL,"
    "    data BLOB NOT NULL,"
    "    PRIMARY KEY (x, y, z, level)"
    ");"
    "CREATE INDEX IF NOT EXISTS entities_index ON entities (level);"
    "CREATE INDEX IF NOT EXISTS tiles_2d_index ON tiles_2d (level, x, y);"
    "CREATE INDEX IF NOT EXISTS tiles_3d_index ON tiles_3d (level, x, y, z);"
    " ";
static constexpr const char* kWriteStatusSQL =
    "INSERT OR REPLACE INTO status (id) VALUES (0);";
static constexpr const char* kWriteSQL =
    "INSERT OR REPLACE INTO header (id, data) VALUES (0, ?);";
static constexpr const char* kWriteLevelSQL =
    "INSERT OR REPLACE INTO levels (level, data) VALUES (?, ?);";
static constexpr const char* kInsertEntitySQL =
    "INSERT INTO entities (level, data) VALUES (?, ?);";
static constexpr const char* kUpdateEntitySQL =
    "UPDATE entities SET level = ?, data = ? WHERE id = ?;";
static constexpr const char* kWriteTile2DSQL =
    "INSERT OR REPLACE INTO tiles_2d (x, y, level, data) VALUES (?, ?, ?, ?);";
static constexpr const char* kWriteTile3DSQL =
    "INSERT OR REPLACE INTO tiles_3d (x, y, z, level, data) VALUES (?, ?, ?, ?, ?);";
static constexpr const char* kReadStatusSQL =
    "SELECT 0 FROM status WHERE id = 0;";
static constexpr const char* kReadSQL =
    "SELECT data FROM header WHERE id = 0;";
static constexpr const char* kReadLevelSQL =
    "SELECT data FROM levels WHERE level = ?;";
static constexpr const char* kReadEntitySQL =
    "SELECT data FROM entities WHERE id = ?;";
static constexpr const char* kReadEntitiesSQL =
    "SELECT id, data FROM entities WHERE level = ?;";
static constexpr const char* kReadAllTiles2DSQL =
    "SELECT x, y, data FROM tiles_2d WHERE level = ?;";
static constexpr const char* kReadAllTiles3DSQL =
    "SELECT x, y, z, data FROM tiles_3d WHERE level = ?;";
static constexpr const char* kReadTile2DSQL =
    "SELECT data FROM tiles_2d WHERE level = ? AND x = ? AND y = ?;";
static constexpr const char* kReadTile3DSQL =
    "SELECT data FROM tiles_3d WHERE level = ? AND x = ? AND y = ? AND z = ?;";
static constexpr const char* kReadLevelsSQL =
    "SELECT level FROM levels UNION SELECT level FROM entities UNION SELECT level FROM tiles_2d UNION SELECT level FROM tiles_3d;";
static constexpr const char* kReadRegionTiles2DSQL =
    "SELECT x, y, data FROM tiles_2d WHERE level = ? AND x BETWEEN ? AND ? AND y BETWEEN ? AND ?;";
static constexpr const char* kReadRegionTiles3DSQL =
    "SELECT x, y, z, data FROM tiles_3d WHERE level = ? AND x BETWEEN ? AND ? AND y BETWEEN ? AND ? AND z BETWEEN ? AND ?;";
static constexpr const char* kContainsEntitySQL =
    "SELECT 1 FROM entities WHERE id = ?;";
static constexpr const char* kContainsTile2DSQL =
    "SELECT 1 FROM tiles_2d WHERE level = ? AND x = ? AND y = ?;";
static constexpr const char* kContainsTile3DSQL =
    "SELECT 1 FROM tiles_3d WHERE level = ? AND x = ? AND y = ? AND z = ?;";
static constexpr const char* kCountEntitiesSQL =
    "SELECT COUNT(*) FROM entities WHERE level = ?;";
static constexpr const char* kCountTiles2DSQL =
    "SELECT COUNT(*) FROM tiles_2d WHERE level = ?;";
static constexpr const char* kCountTiles3DSQL =
    "SELECT COUNT(*) FROM tiles_3d WHERE level = ?;";
static constexpr const char* kDeleteSingletonSQL =
    "DELETE FROM header WHERE id = 0;";
static constexpr const char* kDeleteLevelSingletonSQL =
    "DELETE FROM levels WHERE level = ?;";
static constexpr const char* kDeleteEntitySQL =
    "DELETE FROM entities WHERE id = ?;";
static constexpr const char* kDeleteTile2DSQL =
    "DELETE FROM tiles_2d WHERE level = ? AND x = ? AND y = ?;";
static constexpr const char* kDeleteTile3DSQL =
    "DELETE FROM tiles_3d WHERE level = ? AND x = ? AND y = ? AND z = ?;";
static constexpr const char* kDeleteLevelEntitiesSQL =
    "DELETE FROM entities WHERE level = ?;";
static constexpr const char* kDeleteLevelTiles2DSQL =
    "DELETE FROM tiles_2d WHERE level = ?;";
static constexpr const char* kDeleteLevelTiles3DSQL =
    "DELETE FROM tiles_3d WHERE level = ?;";
static constexpr const char* kClearLevelsSQL =
    "DELETE FROM levels;";
static constexpr const char* kClearEntitiesSQL =
    "DELETE FROM entities;";
static constexpr const char* kClearTiles2DSQL =
    "DELETE FROM tiles_2d;";
static constexpr const char* kClearTiles3DSQL =
    "DELETE FROM tiles_3d;";

static std::vector<uint8_t> GetData(sqlite3_stmt* stmt, int column)
{
    // Required so that we don't clobber a statement if the user queries it recursively
    const uint8_t* data = static_cast<const uint8_t*>(sqlite3_column_blob(stmt, column));
    int size = sqlite3_column_bytes(stmt, column);
    return std::vector<uint8_t>(data, data + size);
}

SavepointDriverSQLite3::SavepointDriverSQLite3()
    : ISavepointDriver()
    , Mutex{}
    , Handle{nullptr}
    , WriteStatusStmt{nullptr}
    , WriteStmt{nullptr}
    , WriteLevelStmt{nullptr}
    , InsertEntityStmt{nullptr}
    , UpdateEntityStmt{nullptr}
    , WriteTile2DStmt{nullptr}
    , WriteTile3DStmt{nullptr}
    , ReadStatusStmt{nullptr}
    , ReadStmt{nullptr}
    , ReadLevelStmt{nullptr}
    , ReadEntityStmt{nullptr}
    , ReadEntitiesStmt{nullptr}
    , ReadAllTiles2DStmt{nullptr}
    , ReadAllTiles3DStmt{nullptr}
    , ReadTile2DStmt{nullptr}
    , ReadTile3DStmt{nullptr}
    , ReadLevelsStmt{nullptr}
    , ReadRegionTiles2DStmt{nullptr}
    , ReadRegionTiles3DStmt{nullptr}
    , ContainsEntityStmt{nullptr}
    , ContainsTile2DStmt{nullptr}
    , ContainsTile3DStmt{nullptr}
    , CountEntitiesStmt{nullptr}
    , CountTiles2DStmt{nullptr}
    , CountTiles3DStmt{nullptr}
    , DeleteSingletonStmt{nullptr}
    , DeleteLevelSingletonStmt{nullptr}
    , DeleteEntityStmt{nullptr}
    , DeleteTile2DStmt{nullptr}
    , DeleteTile3DStmt{nullptr}
    , DeleteLevelEntitiesStmt{nullptr}
    , DeleteLevelTiles2DStmt{nullptr}
    , DeleteLevelTiles3DStmt{nullptr}
    , ClearLevelsStmt{nullptr}
    , ClearEntitiesStmt{nullptr}
    , ClearTiles2DStmt{nullptr}
    , ClearTiles3DStmt{nullptr}
{
}

SavepointDriverSQLite3::~SavepointDriverSQLite3()
{
    if (IsOpen())
    {
        Close();
    }
}

std::string_view SavepointDriverSQLite3::GetName() const
{
    return "sqlite3";
}

SavepointStatus SavepointDriverSQLite3::Open(const std::string& path, SavepointFlags flags)
{
    SAVEPOINT_PROFILE_SCOPE();
    Mutex.SetEnabled((flags & SavepointFlags::ThreadSafe) == SavepointFlags::ThreadSafe);
    std::scoped_lock lock{Mutex};
    const bool isReadOnly = (flags & SavepointFlags::Write) != SavepointFlags::Write;
    int openFlags = SQLITE_OPEN_NOMUTEX;
    if (isReadOnly)
    {
        openFlags |= SQLITE_OPEN_READONLY;
    }
    else
    {
        openFlags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    }
    if (sqlite3_open_v2(path.c_str(), &Handle, openFlags, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to open database: {}, {}", path, sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_busy_timeout(Handle, kSavepointBusyTimeout) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to set busy timeout: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (!isReadOnly && sqlite3_exec(Handle, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to enable WAL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (!isReadOnly && sqlite3_wal_autocheckpoint(Handle, 2500) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to set wal_autocheckpoint: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (!isReadOnly && sqlite3_exec(Handle, kSQL, nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to execute kSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kWriteStatusSQL, -1, &WriteStatusStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kWriteStatusSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kWriteSQL, -1, &WriteStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kWriteSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kWriteLevelSQL, -1, &WriteLevelStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kWriteLevelSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kInsertEntitySQL, -1, &InsertEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kInsertEntitySQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kUpdateEntitySQL, -1, &UpdateEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kUpdateEntitySQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kWriteTile2DSQL, -1, &WriteTile2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kWriteTile2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kWriteTile3DSQL, -1, &WriteTile3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kWriteTile3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadStatusSQL, -1, &ReadStatusStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadStatusSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadSQL, -1, &ReadStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadLevelSQL, -1, &ReadLevelStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadLevelSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadEntitySQL, -1, &ReadEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadEntitySQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadEntitiesSQL, -1, &ReadEntitiesStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadEntitiesSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadAllTiles2DSQL, -1, &ReadAllTiles2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadAllTiles2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadAllTiles3DSQL, -1, &ReadAllTiles3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadAllTiles3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadTile2DSQL, -1, &ReadTile2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadTile2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadTile3DSQL, -1, &ReadTile3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadTile3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadLevelsSQL, -1, &ReadLevelsStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadLevelsSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadRegionTiles2DSQL, -1, &ReadRegionTiles2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadRegionTiles2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kReadRegionTiles3DSQL, -1, &ReadRegionTiles3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kReadRegionTiles3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kContainsEntitySQL, -1, &ContainsEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kContainsEntitySQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kContainsTile2DSQL, -1, &ContainsTile2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kContainsTile2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kContainsTile3DSQL, -1, &ContainsTile3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kContainsTile3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kCountEntitiesSQL, -1, &CountEntitiesStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kCountEntitiesSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kCountTiles2DSQL, -1, &CountTiles2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kCountTiles2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kCountTiles3DSQL, -1, &CountTiles3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kCountTiles3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteSingletonSQL, -1, &DeleteSingletonStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteSingletonSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteLevelSingletonSQL, -1, &DeleteLevelSingletonStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteLevelSingletonSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteEntitySQL, -1, &DeleteEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteEntitySQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteTile2DSQL, -1, &DeleteTile2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteTile2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteTile3DSQL, -1, &DeleteTile3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteTile3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteLevelEntitiesSQL, -1, &DeleteLevelEntitiesStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteLevelEntitiesSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteLevelTiles2DSQL, -1, &DeleteLevelTiles2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteLevelTiles2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kDeleteLevelTiles3DSQL, -1, &DeleteLevelTiles3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteLevelTiles3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kClearLevelsSQL, -1, &ClearLevelsStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kClearLevelsSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kClearEntitiesSQL, -1, &ClearEntitiesStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kClearEntitiesSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kClearTiles2DSQL, -1, &ClearTiles2DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kClearTiles2DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_prepare_v2(Handle, kClearTiles3DSQL, -1, &ClearTiles3DStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kClearTiles3DSQL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    SavepointStatus status;
    if (sqlite3_step(ReadStatusStmt) == SQLITE_ROW)
    {
        status = SavepointStatus::Existing;
    }
    else
    {
        status = SavepointStatus::New;
    }
    sqlite3_reset(ReadStatusStmt);
    if (!isReadOnly && sqlite3_exec(Handle, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to take the write lock (is another writer open?): {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    return status;
}

bool SavepointDriverSQLite3::IsOpen()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    return Handle != nullptr;
}

bool SavepointDriverSQLite3::Write(const void* data, int size)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    sqlite3_bind_blob(WriteStmt, 1, data, size, SQLITE_STATIC);
    if (sqlite3_step(WriteStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(WriteStmt);
    return success;
}

bool SavepointDriverSQLite3::Write(const void* data, int size, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    sqlite3_bind_int(WriteLevelStmt, 1, level);
    sqlite3_bind_blob(WriteLevelStmt, 2, data, size, SQLITE_STATIC);
    if (sqlite3_step(WriteLevelStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write level: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(WriteLevelStmt);
    return success;
}

int64_t SavepointDriverSQLite3::Insert(const void* data, int size, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    int64_t id = SavepointID::kInvalidID;
    sqlite3_bind_int(InsertEntityStmt, 1, level);
    sqlite3_bind_blob(InsertEntityStmt, 2, data, size, SQLITE_STATIC);
    if (sqlite3_step(InsertEntityStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to insert entity: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        id = sqlite3_last_insert_rowid(Handle);
    }
    sqlite3_reset(InsertEntityStmt);
    return id;
}

bool SavepointDriverSQLite3::Update(const void* data, int size, int64_t id, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = false;
    sqlite3_bind_int(UpdateEntityStmt, 1, level);
    sqlite3_bind_blob(UpdateEntityStmt, 2, data, size, SQLITE_STATIC);
    sqlite3_bind_int64(UpdateEntityStmt, 3, id);
    if (sqlite3_step(UpdateEntityStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to update entity: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(UpdateEntityStmt);
    return success;
}

bool SavepointDriverSQLite3::Write(const void* data, int size, int x, int y, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    sqlite3_bind_int(WriteTile2DStmt, 1, x);
    sqlite3_bind_int(WriteTile2DStmt, 2, y);
    sqlite3_bind_int(WriteTile2DStmt, 3, level);
    sqlite3_bind_blob(WriteTile2DStmt, 4, data, size, SQLITE_STATIC);
    if (sqlite3_step(WriteTile2DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write tile: {}, {}, {}", x, y, sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(WriteTile2DStmt);
    return success;
}

bool SavepointDriverSQLite3::Write(const void* data, int size, int x, int y, int z, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    sqlite3_bind_int(WriteTile3DStmt, 1, x);
    sqlite3_bind_int(WriteTile3DStmt, 2, y);
    sqlite3_bind_int(WriteTile3DStmt, 3, z);
    sqlite3_bind_int(WriteTile3DStmt, 4, level);
    sqlite3_bind_blob(WriteTile3DStmt, 5, data, size, SQLITE_STATIC);
    if (sqlite3_step(WriteTile3DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write tile: {}, {}, {}, {}", x, y, z, sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(WriteTile3DStmt);
    return success;
}

void SavepointDriverSQLite3::Read(const SavepointReadDataFunction& function)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    std::vector<uint8_t> data;
    bool found = sqlite3_step(ReadStmt) == SQLITE_ROW;
    if (found)
    {
        data = GetData(ReadStmt, 0);
    }
    sqlite3_reset(ReadStmt);
    if (found)
    {
        function(data.data(), data.size());
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadLevelStmt, 1, level);
    std::vector<uint8_t> data;
    bool found = sqlite3_step(ReadLevelStmt) == SQLITE_ROW;
    if (found)
    {
        data = GetData(ReadLevelStmt, 0);
    }
    sqlite3_reset(ReadLevelStmt);
    if (found)
    {
        function(data.data(), data.size());
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllEntityDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    struct Row
    {
        int64_t Id;
        std::vector<uint8_t> Data;
    };
    std::vector<Row> rows;
    sqlite3_bind_int(ReadEntitiesStmt, 1, level);
    while (sqlite3_step(ReadEntitiesStmt) == SQLITE_ROW)
    {
        Row row;
        row.Id = sqlite3_column_int64(ReadEntitiesStmt, 0);
        row.Data = GetData(ReadEntitiesStmt, 1);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(ReadEntitiesStmt);
    for (Row& row : rows)
    {
        function(row.Data.data(), row.Data.size(), row.Id);
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile2DDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    struct Row
    {
        int X;
        int Y;
        std::vector<uint8_t> Data;
    };
    std::vector<Row> rows;
    sqlite3_bind_int(ReadAllTiles2DStmt, 1, level);
    while (sqlite3_step(ReadAllTiles2DStmt) == SQLITE_ROW)
    {
        Row row;
        row.X = sqlite3_column_int(ReadAllTiles2DStmt, 0);
        row.Y = sqlite3_column_int(ReadAllTiles2DStmt, 1);
        row.Data = GetData(ReadAllTiles2DStmt, 2);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(ReadAllTiles2DStmt);
    for (Row& row : rows)
    {
        function(row.Data.data(), row.Data.size(), row.X, row.Y);
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile3DDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    struct Row
    {
        int X;
        int Y;
        int Z;
        std::vector<uint8_t> Data;
    };
    std::vector<Row> rows;
    sqlite3_bind_int(ReadAllTiles3DStmt, 1, level);
    while (sqlite3_step(ReadAllTiles3DStmt) == SQLITE_ROW)
    {
        Row row;
        row.X = sqlite3_column_int(ReadAllTiles3DStmt, 0);
        row.Y = sqlite3_column_int(ReadAllTiles3DStmt, 1);
        row.Z = sqlite3_column_int(ReadAllTiles3DStmt, 2);
        row.Data = GetData(ReadAllTiles3DStmt, 3);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(ReadAllTiles3DStmt);
    for (Row& row : rows)
    {
        function(row.Data.data(), row.Data.size(), row.X, row.Y, row.Z);
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadTile2DStmt, 1, level);
    sqlite3_bind_int(ReadTile2DStmt, 2, x);
    sqlite3_bind_int(ReadTile2DStmt, 3, y);
    std::vector<uint8_t> data;
    bool found = sqlite3_step(ReadTile2DStmt) == SQLITE_ROW;
    if (found)
    {
        data = GetData(ReadTile2DStmt, 0);
    }
    sqlite3_reset(ReadTile2DStmt);
    if (found)
    {
        function(data.data(), data.size());
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadTile3DStmt, 1, level);
    sqlite3_bind_int(ReadTile3DStmt, 2, x);
    sqlite3_bind_int(ReadTile3DStmt, 3, y);
    sqlite3_bind_int(ReadTile3DStmt, 4, z);
    std::vector<uint8_t> data;
    bool found = sqlite3_step(ReadTile3DStmt) == SQLITE_ROW;
    if (found)
    {
        data = GetData(ReadTile3DStmt, 0);
    }
    sqlite3_reset(ReadTile3DStmt);
    if (found)
    {
        function(data.data(), data.size());
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllLevelsFunction& function)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    std::vector<int> levels;
    while (sqlite3_step(ReadLevelsStmt) == SQLITE_ROW)
    {
        levels.push_back(sqlite3_column_int(ReadLevelsStmt, 0));
    }
    sqlite3_reset(ReadLevelsStmt);
    for (int level : levels)
    {
        function(level);
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile2DDataFunction& function, int level, int minX, int minY, int maxX, int maxY)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    struct Row
    {
        int X;
        int Y;
        std::vector<uint8_t> Data;
    };
    std::vector<Row> rows;
    sqlite3_bind_int(ReadRegionTiles2DStmt, 1, level);
    sqlite3_bind_int(ReadRegionTiles2DStmt, 2, minX);
    sqlite3_bind_int(ReadRegionTiles2DStmt, 3, maxX);
    sqlite3_bind_int(ReadRegionTiles2DStmt, 4, minY);
    sqlite3_bind_int(ReadRegionTiles2DStmt, 5, maxY);
    while (sqlite3_step(ReadRegionTiles2DStmt) == SQLITE_ROW)
    {
        Row row;
        row.X = sqlite3_column_int(ReadRegionTiles2DStmt, 0);
        row.Y = sqlite3_column_int(ReadRegionTiles2DStmt, 1);
        row.Data = GetData(ReadRegionTiles2DStmt, 2);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(ReadRegionTiles2DStmt);
    for (Row& row : rows)
    {
        function(row.Data.data(), row.Data.size(), row.X, row.Y);
    }
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile3DDataFunction& function, int level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    struct Row
    {
        int X;
        int Y;
        int Z;
        std::vector<uint8_t> Data;
    };
    std::vector<Row> rows;
    sqlite3_bind_int(ReadRegionTiles3DStmt, 1, level);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 2, minX);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 3, maxX);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 4, minY);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 5, maxY);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 6, minZ);
    sqlite3_bind_int(ReadRegionTiles3DStmt, 7, maxZ);
    while (sqlite3_step(ReadRegionTiles3DStmt) == SQLITE_ROW)
    {
        Row row;
        row.X = sqlite3_column_int(ReadRegionTiles3DStmt, 0);
        row.Y = sqlite3_column_int(ReadRegionTiles3DStmt, 1);
        row.Z = sqlite3_column_int(ReadRegionTiles3DStmt, 2);
        row.Data = GetData(ReadRegionTiles3DStmt, 3);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(ReadRegionTiles3DStmt);
    for (Row& row : rows)
    {
        function(row.Data.data(), row.Data.size(), row.X, row.Y, row.Z);
    }
}

void SavepointDriverSQLite3::ReadEntity(const SavepointReadDataFunction& function, int64_t id)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int64(ReadEntityStmt, 1, id);
    std::vector<uint8_t> data;
    bool found = sqlite3_step(ReadEntityStmt) == SQLITE_ROW;
    if (found)
    {
        data = GetData(ReadEntityStmt, 0);
    }
    sqlite3_reset(ReadEntityStmt);
    if (found)
    {
        function(data.data(), data.size());
    }
}

bool SavepointDriverSQLite3::Contains(int64_t id)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int64(ContainsEntityStmt, 1, id);
    bool success = sqlite3_step(ContainsEntityStmt) == SQLITE_ROW;
    sqlite3_reset(ContainsEntityStmt);
    return success;
}

bool SavepointDriverSQLite3::Contains(int x, int y, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ContainsTile2DStmt, 1, level);
    sqlite3_bind_int(ContainsTile2DStmt, 2, x);
    sqlite3_bind_int(ContainsTile2DStmt, 3, y);
    bool success = sqlite3_step(ContainsTile2DStmt) == SQLITE_ROW;
    sqlite3_reset(ContainsTile2DStmt);
    return success;
}

bool SavepointDriverSQLite3::Contains(int x, int y, int z, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ContainsTile3DStmt, 1, level);
    sqlite3_bind_int(ContainsTile3DStmt, 2, x);
    sqlite3_bind_int(ContainsTile3DStmt, 3, y);
    sqlite3_bind_int(ContainsTile3DStmt, 4, z);
    bool success = sqlite3_step(ContainsTile3DStmt) == SQLITE_ROW;
    sqlite3_reset(ContainsTile3DStmt);
    return success;
}

int SavepointDriverSQLite3::CountEntities(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    int count = 0;
    sqlite3_bind_int(CountEntitiesStmt, 1, level);
    if (sqlite3_step(CountEntitiesStmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(CountEntitiesStmt, 0);
    }
    else
    {
        SavepointLog(std::format("Failed to count: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(CountEntitiesStmt);
    return count;
}

int SavepointDriverSQLite3::CountTiles2D(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    int count = 0;
    sqlite3_bind_int(CountTiles2DStmt, 1, level);
    if (sqlite3_step(CountTiles2DStmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(CountTiles2DStmt, 0);
    }
    else
    {
        SavepointLog(std::format("Failed to count: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(CountTiles2DStmt);
    return count;
}

int SavepointDriverSQLite3::CountTiles3D(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    int count = 0;
    sqlite3_bind_int(CountTiles3DStmt, 1, level);
    if (sqlite3_step(CountTiles3DStmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(CountTiles3DStmt, 0);
    }
    else
    {
        SavepointLog(std::format("Failed to count: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(CountTiles3DStmt);
    return count;
}

bool SavepointDriverSQLite3::Delete()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = false;
    if (sqlite3_step(DeleteSingletonStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete singleton: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteSingletonStmt);
    return success;
}

bool SavepointDriverSQLite3::Delete(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(DeleteLevelSingletonStmt, 1, level);
    bool success = false;
    if (sqlite3_step(DeleteLevelSingletonStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete level singleton: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteLevelSingletonStmt);
    return success;
}

bool SavepointDriverSQLite3::Delete(int64_t id)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int64(DeleteEntityStmt, 1, id);
    bool success = false;
    if (sqlite3_step(DeleteEntityStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete entity: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteEntityStmt);
    return success;
}

bool SavepointDriverSQLite3::Delete(int x, int y, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(DeleteTile2DStmt, 1, level);
    sqlite3_bind_int(DeleteTile2DStmt, 2, x);
    sqlite3_bind_int(DeleteTile2DStmt, 3, y);
    bool success = false;
    if (sqlite3_step(DeleteTile2DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete tile 2d: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteTile2DStmt);
    return success;
}

bool SavepointDriverSQLite3::Delete(int x, int y, int z, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(DeleteTile3DStmt, 1, level);
    sqlite3_bind_int(DeleteTile3DStmt, 2, x);
    sqlite3_bind_int(DeleteTile3DStmt, 3, y);
    sqlite3_bind_int(DeleteTile3DStmt, 4, z);
    bool success = false;
    if (sqlite3_step(DeleteTile3DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete tile 3d: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        success = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteTile3DStmt);
    return success;
}

bool SavepointDriverSQLite3::DeleteLevel(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = false;
    sqlite3_bind_int(DeleteLevelSingletonStmt, 1, level);
    sqlite3_bind_int(DeleteLevelEntitiesStmt, 1, level);
    sqlite3_bind_int(DeleteLevelTiles2DStmt, 1, level);
    sqlite3_bind_int(DeleteLevelTiles3DStmt, 1, level);
    bool deleted = false;
    if (sqlite3_step(DeleteLevelSingletonStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete level singleton: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        deleted = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteLevelSingletonStmt);
    success |= deleted;
    deleted = false;
    if (sqlite3_step(DeleteLevelEntitiesStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete level entities: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        deleted = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteLevelEntitiesStmt);
    success |= deleted;
    deleted = false;
    if (sqlite3_step(DeleteLevelTiles2DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete level tiles 2d: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        deleted = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteLevelTiles2DStmt);
    success |= deleted;
    deleted = false;
    if (sqlite3_step(DeleteLevelTiles3DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete level tiles 3d: {}", sqlite3_errmsg(Handle)));
    }
    else
    {
        deleted = sqlite3_changes(Handle) > 0;
    }
    sqlite3_reset(DeleteLevelTiles3DStmt);
    success |= deleted;
    return success;
}

bool SavepointDriverSQLite3::Close()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_finalize(WriteStatusStmt);
    sqlite3_finalize(WriteStmt);
    sqlite3_finalize(WriteLevelStmt);
    sqlite3_finalize(InsertEntityStmt);
    sqlite3_finalize(UpdateEntityStmt);
    sqlite3_finalize(WriteTile2DStmt);
    sqlite3_finalize(WriteTile3DStmt);
    sqlite3_finalize(ReadStatusStmt);
    sqlite3_finalize(ReadStmt);
    sqlite3_finalize(ReadLevelStmt);
    sqlite3_finalize(ReadEntityStmt);
    sqlite3_finalize(ReadEntitiesStmt);
    sqlite3_finalize(ReadTile2DStmt);
    sqlite3_finalize(ReadTile3DStmt);
    sqlite3_finalize(ReadAllTiles2DStmt);
    sqlite3_finalize(ReadAllTiles3DStmt);
    sqlite3_finalize(ReadLevelsStmt);
    sqlite3_finalize(ReadRegionTiles2DStmt);
    sqlite3_finalize(ReadRegionTiles3DStmt);
    sqlite3_finalize(ContainsEntityStmt);
    sqlite3_finalize(ContainsTile2DStmt);
    sqlite3_finalize(ContainsTile3DStmt);
    sqlite3_finalize(CountEntitiesStmt);
    sqlite3_finalize(CountTiles2DStmt);
    sqlite3_finalize(CountTiles3DStmt);
    sqlite3_finalize(DeleteSingletonStmt);
    sqlite3_finalize(DeleteLevelSingletonStmt);
    sqlite3_finalize(DeleteEntityStmt);
    sqlite3_finalize(DeleteTile2DStmt);
    sqlite3_finalize(DeleteTile3DStmt);
    sqlite3_finalize(DeleteLevelEntitiesStmt);
    sqlite3_finalize(DeleteLevelTiles2DStmt);
    sqlite3_finalize(DeleteLevelTiles3DStmt);
    sqlite3_finalize(ClearLevelsStmt);
    sqlite3_finalize(ClearEntitiesStmt);
    sqlite3_finalize(ClearTiles2DStmt);
    sqlite3_finalize(ClearTiles3DStmt);
    bool success = true;
    if (sqlite3_close(Handle) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to close database: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    Handle = nullptr;
    return success;
}

bool SavepointDriverSQLite3::Save()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    if (sqlite3_step(WriteStatusStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write status: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(WriteStatusStmt);
    if (!success)
    {
        return false;
    }
    if (sqlite3_exec(Handle, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to end transaction: {}", sqlite3_errmsg(Handle)));
        return false;
    }
    if (sqlite3_exec(Handle, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to take the write lock again (is another writer open?): {}", sqlite3_errmsg(Handle)));
    }
    return true;
}

bool SavepointDriverSQLite3::Rollback()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    if (!sqlite3_get_autocommit(Handle) && sqlite3_exec(Handle, "ROLLBACK;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to rollback transaction: {}", sqlite3_errmsg(Handle)));
        return false;
    }
    if (sqlite3_exec(Handle, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to take the write lock (is another writer open?): {}", sqlite3_errmsg(Handle)));
        return false;
    }
    return true;
}

bool SavepointDriverSQLite3::Backup(const std::string& path)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3* source = nullptr;
    sqlite3* destination = nullptr;
    bool success = false;
    if (sqlite3_open_v2(sqlite3_db_filename(Handle, "main"), &source, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to open backup source: {}", sqlite3_errmsg(source)));
    }
    else if (sqlite3_open_v2(path.c_str(), &destination, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to open backup destination: {}, {}", path, sqlite3_errmsg(destination)));
    }
    else if (sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main"))
    {
        success = sqlite3_backup_step(backup, -1) == SQLITE_DONE;
        sqlite3_backup_finish(backup);
        if (!success)
        {
            SavepointLog(std::format("Failed to backup: {}, {}", path, sqlite3_errmsg(destination)));
        }
    }
    else
    {
        SavepointLog(std::format("Failed to start backup: {}, {}", path, sqlite3_errmsg(destination)));
    }
    sqlite3_close(destination);
    sqlite3_close(source);
    return success;
}

bool SavepointDriverSQLite3::Clear()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = true;
    if (sqlite3_step(ClearLevelsStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear levels: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(ClearLevelsStmt);
    if (sqlite3_step(ClearEntitiesStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear entities: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(ClearEntitiesStmt);
    if (sqlite3_step(ClearTiles2DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear tiles 2d: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(ClearTiles2DStmt);
    if (sqlite3_step(ClearTiles3DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear tiles 3d: {}", sqlite3_errmsg(Handle)));
        success = false;
    }
    sqlite3_reset(ClearTiles3DStmt);
    return success;
}
