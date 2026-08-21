#pragma once

/** @cond INTERNAL */

#ifdef SAVEPOINT_PROFILE
#include <tracy/Tracy.hpp>
#define SAVEPOINT_PROFILE_SCOPE() ZoneScoped
#else
#define SAVEPOINT_PROFILE_SCOPE()
#endif

/** @endcond INTERNAL */
