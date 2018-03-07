// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FBlueprintActionFilter;
struct FBlueprintActionInfo;

class FBlueprintGraphModule : public IModuleInterface
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FActionMenuRejectionTest, FBlueprintActionFilter const&, FBlueprintActionInfo&);
	/**
	 * Provides access to a list of additional rejection tests that will be  
	 * utilized when constructing Blueprint context/action menus. Helpful to 
	 * game modules, so they can inject their own custom filtering (and hide 
	 * options that may be undesired for their specific project).
	 *
	 * These rejection tests will be ran after all the standard filter tests, 
	 * and in the order that they are in this array.
	 * 
	 * @return An array of additional rejection tests for FBlueprintActionFilter.
	 */
	BLUEPRINTGRAPH_API TArray<FActionMenuRejectionTest>& GetExtendedActionMenuFilters() { return ExtendedMenuFilters; }

private:
	TArray<FActionMenuRejectionTest> ExtendedMenuFilters;
};
