// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FHierarchicalLODProxyProcessor;
class IHierarchicalLODUtilities;

class IHierarchicalLODUtilitiesModule : public IModuleInterface
{
public:

	virtual FHierarchicalLODProxyProcessor* GetProxyProcessor() = 0;
	virtual IHierarchicalLODUtilities* GetUtilities() = 0;
};

/**
* IHierarchicalLODUtilities module interface
*/
class HIERARCHICALLODUTILITIES_API FHierarchicalLODUtilitiesModule : public IHierarchicalLODUtilitiesModule
{
public:
	virtual void ShutdownModule() override;
	virtual void StartupModule() override;

	/** Returns the Proxy processor instance from within this module */
	virtual FHierarchicalLODProxyProcessor* GetProxyProcessor() override;
	virtual IHierarchicalLODUtilities* GetUtilities() override;
private:
	FHierarchicalLODProxyProcessor* ProxyProcessor;
	IHierarchicalLODUtilities* Utilities;
};
