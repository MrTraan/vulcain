#pragma once
#include "ngLib/nglib.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <queue>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

typedef u32 Entity;

struct CpntTransform {
	CpntTransform() = default;
	CpntTransform( const CpntTransform & ) = default;

  public:
	const glm::mat4 & GetMatrix() const { return matrix; }

	void Translate( const glm::vec3 & v ) {
		translation += v;
		ComputeMatrix();
	}

	void SetTranslation( const glm::vec3 & v ) {
		translation = v;
		ComputeMatrix();
	}

	glm::vec3 GetTranslation() const { return translation; }

	void ComputeMatrix() {
		matrix = glm::translate( glm::mat4( 1.0f ), translation );
		matrix = glm::scale( matrix, scale );
	}

  private:
	glm::mat4 matrix = glm::mat4( 1.0f );
	glm::vec3 translation;
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
};

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

struct ICpntRegistery {
	virtual ~ICpntRegistery() {}
	virtual void RemoveComponent( Entity e ) = 0;
};

constexpr u32 INVALID_ENTITY_INDEX = ( u32 )-1;

template < class T > struct CpntRegistery : public ICpntRegistery {

	CpntRegistery( u32 maxNumberOfEntities ) {
		components = new T[ maxNumberOfEntities ];
		indexOfEntities = new u32[ maxNumberOfEntities ];
		entityOfComponent = new Entity[ maxNumberOfEntities ];
		for ( u32 i = 0; i < maxNumberOfEntities; i++ ) {
			indexOfEntities[ i ] = INVALID_ENTITY_INDEX;
			entityOfComponent[ i ] = INVALID_ENTITY_ID;
		}
	}

	~CpntRegistery() {
		delete[] components;
		delete[] indexOfEntities;
		delete[] entityOfComponent;
	}

	T *      components = nullptr;
	u32 *    indexOfEntities = nullptr;
	Entity * entityOfComponent = nullptr;
	u32      numComponents = 0;

	template < class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		ng_assert( HasComponent( e ) == false );
		indexOfEntities[ e ] = numComponents++;
		T * cpnt = components + indexOfEntities[ e ];
		entityOfComponent[ indexOfEntities[ e ] ] = e;
		cpnt = new ( cpnt ) T( std::forward< Args >( args )... );
		return *cpnt;
	}

	virtual void RemoveComponent( Entity e ) override {
		if ( !HasComponent( e ) ) {
			return;
		}
		ng_assert( numComponents > 0 );
		u32 indexToDelete = indexOfEntities[ e ];
		u32 indexToSwap = numComponents - 1;
		if ( indexToDelete != indexToSwap ) {
			components[ indexToDelete ] = components[ indexToSwap ];
			indexOfEntities[ entityOfComponent[ indexToSwap ] ] = indexToDelete;
			entityOfComponent[ indexToDelete ] = entityOfComponent[ indexToSwap ];
			entityOfComponent[ indexToSwap ] = INVALID_ENTITY_ID;
		}
		indexOfEntities[ e ] = INVALID_ENTITY_INDEX;
		numComponents--;
	}

	bool HasComponent( Entity e ) const { return indexOfEntities[ e ] != INVALID_ENTITY_INDEX; }

	const T & GetComponent( Entity e ) const {
		ng_assert( HasComponent( e ) );
		return components[ indexOfEntities[ e ] ];
	}

	T & GetComponent( Entity e ) {
		ng_assert( HasComponent( e ) );
		return components[ indexOfEntities[ e ] ];
	}

	u64 GetSize() const { return numComponents; }

	struct Iterator {
		Iterator( Entity * e, T * cpnt ) : e( e ), cpnt( cpnt ) {}
		Iterator operator++() {
			e++;
			cpnt++;
			return *this;
		}

		bool                     operator!=( const Iterator & other ) const { return cpnt != other.cpnt; }
		std::pair< Entity, T & > operator*() { return std::pair< Entity, T & >( *e, *cpnt ); }

		Entity * e;
		T *      cpnt;
	};

	// iterators
	auto begin() { return Iterator( entityOfComponent, components ); }
	auto begin() const { return Iterator( entityOfComponent, components ); }
	auto end() { return Iterator( entityOfComponent + numComponents, components + numComponents ); }
	auto end() const { return Iterator( entityOfComponent + numComponents, components + numComponents ); }
};

constexpr u32    INITIAL_ENTITY_ALLOC = 4096u;
constexpr Entity INVALID_ENTITY_ID = ( Entity )-1;

struct Registery {
	std::unordered_map< u64, ICpntRegistery * > cpntRegistriesMap;

	Registery() {
		for ( u32 i = 0; i < INITIAL_ENTITY_ALLOC; i++ ) {
			availableEntityIds.push( i );
		}
	}

	~Registery() {
		for ( auto [ type, registery ] : cpntRegistriesMap ) {
			delete registery;
		}
	}

	Entity CreateEntity() {
		if ( availableEntityIds.size() == 0 ) {
			ng_assert( false );
			// TODO: When we run out of entities, we should grow the pool size and in consequence grow every cpnt pool
			return INVALID_ENTITY_ID;
		}
		Entity id = availableEntityIds.front();
		availableEntityIds.pop();
		return id;
	}

	void DestroyEntity( Entity e ) {
		availableEntityIds.push( e );
		for ( auto [ type, registery ] : cpntRegistriesMap ) {
			registery->RemoveComponent( e );
		}
	}

	void MarkForDelete( Entity e ) { markedForDeleteEntityIds.push( e ); }

	void FlushDeleteQueue() {
		while ( markedForDeleteEntityIds.empty() == false ) {
			DestroyEntity( markedForDeleteEntityIds.front() );
			markedForDeleteEntityIds.pop();
		}
	}

	template < class T, class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		CpntRegistery< T > & registery = GetComponentRegistery< T >();
		return registery.AssignComponent( e, std::forward< Args >( args )... );
	}

	template < class T > T &  GetComponent( Entity e ) { return GetComponentRegistery< T >().GetComponent( e ); }
	template < class T > bool HasComponent( Entity e ) const { return GetComponentRegistery< T >().HasComponent( e ); }

	template < class T > CpntRegistery< T > & IterateOver() { return GetComponentRegistery< T >(); }

	template < class T > CpntRegistery< T > & GetComponentRegistery() {
		auto typeHash = std::type_index( typeid( T ) ).hash_code();
		// TODO: This if must go away someday
		if ( !cpntRegistriesMap.contains( typeHash ) ) {
			cpntRegistriesMap[ typeHash ] = new CpntRegistery< T >( INITIAL_ENTITY_ALLOC );
		}
		CpntRegistery< T > * returnValue = ( CpntRegistery< T > * )cpntRegistriesMap.at( typeHash );
		return *returnValue;
	}
	template < class T > const CpntRegistery< T > & GetComponentRegistery() const {
		auto typeHash = std::type_index( typeid( T ) ).hash_code();
		// TODO: This if must go away someday
		ng_assert( cpntRegistriesMap.contains( typeHash ) );
		CpntRegistery< T > * returnValue = ( CpntRegistery< T > * )cpntRegistriesMap.at( typeHash );
		return *returnValue;
	}

	std::queue< Entity > availableEntityIds;
	std::queue< Entity > markedForDeleteEntityIds;
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
		reg.FlushDeleteQueue();
		for ( auto [ type, system ] : systems ) {
			system->Update( reg, dt );
		}
		reg.FlushDeleteQueue();
	}
};
