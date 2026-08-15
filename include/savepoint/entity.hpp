#pragma once

#include <savepoint/id.hpp>

/**
 * @brief Used to uniquely identify a Savepoint entry.
 * 
 * For objects that don't have unique locations, a base class is provided to
 * ensure the object gets a unique entry. When users write their object to the
 * Savepoint, Savepoint will use the base class to insert or update an entry.
 * Users should not modify the base class themselves.
 * 
 * @snippet examples/2_basic_usage.cpp 2_basic_usage
 * @see Savepoint
 * @see SavepointID
 */
class SavepointEntity
{
    friend class Savepoint;

public:
    /**
     * @brief Get the unique entity ID.
     * 
     * A limitation of Savepoint is that you can't serialize references to other entities.
     * Instead of a reference, it will create a copy and unassociate the reference when
     * deserialized. If you need to maintain references, you can serialize the ID instead.
     * 
     * @see SavepointID
     * @return The unique ID.
     */
    SavepointID GetID() const
    {
        return ID;
    }

private:
    SavepointID ID;
};
