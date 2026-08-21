// [9_version]
#include <savepoint/savepoint.hpp>

#include "assert.hpp"

int main()
{
    static constexpr SavepointVersion kDefaultVersion;
    static constexpr SavepointVersion kVersion{1, 2, 3};

    static_assert(kDefaultVersion == SavepointVersion{0, 0, 0});
    static_assert(kVersion.GetMajor() == 1);
    static_assert(kVersion.GetMinor() == 2);
    static_assert(kVersion.GetPatch() == 3);

    static_assert(kVersion == SavepointVersion{1, 2, 3});
    static_assert(kVersion != SavepointVersion{1, 2, 4});
    static_assert(kVersion < SavepointVersion{2, 0, 0});
    static_assert(kVersion > SavepointVersion{1, 2, 2});
    static_assert(kVersion <= SavepointVersion{1, 2, 3});
    static_assert(kVersion >= SavepointVersion{1, 2, 3});

    ASSERT(kVersion.GetString() == "1.2.3");
    return 0;
}
// [9_version]
