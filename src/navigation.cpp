#include "navigation.h"
#include "message.h"
#include "ngLib/logs.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "ngLib/types.h"
#include "registery.h"
#include <array>
#include <vector>

struct AStarStep {
	Cell                coord;
	RoadNetwork::Node * node = nullptr;
	AStarStep *         parent = nullptr;
	int                 f, g, h;

	bool operator<( const AStarStep & rhs ) const { return f < rhs.f; }
};

static AStarStep * FindNodeInSet( std::vector< AStarStep * > & set, Cell coords ) {
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

static thread_local ng::ObjectPool< AStarStep > aStarStepPool;

bool AStar( Cell start, Cell goal, AStarMovementAllowed movement, const Map & map, std::vector< Cell > & outPath ) {
	ng::ScopedChrono chrono( "A Star" );
	constexpr int    infiniteWeight = INT_MAX;

	std::vector< AStarStep * > openSet;
	std::vector< AStarStep * > closedSet;

	AStarStep * startStep = aStarStepPool.Pop();
	startStep->coord = start;
	startStep->h = Heuristic( start, goal, movement );
	startStep->g = 1;
	startStep->f = startStep->h + startStep->g;
	openSet.push_back( startStep );

	AStarStep * current = nullptr;
	while ( openSet.empty() == false ) {
		auto currentIt = openSet.begin();
		current = *currentIt;

		for ( auto it = openSet.begin(); it != openSet.end(); it++ ) {
			AStarStep * step = *it;
			if ( step->f <= current->f ) {
				current = step;
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

				AStarStep * neighbor = FindNodeInSet( openSet, neighborCoords );
				if ( neighbor == nullptr ) {
					neighbor = aStarStepPool.Pop();
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

	for ( AStarStep * step : openSet ) {
		aStarStepPool.Push( step );
	}
	for ( AStarStep * step : closedSet ) {
		aStarStepPool.Push( step );
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

static void GetNeighborsOfCell( Cell base, const Map & map, ng::StaticArray< Cell, 4 > & neighbors ) {
	if ( base.x > 0 )
		neighbors.PushBack( GetCellAfterMovement( base, -1, 0 ) );
	if ( base.x < map.sizeX - 1 )
		neighbors.PushBack( GetCellAfterMovement( base, 1, 0 ) );
	if ( base.z > 0 )
		neighbors.PushBack( GetCellAfterMovement( base, 0, -1 ) );
	if ( base.z < map.sizeZ - 1 )
		neighbors.PushBack( GetCellAfterMovement( base, 0, 1 ) );
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
	GetRoadNeighborsOfCell( cellToAdd, map, roadNeighbors );

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
	GetRoadNeighborsOfCell( cellToRemove, map, roadNeighbors );
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

	ng_assert( map.GetTile( cell ) == MapTile::ROAD );
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
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
	GetRoadNeighborsOfCell( cell, map, roadNeighbors );
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
			GetRoadNeighborsOfCell( roadNeighbors[ i ], map, neighbors );
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
bool BuildPathInsideNodes( Cell                      start,
                           Cell                      goal,
                           const RoadNetwork::Node & nodeA,
                           const RoadNetwork::Node & nodeB,
                           const Map &               map,
                           std::vector< Cell > &     outPath ) {
	if ( start == goal ) {
		outPath.push_back( start );
		return true;
	}

	ng::StaticArray< Cell, 4 > roadNeighbors;
	GetRoadNeighborsOfCell( start, map, roadNeighbors );
	ng_assert( roadNeighbors.Size() == 2 );
	Cell                currentCell[ 2 ] = { roadNeighbors[ 0 ], roadNeighbors[ 1 ] };
	Cell                previousCell[ 2 ] = { start, start };
	CardinalDirection   previousDirection[ 2 ] = { GetDirectionFromCellTo( start, roadNeighbors[ 0 ] ),
                                                 GetDirectionFromCellTo( start, roadNeighbors[ 1 ] ) };
	std::vector< Cell > paths[ 2 ];
	int                 skipIndex0 = 0;
	int                 skipIndex1 = 0;
	while ( skipIndex0 == 0 || skipIndex1 == 0 ) {
		for ( int i = skipIndex0; i < 2 - skipIndex1; i++ ) {
			if ( currentCell[ i ] == goal ) {
				paths[ i ].push_back( goal );
				outPath.insert( outPath.end(), paths[ i ].begin(), paths[ i ].end() );
				return true;
			}

			ng::StaticArray< Cell, 4 > neighbors;
			GetRoadNeighborsOfCell( currentCell[ i ], map, neighbors );
			bool nextRoadIsFound = false;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousCell[ i ] ) {
					nextRoadIsFound = true;
					CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell[ i ], neighbor );
					if ( currentDirection != previousDirection[ i ] ) {
						paths[ i ].push_back( currentCell[ i ] );
					}
					previousDirection[ i ] = currentDirection;
					previousCell[ i ] = currentCell[ i ];
					currentCell[ i ] = neighbor;
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

bool BuildPathBetweenNodes( RoadNetwork::Node &   start,
                            RoadNetwork::Node &   goal,
                            const Map &           map,
                            std::vector< Cell > & outPath ) {
	RoadNetwork::Connection * connection = start.FindShortestConnectionWith( goal.position );
	CardinalDirection         direction = start.GetDirectionOfConnection( connection );

	Cell              currentCell = GetCellAfterMovement( start.position, direction );
	Cell              previousCell = start.position;
	CardinalDirection previousDirection = direction;
	while ( true ) {
		if ( currentCell == goal.position ) {
			outPath.push_back( goal.position );
			return true;
		}

		ng::StaticArray< Cell, 4 > neighbors;
		GetRoadNeighborsOfCell( currentCell, map, neighbors );
		bool nextRoadIsFound = false;
		for ( const Cell & neighbor : neighbors ) {
			if ( neighbor != previousCell ) {
				nextRoadIsFound = true;
				CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell, neighbor );
				if ( currentDirection != previousDirection ) {
					outPath.push_back( currentCell );
				}
				previousDirection = currentDirection;
				previousCell = currentCell;
				currentCell = neighbor;
				break;
			}
		}
		ng_assert( nextRoadIsFound == true );
	}
	return false;
}

bool BuildPathFromNodeToCell( RoadNetwork::Node &   start,
                              const Cell &          goal,
                              const Map &           map,
                              std::vector< Cell > & outPath ) {
	for ( u32 i = 0; i < start.NumSetConnections(); i++ ) {
		std::vector< Cell >       path;
		RoadNetwork::Connection * connection = start.GetValidConnectionWithOffset( i );
		CardinalDirection         direction = start.GetDirectionOfConnection( connection );

		Cell              currentCell = GetCellAfterMovement( start.position, direction );
		Cell              previousCell = start.position;
		CardinalDirection previousDirection = direction;
		while ( true ) {
			if ( currentCell == goal ) {
				path.push_back( goal );
				outPath.insert( outPath.end(), path.begin(), path.end() );
				return true;
			}
			if ( currentCell == connection->connectedTo ) {
				break;
			}

			ng::StaticArray< Cell, 4 > neighbors;
			GetRoadNeighborsOfCell( currentCell, map, neighbors );
			bool nextRoadIsFound = false;
			for ( const Cell & neighbor : neighbors ) {
				if ( neighbor != previousCell ) {
					nextRoadIsFound = true;
					CardinalDirection currentDirection = GetDirectionFromCellTo( currentCell, neighbor );
					if ( currentDirection != previousDirection ) {
						path.push_back( currentCell );
					}
					previousDirection = currentDirection;
					previousCell = currentCell;
					currentCell = neighbor;
					break;
				}
			}
			ng_assert( nextRoadIsFound == true );
		}
	}
	return false;
}

bool RoadNetwork::FindPath( Cell start, Cell goal, const Map & map, std::vector< Cell > & outPath ) {
	constexpr int    infiniteWeight = INT_MAX;
	ng::ScopedChrono chrono( "RoadNetwork::FindPath" );
	outPath.clear();

	if ( map.GetTile(start) != MapTile::ROAD || map.GetTile(goal) != MapTile::ROAD ) {
		return false;
	}

	std::vector< AStarStep * > openSet;
	std::vector< AStarStep * > closedSet;

	auto pushOrUpdateStep = [ & ]( const Cell & position, Node * node, int totalCost, AStarStep * parent ) {
		AStarStep * newStep = FindNodeInSet( openSet, position );
		if ( newStep == nullptr ) {
			newStep = aStarStepPool.Pop();
			newStep->coord = position;
			newStep->node = node;
			newStep->parent = parent;
			newStep->g = totalCost;
			newStep->h = Heuristic( position, goal, ASTAR_FORBID_DIAGONALS );
			newStep->f = newStep->g + newStep->h;
			openSet.push_back( newStep );
		} else if ( newStep->g > totalCost ) {
			newStep->parent = parent;
			newStep->g = totalCost;
			newStep->f = newStep->g + newStep->h;
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

	if ( (searchStartA.node == searchGoalA.node && searchStartB.node == searchGoalB.node ) ||
	(searchStartA.node == searchGoalB.node && searchStartB.node == searchGoalA.node )) {
		// Start and goal are between the same nodes
		outPath.push_back( goal );
		BuildPathInsideNodes( goal, start, *searchStartA.node, *searchStartB.node, map, outPath );
		return true;
	}
	if ( Node * startNode = FindNodeWithPosition( start );
	     startNode != nullptr && ( searchGoalA.node == startNode || searchGoalB.node == startNode ) ) {
		std::vector< Cell > reversePath;
		BuildPathFromNodeToCell( *startNode, goal, map, reversePath );
		for ( int i = reversePath.size() - 1; i >= 0; i-- ) {
			outPath.push_back( reversePath[ i ] );
		}
		outPath.push_back( start );
		return true;
	}
	if ( Node * goalNode = FindNodeWithPosition( goal );
	     goalNode != nullptr && ( searchStartA.node == goalNode || searchStartB.node == goalNode ) ) {
		outPath.push_back( goal );
		BuildPathFromNodeToCell( *goalNode, start, map, outPath );
		return true;
	}

	pushOrUpdateStep( searchStartA.node->position, searchStartA.node, searchStartA.distance, nullptr );
	if ( searchStartB.found == true ) {
		pushOrUpdateStep( searchStartB.node->position, searchStartB.node, searchStartB.distance, nullptr );
	}

	bool        goalFound = false;
	AStarStep * current = nullptr;
	while ( openSet.empty() == false ) {
		auto currentIt = openSet.begin();
		current = *currentIt;

		for ( auto it = openSet.begin(); it != openSet.end(); it++ ) {
			AStarStep * step = *it;
			if ( step->f <= current->f ) {
				current = step;
				currentIt = it;
			}
		}

		if ( current->coord == goal ) {
			goalFound = true;
			break;
		}

		closedSet.push_back( current );
		openSet.erase( currentIt );

		Node * node = current->node;
		ng_assert( node != nullptr );
		if ( current->coord == searchGoalA.node->position ) {
			int totalCost = current->g + searchGoalA.distance;
			pushOrUpdateStep( goal, nullptr, totalCost, current );
		} else if ( searchGoalB.found == true && current->coord == searchGoalB.node->position ) {
			int totalCost = current->g + searchGoalB.distance;
			pushOrUpdateStep( goal, nullptr, totalCost, current );
		} else {
			for ( u32 i = 0; i < node->NumSetConnections(); i++ ) {
				Connection * connection = node->GetValidConnectionWithOffset( i );
				int          totalCost = current->g + connection->distance;
				if ( FindNodeInSet( closedSet, connection->connectedTo ) == nullptr ) {
					pushOrUpdateStep( connection->connectedTo, ResolveConnection( connection ), totalCost, current );
				}
			}
		}
	}
	if ( goalFound == false ) {
		return false;
	}
	if ( current == nullptr ) {
		return false;
	}

	std::vector< Cell > reversePath;
	BuildPathFromNodeToCell( *current->parent->node, current->coord, map, reversePath );
	for ( int i = reversePath.size() - 1; i >= 0; i-- ) {
		outPath.push_back( reversePath[ i ] );
	}

	auto cursor = current->parent;
	while ( cursor != nullptr ) {
		if ( outPath[ outPath.size() - 1 ] != cursor->coord ) {
			outPath.push_back( cursor->coord );
		}
		if ( cursor->parent != nullptr ) {
			BuildPathBetweenNodes( *cursor->node, *cursor->parent->node, map, outPath );
		} else {
			BuildPathFromNodeToCell( *cursor->node, start, map, outPath );
		}
		cursor = cursor->parent;
	}

	for ( AStarStep * step : openSet ) {
		aStarStepPool.Push( step );
	}
	for ( AStarStep * step : closedSet ) {
		aStarStepPool.Push( step );
	}

	return current->coord == goal;
}

bool RoadNetwork::CheckNetworkIntegrity() {
	// Let's check if each link is mutual
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
