// [2_basic_usage]
#include <savepoint/savepoint.hpp>

#include <filesystem>

#include "assert.hpp"

// The version of your application
static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    // The data we want to serialize
    int X;
    int Y;

    Entity() = default;
    Entity(int x, int y)
        : X{x}
        , Y{y}
    {
    }

    // Any objects that use pointers or may be modified in the future should
    // implement the Visit function. The Visit function allows complex datatypes
    // to be serialized (pointers, vectors, maps, etc) and have versioning
    // to avoid breaking old saves
    void Visit(SavepointVisitor& visitor)
    {
        // Visit X and Y
        visitor(X);
        visitor(Y);
    }

    // Optional
    bool operator==(const Entity& other) const
    {
        return X == other.X && Y == other.Y;
    }
};

int main()
{
    // Clear old saves
    std::filesystem::remove("savepoint.sqlite3");

    // Open a Savepoint (only SQLite3 is supported right now)
    Savepoint savepoint;
    switch (savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion))
    {
    case SavepointStatus::Failed:
        // Failed to open for any reason
        return 1;
    case SavepointStatus::Existing:
        // Opened an existing Savepoint
        break;
    case SavepointStatus::New:
        // Opened a new Savepoint
        break;
    }

    // Create and write an entity to level 0
    Entity inEntity{1, 2};
    savepoint.Write(inEntity, 0);

    // Provide a callback to read entities from level 0
    int reads = 0;
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        ASSERT(outEntity == inEntity);
        reads++;
    }, 0);
    ASSERT(reads == 1);

    // Commit the transaction and start a new one. Savepoint::Open will now return Existing instead of New
    savepoint.Save();
    
    savepoint.Close();
    return 0;
}
// [2_basic_usage]
