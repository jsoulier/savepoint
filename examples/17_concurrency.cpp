// [17_concurrency]
#include <savepoint/savepoint.hpp>

#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};
static constexpr int kEntityCount = 32;
static constexpr int kIterationCount = 100;

struct Entity
{
    int ID;
    int Score;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(ID);
        visitor(Score);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint writer1;
    SavepointStatus status = writer1.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);
    for (int id = 0; id < kEntityCount; id++)
    {
        Entity entity{id, id * 1000};
        ASSERT(writer1.Write(entity, id));
    }
    ASSERT(writer1.Save());

    // writer2 fails since writer1 has the write lock
    Savepoint writer2;
    status = writer2.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::Failed);

    std::atomic<bool> isWriting = true;
    std::vector<std::thread> readers;
    for (int reader = 0; reader < 8; reader++)
    {
        readers.emplace_back([&isWriting]()
        {
            Savepoint savepoint;
            SavepointStatus status = savepoint.Open("savepoint.sqlite3", kVersion, SavepointFlags::SQLite3 | SavepointFlags::Read);
            ASSERT(status == SavepointStatus::Existing);

            // Readers can't write
            Entity inEntity{0, -1};
            ASSERT(!savepoint.Write(inEntity, 0));
            ASSERT(!savepoint.Save());

            std::vector<int> scores(kEntityCount, 0);
            while (isWriting)
            {
                for (int id = 0; id < kEntityCount; id++)
                {
                    Entity outEntity;
                    ASSERT(savepoint.Read(outEntity, id));
                    ASSERT(outEntity.ID == id);
                    ASSERT(outEntity.Score >= id * 1000 + scores[id]);
                    ASSERT(outEntity.Score < id * 1000 + kIterationCount);
                    scores[id] = outEntity.Score - id * 1000;
                }
            }
            ASSERT(savepoint.Close());
        });
    }

    for (int iteration = 1; iteration < kIterationCount; iteration++)
    {
        for (int id = 0; id < kEntityCount; id++)
        {
            Entity entity{id, id * 1000 + iteration};
            ASSERT(writer1.Write(entity, id));
        }
        ASSERT(writer1.Save());
    }
    isWriting = false;
    for (std::thread& reader : readers)
    {
        reader.join();
    }
    ASSERT(writer1.Close());

    status = writer1.Open("savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::Existing);
    for (int id = 0; id < kEntityCount; id++)
    {
        Entity entity;
        ASSERT(writer1.Read(entity, id));
        ASSERT(entity.ID == id);
        ASSERT(entity.Score == id * 1000 + kIterationCount - 1);
    }

    ASSERT(writer1.Close());
    return 0;
}
// [17_concurrency]
