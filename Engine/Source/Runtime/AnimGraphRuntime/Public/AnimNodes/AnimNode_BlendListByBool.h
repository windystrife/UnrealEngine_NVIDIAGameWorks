// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNodes/AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByBool.generated.h"

// This node is effectively a 'branch', picking one of two input poses based on an input Boolean value
USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendListByBool : public FAnimNode_BlendListBase
{
	GENERATED_USTRUCT_BODY()
public:
	// Which input should be connected to the output?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Runtime, meta=(AlwaysAsPin))
	bool bActiveValue;

public:	
	FAnimNode_BlendListByBool()
		: FAnimNode_BlendListBase()
	{
	}

protected:
	virtual int32 GetActiveChildIndex() override;
	virtual FString GetNodeName(FNodeDebugData& DebugData) override { return DebugData.GetNodeName(this); }
};
