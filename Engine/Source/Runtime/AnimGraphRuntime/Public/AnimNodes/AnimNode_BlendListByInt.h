// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNodes/AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByInt.generated.h"

// Blend list node; has many children
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendListByInt : public FAnimNode_BlendListBase
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Runtime, meta=(AlwaysAsPin))
	mutable int32 ActiveChildIndex;


public:	
	FAnimNode_BlendListByInt()
		:	FAnimNode_BlendListBase()
		,	ActiveChildIndex(0)
	{
	}

protected:
	virtual int32 GetActiveChildIndex();
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};
