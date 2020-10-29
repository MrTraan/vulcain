#pragma once
#include "ngLib/ngcontainers.h"
#include "ngLib/nglib.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <typeindex>
#include <typeinfo>

struct Registery;

using CpntTypeHash = u64;
template < typename T > constexpr CpntTypeHash HashComponent() { return typeid( T ).hash_code(); }

struct Entity {
	u32 id;
	u32 version;

	bool operator==( const Entity & rhs ) const { return id == rhs.id && version == rhs.version; }
};
constexpr u32    INVALID_ENTITY_INDEX = ( u32 )-1;
constexpr Entity INVALID_ENTITY = { ( u32 )-1, 0 };

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
