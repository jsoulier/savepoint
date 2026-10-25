#include <savepoint/savepoint.hpp>

#include <array>
#include <deque>
#include <istream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

static_assert(SavepointIsPointer<void*>);
static_assert(SavepointIsPointer<void**>);
static_assert(SavepointIsPointer<const void*>);
static_assert(SavepointIsPointer<void const*>);
static_assert(SavepointIsPointer<const void* const>);
static_assert(!SavepointIsPointer<int>);
static_assert(!SavepointIsPointer<std::shared_ptr<int>>); // std::shared_ptr is unsupported
static_assert(SavepointIsPointer<std::unique_ptr<int>>);

struct NoVisit {};
struct MemberVisit { void Visit(SavepointVisitor& visitor) {} };
struct FreeVisit {};
static void Visit(SavepointVisitor& visitor, FreeVisit& item) {}
struct StreamVisit { int Value; };
static std::ostream& operator<<(std::ostream& stream, const StreamVisit& item) { return stream << item.Value; }
static std::istream& operator>>(std::istream& stream, StreamVisit& item) { return stream >> item.Value; }
enum class SignedEnum : int8_t {};
enum class UnsignedEnum : uint8_t {};

static_assert(SavepointHasFreeVisit<FreeVisit>);
static_assert(!SavepointHasFreeVisit<MemberVisit>);
static_assert(!SavepointHasFreeVisit<NoVisit>);
static_assert(!SavepointHasMemberVisit<FreeVisit>);
static_assert(SavepointHasMemberVisit<MemberVisit>);
static_assert(!SavepointHasMemberVisit<NoVisit>);
static_assert(SavepointHasStreamOperators<StreamVisit>);
static_assert(SavepointHasStreamOperators<int>);
static_assert(SavepointHasStreamOperators<std::string>);
static_assert(!SavepointHasFreeVisit<StreamVisit>);

static_assert(std::is_same_v<SavepointPortableTypeConverter<int8_t>::Type, int8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<int16_t>::Type, int16_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<int32_t>::Type, int32_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<int64_t>::Type, int64_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<uint8_t>::Type, uint8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<uint16_t>::Type, uint16_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<uint32_t>::Type, uint32_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<uint64_t>::Type, uint64_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<bool>::Type, uint8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<char>::Type, int8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<SignedEnum>::Type, int8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<UnsignedEnum>::Type, uint8_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<float>::Type, float>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<double>::Type, double>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<long>::Type, int64_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<unsigned long>::Type, uint64_t>);
static_assert(std::is_same_v<SavepointPortableTypeConverter<wchar_t>::Type, uint32_t>);

static_assert(SavepointIsCopyable<int>);
static_assert(SavepointIsCopyable<SignedEnum>);
static_assert(!SavepointIsCopyable<NoVisit>);
static_assert(!SavepointIsCopyable<MemberVisit>);
static_assert(!SavepointIsCopyable<FreeVisit>);

static_assert(!SavepointIsDynamicRange<std::array<int, 1>>);
static_assert(SavepointIsDynamicRange<std::vector<int>>);
static_assert(!SavepointIsDynamicRange<std::string_view>);
static_assert(SavepointIsDynamicRange<std::map<int, int>>);
static_assert(SavepointIsDynamicRange<std::unordered_map<int, int>>);
static_assert(SavepointIsDynamicRange<std::set<int>>);
static_assert(SavepointIsDynamicRange<std::unordered_set<int>>);
static_assert(SavepointIsDynamicRange<std::deque<int>>);
static_assert(SavepointIsDynamicRange<std::string>);
static_assert(SavepointIsDynamicRange<std::list<int>>);

static_assert(SavepointIsStaticRange<std::array<int, 1>>);
static_assert(!SavepointIsStaticRange<std::vector<int>>);
static_assert(!SavepointIsStaticRange<std::string_view>);
static_assert(!SavepointIsStaticRange<std::map<int, int>>);
static_assert(!SavepointIsStaticRange<std::unordered_map<int, int>>);
static_assert(!SavepointIsStaticRange<std::set<int>>);
static_assert(!SavepointIsStaticRange<std::unordered_set<int>>);
static_assert(!SavepointIsStaticRange<std::string>);
static_assert(!SavepointIsStaticRange<std::deque<int>>);

static_assert(!SavepointHasMemberVisit<std::string>);
static_assert(SavepointHasFreeVisit<std::string>);

static_assert(SavepointIsTuple<std::tuple<int>>);
static_assert(SavepointIsTuple<std::tuple<int, int>>);
static_assert(SavepointIsTuple<std::tuple<int, int, int>>);
static_assert(SavepointIsTuple<std::tuple<>>);
static_assert(SavepointIsTuple<std::pair<int, int>>);

static_assert(SavepointIsOptional<std::optional<int>>);
static_assert(!SavepointIsOptional<std::tuple<int>>);

int main()
{
    return 0;
}
