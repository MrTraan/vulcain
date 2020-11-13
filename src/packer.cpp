#include "packer.h"
#include <LZ4.h>
#include <algorithm>
#include <filesystem>
#include "window.h"

const char * ResourceTypeToString( PackerResource::Type type ) {
	switch ( type ) {
	case PackerResource::Type::PNG:
		return "PackerResource::Type::PNG";
	case PackerResource::Type::VERTEX_SHADER:
		return "PackerResource::Type::VERTEX_SHADER";
	case PackerResource::Type::FRAGMENT_SHADER:
		return "PackerResource::Type::FRAGMENT_SHADER";
	case PackerResource::Type::FONT:
		return "PackerResource::Type::FONT";
	case PackerResource::Type::OBJ:
		return "PackerResource::Type::OBJ";
	case PackerResource::Type::MATERIAL:
		return "PackerResource::Type::MATERIAL";
	case PackerResource::Type::COLLADA:
		return "PackerResource::Type::COLLADA";
	case PackerResource::Type::INVALID:
	default:
		ng_assert( false );
		return "PackerResource::Type::INVALID";
	}
}

PackerResource::Type GuessTypeFromExtension( const std::string & ext ) {
	if ( ext == ".png" ) {
		return PackerResource::Type::PNG;
	}
	if ( ext == ".vert" ) {
		return PackerResource::Type::VERTEX_SHADER;
	}
	if ( ext == ".frag" ) {
		return PackerResource::Type::FRAGMENT_SHADER;
	}
	if ( ext == ".ttf" ) {
		return PackerResource::Type::FONT;
	}
	if ( ext == ".obj" ) {
		return PackerResource::Type::OBJ;
	}
	if ( ext == ".dae" ) {
		return PackerResource::Type::COLLADA;
	}
	if ( ext == ".mtl" ) {
		return PackerResource::Type::MATERIAL;
	}
	return PackerResource::Type::INVALID;
}

static void AdaptSourceToOpenglCompatibilityVersion( u8 *src, size_t srcLen )
{
#if !OPENGL_COMPATIBILITY_VERSION
	return;
#endif
	char	*firstLine = "#version 4";
	char	*firstLineEnd = " core\n";
	char	*secondLine = "#define OPENGL_COMPATIBILITY_VERSION ";
	int 	len1 = strlen( firstLine );
	int		len1End = strlen( firstLineEnd );
	int		len2 = strlen( secondLine );
	if ( srcLen < len1 + 2 + len1End + len2 + 1 )
		return;
	if ( memcmp( firstLine, src, len1 ) == 0 && memcmp( secondLine, src + len1 + 2 + len1End, len2 ) == 0  ) {
		// Change 4xx to 410 (max version supported by OSX)
		src[ len1 ] = '1';
		src[ len1 + 1 ] = '0';
		// Set OPENGL_COMPATIBILITY_VERSION to 1
		src[ len1 + 2 + len1End + len2 ] = '1';
	}
}

bool PackerReadArchive( const char * path, PackerPackage * package ) {
	ng::File archive;
	bool     success = archive.Open( path, ng::File::MODE_READ );
	if ( !success ) {
		return false;
	}

	u64  archiveSize = archive.GetSize();
	u8 * inBuffer = new u8[ archiveSize ];
	u64  bytesRead = archive.Read( inBuffer, archiveSize );
	archive.Close();
	ng_assert( bytesRead == archiveSize );
	ng_assert( bytesRead > sizeof( u64 ) );

	u64 uncompressedBufferSize = *( ( u64 * )inBuffer );

	u8 * uncompressedBuffer = new u8[ uncompressedBufferSize ];

	int bytesDecompressed = LZ4_decompress_safe( ( char * )inBuffer + sizeof( u64 ), ( char * )uncompressedBuffer,
	                                             (int)(archiveSize - sizeof( u64 )), (int)uncompressedBufferSize );

	ng_assert( bytesDecompressed > 0 );

	package->data = uncompressedBuffer;
	package->size = bytesDecompressed;
	package->resourceList.clear();

	u64 offset = 0;
	while ( offset < package->size ) {
		PackerResource * res = ( PackerResource * )( package->data + offset );
		offset += sizeof( PackerResource );
		offset += res->size;
		package->resourceList.push_back( *res );
	}

	delete[] inBuffer;
	return true;
}

bool PackerCreateRuntimeArchive( const char * resourcesPath, PackerPackage * package ) {
	std::vector< std::string > filesInFolder;
	bool success = ng::ListFilesInDirectory( resourcesPath, filesInFolder, ng::ListFileMode::RECURSIVE );
	std::sort( filesInFolder.begin(), filesInFolder.end() );
	ng_assert( success == true );
	if ( success == false ) {
		return false;
	}

	u64  nextID = 0;
	u8 * archiveData = nullptr;
	u64  archiveDataSize = 0;
	for ( const std::string & filePath : filesInFolder ) {
		std::filesystem::path fsPath( filePath );
		PackerResource::Type  type = GuessTypeFromExtension( fsPath.extension().string() );
		ng_assert( filePath.starts_with( resourcesPath ) );
		std::string fileName = filePath.substr( strlen( resourcesPath ) );
		while ( fileName.at( 0 ) == '\\' || fileName.at( 0 ) == '/' ) {
			fileName.erase( 0, 1 );
		}
		std::replace( fileName.begin(), fileName.end(), '\\', '/' );
		if ( type == PackerResource::Type::INVALID ) {
			ng::Printf( "Skipping unkown type resource %s\n", filePath.c_str() );
			continue;
		}

		ng::File file;
		success = file.Open( filePath.c_str(), ng::File::MODE_READ );
		ng_assert( success == true );

		u64 fileSize = file.GetSize();
		u64 filePrefixSize = 0;
		char *openGlShaderDefine = "#define OPENGL_COMPATIBILITY_VERSION 0\n";
		if ( type == PackerResource::Type::VERTEX_SHADER || type == PackerResource::Type::FRAGMENT_SHADER )
			filePrefixSize = strlen( openGlShaderDefine );
		archiveData = ( u8 * )realloc( archiveData, archiveDataSize + sizeof( PackerResource ) + filePrefixSize + fileSize );
		ng_assert( archiveData != nullptr );
		u8 *fileData = archiveData + archiveDataSize + sizeof( PackerResource );

		PackerResource * header = ( PackerResource * )( archiveData + archiveDataSize );
		header->type = type;
		strncpy( header->name, fileName.c_str(), 63 );
		header->id = nextID++;
		header->name[ 63 ] = 0;
		header->size = filePrefixSize + fileSize;
		header->offset = archiveDataSize + sizeof( PackerResource );

		file.Read( fileData + filePrefixSize, fileSize );

		if ( type == PackerResource::Type::VERTEX_SHADER || type == PackerResource::Type::FRAGMENT_SHADER ) {
			if ( memcmp( "#version ", fileData + filePrefixSize, strlen( "#version " ) ) == 0 &&
					memcmp( " core\n", fileData + filePrefixSize + strlen( "#version " ) + 3, strlen( " core\n" ) ) == 0
			) {
				memcpy( fileData, fileData + filePrefixSize, strlen( "#version " ) + 3 + strlen( " core\n" ) );
				memcpy( fileData + strlen( "#version " ) + 3 + strlen( " core\n" ), openGlShaderDefine, filePrefixSize );
				AdaptSourceToOpenglCompatibilityVersion( archiveData + archiveDataSize + sizeof( PackerResource ), filePrefixSize + fileSize );
			}
		}

		archiveDataSize += sizeof( PackerResource ) + filePrefixSize + fileSize;
	}
	package->data = archiveData;
	package->size = archiveDataSize;
	package->resourceList.clear();

	u64 offset = 0;
	while ( offset < package->size ) {
		PackerResource * res = ( PackerResource * )( package->data + offset );
		offset += sizeof( PackerResource );
		offset += res->size;
		package->resourceList.push_back( *res );
	}
	return true;
}

bool PackerCreateArchive( const char * resourcesPath, const char * outPath ) {
	u8 * archiveData = nullptr;
	u64  archiveDataSize = 0;

	std::vector< std::string > filesInFolder;
	bool success = ng::ListFilesInDirectory( resourcesPath, filesInFolder, ng::ListFileMode::RECURSIVE );
	std::sort( filesInFolder.begin(), filesInFolder.end() );
	ng_assert( success == true );
	if ( success == false ) {
		return false;
	}

	std::string headerFileSource = "#pragma once\n\n#include \"packer.h\"\n\nnamespace PackerResources {\n";

	u64 nextID = 0;
	for ( const std::string & filePath : filesInFolder ) {
		std::filesystem::path fsPath( filePath );
		PackerResource::Type  type = GuessTypeFromExtension( fsPath.extension().string() );
		ng_assert( filePath.starts_with( resourcesPath ) );
		std::string fileName = filePath.substr( strlen( resourcesPath ) );
		while ( fileName.at( 0 ) == '\\' || fileName.at( 0 ) == '/' ) {
			fileName.erase( 0, 1 );
		}
		std::replace( fileName.begin(), fileName.end(), '\\', '/' );
		if ( type == PackerResource::Type::INVALID ) {
			ng::Printf( "Skipping unkown type resource %s\n", filePath.c_str() );
			continue;
		}

		ng::File file;
		success = file.Open( filePath.c_str(), ng::File::MODE_READ );
		ng_assert( success == true );

		u64 fileSize = file.GetSize();
		u64 filePrefixSize = 0;
		char *openGlShaderDefine = "#define OPENGL_COMPATIBILITY_VERSION 0\n";
		if ( type == PackerResource::Type::VERTEX_SHADER || type == PackerResource::Type::FRAGMENT_SHADER )
			filePrefixSize = strlen( openGlShaderDefine );
		archiveData = ( u8 * )realloc( archiveData, archiveDataSize + sizeof( PackerResource ) + filePrefixSize + fileSize );
		ng_assert( archiveData != nullptr );
		u8 *fileData = archiveData + archiveDataSize + sizeof( PackerResource );

		PackerResource * header = ( PackerResource * )( archiveData + archiveDataSize );
		header->type = type;
		strncpy( header->name, fileName.c_str(), 63 );
		header->id = nextID++;
		header->name[ 63 ] = 0;
		header->size =  filePrefixSize + fileSize;
		header->offset = archiveDataSize + sizeof( PackerResource );

		headerFileSource += "constexpr PackerResourceID ";
		std::string varName = header->name;
		std::replace( varName.begin(), varName.end(), '/', '_' );
		std::replace( varName.begin(), varName.end(), '.', '_' );
		std::transform( varName.begin(), varName.end(), varName.begin(), ::toupper );
		headerFileSource += varName;
		headerFileSource += " = ";
		headerFileSource += std::to_string( header->id );
		headerFileSource += "u;\n";

		file.Read( archiveData + archiveDataSize + sizeof( PackerResource ), fileSize );

		if ( type == PackerResource::Type::VERTEX_SHADER || type == PackerResource::Type::FRAGMENT_SHADER ) {
			if ( memcmp( "#version ", fileData + filePrefixSize, strlen( "#version " ) ) == 0 &&
					memcmp( " core\n", fileData + filePrefixSize + strlen( "#version " ) + 3, strlen( " core\n" ) ) == 0
			) {
				memcpy( fileData, fileData + filePrefixSize, strlen( "#version " ) + 3 + strlen( " core\n" ) );
				memcpy( fileData + strlen( "#version " ) + 3 + strlen( " core\n" ), openGlShaderDefine, filePrefixSize );
				AdaptSourceToOpenglCompatibilityVersion( archiveData + archiveDataSize + sizeof( PackerResource ), filePrefixSize + fileSize );
			}
		}

		archiveDataSize += sizeof( PackerResource ) + filePrefixSize + fileSize;
	}
	headerFileSource += "};\n";

	ng::File headerFile;
	success = headerFile.Open( "packer_resource_list.h",
	                           ng::File::MODE_TRUNCATE | ng::File::MODE_CREATE | ng::File::MODE_WRITE );
	ng_assert( success == true );
	headerFile.Write( headerFileSource.c_str(), headerFileSource.size() );
	ng::Printf( "Generate header file at %s\n", headerFile.path.c_str() );
	headerFile.Close();

	int  maxCompressedSize = LZ4_compressBound( (int)archiveDataSize );
	u8 * compressedData = ( u8 * )malloc( maxCompressedSize );
	ng_assert( compressedData != nullptr );
	int compressedDataSize =
	    LZ4_compress_default( ( char * )archiveData, ( char * )compressedData, (int)archiveDataSize, (int)maxCompressedSize );
	ng_assert( compressedDataSize > 0 );
	ng::Printf( "Successfully compressed resources archive. TotalSize %.2fmb, Ratio: %.2f\n",
	            ( float )compressedDataSize / 1024.0f / 1024.0f, ( float )compressedDataSize / archiveDataSize );

	ng::File outFile;
	success = outFile.Open( outPath, ng::File::MODE_CREATE | ng::File::MODE_TRUNCATE | ng::File::MODE_WRITE );
	ng_assert( success == true );
	if ( success == false ) {
		free( archiveData );
		return false;
	}

	outFile.Write( &archiveDataSize, sizeof( u64 ) );
	outFile.Write( compressedData, compressedDataSize );
	outFile.Close();

	free( archiveData );
	free( compressedData );
	return true;
}

PackerResource * PackerPackage::GrabResource( PackerResourceID resourceID ) {
	for ( auto & r : resourceList ) {
		if ( r.id == resourceID ) {
			return &r;
		}
	}
	return nullptr;
}

PackerResource * PackerPackage::GrabResourceByName( const char * name ) {
	for ( auto & r : resourceList ) {
		if ( strcmp( name, r.name ) == 0 ) {
			return &r;
		}
	}
	ng_assert( false );
	return nullptr;
}

u8 * PackerPackage::GrabResourceData( PackerResourceID resourceID ) {
	for ( auto & r : resourceList ) {
		if ( r.id == resourceID ) {
			ng_assert( r.offset + r.size <= size );
			return data + r.offset;
		}
	}
	ng_assert( false );
	return nullptr;
}

u8 * PackerPackage::GrabResourceData( const PackerResource & resource ) {
	ng_assert( resource.offset + resource.size <= size );
	return data + resource.offset;
}
