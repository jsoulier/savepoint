// [6_spatial_types]
#include <savepoint/savepoint.hpp>

#include <cassert>
#include <filesystem>
#include <random>

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
    assert(status == SavepointStatus::New);

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
        assert(outTile == inTiles[x][y]);
        reads++;
    }, 0);
    assert(reads == 32 * 32);

    for (int x = 0; x < 32; x++)
    for (int y = 0; y < 32; y++)
    {
        Tile tile;
        assert(savepoint.Read(tile, x, y, 0));
        assert(tile.X == x);
        assert(tile.Y == y);
        assert(tile.Z == 0);
    }

    Tile overwritten{TileTypeStone, 10, 20, 0};
    savepoint.Write(overwritten, 10, 20, 0);
    Tile tile;
    assert(savepoint.Read(tile, 10, 20, 0));
    assert(tile == overwritten);
    assert(!savepoint.Read(tile, -1, -1, 0));

    savepoint.Clear();
    assert(!savepoint.Read(tile, 10, 20, 0));

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
        assert(outTile.Type == TileType((x + y + z) % 3));
        assert(outTile.X == x);
        assert(outTile.Y == y);
        assert(outTile.Z == z);
        reads++;
    }, 0);
    assert(reads == 8 * 8 * 8);
    assert(savepoint.Read(tile, 1, 2, 3, 0));
    Tile expected{TileTypeGrass, 1, 2, 3};
    assert(tile == expected);
    assert(!savepoint.Read(tile, -1, -1, -1, 0));

    savepoint.Clear();
    assert(!savepoint.Read(tile, 1, 2, 3, 0));
    savepoint.Close();
    return 0;
}
// [6_spatial_types]
