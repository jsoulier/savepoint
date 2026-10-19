// [19_stream]
#include <savepoint/savepoint.hpp>

#include <iomanip>
#include <istream>
#include <ostream>
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

    bool operator==(const Object& other) const
    {
        return Value == other.Value && Name == other.Name && Samples == other.Samples;
    }
};

static std::ostream& operator<<(std::ostream& stream, const Object& object)
{
    stream << object.Value << ' ' << std::quoted(object.Name) << ' ' << object.Samples.size();
    for (float sample : object.Samples)
    {
        stream << ' ' << sample;
    }
    return stream;
}

static std::istream& operator>>(std::istream& stream, Object& object)
{
    int count{};
    stream >> object.Value >> std::quoted(object.Name) >> count;
    object.Samples.resize(count);
    for (float& sample : object.Samples)
    {
        stream >> sample;
    }
    return stream;
}

int main()
{
    SavepointVisitor writer;
    Object input{42, "MyObject", {1.0f, 0.1f, 1.0f / 3.0f}};
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
