// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * This kismet library is used for helper functions primarily used in the kismet compiler for AI related nodes
 * NOTE: Do not change the signatures for any of these functions as it can break the kismet compiler and/or the nodes referencing them
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Pawn.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AIBlueprintHelperLibrary.generated.h"

class AAIController;
class UAIAsyncTaskBlueprintProxy;
class UAnimInstance;
class UBehaviorTree;
class UBlackboardComponent;

UCLASS()
class AIMODULE_API UAIBlueprintHelperLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", BlueprintInternalUseOnly = "TRUE"))
	static UAIAsyncTaskBlueprintProxy* CreateMoveToProxyObject(UObject* WorldContextObject, APawn* Pawn, FVector Destination, AActor* TargetActor = NULL, float AcceptanceRadius = 5.f, bool bStopOnOverlap = false);

	UFUNCTION(BlueprintCallable, Category="AI", meta=(DefaultToSelf="MessageSource"))
	static void SendAIMessage(APawn* Target, FName Message, UObject* MessageSource, bool bSuccess = true);

	UFUNCTION(BlueprintCallable, Category="AI", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static APawn* SpawnAIFromClass(UObject* WorldContextObject, TSubclassOf<APawn> PawnClass, UBehaviorTree* BehaviorTree, FVector Location, FRotator Rotation=FRotator::ZeroRotator, bool bNoCollisionFail=false);

	/** The way it works exactly is if the actor passed in is a pawn, then the function retrieves 
	 *	pawn's controller cast to AIController. Otherwise the function returns actor cast to AIController. */
	UFUNCTION(BlueprintPure, Category = "AI", meta = (DefaultToSelf = "ControlledObject"))
	static AAIController* GetAIController(AActor* ControlledActor);

	UFUNCTION(BlueprintPure, Category="AI", meta=(DefaultToSelf="Target"))
	static UBlackboardComponent* GetBlackboard(AActor* Target);

	/** locks indicated AI resources of animated pawn */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, meta = (DefaultToSelf = "AnimInstance"))
	static void LockAIResourcesWithAnimation(UAnimInstance* AnimInstance, bool bLockMovement, bool LockAILogic);

	/** unlocks indicated AI resources of animated pawn. Will unlock only animation-locked resources */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, meta = (DefaultToSelf = "AnimInstance"))
	static void UnlockAIResourcesWithAnimation(UAnimInstance* AnimInstance, bool bUnlockMovement, bool UnlockAILogic);

	UFUNCTION(BlueprintPure, Category = "AI")
	static bool IsValidAILocation(FVector Location);

	UFUNCTION(BlueprintPure, Category = "AI")
	static bool IsValidAIDirection(FVector DirectionVector);

	UFUNCTION(BlueprintPure, Category = "AI")
	static bool IsValidAIRotation(FRotator Rotation);

	/** Returns a copy of navigation path given controller is currently using. 
	 *	The result being a copy means you won't be able to influence agent's pathfollowing 
	 *	by manipulating received path */
	UFUNCTION(BlueprintPure, Category = "AI", meta = (UnsafeDuringActorConstruction = "true"))
	static UNavigationPath* GetCurrentPath(AController* Controller);

};
