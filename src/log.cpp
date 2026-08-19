#include <savepoint/log.hpp>

#include <cstdio>
#include <mutex>
#include <string_view>

static std::mutex mutex;

static void DefaultLogFunction(std::string_view string)
{
    std::scoped_lock lock(mutex);
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
