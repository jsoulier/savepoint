#pragma once

#include <imgui.h>

#include <savepoint/savepoint.hpp>

#include <array>
#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class SavepointDebuggerMode : uint8_t
{
    Singleton,
    Levels,
    Entities,
    Tiles2D,
    Tiles3D,
};

static constexpr std::array<std::string_view, 5> kSavepointDebuggerModes =
{
    "Singleton",
    "Levels",
    "Entities",
    "2D tiles",
    "3D tiles",
};

class SavepointDebuggerTree
{
public:
    SavepointDebuggerTree(SavepointID id, int x, int y, int z, int level, std::vector<SavepointDebugNode> nodes)
        : ID{id}
        , X{x}
        , Y{y}
        , Z{z}
        , Level{level}
        , Nodes{std::move(nodes)}
    {
    }

    void Render(SavepointDebuggerMode mode) const
    {
        static constexpr ImGuiTreeNodeFlags kLeafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
        static constexpr ImGuiTreeNodeFlags kNodeFlags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
        switch (mode)
        {
        case SavepointDebuggerMode::Singleton:
            ImGui::TextUnformatted("singleton");
            break;
        case SavepointDebuggerMode::Levels:
            ImGui::Text("level %d", Level);
            break;
        case SavepointDebuggerMode::Entities:
            ImGui::Text("id %u, level %d", ID.GetValue(), Level);
            break;
        case SavepointDebuggerMode::Tiles2D:
            ImGui::Text("x %d, y %d, level %d", X, Y, Level);
            break;
        case SavepointDebuggerMode::Tiles3D:
            ImGui::Text("x %d, y %d, z %d, level %d", X, Y, Z, Level);
            break;
        }
        ImGui::Separator();
        if (Nodes.empty())
        {
            ImGui::TextDisabled("Nothing was read.");
            return;
        }
        int pushed = 0;
        int collapsed = -1;
        for (const SavepointDebugNode& node : Nodes)
        {
            int depth = node.GetDepth();
            if (collapsed >= 0)
            {
                if (depth > collapsed)
                {
                    continue;
                }
                collapsed = -1;
            }
            while (pushed > depth)
            {
                ImGui::TreePop();
                pushed--;
            }
            std::string_view type = node.GetTypeName();
            if (node.GetIsLeaf())
            {
                ImGui::TreeNodeEx(&node, kLeafFlags, "%.*s = %s", int(type.size()), type.data(), node.GetValue().data());
            }
            else if (ImGui::TreeNodeEx(&node, kNodeFlags, "%.*s", int(type.size()), type.data()))
            {
                pushed++;
            }
            else
            {
                collapsed = depth;
            }
        }
        while (pushed > 0)
        {
            ImGui::TreePop();
            pushed--;
        }
    }

    void RenderRow(SavepointDebuggerMode mode, int index, int& selected) const
    {
        std::vector<std::string> values;
        switch (mode)
        {
        case SavepointDebuggerMode::Levels:
            values = {std::format("{}", Level)};
            break;
        case SavepointDebuggerMode::Entities:
            values = {std::format("{}", ID.GetValue())};
            break;
        case SavepointDebuggerMode::Tiles2D:
            values = {std::format("{}", X), std::format("{}", Y)};
            break;
        case SavepointDebuggerMode::Tiles3D:
            values = {std::format("{}", X), std::format("{}", Y), std::format("{}", Z)};
            break;
        default:
            break;
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(index);
        if (ImGui::Selectable(values[0].data(), selected == index, ImGuiSelectableFlags_SpanAllColumns))
        {
            selected = index;
        }
        ImGui::PopID();
        for (int key = 1; key < values.size(); key++)
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(values[key].data());
        }
        for (const SavepointDebugNode& node : Nodes)
        {
            if (!node.GetIsLeaf())
            {
                continue;
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(node.GetValue().data());
        }
    }

    SavepointID ID;
    int X;
    int Y;
    int Z;
    int Level;
    std::vector<SavepointDebugNode> Nodes;
};

class SavepointDebugger
{
public:
    SavepointDebugger()
        : Mode{SavepointDebuggerMode::Singleton}
        , CachedMode{SavepointDebuggerMode::Singleton}
        , Level{0}
        , CachedLevel{0}
        , Selected{-1}
        , Dirty{true}
    {
    }

    void Refresh()
    {
        Dirty = true;
    }

    void Render(Savepoint& savepoint)
    {
        if (Dirty || Mode != CachedMode || Level != CachedLevel)
        {
            Refresh(savepoint);
        }
        if (Mode == SavepointDebuggerMode::Singleton && !Trees.empty())
        {
            Selected = 0;
        }
        if (!ImGui::BeginTable("Savepoint", 3, ImGuiTableFlags_Resizable))
        {
            return;
        }
        ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("##3", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        uint8_t mode = uint8_t(Mode);
        if (ImGui::BeginCombo("Mode", kSavepointDebuggerModes[mode].data()))
        {
            for (uint8_t i = 0; i < kSavepointDebuggerModes.size(); i++)
            {
                if (ImGui::Selectable(kSavepointDebuggerModes[i].data(), mode == i))
                {
                    Mode = SavepointDebuggerMode(i);
                }
            }
            ImGui::EndCombo();
        }
        if (Mode != SavepointDebuggerMode::Singleton)
        {
            ImGui::InputInt("Level", &Level);
        }
        if (ImGui::Button("Refresh"))
        {
            Dirty = true;
        }
        ImGui::Text("%zu rows", Trees.size());
        ImGui::TableNextColumn();
        if (Trees.empty())
        {
            ImGui::TextDisabled("Empty");
        }
        else if (Mode != SavepointDebuggerMode::Singleton)
        {
            std::vector<std::string_view> keys = GetKeys();
            int fields = 0;
            for (const SavepointDebuggerTree& tree : Trees)
            {
                int leaves = 0;
                for (const SavepointDebugNode& node : tree.Nodes)
                {
                    if (node.GetIsLeaf())
                    {
                        leaves++;
                    }
                }
                fields = std::max(fields, leaves);
            }
            int columns = keys.size() + fields;
            ImGuiTableFlags flags = ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##Rows", columns, flags))
            {
                ImGui::TableSetupScrollFreeze(keys.size(), 1);
                for (std::string_view key : keys)
                {
                    ImGui::TableSetupColumn(key.data(), ImGuiTableColumnFlags_WidthFixed, 50.0f);
                }
                for (int field = 0; field < fields; field++)
                {
                    ImGui::TableSetupColumn(std::format("##Column{}", field).data(), ImGuiTableColumnFlags_WidthFixed, 50.0f);
                }
                ImGui::TableHeadersRow();
                for (int i = 0; i < Trees.size(); i++)
                {
                    Trees[i].RenderRow(Mode, i, Selected);
                }
                ImGui::EndTable();
            }
        }
        ImGui::TableNextColumn();
        if (ImGui::BeginChild("##Contents"))
        {
            if (Selected < 0 || Selected >= Trees.size())
            {
                ImGui::TextDisabled("Select a row");
            }
            else
            {
                Trees[Selected].Render(Mode);
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }

private:
    void Refresh(Savepoint& savepoint)
    {
        CachedMode = Mode;
        CachedLevel = Level;
        Selected = -1;
        Trees.clear();
        Dirty = false;
        std::vector<SavepointDebugNode> nodes;
        switch (Mode)
        {
        case SavepointDebuggerMode::Singleton:
            if (savepoint.ReadDebug(nodes))
            {
                Trees.emplace_back(SavepointID{}, 0, 0, 0, 0, std::move(nodes));
            }
            break;
        case SavepointDebuggerMode::Levels:
            if (savepoint.ReadDebug(nodes, Level))
            {
                Trees.emplace_back(SavepointID{}, 0, 0, 0, Level, std::move(nodes));
            }
            break;
        case SavepointDebuggerMode::Entities:
            savepoint.ReadDebug([this](const std::vector<SavepointDebugNode>& read, SavepointID id)
            {
                Trees.emplace_back(id, 0, 0, 0, Level, read);
            }, Level);
            break;
        case SavepointDebuggerMode::Tiles2D:
            savepoint.ReadDebug([this](const std::vector<SavepointDebugNode>& read, int x, int y)
            {
                Trees.emplace_back(SavepointID{}, x, y, 0, Level, read);
            }, Level);
            break;
        case SavepointDebuggerMode::Tiles3D:
            savepoint.ReadDebug([this](const std::vector<SavepointDebugNode>& read, int x, int y, int z)
            {
                Trees.emplace_back(SavepointID{}, x, y, z, Level, read);
            }, Level);
            break;
        }
    }

    std::vector<std::string_view> GetKeys() const
    {
        switch (Mode)
        {
        case SavepointDebuggerMode::Levels:
            return {"level"};
        case SavepointDebuggerMode::Entities:
            return {"id"};
        case SavepointDebuggerMode::Tiles2D:
            return {"x", "y"};
        case SavepointDebuggerMode::Tiles3D:
            return {"x", "y", "z"};
        default:
            return {};
        }
    }

    SavepointDebuggerMode Mode;
    SavepointDebuggerMode CachedMode;
    int Level;
    int CachedLevel;
    int Selected;
    std::vector<SavepointDebuggerTree> Trees;
    bool Dirty;
};
