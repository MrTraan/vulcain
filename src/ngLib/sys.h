#pragma once

#include "types.h"
#include <stdio.h>
#include <string>
#include <vector>

#define NG_UNSUPPORTED_PLATFORM static_assert( false, "Platform specific not handled here" );

#if defined( _WIN32 )
#define DEBUG_BREAK __debugbreak()
#define SYS_WIN
#include <windows.h>
#include <Fileapi.h>
#include <Handleapi.h>
#elif defined( __linux )
#define DEBUG_BREAK __asm__ __volatile__( "int $0x03" )
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#define SYS_LINUX
#define SYS_UNIX
#elif defined( __APPLE__ )
#include <signal.h>
#define DEBUG_BREAK raise( SIGTRAP )
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#define SYS_OSX
#define SYS_UNIX
#else
NG_UNSUPPORTED_PLATFORM
#endif

namespace ng {
void InitSys();

int64 SysGetTimeInMicro();
float SysGetTimeInMs();

#if defined( SYS_WIN )
using NativeFileHandler = HANDLE;
using FileOffset = int64;
#define INVALID_HANDLER INVALID_HANDLE_VALUE
#elif defined( SYS_UNIX )
using NativeFileHandler = int;
using FileOffset = off64_t;
constexpr NativeFileHandler INVALID_HANDLER = -1;
#endif

struct File {
	NativeFileHandler handler = INVALID_HANDLER;

	bool Open( const char * path, int mode );
	bool Close();

	size_t Read( void * dst, size_t size );
	size_t Write( const void * src, size_t size );
	void   Truncate();

	int64 GetSize() const;

	int         mode = 0;
	std::string path;

	static constexpr int MODE_READ = 1 << 1;
	static constexpr int MODE_WRITE = 1 << 2;
	static constexpr int MODE_RW = MODE_READ | MODE_WRITE;
	static constexpr int MODE_CREATE = 1 << 3;
	static constexpr int MODE_TRUNCATE = 1 << 4;

	inline bool ModeCanRead() const { return ( mode & MODE_READ ) != 0; }
	inline bool ModeCanWrite() const { return ( mode & MODE_WRITE ) != 0; }
	inline bool ModeCanCreate() const { return ( mode & MODE_CREATE ) != 0; }
	inline bool ModeCanTruncate() const { return ( mode & MODE_TRUNCATE ) != 0; }

	enum class SeekWhence {
		START,
		CUR,
		END,
	};

	FileOffset TellOffset() const;
	bool       SeekOffset( FileOffset offset, SeekWhence whence );
};

struct FileRead {};
struct FileWrite {};
struct FileReadWrite {};

enum class ListFileMode {
	NORMAL,
	RECURSIVE,
};

bool ListFilesInDirectory( const char *                 path,
                           std::vector< std::string > & results,
                           ListFileMode                 mode = ListFileMode::NORMAL );

bool FileExists( const char * path );
bool CreateDirectory( const char * path );
bool IsDirectory( const char * path );

}; // namespace ng
