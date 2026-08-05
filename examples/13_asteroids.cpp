#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <savepoint/imgui.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <numbers>
#include <random>
#include <string>
#include <vector>

static constexpr int kWidth = 960;
static constexpr int kHeight = 720;

static glm::vec2 Direction(float rotation);
static void Wrap(glm::vec2& position);
static glm::vec2 Offset(glm::vec2 a, glm::vec2 b);
static float RandomFloat();
static int RandomInt();
static void Lines(const std::vector<glm::vec2>& points, glm::vec2 position, float rotation);

void Visit(SavepointVisitor& visitor, glm::vec2& vector)
{
    visitor(vector.x);
    visitor(vector.y);
}

struct Entity
{
    glm::vec2 Position;
    glm::vec2 Velocity;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Position);
        visitor(Velocity);
    }
};

struct Bullet : Entity
{
    float Life = 0.0f;

    void Update(float deltaTime)
    {
        Position += Velocity * deltaTime;
        Life -= deltaTime;
        Wrap(Position);
    }

    void Draw(SDL_Renderer* renderer) const
    {
        SDL_FRect rect{Position.x - 2.0f, Position.y - 2.0f, 4.0f, 4.0f};
        SDL_SetRenderDrawColor(renderer, 255, 225, 125, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        visitor(Life);
    }
};

struct Player : Entity
{
    float Rotation = -std::numbers::pi_v<float> * 0.5f;
    float ShotTimer = 0.0f;
    float InvincibilityTimer = 2.0f;
    float Radius = 9.0f;
    int Lives = 3;
    bool IsAccelerating = false;
    std::vector<Bullet> Bullets;

    Player()
    {
        Position = {kWidth * 0.5f, kHeight * 0.5f};
    }

    void Fire()
    {
        if (ShotTimer > 0.0f || Bullets.size() >= 8 || Lives == 0)
        {
            return;
        }
        glm::vec2 direction = Direction(Rotation);
        Bullet bullet;
        bullet.Position = Position + direction * 14.0f;
        bullet.Velocity = Velocity + direction * 390.0f;
        bullet.Life = 1.2f;
        Bullets.push_back(bullet);
        ShotTimer = 0.18f;
    }

    void Update(float deltaTime)
    {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        Rotation += (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * 3.8f * deltaTime;
        IsAccelerating = keys[SDL_SCANCODE_W];
        if (IsAccelerating)
        {
            Velocity += Direction(Rotation) * 190.0f * deltaTime;
        }
        if (keys[SDL_SCANCODE_SPACE])
        {
            Fire();
        }
        Velocity *= std::pow(0.72f, deltaTime);
        if (glm::length(Velocity) > 300.0f)
        {
            Velocity = glm::normalize(Velocity) * 300.0f;
        }
        Position += Velocity * deltaTime;
        Wrap(Position);
        ShotTimer = std::max(0.0f, ShotTimer - deltaTime);
        InvincibilityTimer = std::max(0.0f, InvincibilityTimer - deltaTime);
        for (Bullet& bullet : Bullets)
        {
            bullet.Update(deltaTime);
        }
        std::erase_if(Bullets, [](const Bullet& bullet) { return bullet.Life <= 0.0f; });
    }

    void Destroy()
    {
        Lives--;
        Position = {kWidth * 0.5f, kHeight * 0.5f};
        Velocity = {};
        Rotation = -std::numbers::pi_v<float> * 0.5f;
        InvincibilityTimer = 2.5f;
    }

    void Draw() const
    {
        if (InvincibilityTimer == 0.0f || int(InvincibilityTimer * 10.0f) % 2)
        {
            Lines({{15.0f, 0.0f}, {-11.0f, -9.0f}, {-7.0f, 0.0f}, {-11.0f, 9.0f}}, Position, Rotation);
            if (IsAccelerating)
            {
                Lines({{-8.0f, -5.0f}, {-19.0f, 0.0f}, {-8.0f, 5.0f}}, Position, Rotation);
            }
        }
    }

    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        visitor(Rotation);
        visitor(ShotTimer);
        visitor(InvincibilityTimer);
        visitor(Lives);
        visitor(Bullets);
    }
};

struct Asteroid : Entity
{
    float Radius = 40.0f;
    float Rotation = 0.0f;
    float RotationRate = 0.0f;
    uint32_t Seed = 0;

    void Spawn(glm::vec2 position, float radius)
    {
        float rotation = RandomFloat() * std::numbers::pi_v<float> * 2.0f;
        Position = position;
        Velocity = Direction(rotation) * (35.0f + RandomFloat() * 35.0f);
        Radius = radius;
        Rotation = RandomFloat() * std::numbers::pi_v<float> * 2.0f;
        RotationRate = (RandomFloat() * 2.0f - 1.0f) * 0.8f;
        Seed = RandomInt();
    }

    void Update(float deltaTime)
    {
        Position += Velocity * deltaTime;
        Rotation += RotationRate * deltaTime;
        Wrap(Position);
    }

    int Split(std::vector<Asteroid>& asteroids) const
    {
        if (Radius <= 20.0f)
        {
            return 100;
        }
        float radius = Radius * 0.56f;
        asteroids.emplace_back().Spawn(Position, radius);
        asteroids.emplace_back().Spawn(Position, radius);
        return Radius > 30.0f ? 20 : 50;
    }

    void Draw() const
    {
        std::minstd_rand generator{Seed};
        std::vector<glm::vec2> points;
        for (int i = 0; i < 10; i++)
        {
            float rotation = i / 10.0f * std::numbers::pi_v<float> * 2.0f;
            float radius = Radius * (0.78f + generator() % 23 * 0.01f);
            points.push_back(Direction(rotation) * radius);
        }
        Lines(points, Position, Rotation);
    }

    void Visit(SavepointVisitor& visitor)
    {
        Entity::Visit(visitor);
        visitor(Radius);
        visitor(Rotation);
        visitor(RotationRate);
        visitor(Seed);
    }
};

struct State
{
    std::minstd_rand Generator;
    Player User;
    std::vector<Asteroid> Asteroids;
    int Score = 0;
    int Round = 0;
    
    State()
    {
        Generator.seed(SDL_GetTicks());
    }

    void Spawn()
    {
        Round++;
        Asteroids.clear();
        User.Bullets.clear();
        int count = std::min(10, Round + 3);
        for (int i = 0; i < count; i++)
        {
            glm::vec2 position;
            do
            {
                position = {RandomFloat() * kWidth, RandomFloat() * kHeight};
            }
            while (glm::length(Offset(position, User.Position)) < 180.0f);
            Asteroid& asteroid = Asteroids.emplace_back();
            asteroid.Spawn(position, 42.0f);
        }
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(User);
        visitor(Generator);
        visitor(Score);
        visitor(Round);
        visitor(Asteroids);
    }
};

static SDL_Window* window;
static SDL_Renderer* renderer;
static Savepoint savepoint;
static State state;
static SavepointDebugger debugger;
static bool showDebugger = false;

static void Wrap(glm::vec2& position)
{
    if (position.x < 0.0f)
    {
        position.x += kWidth;
    }
    if (position.x >= kWidth)
    {
        position.x -= kWidth;
    }
    if (position.y < 0.0f)
    {
        position.y += kHeight;
    }
    if (position.y >= kHeight)
    {
        position.y -= kHeight;
    }
}

static glm::vec2 Offset(glm::vec2 a, glm::vec2 b)
{
    glm::vec2 offset = a - b;
    if (offset.x > kWidth / 2)
    {
        offset.x -= kWidth;
    }
    if (offset.x < -kWidth / 2)
    {
        offset.x += kWidth;
    }
    if (offset.y > kHeight / 2)
    {
        offset.y -= kHeight;
    }
    if (offset.y < -kHeight / 2)
    {
        offset.y += kHeight;
    }
    return offset;
}

static glm::vec2 Direction(float rotation)
{
    return {std::cos(rotation), std::sin(rotation)};
}

static float RandomFloat()
{
    return std::generate_canonical<float, 24>(state.Generator);
}

static int RandomInt()
{
    return state.Generator();
}

static void Lines(const std::vector<glm::vec2>& points, glm::vec2 position, float rotation)
{
    float c = std::cos(rotation);
    float s = std::sin(rotation);
    glm::mat2 rotationMatrix{{c, s}, {-s, c}};
    SDL_SetRenderDrawColor(renderer, 210, 227, 225, 255);
    for (size_t i = 0; i < points.size(); i++)
    {
        glm::vec2 a = rotationMatrix * points[i] + position;
        glm::vec2 b = rotationMatrix * points[(i + 1) % points.size()] + position;
        SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
    }
}

static void Text(float x, float y, std::string text, float size)
{
    SDL_SetRenderDrawColor(renderer, 220, 235, 232, 255);
    SDL_SetRenderScale(renderer, size, size);
    SDL_RenderDebugText(renderer, x / size, y / size, text.c_str());
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

static void CenteredText(float y, std::string text, float size)
{
    Text((kWidth - text.size() * 8.0f * size) * 0.5f, y, text, size);
}

static void Restart()
{
    state = {};
    state.Spawn();
}

static void Save()
{
    savepoint.Write(state);
    savepoint.Save();
    debugger.Refresh();
}

static void Load()
{
    savepoint.Read(state);
    debugger.Refresh();
}

static void Update(float deltaTime)
{
    Player& player = state.User;
    if (player.Lives == 0)
    {
        return;
    }
    player.Update(deltaTime);
    for (Asteroid& asteroid : state.Asteroids)
    {
        asteroid.Update(deltaTime);
    }
    for (int bullet = 0; bullet < player.Bullets.size();)
    {
        bool hit = false;
        for (int asteroid = 0; asteroid < state.Asteroids.size(); asteroid++)
        {
            glm::vec2 offset = Offset(player.Bullets[bullet].Position, state.Asteroids[asteroid].Position);
            if (glm::length(offset) < state.Asteroids[asteroid].Radius)
            {
                player.Bullets.erase(player.Bullets.begin() + bullet);
                Asteroid hitAsteroid = state.Asteroids[asteroid];
                state.Asteroids.erase(state.Asteroids.begin() + asteroid);
                state.Score += hitAsteroid.Split(state.Asteroids);
                hit = true;
                break;
            }
        }
        if (!hit)
        {
            bullet++;
        }
    }
    if (player.InvincibilityTimer == 0.0f)
    {
        for (const Asteroid& asteroid : state.Asteroids)
        {
            glm::vec2 offset = Offset(player.Position, asteroid.Position);
            if (glm::length(offset) < asteroid.Radius + player.Radius)
            {
                player.Destroy();
                break;
            }
        }
    }
    if (state.Asteroids.empty())
    {
        state.Spawn();
    }
}

static void Draw()
{
    const Player& player = state.User;
    SDL_SetRenderDrawColor(renderer, 2, 7, 11, 255);
    SDL_RenderClear(renderer);
    std::minstd_rand generator{12345};
    for (int i = 0; i < 100; i++)
    {
        uint32_t star = generator();
        SDL_SetRenderDrawColor(renderer, 100, 120, 120, 255);
        SDL_RenderPoint(renderer, float(star % kWidth), float((star >> 12) % kHeight));
    }
    for (const Asteroid& asteroid : state.Asteroids)
    {
        asteroid.Draw();
    }
    for (const Bullet& bullet : player.Bullets)
    {
        bullet.Draw(renderer);
    }
    player.Draw();
    Text(20.0f, 18.0f, std::format("SCORE {:06}", state.Score), 1.5f);
    CenteredText(18.0f, std::format("WAVE {}", state.Round), 1.5f);
    Text(20.0f, 52.0f, std::format("LIVES {}", player.Lives), 1.0f);
    Text(20.0f, kHeight - 28.0f, "A/D TURN  W ACCELERATE  SPACE FIRE  F5 SAVE  F9 LOAD  N NEW  F10 DEBUG", 1.0f);
    if (player.Lives == 0)
    {
        SDL_FRect rect{0.0f, 0.0f, kWidth, kHeight};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 2, 7, 11, 210);
        SDL_RenderFillRect(renderer, &rect);
        CenteredText(250.0f, "GAME OVER", 3.0f);
        CenteredText(315.0f, "N FOR NEW GAME", 1.0f);
    }
}

static void DrawDebugger()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("13_SavepointDebugger", &showDebugger, flags))
    {
        debugger.Render(savepoint);
    }
    ImGui::End();
    ImGui::Render();
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_SetRenderLogicalPresentation(renderer, kWidth, kHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

int main(int argc, char** argv)
{
    SavepointSetLogFunction([](std::string_view string) { SDL_Log("%.*s", int(string.size()), string.data()); });
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer("Asteroids", kWidth, kHeight, SDL_WINDOW_RESIZABLE, &window, &renderer);
    SDL_SetRenderLogicalPresentation(renderer, kWidth, kHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    switch (savepoint.Open(SavepointDriver::SQLite3, "asteroids.sqlite3", SavepointVersion{}))
    {
    case SavepointStatus::New:
        Restart();
        break;
    case SavepointStatus::Existing:
        Load();
        break;
    }
    uint64_t ticks1 = SDL_GetTicks();
    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.scancode)
                {
                case SDL_SCANCODE_N:
                    Restart();
                    break;
                case SDL_SCANCODE_F5:
                    Save();
                    break;
                case SDL_SCANCODE_F9:
                    Load();
                    break;
                case SDL_SCANCODE_F10:
                    showDebugger = !showDebugger;
                    break;
                }
                break;
            }
        }
        uint64_t ticks2 = SDL_GetTicks();
        Update(float(ticks2 - ticks1) / 1000.0f);
        ticks1 = ticks2;
        Draw();
        if (showDebugger)
        {
            DrawDebugger();
        }
        SDL_RenderPresent(renderer);
    }
    Save();
    savepoint.Close();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
