#include "nglib.h"

void ng::Init() {
	InitSys();
}

void ng::Shutdown() {}

#ifdef TRACY_ENABLE
void * ng::profiledAlloc( size_t size ) {
	auto ptr = malloc( size );
	TracyAlloc( ptr, size );
	return ptr;
}

void ng::profiledFree( void * ptr ) {
	TracyFree( ptr );
	free( ptr );
}
#endif

void * operator new( std::size_t size ) {
	return ng_alloc( size );
}

void operator delete( void * ptr ) noexcept {
	ng_free( ptr );
}