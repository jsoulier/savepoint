#include <savepoint/savepoint.hpp>

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct Entity : SavepointEntity
{
    std::string Name;
    std::vector<int> Inventory;
    std::optional<float> Health;

    Entity() = default;
    Entity(std::string name, std::vector<int> inventory, std::optional<float> health)
        : Name{std::move(name)}
        , Inventory{std::move(inventory)}
        , Health{health}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Name);
        visitor(Inventory);
        visitor(Health);
    }

    bool operator==(const Entity& other) const
    {
        return Name == other.Name && Inventory == other.Inventory && Health == other.Health;
    }
};

int main()
{
    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", SavepointVersion{});

    Entity inEntity{"Player", {1, 2, 3}, 0.5f};
    savepoint.Write(inEntity, 0);
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        assert(outEntity == inEntity);
    }, 0);

    savepoint.Close();
    return 0;
}
