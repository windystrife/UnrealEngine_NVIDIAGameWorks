// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "AnimNodes/AnimNode_CurveSource.h"
#include "AnimNodes/AnimNode_PoseBlendNode.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "AnimPreviewInstance.generated.h"

/** Enum to know how montage is being played */
UENUM()
enum EMontagePreviewType
{
	/** Playing montage in usual way. */
	EMPT_Normal, 
	/** Playing all sections. */
	EMPT_AllSections,
	EMPT_MAX,
};


/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct ANIMGRAPH_API FAnimPreviewInstanceProxy : public FAnimSingleNodeInstanceProxy
{
	GENERATED_BODY()

public:
	FAnimPreviewInstanceProxy()
	{
		bCanProcessAdditiveAnimations = true;
	}

	FAnimPreviewInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimSingleNodeInstanceProxy(InAnimInstance)
		, SkeletalControlAlpha(1.0f)
#if WITH_EDITORONLY_DATA
		, bForceRetargetBasePose(false)
#endif
		, bEnableControllers(true)
		, bSetKey(false)
	{
		bCanProcessAdditiveAnimations = true;
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;

	void ResetModifiedBone(bool bCurveController = false);

	FAnimNode_ModifyBone* FindModifiedBone(const FName& InBoneName, bool bCurveController = false);	

	FAnimNode_ModifyBone& ModifyBone(const FName& InBoneName, bool bCurveController = false);

	void RemoveBoneModification(const FName& InBoneName, bool bCurveController = false);

	void SetForceRetargetBasePose(bool bInForceRetargetBasePose)
	{ 
		bForceRetargetBasePose = bInForceRetargetBasePose; 
	}

	bool GetForceRetargetBasePose() const 
	{ 
		return bForceRetargetBasePose; 
	}

	void EnableControllers(bool bEnable)
	{
		bEnableControllers = bEnable;
	}

	void SetSkeletalControlAlpha(float InSkeletalControlAlpha)
	{
		SkeletalControlAlpha = FMath::Clamp<float>(InSkeletalControlAlpha, 0.f, 1.f);
	}

	void SetKey(FSimpleDelegate InOnSetKeyCompleteDelegate)
	{
#if WITH_EDITOR
		bSetKey = true;
		OnSetKeyCompleteDelegate = InOnSetKeyCompleteDelegate;
#endif
	}

	void SetKey()
	{
#if WITH_EDITOR
		bSetKey = true;
#endif
	}

	void SetKeyCompleteDelegate(FSimpleDelegate InOnSetKeyCompleteDelegate)
	{
#if WITH_EDITOR
		OnSetKeyCompleteDelegate = InOnSetKeyCompleteDelegate;
#endif
	}

	void RefreshCurveBoneControllers(UAnimationAsset* AssetToRefreshFrom);

	TArray<FAnimNode_ModifyBone>& GetBoneControllers()
	{
		return BoneControllers;
	}

	TArray<FAnimNode_ModifyBone>& GetCurveBoneControllers()
	{
		return CurveBoneControllers;
	}

	/** Sets an external debug skeletal mesh component to use to debug */
	void SetDebugSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);

	/** Gets the external debug skeletal mesh component we are debugging */
	USkeletalMeshComponent* GetDebugSkeletalMeshComponent() const;

private:
	void UpdateCurveController();

	void ApplyBoneControllers(TArray<FAnimNode_ModifyBone> &InBoneControllers, FComponentSpacePoseContext& ComponentSpacePoseContext);

	void SetKeyImplementation(const FCompactPose& PreControllerInLocalSpace, const FCompactPose& PostControllerInLocalSpace);

	void AddKeyToSequence(UAnimSequence* Sequence, float Time, const FName& BoneName, const FTransform& AdditiveTransform);

private:
	/** Controllers for individual bones */
	TArray<FAnimNode_ModifyBone> BoneControllers;

	/** Curve modifiers */
	TArray<FAnimNode_ModifyBone> CurveBoneControllers;

	/** External curve for in-editor curve sources (such as audio) */
	FAnimNode_CurveSource CurveSource;

	/** Pose blend node for evaluating pose assets (for previewing curve sources) */
	FAnimNode_PoseBlendNode PoseBlendNode;

	/** Allows us to copy a pose from the mesh being debugged */
	FAnimNode_CopyPoseFromMesh CopyPoseNode;

	/**
	 * Delegate to call after Key is set
	 */
	FSimpleDelegate OnSetKeyCompleteDelegate;

	/** Shared parameters for previewing blendspace or animsequence **/
	float SkeletalControlAlpha;

#if WITH_EDITORONLY_DATA
	bool bForceRetargetBasePose;
#endif

	/*
	 * Used to determine if controller has to be applied or not
	 * Used to disable controller during editing
	 */
	bool bEnableControllers;

	/* 
	 * When this flag is true, it sets key
	 */
	bool bSetKey;
};

/**
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

UCLASS(transient, NotBlueprintable, noteditinlinenew)
class ANIMGRAPH_API UAnimPreviewInstance : public UAnimSingleNodeInstance
{
	GENERATED_UCLASS_BODY()

		/** Shared parameters for previewing blendspace or animsequence **/
	UPROPERTY(transient)
	TEnumAsByte<enum EMontagePreviewType> MontagePreviewType;

	UPROPERTY(transient)
	int32 MontagePreviewStartSectionIdx;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UAnimInstance Interface
	virtual void NativeInitializeAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
protected:
	virtual void Montage_Advance(float DeltaTime) override;
	//~ End UAnimInstance Interface

public:
	/** Set SkeletalControl Alpha**/
	void SetSkeletalControlAlpha(float SkeletalControlAlpha);

	UAnimSequence* GetAnimSequence();

	//~ Begin UAnimSingleNodeInstance Interface
	virtual void RestartMontage(UAnimMontage* Montage, FName FromSection = FName()) override;
	virtual void SetAnimationAsset(UAnimationAsset* NewAsset, bool bIsLooping = true, float InPlayRate = 1.f) override;
	//~ End UAnimSingleNodeInstance Interface

	/** Montage preview functions */
	void MontagePreview_JumpToStart();
	void MontagePreview_JumpToEnd();
	void MontagePreview_JumpToPreviewStart();
	void MontagePreview_Restart();
	void MontagePreview_PreviewNormal(int32 FromSectionIdx = INDEX_NONE, bool bPlay = true);
	void MontagePreview_SetLoopNormal(bool bIsLooping, int32 PreferSectionIdx = INDEX_NONE);
	void MontagePreview_PreviewAllSections(bool bPlay = true);
	void MontagePreview_SetLoopAllSections(bool bIsLooping);
	void MontagePreview_SetLoopAllSetupSections(bool bIsLooping);
	void MontagePreview_ResetSectionsOrder();
	void MontagePreview_SetLooping(bool bIsLooping);
	void MontagePreview_SetPlaying(bool bIsPlaying);
	void MontagePreview_SetReverse(bool bInReverse);
	void MontagePreview_StepForward();
	void MontagePreview_StepBackward();
	void MontagePreview_JumpToPosition(float NewPosition);
	int32 MontagePreview_FindFirstSectionAsInMontage(int32 AnySectionIdx);
	int32 MontagePreview_FindLastSection(int32 StartSectionIdx);
	float MontagePreview_CalculateStepLength();
	void MontagePreview_RemoveBlendOut();
	bool IsPlayingMontage() { return GetActiveMontageInstance() != NULL; }

	/** 
	 * Finds an already modified bone 
	 * @param	InBoneName	The name of the bone modification to find
	 * @return the bone modification or NULL if no current modification was found
	 */
	FAnimNode_ModifyBone* FindModifiedBone(const FName& InBoneName, bool bCurveController=false);

	/** 
	 * Modifies a single bone. Create a new FAnimNode_ModifyBone if one does not exist for the passed-in bone.
	 * @param	InBoneName	The name of the bone to modify
	 * @return the new or existing bone modification
	 */
	FAnimNode_ModifyBone& ModifyBone(const FName& InBoneName, bool bCurveController=false);

	/**
	 * Removes an existing bone modification
	 * @param	InBoneName	The name of the existing modification to remove
	 */
	void RemoveBoneModification(const FName& InBoneName, bool bCurveController=false);

	/**
	 * Reset all bone modified
	 */
	void ResetModifiedBone(bool bCurveController=false);

	void SetForceRetargetBasePose(bool ForceRetargetBasePose);

	bool GetForceRetargetBasePose() const;

	/**
	 * Convert current modified bone transforms (BoneControllers) to transform curves (CurveControllers)
	 * it does based on CurrentTime. This function does not set key directly here. 
	 * It does wait until next update, and it gets the delta of transform before applying curves, and 
	 * creates curves from it, so you'll need delegate if you'd like to do something after
	 * 
	 * @param Delegate To be called once set key is completed
	 */
	void SetKey(FSimpleDelegate InOnSetKeyCompleteDelegate);

	/**
	 * Convert current modified bone transforms (BoneControllers) to transform curves (CurveControllers)
	 * it does based on CurrentTime. This function does not set key directly here. 
	 * It does wait until next update, and it gets the delta of transform before applying curves, and 
	 * creates curves from it, so you'll need delegate if you'd like to do something after (set with SetKeyCompleteDelegate)
	 */
	void SetKey();

	/**
	 * Set the delegate to be called when a key is set.
	 * 
	 * @param Delegate To be called once set key is completed
	 */
	void SetKeyCompleteDelegate(FSimpleDelegate InOnSetKeyCompleteDelegate);

	/** 
	 * Refresh Curve Bone Controllers based on TransformCurves from Animation data
	 */
	void RefreshCurveBoneControllers();

	/** 
	 * Enable Controllers
	 * This is used by when editing, when controller has to be disabled
	 */
	void EnableControllers(bool bEnable);

	/** Sets an external debug skeletal mesh component to use to debug */
	void SetDebugSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);

	/** Gets the external debug skeletal mesh component we are debugging */
	USkeletalMeshComponent* GetDebugSkeletalMeshComponent() const;
};



