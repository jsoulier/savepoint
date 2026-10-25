#pragma once

#include <cstdint>

class ISavepointDriver;
class Savepoint;
class SavepointPolymorph;
class SavepointEntity;
class SavepointVersion;
class SavepointVisitor;
enum class SavepointFlags : uint32_t;
enum class SavepointVisitorFlags : uint8_t;
