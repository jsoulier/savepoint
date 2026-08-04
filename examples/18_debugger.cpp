// [18_debugger]
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <savepoint/imgui.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

static constexpr SavepointVersion kVersion{1, 0, 0};

struct Armor
{
    int Durability = 100;
    float Weight = 1.5f;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Durability);
        visitor(Weight);
    }

    void Randomize(std::mt19937& random)
    {
        Durability = std::uniform_int_distribution<int>{0, 100}(random);
        Weight = std::uniform_real_distribution<float>{0.5f, 20.0f}(random);
    }
};

struct Header
{
    int Time = 0;
    std::mt19937 Random;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Time);
        visitor(Random);
    }

    void Randomize(std::mt19937& random)
    {
        Time = std::uniform_int_distribution<int>{0, 100000}(random);
        Random = random;
    }
};

struct Level
{
    std::string Name = "level";
    bool Unlocked = false;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Name);
        visitor(Unlocked);
    }

    void Randomize(std::mt19937& random)
    {
        static constexpr std::array<std::string_view, 4> kNames = {"Caves", "Surface", "Ruins", "Depths"};
        Name = kNames[std::uniform_int_distribution<size_t>{0, kNames.size() - 1}(random)];
        Unlocked = std::uniform_int_distribution<int>{0, 1}(random) != 0;
    }
};

enum class PlayerTeam 
{
    Red,
    Blue,
    Green,
};

struct Player : SavepointEntity
{
    std::string Name = "John";
    int Health = 100;
    float X = 0.0f;
    float Y = 0.0f;
    PlayerTeam Team = PlayerTeam::Blue;
    Armor Chestplate;
    Armor Boots;
    std::vector<int> Inventory;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Health);
        visitor(X);
        visitor(Y);
        visitor(Name);
        visitor(Team);
        visitor(Chestplate);
        visitor(Boots);
        visitor(Inventory);
    }

    void Randomize(std::mt19937& random)
    {
        static constexpr std::array<std::string_view, 4> kNames = {"Simon", "Paul", "David", "Marc"};
        Health = std::uniform_int_distribution<int>{1, 100}(random);
        X = std::uniform_real_distribution<float>{-64.0f, 64.0f}(random);
        Y = std::uniform_real_distribution<float>{-64.0f, 64.0f}(random);
        Name = kNames[std::uniform_int_distribution<int>{0, kNames.size() - 1}(random)];
        Team = PlayerTeam(std::uniform_int_distribution<int>{0, 2}(random));
        Chestplate.Randomize(random);
        Boots.Randomize(random);
        Inventory.clear();
        for (int i = 0; i < std::uniform_int_distribution<int>{0, 3}(random); i++)
        {
            Inventory.push_back(std::uniform_int_distribution<int>{1, 99}(random));
        }
    }
};

enum class Terrain : uint8_t
{
    Empty,
    Grass,
    Stone,
    Water,
};

struct Floor
{
    Terrain Type = Terrain::Empty;
    int Variant = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Type);
        visitor(Variant);
    }

    void Randomize(std::mt19937& random)
    {
        Type = Terrain(std::uniform_int_distribution<int>{0, 3}(random));
        Variant = std::uniform_int_distribution<int>{0, 5}(random);
    }
};

struct VoxelV1
{
    uint8_t Material = 0;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Material);
    }

    void Randomize(std::mt19937& random)
    {
        Material = std::uniform_int_distribution<int>{0, 7}(random);
    }
};

struct VoxelV2
{
    int Type = 1;
    uint8_t Material = 0;
    float Mass = 0.0f;

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Type);
        visitor(Material);
        visitor(Mass);
    }

    void Randomize(std::mt19937& random)
    {
        Type = std::uniform_int_distribution<int>{0, 256}(random);
        Material = std::uniform_int_distribution<int>{0, 7}(random);
        Mass = std::uniform_real_distribution<float>{0.0f, 1.0f}(random);
    }
};

// For the time being, types must be manually defined 
SAVEPOINT_TYPE(Header)
SAVEPOINT_TYPE(Level)
SAVEPOINT_TYPE(Player)
SAVEPOINT_TYPE(Floor)
// Intentionally removed: SAVEPOINT_TYPE(VoxelV1)
SAVEPOINT_TYPE(VoxelV2)

int main(int argc, char** argv)
{
    SavepointSetLogFunction([](const std::string_view& string) { SDL_Log("%.*s", int(string.size()), string.data()); });
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Savepoint Debugger", 960, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    SDL_ShowWindow(window);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    std::filesystem::remove("debugger.sqlite3");
    Savepoint savepoint;
    savepoint.Open(SavepointDriver::SQLite3, "debugger.sqlite3", kVersion);
    std::mt19937 random{std::random_device{}()};
    Header header;
    header.Randomize(random);
    savepoint.Write(header);
    for (int i = 0; i < 3; i++)
    {
        Level info;
        info.Randomize(random);
        savepoint.Write(info, i);
    }
    for (int i = 0; i < 8; i++)
    {
        Player player;
        player.Randomize(random);
        savepoint.Write(player, 0);
    }
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            Floor floor;
            floor.Randomize(random);
            savepoint.Write(floor, x, y, 0);
            for (int z = 0; z < 3; z++)
            {
                VoxelV1 voxel1;
                VoxelV2 voxel2;
                voxel1.Randomize(random);
                voxel2.Randomize(random);
                savepoint.Write(voxel1, x, y, z * 2, 0);
                savepoint.Write(voxel2, x, y, z * 2 + 1, 0);
            }
        }
    }
    savepoint.Save();

    SavepointDebugger debugger;
    while (true)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                return 0;
            }
        }
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (ImGui::Begin("18_SavepointDebugger", nullptr, flags))
        {
            debugger.Render(savepoint);
        }
        ImGui::End();
        ImGui::Render();
        SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
    return 0;
}
// [18_debugger]