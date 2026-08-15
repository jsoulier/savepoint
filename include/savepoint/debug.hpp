#pragma once

#include <savepoint/fwd.hpp>
#include <savepoint/traits.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#ifdef SAVEPOINT_DEBUGGER

/**
 * @brief Debug representation of a SavepointVisitor::Visit.
 */
class SavepointDebugNode
{
    friend class SavepointVisitor;

public:
    SavepointDebugNode()
        : Depth{0}
    {
    }

    /**
     * @brief Get how deeply nested the node is (zero at the root). Useful for indentation
     *
     * @return The depth.
     */
    int GetDepth() const
    {
        return Depth;
    }

    /**
     * @brief Get the node's type name. 
     *
     * @return The type name.
     */
    std::string_view GetTypeName() const
    {
        return Type;
    }

    /**
     * @brief Get the node's value. Empty if the node is a leaf.
     *
     * @return The value.
     */
    const std::string& GetValue() const
    {
        return Value;
    }

    /**
     * @brief Check if a node has no children.
     *
     * @return True if the node has no children.
     */
    bool GetIsLeaf() const
    {
        return !Value.empty();
    }

private:
    int Depth;
    std::string Type;
    std::string Value;
};

/** @cond INTERNAL */

// Type-erased function for populating a visitor's debug information
using SavepointDebugFunction = void(*)(SavepointVisitor& visitor);

// Register a debug function. Does nothing for ID collisions
void SavepointAddDebugFunction(uint32_t id, std::string_view name, const SavepointDebugFunction function);

// Get a debug function registered to an ID
SavepointDebugFunction SavepointGetDebugFunction(uint32_t id);

// Get the type name registered to an ID
std::string_view SavepointGetDebugTypeName(uint32_t id);

// For registering debug functions on instantiation of Savepoint::Read/Write
template<typename T>
struct SavepointDebugRegistrar
{
    static inline const bool kRegistered = []
    {
        static_assert(std::is_default_constructible_v<T>);
        SavepointAddDebugFunction(
            SavepointTypeID<T>(),
            SavepointTypeName<T>::kValue,
            [](SavepointVisitor& visitor)
            {
                T item{};
                visitor(item);
            }
        );
        return true;
    }();
};

/** @endcond */

#endif
