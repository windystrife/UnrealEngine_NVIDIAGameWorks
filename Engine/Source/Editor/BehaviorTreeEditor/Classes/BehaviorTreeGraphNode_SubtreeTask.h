// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeGraphNode_SubtreeTask.generated.h"

UCLASS()
class UBehaviorTreeGraphNode_SubtreeTask : public UBehaviorTreeGraphNode_Task
{
	GENERATED_UCLASS_BODY()

	/** UBehaviorTreeGraph.UpdateCounter value of subtree graph */
	int32 SubtreeVersion;

	/** path of behavior tree asset used to create injected nodes preview */
	FString SubtreePath;

	/** updates nodes injected from subtree's root */
	bool UpdateInjectedNodes();
};
