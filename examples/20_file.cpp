// [20_file]
#include <savepoint/savepoint.hpp>

#include <string>
#include <vector>

#include "assert.hpp"

static constexpr SavepointVersion kVersion{0, 0, 0};

struct Settings
{
    SavepointFile File;
    std::string Name;
    std::vector<int> Values;

    Settings()
        : File{"savepoint_20_file_example.bin"}
        , Name{"file"}
        , Values{1, 2, 3}
    {
        ASSERT(File.Write(*this, kVersion));
        Name.clear();
        Values.clear();
        ASSERT(File.TryRead(*this));
        ASSERT(Name == "file");
        ASSERT(Values.size() == 3);
        ASSERT(Values[0] == 1);
        ASSERT(Values[1] == 2);
        ASSERT(Values[2] == 3);
        ASSERT(File.Delete());
        Name.clear();
        Values.clear();
        ASSERT(!File.TryRead(*this));
        ASSERT(Name.empty());
        ASSERT(Values.empty());
    }

    void Visit(SavepointVisitor& visitor)
    {
        visitor(Name);
        visitor(Values);
    }
};

int main()
{
    Settings settings;
    return 0;
}
// [20_file]
