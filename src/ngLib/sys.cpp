#include "sys.h"
#include "nglib.h"

#if defined( _WIN32 )
#include <Windows.h>
#endif

#if defined( _WIN32 )
#include <filesystem>
#elif defined( SYS_UNIX )
#include <dirent.h>
#include <sys/types.h>
#else
NG_UNSUPPORTED_PLATFORM
#endif

namespace ng {

static int64 clockTicksPerSecond = 0;
static int64 clockTicksAtStartup = 0;
static int64 timeMicroAtStartup = 0;

void InitSys() {
#if defined( _WIN32 )
	LARGE_INTEGER li;
	QueryPerformanceFrequency( &li );
	clockTicksPerSecond = li.QuadPart;
	QueryPerformanceCounter( &li );
	clockTicksAtStartup = li.QuadPart;
#elif defined( SYS_UNIX )
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
	timeMicroAtStartup = tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
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
#elif defined( SYS_UNIX )
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
	return (tv.tv_sec * 1000000 + tv.tv_nsec / 1000) - timeMicroAtStartup;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

float SysGetTimeInMs() { return ( float )SysGetTimeInMicro() / 1000.0f; }

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
#elif defined( SYS_UNIX )
	int open_mode = 0;
	if ( ModeCanRead() && !ModeCanWrite() ) {
		open_mode |= O_RDONLY;
	}
	if ( !ModeCanRead() && ModeCanWrite() ) {
		open_mode |= O_WRONLY;
	}
	if ( ModeCanRead() && ModeCanWrite() ) {
		open_mode |= O_RDWR;
	}
	if ( ModeCanCreate() ) {
		open_mode |= O_CREAT;
	}
	if ( ModeCanTruncate() ) {
		open_mode |= O_TRUNC;
	}
	this->open_mode = open_mode;
	int fd = open( path, open_mode );
	ng_assert_msg( fd != INVALID_FD, "open failed with error '%s'", strerror( errno ) );
	return fd != INVALID_FD;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

bool File::Close() {
#if defined( SYS_WIN )
	ng_assert( handler != INVALID_HANDLER );
	return CloseHandle( handler );
#elif defined( SYS_UNIX )
	ng_assert( fd != INVALID_FD );
	return close( fd ) != -1;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

size_t File::Read( void * dst, size_t size ) {
	ng_assert( ModeCanRead() );
#if defined( SYS_WIN )
	ng_assert( handler != INVALID_HANDLER );
	DWORD bytesRead;
	BOOL  success = ReadFile( handler, dst, ( DWORD )size, &bytesRead, nullptr );
	ng_assert( success != 0 );
	return ( size_t )bytesRead;
#elif defined( SYS_UNIX )
	// For some reason, '.GetSize' seems to make the file descriptor invalid. Just reopen the file for now, YOLO
	close( fd );
 	fd = open( this->path.c_str(), this->open_mode );
	ng_assert( fd != INVALID_FD );
	size_t bytes_read = read( fd, dst, size );
	ng_assert( bytes_read != -1 );
	return bytes_read;
#else
	NG_UNSUPPORTED_PLATFORM
#endif
}

size_t File::Write( const void * src, size_t size ) {
	ng_assert( ModeCanWrite() );
#if defined( SYS_WIN )
	ng_assert( handler != INVALID_HANDLER );
	DWORD bytesRead;
	BOOL  success = WriteFile( handler, src, ( DWORD )size, &bytesRead, nullptr );
	ng_assert( success != 0 );
	return ( size_t )bytesRead;
#elif defined( SYS_UNIX )
	// For some reason, the file descriptor invalid. Just reopen the file for now, YOLO
	close( fd );
 	fd = open( this->path.c_str(), this->open_mode );
	ng_assert( fd != INVALID_FD );
	size_t bytesWritten = write( fd, src, size );
	ng_assert( bytesWritten != -1 );
	return bytesWritten;
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
#elif defined( SYS_UNIX )
	struct stat st;
	int success = stat(path.c_str(), &st);
	ng_assert( success != -1 );
	return st.st_size;
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
		return false;
	}

	dirent * dirFiles;
	while ( ( dirFiles = readdir( dir ) ) != nullptr ) {
		if ( strcmp( dirFiles->d_name, "." ) == 0 || strcmp( dirFiles->d_name, ".." ) == 0 )
			continue;
		if ( dirFiles->d_type == DT_DIR ) {
			int ori_path_len = strlen( path );
			char *full_dir_path = new char[ ori_path_len + strlen(dirFiles->d_name) + 1 ];
			strcpy( full_dir_path, path );
			strcpy( full_dir_path + sizeof(char) * ori_path_len, dirFiles->d_name );
			full_dir_path[ ori_path_len + strlen(dirFiles->d_name) ] = '\0';
			success &= ListFilesInDirectory( full_dir_path, results, mode );
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

}; // namespace ng
