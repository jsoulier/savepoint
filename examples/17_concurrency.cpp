// [17_concurrency]
#include <savepoint/savepoint.hpp>

#include <barrier>
#include <filesystem>
#include <thread>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};
static constexpr int kThreadCount = 32;
static constexpr int kIterationCount = 1000;

// Each thread gets its own Savepoint instance so we don't need thread safety
static constexpr bool kThreadSafe = false;
static constexpr int kMaxWait = 10000;

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

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);
    for (int id = 0; id < kThreadCount; id++)
    {
        Entity entity{id, id * 100};
        savepoint.Write(entity, id);
    }
    savepoint.Save();
    savepoint.Close();

    std::barrier barrier{kThreadCount};
    std::vector<std::thread> threads;
    for (int id = 0; id < kThreadCount; id++)
    {
        threads.emplace_back([&barrier, id]()
        {
            Savepoint savepoint;
            SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion, false, kMaxWait);
            ASSERT(status == SavepointStatus::Existing);

            barrier.arrive_and_wait();
            for (int iteration = 0; iteration < kIterationCount; iteration++)
            {
                Entity inEntity{id, id * 1000 + iteration};
                savepoint.Write(inEntity, id);

                Entity outEntity;
                ASSERT(savepoint.Read(outEntity, id));
                ASSERT(outEntity.ID == inEntity.ID);
                ASSERT(outEntity.Score == inEntity.Score);
            }
            savepoint.Save();
            savepoint.Close();
        });
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::Existing);
    for (int id = 0; id < kThreadCount; id++)
    {
        Entity entity;
        ASSERT(savepoint.Read(entity, id));
        ASSERT(entity.ID == id);
        ASSERT(entity.Score == id * 1000 + kIterationCount - 1);
    }

    savepoint.Close();
    return 0;
}
// [17_concurrency]
