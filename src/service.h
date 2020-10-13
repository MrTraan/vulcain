#pragma once

#include "game_time.h"

enum class GameService {
	WATER,
	NUM_SERVICES, // keep me at the end
};

constexpr Duration servicesExpireAfter = DurationFromSeconds(20);

inline bool IsServiceFulfilled( TimePoint lastAccess, TimePoint now ) { return (now - lastAccess) <= servicesExpireAfter; }
