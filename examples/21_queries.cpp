// [21_queries]
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

struct Level
{
    int Seed = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Seed);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    Entity player;
    player.Health = 10;
    ASSERT(savepoint.Write(player, 0));
    Entity enemy;
    enemy.Health = 5;
    ASSERT(savepoint.Write(enemy, 0));
    ASSERT(savepoint.GetEntityCount(0) == 2);
    Entity outEntity;
    ASSERT(savepoint.Read(outEntity, player.GetID()));
    ASSERT(outEntity.Health == 10);
    ASSERT(outEntity.GetID() == player.GetID());
    ASSERT(savepoint.Contains(enemy.GetID()));
    ASSERT(savepoint.Delete(enemy.GetID()));
    ASSERT(!savepoint.Contains(enemy.GetID()));
    ASSERT(!savepoint.Read(outEntity, enemy.GetID()));
    ASSERT(!savepoint.Delete(enemy.GetID()));
    ASSERT(savepoint.GetEntityCount(0) == 1);

    for (int x = 0; x < 10; x++)
    {
        for (int y = 0; y < 10; y++)
        {
            Tile tile;
            tile.Type = x * 10 + y;
            ASSERT(savepoint.Write(tile, x, y, 1));
            ASSERT(savepoint.Write(tile, x, y, 0, 1));
        }
    }
    ASSERT(savepoint.GetTile2DCount(1) == 100);
    ASSERT(savepoint.GetTile3DCount(1) == 100);
    int reads = 0;
    savepoint.Read<Tile>([&](Tile& tile, int x, int y)
    {
        ASSERT(x >= 2 && x <= 4);
        ASSERT(y >= 3 && y <= 5);
        ASSERT(tile.Type == x * 10 + y);
        reads++;
    }, 2, 3, 4, 5, 1);
    ASSERT(reads == 9);
    reads = 0;
    savepoint.Read<Tile>([&](Tile& tile, int x, int y, int z)
    {
        ASSERT(x >= 0 && x <= 1);
        ASSERT(y >= 0 && y <= 1);
        ASSERT(z == 0);
        ASSERT(tile.Type == x * 10 + y);
        reads++;
    }, 0, 0, 0, 1, 1, 0, 1);
    ASSERT(reads == 4);

    ASSERT(savepoint.Contains(0, 0, 1));
    ASSERT(savepoint.Delete(0, 0, 1));
    ASSERT(!savepoint.Contains(0, 0, 1));
    ASSERT(!savepoint.Delete(0, 0, 1));
    ASSERT(savepoint.Contains(0, 0, 0, 1));
    ASSERT(savepoint.Delete(0, 0, 0, 1));
    ASSERT(!savepoint.Contains(0, 0, 0, 1));
    Level inLevel;
    inLevel.Seed = 42;
    Level outLevel;
    ASSERT(savepoint.Write(inLevel, 1));
    ASSERT(savepoint.Delete(1));
    ASSERT(!savepoint.Read(outLevel, 1));
    ASSERT(savepoint.Write(inLevel));
    ASSERT(savepoint.Delete());
    ASSERT(!savepoint.Read(outLevel));
    ASSERT(savepoint.DeleteLevel(1));
    ASSERT(savepoint.GetTile2DCount(1) == 0);
    ASSERT(savepoint.GetTile3DCount(1) == 0);
    ASSERT(savepoint.GetEntityCount(0) == 1);
    ASSERT(savepoint.Save());

    status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::Existing);

    ASSERT(savepoint.Close());
    ASSERT(!savepoint.Close());
    return 0;
}
// [21_queries]
