#pragma once

#include <cstdint>

class ISavepointDriver;
class Savepoint;
class SavepointPolymorph;
class SavepointEntity;
class SavepointVersion;
class SavepointVisitor;
enum class SavepointDriver : uint8_t;
enum class SavepointVisitorFlags : uint32_t;
