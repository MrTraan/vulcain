#include "navigation.h"
#include "message.h"
#include "ngLib/logs.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "ngLib/types.h"
#include "registery.h"
#include <vector>

struct AStarNode {
	Cell        coord;
	AStarNode * parent = nullptr;
	int         f, g, h;

	bool operator<( const AStarNode & rhs ) const { return f < rhs.f; }
};

static AStarNode * FindNodeInSet( std::vector< AStarNode * > & set, Cell coords ) {
	for ( auto node : set ) {
		if ( node->coord == coords ) {
			return node;
		}
	}
	return nullptr;
}

static int Heuristic( Cell node, Cell goal, AStarMovementAllowed movement ) {
	switch ( movement ) {
	case ASTAR_ALLOW_DIAGONALS:
		return MAX( std::abs( ( int )node.x - ( int )goal.x ), std::abs( ( int )node.z - ( int )goal.z ) );
	case ASTAR_FORBID_DIAGONALS:
		return std::abs( ( int )node.x - ( int )goal.x ) + std::abs( ( int )node.z - ( int )goal.z );
	case ASTAR_FREE_MOVEMENT:
		float x = ( float )node.x - goal.x;
		float z = ( float )node.z - goal.z;
		return ( int )( sqrtf( x * x + z * z ) * 10.0f );
	}
	ng_assert( false );
	return 0;
}

glm::vec3 GetPointInMiddleOfCell( Cell cell ) {
	return glm::vec3( cell.x + CELL_SIZE / 2.0f, 0.0f, cell.z + CELL_SIZE / 2.0f );
}

glm::vec3 GetPointInCornerOfCell( Cell cell ) { return glm::vec3( cell.x, 0.0f, cell.z ); }

Cell GetCellForPoint( glm::vec3 point ) {
	// ng_assert( point.x >= 0.0f && point.z >= 0.0f );
	if ( point.x < 0 ) {
		point.x = 0;
	}
	if ( point.z < 0 ) {
		point.z = 0;
	}
	auto cell = Cell( ( u32 )point.x, ( u32 )point.z );
	return cell;
}

Cell GetCellAfterMovement( Cell start, int movementX, int movementZ ) {
	ng_assert( movementX >= 0 || start.x > 0 );
	ng_assert( movementZ >= 0 || start.z > 0 );
	return Cell( start.x + movementX, start.z + movementZ );
}

Cell GetCellAfterMovement( Cell start, CardinalDirection direction ) {
	switch ( direction ) {
	case SOUTH:
		return GetCellAfterMovement( start, -1, 0 );
	case NORTH:
		return GetCellAfterMovement( start, 1, 0 );
	case EAST:
		return GetCellAfterMovement( start, 0, -1 );
	case WEST:
		return GetCellAfterMovement( start, 0, 1 );
	}
	return INVALID_CELL;
}

CardinalDirection GetDirectionFromCellTo( Cell from, Cell to ) {
	int deltaX = ( int )to.x - from.x;
	int deltaZ = ( int )to.z - from.z;
	ng_assert( std::abs( deltaX ) + std::abs( deltaZ ) == 1 );
	if ( deltaX > 0 )
		return NORTH;
	if ( deltaX < 0 )
		return SOUTH;
	if ( deltaZ > 0 )
		return WEST;
	if ( deltaZ < 0 )
		return EAST;
	return NORTH;
}

CardinalDirection OppositeDirection( CardinalDirection direction ) {
	switch ( direction ) {
	case SOUTH:
		return NORTH;
	case NORTH:
		return SOUTH;
	case EAST:
		return WEST;
	case WEST:
		return EAST;
	default:
		ng_assert( false );
		return NORTH;
	}
}

static thread_local ng::ObjectPool< AStarNode > aStarNodePool;

bool AStar( Cell start, Cell goal, AStarMovementAllowed movement, const Map & map, std::vector< Cell > & outPath ) {
	ng::ScopedChrono chrono( "A Star" );
	constexpr int    infiniteWeight = INT_MAX;

	std::vector< AStarNode * > openSet;
	std::vector< AStarNode * > closedSet;

	AStarNode * startNode = aStarNodePool.Pop();
	startNode->coord = start;
	startNode->h = Heuristic( start, goal, movement );
	startNode->g = 1;
	startNode->f = startNode->h + startNode->g;
	openSet.push_back( startNode );

	AStarNode * current = nullptr;
	while ( openSet.empty() == false ) {
		auto currentIt = openSet.begin();
		current = *currentIt;

		for ( auto it = openSet.begin(); it != openSet.end(); it++ ) {
			AStarNode * node = *it;
			if ( node->f <= current->f ) {
				current = node;
				currentIt = it;
			}
		}

		if ( current->coord == goal ) {
			break;
		}

		closedSet.push_back( current );
		openSet.erase( currentIt );

		for ( u32 x = current->coord.x == 0 ? current->coord.x : current->coord.x - 1; x <= current->coord.x + 1;
		      x++ ) {
			for ( u32 z = current->coord.z == 0 ? current->coord.z : current->coord.z - 1; z <= current->coord.z + 1;
			      z++ ) {
				if ( ( x == current->coord.x && z == current->coord.z ) || x >= map.sizeX || z >= map.sizeZ ) {
					continue;
				}
				Cell neighborCoords( x, z );
				if ( map.IsTileWalkable( neighborCoords ) == false ||
				     ( movement == ASTAR_FORBID_DIAGONALS && x != current->coord.x && z != current->coord.z ) ) {
					continue;
				}
				if ( FindNodeInSet( closedSet, neighborCoords ) ) {
					continue;
				}

				// 14 is the equivalent of sqrtf(2.0f) * 10
				int distance = x == current->coord.x || z == current->coord.z ? 10 : 14;
				int totalCost = current->g + distance;

				AStarNode * neighbor = FindNodeInSet( openSet, neighborCoords );
				if ( neighbor == nullptr ) {
					neighbor = aStarNodePool.Pop();
					neighbor->coord = neighborCoords;
					neighbor->parent = current;
					neighbor->g = totalCost;
					neighbor->h = Heuristic( neighbor->coord, goal, movement );
					neighbor->f = neighbor->g + neighbor->h;
					openSet.push_back( neighbor );
				} else if ( neighbor->g > totalCost ) {
					neighbor->parent = current;
					neighbor->g = totalCost;
					neighbor->f = neighbor->g + neighbor->h;
				}
			}
		}
	}
	if ( current == nullptr ) {
		return false;
	}
	// return reconstruct path
	outPath.clear();
	outPath.push_back( current->coord );

	auto cursor = current->parent;
	while ( cursor != nullptr ) {
		outPath.push_back( cursor->coord );
		cursor = cursor->parent;
	}

	for ( AStarNode * node : openSet ) {
		aStarNodePool.Push( node );
	}
	for ( AStarNode * node : closedSet ) {
		aStarNodePool.Push( node );
	}

	return current->coord == goal;
}

void SystemNavAgent::Update( Registery & reg, float dt ) {
	for ( auto & [ e, agent ] : reg.IterateOver< CpntNavAgent >() ) {
		CpntTransform & transform = reg.GetComponent< CpntTransform >( e );
		float           remainingSpeed = agent.movementSpeed * dt;
		while ( agent.pathfindingNextSteps.empty() == false && remainingSpeed > 0.0f ) {
			Cell      nextStep = agent.pathfindingNextSteps.back();
			glm::vec3 nextCoord = GetPointInMiddleOfCell( nextStep );
			float     distance = glm::distance( transform.GetTranslation(), nextCoord );
			if ( distance > agent.movementSpeed * dt ) {
				glm::vec3 direction = glm::normalize( nextCoord - transform.GetTranslation() );
				transform.Translate( direction * agent.movementSpeed * dt );
			} else {
				transform.SetTranslation( nextCoord );
				agent.pathfindingNextSteps.pop_back();
				if ( agent.pathfindingNextSteps.empty() == true ) {
					// we are at destination
					reg.BroadcastMessage( e, MESSAGE_PATHFINDING_DESTINATION_REACHED );
				}
			}
			remainingSpeed -= distance;
		}
	}
}

static std::vector< Cell > GetNeighborsOfCell( Cell base, const Map & map ) {
	std::vector< Cell > neighbors;
	if ( base.x > 0 )
		neighbors.push_back( GetCellAfterMovement( base, -1, 0 ) );
	if ( base.x < map.sizeX - 1 )
		neighbors.push_back( GetCellAfterMovement( base, 1, 0 ) );
	if ( base.z > 0 )
		neighbors.push_back( GetCellAfterMovement( base, 0, -1 ) );
	if ( base.z < map.sizeZ - 1 )
		neighbors.push_back( GetCellAfterMovement( base, 0, 1 ) );

	return neighbors;
}

static void GetNeighborsOfCell( Cell base, Cell neighbors[ 4 ] ) {
	neighbors[ 0 ] = GetCellAfterMovement( base, -1, 0 );
	neighbors[ 1 ] = GetCellAfterMovement( base, 1, 0 );
	neighbors[ 2 ] = GetCellAfterMovement( base, 0, -1 );
	neighbors[ 3 ] = GetCellAfterMovement( base, 0, 1 );
}

RoadNetwork::Node * RoadNetwork::FindNodeWithPosition( Cell cell ) {
	for ( Node & node : nodes ) {
		if ( node.position == cell ) {
			return &node;
		}
	}
	return nullptr;
}

bool RoadNetwork::RemoveNodeByPosition( Cell cell ) {
	for ( u64 i = 0; i < nodes.size(); i++ ) {
		if ( nodes[ i ].position == cell ) {
			nodes.erase( nodes.begin() + i );
			return true;
		}
	}
	return false;
}

RoadNetwork::Node SplitRoad( RoadNetwork & network, const Map & map, Cell cell ) {
	RoadNetwork::NodeSearchResult searchA{};
	RoadNetwork::NodeSearchResult searchB{};
	network.FindNearestRoadNodes( cell, map, searchA, searchB );
	ng_assert( searchA.found == true && searchB.found == true );

	RoadNetwork::Connection * connectionFromA = searchA.node->FindConnectionWith( searchB.node->position );
	connectionFromA->connectedTo = cell;
	connectionFromA->distance = searchA.distance;

	RoadNetwork::Connection * connectionFromB = searchB.node->FindConnectionWith( searchA.node->position );
	connectionFromB->connectedTo = cell;
	connectionFromB->distance = searchB.distance;

	// create new node on old road
	RoadNetwork::Node newNode;
	newNode.position = cell;
	newNode.connections[ searchA.directionFromStart ] =
	    RoadNetwork::Connection( searchA.node->position, searchA.distance );
	newNode.connections[ searchB.directionFromStart ] =
	    RoadNetwork::Connection( searchB.node->position, searchB.distance );
	return newNode;
}

void RoadNetwork::AddRoadCellToNetwork( Cell buildingCell, const Map & map ) {
	auto                neighbors = GetNeighborsOfCell( buildingCell, map );
	std::vector< Cell > roadNeighbors;
	u32                 numRoadNeighbors = 0;
	for ( const Cell & neighbor : neighbors ) {
		if ( map.GetTile( neighbor ) == MapTile::ROAD ) {
			numRoadNeighbors++;
			roadNeighbors.push_back( neighbor );
		}
	}

	if ( numRoadNeighbors == 0 ) {
		// Lonely new road
		Node newNode;
		newNode.position = buildingCell;
		nodes.push_back( newNode );
		return;
	}

	if ( roadNeighbors.size() == 1 ) {
		// creating, growing or splitting a road
		Cell & roadNeighbor = roadNeighbors[ 0 ];
		Node * nodeNeighbor = FindNodeWithPosition( roadNeighbor );
		if ( nodeNeighbor != nullptr ) {
			// Growing or creating a new road
			if ( nodeNeighbor->NumSetConnections() == 1 ) {
				// Growing road
				Node * nodeConnectedTo =
				    FindNodeWithPosition( nodeNeighbor->GetSetConnectionWithOffset( 0 )->connectedTo );
				u32 previousDistance = nodeNeighbor->GetSetConnectionWithOffset( 0 )->distance;
				ng_assert( nodeConnectedTo != nullptr );

				Connection * connection = nodeConnectedTo->FindConnectionWith( roadNeighbor );
				ng_assert( connection != nullptr );
				connection->distance++;
				connection->connectedTo = buildingCell;

				CardinalDirection growingDirection = GetDirectionFromCellTo( buildingCell, roadNeighbor );
				nodeNeighbor->ClearConnections();
				nodeNeighbor->connections[ growingDirection ].connectedTo = nodeConnectedTo->position;
				nodeNeighbor->connections[ growingDirection ].distance = previousDistance + 1;
				nodeNeighbor->position = buildingCell;
			} else {
				// Creating new road
				CardinalDirection growingDirection = GetDirectionFromCellTo( roadNeighbor, buildingCell );
				nodeNeighbor->connections[ growingDirection ].connectedTo = buildingCell;
				nodeNeighbor->connections[ growingDirection ].distance = 1;

				Node newNode;
				newNode.position = buildingCell;
				newNode.connections[ OppositeDirection( growingDirection ) ].connectedTo = roadNeighbor;
				newNode.connections[ OppositeDirection( growingDirection ) ].distance = 1;
				nodes.push_back( newNode );
			}
		} else {
			// Splitting a road
			Node newNodeOnOldRoad = SplitRoad( *this, map, roadNeighbor );
			newNodeOnOldRoad.connections[ GetDirectionFromCellTo( roadNeighbor, buildingCell ) ] =
			    Connection( buildingCell, 1 );

			// Create new node
			Node newNode;
			newNode.position = buildingCell;
			newNode.connections[ GetDirectionFromCellTo( buildingCell, roadNeighbor ) ] = Connection( roadNeighbor, 1 );

			nodes.push_back( newNodeOnOldRoad );
			nodes.push_back( newNode );
		}
	} else if ( roadNeighbors.size() == 2 ) {
		// Merge or connect two roads
		Cell & cellA = roadNeighbors[ 0 ];
		Cell & cellB = roadNeighbors[ 1 ];
		Node * nodeA = FindNodeWithPosition( cellA );
		Node * nodeB = FindNodeWithPosition( cellB );

		if ( nodeA != nullptr && nodeB != nullptr ) {
			// If both neighbords are a node
			if ( nodeA->NumSetConnections() == 0 || nodeB->NumSetConnections() == 0 ||
			     ( nodeA->NumSetConnections() > 1 && nodeB->NumSetConnections() > 1 ) ) {
				// If one of them has no connections OR if both of them already has multiple connections,
				// then just connect them
				CardinalDirection growDirectionA = GetDirectionFromCellTo( cellA, buildingCell );
				CardinalDirection growDirectionB = GetDirectionFromCellTo( cellB, buildingCell );
				ng_assert( nodeA->connections[ growDirectionA ].IsValid() == false );
				ng_assert( nodeB->connections[ growDirectionB ].IsValid() == false );
				Connection * connectionA = &nodeA->connections[ growDirectionA ];
				Connection * connectionB = &nodeB->connections[ growDirectionB ];
				connectionA->connectedTo = cellB;
				connectionA->distance = 2;
				connectionB->connectedTo = cellA;
				connectionB->distance = 2;
			} else if ( nodeA->NumSetConnections() == 1 && nodeB->NumSetConnections() == 1 ) {
				// if both are nodes, and both of them has only one connection, we can merge them
				Connection * connectionFromA = nodeA->GetSetConnectionWithOffset( 0 );
				Connection * connectionFromB = nodeB->GetSetConnectionWithOffset( 0 );
				Node *       nodeConnectedWithA = FindNodeWithPosition( connectionFromA->connectedTo );
				Node *       nodeConnectedWithB = FindNodeWithPosition( connectionFromB->connectedTo );
				Connection * connectionFromNodeConnectedWithA =
				    nodeConnectedWithA->FindConnectionWith( nodeA->position );
				Connection * connectionFromNodeConnectedWithB =
				    nodeConnectedWithB->FindConnectionWith( nodeB->position );

				// The new distance is the sum of the old distance, plus 2 for the new bridge
				u32 distance =
				    connectionFromNodeConnectedWithA->distance + connectionFromNodeConnectedWithB->distance + 2;

				connectionFromNodeConnectedWithA->connectedTo = nodeConnectedWithB->position;
				connectionFromNodeConnectedWithA->distance = distance;

				connectionFromNodeConnectedWithB->connectedTo = nodeConnectedWithA->position;
				connectionFromNodeConnectedWithB->distance = distance;

				RemoveNodeByPosition( cellA );
				RemoveNodeByPosition( cellB );
			} else if ( nodeA->NumSetConnections() == 1 || nodeB->NumSetConnections() == 1 ) {
				// One of them has 1 connection, the other has more
				// We can grow the road that has 1 an connect it to the node that has more
				// Let's consider A the one with only 1 connection
				if ( nodeA->NumSetConnections() > 1 ) {
					std::swap( nodeA, nodeB );
					std::swap( cellA, cellB );
				}
				// Node A will disappear and give its connection to B
				CardinalDirection growDirectionB = GetDirectionFromCellTo( cellB, buildingCell );
				ng_assert( nodeB->connections[ growDirectionB ].IsValid() == false );

				Connection * connectionFromA = nodeA->GetSetConnectionWithOffset( 0 );
				Node *       nodeConnectedWithA = FindNodeWithPosition( connectionFromA->connectedTo );
				Connection * connectionFromNodeConnectedWithA =
				    nodeConnectedWithA->FindConnectionWith( nodeA->position );
				// The new distance is the old distance, plus 2 for the new bridge
				u32 distance = connectionFromNodeConnectedWithA->distance + 2;

				connectionFromNodeConnectedWithA->connectedTo = cellB;
				connectionFromNodeConnectedWithA->distance = distance;
				nodeB->connections[ growDirectionB ].connectedTo = nodeConnectedWithA->position;
				nodeB->connections[ growDirectionB ].distance = distance;

				RemoveNodeByPosition( cellA );
			} else {
				// Did I miss a potential case?
				ng_assert( false );
			}
		} else if ( nodeA != nullptr || nodeB != nullptr ) {
			// If only one of them is a road node
			// Let's consider A the node
			if ( nodeA == nullptr ) {
				std::swap( nodeA, nodeB );
				std::swap( cellA, cellB );
			}
			// If A has only one connection, "move" A into position B and split the road B is on
			// If A has no or multiple connections, create a node on position B a connect it to A
			// So either way, create a node on position B and split its road
			Node              newNodeOnB = SplitRoad( *this, map, cellB );
			nodes.push_back( newNodeOnB );
			// Poll A again because pushing B might invalidate the pointer
			nodeA = FindNodeWithPosition(cellA);
			nodeB = FindNodeWithPosition(cellB);
			CardinalDirection growDirectionB = GetDirectionFromCellTo( cellB, buildingCell );

			if ( nodeA->NumSetConnections() == 1 ) {
				Connection * connectionFromA = nodeA->GetSetConnectionWithOffset( 0 );
				Node *       nodeConnectedWithA = FindNodeWithPosition( connectionFromA->connectedTo );
				Connection * connectionFromNodeConnectedWithA =
				    nodeConnectedWithA->FindConnectionWith( nodeA->position );
				// The new distance is the old distance, plus 2 for the new bridge
				u32 distance = connectionFromNodeConnectedWithA->distance + 2;

				connectionFromNodeConnectedWithA->connectedTo = cellB;
				connectionFromNodeConnectedWithA->distance = distance;
				nodeB->connections[ growDirectionB ].connectedTo = nodeConnectedWithA->position;
				nodeB->connections[ growDirectionB ].distance = distance;
				RemoveNodeByPosition( cellA );
			} else {
				CardinalDirection growDirectionA = GetDirectionFromCellTo( cellA, buildingCell );
				nodeA->connections[ growDirectionA ].connectedTo = cellB;
				nodeA->connections[ growDirectionA ].distance = 2;
				nodeB->connections[ growDirectionB ].connectedTo = cellA;
				nodeB->connections[ growDirectionB ].distance = 2;
			}
		} else {
			// Both cells are not node, split their road on their positions and join them
			Node newNodeOnA = SplitRoad( *this, map, cellA );
			// We add A to the network right away, because B might get connected with A
			nodes.push_back( newNodeOnA );
			Node newNodeOnB = SplitRoad( *this, map, cellB );
			nodes.push_back( newNodeOnB );

			nodeA = FindNodeWithPosition( cellA );
			nodeB = FindNodeWithPosition( cellB );
			CardinalDirection growDirectionA = GetDirectionFromCellTo( cellA, buildingCell );
			CardinalDirection growDirectionB = GetDirectionFromCellTo( cellB, buildingCell );
			ng_assert( nodeA->connections[ growDirectionA ].IsValid() == false );
			ng_assert( nodeB->connections[ growDirectionB ].IsValid() == false );
			nodeA->connections[ growDirectionA ].connectedTo = cellB;
			nodeA->connections[ growDirectionA ].distance = 2;
			nodeB->connections[ growDirectionB ].connectedTo = cellA;
			nodeB->connections[ growDirectionB ].distance = 2;
		}
	} else if ( roadNeighbors.size() == 3 ) {
	} else if ( roadNeighbors.size() == 4 ) {
	}
}

RoadNetwork::NodeSearchResult RoadNetwork::FindNearestRoadNode( Cell cell, const Map & map ) {
	ng_assert( map.GetTile( cell ) == MapTile::ROAD );
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
		return NodeSearchResult{};
	}

	Node * node = FindNodeWithPosition( cell );
	if ( node != nullptr ) {
		ng_assert( false );
		NodeSearchResult result{};
		result.found = true;
		result.node = node;
		result.distance = 0;
		return result;
	}

	Cell roadNeighbors[ 2 ];
	auto neighbors = GetNeighborsOfCell( cell, map );
	u32  numRoadNeighbors = 0;
	for ( const Cell & neighbor : neighbors ) {
		if ( map.GetTile( neighbor ) == MapTile::ROAD ) {
			roadNeighbors[ numRoadNeighbors++ ] = neighbor;
			ng_assert( numRoadNeighbors <= 2 );
		}
	}
	ng_assert( numRoadNeighbors == 2 );

	Cell              previousNodes[ 2 ] = { cell, cell };
	u32               distancesFromStart[ 2 ] = { 1, 1 };
	CardinalDirection directionsFromStart[ 2 ] = { GetDirectionFromCellTo( cell, roadNeighbors[ 0 ] ),
	                                               GetDirectionFromCellTo( cell, roadNeighbors[ 1 ] ) };

	while ( true ) {
		for ( int i = 0; i < 2; i++ ) {
			Node * node = FindNodeWithPosition( roadNeighbors[ i ] );
			if ( node != nullptr ) {
				NodeSearchResult result{};
				result.found = true;
				result.node = node;
				result.distance = distancesFromStart[ i ];
				result.directionFromStart = directionsFromStart[ i ];
				return result;
			}
			auto neighbors = GetNeighborsOfCell( roadNeighbors[ i ], map );
			bool nextRoadIsFound = false;
			distancesFromStart[ i ]++;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousNodes[ i ] && map.GetTile( neighbor ) == MapTile::ROAD ) {
					nextRoadIsFound = true;
					previousNodes[ i ] = roadNeighbors[ i ];
					roadNeighbors[ i ] = neighbor;
					break;
				}
			}
			ng_assert( nextRoadIsFound == true );
		}
	}

	return NodeSearchResult{};
}

void RoadNetwork::FindNearestRoadNodes( Cell               cell,
                                        const Map &        map,
                                        NodeSearchResult & first,
                                        NodeSearchResult & second ) {

	ng_assert( map.GetTile( cell ) == MapTile::ROAD );
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
		return;
	}

	Node * node = FindNodeWithPosition( cell );
	if ( node != nullptr ) {
		ng_assert( false );
		first.found = true;
		first.node = node;
		first.distance = 0;
		return;
	}

	Cell roadNeighbors[ 2 ];
	auto neighbors = GetNeighborsOfCell( cell, map );
	u32  numRoadNeighbors = 0;
	for ( const Cell & neighbor : neighbors ) {
		if ( map.GetTile( neighbor ) == MapTile::ROAD ) {
			roadNeighbors[ numRoadNeighbors++ ] = neighbor;
			ng_assert( numRoadNeighbors <= 2 );
		}
	}
	ng_assert( numRoadNeighbors == 2 );

	Cell              previousNodes[ 2 ] = { cell, cell };
	u32               distancesFromStart[ 2 ] = { 1, 1 };
	CardinalDirection directionsFromStart[ 2 ] = { GetDirectionFromCellTo( cell, roadNeighbors[ 0 ] ),
	                                               GetDirectionFromCellTo( cell, roadNeighbors[ 1 ] ) };

	bool nodeAFound = false;
	bool nodeBFound = true;
	for ( int i = 0; i < 2; i++ ) {
		while ( true ) {
			Node * node = FindNodeWithPosition( roadNeighbors[ i ] );
			if ( node != nullptr ) {
				NodeSearchResult result{};
				result.found = true;
				result.node = node;
				result.distance = distancesFromStart[ i ];
				result.directionFromStart = directionsFromStart[ i ];
				if ( i == 0 ) {
					first = result;
				} else {
					second = result;
				}
				break;
			}
			auto neighbors = GetNeighborsOfCell( roadNeighbors[ i ], map );
			bool nextRoadIsFound = false;
			distancesFromStart[ i ]++;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousNodes[ i ] && map.GetTile( neighbor ) == MapTile::ROAD ) {
					nextRoadIsFound = true;
					previousNodes[ i ] = roadNeighbors[ i ];
					roadNeighbors[ i ] = neighbor;
					break;
				}
			}
			ng_assert( nextRoadIsFound == true );
		}
	}
}
