#pragma once

/**
 * @brief The statuses returned by Savepoint.
 */
enum class SavepointStatus
{
    Failed,   /**< Failed to open a Savepoint for any reason */
    Existing, /**< Opened an existing Savepoint */
    New,      /**< Created a new Savepoint */
};
