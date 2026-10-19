#pragma once

#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "mutex.hpp"

typedef struct sqlite3 sqlite;
typedef struct sqlite3_stmt sqlite_stmt;

class SavepointDriverSQLite3 : public ISavepointDriver
{
public:
    SavepointDriverSQLite3();
    ~SavepointDriverSQLite3() override;
    std::string_view GetName() const override;
    SavepointStatus Open(const std::string& path, SavepointFlags flags) override;
    bool IsOpen() override;
    bool Write(const void* data, int size) override;
    bool Write(const void* data, int size, int level) override;
    int64_t Insert(const void* data, int size, int level) override;
    bool Update(const void* data, int size, int64_t id, int level) override;
    bool Write(const void* data, int size, int x, int y, int level) override;
    bool Write(const void* data, int size, int x, int y, int z, int level) override;
    void Read(const SavepointReadDataFunction& function) override;
    void Read(const SavepointReadDataFunction& function, int level) override;
    void ReadEntity(const SavepointReadDataFunction& function, int64_t id) override;
    void Read(const SavepointReadAllEntityDataFunction& function, int level) override;
    void Read(const SavepointReadAllTile2DDataFunction& function, int level) override;
    void Read(const SavepointReadAllTile3DDataFunction& function, int level) override;
    void Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y) override;
    void Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z) override;
    void Read(const SavepointReadAllLevelsFunction& function) override;
    void Read(const SavepointReadAllTile2DDataFunction& function, int level, int minX, int minY, int maxX, int maxY) override;
    void Read(const SavepointReadAllTile3DDataFunction& function, int level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ) override;
    bool Contains(int64_t id) override;
    bool Contains(int x, int y, int level) override;
    bool Contains(int x, int y, int z, int level) override;
    int CountEntities(int level) override;
    int CountTiles2D(int level) override;
    int CountTiles3D(int level) override;
    bool Delete() override;
    bool Delete(int level) override;
    bool Delete(int64_t id) override;
    bool Delete(int x, int y, int level) override;
    bool Delete(int x, int y, int z, int level) override;
    bool DeleteLevel(int level) override;
    bool Close() override;
    bool Save() override;
    bool Rollback() override;
    bool Backup(const std::string& path) override;
    bool Clear() override;

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
    sqlite3_stmt* ReadEntityStmt;
    sqlite3_stmt* ReadEntitiesStmt;
    sqlite3_stmt* ReadAllTiles2DStmt;
    sqlite3_stmt* ReadAllTiles3DStmt;
    sqlite3_stmt* ReadTile2DStmt;
    sqlite3_stmt* ReadTile3DStmt;
    sqlite3_stmt* ReadLevelsStmt;
    sqlite3_stmt* ReadRegionTiles2DStmt;
    sqlite3_stmt* ReadRegionTiles3DStmt;
    sqlite3_stmt* ContainsEntityStmt;
    sqlite3_stmt* ContainsTile2DStmt;
    sqlite3_stmt* ContainsTile3DStmt;
    sqlite3_stmt* CountEntitiesStmt;
    sqlite3_stmt* CountTiles2DStmt;
    sqlite3_stmt* CountTiles3DStmt;
    sqlite3_stmt* DeleteSingletonStmt;
    sqlite3_stmt* DeleteLevelSingletonStmt;
    sqlite3_stmt* DeleteEntityStmt;
    sqlite3_stmt* DeleteTile2DStmt;
    sqlite3_stmt* DeleteTile3DStmt;
    sqlite3_stmt* DeleteLevelEntitiesStmt;
    sqlite3_stmt* DeleteLevelTiles2DStmt;
    sqlite3_stmt* DeleteLevelTiles3DStmt;
    sqlite3_stmt* ClearLevelsStmt;
    sqlite3_stmt* ClearEntitiesStmt;
    sqlite3_stmt* ClearTiles2DStmt;
    sqlite3_stmt* ClearTiles3DStmt;
};
