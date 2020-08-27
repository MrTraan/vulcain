#include "navigation.h"
#include <catch.hpp>

TEST_CASE( "Cardinal direction", "[cardinal direction]" ) {
	REQUIRE( GetDirectionFromCellTo( Cell( 10, 10 ), Cell( 11, 10 ) ) == NORTH );
}

TEST_CASE( "Road network creation", "[road network]" ) {
	SECTION( "can create lonely road" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );

		Cell cell( 10, 10 );
		map.SetTile( cell, MapTile::ROAD );
		REQUIRE( network.nodes.size() == 1 );
		RoadNetwork::Node * node = network.FindNodeWithPosition( cell );
		REQUIRE( node != nullptr );
		REQUIRE( node->NumSetConnections() == 0 );

		cell = Cell( 20, 20 );
		map.SetTile( cell, MapTile::ROAD );
		REQUIRE( network.nodes.size() == 2 );
		node = network.FindNodeWithPosition( cell );
		REQUIRE( node != nullptr );
		REQUIRE( node->NumSetConnections() == 0 );
	}

	SECTION( "can create lonely road and grow it" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );

		Cell cellA( 10, 10 );
		map.SetTile( cellA, MapTile::ROAD );
		Cell cellB( 11, 10 );
		map.SetTile( cellB, MapTile::ROAD );
		RoadNetwork::Node * nodeA = network.FindNodeWithPosition( cellA );
		RoadNetwork::Node * nodeB = network.FindNodeWithPosition( cellB );
		REQUIRE( nodeA != nullptr );
		REQUIRE( nodeB != nullptr );
		REQUIRE( nodeA->NumSetConnections() == 1 );
		REQUIRE( nodeB->NumSetConnections() == 1 );
		REQUIRE( nodeA->connections[ NORTH ].connectedTo == cellB );
		REQUIRE( nodeB->connections[ SOUTH ].connectedTo == cellA );
	}

	SECTION( "can expand a road on several tiles" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 10; x <= 20; x++ ) {
			Cell cell( x, 10 );
			map.SetTile( cell, MapTile::ROAD );
		}
		for ( u32 z = 11; z <= 20; z++ ) {
			Cell cell( 20, z );
			map.SetTile( cell, MapTile::ROAD );
		}
		REQUIRE( network.nodes.size() == 2 );
		RoadNetwork::Node * nodeA = network.FindNodeWithPosition( Cell( 10, 10 ) );
		RoadNetwork::Node * nodeB = network.FindNodeWithPosition( Cell( 20, 20 ) );
		REQUIRE( nodeA != nullptr );
		REQUIRE( nodeB != nullptr );
		REQUIRE( nodeA->NumSetConnections() == 1 );
		REQUIRE( nodeB->NumSetConnections() == 1 );
		REQUIRE( nodeA->connections[ NORTH ].connectedTo == Cell( 20, 20 ) );
		REQUIRE( nodeA->connections[ NORTH ].distance == 20 );
		REQUIRE( nodeB->connections[ EAST ].connectedTo == Cell( 10, 10 ) );
		REQUIRE( nodeB->connections[ EAST ].distance == 20 );
	}

	SECTION( "can split a road" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 10; x <= 20; x++ ) {
			Cell cell( x, 10 );
			map.SetTile( cell, MapTile::ROAD );
		}
		REQUIRE( network.nodes.size() == 2 );
		map.SetTile( Cell( 15, 11 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 4 );
	}

	SECTION( "can connect two lonely roads" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile( Cell( 10, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 12, 10 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 2 );
		REQUIRE( network.nodes[ 0 ].NumSetConnections() == 0 );
		REQUIRE( network.nodes[ 1 ].NumSetConnections() == 0 );
		map.SetTile( Cell( 11, 10 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 2 );
		REQUIRE( network.nodes[ 0 ].NumSetConnections() == 1 );
		REQUIRE( network.nodes[ 1 ].NumSetConnections() == 1 );
		REQUIRE( network.nodes[ 0 ].GetValidConnectionWithOffset( 0 )->connectedTo == Cell( 12, 10 ) );
		REQUIRE( network.nodes[ 0 ].GetValidConnectionWithOffset( 0 )->distance == 2 );
		REQUIRE( network.nodes[ 1 ].GetValidConnectionWithOffset( 0 )->connectedTo == Cell( 10, 10 ) );
		REQUIRE( network.nodes[ 1 ].GetValidConnectionWithOffset( 0 )->distance == 2 );
	}

	SECTION( "can merge two roads" ) {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile( Cell( 10, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 11, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 12, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 14, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 15, 10 ), MapTile::ROAD );
		map.SetTile( Cell( 16, 10 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 4 );
		map.SetTile( Cell( 13, 10 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 2 );
		REQUIRE( network.nodes[ 0 ].NumSetConnections() == 1 );
		REQUIRE( network.nodes[ 0 ].GetValidConnectionWithOffset( 0 )->connectedTo == Cell( 16, 10 ) );
		REQUIRE( network.nodes[ 0 ].GetValidConnectionWithOffset( 0 )->distance == 6 );
		REQUIRE( network.nodes[ 1 ].NumSetConnections() == 1 );
		REQUIRE( network.nodes[ 1 ].GetValidConnectionWithOffset( 0 )->connectedTo == Cell( 10, 10 ) );
		REQUIRE( network.nodes[ 1 ].GetValidConnectionWithOffset( 0 )->distance == 6 );
	}
	
	SECTION( "can create a smal 2x2 circle section" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 10, MapTile::ROAD );
		}
		map.SetTile(11, 10, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		map.SetTile(11, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		map.SetTile(10, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 2 );
	}
	
	SECTION( "can create a large circle section" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile(10, 10, MapTile::ROAD);
		map.SetTile(11, 10, MapTile::ROAD);
		map.SetTile(12, 10, MapTile::ROAD);
		map.SetTile(13, 10, MapTile::ROAD);
		map.SetTile(13, 11, MapTile::ROAD);
		map.SetTile(13, 12, MapTile::ROAD);
		map.SetTile(13, 13, MapTile::ROAD);
		map.SetTile(12, 13, MapTile::ROAD);
		map.SetTile(11, 13, MapTile::ROAD);
		map.SetTile(10, 13, MapTile::ROAD);
		map.SetTile(10, 12, MapTile::ROAD);
		map.SetTile(10, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
	}
	
	SECTION( "can create a large circle section and connect a road to it" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile(10, 10, MapTile::ROAD);
		map.SetTile(11, 10, MapTile::ROAD);
		map.SetTile(12, 10, MapTile::ROAD);
		map.SetTile(13, 10, MapTile::ROAD);
		map.SetTile(13, 11, MapTile::ROAD);
		map.SetTile(13, 12, MapTile::ROAD);
		map.SetTile(13, 13, MapTile::ROAD);
		map.SetTile(12, 13, MapTile::ROAD);
		map.SetTile(11, 13, MapTile::ROAD);
		map.SetTile(10, 13, MapTile::ROAD);
		map.SetTile(10, 12, MapTile::ROAD);
		map.SetTile(10, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
		map.SetTile(16, 12, MapTile::ROAD);
		map.SetTile(15, 12, MapTile::ROAD);
		map.SetTile(14, 12, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 3 );
	}
	
	SECTION( "can create two circle at once" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile( 11, 10, MapTile::ROAD );
		map.SetTile( 12, 10, MapTile::ROAD );
		map.SetTile( 13, 10, MapTile::ROAD );
		map.SetTile( 13, 9, MapTile::ROAD );
		map.SetTile( 13, 8, MapTile::ROAD );
		map.SetTile( 13, 7, MapTile::ROAD );
		map.SetTile( 12, 7, MapTile::ROAD );
		map.SetTile( 11, 7, MapTile::ROAD );
		map.SetTile( 10, 7, MapTile::ROAD );
		map.SetTile( 10, 8, MapTile::ROAD );
		map.SetTile( 10, 9, MapTile::ROAD );
		
		map.SetTile( 9, 10, MapTile::ROAD );
		map.SetTile( 8, 10, MapTile::ROAD );
		map.SetTile( 7, 10, MapTile::ROAD );
		map.SetTile( 7, 11, MapTile::ROAD );
		map.SetTile( 7, 12, MapTile::ROAD );
		map.SetTile( 7, 13, MapTile::ROAD );
		map.SetTile( 8, 13, MapTile::ROAD );
		map.SetTile( 9, 13, MapTile::ROAD );
		map.SetTile( 10, 13, MapTile::ROAD );
		map.SetTile( 10, 12, MapTile::ROAD );
		map.SetTile( 10, 11, MapTile::ROAD );

		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 4 );

		map.SetTile( 10, 10, MapTile::ROAD );
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
	}
}

TEST_CASE( "Road network destruction", "[road network remove]" ) {
	SECTION( "can split a small 2x2 circle section" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 10, MapTile::ROAD );
		}
		map.SetTile(11, 10, MapTile::ROAD);
		map.SetTile(11, 11, MapTile::ROAD);
		map.SetTile(10, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 2 );
		map.SetTile(11, 11, MapTile::EMPTY);
		REQUIRE( network.nodes.size() == 4 );
		REQUIRE( network.CheckNetworkIntegrity() == true );
	}
	
	SECTION( "can delete a small section connected to a circle and then recreate it" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile(10, 10, MapTile::ROAD);
		map.SetTile(11, 10, MapTile::ROAD);
		map.SetTile(12, 10, MapTile::ROAD);
		map.SetTile(13, 10, MapTile::ROAD);
		map.SetTile(13, 11, MapTile::ROAD);
		map.SetTile(13, 12, MapTile::ROAD);
		map.SetTile(13, 13, MapTile::ROAD);
		map.SetTile(12, 13, MapTile::ROAD);
		map.SetTile(11, 13, MapTile::ROAD);
		map.SetTile(10, 13, MapTile::ROAD);
		map.SetTile(10, 12, MapTile::ROAD);
		map.SetTile(10, 11, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
		map.SetTile(16, 12, MapTile::ROAD);
		map.SetTile(15, 12, MapTile::ROAD);
		map.SetTile(14, 12, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 3 );
		map.SetTile(14, 12, MapTile::EMPTY);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 3 );
		map.SetTile(14, 12, MapTile::ROAD);
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 3 );
	}
	
	SECTION( "can delete the center of a double circle and then recreate it" ) {
		Map map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		map.SetTile( 11, 10, MapTile::ROAD );
		map.SetTile( 12, 10, MapTile::ROAD );
		map.SetTile( 13, 10, MapTile::ROAD );
		map.SetTile( 13, 9, MapTile::ROAD );
		map.SetTile( 13, 8, MapTile::ROAD );
		map.SetTile( 13, 7, MapTile::ROAD );
		map.SetTile( 12, 7, MapTile::ROAD );
		map.SetTile( 11, 7, MapTile::ROAD );
		map.SetTile( 10, 7, MapTile::ROAD );
		map.SetTile( 10, 8, MapTile::ROAD );
		map.SetTile( 10, 9, MapTile::ROAD );
		
		map.SetTile( 9, 10, MapTile::ROAD );
		map.SetTile( 8, 10, MapTile::ROAD );
		map.SetTile( 7, 10, MapTile::ROAD );
		map.SetTile( 7, 11, MapTile::ROAD );
		map.SetTile( 7, 12, MapTile::ROAD );
		map.SetTile( 7, 13, MapTile::ROAD );
		map.SetTile( 8, 13, MapTile::ROAD );
		map.SetTile( 9, 13, MapTile::ROAD );
		map.SetTile( 10, 13, MapTile::ROAD );
		map.SetTile( 10, 12, MapTile::ROAD );
		map.SetTile( 10, 11, MapTile::ROAD );

		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 4 );

		map.SetTile( 10, 10, MapTile::ROAD );
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
		
		map.SetTile( 10, 10, MapTile::EMPTY );
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 4 );
		
		map.SetTile( 10, 10, MapTile::ROAD );
		REQUIRE( network.CheckNetworkIntegrity() == true );
		REQUIRE( network.nodes.size() == 1 );
	}
}

TEST_CASE( "Road network lookup", "[road network]" ) {
	Map           map;
	RoadNetwork & network = map.roadNetwork;
	map.AllocateGrid( 100, 100 );
	for ( u32 x = 0; x <= 10; x++ ) {
		map.SetTile( x, 0, MapTile::ROAD );
	}

	SECTION( "network can find the two nearest nodes" ) {
		RoadNetwork::NodeSearchResult searchA{};
		RoadNetwork::NodeSearchResult searchB{};
		network.FindNearestRoadNodes( Cell( 3, 0 ), map, searchA, searchB );
		REQUIRE( searchA.found == true );
		REQUIRE( searchB.found == true );
		REQUIRE( searchA.node != nullptr );
		REQUIRE( searchB.node != nullptr );
		REQUIRE( searchA.distance == 3 );
		REQUIRE( searchB.distance == 7 );
		REQUIRE( searchA.node->position == Cell( 0, 0 ) );
		REQUIRE( searchB.node->position == Cell( 10, 0 ) );
	}
}

TEST_CASE("Road network find path", "[FindPath]") {
	SECTION("can find the path between two cells on the same road") {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 0, MapTile::ROAD );
		}
		std::vector<Cell> path;
		bool ok = network.FindPath(Cell(3, 0), Cell(6, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 2 );
		REQUIRE(path[0] == Cell(6, 0));
		REQUIRE(path[1] == Cell(3, 0));
	}
	
	SECTION("can find the path to a node on the same road") {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 0, MapTile::ROAD );
		}
		std::vector<Cell> path;
		bool ok = network.FindPath(Cell(3, 0), Cell(10, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 2 );
		REQUIRE(path[0] == Cell(10, 0));
		REQUIRE(path[1] == Cell(3, 0));
		
		ok = network.FindPath(Cell(3, 0), Cell(0, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 2 );
		REQUIRE(path[0] == Cell(0, 0));
		REQUIRE(path[1] == Cell(3, 0));
	}
	
	SECTION("can find the path from a node to a cell on the same road") {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 0, MapTile::ROAD );
		}
		std::vector<Cell> path;
		bool ok = network.FindPath(Cell(10, 0), Cell(3, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 2 );
		REQUIRE(path[0] == Cell(3, 0));
		REQUIRE(path[1] == Cell(10, 0));
		
		ok = network.FindPath(Cell(0, 0), Cell(3, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 2 );
		REQUIRE(path[0] == Cell(3, 0));
		REQUIRE(path[1] == Cell(0, 0));
			
		map.SetTile( 10, 1, MapTile::ROAD );
		map.SetTile( 11, 1, MapTile::ROAD );
		map.SetTile( 11, 2, MapTile::ROAD );
		map.SetTile( 12, 2, MapTile::ROAD );
		
		ok = network.FindPath(Cell(0, 0), Cell(11, 2), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 5 );
		REQUIRE(path[0] == Cell(11, 2));
		REQUIRE(path[1] == Cell(11, 1));
		REQUIRE(path[2] == Cell(10, 1));
		REQUIRE(path[3] == Cell(10, 0));
		REQUIRE(path[4] == Cell(0, 0));

		ok = network.FindPath(Cell(12, 2), Cell(10, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 5 );
		REQUIRE(path[0] == Cell(10, 0));
		REQUIRE(path[1] == Cell(10, 1));
		REQUIRE(path[2] == Cell(11, 1));
		REQUIRE(path[3] == Cell(11, 2));
		REQUIRE(path[4] == Cell(12, 2));
	}
	
	SECTION("can find the path between two cells on the same non linear road") {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 0, MapTile::ROAD );
		}
	
		map.SetTile( 10, 1, MapTile::ROAD );
		map.SetTile( 11, 1, MapTile::ROAD );
		map.SetTile( 11, 2, MapTile::ROAD );
		map.SetTile( 12, 2, MapTile::ROAD );

		std::vector<Cell> path;
		bool ok = network.FindPath(Cell(11, 2), Cell(10, 0), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 4 );
		REQUIRE(path[0] == Cell(10, 0));
		REQUIRE(path[1] == Cell(10, 1));
		REQUIRE(path[2] == Cell(11, 1));
		REQUIRE(path[3] == Cell(11, 2));
		
		ok = network.FindPath(Cell(10, 0), Cell(12, 2), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 5 );
		REQUIRE(path[0] == Cell(12, 2));
		REQUIRE(path[1] == Cell(11, 2));
		REQUIRE(path[2] == Cell(11, 1));
		REQUIRE(path[3] == Cell(10, 1));
		REQUIRE(path[4] == Cell(10, 0));
	}
	
	SECTION("can find the path between two cells on two different roads") {
		Map           map;
		RoadNetwork & network = map.roadNetwork;
		map.AllocateGrid( 100, 100 );
		for ( u32 x = 0; x <= 10; x++ ) {
			map.SetTile( x, 0, MapTile::ROAD );
		}
		for ( u32 z = 1; z <= 10; z++ ) {
			map.SetTile( 5, z, MapTile::ROAD );
		}
		std::vector<Cell> path;
		bool ok = network.FindPath(Cell(3, 0), Cell(5, 6), map, path);
		REQUIRE(ok == true);
		REQUIRE(path.size() == 3 );
		REQUIRE(path[0] == Cell(5, 6));
		REQUIRE(path[1] == Cell(5, 0));
		REQUIRE(path[2] == Cell(3, 0));
	}
}
