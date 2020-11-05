#include "trees.h"
#include "../game.h"
#include "../mesh.h"
#include "../packer_resource_list.h"

SystemTree::SystemTree() { instances.Init( g_modelAtlas.GetModel( PackerResources::PINE_DAE) ); }

void SystemTree::OnCpntAttached( Entity e, CpntTree & t ) {
	instances.AddInstance( e, theGame->registery->GetComponent< CpntTransform >( e ).GetMatrix() );
}

void SystemTree::OnCpntRemoved( Entity e, CpntTree & t ) { instances.RemoveInstance( e ); }
