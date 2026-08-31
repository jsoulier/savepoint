// [11_error_handling]
#include <savepoint/savepoint.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    uint32_t Value1;
    uint32_t Value2;

    void OnCreate()
    {
        Value1 = 0xDEADBEEF;
        Value2 = 0xBAADF00D;
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Value1);
        visitor(Value2);
    }

    bool operator==(const Entity other) const
    {
        return Value1 == other.Value1 && Value2 == other.Value2;
    }
};

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

struct ReadPastEntity : Entity
{
    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        if (visitor.IsReading())
        {
            uint32_t value = 0;
            visitor(value);
        }
    }
};

struct PartialReadEntity : Entity
{
    void Visit(SavepointVisitor& visitor)
    {
        visitor(Value1);
        if (visitor.IsReading())
        {
            visitor.SetError();
            return;
        }
        visitor(Value2);
    }
};

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
    savepoint.Read<ReadEntity>([](ReadEntity& outEntity) { ASSERT(false); }, 0);

    // Reading past the end of the visitor caused a failure and the callback isn't invoked
    savepoint.Read<ReadPastEntity>([](ReadPastEntity& outEntity) { ASSERT(false); }, 0);

    // Not reading the entire visitor caused a failure and the callback isn't invoked
    savepoint.Read<PartialReadEntity>([](PartialReadEntity& outEntity) { ASSERT(false); }, 0);

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
