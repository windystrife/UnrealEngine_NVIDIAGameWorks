// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *	Abstract base class for a skeletal controller.
 *	A SkelControl is a module that can modify the position or orientation of a set of bones in a skeletal mesh in some programmatic way.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/InputScaleBias.h"
#include "AnimNode_SkeletalControlBase.generated.h"

class USkeletalMeshComponent;

USTRUCT()
struct FSocketReference
{
	GENERATED_USTRUCT_BODY()

	/** Target socket to look at. Used if LookAtBone is empty. - You can use  LookAtLocation if you need offset from this point. That location will be used in their local space. **/
	UPROPERTY(EditAnywhere, Category = FSocketReference)
	FName SocketName;

private:
	int32 CachedSocketMeshBoneIndex;
	FCompactPoseBoneIndex CachedSocketCompactBoneIndex;
	FTransform CachedSocketLocalTransform;

public:
	FSocketReference()
		: CachedSocketMeshBoneIndex(INDEX_NONE)
		, CachedSocketCompactBoneIndex(INDEX_NONE)
	{
	}

	FSocketReference(const FName& InSocketName)
		: SocketName(InSocketName)
		, CachedSocketMeshBoneIndex(INDEX_NONE)
		, CachedSocketCompactBoneIndex(INDEX_NONE)
	{
	}

	void InitializeSocketInfo(const FAnimInstanceProxy* InAnimInstanceProxy);
	void InitialzeCompactBoneIndex(const FBoneContainer& RequiredBones);
	/* There are subtle difference between this two IsValid function
	 * First one says the configuration had a valid socket as mesh index is valid
	 * Second one says the current bonecontainer doesn't contain it, meaning the current LOD is missing the joint that is required to evaluate 
	 * Although the expected behavior is ambiguous, I'll still split these two, and use it accordingly */
	bool HasValidSetup() const
	{
		return (CachedSocketMeshBoneIndex != INDEX_NONE);
	}

	bool IsValidToEvaluate() const
	{
		return (CachedSocketCompactBoneIndex != INDEX_NONE);
	}

	FCompactPoseBoneIndex GetCachedSocketCompactBoneIndex() const
	{
		return CachedSocketCompactBoneIndex;
	}

	template<typename poseType>
	FTransform GetAnimatedSocketTransform(struct FCSPose<poseType>& InPose) const
	{
		// current LOD has valid index (FCompactPoseBoneIndex is valid if current LOD supports)
		if (CachedSocketCompactBoneIndex != INDEX_NONE)
		{
			FTransform BoneTransform = InPose.GetComponentSpaceTransform(CachedSocketCompactBoneIndex);
			return CachedSocketLocalTransform * BoneTransform;
		}

		return FTransform::Identity;
	}
};

USTRUCT()
struct FBoneSocketTarget
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = FBoneSocketTarget)
	bool bUseSocket;

	UPROPERTY(EditAnywhere, Category = FBoneSocketTarget, meta=(EditCondition = "!bUseSocket"))
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category = FBoneSocketTarget, meta = (EditCondition = "bUseSocket"))
	FSocketReference SocketReference;

	FBoneSocketTarget(FName InName = NAME_None, bool bInUseSocket = false)
	{
		bUseSocket = bInUseSocket;

		if (bUseSocket)
		{
			SocketReference.SocketName = InName;
		}
		else
		{
			BoneReference.BoneName = InName;
		}
	}

	void InitializeBoneReferences(const FBoneContainer& RequiredBones)
	{
		if (bUseSocket)
		{
			SocketReference.InitialzeCompactBoneIndex(RequiredBones);
		}
		else
		{
			BoneReference.Initialize(RequiredBones);
		}
	}

	/** Initialize Bone Reference, return TRUE if success, otherwise, return false **/
	void Initialize(const FAnimInstanceProxy* InAnimInstanceProxy)
	{
		if (bUseSocket)
		{
			SocketReference.InitializeSocketInfo(InAnimInstanceProxy);
		}
	}


	/** return true if valid. Otherwise return false **/
	bool HasValidSetup() const
	{
		if (bUseSocket)
		{
			return SocketReference.HasValidSetup();
		}

		return BoneReference.BoneIndex != INDEX_NONE;
	}

	bool HasTargetSetup() const
	{
		if (bUseSocket)
		{
			return (SocketReference.SocketName != NAME_None);
		}

		return (BoneReference.BoneName != NAME_None);
	}

	FName GetTargetSetup() const
	{
		if (bUseSocket)
		{
			return (SocketReference.SocketName);
		}

		return (BoneReference.BoneName);
	}

	/** return true if valid. Otherwise return false **/
	bool IsValidToEvaluate(const FBoneContainer& RequiredBones) const
	{
		if (bUseSocket)
		{
			return SocketReference.IsValidToEvaluate();
		}

		return BoneReference.IsValidToEvaluate(RequiredBones);
	}

	// this will return the compact pose bone index that matters
	// if you're using sockeet, it will return socket's related joint's compact pose index
	FCompactPoseBoneIndex GetCompactPoseBoneIndex() const 
	{
		if (bUseSocket)
		{
			return SocketReference.GetCachedSocketCompactBoneIndex();
		}

		return BoneReference.CachedCompactPoseIndex;
	}

	/** Get Target Transform from current incoming pose */
	template<typename poseType>
	FTransform GetTargetTransform(const FVector& TargetOffset, FCSPose<poseType>& InPose, const FTransform& InComponentToWorld) const
	{
		FTransform OutTargetTransform;

		auto SetComponentSpaceOffset = [](const FVector& InTargetOffset, const FTransform& LocalInComponentToWorld, FTransform& LocalOutTargetTransform)
		{
			LocalOutTargetTransform.SetIdentity();
			FVector CSTargetOffset = LocalInComponentToWorld.InverseTransformPosition(InTargetOffset);
			LocalOutTargetTransform.SetLocation(CSTargetOffset);
		};

		if (bUseSocket)
		{
			// this has to be done outside
			if (SocketReference.IsValidToEvaluate())
			{
				FTransform SocketTransformInCS = SocketReference.GetAnimatedSocketTransform(InPose);

				FVector CSTargetOffset = SocketTransformInCS.TransformPosition(TargetOffset);
				OutTargetTransform = SocketTransformInCS;
				OutTargetTransform.SetLocation(CSTargetOffset);
			}
			else
			{
				// if none is found, we consider this offset is world offset
				SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
			}
		}
		// if valid data is available
		else if (BoneReference.HasValidSetup())
		{
			if (BoneReference.IsValidToEvaluate())
			{
				OutTargetTransform = InPose.GetComponentSpaceTransform(BoneReference.CachedCompactPoseIndex);
				FVector CSTargetOffset = OutTargetTransform.TransformPosition(TargetOffset);
				OutTargetTransform.SetLocation(CSTargetOffset);
			}
			else
			{
				// if none is found, we consider this offset is world offset
				SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
			}
		}
		else
		{
			// if none is found, we consider this offset is world offset
			SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
		}

		return OutTargetTransform;
	}

	template<typename poseType>
	FTransform GetTargetTransform(const FTransform& TargetOffset, FCSPose<poseType>& InPose, const FTransform& InComponentToWorld) const
	{
		FTransform OutTargetTransform;

		auto SetComponentSpaceOffset = [](const FTransform& InTargetOffset, const FTransform& LocalInComponentToWorld, FTransform& LocalOutTargetTransform)
		{
			LocalOutTargetTransform = InTargetOffset.GetRelativeTransform(LocalInComponentToWorld);
		};

		if (bUseSocket)
		{
			// this has to be done outside
			if (SocketReference.IsValidToEvaluate())
			{
				OutTargetTransform = TargetOffset * SocketReference.GetAnimatedSocketTransform(InPose);
			}
			else
			{
				SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
			}
		}
		// if valid data is available
		else if (BoneReference.HasValidSetup())
		{
			if (BoneReference.IsValidToEvaluate())
			{
				OutTargetTransform = TargetOffset * InPose.GetComponentSpaceTransform(BoneReference.CachedCompactPoseIndex);
			}
			else
			{
				SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
			}
		}
		else
		{
			SetComponentSpaceOffset(TargetOffset, InComponentToWorld, OutTargetTransform);
		}

		return OutTargetTransform;
	}
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_SkeletalControlBase : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	// Input link
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Links)
	FComponentSpacePoseLink ComponentPose;

	// Current strength of the skeletal control
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(PinShownByDefault))
	mutable float Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FInputScaleBias AlphaScaleBias;

	/*
	* Max LOD that this node is allowed to run
	* For example if you have LODThreadhold to be 2, it will run until LOD 2 (based on 0 index)
	* when the component LOD becomes 3, it will stop update/evaluate
	* currently transition would be issue and that has to be re-visited
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Performance, meta = (DisplayName = "LOD Threshold"))
	int32 LODThreshold;

	UPROPERTY(Transient)
	float ActualAlpha;

public:

	FAnimNode_SkeletalControlBase()
		: Alpha(1.0f)
		, LODThreshold(INDEX_NONE)
		, ActualAlpha(0.f)
	{
	}

public:
#if WITH_EDITORONLY_DATA
	// forwarded pose data from the wired node which current node's skeletal control is not applied yet
	FCSPose<FCompactHeapPose> ForwardedPose;
#endif //#if WITH_EDITORONLY_DATA

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)  override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) final;
	virtual void EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output) final;
	// End of FAnimNode_Base interface

protected:
	// Interface for derived skeletal controls to implement
	// use this function to update for skeletal control base
	virtual void UpdateInternal(const FAnimationUpdateContext& Context);
	// use this function to evaluate for skeletal control base
	virtual void EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context);
	DEPRECATED(4.16, "Please use EvaluateSkeletalControl_AnyThread.")
	virtual void EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms) {}
	// Evaluate the new component-space transforms for the affected bones.
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms);
	// return true if it is valid to Evaluate
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) { return false; }
	// initialize any bone references you have
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones){};

	/** Allow base to add info to the node debug output */
	void AddDebugNodeData(FString& OutDebugData);
private:

	// Resused bone transform array to avoid reallocating in skeletal controls
	TArray<FBoneTransform> BoneTransforms;
};
