#include <savepoint/savepoint.hpp>

#include <cstddef>
#include <cstdio>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "null.hpp"
#if SAVEPOINT_SQLITE3
#include "sqlite3.hpp"
#endif

static void DefaultLogFunction(std::string_view string)
{
    std::fwrite(string.data(), sizeof(char), string.size(), stderr);
    std::fputc('\n', stderr);
}

static SavepointLogFunction logFunction = DefaultLogFunction;

void SavepointSetLogFunction(const SavepointLogFunction& function)
{
    logFunction = function;
}

void SavepointLog(std::string_view string)
{
    logFunction(string);
}

template<typename ReturnT, typename MapT, typename KeyT, typename ProjectionT = std::identity>
static ReturnT GetOr(MapT& map, const KeyT& key, ProjectionT projection = {})
{
    auto it = map.find(key);
    if (it != map.end())
    {
        return std::invoke(projection, it->second);
    }
    else
    {
        return {};
    }
}

struct Hash
{
    using is_transparent = void;

    size_t operator()(std::string_view string) const
    {
        return std::hash<std::string_view>{}(string);
    }

    size_t operator()(const std::string& string) const
    {
        return std::hash<std::string_view>{}(string);
    }
};

static auto& GetPolyFunctions()
{
    static std::unordered_map<std::string, SavepointPolyFunction, Hash, std::equal_to<>> functions;
    return functions;
}

void SavepointAddPolyFunction(std::string_view string, const SavepointPolyFunction function)
{
    GetPolyFunctions().emplace(string, function);
}

SavepointPolyFunction SavepointGetPolyFunction(std::string_view string)
{
    return GetOr<SavepointPolyFunction>(GetPolyFunctions(), string);
}

SavepointPoly* SavepointReadPoly(SavepointVisitor& visitor)
{
    std::string string;
    visitor(string);
    SavepointPolyFunction function = SavepointGetPolyFunction(string);
    if (function == nullptr)
    {
        SavepointLog(std::format("Failed to find poly string: {}", string));
        visitor.SetError();
        return nullptr;
    }
    SavepointPoly* poly = function();
    if (poly)
    {
        visitor(*poly);
    }
    else
    {
        SavepointLog(std::format("Failed to allocate poly: {}", string));
        visitor.SetError();
    }
    return poly;
}

void SavepointWritePoly(SavepointPoly* poly, SavepointVisitor& visitor)
{
    std::string_view string = poly->GetClassName();
    visitor(string);
    visitor(*poly);
}

struct DebugFunction
{
    std::string Name;
    SavepointDebugFunction Function;
};

static auto& GetDebugFunctions()
{
    static std::unordered_map<uint32_t, DebugFunction> functions;
    return functions;
}

void SavepointAddDebugFunction(uint32_t id, std::string_view name, const SavepointDebugFunction function)
{
    GetDebugFunctions().emplace(id, DebugFunction{std::string{name}, function});
}

SavepointDebugFunction SavepointGetDebugFunction(uint32_t id)
{
    return GetOr<SavepointDebugFunction>(GetDebugFunctions(), id, &DebugFunction::Function);
}

std::string_view SavepointGetDebugTypeName(uint32_t id)
{
    return GetOr<std::string_view>(GetDebugFunctions(), id, &DebugFunction::Name);
}

void SavepointVersion::Visit(SavepointVisitor& visitor)
{
    visitor(Value);
}

void SavepointID::Visit(SavepointVisitor& visitor)
{
    visitor(Value);
}

Savepoint::~Savepoint()
{
    if (Driver && Driver->IsOpen())
    {
        Close();
    }
}

SavepointStatus Savepoint::Open(SavepointDriver driver, std::string_view path, SavepointVersion version, bool threadSafe, int maxWait)
{
    switch (driver)
    {
    case SavepointDriver::Null:
        Driver = std::make_unique<SavepointDriverNull>();
        break;
#ifdef SAVEPOINT_SQLITE3
    case SavepointDriver::SQLite3:
        Driver = std::make_unique<SavepointDriverSQLite3>();
        break;
#endif
    default:
        SavepointLog(std::format("Unknown driver: {}", std::to_underlying(driver)));
        return SavepointStatus::Failed;
    }
    if (!Driver)
    {
        SavepointLog(std::format("Failed to create driver: {}", std::to_underlying(driver)));
        return SavepointStatus::Failed;
    }
    Version = version;
    SavepointStatus status = Driver->Open(path, threadSafe, maxWait);
    if (status == SavepointStatus::Failed && Driver->IsOpen())
    {
        Driver->Close();
    }
    return status;
}

void Savepoint::Close()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Close();
    }
}

void Savepoint::Save()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Save();
    }
}

void Savepoint::Clear()
{
    if (Driver && Driver->IsOpen())
    {
        Driver->Clear();
    }
}
