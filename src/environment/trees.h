#pragma once
#include "../system.h"
#include "../mesh.h"

struct CpntTree {};

struct SystemTree : public System< CpntTree > {
	SystemTree();
	virtual void OnCpntAttached( Entity e, CpntTree & t ) override;
	virtual void OnCpntRemoved( Entity e, CpntTree & t ) override;

	InstancedModelBatch instances;
};