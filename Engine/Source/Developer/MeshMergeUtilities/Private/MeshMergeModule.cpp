// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeModule.h"
#include "MeshMergeUtilities.h"
#include "Modules/ModuleManager.h"

class FMeshMergeModule : public IMeshMergeModule
{
public:
	virtual const IMeshMergeUtilities& GetUtilities() const override
	{
		return *dynamic_cast<const IMeshMergeUtilities*>(&Utilities);
	}
protected:
	FMeshMergeUtilities Utilities;
};


IMPLEMENT_MODULE(FMeshMergeModule, MeshMergeUtilities);