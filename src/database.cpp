#include <savepoint/profile.hpp>
#include <savepoint/database.hpp>

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#ifdef SAVEPOINT_NULL
#include "null.hpp"
#endif
#ifdef SAVEPOINT_SQLITE3
#include "sqlite3.hpp"
#endif

Savepoint::Savepoint()
    : Flags{SavepointFlags::None}
{
}

Savepoint::~Savepoint()
{
    Close();
}

Savepoint::Savepoint(Savepoint&& other)
    : Savepoint()
{
    *this = std::move(other);
}

Savepoint& Savepoint::operator=(Savepoint&& other)
{
    if (this != &other)
    {
        Close();
        Version = std::exchange(other.Version, SavepointVersion{});
        Flags = std::exchange(other.Flags, SavepointFlags::None);
        Driver = std::move(other.Driver);
    }
    return *this;
}

SavepointStatus Savepoint::Open(const std::string& path, SavepointVersion version, SavepointFlags flags)
{
    SAVEPOINT_PROFILE_SCOPE();
    Close();
    Driver.reset();
    if ((flags & SavepointFlags::Write) == SavepointFlags::Write)
    {
        flags |= SavepointFlags::Read;
    }
    if ((flags & SavepointFlags::Read) != SavepointFlags::Read)
    {
        SavepointLog("Tried to open without SavepointFlags::Read or SavepointFlags::Write");
        return SavepointStatus::Failed;
    }
#ifdef SAVEPOINT_SQLITE3
    if (!Driver && (flags & SavepointFlags::SQLite3) == SavepointFlags::SQLite3)
    {
        Driver = std::make_unique<SavepointDriverSQLite3>();
    }
#endif
#ifdef SAVEPOINT_NULL
    if (!Driver && (flags & SavepointFlags::Null) == SavepointFlags::Null)
    {
        Driver = std::make_unique<SavepointDriverNull>();
    }
#endif
    if (!Driver)
    {
        SavepointLog(std::format("No supported driver in flags: {}", std::to_underlying(flags)));
        return SavepointStatus::Failed;
    }
    Version = version;
    Flags = flags;
    SavepointStatus status = Driver->Open(path, flags);
    if (status == SavepointStatus::Failed && Driver->IsOpen())
    {
        Driver->Close();
    }
    return status;
}

bool Savepoint::IsOpen() const
{
    return Driver && Driver->IsOpen();
}

std::string_view Savepoint::GetDriverName() const
{
    if (Driver)
    {
        return Driver->GetName();
    }
    return "";
}

std::vector<int> Savepoint::GetLevels()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
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

bool Savepoint::Contains(SavepointID id)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead() || !id.IsValid())
    {
        return false;
    }
    return Driver->Contains(id.Value);
}

bool Savepoint::Contains(int x, int y, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return false;
    }
    return Driver->Contains(x, y, level);
}

bool Savepoint::Contains(int x, int y, int z, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return false;
    }
    return Driver->Contains(x, y, z, level);
}

int Savepoint::GetEntityCount(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return 0;
    }
    return Driver->CountEntities(level);
}

int Savepoint::GetTile2DCount(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return 0;
    }
    return Driver->CountTiles2D(level);
}

int Savepoint::GetTile3DCount(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return 0;
    }
    return Driver->CountTiles3D(level);
}

bool Savepoint::Delete()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Delete();
}

bool Savepoint::Delete(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Delete(level);
}

bool Savepoint::Delete(SavepointID id)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite() || !id.IsValid())
    {
        return false;
    }
    return Driver->Delete(id.Value);
}

bool Savepoint::Delete(int x, int y, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Delete(x, y, level);
}

bool Savepoint::Delete(int x, int y, int z, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Delete(x, y, z, level);
}

bool Savepoint::DeleteLevel(int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->DeleteLevel(level);
}

bool Savepoint::Close()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!Driver || !Driver->IsOpen())
    {
        return false;
    }
    return Driver->Close();
}

bool Savepoint::Save()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Save();
}

bool Savepoint::Rollback()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Rollback();
}

bool Savepoint::Backup(const std::string& path)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return false;
    }
    return Driver->Backup(path);
}

bool Savepoint::Clear()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanWrite())
    {
        return false;
    }
    return Driver->Clear();
}

#ifdef SAVEPOINT_DEBUGGER

bool Savepoint::ReadDebug(std::vector<SavepointDebugNode>& nodes)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return false;
    }
    SavepointVisitor visitor;
    bool success = false;
    Driver->Read([&visitor, &nodes, &success](const void* data, int size)
    {
        visitor.Begin(data, size);
        nodes = visitor.GetDebugNodes();
        success = true;
    });
    return success;
}

bool Savepoint::ReadDebug(std::vector<SavepointDebugNode>& nodes, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return false;
    }
    SavepointVisitor visitor;
    bool success = false;
    Driver->Read([&visitor, &nodes, &success](const void* data, int size)
    {
        visitor.Begin(data, size);
        nodes = visitor.GetDebugNodes();
        success = true;
    }, level);
    return success;
}

void Savepoint::ReadDebug(const SavepointDebugEntityFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return;
    }
    SavepointVisitor visitor;
    Driver->Read([&visitor, &function](const void* data, int size, int64_t id)
    {
        visitor.Begin(data, size);
        function(visitor.GetDebugNodes(), SavepointID{id});
        return true;
    }, level);
}

void Savepoint::ReadDebug(const SavepointDebugTile2DFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return;
    }
    SavepointVisitor visitor;
    Driver->Read([&visitor, &function](const void* data, int size, int x, int y)
    {
        visitor.Begin(data, size);
        function(visitor.GetDebugNodes(), x, y);
        return true;
    }, level);
}

void Savepoint::ReadDebug(const SavepointDebugTile3DFunction& function, int level)
{
    SAVEPOINT_PROFILE_SCOPE();
    if (!CanRead())
    {
        return;
    }
    SavepointVisitor visitor;
    Driver->Read([&visitor, &function](const void* data, int size, int x, int y, int z)
    {
        visitor.Begin(data, size);
        function(visitor.GetDebugNodes(), x, y, z);
        return true;
    }, level);
}

#endif

bool Savepoint::CanRead() const
{
    if (!Driver || !Driver->IsOpen())
    {
        return false;
    }
    if ((Flags & SavepointFlags::Read) != SavepointFlags::Read)
    {
        SavepointLog("Tried to read from a non-readable connection");
        return false;
    }
    return true;
}

bool Savepoint::CanWrite() const
{
    if (!Driver || !Driver->IsOpen())
    {
        return false;
    }
    if ((Flags & SavepointFlags::Write) != SavepointFlags::Write)
    {
        SavepointLog("Tried to write to a non-writable connection");
        return false;
    }
    return true;
}
