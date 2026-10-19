#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "null.hpp"

SavepointDriverNull::SavepointDriverNull()
    : ISavepointDriver()
    , ID{1}
{
}

std::string_view SavepointDriverNull::GetName() const
{
    return "null";
}

SavepointStatus SavepointDriverNull::Open(const std::string& path, SavepointFlags flags)
{
    return SavepointStatus::New;
}

bool SavepointDriverNull::IsOpen()
{
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size)
{
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size, int level)
{
    return true;
}

int64_t SavepointDriverNull::Insert(const void* data, int size, int level)
{
    return ID.fetch_add(1);
}

bool SavepointDriverNull::Update(const void* data, int size, int64_t id, int level)
{
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size, int x, int y, int level)
{
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size, int x, int y, int z, int level)
{
    return true;
}

void SavepointDriverNull::Read(const SavepointReadDataFunction& function)
{
}

void SavepointDriverNull::Read(const SavepointReadDataFunction& function, int level)
{
}

void SavepointDriverNull::ReadEntity(const SavepointReadDataFunction& function, int64_t id)
{
}

void SavepointDriverNull::Read(const SavepointReadAllEntityDataFunction& function, int level)
{
}

void SavepointDriverNull::Read(const SavepointReadAllTile2DDataFunction& function, int level)
{
}

void SavepointDriverNull::Read(const SavepointReadAllTile3DDataFunction& function, int level)
{
}

void SavepointDriverNull::Read(const SavepointReadTile2DDataFunction& function, int level, int x, int y)
{
}

void SavepointDriverNull::Read(const SavepointReadTile3DDataFunction& function, int level, int x, int y, int z)
{
}

void SavepointDriverNull::Read(const SavepointReadAllLevelsFunction& function)
{
}

void SavepointDriverNull::Read(const SavepointReadAllTile2DDataFunction& function, int level, int minX, int minY, int maxX, int maxY)
{
}

void SavepointDriverNull::Read(const SavepointReadAllTile3DDataFunction& function, int level, int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
{
}

bool SavepointDriverNull::Contains(int64_t id)
{
    return false;
}

bool SavepointDriverNull::Contains(int x, int y, int level)
{
    return false;
}

bool SavepointDriverNull::Contains(int x, int y, int z, int level)
{
    return false;
}

int SavepointDriverNull::CountEntities(int level)
{
    return 0;
}

int SavepointDriverNull::CountTiles2D(int level)
{
    return 0;
}

int SavepointDriverNull::CountTiles3D(int level)
{
    return 0;
}

bool SavepointDriverNull::Delete()
{
    return true;
}

bool SavepointDriverNull::Delete(int level)
{
    return true;
}

bool SavepointDriverNull::Delete(int64_t id)
{
    return true;
}

bool SavepointDriverNull::Delete(int x, int y, int level)
{
    return true;
}

bool SavepointDriverNull::Delete(int x, int y, int z, int level)
{
    return true;
}

bool SavepointDriverNull::DeleteLevel(int level)
{
    return true;
}

bool SavepointDriverNull::Close()
{
    return true;
}

bool SavepointDriverNull::Save()
{
    return true;
}

bool SavepointDriverNull::Rollback()
{
    return true;
}

bool SavepointDriverNull::Backup(const std::string& path)
{
    return true;
}

bool SavepointDriverNull::Clear()
{
    return true;
}
