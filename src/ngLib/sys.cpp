#include "sys.h"
#include "nglib.h"

#if defined( _WIN32 )
#include <Windows.h>
#endif

#if defined( _WIN32 )
#include <filesystem>
#elif defined( __linux ) || defined( __APPLE__ )
#include <dirent.h>
#include <sys/types.h>
#else
NG_UNSUPPORTED_PLATFORM
#endif

namespace ng {

static int64 clockTicksPerSecond = 0;
static int64 clockTicksAtStartup = 0;

void InitSys() {
#if defined( _WIN32 )
	LARGE_INTEGER li;
	QueryPerformanceFrequency( &li );
	clockTicksPerSecond = li.QuadPart;
	QueryPerformanceCounter( &li );
	clockTicksAtStartup = li.QuadPart;
#elif defined( __linux )
	NG_UNSUPPORTED_PLATFORM
#elif defined( __APPLE__ )
	NG_UNSUPPORTED_PLATFORM
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

int64 SysGetTimeInMicro() {
#if defined( _WIN32 )
	LARGE_INTEGER li;
	QueryPerformanceCounter( &li );
	int64 ticks = li.QuadPart - clockTicksAtStartup;
	return ticks * 1000000 * clockTicksPerSecond;
#elif defined( __linux )
	NG_UNSUPPORTED_PLATFORM
#elif defined( __APPLE__ )
	NG_UNSUPPORTED_PLATFORM
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

float SysGetTimeInMs() { return ( float )SysGetTimeInMicro() / 1000.0f; }

FileOffset File::TellOffset() const {
#if defined( SYS_WIN )
	LARGE_INTEGER offset;
	LARGE_INTEGER liOffset;
	liOffset.QuadPart = 0;
	BOOL res = SetFilePointerEx( handler, liOffset, &offset, FILE_CURRENT );
	ng_assert( res != 0 );
	return offset.QuadPart;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool File::SeekOffset( FileOffset offset, SeekWhence whence ) {
#if defined( _WIN32 )
	DWORD moveMethod = 0;
	switch ( whence ) {
	case SeekWhence::START:
		moveMethod = FILE_BEGIN;
		break;
	case SeekWhence::CUR:
		moveMethod = FILE_CURRENT;
		break;
	case SeekWhence::END:
		moveMethod = FILE_END;
		break;
	}
	LARGE_INTEGER liOffset;
	liOffset.QuadPart = offset;
	BOOL res = SetFilePointerEx( handler, liOffset, nullptr, moveMethod );
	ng_assert( res != 0 );
	return res;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool File::Open( const char * path, int mode ) {
	this->mode = mode;
	this->path = path;
	char fopenStr[ 4 ] = { 'b', 0, 0, 0 };

#if defined( SYS_WIN )
	DWORD dwDesiredAccess = 0;
	if ( ModeCanRead() ) {
		dwDesiredAccess |= GENERIC_READ;
	}
	if ( ModeCanWrite() ) {
		dwDesiredAccess |= GENERIC_WRITE;
	}
	DWORD dwCreationDisposition = 0;
	if ( !ModeCanCreate() ) {
		dwCreationDisposition = OPEN_EXISTING;
	} else {
		if ( ModeCanTruncate() ) {
			dwCreationDisposition = CREATE_ALWAYS;
		} else {
			dwCreationDisposition = OPEN_ALWAYS;
		}
	}
	handler = CreateFileA( path, dwDesiredAccess, 0, nullptr, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr );
	ng_assert_msg( handler != INVALID_HANDLE_VALUE, "CreateFileA failed with error code %lu", GetLastError() );
	return handler != INVALID_HANDLE_VALUE;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool File::Close() {
	ng_assert( handler != INVALID_HANDLER );
#if defined( SYS_WIN )
	return CloseHandle( handler );
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

size_t File::Read( void * dst, size_t size ) {
	ng_assert( handler != INVALID_HANDLER );
	ng_assert( ModeCanRead() );
#if defined( SYS_WIN )
	DWORD bytesRead;
	BOOL  success = ReadFile( handler, dst, size, &bytesRead, nullptr );
	// ng_assert( success == 0 );
	return ( size_t )bytesRead;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

size_t File::Write( const void * src, size_t size ) {
	ng_assert( handler != INVALID_HANDLER );
	ng_assert( ModeCanWrite() );
#if defined( SYS_WIN )
	DWORD bytesRead;
	BOOL  success = WriteFile( handler, src, size, &bytesRead, nullptr );
	ng_assert( success != 0 );
	return ( size_t )bytesRead;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

int64 File::GetSize() const {
#if defined( SYS_WIN )
	LARGE_INTEGER size;
	BOOL          success = GetFileSizeEx( handler, &size );
	ng_assert( success != 0 );
	return size.QuadPart;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

void File::Truncate() {
#if defined( SYS_WIN )
	SeekOffset( 0, SeekWhence::START );
	BOOL success = SetEndOfFile( handler );
	ng_assert( success != 0 );
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool ListFilesInDirectory( const char *                 path,
                           std::vector< std::string > & results,
                           ListFileMode                 mode /*= ListFileMode::NORMAL */ ) {
	bool success = true;
#if defined( SYS_WIN )
	for ( const auto & entry : std::filesystem::directory_iterator( path ) ) {
		if ( entry.is_directory() && mode == ListFileMode::RECURSIVE ) {
			success &= ListFilesInDirectory( entry.path().string().c_str(), results, mode );
		} else {
			results.push_back( entry.path().string() );
		}
	}
#elif defined( SYS_UNIX )
	DIR * dir = opendir( path );
	if ( dir == nullptr ) {
		return;
	}

	dirent * dirFiles;
	while ( ( dirFiles = readdir( dir ) ) != nullptr ) {
		if ( strcmp( dirFiles->d_name, "." ) == 0 || strcmp( dirFiles->d_name, ".." ) == 0 )
			continue;
		if ( dirFiles->d_type == DT_DIR ) {
			success &= ListFilesInDirectory( dirFiles->d_name, results, mode );
		} else {
			std::string str = path;
			str += "/";
			str += dirFiles->d_name;
			results.push_back( str );
		}
	}
	closedir( dir );
#else
	NG_UNSUPPORTED_PLATFORM
#endif
	return success;
}

bool FileExists( const char * path ) {
#if defined( SYS_WIN )
	DWORD attributes = GetFileAttributesA( path );
	return attributes != INVALID_FILE_ATTRIBUTES;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool IsDirectory( const char * path ) {
#if defined( SYS_WIN )
	DWORD attributes = GetFileAttributesA( path );
	ng_assert( attributes != INVALID_FILE_ATTRIBUTES );
	return ( attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool CreateDirectory( const char * path ) {
#if defined( SYS_WIN )
	BOOL success = ::CreateDirectoryA( path, nullptr );
	ng_assert( success != 0 );
	return success != 0;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

}; // namespace ng
