#pragma once

#include <savepoint/fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

/**
 * @brief Used to uniquely identify a Savepoint entry.
 * 
 * @see SavepointEntity
 */
class SavepointID
{
    friend class Savepoint;

    SavepointID(int value)
        : Value{value}
    {
    }

public:
    /**
     * @brief Reserved value for an invalid ID.
     */
    static constexpr int kInvalidID = std::numeric_limits<int>::max();

    /**
     * @brief Default initialize an invalid ID.
     */
    constexpr SavepointID()
        : Value{kInvalidID}
    {
    }

    /**
     * @brief Compare the ID to another ID.
     * 
     * @param other The other ID.
     * @return True if the comparison evaluated to true.
     */
    constexpr auto operator<=>(const SavepointID& other) const = default;

    /**
     * @brief Check if an ID is valid.
     * 
     * @return True if the ID is valid.
     */
    constexpr bool IsValid() const
    {
        return Value != SavepointID{}.Value;
    }

    /**
     * @brief Get the raw ID value.
     *
     * @return The raw ID value.
     */
    constexpr int GetValue() const
    {
        return Value;
    }

    /**
     * @brief Serialize the ID.
     *
     * @param visitor The visitor.
     */
    void Visit(SavepointVisitor& visitor);

private:
    int Value;
};

namespace std
{

/**
 * @brief Hash implementation for a SavepointID.
 */
template <>
struct hash<SavepointID>
{
    /**
     * @brief Hash a SavepointID.
     * 
     * @param id The ID.
     * @return The hash.
     */
    size_t operator()(const SavepointID& id) const noexcept
    {
        return std::hash<uint32_t>{}(id.GetValue());
    }
};

}
