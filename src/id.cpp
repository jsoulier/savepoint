#include <savepoint/id.hpp>
#include <savepoint/visitor.hpp>

void SavepointID::Visit(SavepointVisitor& visitor)
{
    visitor(Value);
}
