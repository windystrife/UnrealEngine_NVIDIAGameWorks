// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class IMeshReduction;
class IMeshMerging;

class IMeshReductionManagerModule : public IModuleInterface
{
public:	
	/** Returns the mesh reduction plugin if available. */
	virtual IMeshReduction* GetStaticMeshReductionInterface() const = 0;

	/** Returns the mesh reduction plugin if available. */
	virtual IMeshReduction* GetSkeletalMeshReductionInterface() const = 0;

	/** Returns the mesh merging plugin if available. */
	virtual IMeshMerging* GetMeshMergingInterface() const = 0;

	/** Returns the distributed mesh merging plugin if available. */
	virtual IMeshMerging* GetDistributedMeshMergingInterface() const = 0;
};
