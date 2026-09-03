// [19_stream]
#include <savepoint/savepoint.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "assert.hpp"

struct Object
{
    int Value;
    std::string Name;
    std::vector<float> Samples;

    Object()
        : Value{}
    {
    }

    Object(int value, std::string name, std::vector<float> samples)
        : Value{value}
        , Name{std::move(name)}
        , Samples{std::move(samples)}
    {
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Value);
        visitor(Name);
        visitor(Samples);
    }

    bool operator==(const Object& other) const
    {
        return Value == other.Value && Name == other.Name && Samples == other.Samples;
    }
};

int main()
{
    SavepointVisitor writer;
    Object input{42, "stream", {1.0f, 2.0f, 3.0f}};
    writer.Begin(input, SavepointVersion{});

    std::stringstream stream1{std::ios::in | std::ios::out | std::ios::binary};
    stream1 << writer;
    ASSERT(stream1.good());
    stream1.seekg(0);

    SavepointVisitor reader;
    stream1 >> reader;
    ASSERT(stream1.good());

    Object output;
    reader(output);
    ASSERT(!reader.HasError());
    ASSERT(reader.IsEmpty());
    ASSERT(output == input);

    // The stream tried to write a reader
    std::stringstream stream2;
    stream2 << reader;
    ASSERT(stream2.fail());

    stream1.clear();
    stream1.seekg(0);
    stream1 >> reader;
    ASSERT(stream1.fail());
    return 0;
}
// [19_stream]
