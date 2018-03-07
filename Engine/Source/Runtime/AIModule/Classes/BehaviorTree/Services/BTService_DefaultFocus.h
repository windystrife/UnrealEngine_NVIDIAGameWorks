// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "InputCoreTypes.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_DefaultFocus.generated.h"

class AActor;
class UBlackboardComponent;

struct FBTFocusMemory
{
	AActor* FocusActorSet;
	FVector FocusLocationSet;
	bool bActorSet;

	void Reset()
	{
		FocusActorSet = nullptr;
		FocusLocationSet = FAISystem::InvalidLocation;
		bActorSet = false;
	}
};

/**
 * Default Focus service node.
 * A service node that automatically sets the AI controller's focus when it becomes active.
 */
UCLASS(hidecategories=(Service))
class AIMODULE_API UBTService_DefaultFocus : public UBTService_BlackboardBase
{
	GENERATED_BODY()

protected:
	// not exposed to users on purpose. Here to make reusing focus-setting mechanics by derived classes possible
	UPROPERTY()
	uint8 FocusPriority;

	UBTService_DefaultFocus(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFocusMemory); }
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;

	EBlackboardNotificationResult OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
