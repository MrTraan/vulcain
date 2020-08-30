#include <benchmark/benchmark.h>
#include "navigation.h"

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
		AStar( Cell( 33, 30 ), Cell( 164, 90 ), ASTAR_FORBID_DIAGONALS, map, out );
	}
}

BENCHMARK( BM_AStar );

int RunBenchmarks( int argc, char ** argv ) {
	::benchmark::Initialize( &argc, argv );
	if ( ::benchmark::ReportUnrecognizedArguments( argc, argv ) ) {
		return 1;
	}
	::benchmark::RunSpecifiedBenchmarks();
	return 0;
}
