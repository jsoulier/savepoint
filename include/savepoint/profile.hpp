#pragma once

/** @cond INTERNAL */

#ifdef SAVEPOINT_PROFILE
#include <tracy/Tracy.hpp>
#define SAVEPOINT_PROFILE_SCOPE() ZoneScoped
#define SAVEPOINT_PROFILE_SCOPEN(name) ZoneScopedN(name)
#else
#define SAVEPOINT_PROFILE_SCOPE()
#define SAVEPOINT_PROFILE_SCOPEN(name)
#endif

/** @endcond INTERNAL */
