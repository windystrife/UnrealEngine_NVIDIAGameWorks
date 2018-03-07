// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavAgentInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavAgentInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavAgentInterface
{
	GENERATED_IINTERFACE_BODY()

	/**
	 *	Retrieves FNavAgentProperties expressing navigation props and caps of represented agent
	 *	@NOTE the function will be renamed to GetNavAgentProperties in 4.8. Current name was introduced
	 *		to help with deprecating old GetNavAgentProperties function
	 */
	virtual const FNavAgentProperties& GetNavAgentPropertiesRef() const { return FNavAgentProperties::DefaultProperties; }

	/**
	 *	Retrieves Agent's location
	 */
	virtual FVector GetNavAgentLocation() const PURE_VIRTUAL(INavAgentInterface::GetNavAgentLocation, return FVector::ZeroVector;);

	/** Allow actor to specify additional offset (relative to NavLocation) when it's used as move goal */
	virtual FVector GetMoveGoalOffset(const AActor* MovingActor) const { return FVector::ZeroVector; }

	/** Get cylinder for testing if actor has been reached
	 * @param MoveOffset - destination (relative to actor's nav location)
	 * @param GoalOffset - cylinder center (relative to actor's nav location)
	 * @param GoalRadius - cylinder radius
	 * @param GoalHalfHeight - cylinder half height
	 */
	virtual void GetMoveGoalReachTest(const AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const { }

	/** Allows delaying repath requests */
	virtual bool ShouldPostponePathUpdates() const { return false; }

	/** Checks if the agent is actively following a navigation path */
	virtual bool IsFollowingAPath() const { return false; }

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
	DEPRECATED_FORGAME(4.13, "This function is now deprecated and will not be called, please use override with const Actor pointer.")
	virtual FVector GetMoveGoalOffset(AActor* MovingActor) const
	{
		return GetMoveGoalOffset((const AActor*)MovingActor);
	}

	DEPRECATED_FORGAME(4.13, "This function is now deprecatedand will not be called, please use override with const Actor pointer.")
	virtual void GetMoveGoalReachTest(AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const 
	{
		GetMoveGoalReachTest((const AActor*)MovingActor, MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);
	}
};
