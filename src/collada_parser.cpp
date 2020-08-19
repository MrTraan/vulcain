#include "collada_parser.h"
#include "game.h"

#include <glm/gtx/euler_angles.hpp>
#include <map>
#include <string>
#include <tinyxml2.h>

static void ParseFloatList( const char * data, u64 count, float * out ) {
	for ( u64 i = 0; i < count; i++ ) {
		char * pEnd;
		out[ i ] = strtof( data, &pEnd );
		data = pEnd + 1;
	}
}

static glm::mat4 ReadMatrixText( const char * text ) {
	float f[ 16 ];
	ParseFloatList( text, 16, f );
	return glm::mat4( f[ 0 ], f[ 4 ], f[ 8 ], f[ 12 ], f[ 1 ], f[ 5 ], f[ 9 ], f[ 13 ], f[ 2 ], f[ 6 ], f[ 10 ],
	                  f[ 14 ], f[ 3 ], f[ 7 ], f[ 11 ], f[ 15 ] );
}

void ParseSceneNode( tinyxml2::XMLElement *            node,
                     std::map< std::string, Mesh * > & geometryDictionnary,
                     glm::mat4                         parentTransform ) {
	auto localTransform = parentTransform;

	auto matrixNode = node->FirstChildElement( "matrix" );
	if ( matrixNode != nullptr ) {
		std::string sid = matrixNode->Attribute( "sid" );
		ng_assert( sid == "transform" );
		glm::mat4 transform = ReadMatrixText( matrixNode->GetText() );
		localTransform = parentTransform * transform;
	}

	auto instanceNode = node->FirstChildElement( "instance_geometry" );
	if ( instanceNode != nullptr ) {
		std::string id = instanceNode->Attribute( "url" );
		ng_assert( geometryDictionnary.contains( id ) );
		Mesh * mesh = geometryDictionnary.at( id );
		mesh->transformation = localTransform;
	}

	for ( auto subNode = node->FirstChildElement( "node" ); subNode != nullptr;
	      subNode = subNode->NextSiblingElement( "node" ) ) {
		ParseSceneNode( subNode, geometryDictionnary, localTransform );
	}
}

bool ImportColladaFile( PackerResourceID resourceID, Model & outModel ) {
	ng::ScopedChrono chrono( "Import collada file" );
	auto             objResource = theGame->package.GrabResource( resourceID );
	ng_assert( objResource->type == PackerResource::Type::COLLADA );
	const char * data = ( const char * )theGame->package.GrabResourceData( *objResource );

	tinyxml2::XMLDocument doc;
	auto                  ret = doc.Parse( data, objResource->size );
	ng_assert( ret == tinyxml2::XML_SUCCESS );
	auto colladaDOM = doc.FirstChildElement( "COLLADA" );

	std::map< std::string, PackerResource * > textureDictionnary;
	std::map< std::string, Material >         effectsDictionnary;
	std::map< std::string, Mesh * >           geometryDictionnary;

	// Parse textures
	auto imagesLibraryNode = colladaDOM->FirstChildElement( "library_images" );
	ng_assert( imagesLibraryNode != nullptr );
	for ( auto imageNode = imagesLibraryNode->FirstChildElement( "image" ); imageNode != nullptr;
	      imageNode = imageNode->NextSiblingElement( "image" ) ) {
		std::string id = imageNode->Attribute( "id" );
		
		auto initFromNode = imageNode->FirstChildElement("init_from");
		ng_assert(initFromNode != nullptr );
		auto        textureResource = theGame->package.GrabResourceByName( initFromNode->GetText() );
		ng_assert(textureResource != nullptr );
		textureDictionnary[id] = textureResource;
	}

	// Parse effects
	auto effectsLibraryNode = colladaDOM->FirstChildElement( "library_effects" );
	ng_assert( effectsLibraryNode != nullptr );

	for ( auto effectNode = effectsLibraryNode->FirstChildElement( "effect" ); effectNode != nullptr;
	      effectNode = effectNode->NextSiblingElement( "effect" ) ) {
		std::string id = std::string( "#" ) + effectNode->Attribute( "id" );
		Material &  material = effectsDictionnary[ id ];

		auto profileNode = effectNode->FirstChildElement( "profile_COMMON" );
		ng_assert( profileNode != nullptr );

		std::map<std::string, std::string> surfaceToTextureID;
		std::map<std::string, std::string> samplerToSurfaceID;
		for ( auto paramNode = profileNode->FirstChildElement("newparam"); paramNode != nullptr; paramNode = paramNode->NextSiblingElement("newparam")) {
			std::string id = paramNode->Attribute("sid");

			auto surfaceNode = paramNode->FirstChildElement("surface");
			if ( surfaceNode != nullptr) {
				ng_assert(std::string(surfaceNode->Attribute("type")) == "2D");
				auto initFromNode = surfaceNode->FirstChildElement("init_from");
				ng_assert(initFromNode != nullptr);
				surfaceToTextureID.emplace( id, initFromNode->GetText() );
			}

			auto samplerNode = paramNode->FirstChildElement("sampler2D");
			if ( samplerNode != nullptr) {
				auto sourceNode = samplerNode->FirstChildElement("source");
				ng_assert(sourceNode != nullptr);
				samplerToSurfaceID.emplace( id, sourceNode->GetText() );
			}
		}


		auto techniqueNode = profileNode->FirstChildElement( "technique" );
		ng_assert( techniqueNode != nullptr );
		ng_assert( std::string( techniqueNode->Attribute( "sid" ) ) == "common" );

		auto lambertNode = techniqueNode->FirstChildElement( "lambert" );
		ng_assert( lambertNode != nullptr );

		// TODO: We can parse emission attribute
		auto diffuseNode = lambertNode->FirstChildElement( "diffuse" );
		if ( diffuseNode != nullptr ) {
			auto colorNode = diffuseNode->FirstChildElement( "color" );
			if ( colorNode != nullptr ) {
				float values[ 3 ];
				ParseFloatList( colorNode->GetText(), 3, values );
				material.diffuse = glm::vec3( values[ 0 ], values[ 1 ], values[ 2 ] );
			}

			auto textureNode = diffuseNode->FirstChildElement( "texture" );
			if ( textureNode != nullptr ) {
				std::string samplerID = textureNode->Attribute("texture");
				auto textureResource = textureDictionnary.at( surfaceToTextureID.at( samplerToSurfaceID.at(samplerID) ) );
				if ( textureResource == nullptr ) {
					material.diffuseTexture = CreatePlaceholderPinkTexture();
				} else {
					material.diffuseTexture = CreateTextureFromResource( *textureResource );
				}
				if ( material.diffuseTexture.hasTransparency ) {
					material.mode = Material::MODE_TRANSPARENT;
				}
			} else {
				material.diffuseTexture = Texture::DefaultWhiteTexture();
			}
		}
		auto iorNode = lambertNode->FirstChildElement( "index_of_refraction" );
		if ( iorNode != nullptr ) {
			auto floatNode = iorNode->FirstChildElement( "float" );
			ng_assert( floatNode != nullptr );
			material.shininess = strtof( floatNode->GetText(), nullptr );
		}
	}

	// Parse materials
	auto materialLibraryNode = colladaDOM->FirstChildElement( "library_materials" );
	ng_assert( materialLibraryNode != nullptr );
	for ( auto materialNode = materialLibraryNode->FirstChildElement( "material" ); materialNode != nullptr;
	      materialNode = materialNode->NextSiblingElement( "material" ) ) {
		std::string id = materialNode->Attribute( "id" );
		auto        instanceNode = materialNode->FirstChildElement( "instance_effect" );
		ng_assert( instanceNode != nullptr );
		std::string instanceID = instanceNode->Attribute( "url" );
		outModel.materials.emplace( id, effectsDictionnary.at( instanceID ) );
	}

	// Parse geometry
	auto geometryRoot = colladaDOM->FirstChildElement( "library_geometries" );

	// Compute number of meshes in model
	u32 numMeshes = 0;
	for ( auto geometryNode = geometryRoot->FirstChildElement( "geometry" ); geometryNode != nullptr;
	      geometryNode = geometryNode->NextSiblingElement( "geometry" ) ) {
		numMeshes++;
	}
	outModel.meshes.resize( numMeshes );

	u32 meshIndex = 0;
	for ( auto geometryNode = geometryRoot->FirstChildElement( "geometry" ); geometryNode != nullptr;
	      geometryNode = geometryNode->NextSiblingElement( "geometry" ) ) {
		const char * id = geometryNode->Attribute( "id" );
		const char * name = geometryNode->Attribute( "name" );

		Mesh & mesh = outModel.meshes[ meshIndex++ ];

		geometryDictionnary[ std::string( "#" ) + id ] = &mesh;

		auto meshNode = geometryNode->FirstChildElement( "mesh" );
		ng_assert( meshNode != nullptr );

		struct FloatSourceData {
			std::vector< float > data;
			int                  stride;
			u64                  count;
		};
		std::map< std::string, FloatSourceData > sources;

		// Parse all sources
		for ( auto sourceNode = meshNode->FirstChildElement( "source" ); sourceNode != nullptr;
		      sourceNode = sourceNode->NextSiblingElement( "source" ) ) {
			auto floatArray = sourceNode->FirstChildElement( "float_array" );
			ng_assert( floatArray != nullptr );
			std::string       id = std::string( "#" ) + sourceNode->Attribute( "id" );
			int               numElements = atol( floatArray->Attribute( "count" ) );
			FloatSourceData & sourceData = sources[ id ];
			sourceData.data.reserve( numElements );
			const char * cursor = floatArray->GetText();
			char *       endPtr = nullptr;
			for ( int i = 0; i < numElements; i++ ) {
				float data = static_cast< float >( strtod( cursor, &endPtr ) );
				sourceData.data.push_back( data );
				ng_assert( *endPtr == '\0' || *endPtr == ' ' );
				ng_assert( cursor != endPtr );
				cursor = endPtr + 1;
			}
			auto techniqueNode = sourceNode->FirstChildElement( "technique_common" );
			ng_assert( techniqueNode != nullptr );
			auto accessorNode = techniqueNode->FirstChildElement( "accessor" );
			ng_assert( accessorNode != nullptr );
			sourceData.stride = strtol( accessorNode->Attribute( "stride" ), nullptr, 10 );
			sourceData.count = strtoull( accessorNode->Attribute( "count" ), nullptr, 10 );
		}

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

		mesh.vertices.resize( numTriangles * 3ull );
		u32 currentIndex = 0;

		std::string materialName = trianglesNode->Attribute( "material" );
		mesh.material = &outModel.materials.at( materialName );

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
					// flip vertically
					vertex.texCoords = glm::vec2( inputData.data[ startIndex + 0 ], 1.0f - inputData.data[ startIndex + 1 ] );
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

	// Check wheter Z or Y is up
	// If Z is up, rotate the model -90 degrees around the X axis to put Y up
	enum UpAxisConfig {
		Z_UpAxis,
		Y_UpAxis,
	};

	UpAxisConfig upAxis = Z_UpAxis;

	auto assetNode = colladaDOM->FirstChildElement( "asset" );
	ng_assert( assetNode != nullptr );
	if ( auto upAxisNode = assetNode->FirstChildElement( "up_axis" ); upAxisNode != nullptr ) {
		std::string content = upAxisNode->GetText();
		if ( content == "Z_UP" ) {
			upAxis = Z_UpAxis;
		} else if ( content == "Y_UP" ) {
			upAxis = Y_UpAxis;
		} else {
			ng_assert( false );
		}
	}

	// Parse library_visual_scenes
	auto visualSceneLibrary = colladaDOM->FirstChildElement( "library_visual_scenes" );
	if ( visualSceneLibrary != nullptr ) {
		// TODO: Should we parse multiple scenes?
		auto visualSceneNode = visualSceneLibrary->FirstChildElement( "visual_scene" );
		for ( auto sceneNode = visualSceneNode->FirstChildElement( "node" ); sceneNode != nullptr;
		      sceneNode = sceneNode->NextSiblingElement( "node" ) ) {
			glm::mat4 baseTransform( 1.0f );
			if ( upAxis == Z_UpAxis ) {
				baseTransform = glm::eulerAngleX( glm::radians( -90.0f ) );
			}
			ParseSceneNode( sceneNode, geometryDictionnary, baseTransform );
		}
	}

	ComputeModelSize( outModel );

	return true;
}
