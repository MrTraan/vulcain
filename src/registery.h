#pragma once
#include "entity.h"

#include "system.h"
#include <concurrentqueue.h>
#include <imgui/imgui.h>
#include <queue>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

struct ICpntRegistery {
	virtual ~ICpntRegistery() {}
	virtual bool RemoveComponent( SystemManager *, Entity e ) = 0;
	virtual void FlushCreationQueue( SystemManager * systemManager ) = 0;
	virtual u64  GetSize() const = 0;
#ifdef DEBUG
	virtual u64 ComputeMemoryUsage() const = 0;
#endif
};

template < class T > struct CpntRegistery : public ICpntRegistery {
	CpntRegistery( u32 maxNumberOfEntities ) {
		sizeOfArrays = maxNumberOfEntities;
		components = new T[ maxNumberOfEntities ];
		indexOfEntities = new u32[ maxNumberOfEntities ];
		entityOfComponent = new Entity[ maxNumberOfEntities ];
		for ( u32 i = 0; i < maxNumberOfEntities; i++ ) {
			indexOfEntities[ i ] = INVALID_ENTITY_INDEX;
			entityOfComponent[ i ] = INVALID_ENTITY;
		}
	}

	~CpntRegistery() {
		delete[] components;
		delete[] indexOfEntities;
		delete[] entityOfComponent;
	}

	// components, indexOfEntites and entityOfComponent all have the same size, sizeOfArrays
	T *      components = nullptr;
	u32 *    indexOfEntities = nullptr;
	Entity * entityOfComponent = nullptr;
	u32      sizeOfArrays = 0;
	u32      numComponents = 0;

	ng::LinkedList< ng::Tuple< Entity, T > > creationQueue;

	virtual void FlushCreationQueue( SystemManager * systemManager ) override {
		auto cursor = creationQueue.GetLastNode();
		while ( cursor != nullptr ) {
			ng::Tuple< Entity, T > & elem = cursor->data;
			Entity                   e = elem.First();
			T &                      cpntData = elem.Second();

			ng_assert( HasComponent( e ) == false );
			indexOfEntities[ e.id ] = numComponents++;
			T * cpnt = components + indexOfEntities[ e.id ];
			entityOfComponent[ indexOfEntities[ e.id ] ] = e;
			*cpnt = cpntData;
			systemManager->GetSystemForCpnt< T >().OnCpntAttached( e, *cpnt );
			auto next = cursor->previous;
			creationQueue.DeleteNode( cursor );
			cursor = next;
		}
	}

	template < class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		ng_assert( HasComponent( e ) == false );
		//if ( e.version == 1 ) {
		//	Entity foo = e;
		//	foo.version = 0;
		//	if ( HasComponent( foo ) ) {
		//		ng_assert_msg( false, "WHAT IN THE LORD'S NAME?" );
		//	}
		//}
		ng::Tuple< Entity, T > & tuple = creationQueue.Alloc();
		tuple.First() = e;
		T * memory = &( tuple.Second() );
		// Construct inplace
		memory = new ( memory ) T( std::forward< Args >( args )... );
		return tuple.Second();
	}

	virtual bool RemoveComponent( SystemManager * systemManager, Entity e ) override {
		if ( !HasComponent( e ) ) {
			return false;
		}
		systemManager->GetSystemForCpnt< T >().OnCpntRemoved( e, GetComponent( e ) );
		ng_assert( numComponents > 0 );
		u32 indexToDelete = indexOfEntities[ e.id ];
		u32 indexToSwap = numComponents - 1;
		if ( indexToDelete != indexToSwap ) {
			components[ indexToDelete ] = components[ indexToSwap ];
			indexOfEntities[ entityOfComponent[ indexToSwap ].id ] = indexToDelete;
			entityOfComponent[ indexToDelete ] = entityOfComponent[ indexToSwap ];
			entityOfComponent[ indexToSwap ] = INVALID_ENTITY;
		} else {
			entityOfComponent[ indexToDelete ] = INVALID_ENTITY;
		}
		indexOfEntities[ e.id ] = INVALID_ENTITY_INDEX;
		numComponents--;
		return true;
	}

	bool HasComponent( Entity e ) const {
		return indexOfEntities[ e.id ] != INVALID_ENTITY_INDEX && entityOfComponent[ indexOfEntities[ e.id ] ] == e;
	}

	const T & GetComponent( Entity e ) const {
		ng_assert( HasComponent( e ) );
		return components[ indexOfEntities[ e.id ] ];
	}

	T & GetComponent( Entity e ) {
		ng_assert( HasComponent( e ) );
		return components[ indexOfEntities[ e.id ] ];
	}

	const T * TryGetComponent( Entity e ) const {
		if ( HasComponent( e ) ) {
			return &components[ indexOfEntities[ e.id ] ];
		}
		return nullptr;
	}

	T * TryGetComponent( Entity e ) {
		if ( HasComponent( e ) ) {
			return &components[ indexOfEntities[ e.id ] ];
		}
		return nullptr;
	}

	virtual u64 GetSize() const override { return numComponents; }

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

#ifdef DEBUG
	virtual u64 ComputeMemoryUsage() const override {
		return ( sizeOfArrays * sizeof( components[ 0 ] ) ) + ( sizeOfArrays * sizeof( entityOfComponent[ 0 ] ) ) +
		       ( sizeOfArrays * sizeof( indexOfEntities[ 0 ] ) );
	}
#endif
};

constexpr u32 INITIAL_ENTITY_ALLOC = 4096u;

struct Registery {
	std::unordered_map< CpntTypeHash, ICpntRegistery * > cpntRegistriesMap;
#ifdef DEBUG
	std::unordered_map< CpntTypeHash, std::string > cpntTypesToName;
#endif

	char * isEntityAlive = nullptr;

	Registery( SystemManager * systemManager ) : systemManager( systemManager ) {
		// Enqueue ids from 0 to INITIAL_ENTITY_ALLOC
		Entity * intialEntitiesIds = new Entity[ INITIAL_ENTITY_ALLOC ];
		isEntityAlive = new char[ INITIAL_ENTITY_ALLOC ];
		for ( u32 i = 0; i < INITIAL_ENTITY_ALLOC; i++ ) {
			intialEntitiesIds[ i ].id = i;
			intialEntitiesIds[ i ].version = 0;
			isEntityAlive[ i ] = 0;
		}
		availableEntityIds.enqueue_bulk( intialEntitiesIds, INITIAL_ENTITY_ALLOC );
		delete[] intialEntitiesIds;
	}

	~Registery() {
		for ( auto [ type, registery ] : cpntRegistriesMap ) {
			delete registery;
		}
		delete isEntityAlive;
	}

	Entity CreateEntity() {
		Entity e = INVALID_ENTITY;
		bool   ok = availableEntityIds.try_dequeue( e );
		if ( !ok ) {
			ng_assert( false );
			// TODO: When we run out of entities, we should grow the pool size and in consequence grow every cpnt pool
			exit( 42 );
			return INVALID_ENTITY;
		}
		isEntityAlive[ e.id ] = 1;
		return e;
	}

	bool DestroyEntity( Entity e ) {
		// @TODO: We could clean the systems event queues if they listen to an entity that is now dead
		if ( isEntityAlive[ e.id ] ) {
			isEntityAlive[ e.id ] = 0;
			for ( auto [ type, registery ] : cpntRegistriesMap ) {
				registery->RemoveComponent( systemManager, e );
			}
			e.version++;
			bool ok = availableEntityIds.enqueue( e );
			ng_assert( ok );
		}
		return true;
	}

	void MarkForDelete( Entity e );

	template < class T, class... Args > T & AssignComponent( Entity e, Args &&... args ) {
		CpntRegistery< T > & registery = GetComponentRegistery< T >();
		T &                  res = registery.AssignComponent( e, std::forward< Args >( args )... );
		return res;
	}

	void FlushCreationQueues() {
		for ( auto [ type, registery ] : cpntRegistriesMap ) {
			registery->FlushCreationQueue( systemManager );
		}
	}

	template < class T > T * TryGetComponent( Entity e ) { return GetComponentRegistery< T >().TryGetComponent( e ); }
	template < class T > T & GetComponent( Entity e ) { return GetComponentRegistery< T >().GetComponent( e ); }
	template < class T > const T * TryGetComponent( Entity e ) const {
		return GetComponentRegistery< T >().TryGetComponent( e );
	}
	template < class T > const T & GetComponent( Entity e ) const {
		return GetComponentRegistery< T >().GetComponent( e );
	}
	template < class T > bool HasComponent( Entity e ) const { return GetComponentRegistery< T >().HasComponent( e ); }

	template < class T > CpntRegistery< T > &       IterateOver() { return GetComponentRegistery< T >(); }
	template < class T > const CpntRegistery< T > & IterateOver() const { return GetComponentRegistery< T >(); }

	template < class T > CpntRegistery< T > & GetComponentRegistery() {
		CpntTypeHash typeHash = HashComponent< T >();
		// TODO: This if must go away someday
		if ( !cpntRegistriesMap.contains( typeHash ) ) {
			cpntRegistriesMap[ typeHash ] = new CpntRegistery< T >( INITIAL_ENTITY_ALLOC );
#ifdef DEBUG
			cpntTypesToName[ typeHash ] = std::string( std::type_index( typeid( T ) ).name() );
#endif
		}
		CpntRegistery< T > * returnValue = ( CpntRegistery< T > * )cpntRegistriesMap.at( typeHash );
		return *returnValue;
	}
	template < class T > const CpntRegistery< T > & GetComponentRegistery() const {
		auto typeHash = HashComponent< T >();
		// TODO: This if must go away someday
		auto mutable_this = const_cast< Registery * >( this );
		if ( !mutable_this->cpntRegistriesMap.contains( typeHash ) ) {
			mutable_this->cpntRegistriesMap[ typeHash ] = new CpntRegistery< T >( INITIAL_ENTITY_ALLOC );
#ifdef DEBUG
			mutable_this->cpntTypesToName[ typeHash ] = std::string( std::type_index( typeid( T ) ).name() );
#endif
		}
		CpntRegistery< T > * returnValue = ( CpntRegistery< T > * )cpntRegistriesMap.at( typeHash );
		return *returnValue;
	}

	void DebugDraw() {
#ifdef DEBUG
		u64 totalMemUsage = 0;
		for ( auto [ type, registery ] : cpntRegistriesMap ) {
			totalMemUsage += registery->ComputeMemoryUsage();
			if ( ImGui::TreeNode( cpntTypesToName[ type ].c_str() ) ) {
				ImGui::Text( "Hash: %llu\n", type );
				ImGui::Text( "Num components: %d\n", registery->GetSize() );
				ImGui::Text( "Memory used: %llukb\n", registery->ComputeMemoryUsage() / 1024 );
				ImGui::TreePop();
			}
		}
		ImGui::Text( "Total memory usage: %llukb", totalMemUsage / 1024 );
#endif
	}

	moodycamel::ConcurrentQueue< Entity > availableEntityIds;
	moodycamel::ConcurrentQueue< Entity > markedForDeleteEntityIds;

	SystemManager * systemManager = nullptr;
};
