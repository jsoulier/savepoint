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

static SavepointLogFunction& GetFunction()
{
    static SavepointLogFunction logFunction = DefaultLogFunction;
    return logFunction;
}

void SavepointSetLogFunction(const SavepointLogFunction& function)
{
    GetFunction() = function;
}

void SavepointLog(std::string_view string)
{
    GetFunction()(string);
}
