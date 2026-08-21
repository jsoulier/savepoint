// [14_random]
#include <savepoint/savepoint.hpp>

#include <filesystem>
#include <random>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    std::minstd_rand inGenerator{12345};
    std::minstd_rand expected{12345};
    for (int i = 0; i < 5; i++)
    {
        ASSERT(inGenerator() == expected());
    }

    savepoint.Write(inGenerator);
    savepoint.Save();

    std::minstd_rand outGenerator;
    bool exists = savepoint.Read(outGenerator);
    ASSERT(exists);
    for (int i = 0; i < 5; i++)
    {
        ASSERT(outGenerator() == expected());
    }

    savepoint.Close();
    return 0;
}
// [14_random]
