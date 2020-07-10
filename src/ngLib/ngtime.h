#pragma once

#include "types.h"
#include <chrono>

namespace ng {
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::steady_clock::duration;

inline TimePoint ClockNow() { return std::chrono::high_resolution_clock::now(); }

inline Duration DurationInMs( float duration ) { return std::chrono::microseconds( ( int64 )( duration * 1000.0f ) ); }
inline Duration DurationInSeconds( float duration ) { return std::chrono::microseconds( ( int64 )( duration * 1000000.0f ) ); }

}; // namespace ng