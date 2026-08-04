// [3_std_types]
#include <savepoint/savepoint.hpp>

#include <array>
#include <cassert>
#include <deque>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Entity : SavepointEntity
{
    std::array<int, 5> Array;
    std::vector<int> Vector;
    std::deque<int> Deque;
    std::map<int, int> Map;
    std::unordered_map<int, int> UnorderedMap;
    std::multimap<int, int> Multimap;
    std::unordered_multimap<int, int> UnorderedMultimap;
    std::set<int> Set;
    std::unordered_set<int> UnorderedSet;
    std::multiset<int> Multiset;
    std::unordered_multiset<int> UnorderedMultiset;
    std::tuple<int, int, int> Tuple;
    std::pair<int, int> Pair;
    std::optional<int> Optional;
    std::optional<int> NullOptional;
    std::unique_ptr<int> UniquePtr;
    std::unique_ptr<int> NullUniquePtr;
    std::shared_ptr<int> SharedPtr;
    std::shared_ptr<int> NullSharedPtr;
    std::vector<std::unique_ptr<int>> UniquePtrVector;
    std::vector<std::shared_ptr<int>> SharedPtrVector;
    std::list<int> List;
    std::string String;
    std::vector<std::unique_ptr<int>> VectorUniquePtr;

    void OnCreate()
    {
        Array = {1, 2, 3, 4, 5};
        Vector = {1, 2, 3};
        Deque = {1, 2, 3};
        Map = {{1, 2}, {2, 3}};
        UnorderedMap = {{1, 2}, {2, 3}};
        Multimap = {{1, 2}, {1, 2}};
        UnorderedMultimap = {{1, 2}, {1, 2}};
        Set = {1, 2};
        UnorderedSet = {1, 2};
        Multiset = {1, 1};
        UnorderedMultiset = {1, 1};
        Tuple = std::make_tuple(1, 2, 3);
        Pair = std::make_pair(1, 2);
        Optional = 1;
        NullOptional = {};
        UniquePtr = std::make_unique<int>(1);
        NullUniquePtr = {};
        SharedPtr = std::make_shared<int>(1);
        NullSharedPtr = {};
        UniquePtrVector.emplace_back(std::make_unique<int>(1));
        UniquePtrVector.emplace_back(std::make_unique<int>(3));
        UniquePtrVector.emplace_back(std::make_unique<int>(2));
        SharedPtrVector.emplace_back(std::make_shared<int>(1));
        SharedPtrVector.emplace_back(std::make_shared<int>(3));
        SharedPtrVector.emplace_back(std::make_shared<int>(2));
        List = {1, 2};
        String = "string";
        VectorUniquePtr.emplace_back(std::make_unique<int>(1));
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Array);
        visitor(Vector);
        visitor(Deque);
        visitor(Map);
        visitor(UnorderedMap);
        visitor(Multimap);
        visitor(UnorderedMultimap);
        visitor(Set);
        visitor(UnorderedSet);
        visitor(Multiset);
        visitor(UnorderedMultiset);
        visitor(Tuple);
        visitor(Pair);
        visitor(Optional);
        visitor(NullOptional);
        visitor(UniquePtr);
        visitor(NullUniquePtr);
        visitor(SharedPtr);
        visitor(NullSharedPtr);
        visitor(UniquePtrVector);
        visitor(SharedPtrVector);
        visitor(List);
        visitor(String);
        visitor(VectorUniquePtr);
    }

    bool operator==(const Entity& other) const
    {
        return
            Array == other.Array &&
            Vector == other.Vector &&
            Deque == other.Deque &&
            Map == other.Map &&
            UnorderedMap == other.UnorderedMap &&
            Multimap == other.Multimap &&
            UnorderedMultimap == other.UnorderedMultimap &&
            Set == other.Set &&
            UnorderedSet == other.UnorderedSet &&
            Multiset == other.Multiset &&
            UnorderedMultiset == other.UnorderedMultiset &&
            Tuple == other.Tuple &&
            Pair == other.Pair &&
            *Optional == *other.Optional &&
            bool(NullOptional) == bool(other.NullOptional) &&
            *UniquePtr == *other.UniquePtr &&
            bool(NullUniquePtr) == bool(other.NullUniquePtr) &&
            *SharedPtr == *other.SharedPtr &&
            bool(NullSharedPtr) == bool(other.NullSharedPtr) &&
            *UniquePtrVector[0] == *other.UniquePtrVector[0] &&
            *UniquePtrVector[1] == *other.UniquePtrVector[1] &&
            *UniquePtrVector[2] == *other.UniquePtrVector[2] &&
            *SharedPtrVector[0] == *other.SharedPtrVector[0] &&
            *SharedPtrVector[1] == *other.SharedPtrVector[1] &&
            *SharedPtrVector[2] == *other.SharedPtrVector[2] &&
            List == other.List &&
            String == other.String &&
            *VectorUniquePtr[0] == *other.VectorUniquePtr[0];
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    assert(status == SavepointStatus::New);

    Entity inEntity;
    inEntity.OnCreate();
    savepoint.Write(inEntity, 0);

    int reads = 0;
    savepoint.Read<Entity>([&](Entity& outEntity)
    {
        assert(outEntity == inEntity);
        reads++;
    }, 0);
    assert(reads == 1);

    savepoint.Close();
    return 0;
}
// [3_std_types]
