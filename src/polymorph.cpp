#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
#include <savepoint/profile.hpp>
#include <savepoint/std.hpp>
#include <savepoint/visitor.hpp>

#include <format>
#include <string>
#include <string_view>

#include "map.hpp"

static Map<std::string, SavepointPolymorphFunction>& GetFunctions()
{
    static Map<std::string, SavepointPolymorphFunction> functions;
    return functions;
}

void SavepointAddPolymorphFunction(std::string_view string, const SavepointPolymorphFunction function)
{
    GetFunctions().emplace(string, function);
}

SavepointPolymorphFunction SavepointGetPolymorphFunction(std::string_view string)
{
    return GetOr<SavepointPolymorphFunction>(GetFunctions(), string);
}

SavepointPolymorph* SavepointCreatePolymorph(std::string_view string)
{
    SAVEPOINT_PROFILE_SCOPE();
    SavepointPolymorphFunction function = SavepointGetPolymorphFunction(string);
    if (!function)
    {
        SavepointLog(std::format("Failed to find polymorph string: {}", string));
        return nullptr;
    }
    return function();
}
