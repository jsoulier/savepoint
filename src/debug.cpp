#include <savepoint/debug.hpp>

#ifdef SAVEPOINT_DEBUGGER

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "map.hpp"

struct DebugFunction
{
    std::string Name;
    SavepointDebugFunction Function;
};

static Map<uint32_t, DebugFunction>& GetFunctions()
{
    static Map<uint32_t, DebugFunction> functions;
    return functions;
}

void SavepointAddDebugFunction(uint32_t id, std::string_view name, const SavepointDebugFunction function)
{
    GetFunctions().emplace(id, DebugFunction{std::string{name}, function});
}

SavepointDebugFunction SavepointGetDebugFunction(uint32_t id)
{
    return GetOr<SavepointDebugFunction>(GetFunctions(), id, &DebugFunction::Function);
}

std::string_view SavepointGetDebugTypeName(uint32_t id)
{
    return GetOr<std::string_view>(GetFunctions(), id, &DebugFunction::Name);
}

#endif
