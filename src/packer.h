#pragma once

#include "ngLib/nglib.h"
#include <string>
#include <vector>

using PackerResourceID = u64;

struct PackerResource {
	enum class Type {
		PNG,
		VERTEX_SHADER,
		FRAGMENT_SHADER,
		FONT,
		INVALID,
	};

	PackerResourceID id;
	char             name[ 64 ];
	u64              offset;
	u64              size;
	Type             type;
};

struct PackerPackage {
	u8 *                          data;
	u64                           size;
	std::vector< PackerResource > resourceList;

	PackerResource * GrabResource( PackerResourceID resourceID );
	u8 *             GrabResourceData( PackerResourceID resourceID );
	u8 *             GrabResourceData( const PackerResource & resource );
};

bool PackerReadArchive( const char * path, PackerPackage * package );
bool PackerCreateArchive( const char * resourcesPath, const char * outPath );
bool PackerCreateRuntimeArchive( const char * resourcesPath, PackerPackage * package );
