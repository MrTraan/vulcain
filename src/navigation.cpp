#include "navigation.h"
#include "ngLib/logs.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include "ngLib/types.h"
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

Cell GetCellForPoint( glm::vec3 point ) {
	ng_assert( point.x >= 0.0f && point.z >= 0.0f );
	auto cell = Cell( ( u32 )point.x, ( u32 )point.z );
	return cell;
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
			}
			remainingSpeed -= distance;
		}
	}
}
