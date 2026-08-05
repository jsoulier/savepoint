// [15_reserved_ids]
#include <savepoint/savepoint.hpp>

#include <cassert>
#include <filesystem>

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
    assert(status == SavepointStatus::New);

    Player inPlayer{10};
    Boss inBoss{20, 100, 1};
    savepoint.Write(inPlayer, kPlayerID);
    savepoint.Write(inBoss, kBossID);

    Player outPlayer;
    assert(savepoint.Read(outPlayer, kPlayerID));
    assert(outPlayer.Score == inPlayer.Score);

    Boss outBoss;
    assert(savepoint.Read(outBoss, kBossID));
    assert(outBoss.Difficulty == inBoss.Difficulty);
    assert(outBoss.Health == inBoss.Health);
    assert(outBoss.Attempts == inBoss.Attempts);

    inBoss.Difficulty = 30;
    inBoss.Attempts++;
    savepoint.Write(inBoss, kBossID);
    assert(savepoint.Read(outBoss, kBossID));
    assert(outBoss.Difficulty == inBoss.Difficulty);
    assert(outBoss.Health == inBoss.Health);
    assert(outBoss.Attempts == inBoss.Attempts);
    assert(savepoint.Read(outPlayer, kPlayerID));
    assert(outPlayer.Score == inPlayer.Score);

    assert(!savepoint.Read(outBoss, 1003));

    savepoint.Clear();
    assert(!savepoint.Read(outPlayer, kPlayerID));
    assert(!savepoint.Read(outBoss, kBossID));

    savepoint.Close();
    return 0;
}
// [15_reserved_ids]
