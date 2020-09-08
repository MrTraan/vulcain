#include "navigation.h"
#include "ngLib/ngcontainers.h"
#include <benchmark/benchmark.h>
#include <list>

static void BM_NetworkFindPath( benchmark::State & state ) {
	Map           map;
	RoadNetwork & network = map.roadNetwork;
	map.AllocateGrid( 200, 200 );
	for ( u32 x = 30; x <= 190; x++ ) {
		for ( u32 z = 30; z <= 190; z++ ) {
			if ( x % 10 == 0 || z % 10 == 0 )
				map.SetTile( x, z, MapTile::ROAD );
		}
	}

	std::vector< Cell > out;
	for ( auto _ : state ) {
		map.FindPath( Cell( 34, 30 ), Cell( 164, 90 ), out );
	}
}

BENCHMARK( BM_NetworkFindPath );

static void BM_AStar( benchmark::State & state ) {
	Map           map;
	RoadNetwork & network = map.roadNetwork;
	map.AllocateGrid( 200, 200 );
	for ( u32 x = 30; x <= 190; x++ ) {
		for ( u32 z = 30; z <= 190; z++ ) {
			if ( x % 10 == 0 || z % 10 == 0 )
				map.SetTile( x, z, MapTile::ROAD );
		}
	}

	std::vector< Cell > out;
	for ( auto _ : state ) {
		AStar( Cell( 34, 30 ), Cell( 164, 90 ), ASTAR_FORBID_DIAGONALS, map, out );
	}
}

BENCHMARK( BM_AStar );

static void BM_ngBitfieldSet( benchmark::State & state ) {
	ng::Bitfield64 field;
	for ( auto _ : state ) {
		for ( int i = 0; i < 64; i++ ) {
			field.Set( i );
		}
	}
	benchmark::DoNotOptimize( field );
}

BENCHMARK( BM_ngBitfieldSet );

static void BM_stlBitfieldSet( benchmark::State & state ) {
	std::bitset< 64 > field;
	for ( auto _ : state ) {
		for ( int i = 0; i < 64; i++ ) {
			field.set( i );
		}
	}
	benchmark::DoNotOptimize( field );
}

BENCHMARK( BM_stlBitfieldSet );

static void BM_ngBitfieldTest( benchmark::State & state ) {
	ng::Bitfield64 field;
	for ( auto _ : state ) {
		for ( int i = 0; i < 64; i++ ) {
			bool ok = field.Test( i );
			benchmark::DoNotOptimize( ok );
		}
	}
	benchmark::DoNotOptimize( field );
}

BENCHMARK( BM_ngBitfieldTest );

static void BM_stlBitfieldTest( benchmark::State & state ) {
	std::bitset< 64 > field;
	for ( auto _ : state ) {
		for ( int i = 0; i < 64; i++ ) {
			bool ok = field.test( i );
			benchmark::DoNotOptimize( ok );
		}
	}
	benchmark::DoNotOptimize( field );
}

BENCHMARK( BM_stlBitfieldTest );

static void BM_objectPoolCreateOne( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::ObjectPool< int > pool;
		int * elem = pool.Pop();
		benchmark::DoNotOptimize( elem );
	}
}

BENCHMARK( BM_objectPoolCreateOne );

static void BM_objectPoolCreate640( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::ObjectPool< int > pool;
		for ( int i = 0; i < 640; i++ ) {
			int * elem = pool.Pop();
			benchmark::DoNotOptimize( elem );
		}
	}
}

BENCHMARK( BM_objectPoolCreate640 );

static void BM_objectPoolCreation( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::ObjectPool< int > pool;
		for ( int i = 0; i < 64 * 20; i++ ) {
			int * elem = pool.Pop();
			benchmark::DoNotOptimize( elem );
		}
	}
}

BENCHMARK( BM_objectPoolCreation );

static void BM_stlLinkedListInsertion( benchmark::State & state ) {
	for ( auto _ : state ) {
		std::list< int > list;
		for ( int i = 0; i < 64 * 20; i++ ) {
			list.push_front( i );
		}
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_stlLinkedListInsertion );


static void BM_ngLinkedListInsertion( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::LinkedList< int > list;
		for ( int i = 0; i < 64 * 20; i++ ) {
			list.PushFront( i );
		}
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_ngLinkedListInsertion );

static void BM_stlLinkedListInsertTwo( benchmark::State & state ) {
	for ( auto _ : state ) {
		std::list< int > list;
		list.push_front( 1 );
		list.push_front( 1 );
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_stlLinkedListInsertTwo );

static void BM_ngLinkedListInsertTwo( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::LinkedList< int > list;
		list.PushFront( 1 );
		list.PushFront( 1 );
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_ngLinkedListInsertTwo );

static void BM_stlLinkedListInsertOne( benchmark::State & state ) {
	for ( auto _ : state ) {
		std::list< int > list;
		list.push_front( 1 );
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_stlLinkedListInsertOne );

static void BM_ngLinkedListInsertOne( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::LinkedList< int > list;
		list.PushFront( 1 );
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_ngLinkedListInsertOne );

static void BM_stlVectorInsertion( benchmark::State & state ) {
	for ( auto _ : state ) {
		std::vector< int > list;
		for ( int i = 0; i < 64 * 20; i++ ) {
			list.push_back( i );
		}
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_stlVectorInsertion );

static void BM_ngVectorInsertion( benchmark::State & state ) {
	for ( auto _ : state ) {
		ng::DynamicArray< int > list;
		for ( int i = 0; i < 64 * 20; i++ ) {
			list.PushBack( i );
		}
		benchmark::DoNotOptimize( list );
	}
}

BENCHMARK( BM_ngVectorInsertion );

int RunBenchmarks( int argc, char ** argv ) {
	::benchmark::Initialize( &argc, argv );
	if ( ::benchmark::ReportUnrecognizedArguments( argc, argv ) ) {
		return 1;
	}
	::benchmark::RunSpecifiedBenchmarks();
	return 0;
}
