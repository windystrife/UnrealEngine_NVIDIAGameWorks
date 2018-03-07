// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class ISnappingPolicy;

/**
 * Snapping policy manager module
 */
class IViewportSnappingModule : public IModuleInterface//, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	virtual void RegisterSnappingPolicy(TSharedPtr<ISnappingPolicy> NewPolicy)=0;
	virtual void UnregisterSnappingPolicy(TSharedPtr<ISnappingPolicy> PolicyToRemove)=0;

	virtual TSharedPtr<ISnappingPolicy> GetMergedPolicy()=0;

	static TSharedPtr<ISnappingPolicy> GetSnapManager()
	{
		IViewportSnappingModule& Module = FModuleManager::LoadModuleChecked<IViewportSnappingModule>("ViewportSnapping");
		return Module.GetMergedPolicy();
	}
};

