// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_RefPose.generated.h"

UENUM()
enum ERefPoseType
{
	EIT_LocalSpace, 
	EIT_Additive
};

// RefPose pose nodes - ref pose or additive RefPose pose
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_RefPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<ERefPoseType> RefPoseType;
	
public:	
	FAnimNode_RefPose()
		:	FAnimNode_Base()
		, 	RefPoseType(EIT_LocalSpace)
	{
	}

	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

};

USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_MeshSpaceRefPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()
public:	
	FAnimNode_MeshSpaceRefPose()
		:	FAnimNode_Base()
	{
	}

	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output);
};
