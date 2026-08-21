// [15_reserved_ids]
#include <savepoint/savepoint.hpp>

#include <filesystem>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

// IDs (or levels) can be reserved
static constexpr int kPlayerID = 1001;
static constexpr int kBossID = 1002;

struct Player
{
    int Score;

    Player() = default;
    Player(int score)
        : Score{score}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Score);
    }
};

struct Boss
{
    int Difficulty;
    int Health;
    int Attempts;

    Boss() = default;
    Boss(int difficulty, int health, int attempts)
        : Difficulty{difficulty}
        , Health{health}
        , Attempts{attempts}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Difficulty);
        visitor(Health);
        visitor(Attempts);
    }
};

int main()
{
    std::filesystem::remove("savepoint.sqlite3");

    Savepoint savepoint;
    SavepointStatus status = savepoint.Open(SavepointDriver::SQLite3, "savepoint.sqlite3", kVersion);
    ASSERT(status == SavepointStatus::New);

    Player inPlayer{10};
    Boss inBoss{20, 100, 1};
    savepoint.Write(inPlayer, kPlayerID);
    savepoint.Write(inBoss, kBossID);

    Player outPlayer;
    ASSERT(savepoint.Read(outPlayer, kPlayerID));
    ASSERT(outPlayer.Score == inPlayer.Score);

    Boss outBoss;
    ASSERT(savepoint.Read(outBoss, kBossID));
    ASSERT(outBoss.Difficulty == inBoss.Difficulty);
    ASSERT(outBoss.Health == inBoss.Health);
    ASSERT(outBoss.Attempts == inBoss.Attempts);

    inBoss.Difficulty = 30;
    inBoss.Attempts++;
    savepoint.Write(inBoss, kBossID);
    ASSERT(savepoint.Read(outBoss, kBossID));
    ASSERT(outBoss.Difficulty == inBoss.Difficulty);
    ASSERT(outBoss.Health == inBoss.Health);
    ASSERT(outBoss.Attempts == inBoss.Attempts);
    ASSERT(savepoint.Read(outPlayer, kPlayerID));
    ASSERT(outPlayer.Score == inPlayer.Score);

    ASSERT(!savepoint.Read(outBoss, 1003));

    savepoint.Clear();
    ASSERT(!savepoint.Read(outPlayer, kPlayerID));
    ASSERT(!savepoint.Read(outBoss, kBossID));

    savepoint.Close();
    return 0;
}
// [15_reserved_ids]
