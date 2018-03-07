// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SNodePanel.h"

class UBlueprint;

// Context used to aid debugging displays for nodes
struct FKismetNodeInfoContext : public FNodeInfoContext
{
public:
	struct FObjectUUIDPair
	{
	public:
		class UObject* Object;
		int32 UUID;
	public:
		FObjectUUIDPair(class UObject* InObject, int32 InUUID) : Object(InObject), UUID(InUUID) {}

		FString GetDisplayName() const
		{
			if (AActor* Actor = Cast<AActor>(Object))
			{
				return Actor->GetActorLabel();
			}
			else
			{
				return (Object != NULL) ? Object->GetName() : TEXT("(null)");
			}
		}
	};

	TMap<class UEdGraphNode*, TArray<FObjectUUIDPair> > NodesWithActiveLatentActions;

	// Set of pins with watches
	TSet<UEdGraphPin*> WatchedPinSet;

	// Set of nodes with a pin that is being watched
	TSet<UEdGraphNode*> WatchedNodeSet;

	// Source blueprint for the graph
	UBlueprint* SourceBlueprint;

	// Object being debugged for the graph
	UObject* ActiveObjectBeingDebugged;

public:
	FKismetNodeInfoContext(class UEdGraph* SourceGraph);
};

