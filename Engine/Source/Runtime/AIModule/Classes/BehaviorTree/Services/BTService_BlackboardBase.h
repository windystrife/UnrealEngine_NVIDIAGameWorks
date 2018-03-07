// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTService.h"
#include "BTService_BlackboardBase.generated.h"

class UBehaviorTree;

UCLASS(Abstract)
class AIMODULE_API UBTService_BlackboardBase : public UBTService
{
	GENERATED_UCLASS_BODY()

	/** initialize any asset related data */
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	/** get name of selected blackboard key */
	FName GetSelectedBlackboardKey() const;

protected:

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	struct FBlackboardKeySelector BlackboardKey;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE FName UBTService_BlackboardBase::GetSelectedBlackboardKey() const
{
	return BlackboardKey.SelectedKeyName;
}
