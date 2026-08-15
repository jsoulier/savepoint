#pragma once

#include <savepoint/fwd.hpp>

#include <string>
#include <string_view>

/**
 * @brief Serves as the base class for the user's base class.
 * 
 * To support polymorphic types, Savepoint offers a base class you can use.
 * Savepoint checks if visited objects inherit from SavepointPolymorph and if so, 
 * serializes the object alongside its type information. When Savepoint reads
 * the type information out, it knows to instantiate the derived class.
 * 
 * @snippet examples/8_polymorphic_types.cpp 8_polymorphic_types
 * @see SAVEPOINT_POLYMORPH
 */
class SavepointPolymorph
{
public:
    /**
     * @brief Default destructor.
     */
    virtual ~SavepointPolymorph() = default;

    /**
     * @brief The Visit method to be called from SavepointVisitor.
     * 
     * @param visitor The visitor.
     * @see SavepointVisitor
     */
    virtual void Visit(SavepointVisitor& visitor) {}

    /**
     * @brief Get the class name string of the underlying object.
     * 
     * @return The class name string.
     */
    virtual std::string_view GetClassName() const = 0;
};

/**
 * @brief Helper for concrete derived classes to implement SavepointPolymorph methods.
 * 
 * Implements GetClassName and automatically registers a factory function
 * for the derived class. It allows a SavepointVisitor to to create an instance of
 * the derived class whilst only knowing its class name.
 * 
 * @param T The class type.
 * @see SavepointPolymorph
 */
#define SAVEPOINT_POLYMORPH(T) \
    private: \
        struct SavepointRegistrar \
        { \
            static SavepointPolymorph* Function() \
            { \
                return new T(); \
            } \
            SavepointRegistrar() \
            { \
                SavepointAddPolymorphFunction(#T, Function); \
            } \
        }; \
        static inline SavepointRegistrar SavepointRegistrar; \
    public: \
        std::string_view GetClassName() const override \
        { \
            return #T; \
        } \

/** @cond INTERNAL */

// Polymorph factory function signature
using SavepointPolymorphFunction = SavepointPolymorph*(*)();

// Register a factory function for polymorph types
void SavepointAddPolymorphFunction(std::string_view string, const SavepointPolymorphFunction function);

// Get a factory function for polymorph types
SavepointPolymorphFunction SavepointGetPolymorphFunction(std::string_view string);

// Read the visitor to get the polymorph factory and create the polymorph instance
SavepointPolymorph* SavepointReadPolymorph(SavepointVisitor& visitor);

// Write the polymorph instance to the visitor
void SavepointWritePolymorph(SavepointPolymorph* polymorph, SavepointVisitor& visitor);

/** @endcond */
