#include <savepoint/profile.hpp>
#include <savepoint/database.hpp>

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#ifdef SAVEPOINT_NULL
#include "null.hpp"
#endif
#ifdef SAVEPOINT_SQLITE3
#include "sqlite3.hpp"
#endif

Savepoint::~Savepoint()
{
    if (Driver && Driver->IsOpen())
    {
        Close();
    }
}

SavepointStatus Savepoint::Open(SavepointDriver driver, std::string_view path, SavepointVersion version, bool threadSafe, int maxWait)
{
    SAVEPOINT_PROFILE_SCOPE();
    switch (driver)
    {
#ifdef SAVEPOINT_NULL
    case SavepointDriver::Null:
        Driver = std::make_unique<SavepointDriverNull>();
        break;
#endif
#ifdef SAVEPOINT_SQLITE3
    case SavepointDriver::SQLite3:
        Driver = std::make_unique<SavepointDriverSQLite3>();
        break;
#endif
    default:
        SavepointLog(std::format("Unknown driver: {}", std::to_underlying(driver)));
        return SavepointStatus::Failed;
    }
    if (!Driver)
    {
        SavepointLog(std::format("Failed to create driver: {}", std::to_underlying(driver)));
        return SavepointStatus::Failed;
    }
    Version = version;
    SavepointStatus status = Driver->Open(path, threadSafe, maxWait);
    if (status == SavepointStatus::Failed && Driver->IsOpen())
    {
        Driver->Close();
    }
    return status;
}

std::string_view Savepoint::GetDriverName() const
{
    if (Driver)
    {
        return Driver->GetName();
    }
    return "";
}

void Savepoint::Close()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (Driver && Driver->IsOpen())
    {
        Driver->Close();
    }
}

void Savepoint::Save()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (Driver && Driver->IsOpen())
    {
        Driver->Save();
    }
}

void Savepoint::Clear()
{
    SAVEPOINT_PROFILE_SCOPE();
    if (Driver && Driver->IsOpen())
    {
        Driver->Clear();
    }
}
