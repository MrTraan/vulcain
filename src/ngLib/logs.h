#pragma once

#include "types.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

using ngString = std::string;

namespace ng {
enum LogSeverity {
	LOG_INFO,
	LOG_ERROR,
	NUM_LOG_SEVERITY, // Keep me at the end plz
};

void Printf( const char * fmt, ... );
void Errorf( const char * fmt, ... );
void LogV( const char * fmt, va_list args, LogSeverity severity );

struct ScopedChrono {
	std::chrono::steady_clock::time_point start;
	const char *                          label;
	ScopedChrono( const char * _label ) {
		start = std::chrono::high_resolution_clock::now();
		label = _label;
	}

	~ScopedChrono() {
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast< std::chrono::milliseconds >( end - start ).count();
		if ( duration > 5 ) {
			ng::Printf( "%s: %lldms\n", label, duration );
		} else {
			ng::Printf( "%s: %lldus\n", label,
			            std::chrono::duration_cast< std::chrono::microseconds >( end - start ).count() );
		}
	}
};

struct Console {
	struct Log {
		ngString    text;
		LogSeverity severity;
		int64       timestamp;
	};

	std::vector< Log > logs;
	std::mutex         mutex;

	void PrintLog( const char * text, LogSeverity severity );
	void Draw();
};

Console & GetConsole();

}; // namespace ng