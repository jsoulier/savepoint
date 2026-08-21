// [7_levels]
#include <savepoint/savepoint.hpp>

#include <algorithm>
#include <filesystem>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    void Visit(SavepointVisitor& visitor) {}
};

struct Tile
{
    void Visit(SavepointVisitor& visitor) {}
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);

    Entity entity1;
    savepoint.Write(entity1, 0);
    savepoint.Write(entity1, 1);
    savepoint.Write(entity1, 2);
    std::vector<int> inLevels = {2};
    std::vector<int> outLevels = savepoint.GetLevels();
    ASSERT(outLevels == inLevels);

    Entity entity2;
    Entity entity3;
    savepoint.Write(entity2, 3);
    savepoint.Write(entity3, 4);
    inLevels = {2, 3, 4};
    outLevels = savepoint.GetLevels();
    std::sort(outLevels.begin(), outLevels.end());
    ASSERT(outLevels == inLevels);

    Tile tile;
    savepoint.Write(tile, 0, 0, 0);
    savepoint.Write(tile, 1, 0, 0);
    savepoint.Write(tile, 0, 1, 0);
    savepoint.Write(tile, 0, 0, 4);
    savepoint.Write(tile, 0, 0, 5);
    savepoint.Write(tile, 0, 0, 5);
    savepoint.Write(tile, 0, 0, 6);
    inLevels = {0, 2, 3, 4, 5, 6};
    outLevels = savepoint.GetLevels();
    std::sort(outLevels.begin(), outLevels.end());
    ASSERT(outLevels == inLevels);

    savepoint.Delete(entity1);
    savepoint.Delete(entity2);
    savepoint.Delete(entity3);
    inLevels = {0, 4, 5, 6};
    outLevels = savepoint.GetLevels();
    std::sort(outLevels.begin(), outLevels.end());
    ASSERT(outLevels == inLevels);

    savepoint.Close();
    return 0;
}
// [7_levels]
