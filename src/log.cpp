#include <savepoint/log.hpp>

#include <cstdio>
#include <string_view>

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
