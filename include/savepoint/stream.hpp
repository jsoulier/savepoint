#pragma once

#include <iosfwd>

class SavepointVisitor;

/**
 * @brief Read the remaining bytes in a stream into a visitor.
 *
 * @param stream The input stream.
 * @param visitor The visitor.
 * @return The input stream.
 */
std::istream& operator>>(std::istream& stream, SavepointVisitor& visitor);

/**
 * @brief Write a visitor's serialized bytes to a stream.
 *
 * @param stream The output stream.
 * @param visitor The visitor.
 * @return The output stream.
 */
std::ostream& operator<<(std::ostream& stream, const SavepointVisitor& visitor);
