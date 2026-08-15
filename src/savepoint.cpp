#include <savepoint/savepoint.hpp>

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#include "null.hpp"
#if SAVEPOINT_SQLITE3
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
    switch (driver)
    {
    case SavepointDriver::Null:
        Driver = std::make_unique<SavepointDriverNull>();
        break;
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

void Savepoint::Close()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Close();
    }
}

void Savepoint::Save()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Save();
    }
}

void Savepoint::Clear()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Clear();
    }
}
