// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EdGraph/EdGraphPin.h"
#include "BehaviorTreeEditorTypes.generated.h"

struct FAbortDrawHelper
{
	uint16 AbortStart;
	uint16 AbortEnd;
	uint16 SearchStart;
	uint16 SearchEnd;

	FAbortDrawHelper() : AbortStart(MAX_uint16), AbortEnd(MAX_uint16), SearchStart(MAX_uint16), SearchEnd(MAX_uint16) {}
};

struct FCompareNodeXLocation
{
	FORCEINLINE bool operator()(const UEdGraphPin& A, const UEdGraphPin& B) const
	{
		const UEdGraphNode* NodeA = A.GetOwningNode();
		const UEdGraphNode* NodeB = B.GetOwningNode();

		if (NodeA->NodePosX == NodeB->NodePosX)
		{
			return NodeA->NodePosY < NodeB->NodePosY;
		}

		return NodeA->NodePosX < NodeB->NodePosX;
	}
};

namespace ESubNode
{
	enum Type {
		Decorator,
		Service
	};
}

struct FNodeBounds
{
	FVector2D Position;
	FVector2D Size;

	FNodeBounds(FVector2D InPos, FVector2D InSize)
	{
		Position = InPos;
		Size = InSize;
	}
};

UCLASS()
class UBehaviorTreeEditorTypes : public UObject
{
	GENERATED_UCLASS_BODY()

	static const FString PinCategory_MultipleNodes;
	static const FString PinCategory_SingleComposite;
	static const FString PinCategory_SingleTask;
	static const FString PinCategory_SingleNode;
};
