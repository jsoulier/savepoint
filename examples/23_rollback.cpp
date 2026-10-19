// [23_rollback]
#include <savepoint/savepoint.hpp>

#include <filesystem>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    int Health = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Health);
    }
};

struct Tile
{
    int Type = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Type);
    }
};

struct World
{
    int Day = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Day);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    ASSERT(savepoint.Rollback());

    Entity player;
    player.Health = 10;
    ASSERT(savepoint.Write(player, 0));
    Tile tile;
    tile.Type = 1;
    ASSERT(savepoint.Write(tile, 0, 0, 0));
    World world;
    world.Day = 1;
    ASSERT(savepoint.Write(world));
    ASSERT(savepoint.Save());

    player.Health = 5;
    ASSERT(savepoint.Write(player, 0));
    Entity enemy;
    ASSERT(savepoint.Write(enemy, 0));
    ASSERT(savepoint.Delete(0, 0, 0));
    ASSERT(savepoint.Write(tile, 1, 1, 0));
    world.Day = 2;
    ASSERT(savepoint.Write(world));
    ASSERT(savepoint.GetEntityCount(0) == 2);

    ASSERT(savepoint.Rollback());
    ASSERT(player.Health == 5);
    ASSERT(savepoint.Read(player, player.GetID()));
    ASSERT(player.Health == 10);
    ASSERT(savepoint.GetEntityCount(0) == 1);
    ASSERT(!savepoint.Contains(enemy.GetID()));
    ASSERT(savepoint.Contains(0, 0, 0));
    ASSERT(!savepoint.Contains(1, 1, 0));
    ASSERT(savepoint.Read(world));
    ASSERT(world.Day == 1);

    ASSERT(enemy.GetID().IsValid());
    ASSERT(savepoint.Write(enemy, 0));
    ASSERT(savepoint.GetEntityCount(0) == 2);

    ASSERT(savepoint.Save());
    ASSERT(savepoint.Close());
    status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::Existing);
    ASSERT(savepoint.GetEntityCount(0) == 2);

    ASSERT(savepoint.Rollback());
    ASSERT(savepoint.GetEntityCount(0) == 2);

    Savepoint reader;
    status = reader.Open("savepoint.sqlite3", kVersion, SavepointFlags::SQLite3 | SavepointFlags::Read);
    ASSERT(status == SavepointStatus::Existing);
    ASSERT(!reader.Rollback());
    ASSERT(reader.Close());

    ASSERT(savepoint.Close());
    return 0;
}
// [23_rollback]
