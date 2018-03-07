// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavRelevantInterface.generated.h"

struct FNavigableGeometryExport;
struct FNavigationRelevantData;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavRelevantInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavRelevantInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Prepare navigation modifiers */
	virtual void GetNavigationData(FNavigationRelevantData& Data) const {}

	/** Get bounds for navigation octree */
	virtual FBox GetNavigationBounds() const { return FBox(ForceInit); }

	/** if this instance knows how to export sub-sections of self */
	virtual bool SupportsGatheringGeometrySlices() const { return false; }

	/** This function is called "on demand", whenever that specified piece of geometry is needed for navigation generation */
	virtual void GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const {}

	virtual ENavDataGatheringMode GetGeometryGatheringMode() const { return ENavDataGatheringMode::Default; }

	/** Called on Game-thread to give implementer a change to perform actions that require game-thread to run, 
	 *	for example precaching physics data */
	virtual void PrepareGeometryExportSync() {}

	/** Update bounds, called after moving owning actor */
	virtual void UpdateNavigationBounds() {}

	/** Are modifiers active? */
	virtual bool IsNavigationRelevant() const { return true; }

	/** Get navigation parent
	 *  Adds modifiers to existing octree node, GetNavigationBounds and IsNavigationRelevant won't be checked
	 */
	virtual UObject* GetNavigationParent() const { return NULL; }
};
