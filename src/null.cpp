#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "null.hpp"

SavepointDriverNull::SavepointDriverNull()
    : ISavepointDriver()
    , ID{0}
{
}

std::string_view SavepointDriverNull::GetName() const
{
    return "null";
}

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
    return true;
}

bool SavepointDriverNull::Write(const void* data, int size, int level)
{
    return true;
}

int SavepointDriverNull::Insert(const void* data, int size, int level)
{
    return ID.fetch_add(1);
}

bool SavepointDriverNull::Update(const void* data, int size, int id, int level)
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
