#include "navigation.h"
#include "buildings/building.h"
#include "message.h"
#include "ngLib/logs.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "ngLib/types.h"
#include "registery.h"
#include <algorithm>
#include <array>
#include <functional>
#include <tracy/Tracy.hpp>
#include <vector>

struct AStarStep {
	Cell                coord;
	RoadNetwork::Node * node = nullptr;
	AStarStep *         parent = nullptr;
	int                 f, g, h;

	bool operator==( const AStarStep & rhs ) const { return coord == rhs.coord; }
	bool operator<( const AStarStep & rhs ) const { return coord < rhs.coord; }

	bool operator==( const Cell & rhs ) const { return coord == rhs; }
	bool operator<( const Cell & rhs ) const { return coord < rhs; }
	bool operator>( const Cell & rhs ) const { return !( coord < rhs || coord == rhs ); }
};

static auto InsertNodeSorted( std::vector< AStarStep * > & set, AStarStep * step ) {
	return set.insert( std::upper_bound( set.begin(), set.end(), step,
	                                     []( AStarStep * a, AStarStep * b ) -> bool { return *a < *b; } ),
	                   step );
}

static AStarStep * BinarySearchNode( std::vector< AStarStep * > & set, Cell coords ) {
	ZoneScoped;
	if ( set.size() == 0 ) {
		return nullptr;
	}
	int64 left = 0;
	int64 right = set.size() - 1;
	while ( left <= right ) {
		int64 m = ( left + right ) / 2;
		if ( *set[ m ] < coords ) {
			left = m + 1;
		} else if ( *set[ m ] > coords ) {
			right = m - 1;
		} else {
			return set[ m ];
		}
	}
	return nullptr;
}

static AStarStep * FindNodeInSet( ng::DynamicArray< AStarStep * > & set, Cell coords ) {
	for ( auto node : set ) {
		if ( node->coord == coords ) {
			return node;
		}
	}
	return nullptr;
}

static bool FindNodeInList( ng::LinkedList< AStarStep > & list, Cell coords ) {
	for ( auto & node : list ) {
		if ( node.coord == coords ) {
			return true;
		}
	}
	return false;
}

static bool PopNodeFromList( ng::LinkedList< AStarStep > & list, Cell coords, AStarStep & out ) {
	for ( auto cursor = list.head; cursor != nullptr; cursor = cursor->next ) {
		if ( cursor->data.coord == coords ) {
			out = cursor->data;
			if ( cursor == list.head ) {
				list.head = nullptr;
			}
			list.DeleteNode( cursor );
			return true;
		}
	}
	return false;
}

static int Heuristic( Cell node, Cell goal, MovementAllowed movement ) {
	switch ( movement ) {
	case ASTAR_ALLOW_DIAGONALS:
		return MAX( std::abs( ( int )node.x - ( int )goal.x ), std::abs( ( int )node.z - ( int )goal.z ) );
	case ASTAR_FORBID_DIAGONALS:
		return std::abs( ( int )node.x - ( int )goal.x ) + std::abs( ( int )node.z - ( int )goal.z );
	}
	ng_assert( false );
	return 0;
}

glm::vec3 GetPointInMiddleOfCell( Cell cell ) {
	return glm::vec3( cell.x + CELL_SIZE / 2.0f, 0.0f, cell.z + CELL_SIZE / 2.0f );
}

glm::vec3 GetPointInCornerOfCell( Cell cell ) { return glm::vec3( cell.x, 0.0f, cell.z ); }

Cell GetCellForPoint( glm::vec3 point ) {
	if ( point.x < 0 ) {
		point.x = 0;
	}
	if ( point.z < 0 ) {
		point.z = 0;
	}
	auto cell = Cell( ( u32 )point.x, ( u32 )point.z );
	return cell;
}

Cell GetCellForTransform( const CpntTransform & transform ) {
	return GetCellForPoint( transform.GetTranslation() ); 
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
	else
		return EAST;
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

static thread_local ng::ObjectPool< AStarStep > aStarStepPool;

bool AStar(
    Cell start, Cell goal, MovementAllowed movement, const Map & map, ng::DynamicArray< Cell > & outPath ) {
	ZoneScoped;

	ng_assert(movement == ASTAR_ALLOW_DIAGONALS || movement == ASTAR_FORBID_DIAGONALS );
	if ( !map.IsTileAStarNavigable( start ) || !map.IsTileAStarNavigable( goal ) ) {
		return false;
	}

	std::vector< AStarStep * > openSet;
	openSet.reserve( 256 );
	std::vector< AStarStep * > closedSet;
	closedSet.reserve( 256 );

	AStarStep * startStep = aStarStepPool.Pop();
	startStep->parent = nullptr;
	startStep->coord = start;
	startStep->h = Heuristic( start, goal, movement );
	startStep->g = 1;
	startStep->f = startStep->h + startStep->g;
	openSet.push_back( startStep );

	AStarStep * current = nullptr;
	while ( openSet.empty() == false ) {
		auto currentIt = openSet.begin();
		{
			ZoneScopedN( "Find cell with lowest score" );
			current = *currentIt;

			for ( auto it = openSet.begin(); it != openSet.end(); it++ ) {
				AStarStep * step = *it;
				if ( step->f <= current->f ) {
					current = step;
					currentIt = it;
				}
			}
		}

		if ( current->coord == goal ) {
			break;
		}

		InsertNodeSorted( closedSet, current );
		openSet.erase( currentIt );

		for ( u32 x = current->coord.x == 0 ? current->coord.x : current->coord.x - 1; x <= current->coord.x + 1;
		      x++ ) {
			for ( u32 z = current->coord.z == 0 ? current->coord.z : current->coord.z - 1; z <= current->coord.z + 1;
			      z++ ) {
				if ( ( x == current->coord.x && z == current->coord.z ) || x >= map.sizeX || z >= map.sizeZ ) {
					continue;
				}
				Cell neighborCoords( x, z );
				if ( !map.IsTileAStarNavigable( neighborCoords ) ||
				     ( movement == ASTAR_FORBID_DIAGONALS && x != current->coord.x && z != current->coord.z ) ) {
					continue;
				}
				if ( BinarySearchNode( closedSet, neighborCoords ) ) {
					continue;
				}

				// 14 is the equivalent of sqrtf(2.0f) * 10
				int distance = x == current->coord.x || z == current->coord.z ? 10 : 14;
				int totalCost = current->g + distance;

				AStarStep * neighbor = BinarySearchNode( openSet, neighborCoords );
				if ( neighbor == nullptr ) {
					neighbor = aStarStepPool.Pop();
					neighbor->coord = neighborCoords;
					neighbor->parent = current;
					neighbor->g = totalCost;
					neighbor->h = Heuristic( neighbor->coord, goal, movement );
					neighbor->f = neighbor->g + neighbor->h;
					InsertNodeSorted( openSet, neighbor );
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
	if ( current->coord != goal ) {
		return false;
	}

	// return reconstruct path
	outPath.Clear();
	outPath.PushBack( current->coord );

	auto cursor = current->parent;
	while ( cursor != nullptr ) {
		outPath.PushBack( cursor->coord );
		cursor = cursor->parent;
	}

	for ( AStarStep * step : openSet ) {
		aStarStepPool.Push( step );
	}
	for ( AStarStep * step : closedSet ) {
		aStarStepPool.Push( step );
	}

	return true;
}

void SystemNavAgent::Update( Registery & reg, Duration ticks ) {
	for ( auto [ e, agent ] : reg.IterateOver< CpntNavAgent >() ) {
		CpntTransform & transform = reg.GetComponent< CpntTransform >( e );
		float           remainingSpeed = agent.movementSpeed * ticks;
		while ( agent.pathfindingNextSteps.Empty() == false && remainingSpeed > 0.0f ) {
			Cell      nextStep = agent.pathfindingNextSteps.Last();
			glm::vec3 nextCoord = GetPointInMiddleOfCell( nextStep );
			float     distance = glm::distance( transform.GetTranslation(), nextCoord );
			if ( distance > agent.movementSpeed * ticks ) {
				glm::vec3 direction = glm::normalize( nextCoord - transform.GetTranslation() );
				transform.Translate( direction * ( agent.movementSpeed * ticks ) );
			} else {
				transform.SetTranslation( nextCoord );
				agent.pathfindingNextSteps.PopBack();
				if ( agent.pathfindingNextSteps.Empty() == true ) {
					// we are at destination
					PostMsg( MESSAGE_NAVAGENT_DESTINATION_REACHED, e, e );
					if ( agent.deleteAtDestination ) {
						reg.MarkForDelete( e );
					}
				}
			}
			remainingSpeed -= distance;
		}
	}
}

void GetNeighborsOfCell( Cell base, const Map & map, ng::StaticArray< Cell, 4 > & neighbors ) {
	if ( base.x > 0 )
		neighbors.PushBack( GetCellAfterMovement( base, -1, 0 ) );
	if ( base.x < map.sizeX - 1 )
		neighbors.PushBack( GetCellAfterMovement( base, 1, 0 ) );
	if ( base.z > 0 )
		neighbors.PushBack( GetCellAfterMovement( base, 0, -1 ) );
	if ( base.z < map.sizeZ - 1 )
		neighbors.PushBack( GetCellAfterMovement( base, 0, 1 ) );
}

static void GetWalkableNeighborsOfCell( Cell base, const Map & map, ng::StaticArray< Cell, 4 > & neighbors ) {
	if ( base.x > 0 ) {
		Cell cell = GetCellAfterMovement( base, -1, 0 );
		if ( map.IsTileWalkable( cell ) ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.x < map.sizeX - 1 ) {
		Cell cell = GetCellAfterMovement( base, 1, 0 );
		if ( map.IsTileWalkable( cell ) ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.z > 0 ) {
		Cell cell = GetCellAfterMovement( base, 0, -1 );
		if ( map.IsTileWalkable( cell ) ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.z < map.sizeZ - 1 ) {
		Cell cell = GetCellAfterMovement( base, 0, 1 );
		if ( map.IsTileWalkable( cell ) ) {
			neighbors.PushBack( cell );
		}
	}
}

static void GetRoadNeighborsOfCell( Cell base, const Map & map, ng::StaticArray< Cell, 4 > & neighbors ) {
	if ( base.x > 0 ) {
		Cell cell = GetCellAfterMovement( base, -1, 0 );
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.x < map.sizeX - 1 ) {
		Cell cell = GetCellAfterMovement( base, 1, 0 );
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.z > 0 ) {
		Cell cell = GetCellAfterMovement( base, 0, -1 );
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			neighbors.PushBack( cell );
		}
	}
	if ( base.z < map.sizeZ - 1 ) {
		Cell cell = GetCellAfterMovement( base, 0, 1 );
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			neighbors.PushBack( cell );
		}
	}
}

RoadNetwork::Node * RoadNetwork::FindNodeWithPosition( Cell cell ) {
	for ( Node & node : nodes ) {
		if ( node.position == cell ) {
			return &node;
		}
	}
	return nullptr;
}

RoadNetwork::Node * RoadNetwork::ResolveConnection( Connection * connection ) {
	ng_assert( connection->IsValid() );
	return FindNodeWithPosition( connection->connectedTo );
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

	RoadNetwork::Connection & connectionFromA = searchA.node->connections[ searchA.directionFromEnd ];
	RoadNetwork::Connection & connectionFromB = searchB.node->connections[ searchB.directionFromEnd ];
	ng_assert( connectionFromA.IsValid() && connectionFromB.IsValid() );

	connectionFromA.connectedTo = cell;
	connectionFromA.distance = searchA.distance;

	connectionFromB.connectedTo = cell;
	connectionFromB.distance = searchB.distance;

	// create new node on old road
	RoadNetwork::Node newNode;
	newNode.position = cell;
	newNode.connections[ searchA.directionFromStart ] =
	    RoadNetwork::Connection( searchA.node->position, searchA.distance );
	newNode.connections[ searchB.directionFromStart ] =
	    RoadNetwork::Connection( searchB.node->position, searchB.distance );
	return newNode;
}

void RoadNetwork::AddRoadCellToNetwork( Cell cellToAdd, const Map & map ) {
	ng::StaticArray< Cell, 4 > roadNeighbors;
	GetWalkableNeighborsOfCell( cellToAdd, map, roadNeighbors );

	Node newNode;
	newNode.position = cellToAdd;

	for ( Cell & neighbor : roadNeighbors ) {
		// Connect the new cell with its neighbors
		Node *            nodeNeighbor = FindNodeWithPosition( neighbor );
		CardinalDirection direction = GetDirectionFromCellTo( cellToAdd, neighbor );
		if ( nodeNeighbor == nullptr ) {
			// Split the road
			Node split = SplitRoad( *this, map, neighbor );
			nodes.push_back( split );
		}
		nodeNeighbor = FindNodeWithPosition( neighbor );
		ng_assert( nodeNeighbor != nullptr );
		nodeNeighbor->connections[ OppositeDirection( direction ) ] = Connection( cellToAdd, 1 );
		newNode.connections[ direction ] = Connection( neighbor, 1 );
	}
	nodes.push_back( newNode );

	// Now that we have connected with the neighbors, let's see if we can simplify the mesh
	for ( Cell & neighbor : roadNeighbors ) {
		Node * buildingNode = FindNodeWithPosition( cellToAdd );
		Node * nodeNeighbor = FindNodeWithPosition( neighbor );
		if ( nodeNeighbor->NumSetConnections() == 2 && nodeNeighbor->IsConnectedToItself() == false ) {
			DissolveNode( *nodeNeighbor );
		}
	}

	Node * buildingNode = FindNodeWithPosition( cellToAdd );
	if ( buildingNode->NumSetConnections() == 2 && buildingNode->IsConnectedToItself() == false ) {
		// We can remove the connection just built
		DissolveNode( *buildingNode );
	}
}

void RoadNetwork::RemoveRoadCellFromNetwork( Cell cellToRemove, const Map & map ) {
	if ( FindNodeWithPosition( cellToRemove ) == nullptr ) {
		Node split = SplitRoad( *this, map, cellToRemove );
		nodes.push_back( split );
	}

	ng::StaticArray< Cell, 4 > roadNeighbors;
	GetWalkableNeighborsOfCell( cellToRemove, map, roadNeighbors );
	for ( const Cell & neighbor : roadNeighbors ) {
		// If neighbor is not a node, split
		if ( FindNodeWithPosition( neighbor ) == nullptr ) {
			Node split = SplitRoad( *this, map, neighbor );
			nodes.push_back( split );
		}
	}

	for ( Node * node = FindNodeWithPosition( cellToRemove ); node->NumSetConnections() > 0;
	      node = FindNodeWithPosition( cellToRemove ) ) {
		Node * connectedTo = ResolveConnection( node->GetValidConnectionWithOffset( 0 ) );
		node->GetValidConnectionWithOffset( 0 )->Invalidate();
		connectedTo->FindConnectionWith( cellToRemove )->Invalidate();

		// Let's see if our neighbor can be removed now
		if ( connectedTo->NumSetConnections() == 2 && connectedTo->IsConnectedToItself() == false ) {
			// we left a lonely cell with only two connections, we can now remove it
			DissolveNode( *connectedTo );
		}
	}

	RemoveNodeByPosition( cellToRemove );
}

void RoadNetwork::DissolveNode( Node & nodeToDissolve ) {
	ng_assert( nodeToDissolve.NumSetConnections() == 2 );
	Node * nodeA = FindNodeWithPosition( nodeToDissolve.GetValidConnectionWithOffset( 0 )->connectedTo );
	Node * nodeB = FindNodeWithPosition( nodeToDissolve.GetValidConnectionWithOffset( 1 )->connectedTo );

	Connection * connectionFromA = nullptr;
	Connection * connectionFromB = nullptr;
	if ( nodeA->position == nodeB->position ) {
		// We are connected to the same node on both side
		// Lets manually get the two connection
		for ( u32 i = 0; i < nodeA->NumSetConnections(); i++ ) {
			Connection * connection = nodeA->GetValidConnectionWithOffset( i );
			if ( connection->connectedTo == nodeToDissolve.position ) {
				if ( connectionFromA == nullptr ) {
					connectionFromA = connection;
				} else {
					connectionFromB = connection;
				}
			}
		}
	} else {
		connectionFromA = nodeA->FindConnectionWith( nodeToDissolve.position );
		connectionFromB = nodeB->FindConnectionWith( nodeToDissolve.position );
	}

	ng_assert( connectionFromA != nullptr && connectionFromB != nullptr );

	u32 distance = connectionFromA->distance + connectionFromB->distance;
	connectionFromA->connectedTo = nodeB->position;
	connectionFromA->distance = distance;
	connectionFromB->connectedTo = nodeA->position;
	connectionFromB->distance = distance;

	RemoveNodeByPosition( nodeToDissolve.position );
}

void RoadNetwork::FindNearestRoadNodes( Cell               cell,
                                        const Map &        map,
                                        NodeSearchResult & first,
                                        NodeSearchResult & second ) {

	ng_assert( map.IsTileWalkable( cell ) );
	if ( !map.IsTileWalkable( cell ) ) {
		return;
	}

	Node * node = FindNodeWithPosition( cell );
	if ( node != nullptr ) {
		first.found = true;
		first.node = node;
		first.distance = 0;
		return;
	}

	ng::StaticArray< Cell, 4 > roadNeighbors;
	GetWalkableNeighborsOfCell( cell, map, roadNeighbors );
	ng_assert( roadNeighbors.Size() == 2 );

	Cell              previousNodes[ 2 ] = { cell, cell };
	u32               distancesFromStart[ 2 ] = { 1, 1 };
	CardinalDirection directionsFromStart[ 2 ] = { GetDirectionFromCellTo( cell, roadNeighbors[ 0 ] ),
	                                               GetDirectionFromCellTo( cell, roadNeighbors[ 1 ] ) };
	CardinalDirection previousDirection[ 2 ] = { GetDirectionFromCellTo( cell, roadNeighbors[ 0 ] ),
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
				result.directionFromEnd = OppositeDirection( previousDirection[ i ] );
				if ( i == 0 ) {
					first = result;
				} else {
					second = result;
				}
				break;
			}

			ng::StaticArray< Cell, 4 > neighbors;
			GetWalkableNeighborsOfCell( roadNeighbors[ i ], map, neighbors );
			bool nextRoadIsFound = false;
			distancesFromStart[ i ]++;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousNodes[ i ] ) {
					nextRoadIsFound = true;
					previousDirection[ i ] = GetDirectionFromCellTo( roadNeighbors[ i ], neighbor );
					previousNodes[ i ] = roadNeighbors[ i ];
					roadNeighbors[ i ] = neighbor;
					break;
				}
			}
			ng_assert( nextRoadIsFound == true );
		}
	}
}

// WARNING: This assumes that start and goal are actually connected and on a single road
// It can not follow multiple paths
bool BuildPathInsideNodes( Cell                       start,
                           Cell                       goal,
                           const RoadNetwork::Node &  nodeA,
                           const RoadNetwork::Node &  nodeB,
                           const Map &                map,
                           ng::DynamicArray< Cell > & outPath,
                           u32 &                      outTotalDistance ) {
	if ( start == goal ) {
		outPath.PushBack( start );
		outTotalDistance = 0;
		return true;
	}

	ng::StaticArray< Cell, 4 > roadNeighbors;
	GetWalkableNeighborsOfCell( start, map, roadNeighbors );
	ng_assert( roadNeighbors.Size() == 2 );
	Cell                     currentCell[ 2 ] = { roadNeighbors[ 0 ], roadNeighbors[ 1 ] };
	Cell                     previousCell[ 2 ] = { start, start };
	CardinalDirection        previousDirection[ 2 ] = { GetDirectionFromCellTo( start, roadNeighbors[ 0 ] ),
                                                 GetDirectionFromCellTo( start, roadNeighbors[ 1 ] ) };
	u32                      totalDistances[ 2 ] = { 1, 1 };
	ng::DynamicArray< Cell > paths[ 2 ];
	int                      skipIndex0 = 0;
	int                      skipIndex1 = 0;
	while ( skipIndex0 == 0 || skipIndex1 == 0 ) {
		for ( int i = skipIndex0; i < 2 - skipIndex1; i++ ) {
			if ( currentCell[ i ] == goal ) {
				paths[ i ].PushBack( goal );
				outPath.Append( paths[ i ] );
				outTotalDistance = totalDistances[ i ];
				return true;
			}

			ng::StaticArray< Cell, 4 > neighbors;
			GetWalkableNeighborsOfCell( currentCell[ i ], map, neighbors );
			bool nextRoadIsFound = false;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousCell[ i ] ) {
					nextRoadIsFound = true;
					CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell[ i ], neighbor );
					if ( currentDirection != previousDirection[ i ] ) {
						paths[ i ].PushBack( currentCell[ i ] );
					}
					previousDirection[ i ] = currentDirection;
					previousCell[ i ] = currentCell[ i ];
					currentCell[ i ] = neighbor;
					totalDistances[ i ]++;
					break;
				}
			}
			if ( nextRoadIsFound == false || currentCell[ i ] == nodeA.position ||
			     currentCell[ i ] == nodeB.position ) {
				if ( i == 0 ) {
					skipIndex0 = 1;
				} else if ( i == 1 ) {
					skipIndex1 = 1;
				}
			}
		}
	}
	return false;
}

bool BuildPathBetweenNodes( RoadNetwork::Node &        start,
                            RoadNetwork::Node &        goal,
                            const Map &                map,
                            ng::DynamicArray< Cell > & outPath,
                            u32 &                      outTotalDistance ) {
	RoadNetwork::Connection * connection = start.FindShortestConnectionWith( goal.position );
	CardinalDirection         direction = start.GetDirectionOfConnection( connection );

	Cell              currentCell = GetCellAfterMovement( start.position, direction );
	Cell              previousCell = start.position;
	CardinalDirection previousDirection = direction;
	outTotalDistance = 1;
	while ( true ) {
		if ( currentCell == goal.position ) {
			outPath.PushBack( goal.position );
			return true;
		}

		ng::StaticArray< Cell, 4 > neighbors;
		GetWalkableNeighborsOfCell( currentCell, map, neighbors );
		bool nextRoadIsFound = false;
		for ( const Cell & neighbor : neighbors ) {
			if ( neighbor != previousCell ) {
				nextRoadIsFound = true;
				CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell, neighbor );
				if ( currentDirection != previousDirection ) {
					outPath.PushBack( currentCell );
				}
				previousDirection = currentDirection;
				previousCell = currentCell;
				currentCell = neighbor;
				outTotalDistance++;
				break;
			}
		}
		ng_assert( nextRoadIsFound == true );
	}
	return false;
}

bool BuildPathFromNodeToCell( RoadNetwork::Node &        start,
                              const Cell &               goal,
                              const Map &                map,
                              ng::DynamicArray< Cell > & outPath,
                              u32 &                      outTotalDistance,
                              bool                       insertReverse = false ) {
	for ( u32 i = 0; i < start.NumSetConnections(); i++ ) {
		ng::DynamicArray< Cell >  path( 16 );
		RoadNetwork::Connection * connection = start.GetValidConnectionWithOffset( i );
		CardinalDirection         direction = start.GetDirectionOfConnection( connection );

		Cell              currentCell = GetCellAfterMovement( start.position, direction );
		Cell              previousCell = start.position;
		CardinalDirection previousDirection = direction;
		u32               currentPathDistance = 1;
		while ( true ) {
			if ( currentCell == goal ) {
				path.PushBack( goal );
				if ( insertReverse == true ) {
					for ( int64 i = path.Size() - 1; i >= 0; i-- ) {
						outPath.PushBack( path[ ( u32 )i ] );
					}
				} else {
					outPath.Append( path );
				}
				outTotalDistance = currentPathDistance;
				return true;
			}
			if ( currentCell == connection->connectedTo ) {
				break;
			}

			ng::StaticArray< Cell, 4 > neighbors;
			GetWalkableNeighborsOfCell( currentCell, map, neighbors );
			bool nextRoadIsFound = false;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousCell ) {
					nextRoadIsFound = true;
					CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell, neighbor );
					if ( currentDirection != previousDirection ) {
						path.PushBack( currentCell );
					}
					previousDirection = currentDirection;
					previousCell = currentCell;
					currentCell = neighbor;
					currentPathDistance++;
					break;
				}
			}
			ng_assert( nextRoadIsFound == true );
		}
	}
	return false;
}

bool CreateWandererRoutine(
    const Cell & start, Map & map, RoadNetwork & roadNetwork, ng::DynamicArray< Cell > & outPath, u32 maxDistance ) {
	// From the start cell and then at every intersection, we look for tiles around clockwise
	// If it's accessible, we go there.
	// If we reached maximum distance, we take the shortest path to home

	u32                      currentDistanceWalked = 0;
	ng::DynamicArray< Cell > exploredCells;

	std::function< bool( Cell, Cell ) > ExploreCell = [ & ]( Cell cell, Cell parent ) -> bool {
		if ( currentDistanceWalked >= maxDistance ) {
			return false;
		}
		exploredCells.PushBack( cell );
		ng::StaticArray< Cell, 4 > neighbors;
		GetNeighborsOfCell( cell, map, neighbors );

		outPath.PushBack( cell );
		currentDistanceWalked++;

		for ( const Cell & neighbor : neighbors ) {
			if ( map.GetTile( neighbor ) == MapTile::ROAD && exploredCells.FindIndexByValue( neighbor ) == -1 ) {
				bool ok = ExploreCell( neighbor, cell );
				if ( !ok ) {
					return false;
				}
			}
		}

		if ( parent != INVALID_CELL && currentDistanceWalked < maxDistance ) {
			outPath.PushBack( parent );
			currentDistanceWalked++;
			return true;
		}
		return false;
	};

	ExploreCell( start, INVALID_CELL );
	ng::ReverseArrayInplace( outPath );
	return true;
}

bool RoadNetwork::FindPath( Cell                       start,
                            Cell                       goal,
                            const Map &                map,
                            ng::DynamicArray< Cell > & outPath,
                            u32 *                      outTotalDistance /*= nullptr*/,
                            u32                        maxDistance /*= ULONG_MAX */ ) {
	ZoneScoped;

	thread_local ng::DynamicArray< AStarStep * > findPathOpenSet( 64 );
	thread_local ng::DynamicArray< AStarStep * > findPathClosedSet( 64 );

	findPathOpenSet.Clear();
	findPathClosedSet.Clear();

	u32 totalDistance = 0;

	outPath.Clear();
	outPath.Reserve( 64 );

	if ( !map.IsTileWalkable( start ) || !map.IsTileWalkable( goal ) ) {
		return false;
	}

	auto pushOrUpdateStep = [ & ]( const Cell & position, Node * node, int totalCost, AStarStep * parent ) {
		ZoneScopedN( "pushOrUpdateStep" );
		AStarStep * step = FindNodeInSet( findPathOpenSet, position );
		if ( step == nullptr ) {
			step = aStarStepPool.Pop();
			step->coord = position;
			step->node = node;
			step->parent = parent;
			step->g = totalCost;
			step->h = Heuristic( position, goal, ASTAR_FORBID_DIAGONALS );
			step->f = step->g + step->h;
			findPathOpenSet.PushBack( step );
		} else if ( step->g > totalCost ) {
			step->parent = parent;
			step->g = totalCost;
			step->f = step->g + step->h;
		}
	};

	NodeSearchResult searchGoalA;
	NodeSearchResult searchGoalB;
	NodeSearchResult searchStartA;
	NodeSearchResult searchStartB;

	FindNearestRoadNodes( goal, map, searchGoalA, searchGoalB );
	ng_assert( searchGoalA.found == true );

	FindNearestRoadNodes( start, map, searchStartA, searchStartB );
	ng_assert( searchStartA.found == true );

	if ( ( searchStartA.node == searchGoalA.node && searchStartB.node == searchGoalB.node ) ||
	     ( searchStartA.node == searchGoalB.node && searchStartB.node == searchGoalA.node ) ) {
		// Start and goal are between the same nodes
		outPath.PushBack( goal );
		BuildPathInsideNodes( goal, start, *searchStartA.node, *searchStartB.node, map, outPath, totalDistance );
		if ( outTotalDistance != nullptr ) {
			*outTotalDistance = totalDistance;
		}
		return true;
	}
	if ( Node * startNode = FindNodeWithPosition( start );
	     startNode != nullptr && ( searchGoalA.node == startNode || searchGoalB.node == startNode ) ) {
		BuildPathFromNodeToCell( *startNode, goal, map, outPath, totalDistance, true );
		outPath.PushBack( start );
		if ( outTotalDistance != nullptr ) {
			*outTotalDistance = totalDistance;
		}
		return true;
	}
	if ( Node * goalNode = FindNodeWithPosition( goal );
	     goalNode != nullptr && ( searchStartA.node == goalNode || searchStartB.node == goalNode ) ) {
		outPath.PushBack( goal );
		BuildPathFromNodeToCell( *goalNode, start, map, outPath, totalDistance );
		if ( outTotalDistance != nullptr ) {
			*outTotalDistance = totalDistance;
		}
		return true;
	}

	pushOrUpdateStep( searchStartA.node->position, searchStartA.node, searchStartA.distance, nullptr );
	if ( searchStartB.found == true ) {
		pushOrUpdateStep( searchStartB.node->position, searchStartB.node, searchStartB.distance, nullptr );
	}

	bool        goalFound = false;
	AStarStep * lastStep = nullptr;
	while ( findPathOpenSet.Empty() == false ) {
		ZoneScopedN( "FindSubPath" );

		u32 bestCandidateIndex = 0;
		for ( u32 i = 0; i < findPathOpenSet.Size(); i++ ) {
			if ( findPathOpenSet[ i ]->f < findPathOpenSet[ bestCandidateIndex ]->f ) {
				bestCandidateIndex = i;
			}
		}

		AStarStep * current = findPathOpenSet[ bestCandidateIndex ];

		if ( current->coord == goal ) {
			lastStep = current;
			goalFound = true;
			break;
		}

		AStarStep * parent = findPathClosedSet.PushBack( current );
		findPathOpenSet.DeleteIndexFast( bestCandidateIndex );

		Node * node = current->node;
		ng_assert( node != nullptr );
		if ( current->coord == searchGoalA.node->position ) {
			int totalCost = current->g + searchGoalA.distance;
			pushOrUpdateStep( goal, nullptr, totalCost, parent );
		} else if ( searchGoalB.found == true && current->coord == searchGoalB.node->position ) {
			int totalCost = current->g + searchGoalB.distance;
			pushOrUpdateStep( goal, nullptr, totalCost, parent );
		} else {
			for ( u32 i = 0; i < node->NumSetConnections(); i++ ) {
				Connection * connection = node->GetValidConnectionWithOffset( i );
				int          totalCost = current->g + connection->distance;
				if ( ( u32 )totalCost < maxDistance &&
				     FindNodeInSet( findPathClosedSet, connection->connectedTo ) == nullptr ) {
					pushOrUpdateStep( connection->connectedTo, ResolveConnection( connection ), totalCost, parent );
				}
			}
		}
	}
	if ( goalFound == false || lastStep == nullptr || lastStep->coord != goal ) {
		return false;
	}

	BuildPathFromNodeToCell( *lastStep->parent->node, lastStep->coord, map, outPath, totalDistance, true );

	auto cursor = lastStep->parent;
	while ( cursor != nullptr ) {
		u32 stepDistance = 0;
		if ( outPath.Last() != cursor->coord ) {
			outPath.PushBack( cursor->coord );
		}
		if ( cursor->parent != nullptr ) {
			BuildPathBetweenNodes( *cursor->node, *cursor->parent->node, map, outPath, stepDistance );
		} else {
			BuildPathFromNodeToCell( *cursor->node, start, map, outPath, stepDistance );
		}
		totalDistance += stepDistance;
		cursor = cursor->parent;
	}

	if ( outTotalDistance != nullptr ) {
		*outTotalDistance = totalDistance;
	}

	for ( AStarStep * step : findPathOpenSet ) {
		aStarStepPool.Push( step );
	}
	for ( AStarStep * step : findPathClosedSet ) {
		aStarStepPool.Push( step );
	}
	findPathClosedSet.Clear();
	findPathOpenSet.Clear();

	return true;
}

Cell GetAnyRoadConnectedToBuilding( const CpntBuilding & building, const Map & map ) {
	// TODO: This should pick first road in clockwise order
	for ( u32 x = building.cell.x; x <= building.cell.x + building.tileSizeX && x < map.sizeX; x++ ) {
		if ( building.cell.z > 0 && map.GetTile( x, building.cell.z - 1 ) == MapTile::ROAD ) {
			return Cell( x, building.cell.z - 1 );
		}
		if ( building.cell.z + building.tileSizeZ < map.sizeZ &&
		     map.GetTile( x, building.cell.z + building.tileSizeZ ) == MapTile::ROAD ) {
			return Cell( x, building.cell.z + building.tileSizeZ );
		}
	}
	for ( u32 z = building.cell.z; z <= building.cell.z + building.tileSizeZ && z < map.sizeZ; z++ ) {
		if ( building.cell.x > 0 && map.GetTile( building.cell.x - 1, z ) == MapTile::ROAD ) {
			return Cell( building.cell.x - 1, z );
		}
		if ( building.cell.x + building.tileSizeX < map.sizeX &&
		     map.GetTile( building.cell.x + building.tileSizeX, z ) == MapTile::ROAD ) {
			return Cell( building.cell.x + building.tileSizeX, z );
		}
	}
	return INVALID_CELL;
}

bool FindPathBetweenBuildings( const CpntBuilding &       start,
                               const CpntBuilding &       goal,
                               Map &                      map,
                               RoadNetwork &              roadNetwork,
                               ng::DynamicArray< Cell > & outPath,
                               u32                        maxDistance /*= ULONG_MAX*/,
                               u32 *                      outDistance /*= nullptr */ ) {
	ZoneScoped;

	thread_local ng::DynamicArray< Cell > startingCells( 16 );
	thread_local ng::DynamicArray< Cell > goalCells( 16 );

	startingCells.Clear();
	goalCells.Clear();

	// This is a weird and unneficient way to figure out what unique roads are connected to a building
	// It's not perfect, if B is Building and X road :
	// XXXXXXX
	//   BB
	//   BB
	// XXXXXXX
	// This works, we only consider two cells
	// But for this :
	//    XXX
	// XXXX XXX
	//   BBBBB
	//   BBBBB
	// XXXXXXXXX
	// We think there are two different roads connected to the top of the building, for a total of 3

	bool addNextCellToList = true;
	for ( Cell cell : start.AdjacentCells( map ) ) {
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			if ( addNextCellToList ) {
				startingCells.PushBack( cell );
				addNextCellToList = false;
			}
		} else {
			addNextCellToList = true;
		}
	}
	addNextCellToList = true;
	for ( Cell cell : goal.AdjacentCells( map ) ) {
		if ( map.GetTile( cell ) == MapTile::ROAD ) {
			if ( addNextCellToList ) {
				goalCells.PushBack( cell );
				addNextCellToList = false;
			}
		} else {
			addNextCellToList = true;
		}
	}

	bool pathFound = false;
	u32  shortestDistance = maxDistance;
	ng::DynamicArray< Cell > path(32);
	for ( u32 i = 0; i < startingCells.Size(); i++ ) {
		for ( u32 j = 0; j < goalCells.Size(); j++ ) {
			path.Clear();
			u32                      distance = 0;
			bool                     subPathFound =
			    roadNetwork.FindPath( startingCells[ i ], goalCells[ j ], map, path, &distance, shortestDistance );
			pathFound |= subPathFound;
			if ( subPathFound && distance < shortestDistance ) {
				outPath = path;
				shortestDistance = distance;
			}
		}
	}
	if ( outDistance != nullptr ) {
		*outDistance = shortestDistance;
	}
	return pathFound;
}

bool FindPathFromCellToBuilding( Cell                       start,
                                 const CpntBuilding &       goal,
                                 Map &                      map,
                                 RoadNetwork &              roadNetwork,
                                 ng::DynamicArray< Cell > & outPath,
                                 u32                        maxDistance /*= ULONG_MAX*/,
                                 u32 *                      outDistance /*= nullptr */ ) {
	ZoneScoped;

	thread_local ng::DynamicArray< Cell > goalCells( 16 );

	goalCells.Clear();

	bool addNextCellToList = true;

	auto lookForRoadConnectedToBuilding = [ & ]( const CpntBuilding & building, int64 shiftX, int64 shiftZ,
	                                             ng::DynamicArray< Cell > & res ) {
		int64 x = building.cell.x + shiftX;
		int64 z = building.cell.z + shiftZ;
		if ( x >= 0 && x < map.sizeX && z >= 0 && z < map.sizeZ ) {
			if ( map.GetTile( ( u32 )x, ( u32 )z ) == MapTile::ROAD ) {
				if ( addNextCellToList ) {
					res.PushBack( Cell( ( u32 )x, ( u32 )z ) );
					addNextCellToList = false;
				}
			} else {
				addNextCellToList = true;
			}
		}
	};

	addNextCellToList = true;
	for ( int64 z = 0; z < goal.tileSizeZ; z++ ) {
		lookForRoadConnectedToBuilding( goal, -1, z, goalCells );
	}
	for ( int64 x = 0; x < goal.tileSizeX; x++ ) {
		lookForRoadConnectedToBuilding( goal, x, goal.tileSizeZ, goalCells );
	}
	for ( int64 z = goal.tileSizeZ - 1; z >= 0; z-- ) {
		lookForRoadConnectedToBuilding( goal, goal.tileSizeX, z, goalCells );
	}
	for ( int64 x = goal.tileSizeX - 1; x >= 0; x-- ) {
		lookForRoadConnectedToBuilding( goal, x, -1, goalCells );
	}

	bool pathFound = false;
	u32  shortestDistance = maxDistance;
	for ( u32 i = 0; i < goalCells.Size(); i++ ) {
		ng::DynamicArray< Cell > path;
		u32                      distance = 0;
		bool subPathFound = roadNetwork.FindPath( start, goalCells[ i ], map, path, &distance, shortestDistance );
		pathFound |= subPathFound;
		if ( subPathFound && distance < shortestDistance ) {
			outPath = path;
			shortestDistance = distance;
		}
	}
	if ( outDistance != nullptr ) {
		*outDistance = shortestDistance;
	}
	return pathFound;
}

bool RoadNetwork::CheckNetworkIntegrity() {
	// This is just a debug utility to find weird data inside road network
	bool ok = true;
	for ( Node & node : nodes ) {
		for ( u32 i = 0; i < node.NumSetConnections(); i++ ) {
			Connection * connection = node.GetValidConnectionWithOffset( i );
			Node *       connectedTo = ResolveConnection( connection );
			if ( connectedTo == nullptr ) {
				ok = false;
				ng::Errorf( "Node [%lu, %lu] is connected to a node that does not exists on [%lu, %lu]\n",
				            node.position.x, node.position.z, connection->connectedTo.x, connection->connectedTo.z );
			} else if ( connection->connectedTo == node.position ) {
				bool isSelfConnectionValid = false;
				for ( u32 j = 0; j < node.NumSetConnections(); j++ ) {
					if ( j != i && node.GetValidConnectionWithOffset( j )->connectedTo == node.position ) {
						isSelfConnectionValid = true;
					}
				}
				if ( !isSelfConnectionValid ) {
					ok = false;
					ng::Errorf( "Node [%lu, %lu] is connected to itself, but only once\n", node.position.x,
					            node.position.z );
				}
			} else if ( node.HasMultipleConnectionsWith( connection->connectedTo ) ) {
				// if we have multiple connections, let's check that each of them is reciprocal
			} else if ( connectedTo->FindConnectionWith( node.position ) == nullptr ) {
				ok = false;
				ng::Errorf( "Node [%lu, %lu] has a non mutual connection with [%lu, %lu]\n", node.position.x,
				            node.position.z, connectedTo->position.x, connectedTo->position.z );
			}
		}
	}
	return ok;
}

RoadNetwork::Connection * RoadNetwork::Node::GetValidConnectionWithOffset( u32 offset ) {
	ng_assert( offset < NumSetConnections() );
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].IsValid() ) {
			if ( offset == 0 ) {
				return connections + i;
			}
			offset--;
		}
	}
	return nullptr;
}

u32 RoadNetwork::Node::NumSetConnections() const {
	u32 count = 0;
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].IsValid() ) {
			count++;
		}
	}
	return count;
}

RoadNetwork::Connection * RoadNetwork::Node::FindConnectionWith( const Cell & cell ) {
	ng_assert_msg( HasMultipleConnectionsWith( cell ) == false,
	               "FindConnectionWith will only return the first connection, which is unsafe if a node has "
	               "multiple connections with the same node" );
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].connectedTo == cell ) {
			return &connections[ i ];
		}
	}
	return nullptr;
}

RoadNetwork::Connection * RoadNetwork::Node::FindShortestConnectionWith( const Cell & cell ) {
	Connection * res = nullptr;
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].connectedTo == cell ) {
			if ( res == nullptr ) {
				res = connections + i;
			} else if ( connections[ i ].distance < res->distance ) {
				res = connections + i;
			}
		}
	}
	return res;
}

bool RoadNetwork::Node::HasMultipleConnectionsWith( const Cell & cell ) {
	u32 numConnections = 0;
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].connectedTo == cell ) {
			numConnections++;
		}
	}
	return numConnections > 1;
}

bool RoadNetwork::Node::IsConnectedToItself() const {
	for ( size_t i = 0; i < 4; i++ ) {
		if ( connections[ i ].IsValid() && connections[ i ].connectedTo == position ) {
			return true;
		}
	}
	return false;
}

CardinalDirection RoadNetwork::Node::GetDirectionOfConnection( const Connection * connection ) const {
	int offset = ( int )( connection - connections );
	ng_assert( offset >= 0 && offset < 4 );
	return ( CardinalDirection )offset;
}
