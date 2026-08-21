// [6_spatial_types]
#include <savepoint/savepoint.hpp>

#include <filesystem>
#include <random>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

enum TileType
{
    TileTypeGrass,
    TileTypeDirt,
    TileTypeStone,
};

struct Tile
{
    TileType Type;
    int X;
    int Y;
    int Z;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Type);
        visitor(X);
        visitor(Y);
        visitor(Z);
    }

    bool operator==(const Tile& other) const
    {
        return Type == other.Type && X == other.X && Y == other.Y && Z == other.Z;
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, 2);

    std::array<std::array<Tile, 32>, 32> inTiles;
    for (int x = 0; x < 32; x++)
    for (int y = 0; y < 32; y++)
    {
        inTiles[x][y].Type = TileType(distribution(generator));
        inTiles[x][y].X = x;
        inTiles[x][y].Y = y;
        inTiles[x][y].Z = 0;
        savepoint.Write(inTiles[x][y], x, y, 0);
    }

    int reads = 0;
    savepoint.Read<Tile>([&](Tile& outTile, int x, int y)
    {
        ASSERT(outTile == inTiles[x][y]);
        reads++;
    }, 0);
    ASSERT(reads == 32 * 32);

    for (int x = 0; x < 32; x++)
    for (int y = 0; y < 32; y++)
    {
        Tile tile;
        ASSERT(savepoint.Read(tile, x, y, 0));
        ASSERT(tile.X == x);
        ASSERT(tile.Y == y);
        ASSERT(tile.Z == 0);
    }

    Tile overwritten{TileTypeStone, 10, 20, 0};
    savepoint.Write(overwritten, 10, 20, 0);
    Tile tile;
    ASSERT(savepoint.Read(tile, 10, 20, 0));
    ASSERT(tile == overwritten);
    ASSERT(!savepoint.Read(tile, -1, -1, 0));

    savepoint.Clear();
    ASSERT(!savepoint.Read(tile, 10, 20, 0));

    for (int x = 0; x < 8; x++)
    for (int y = 0; y < 8; y++)
    for (int z = 0; z < 8; z++)
    {
        Tile inTile{TileType((x + y + z) % 3), x, y, z};
        savepoint.Write(inTile, x, y, z, 0);
    }

    reads = 0;
    savepoint.Read<Tile>([&](Tile& outTile, int x, int y, int z)
    {
        ASSERT(outTile.Type == TileType((x + y + z) % 3));
        ASSERT(outTile.X == x);
        ASSERT(outTile.Y == y);
        ASSERT(outTile.Z == z);
        reads++;
    }, 0);
    ASSERT(reads == 8 * 8 * 8);
    ASSERT(savepoint.Read(tile, 1, 2, 3, 0));
    Tile expected{TileTypeGrass, 1, 2, 3};
    ASSERT(tile == expected);
    ASSERT(!savepoint.Read(tile, -1, -1, -1, 0));

    savepoint.Clear();
    ASSERT(!savepoint.Read(tile, 1, 2, 3, 0));
    savepoint.Close();
    return 0;
}
// [6_spatial_types]
