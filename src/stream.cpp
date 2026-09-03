#include <savepoint/log.hpp>
#include <savepoint/stream.hpp>
#include <savepoint/visitor.hpp>

#include <ios>
#include <istream>
#include <ostream>
#include <vector>

std::istream& operator>>(std::istream& stream, SavepointVisitor& visitor)
{
    if (visitor.IsReading())
    {
        SavepointLog("Tried to read a stream into a reading visitor");
        stream.setstate(std::ios::failbit);
        return stream;
    }
    std::streampos begin = stream.tellg();
    stream.seekg(0, std::ios::end);
    std::streampos end = stream.tellg();
    if (!stream)
    {
        stream.setstate(std::ios::failbit);
        return stream;
    }
    int size = int(end - begin);
    std::vector<char> data(size);
    stream.seekg(begin);
    if (size && !stream.read(data.data(), size))
    {
        return stream;
    }
    visitor.Begin(data.data(), size);
    if (visitor.HasError())
    {
        stream.setstate(std::ios::failbit);
    }
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const SavepointVisitor& visitor)
{
    if (!visitor.IsWriting())
    {
        SavepointLog("Tried to write a reading visitor to a stream");
        stream.setstate(std::ios::failbit);
        return stream;
    }
    if (visitor.HasError())
    {
        stream.setstate(std::ios::failbit);
        return stream;
    }
    int size = visitor.GetSize();
    if (size)
    {
        stream.write(reinterpret_cast<const char*>(visitor.GetData()), size);
    }
    return stream;
}
