// [5_external_types]
#include <savepoint/savepoint.hpp>

#include <filesystem>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct ExternalType
{
    int Member1;
    int Member2;

    ExternalType() = default;
    ExternalType(int member1, int member2)
        : Member1{member1}
        , Member2{member2}
    {
    }
};

void Visit(SavepointVisitor& visitor, ExternalType& external)
{
    visitor(external.Member1);
    visitor(external.Member2);
}

bool operator==(const ExternalType& lhs, const ExternalType& rhs)
{
    return lhs.Member1 == rhs.Member1 && lhs.Member2 == rhs.Member2;
}

struct Entity : SavepointEntity
{
    ExternalType External;

    Entity() = default;
    Entity(int member1, int member2)
        : External(member1, member2)
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(External);
    }

    bool operator==(const Entity& other) const
    {
        return External == other.External;
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);

    Entity inEntity{1, 2};
    savepoint.Write(inEntity, 0);
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        ASSERT(outEntity == inEntity);
    }, 0);

    savepoint.Close();
    return 0;
}
// [5_external_types]
