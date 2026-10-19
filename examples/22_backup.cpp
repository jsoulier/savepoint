// [22_backup]
#include <savepoint/savepoint.hpp>

#include <filesystem>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    int Score = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Score);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");
    std::filesystem::remove("savepoint.sqlite3-wal");
    std::filesystem::remove("savepoint.sqlite3-shm");
    std::filesystem::remove("savepoint.bak.sqlite3");
    std::filesystem::remove("savepoint.bak.sqlite3-wal");
    std::filesystem::remove("savepoint.bak.sqlite3-shm");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    Entity entity;
    entity.Score = 42;
    ASSERT(savepoint.Write(entity, 0));
    ASSERT(savepoint.Save());
    ASSERT(savepoint.Backup("savepoint.bak.sqlite3"));
    entity.Score = 100;
    ASSERT(savepoint.Write(entity, 0));
    ASSERT(savepoint.Save());
    entity = {};
    ASSERT(savepoint.Read(entity, entity.GetID()));
    ASSERT(entity.Score == 100);

    Savepoint backup;
    status = backup.Open("savepoint.bak.sqlite3", kVersion, SavepointFlags::SQLite3 | SavepointFlags::Read);
    ASSERT(status == SavepointStatus::Existing);
    ASSERT(backup.GetEntityCount(0) == 1);
    entity = {};
    ASSERT(backup.Read(entity, entity.GetID()));
    ASSERT(entity.Score == 42);
    ASSERT(backup.Close());

    ASSERT(savepoint.Close());
    return 0;
}
// [22_backup]
