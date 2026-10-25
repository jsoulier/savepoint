// [23_references]
#include <savepoint/savepoint.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

static std::unordered_map<SavepointID, std::shared_ptr<SavepointEntity>> Entities;

template<typename T>
static std::shared_ptr<T> GetEntity(SavepointID id)
{
    auto it = Entities.find(id);
    if (it == Entities.end())
    {
        return nullptr;
    }
    return std::static_pointer_cast<T>(it->second);
}

struct Unit : SavepointEntity
{
    std::string Name;
    SavepointID Reference;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Name);
        visitor(Reference);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    // Reserve IDs so entities can reference each other before they're written
    std::shared_ptr<Unit> knight = std::make_shared<Unit>();
    std::shared_ptr<Unit> dragon = std::make_shared<Unit>();
    knight->Name = "Knight";
    dragon->Name = "Dragon";
    ASSERT(savepoint.Reserve(*knight));
    ASSERT(savepoint.Reserve(*dragon));

    knight->Reference = dragon->GetID();
    dragon->Reference = knight->GetID();
    ASSERT(savepoint.Write(*knight, 0));
    ASSERT(savepoint.Write(*dragon, 0));

    // Set the map of references when reading
    savepoint.Read<Unit>([&](Unit& unit)
    {
        Entities[unit.GetID()] = std::make_shared<Unit>(std::move(unit));
        return true;
    }, 0);
    ASSERT(Entities.size() == 2);

    std::shared_ptr<Unit> outKnight = GetEntity<Unit>(knight->GetID());
    ASSERT(outKnight->Name == "Knight");
    ASSERT(GetEntity<Unit>(outKnight->Reference)->Name == "Dragon");
    ASSERT(GetEntity<Unit>(GetEntity<Unit>(outKnight->Reference)->Reference) == outKnight);

    SavepointID dragonID = outKnight->Reference;
    ASSERT(savepoint.Delete(dragonID));
    ASSERT(!savepoint.Contains(dragonID));
    Entities.erase(dragonID);
    ASSERT(!GetEntity<Unit>(outKnight->Reference));

    savepoint.Close();
    return 0;
}
// [23_references]
