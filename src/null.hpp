#pragma once

#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

class SavepointDriverNull : public ISavepointDriver
{
public:
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
};
