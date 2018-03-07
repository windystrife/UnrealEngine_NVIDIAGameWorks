// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "AnimationStateMachineGraph.generated.h"

UCLASS(MinimalAPI)
class UAnimationStateMachineGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	// Entry node within the state machine
	UPROPERTY()
	class UAnimStateEntryNode* EntryNode;

	// Parent instance node
	UPROPERTY()
	class UAnimGraphNode_StateMachineBase* OwnerAnimGraphNode;
};

