#pragma once

// #define TSTE_PROFILE_ENABLE
#ifdef TSTE_PROFILE_ENABLE

#include <tracy/Tracy.hpp>

#define TSTE_PROFILE_SCP_N(__name) ZoneScopedN(__name)

#else

#define TSTE_PROFILE_SCP_N(__name)

#endif
