// [11_error_handling]
#include <savepoint/savepoint.hpp>

#include <cstdint>
#include <filesystem>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    uint32_t Value;

    void OnCreate()
    {
        Value = 0xDEADBEEF;
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Value);
    }

    bool operator==(const Entity other) const
    {
        return Value == other.Value;
    }
};

// Use SetError to prevent the read callback from being invoked (prevents reading the entity)
struct ReadEntity : Entity
{
    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        if (visitor.IsReading())
        {
            visitor.SetError();
        }
    }
};

// Use SetError to prevent the insert/update from being invoked (prevents writing the entity)
struct WriteEntity : Entity
{
    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        if (visitor.IsWriting())
        {
            visitor.SetError();
        }
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);

    Entity inEntity;
    inEntity.OnCreate();

    auto hasSingleEntity = [&]()
    {
        int reads = 0;
        savepoint.Read<Entity>([&](Entity& outEntity)
        {
            ASSERT(outEntity == inEntity);
            reads++;
        }, 0);
        return reads == 1;
    };

    savepoint.Write(inEntity, 0);
    ASSERT(hasSingleEntity());

    // The read failed and the callback isn't invoked
    savepoint.Read<ReadEntity>([](ReadEntity& outReadEntity) { ASSERT(false); }, 0);

    // The write failed so there's nothing to read
    savepoint.Delete(inEntity);
    WriteEntity inWriteEntity;
    savepoint.Write(inWriteEntity, 0);
    savepoint.Read<WriteEntity>([](WriteEntity& outWriteEntity) { ASSERT(false); }, 0);

    savepoint.Write(inEntity, 0);
    ASSERT(hasSingleEntity());

    savepoint.Close();
    return 0;
}
// [11_error_handling]
