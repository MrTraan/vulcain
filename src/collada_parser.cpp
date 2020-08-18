#include "collada_parser.h"
#include "game.h"

#include <map>
#include <string>
#include <tinyxml2.h>

bool ImportColladaFile( PackerResourceID resourceID, Model & outModel ) {
	ng::ScopedChrono chrono( "Import collada file" );
	auto             objResource = theGame->package.GrabResource( resourceID );
	ng_assert( objResource->type == PackerResource::Type::COLLADA );
	const char * data = ( const char * )theGame->package.GrabResourceData( *objResource );

	tinyxml2::XMLDocument doc;
	auto                  ret = doc.Parse( data, objResource->size );
	ng_assert( ret == tinyxml2::XML_SUCCESS );

	// We should do in order:
	// Parse textures
	// parse effects
	// parse materials

	auto geometryRoot = doc.FirstChildElement( "COLLADA" )->FirstChildElement( "library_geometries" );

	for ( auto geometryNode = geometryRoot->FirstChildElement( "geometry" ); geometryNode != nullptr;
	      geometryNode = geometryNode->NextSiblingElement( "geometry" ) ) {
		const char * id = geometryNode->Attribute( "id" );
		const char * name = geometryNode->Attribute( "name" );

		for ( auto meshNode = geometryNode->FirstChildElement( "mesh" ); meshNode != nullptr;
		      meshNode = meshNode->NextSiblingElement( "mesh" ) ) {
			struct FloatSourceData {
				std::vector< float > data;
				int                  stride;
				size_t               count;
			};

			std::map< std::string, FloatSourceData > sources;

			// Parse all sources
			for ( auto sourceNode = meshNode->FirstChildElement( "source" ); sourceNode != nullptr;
			      sourceNode = sourceNode->NextSiblingElement( "source" ) ) {
				auto floatArray = sourceNode->FirstChildElement( "float_array" );
				ng_assert( floatArray != nullptr );
				std::string       id = std::string( "#" ) + sourceNode->Attribute( "id" );
				int               numElements = atol( floatArray->Attribute( "count" ) );
				FloatSourceData & elements = sources[ id ];
				elements.data.reserve( numElements );
				const char * cursor = floatArray->GetText();
				char *       endPtr = nullptr;
				for ( int i = 0; i < numElements; i++ ) {
					float data = static_cast< float >( strtod( cursor, &endPtr ) );
					elements.data.push_back( data );
					ng_assert( *endPtr == '\0' || *endPtr == ' ' );
					ng_assert( cursor != endPtr );
					cursor = endPtr + 1;
				}
				auto techniqueNode = sourceNode->FirstChildElement( "technique_common" );
				ng_assert( techniqueNode != nullptr );
				auto accessorNode = techniqueNode->FirstChildElement( "accessor" );
				ng_assert( accessorNode != nullptr );
				elements.stride = strtol( accessorNode->Attribute( "stride" ), nullptr, 10 );
				elements.count = strtoull( accessorNode->Attribute( "count" ), nullptr, 10 );
			}

			// TODO: Do we care about the node <vertices>?

			auto trianglesNode = meshNode->FirstChildElement( "triangles" );
			ng_assert( trianglesNode != nullptr );

			int         numTriangles = strtol( trianglesNode->Attribute( "count" ), nullptr, 10 );
			std::string vertexSourceId;
			std::string normalSourceId;
			std::string textureSourceId;
			int         vertexOffset = 0;
			int         normalOffset = 0;
			int         textureOffset = 0;

			for ( auto inputNode = trianglesNode->FirstChildElement( "input" ); inputNode != nullptr;
			      inputNode = inputNode->NextSiblingElement( "input" ) ) {
				std::string semantic = inputNode->Attribute( "semantic" );
				if ( semantic == "VERTEX" ) {
					vertexSourceId = inputNode->Attribute( "source" );
					vertexOffset = strtol( inputNode->Attribute( "offset" ), nullptr, 10 );
				} else if ( semantic == "NORMAL" ) {
					normalSourceId = inputNode->Attribute( "source" );
					normalOffset = strtol( inputNode->Attribute( "offset" ), nullptr, 10 );
				} else if ( semantic == "TEXCOORD" ) {
					textureSourceId = inputNode->Attribute( "source" );
					textureOffset = strtol( inputNode->Attribute( "offset" ), nullptr, 10 );
				} else {
					ng_assert( "false" );
				}
			}
			auto verticesNode = meshNode->FirstChildElement( "vertices" );
			if ( verticesNode != nullptr ) {
				std::string verticesNodeId = std::string( "#" ) + verticesNode->Attribute( "id" );
				if ( verticesNodeId == vertexSourceId ) {
					// @HARDCODED
					// i don't really know what to do here, triangles might reference the id of the node <vertices>
					// as input for vertices, but the node <node> vertices redirect to a different input
					auto inputNode = verticesNode->FirstChildElement( "input" );
					ng_assert( inputNode != nullptr );
					vertexSourceId = inputNode->Attribute( "source" );
				}
			}

			ng_assert( vertexSourceId.size() > 0 );
			ng_assert( normalSourceId.size() > 0 );
			ng_assert( textureSourceId.size() > 0 );
			auto facesNode = trianglesNode->FirstChildElement( "p" );
			ng_assert( facesNode != nullptr );

			outModel.meshes.resize( outModel.meshes.size() + 1 );
			auto & mesh = outModel.meshes.at( outModel.meshes.size() - 1 );
			mesh.vertices.resize( numTriangles * 3ull );
			u32 currentIndex = 0;

			std::string materialName = trianglesNode->Attribute( "material" );
			// TODO: Parse materials and use the real one
			static Texture defaultWhiteTexture = CreateDefaultWhiteTexture();
			if ( !outModel.materials.contains( materialName ) ) {
				Material mat;
				mat.diffuseTexture = defaultWhiteTexture;
				outModel.materials[materialName] = mat;
			}

			mesh.material = &outModel.materials[ materialName ];

			const char * facesData = facesNode->GetText();
			while ( facesData != nullptr && facesData[ 0 ] != '\0' ) {
				Vertex & vertex = mesh.vertices[ currentIndex ];

				for ( int i = 0; i < 3; i++ ) {
					char * endPtr;
					u64    index = strtoull( facesData, &endPtr, 10 );
					if ( i == vertexOffset ) {
						FloatSourceData & inputData = sources.at( vertexSourceId );
						u64               startIndex = inputData.stride * index;
						vertex.position = glm::vec3( inputData.data[ startIndex + 0 ], inputData.data[ startIndex + 1 ],
						                             inputData.data[ startIndex + 2 ] );
					} else if ( i == normalOffset ) {
						FloatSourceData & inputData = sources.at( normalSourceId );
						u64               startIndex = inputData.stride * index;
						vertex.normal = glm::vec3( inputData.data[ startIndex + 0 ], inputData.data[ startIndex + 1 ],
						                           inputData.data[ startIndex + 2 ] );
					} else if ( i == textureOffset ) {
						FloatSourceData & inputData = sources.at( textureSourceId );
						u64               startIndex = inputData.stride * index;
						vertex.texCoords =
						    glm::vec2( inputData.data[ startIndex + 0 ], inputData.data[ startIndex + 1 ] );
					} else {
						ng_assert( false );
					}
					if ( *endPtr == ' ' ) {
						facesData = endPtr + 1;
					} else {
						facesData = endPtr;
					}
				}
				mesh.indices.push_back( currentIndex++ );
			}
		}
	}

	ComputeModelSize( outModel );

	return true;
}
