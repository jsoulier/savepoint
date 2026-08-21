// [4_nested_types]
#include <savepoint/savepoint.hpp>

#include <filesystem>
#include <unordered_set>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Vector2
{
    int X;
    int Y;

    // Visit implementation is optional since Vector2 won't ever change
    void Visit(SavepointVisitor& visitor)
    {
        visitor(X);
        visitor(Y);
    }

    bool operator==(const Vector2& other) const
    {
        return X == other.X && Y == other.Y;
    }
};

enum Effect
{
    // Never change the order
    EffectStrength,
    EffectWeakness,
    EffectSwiftness,
    EffectSlowness,
    // And... be sure to add enums to the end
};

struct Item
{
    int Count;
    int Durability;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Count);
        visitor(Durability);
    }

    bool operator==(const Item& other) const
    {
        return Count == other.Count && Durability == other.Durability;
    }
};

struct EntityInventory
{
    // A container of elements with a Visit implementation
    std::vector<Item> Items;

    EntityInventory()
        : Items{{1, 50}, {2, 50}, {5, 50}}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        // Visit will be called for each element in the container
        visitor(Items);
    }

    bool operator==(const EntityInventory& other) const
    {
        return Items == other.Items;
    }
};

// Entity consists of several complex types:
// |-> std::shared_ptr<Inventory>
//   |-> std::vector<Items>
//     |-> Count
//     |-> Durability
// |-> std::unordered_set<Effects>
// |-> Position
//   |-> X
//   |-> Y
struct Entity : SavepointEntity
{
    std::shared_ptr<EntityInventory> Inventory;
    std::unordered_set<Effect> Effects;
    Vector2 Position;

    Entity()
        : Inventory{std::make_shared<EntityInventory>()}
        , Effects{EffectStrength, EffectSlowness}
        , Position{100, 200}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Inventory);
        visitor(Effects);
        visitor(Position);
    }

    bool operator==(const Entity& other) const
    {
        return *Inventory == *other.Inventory &&
            Effects == other.Effects &&
            Position == other.Position;
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);

    Entity inEntity;
    savepoint.Write(inEntity, 0);
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        ASSERT(outEntity == inEntity);
    }, 0);

    savepoint.Close();
    return 0;
}
// [4_nested_types]
