#pragma once

#include <savepoint/fwd.hpp>

#include <cstdint>
#include <format>
#include <string>

/**
 * @brief Used to specify a Savepoint version.
 * 
 * For versioning members and performing quick comparisons, a version wrapper
 * is provided. The version consists of a major, minor, and patch version (with
 * decreasing significance). Versions are packed into a u32 so comparisons are
 * cheap. Versions can also be assigned and compared at compile time.
 * 
 * @snippet examples/9_version.cpp 9_version
 * @see Savepoint
 */
class SavepointVersion
{
public:
    /**
     * @brief Default initializes the version to 0.0.0.
     */
    constexpr SavepointVersion()
        : Value{0}
    {
    }

    /**
     * @brief Initialize the version to major.minor.patch.
     * 
     * @param major The major version.
     * @param minor The minor version.
     * @param patch The patch version.
     */
    constexpr SavepointVersion(uint32_t major, uint32_t minor, uint32_t patch)
        : Value{major << 24 | minor << 16 | patch}
    {
    }

    /**
     * @brief Get the major version.
     * 
     * @return The major version.
     */
    constexpr uint32_t GetMajor() const
    {
        return (Value >> 24) & 0xFF;
    }

    /**
     * @brief Get the minor version.
     * 
     * @return The minor version.
     */
    constexpr uint32_t GetMinor() const
    {
        return (Value >> 16) & 0xFF;
    }

    /**
     * @brief Get the patch version.
     * 
     * @return The patch version.
     */
    constexpr uint32_t GetPatch() const
    {
        return Value & 0xFFFF;
    }

    /**
     * @brief Get the version as a string in the format major.minor.patch.
     * 
     * @return The version as a string.
     */
    std::string GetString() const
    {
        return std::format("{}.{}.{}", GetMajor(), GetMinor(), GetPatch());
    }

    /**
     * @brief Compare the version to another version.
     * 
     * @param other The other version.
     * @return True if the comparison evaluated to true.
     */
    constexpr auto operator<=>(const SavepointVersion& other) const = default;

    /**
     * @brief Serialize the version.
     *
     * @param visitor The visitor.
     */
    void Visit(SavepointVisitor& visitor);

private:
    uint32_t Value;
};

/**
 * @brief The current savepoint version.
 */
static constexpr SavepointVersion kSavepointVersion{1, 0, 0};
