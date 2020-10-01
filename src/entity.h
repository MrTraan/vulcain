#pragma once
#include "ngLib/nglib.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

struct Registery;

typedef u32      Entity;
constexpr u32    INVALID_ENTITY_INDEX = ( u32 )-1;
constexpr Entity INVALID_ENTITY_ID = ( Entity )-1;

struct CpntTransform {
  public:
	CpntTransform() = default;
	CpntTransform( const glm::mat4 matrix ) { DecomposeMatrix( matrix ); }

	const glm::mat4 & GetMatrix() const { return matrix; }

	void Translate( const glm::vec3 & v ) {
		translation += v;
		ComputeMatrix();
	}

	void SetTranslation( const glm::vec3 & v ) {
		translation = v;
		ComputeMatrix();
	}

	void SetScale( const glm::vec3 & v ) {
		scale = v;
		ComputeMatrix();
	}

	void SetRotation( const glm::vec3 & v ) {
		rotation = glm::quat( glm::radians( v ) );
		ComputeMatrix();
	}

	glm::vec3 GetTranslation() const { return translation; }
	glm::vec3 GetScale() const { return scale; }

	void ComputeMatrix() {
		matrix = glm::translate( glm::mat4( 1.0f ), translation );
		matrix = matrix * glm::mat4_cast( rotation );
		matrix = glm::scale( matrix, scale );
	}

	void DecomposeMatrix( const glm::mat4 matrix ) {
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose( matrix, this->scale, this->rotation, this->translation, skew, perspective );
		this->matrix = matrix;
		( void )skew;
		( void )perspective;
		//rotation = glm::conjugate( rotation );
	}

	CpntTransform operator*( const CpntTransform & rhs ) const {
		glm::mat4 matrix = this->matrix * rhs.matrix;
		return CpntTransform( matrix );
	}

	glm::vec3 Front() const { return glm::normalize( glm::vec3( rotation * glm::vec4( 0.0f, 0.0f, 1.0f, 1.0f ) ) ); }
	glm::vec3 Right() const { return glm::normalize( glm::cross( Front(), glm::vec3( 0.0f, 1.0f, 0.0f ) ) ); }
	glm::vec3 Up() const { return glm::normalize( glm::cross( Right(), Front() ) ); }

  private:
	glm::mat4 matrix{ 1.0f };
	glm::vec3 translation{ 0.0f, 0.0f, 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
	glm::quat rotation{ 0.0f, 0.0f, 0.0f, 0.0f };
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

struct ISystem {
	virtual ~ISystem() {}
	virtual void Update( Registery & reg, float dt ) = 0;
	virtual void DebugDraw() {}
};

struct SystemManager {
	std::unordered_map< u64, ISystem * > systems;

	~SystemManager() {
		for ( auto [ type, system ] : systems ) {
			delete system;
		}
	}

	template < class T, class... Args > T & CreateSystem( Args &&... args ) {
		auto system = new T( std::forward< Args >( args )... );
		u64  typeIndex = std::type_index( typeid( T ) ).hash_code();
		systems[ typeIndex ] = system;
		return *system;
	}

	void Update( Registery & reg, float dt );
};
