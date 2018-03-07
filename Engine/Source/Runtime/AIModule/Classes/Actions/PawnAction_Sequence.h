// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Actions/PawnAction.h"
#include "PawnAction_Sequence.generated.h"

UCLASS()
class AIMODULE_API UPawnAction_Sequence : public UPawnAction
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<UPawnAction*> ActionSequence;

	UPROPERTY(Category = PawnAction, EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EPawnActionFailHandling::Type> ChildFailureHandlingMode;

	UPROPERTY(Transient)
	UPawnAction* RecentActionCopy;

	uint32 CurrentActionIndex;

	EPawnSubActionTriggeringPolicy::Type SubActionTriggeringPolicy;

	static UPawnAction_Sequence* CreateAction(UWorld& World, TArray<UPawnAction*>& ActionSequence, EPawnSubActionTriggeringPolicy::Type InSubActionTriggeringPolicy = EPawnSubActionTriggeringPolicy::CopyBeforeTriggering);

protected:
	virtual bool Start() override;
	virtual bool Resume() override;
	virtual void OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult) override;

	bool PushNextActionCopy();
};
