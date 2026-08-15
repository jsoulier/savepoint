#include <savepoint/log.hpp>
#include <savepoint/polymorph.hpp>
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

SavepointPolymorph* SavepointReadPolymorph(SavepointVisitor& visitor)
{
    std::string string;
    visitor(string);
    SavepointPolymorphFunction function = SavepointGetPolymorphFunction(string);
    if (function == nullptr)
    {
        SavepointLog(std::format("Failed to find polymorph string: {}", string));
        visitor.SetError();
        return nullptr;
    }
    SavepointPolymorph* polymorph = function();
    if (polymorph)
    {
        visitor(*polymorph);
    }
    else
    {
        SavepointLog(std::format("Failed to allocate polymorph: {}", string));
        visitor.SetError();
    }
    return polymorph;
}

void SavepointWritePolymorph(SavepointPolymorph* polymorph, SavepointVisitor& visitor)
{
    std::string_view string = polymorph->GetClassName();
    visitor(string);
    visitor(*polymorph);
}
