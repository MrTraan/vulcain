#pragma once

#include "game_time.h"
#include "message.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include <concurrentqueue.h>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <thread>

struct Registery;

constexpr u64 FNV_HASH_BASIS = 0xcbf29ce484222325;
constexpr u64 FNV_PRIME = 0x100000001b3;

inline constexpr u64 FnvHash( const u8 * data, u64 size ) {
	u64 hash = FNV_HASH_BASIS;
	for ( u64 i = 0; i < size; i++ ) {
		hash = hash ^ data[ i ];
		hash = hash * FNV_PRIME;
	}
	return hash;
}

struct ISystem {
	virtual ~ISystem() {}
	virtual void Update( Registery & reg, Duration ticks ) {}
	virtual void ParallelJob() {}
	virtual void HandleMessage( Registery & reg, const Message & msg ) {}
	virtual void DebugDraw() {}

	moodycamel::ConcurrentQueue< Message > messageQueue;
	// Do we listen to a specific event on a specific entity?
	ng::DynamicArray< ng::Tuple< Entity, ng::Bitfield64 > > eventListenerMask;
	ng::Bitfield64                                          globalListenerMask;

	void ListenTo( MessageType type, Entity recipient ) {
		for ( auto & tuple : eventListenerMask ) {
			if ( tuple.First() == recipient ) {
				tuple.Second().Set( ( u32 )type );
				return;
			}
		}
		ng::Bitfield64 mask;
		mask.Set( ( u32 )type );
		eventListenerMask.PushBack( { recipient, mask } );
	}

	void ListenToGlobal( MessageType type ) { globalListenerMask.Set( ( u32 )type ); }
};

struct SystemManager {
	std::unordered_map< u64, ISystem * > systems;

	~SystemManager();

	template < class T, class... Args > T & CreateSystem( Args &&... args ) {
		auto system = new T( std::forward< Args >( args )... );
		u64  typeIndex = std::type_index( typeid( T ) ).hash_code();
		systems[ typeIndex ] = system;
		return *system;
	}

	template < class T > T & GetSystem() {
		u64 typeIndex = std::type_index( typeid( T ) ).hash_code();
		return *( static_cast< T * >( systems.at( typeIndex ) ) );
	}

	template < class T > const T & GetSystem() const {
		u64 typeIndex = std::type_index( typeid( T ) ).hash_code();
		return *( static_cast< const T * >( systems.at( typeIndex ) ) );
	}

	void Update( Registery & reg, Duration ticks );

	void StartJobs();

	ng::DynamicArray<std::thread * > jobs;
};
