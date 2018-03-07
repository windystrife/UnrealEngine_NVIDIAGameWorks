// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimPreviewInstance.h"
//#include "AnimNode_RigidBody.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "PhysicsAssetEditorAnimInstanceProxy.generated.h"

class UAnimSequence;

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct FPhysicsAssetEditorAnimInstanceProxy : public FAnimPreviewInstanceProxy
{
	GENERATED_BODY()

public:
	FPhysicsAssetEditorAnimInstanceProxy()
	{
	}

	FPhysicsAssetEditorAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimPreviewInstanceProxy(InAnimInstance)
	{
	}

	virtual ~FPhysicsAssetEditorAnimInstanceProxy();

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override;
private:
	void ConstructNodes();

	//FAnimNode_RigidBody RagdollNode;
// 	FAnimNode_ConvertComponentToLocalSpace ComponentToLocalSpace;
// 	FAnimNode_SequencePlayer SequencePlayerNode;
// 	FAnimNode_ConvertLocalToComponentSpace LocalToComponentSpace;
	
	void InitAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId);
	void EnsureAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId);
};