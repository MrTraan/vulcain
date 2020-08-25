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
		REQUIRE( network.nodes[0].NumSetConnections() == 0 );
		REQUIRE( network.nodes[1].NumSetConnections() == 0 );
		map.SetTile( Cell( 11, 10 ), MapTile::ROAD );
		REQUIRE( network.nodes.size() == 2 );
		REQUIRE( network.nodes[0].NumSetConnections() == 1 );
		REQUIRE( network.nodes[1].NumSetConnections() == 1 );
		REQUIRE( network.nodes[0].GetSetConnectionWithOffset(0)->connectedTo == Cell(12, 10) );
		REQUIRE( network.nodes[0].GetSetConnectionWithOffset(0)->distance == 2 );
		REQUIRE( network.nodes[1].GetSetConnectionWithOffset(0)->connectedTo == Cell(10, 10) );
		REQUIRE( network.nodes[1].GetSetConnectionWithOffset(0)->distance == 2 );
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
		REQUIRE( network.nodes[0].NumSetConnections() == 1 );
		REQUIRE( network.nodes[0].GetSetConnectionWithOffset(0)->connectedTo == Cell(16, 10) );
		REQUIRE( network.nodes[0].GetSetConnectionWithOffset(0)->distance == 6 );
		REQUIRE( network.nodes[1].NumSetConnections() == 1 );
		REQUIRE( network.nodes[1].GetSetConnectionWithOffset(0)->connectedTo == Cell(10, 10) );
		REQUIRE( network.nodes[1].GetSetConnectionWithOffset(0)->distance == 6 );
	}
}

TEST_CASE( "Road network lookup", "[road network]" ) {
	Map           map;
	RoadNetwork & network = map.roadNetwork;
	map.AllocateGrid( 100, 100 );
	for ( u32 x = 0; x <= 10; x++ ) {
		map.SetTile( x, 0, MapTile::ROAD );
	}

	SECTION( "network can find nearest node" ) {
		auto res = network.FindNearestRoadNode( Cell( 3, 0 ), map );
		REQUIRE( res.found == true );
		REQUIRE( res.node != nullptr );
		REQUIRE( res.node->position == Cell( 0, 0 ) );
		REQUIRE( res.distance == 3 );
		REQUIRE( res.directionFromStart == SOUTH );
		res = network.FindNearestRoadNode( Cell( 6, 0 ), map );
		REQUIRE( res.found == true );
		REQUIRE( res.node != nullptr );
		REQUIRE( res.node->position == Cell( 10, 0 ) );
		REQUIRE( res.distance == 4 );
		REQUIRE( res.directionFromStart == NORTH );
	}

	SECTION( "network can find the two nearest nodes" ) {
		RoadNetwork::Node *           nodeA = nullptr;
		RoadNetwork::Node *           nodeB = nullptr;
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
