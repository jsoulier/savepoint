#pragma once

#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "mutex.hpp"

typedef struct sqlite3 sqlite;
typedef struct sqlite3_stmt sqlite_stmt;

class SavepointDriverSQLite3 : public ISavepointDriver
{
public:
    SavepointDriverSQLite3();
    SavepointStatus Open(std::string_view path, bool threadSafe, int maxWait) override;
    bool IsOpen() override;
    bool Write(const void* data, int size) override;
    bool Write(const void* data, int size, int level) override;
    int Insert(const void* data, int size, int level) override;
    bool Update(const void* data, int size, int id, int level) override;
    bool Write(const void* data, int size, int x, int y, int level) override;
    bool Write(const void* data, int size, int x, int y, int z, int level) override;
    void Read(const SavepointReadDataFunction& function) override;
    void Read(const SavepointReadDataFunction& function, int level) override;
    void Read(const SavepointReadAllEntityDataFunction& function, int level) override;
    void Read(const SavepointReadAllTile2DDataFunction& function, int level) override;
    void Read(const SavepointReadAllTile3DDataFunction& function, int level) override;
    void Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y) override;
    void Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z) override;
    void Read(const SavepointReadAllLevelsFunction& function) override;
    void Delete(int id) override;
    void Close() override;
    void Save() override;
    void Clear() override;

private:
    SavepointMutex Mutex;
    sqlite3* Handle;
    sqlite3_stmt* WriteStatusStmt;
    sqlite3_stmt* WriteStmt;
    sqlite3_stmt* WriteLevelStmt;
    sqlite3_stmt* InsertEntityStmt;
    sqlite3_stmt* UpdateEntityStmt;
    sqlite3_stmt* WriteTile2DStmt;
    sqlite3_stmt* WriteTile3DStmt;
    sqlite3_stmt* ReadStatusStmt;
    sqlite3_stmt* ReadStmt;
    sqlite3_stmt* ReadLevelStmt;
    sqlite3_stmt* ReadEntitiesStmt;
    sqlite3_stmt* ReadAllTiles2DStmt;
    sqlite3_stmt* ReadAllTiles3DStmt;
    sqlite3_stmt* ReadTile2DStmt;
    sqlite3_stmt* ReadTile3DStmt;
    sqlite3_stmt* ReadLevelsStmt;
    sqlite3_stmt* DeleteEntityStmt;
    sqlite3_stmt* ClearLevelsStmt;
    sqlite3_stmt* ClearEntitiesStmt;
    sqlite3_stmt* ClearTiles2DStmt;
    sqlite3_stmt* ClearTiles3DStmt;
};
