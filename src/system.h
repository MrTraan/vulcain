#pragma once

#include "game_time.h"
#include "message.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include <concurrentqueue.h>
#include <imgui/imgui.h>
#include <thread>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

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

template < class T > struct System : public ISystem {
	virtual ~System() {}
	virtual void OnCpntAttached( Entity e, T & t ) {}
	virtual void OnCpntRemoved( Entity e, T & t ) {}

	// This should get constexpr one day
	static CpntTypeHash GetHash() { return HashComponent< T >(); }
};

struct SystemManager {
	std::unordered_map< CpntTypeHash, ISystem * > systems;
#ifdef DEBUG
	std::unordered_map< CpntTypeHash, std::string > systemNames;
#endif

	~SystemManager();

	template < class T, class... Args > T & CreateSystem( Args &&... args ) {
		auto         system = new T( std::forward< Args >( args )... );
		CpntTypeHash typeIndex = T::GetHash();
		systems[ typeIndex ] = system;
#ifdef DEBUG
		systemNames[ typeIndex ] = std::string( std::type_index( typeid( T ) ).name() );
#endif
		return *system;
	}

	template < class T > T & GetSystem() {
		CpntTypeHash typeIndex = T::GetHash();
		return *( static_cast< T * >( systems.at( typeIndex ) ) );
	}

	template < class T > const T & GetSystem() const {
		CpntTypeHash typeIndex = T::GetHash();
		return *( static_cast< const T * >( systems.at( typeIndex ) ) );
	}

	template < class T > System< T > & GetSystemForCpnt() {
		CpntTypeHash typeHash = HashComponent< T >();
		return *( static_cast< System< T > * >( systems.at( typeHash ) ) );
	}

	template < class T > System< T > & GetSystemForCpnt() const {
		CpntTypeHash typeHash = HashComponent< T >();
		return *( static_cast< const System< T > * >( systems.at( typeHash ) ) );
	}

	ISystem * GetSystemForCpntHash( CpntTypeHash hash ) { return systems.at( hash ); }

	void Update( Registery & reg, Duration ticks );

	void StartJobs();

	ng::DynamicArray< std::thread * > jobs;

	void DebugDraw() {
#ifdef DEBUG
		for ( auto [ type, system ] : systems ) {
			if ( ImGui::TreeNode( systemNames[ type ].c_str() ) ) {
				ImGui::Text( "Hash: %llu\n", type );
				system->DebugDraw();
				ImGui::TreePop();
			}
		}
#endif
	}
};
