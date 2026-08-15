#pragma once

#include <functional>
#include <string_view>

/**
 * @brief The log function signature.
 * 
 * @param string The log message.
 */
using SavepointLogFunction = std::function<void(std::string_view string)>;

/**
 * @brief Set the log function used by SavepointLog. Defaults to stderr.
 * 
 * @param function The log function.
 */
void SavepointSetLogFunction(const SavepointLogFunction& function);

/**
 * @brief Invoke the currently set log function.
 * 
 * @param string The log message.
 */
void SavepointLog(std::string_view string);
