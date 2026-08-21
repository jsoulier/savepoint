// [16_basic_thread_safety]
#include <savepoint/savepoint.hpp>

#include <barrier>
#include <filesystem>
#include <thread>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};
static constexpr int kThreadCount = 8;
static constexpr int kIterationCount = 1000;

// The Savepoint is used from multiple threads so we need thread safety
static constexpr bool kThreadSafe = true;

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
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion, kThreadSafe);
    ASSERT(status == SavepointStatus::New);

    std::barrier barrier{kThreadCount};
    std::vector<std::thread> threads;
    for (int id = 0; id < kThreadCount; id++)
    {
        threads.emplace_back([&savepoint, &barrier, id]()
        {
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
        });
    }

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    savepoint.Save();
    savepoint.Close();
    return 0;
}
// [16_basic_thread_safety]
