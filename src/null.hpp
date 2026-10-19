#pragma once

#include <savepoint/savepoint.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class SavepointDriverNull : public ISavepointDriver
{
public:
    SavepointDriverNull();
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
    std::atomic<int64_t> ID;
};
