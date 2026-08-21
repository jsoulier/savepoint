#include <sqlite3.h>

#include <savepoint/profile.hpp>
#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>

#include "sqlite3.hpp"

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
    "CREATE INDEX IF NOT EXISTS tiles_2d_index ON tiles_2d (level);"
    "CREATE INDEX IF NOT EXISTS tiles_3d_index ON tiles_3d (level);"
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
static constexpr const char* kDeleteEntitySQL =
    "DELETE FROM entities WHERE id = ?;";
static constexpr const char* kClearLevelsSQL =
    "DELETE FROM levels;";
static constexpr const char* kClearEntitiesSQL =
    "DELETE FROM entities;";
static constexpr const char* kClearTiles2DSQL =
    "DELETE FROM tiles_2d;";
static constexpr const char* kClearTiles3DSQL =
    "DELETE FROM tiles_3d;";

static int Step(sqlite3_stmt* statement)
{
    SAVEPOINT_PROFILE_SCOPE();
    return sqlite3_step(statement);
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
    , ReadEntitiesStmt{nullptr}
    , ReadAllTiles2DStmt{nullptr}
    , ReadAllTiles3DStmt{nullptr}
    , ReadTile2DStmt{nullptr}
    , ReadTile3DStmt{nullptr}
    , ReadLevelsStmt{nullptr}
    , DeleteEntityStmt{nullptr}
    , ClearLevelsStmt{nullptr}
    , ClearEntitiesStmt{nullptr}
    , ClearTiles2DStmt{nullptr}
    , ClearTiles3DStmt{nullptr}
{
}

SavepointStatus SavepointDriverSQLite3::Open(std::string_view path, bool threadSafe, int maxWait)
{
    SAVEPOINT_PROFILE_SCOPE();
    Mutex.SetEnabled(threadSafe);
    std::scoped_lock lock{Mutex};
    constexpr int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    if (sqlite3_open_v2(path.data(), &Handle, flags, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to open database: {}, {}", path, sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_busy_timeout(Handle, maxWait) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to set busy timeout: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_exec(Handle, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to enable WAL: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_exec(Handle, "PRAGMA wal_autocheckpoint=4000;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to set wal_autocheckpoint: {}", sqlite3_errmsg(Handle)));
        return SavepointStatus::Failed;
    }
    if (sqlite3_exec(Handle, kSQL, nullptr, nullptr, nullptr) != SQLITE_OK)
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
    if (sqlite3_prepare_v2(Handle, kDeleteEntitySQL, -1, &DeleteEntityStmt, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to prepare kDeleteEntitySQL: {}", sqlite3_errmsg(Handle)));
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
    if (sqlite3_exec(Handle, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to begin transaction: {}", sqlite3_errmsg(Handle)));
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

int SavepointDriverSQLite3::Insert(const void* data, int size, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    int id = SavepointID::kInvalidID;
    sqlite3_bind_int(InsertEntityStmt, 1, level);
    sqlite3_bind_blob(InsertEntityStmt, 2, data, size, SQLITE_STATIC);
    int result = Step(InsertEntityStmt);
    if (result != SQLITE_DONE)
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

bool SavepointDriverSQLite3::Update(const void* data, int size, int id, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    bool success = false;
    sqlite3_bind_int(UpdateEntityStmt, 1, level);
    sqlite3_bind_blob(UpdateEntityStmt, 2, data, size, SQLITE_STATIC);
    sqlite3_bind_int(UpdateEntityStmt, 3, id);
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
    int result = Step(WriteTile2DStmt);
    if (result != SQLITE_DONE)
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
    if (sqlite3_step(ReadStmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(ReadStmt, 0);
        int size = sqlite3_column_bytes(ReadStmt, 0);
        function(data, size);
    }
    sqlite3_reset(ReadStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadLevelStmt, 1, level);
    if (sqlite3_step(ReadLevelStmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(ReadLevelStmt, 0);
        int size = sqlite3_column_bytes(ReadLevelStmt, 0);
        function(data, size);
    }
    sqlite3_reset(ReadLevelStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadAllEntityDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadEntitiesStmt, 1, level);
    while (sqlite3_step(ReadEntitiesStmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(ReadEntitiesStmt, 0);
        const void* data = sqlite3_column_blob(ReadEntitiesStmt, 1);
        int size = sqlite3_column_bytes(ReadEntitiesStmt, 1);
        function(data, size, id);
    }
    sqlite3_reset(ReadEntitiesStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile2DDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadAllTiles2DStmt, 1, level);
    while (sqlite3_step(ReadAllTiles2DStmt) == SQLITE_ROW)
    {
        int x = sqlite3_column_int(ReadAllTiles2DStmt, 0);
        int y = sqlite3_column_int(ReadAllTiles2DStmt, 1);
        const void* data = sqlite3_column_blob(ReadAllTiles2DStmt, 2);
        int size = sqlite3_column_bytes(ReadAllTiles2DStmt, 2);
        function(data, size, x, y);
    }
    sqlite3_reset(ReadAllTiles2DStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadAllTile3DDataFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadAllTiles3DStmt, 1, level);
    while (sqlite3_step(ReadAllTiles3DStmt) == SQLITE_ROW)
    {
        int x = sqlite3_column_int(ReadAllTiles3DStmt, 0);
        int y = sqlite3_column_int(ReadAllTiles3DStmt, 1);
        int z = sqlite3_column_int(ReadAllTiles3DStmt, 2);
        const void* data = sqlite3_column_blob(ReadAllTiles3DStmt, 3);
        int size = sqlite3_column_bytes(ReadAllTiles3DStmt, 3);
        function(data, size, x, y, z);
    }
    sqlite3_reset(ReadAllTiles3DStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadTile2DStmt, 1, level);
    sqlite3_bind_int(ReadTile2DStmt, 2, x);
    sqlite3_bind_int(ReadTile2DStmt, 3, y);
    if (sqlite3_step(ReadTile2DStmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(ReadTile2DStmt, 0);
        int size = sqlite3_column_bytes(ReadTile2DStmt, 0);
        function(data, size);
    }
    sqlite3_reset(ReadTile2DStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(ReadTile3DStmt, 1, level);
    sqlite3_bind_int(ReadTile3DStmt, 2, x);
    sqlite3_bind_int(ReadTile3DStmt, 3, y);
    sqlite3_bind_int(ReadTile3DStmt, 4, z);
    if (sqlite3_step(ReadTile3DStmt) == SQLITE_ROW)
    {
        const void* data = sqlite3_column_blob(ReadTile3DStmt, 0);
        int size = sqlite3_column_bytes(ReadTile3DStmt, 0);
        function(data, size);
    }
    sqlite3_reset(ReadTile3DStmt);
}

void SavepointDriverSQLite3::Read(const SavepointReadAllLevelsFunction& function)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    while (sqlite3_step(ReadLevelsStmt) == SQLITE_ROW)
    {
        int level = sqlite3_column_int(ReadLevelsStmt, 0);
        function(level);
    }
    sqlite3_reset(ReadLevelsStmt);
}

void SavepointDriverSQLite3::Delete(int id)
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    sqlite3_bind_int(DeleteEntityStmt, 1, id);
    if (sqlite3_step(DeleteEntityStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to delete entity: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(DeleteEntityStmt);
}

void SavepointDriverSQLite3::Close()
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
    sqlite3_finalize(ReadEntitiesStmt);
    sqlite3_finalize(ReadTile2DStmt);
    sqlite3_finalize(ReadTile3DStmt);
    sqlite3_finalize(ReadAllTiles2DStmt);
    sqlite3_finalize(ReadAllTiles3DStmt);
    sqlite3_finalize(ReadLevelsStmt);
    sqlite3_finalize(DeleteEntityStmt);
    sqlite3_finalize(ClearLevelsStmt);
    sqlite3_finalize(ClearEntitiesStmt);
    sqlite3_finalize(ClearTiles2DStmt);
    sqlite3_finalize(ClearTiles3DStmt);
    sqlite3_close(Handle);
    Handle = nullptr;
}

void SavepointDriverSQLite3::Save()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    if (sqlite3_step(WriteStatusStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to write status: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(WriteStatusStmt);
    if (sqlite3_exec(Handle, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to end transaction: {}", sqlite3_errmsg(Handle)));
    }
    if (sqlite3_exec(Handle, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK)
    {
        SavepointLog(std::format("Failed to begin transaction: {}", sqlite3_errmsg(Handle)));
    }
}

void SavepointDriverSQLite3::Clear()
{
    SAVEPOINT_PROFILE_SCOPE();
    std::scoped_lock lock{Mutex};
    if (sqlite3_step(ClearLevelsStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear levels: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(ClearLevelsStmt);
    if (sqlite3_step(ClearEntitiesStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear entities: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(ClearEntitiesStmt);
    if (sqlite3_step(ClearTiles2DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear tiles 2d: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(ClearTiles2DStmt);
    if (sqlite3_step(ClearTiles3DStmt) != SQLITE_DONE)
    {
        SavepointLog(std::format("Failed to clear tiles 3d: {}", sqlite3_errmsg(Handle)));
    }
    sqlite3_reset(ClearTiles3DStmt);
}
