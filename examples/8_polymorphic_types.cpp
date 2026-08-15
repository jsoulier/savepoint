// [8_polymorphic_types]
#include <savepoint/savepoint.hpp>

#include <cassert>
#include <filesystem>
#include <memory>

static constexpr SavepointVersion kVersion{0, 0, 0};

// Your base class inherits from SavepointPolymorph (and optionally SavepointEntity)
struct Entity : SavepointPolymorph, SavepointEntity
{
    int X = 1;
    int Y = 2;

    // Optionally implement Visit
    void Visit(SavepointVisitor& visitor) override
    {
        visitor(X);
        visitor(Y);
    }

    bool operator==(const Entity& other) const
    {
        return X == other.X && Y == other.Y;
    }
};

// Intermediate classes visit their own fields but do not need SAVEPOINT_POLYMORPH
struct MobEntity : public Entity
{
    int Health = 10;
    int Damage = 3;

    void Visit(SavepointVisitor& visitor) override
    {
        Entity::Visit(visitor);
        visitor(Health);
        visitor(Damage);
    }

    bool operator==(const MobEntity& other) const
    {
        return Entity::operator==(other) &&
            Health == other.Health &&
            Damage == other.Damage;
    }
};

struct ZombieEntity : public MobEntity
{
    // Your concrete derived classes use SAVEPOINT_POLYMORPH to implement required methods
    SAVEPOINT_POLYMORPH(ZombieEntity);

    int Strength;

    ZombieEntity()
        : Strength{5}
    {
    }

    void Visit(SavepointVisitor& visitor) override
    {
        // Make sure to use the base class' Visit function
        MobEntity::Visit(visitor);
        visitor(Strength);
    }

    bool operator==(const ZombieEntity& other) const
    {
        return MobEntity::operator==(other) && Strength == other.Strength;
    }
};

struct SkeletonEntity : public MobEntity
{
    SAVEPOINT_POLYMORPH(SkeletonEntity);
};

struct SpiderEntity : public MobEntity
{
    SAVEPOINT_POLYMORPH(SpiderEntity);

    int Eyes = 8;
    int Legs = 8;

    void Visit(SavepointVisitor& visitor) override
    {
        MobEntity::Visit(visitor);
        visitor(Eyes);
        visitor(Legs);
    }

    bool operator==(const SpiderEntity& other) const
    {
        return MobEntity::operator==(other) &&
            Eyes == other.Eyes &&
            Legs == other.Legs;
    }
};

struct ItemEntity : public Entity
{
    SAVEPOINT_POLYMORPH(ItemEntity);
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    assert(status == SavepointStatus::New);

    // Write concrete derived classes as usual
    std::shared_ptr<ItemEntity> inItem = std::make_shared<ItemEntity>();
    std::shared_ptr<ZombieEntity> inZombie = std::make_shared<ZombieEntity>();
    std::shared_ptr<SkeletonEntity> inSkeleton = std::make_shared<SkeletonEntity>();
    std::shared_ptr<SpiderEntity> inSpider = std::make_shared<SpiderEntity>();
    savepoint.Write(inItem, 0);
    savepoint.Write(inZombie, 0);
    savepoint.Write(inSkeleton, 0);
    savepoint.Write(inSpider, 0);

    // Read using your base class' class name
    int reads = 0;
    savepoint.Read<std::shared_ptr<Entity>>([&](std::shared_ptr<Entity>& entity)
    {
        // The concrete type is restored, so the base pointer can safely be cast back
        if (ItemEntity* outItem = dynamic_cast<ItemEntity*>(entity.get()))
        {
            assert(*outItem == *inItem);
        }
        else if (ZombieEntity* outZombie = dynamic_cast<ZombieEntity*>(entity.get()))
        {
            assert(*outZombie == *inZombie);
        }
        else if (SkeletonEntity* outSkeleton = dynamic_cast<SkeletonEntity*>(entity.get()))
        {
            assert(*outSkeleton == *inSkeleton);
        }
        else if (SpiderEntity* outSpider = dynamic_cast<SpiderEntity*>(entity.get()))
        {
            assert(*outSpider == *inSpider);
        }
        else
        {
            assert(false);
        }
        reads++;
    }, 0);
    assert(reads == 4);

    savepoint.Close();
    return 0;
}
// [8_polymorphic_types]
