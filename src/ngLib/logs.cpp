#include "logs.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
//#include <debugapi.h>
#endif

namespace ng {

void Printf( const char * fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	LogV( fmt, args, LogSeverity::LOG_DEFAULT );
	va_end( args );
}

void Debugf( const char * fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	LogV( fmt, args, LogSeverity::LOG_DEBUG );
	va_end( args );
}

void Infof( const char * fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	LogV( fmt, args, LogSeverity::LOG_INFO );
	va_end( args );
}

void Errorf( const char * fmt, ... ) {
	va_list args;
	va_start( args, fmt );
	LogV( fmt, args, LogSeverity::LOG_ERROR );
	va_end( args );
}

static constexpr const char * prefixBySeverity[] = {
    "Debug: ", // DEBUG
    "",        // DEFAULT
    "Info: ",  // INFO
    "Error: ", // ERROR
    nullptr,
};

void LogV( const char * fmt, va_list args, LogSeverity severity ) {
	const char * prefix = prefixBySeverity[ severity ];
	size_t       prefixSize = strlen( prefix );
	char * buf = new char[ NG_MAX_LOG_SIZE + prefixSize + 1 ];

	strcpy( buf, prefix );

	if ( vsnprintf( buf + prefixSize, ( size_t )NG_MAX_LOG_SIZE + 1, fmt, args ) < 0 ) {
		return;
	}

#ifdef _WIN32
	// OutputDebugString( buf );
#endif

	// FILE * fd = severity == LOG_ERROR ? stderr : stdout;
	FILE * fd = stdout;
	fprintf( fd, "%s", buf );

	GetConsole().PrintLog( buf, severity );

	delete[] buf;
}

}; // namespace ng
