#pragma once

#include "types.h"
#include <chrono>

namespace ng {
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::duration<int64, std::micro>;

inline TimePoint ClockNow() { return std::chrono::high_resolution_clock::now(); }

inline Duration DurationInMs( float duration ) { return std::chrono::microseconds( ( int64 )( duration * 1000.0 ) ); }
inline Duration DurationInSeconds( float duration ) { return std::chrono::microseconds( ( int64 )( duration * 1000000.0 ) ); }

inline float DurationToSeconds( const Duration & duration ) { return std::chrono::duration<float>(duration).count(); }

}; // namespace ng