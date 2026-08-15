#include <savepoint/version.hpp>
#include <savepoint/visitor.hpp>

void SavepointVersion::Visit(SavepointVisitor& visitor)
{
    visitor(Value);
}
