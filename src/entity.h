#pragma once
#include "ngLib/nglib.h"
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

typedef u32 Entity;

struct TransformCpnt {
	TransformCpnt() {}
	TransformCpnt( const glm::mat4 & matrixSrc ) : matrix( matrixSrc ) {}

	glm::mat4 matrix = glm::mat4( 1.0f );
};

struct ICpntRegistery {
	virtual ~ICpntRegistery() {}
};

template < class T > struct CpntRegistery : public ICpntRegistery {
	std::unordered_map< Entity, T > registery;

	template < class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		auto [ it, success ] = registery.emplace( e, T(std::forward< Args >( args )...) );
		return ( *it ).second;
	}

	bool HasComponent( Entity e ) const { return registery.contains( e ); }

	const T & GetComponent( Entity e ) const {
		ng_assert( HasComponent( e ) );
		return registery.at( e );
	}

	u64 GetSize() const { return registery.size(); }

	// iterators
	auto begin() { return registery.begin(); }
	auto begin() const { return registery.begin(); }
	auto end() { return registery.end(); }
	auto end() const { return registery.end(); }
};

struct Registery {
	std::unordered_map< std::type_index, ICpntRegistery * > data;

	~Registery() {
		for ( auto [ type, registery ] : data ) {
			delete registery;
		}
	}

	template < class T, class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		CpntRegistery< T > & registery = GetComponentRegistery< T >();
		return registery.AssignComponent( e, std::forward< Args >( args )... );
	}

	template < class T > const T & GetComponent( Entity e ) { return GetComponentRegistery< T >().GetComponent( e ); }

	template < class T > CpntRegistery< T > & IterateOver() { return GetComponentRegistery< T >(); }

	template < class T > CpntRegistery< T > & GetComponentRegistery() {
		auto typeIndex = std::type_index( typeid( T ) );
		// TODO: This if must go away someday
		if ( !data.contains( typeIndex ) ) {
			data[ typeIndex ] = new CpntRegistery< T >();
		}
		CpntRegistery< T > * returnValue = ( CpntRegistery< T > * )data[ typeIndex ];
		return *returnValue;
	}
};

struct ISystem {
	virtual ~ISystem() {}
	virtual void Update( Registery & reg, float dt ) = 0;
	virtual void DebugDraw() {}
};

struct SystemManager {
	std::unordered_map< std::type_index, ISystem * > systems;

	~SystemManager() {
		for ( auto [ type, system ] : systems ) {
			delete system;
		}
	}

	template < class T, class... Args > T & CreateSystem( Args &&... args ) {
		auto system = new T( std::forward< Args >( args )... );
		auto typeIndex = std::type_index( typeid( T ) );
		systems[ typeIndex ] = system;
		return *system;
	}

	void Update( Registery & reg, float dt ) {
		for ( auto [ type, system ] : systems ) {
			system->Update( reg, dt );
		}
	}
};
