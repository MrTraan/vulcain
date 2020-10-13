#pragma once

#include "ngLib/types.h"

constexpr int64 numTicksPerSeconds = 60;
constexpr float FIXED_TIMESTEP = 1.0f / ( float )numTicksPerSeconds;

using TimePoint = int64; // a time point is expressed as the number of fixed update ticks since the world was created
using Duration = int64;  // a duration is the delta between two time points

constexpr inline Duration DurationFromSeconds( int64 seconds ) { return seconds * numTicksPerSeconds; }
constexpr inline Duration DurationFromMs( int64 ms ) { return ms / ( int64 )( FIXED_TIMESTEP * 1000ll ); }
constexpr inline float    ConvertPerSecondToPerTick( float x ) { return x * FIXED_TIMESTEP; }
constexpr inline float    DurationToSeconds( Duration duration ) { return duration * FIXED_TIMESTEP; }
