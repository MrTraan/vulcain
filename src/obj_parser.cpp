#include <map>
#include <string>

#include "game.h"
#include "ngLib/nglib.h"
#include "obj_parser.h"

static inline bool isWhiteSpace( char c ) { return c == ' ' || c == '\t'; }
static inline bool isDigit( char c ) { return c >= '0' && c <= '9'; }

static u64 EatVector2( const char * data, glm::vec2 & out ) {
	u64    offset = 0;
	char * pEnd;
	out.x = strtof( data + offset, &pEnd );
	offset = pEnd - data;
	out.y = strtof( data + offset, &pEnd );
	offset = pEnd - data;
	return offset;
}

static u64 EatVector3( const char * data, glm::vec3 & out ) {
	u64    offset = 0;
	char * pEnd;
	out.x = strtof( data + offset, &pEnd );
	offset = pEnd - data;
	out.y = strtof( data + offset, &pEnd );
	offset = pEnd - data;
	out.z = strtof( data + offset, &pEnd );
	offset = pEnd - data;
	return offset;
}

void ParseMaterialFile( const char * data, u64 size, std::map< std::string, Material > & out ) {
	u64 i = 0;

	Material * currentMaterial = nullptr;

	while ( i < size ) {
		while ( isWhiteSpace( data[ i ] ) ) {
			i++;
		}
		if ( data[ i ] == '\n' ) {
			// skip empty line
			i++;
			continue;
		}

		if ( strncmp( data + i, "newmtl ", strlen( "newmtl" ) ) == 0 ) {
			i += strlen( "newmtl " );
			u64 materialNameLength = 0;
			while ( data[ i + materialNameLength ] != '\n' ) {
				materialNameLength++;
			}
			std::string materialName( data + i, materialNameLength );
			currentMaterial = &out[ materialName ];
			currentMaterial->diffuseTexture = Texture::DefaultWhiteTexture();
		} else if ( strncmp( data + i, "map_Kd ", strlen( "map_Kd" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "map_Kd " );
			u64 textureNameLength = 0;
			while ( data[ i + textureNameLength ] != '\n' ) {
				textureNameLength++;
			}
			std::string textureName( data + i, textureNameLength );
			auto        textureResource = theGame->package.GrabResourceByName( textureName.c_str() );
			ng_assert( textureResource != nullptr );
			if ( textureResource != nullptr ) {
				currentMaterial->diffuseTexture = CreateTextureFromResource( *textureResource );
			} else {
				currentMaterial->diffuseTexture = CreatePlaceholderPinkTexture();
			}
		} else if ( strncmp( data + i, "map_d ", strlen( "map_d" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "map_d " );
			// TODO: Could this be a different texture than map_Kd?
			currentMaterial->mode = Material::MODE_TRANSPARENT;
		} else if ( strncmp( data + i, "Ka ", strlen( "Ka" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "Ka " );
			i += EatVector3( data + i, currentMaterial->ambiant );
		} else if ( strncmp( data + i, "Kd ", strlen( "Kd" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "Kd " );
			i += EatVector3( data + i, currentMaterial->diffuse );
			// currentMaterial->ambiant = currentMaterial->diffuse;
		} else if ( strncmp( data + i, "Ks ", strlen( "Ks" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "Ks " );
			i += EatVector3( data + i, currentMaterial->specular );
		} else if ( strncmp( data + i, "Ns ", strlen( "Ns" ) ) == 0 ) {
			ng_assert( currentMaterial != nullptr );
			i += strlen( "Ns " );
			currentMaterial->shininess = strtof( data + i, nullptr );
		}

		// Go to end of line
		while ( data[ i ] != '\n' ) {
			i++;
		}
		i++;
	}
}

bool ImportObjFile( PackerResourceID resourceID, Model & out ) {
	ng::ScopedChrono chrono( "Import obj file" );
	auto             objResource = theGame->package.GrabResource( resourceID );
	ng_assert( objResource->type == PackerResource::Type::OBJ );
	const char * data = ( const char * )theGame->package.GrabResourceData( *objResource );

	out.materials.clear();
	out.meshes.clear();

	Mesh * currentMesh = nullptr;

	std::vector< glm::vec3 > positions;
	std::vector< glm::vec3 > normals;
	std::vector< glm::vec2 > textureCoords;
	constexpr u64            initialAlloc = 64;
	positions.reserve( initialAlloc );
	normals.reserve( initialAlloc );
	textureCoords.reserve( initialAlloc );

	u64 i = 0;
	while ( i < objResource->size ) {
		while ( isWhiteSpace( data[ i ] ) ) {
			i++;
		}
		if ( data[ i ] == '\n' ) {
			// skip empty line
			i++;
			continue;
		}
		if ( data[ i ] == '#' ) {
			// Skip line comment
			while ( data[ i ] != '\n' ) {
				i++;
			}
			i++;
		} else {
			// Parse line
			switch ( data[ i ] ) {
			case 'm': {
				ng_assert( strncmp( data + i, "mtllib ", strlen( "mtllib" ) ) == 0 );
				i += strlen( "mtllib " );
				int fileNameLength = 0;
				while ( data[ i + fileNameLength ] != '\n' ) {
					fileNameLength++;
				}
				std::string fileName( data + i, fileNameLength );

				auto materialResource = theGame->package.GrabResourceByName( fileName.c_str() );
				ng_assert( materialResource != nullptr );
				ng_assert( materialResource->type == PackerResource::Type::MATERIAL );
				ParseMaterialFile( ( const char * )theGame->package.GrabResourceData( *materialResource ),
				                   materialResource->size, out.materials );

				break;
			}
			case 'u': {
				ng_assert( strncmp( data + i, "usemtl ", strlen( "usemtl " ) ) == 0 );
				i += strlen( "usemtl " );
				int materialNameLength = 0;
				while ( data[ i + materialNameLength ] != '\n' ) {
					materialNameLength++;
				}
				std::string materialName( data + i, materialNameLength );
				ng_assert( out.materials.contains( materialName ) );
				out.meshes.resize( out.meshes.size() + 1 );
				currentMesh = &out.meshes.at( out.meshes.size() - 1 );
				currentMesh->material = &out.materials[ materialName ];
				break;
			}
			case 'o':
				// object name
				break;
			case 'g':
				// object group
				break;
			case 's':
				// smooth shading
				break;
			case 'v': {
				switch ( data[ i + 1 ] ) {
				case ' ': {
					// vertex
					i++;
					glm::vec3 vertex;
					i += EatVector3( data + i, vertex );
					if ( positions.size() == positions.capacity() ) {
						positions.reserve( positions.size() * 2 );
					}
					positions.push_back( vertex );
					break;
				}
				case 't': {
					// texture
					i += 2;
					glm::vec2 texture;
					i += EatVector2( data + i, texture );
					// flip vertically
					texture.y = 1.0f - texture.y;
					if ( textureCoords.size() == textureCoords.capacity() ) {
						textureCoords.reserve( textureCoords.size() * 2 );
					}
					textureCoords.push_back( texture );
					break;
				}
				case 'n': {
					// normal
					i += 2;
					glm::vec3 normal;
					i += EatVector3( data + i, normal );
					if ( normals.size() == normals.capacity() ) {
						normals.reserve( normals.size() * 2 );
					}
					normals.push_back( normal );
					break;
				}
				default:
					ng_assert( false );
				}
				break;
			}
			case 'f': {
				// face
				const char * line = data + i + 2;
				u32          currentIndex = ( u32 )currentMesh->vertices.size();

				for ( int face = 0; face < 3; face++ ) {
					glm::vec3 position;
					glm::vec3 normal( 0.0f, 0.0f, 0.0f );
					glm::vec2 texCoord( 0.0f, 0.0f );

					char * endPtr;
					int    position1Idx = strtol( line, &endPtr, 10 );
					position = positions[ position1Idx - 1 ];
					line = endPtr;
					if ( *line == '/' ) {
						int texture1Idx = strtol( line + 1, &endPtr, 10 );
						texCoord = textureCoords[ texture1Idx - 1 ];
						line = endPtr;
						if ( *line == '/' ) {
							int normal1Idx = strtol( line + 1, &endPtr, 10 );
							normal = normals[ normal1Idx - 1 ];
							line = endPtr;
						}
					}

					Vertex vertex;
					vertex.position = position;
					vertex.normal = normal;
					vertex.texCoords = texCoord;
					currentMesh->vertices.push_back( vertex );
				}
				currentMesh->indices.push_back( currentIndex + 0 );
				currentMesh->indices.push_back( currentIndex + 1 );
				currentMesh->indices.push_back( currentIndex + 2 );
				if ( line[ 0 ] == ' ' && isDigit( line[ 1 ] ) ) {
					// Square definition, split into 2  triangles
					glm::vec3 position;
					glm::vec3 normal;
					glm::vec2 texCoord;

					char * endPtr;
					int    positionIdx = strtol( line, &endPtr, 10 );
					position = positions[ positionIdx - 1 ];
					line = endPtr;
					if ( *line == '/' ) {
						int textureIdx = strtol( line + 1, &endPtr, 10 );
						texCoord = textureCoords[ textureIdx - 1 ];
						line = endPtr;
						if ( *line == '/' ) {
							int normalIdx = strtol( line + 1, &endPtr, 10 );
							normal = normals[ normalIdx - 1 ];
							line = endPtr;
						}
					}

					Vertex vertex;
					vertex.position = position;
					vertex.normal = normal;
					vertex.texCoords = texCoord;
					currentMesh->vertices.push_back( vertex );
					currentMesh->indices.push_back( currentIndex + 0 );
					currentMesh->indices.push_back( currentIndex + 2 );
					currentMesh->indices.push_back( currentIndex + 3 );
				}

				break;
			}
			default:
				ng_assert( false );
			}
		}
		// Go to end of line
		while ( data[ i ] != '\n' ) {
			i++;
		}
		i++;
	}

	ComputeModelSize( out );

	return true;
}
