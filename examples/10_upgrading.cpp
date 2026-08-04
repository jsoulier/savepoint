// [10_upgrading]
#include <savepoint/savepoint.hpp>

#include <array>
#include <cassert>
#include <filesystem>
#include <string>
#include <string_view>

static constexpr SavepointVersion kVersion1{0, 0, 1};
static constexpr SavepointVersion kVersion2{0, 1, 0};
static constexpr SavepointVersion kVersion3{0, 2, 0};
static constexpr SavepointVersion kVersion4{1, 1, 0};

// Types can gain fields in later versions, including when nested in other types
struct ItemV2
{
    int Durability = 1;
    int Damage = 2;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Durability);
        visitor(Damage);
    }

    bool operator==(const ItemV2& other) const
    {
        return Durability == other.Durability &&
            Damage == other.Damage;
    }
};

struct ItemV4
{
    int Durability = 1;
    int Rarity = 2;
    int Damage = 3;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Durability);
        visitor(Damage);
        visitor(Rarity, kVersion4);
    }

    bool operator==(const ItemV2& other) const
    {
        return Durability == other.Durability &&
            Rarity == 2 &&
            Damage == other.Damage;
    }

    bool operator==(const ItemV4& other) const
    {
        return Durability == other.Durability &&
            Rarity == other.Rarity &&
            Damage == other.Damage;
    }
};

struct EntityV1 : SavepointEntity
{
    float X = 1.0f;
    float Y = 2.0f;
    std::array<int, 2> EquipmentSlots = {1, 3};
    int Health = 3;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(X);
        visitor(Y);
        visitor(EquipmentSlots);
        visitor(Health);
    }

    bool operator==(const EntityV1& other) const
    {
        return X == other.X &&
            Y == other.Y &&
            EquipmentSlots == other.EquipmentSlots &&
            Health == other.Health;
    }
};

struct EntityV2 : SavepointEntity
{
    int Strength = 4; // Added in 0.1.0
    float X = 1.0f;
    float Y = 2.0f;
    ItemV2 Item; // Added in 0.1.0
    std::array<int, 3> EquipmentSlots = {1, 3, 2}; // Expanded in 0.1.0
    int Health = 3;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Strength, kVersion2);
        visitor(X);
        visitor(Y);
        visitor(Item, kVersion2);
        visitor(EquipmentSlots);
        visitor(Health);
    }

    bool operator==(const EntityV1& other) const
    {
        return Strength == 4 &&
            X == other.X &&
            Y == other.Y &&
            Item == ItemV2{} &&
            EquipmentSlots[0] == other.EquipmentSlots[0] &&
            EquipmentSlots[1] == other.EquipmentSlots[1] &&
            EquipmentSlots[2] == 2 &&
            Health == other.Health;
    }

    bool operator==(const EntityV2& other) const
    {
        return Strength == other.Strength &&
            X == other.X &&
            Y == other.Y &&
            Item == other.Item &&
            EquipmentSlots == other.EquipmentSlots &&
            Health == other.Health;
    }
};

struct EntityV4 : SavepointEntity
{
    int Strength = 4;
    float X = 1.0f;
    float Z = 6.0f; // Added in 1.1.0
    float Y = 2.0f;
    ItemV4 Item;
    std::array<int, 1> EquipmentSlots = {1}; // Reduced in 1.1.0
    int Health = 3;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Strength, kVersion2);
        visitor(X);
        visitor(Y);
        visitor(Z, kVersion4);
        visitor(Item, kVersion2);
        visitor(EquipmentSlots);
        visitor(Health);
    }

    bool operator==(const EntityV1& other) const
    {
        return Strength == 4 &&
            X == other.X &&
            Z == 6.0f &&
            Y == other.Y &&
            Item == ItemV4{} &&
            EquipmentSlots[0] == other.EquipmentSlots[0] &&
            Health == other.Health;
    }

    bool operator==(const EntityV2& other) const
    {
        return Strength == other.Strength &&
            X == other.X &&
            Z == 6.0f &&
            Y == other.Y &&
            Item == other.Item &&
            EquipmentSlots[0] == other.EquipmentSlots[0] &&
            Health == other.Health;
    }

    bool operator==(const EntityV4& other) const
    {
        return Strength == other.Strength &&
            X == other.X &&
            Z == other.Z &&
            Y == other.Y &&
            Item == other.Item &&
            EquipmentSlots == other.EquipmentSlots &&
            Health == other.Health;
    }
};

struct ZombieV1 : EntityV1
{
    float Speed = 1.0f;

    void Visit(SavepointVisitor& visitor)
    {
        EntityV1::Visit(visitor);
        visitor(Speed);
    }

    bool operator==(const ZombieV1& other) const
    {
        return EntityV1::operator==(other) &&
            Speed == other.Speed;
    }
};

struct ZombieV2 : EntityV2
{
    float Speed = 1.0f;
    float VelocityX = 2.0f;

    void Visit(SavepointVisitor& visitor)
    {
        EntityV2::Visit(visitor);
        visitor(Speed);
        visitor(VelocityX, kVersion2);
    }

    bool operator==(const ZombieV1& other) const
    {
        return EntityV2::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == 2.0f;
    }

    bool operator==(const ZombieV2& other) const
    {
        return EntityV2::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX;
    }
};

struct ZombieV3 : EntityV2
{
    float Speed = 1.0f;
    float VelocityX = 2.0f;
    float VelocityY = 3.0f;
    float VelocityZ = 4.0f;

    void Visit(SavepointVisitor& visitor)
    {
        EntityV2::Visit(visitor);
        visitor(Speed);
        visitor(VelocityX, kVersion2);
        visitor(VelocityY, kVersion3);
        visitor(VelocityZ, kVersion3);
    }

    bool operator==(const ZombieV1& other) const
    {
        return EntityV2::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == 2.0f &&
            VelocityY == 3.0f &&
            VelocityZ == 4.0f;
    }

    bool operator==(const ZombieV2& other) const
    {
        return EntityV2::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX &&
            VelocityY == 3.0f &&
            VelocityZ == 4.0f;
    }

    bool operator==(const ZombieV3& other) const
    {
        return EntityV2::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX &&
            VelocityY == other.VelocityY &&
            VelocityZ == other.VelocityZ;
    }
};

struct ZombieV4 : EntityV4
{
    float Speed = 1.0f;
    float VelocityX = 2.0f;
    float VelocityY = 3.0f;
    float VelocityZ = 4.0f;

    void Visit(SavepointVisitor& visitor)
    {
        EntityV4::Visit(visitor);
        visitor(Speed);
        visitor(VelocityX, kVersion2);
        visitor(VelocityY, kVersion3);
        visitor(VelocityZ, kVersion3);
    }

    bool operator==(const ZombieV1& other) const
    {
        return EntityV4::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == 2.0f &&
            VelocityY == 3.0f &&
            VelocityZ == 4.0f;
    }

    bool operator==(const ZombieV2& other) const
    {
        return EntityV4::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX &&
            VelocityY == 3.0f &&
            VelocityZ == 4.0f;
    }

    bool operator==(const ZombieV3& other) const
    {
        return EntityV4::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX &&
            VelocityY == other.VelocityY &&
            VelocityZ == other.VelocityZ;
    }

    bool operator==(const ZombieV4& other) const
    {
        return EntityV4::operator==(other) &&
            Speed == other.Speed &&
            VelocityX == other.VelocityX &&
            VelocityY == other.VelocityY &&
            VelocityZ == other.VelocityZ;
    }
};

template<typename InT, typename OutT = InT>
static void ReadWrite(std::string_view name, SavepointVersion version)
{
    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", version);
    assert(status == SavepointStatus::New);
    InT inEntity;
    savepoint.Write(inEntity, 0);
    int reads = 0;
    savepoint.Read<OutT>([&](OutT& outEntity)
    {
        assert(outEntity == inEntity);
        reads++;
    }, 0);
    assert(reads == 1);
    savepoint.Clear();
    savepoint.Close();
}

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    ReadWrite<EntityV1>("upgrading_entity_v1", kVersion1);
    ReadWrite<EntityV1, EntityV2>("upgrading_entity_v1_v2", kVersion1);
    ReadWrite<EntityV1, EntityV4>("upgrading_entity_v1_v4", kVersion1);
    ReadWrite<EntityV2>("upgrading_entity_v2", kVersion2);
    ReadWrite<EntityV2, EntityV4>("upgrading_entity_v2_v4", kVersion2);
    ReadWrite<EntityV4>("upgrading_entity_v4", kVersion4);
    ReadWrite<ZombieV1>("upgrading_zombie_v1", kVersion1);
    ReadWrite<ZombieV1, ZombieV2>("upgrading_zombie_v1_v2", kVersion1);
    ReadWrite<ZombieV1, ZombieV3>("upgrading_zombie_v1_v3", kVersion1);
    ReadWrite<ZombieV1, ZombieV4>("upgrading_zombie_v1_v4", kVersion1);
    ReadWrite<ZombieV2>("upgrading_zombie_v2", kVersion2);
    ReadWrite<ZombieV2, ZombieV3>("upgrading_zombie_v2_v3", kVersion2);
    ReadWrite<ZombieV2, ZombieV4>("upgrading_zombie_v2_v4", kVersion2);
    ReadWrite<ZombieV3>("upgrading_zombie_v3", kVersion3);
    ReadWrite<ZombieV3, ZombieV4>("upgrading_zombie_v3_v4", kVersion3);
    ReadWrite<ZombieV4>("upgrading_zombie_v4", kVersion4);
    return 0;
}
// [10_upgrading]
