// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "NavLinkCustomInterface.generated.h"

/** 
 *  Interface for custom navigation links
 *
 *  They can affect path finding requests without navmesh rebuilds (e.g. opened/closed doors),
 *  allows updating their area class without navmesh rebuilds (e.g. dynamic path cost)
 *  and give hooks for supporting custom movement (e.g. ladders),
 *
 *  Owner is responsible for registering and unregistering links in NavigationSystem:
 *  - RegisterCustomLink
 *  - UnregisterCustomLink
 *
 *  See also: NavLinkCustomComponent
 */

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavLinkCustomInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ENGINE_API INavLinkCustomInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Get basic link data: two points (relative to owner) and direction */
	virtual void GetLinkData(FVector& LeftPt, FVector& RightPt, ENavLinkDirection::Type& Direction) const {};

	/** Get basic link data: area class (null = default walkable) */
	virtual TSubclassOf<UNavArea> GetLinkAreaClass() const { return NULL; }

	/** Get unique ID number for custom link
	 *  Owner should get its unique ID by calling INavLinkCustomInterface::GetUniqueId() and store it
	 */
	virtual uint32 GetLinkId() const { return 0; }

	/** Update unique ID number for custom link by navigation system */
	virtual void UpdateLinkId(uint32 NewUniqueId) { }

	/** Get object owner of navigation link, used for creating containers with multiple links */
	virtual UObject* GetLinkOwner() const;

	/** Check if link allows path finding
	 *  Querier is usually an AIController trying to find path
	 */
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const { return true; }

	/** Notify called when agent starts using this link for movement.
	 *  returns true = custom movement, path following will NOT update velocity until FinishUsingCustomLink() is called on it
	 */
	virtual bool OnLinkMoveStarted(class UPathFollowingComponent* PathComp, const FVector& DestPoint) { return false; }

	/** Notify called when agent finishes using this link for movement */
	virtual void OnLinkMoveFinished(class UPathFollowingComponent* PathComp) {}

	/** Helper function: returns unique ID number for custom links */
	static uint32 GetUniqueId();

	/** Helper function: bump unique ID numbers above given one */
	static void UpdateUniqueId(uint32 AlreadyUsedId);

	/** Helper function: create modifier for navigation data export */
	static FNavigationLink GetModifier(const INavLinkCustomInterface* CustomNavLink);

	static uint32 NextUniqueId;
};
