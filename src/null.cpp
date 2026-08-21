#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "null.hpp"

SavepointStatus SavepointDriverNull::Open(std::string_view path, bool threadSafe, int maxWait)
{
    return SavepointStatus::New;
}

bool SavepointDriverNull::IsOpen()
{
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size)
{
    return false;
}

bool SavepointDriverNull::Write(const void* data, int size, int level)
{
    return false;
}

int SavepointDriverNull::Insert(const void* data, int size, int level)
{
    return SavepointID::kInvalidID; 
}

bool SavepointDriverNull::Update(const void* data, int size, int id, int level)
{
    return false;
}

bool SavepointDriverNull::Write(const void* data, int size, int x, int y, int level)
{
    return false;
}

bool SavepointDriverNull::Write(const void* data, int size, int x, int y, int z, int level)
{
    return false;
}

void SavepointDriverNull::Read(const SavepointReadDataFunction& function)
{
}

void SavepointDriverNull::Read(const SavepointReadDataFunction& function, int level)
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

void SavepointDriverNull::Delete(int id)
{
}

void SavepointDriverNull::Close()
{
}

void SavepointDriverNull::Save()
{
}

void SavepointDriverNull::Clear()
{
}
