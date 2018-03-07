// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "LiveLinkRetargetAsset.h"

class ILiveLinkClient;

#include "AnimNode_LiveLinkPose.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct LIVELINK_API FAnimNode_LiveLinkPose : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SourceData, meta = (PinShownByDefault))
	FName SubjectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear, Category = Retarget, meta = (PinShownByDefault))
	TSubclassOf<ULiveLinkRetargetAsset> RetargetAsset;

	UPROPERTY(transient)
	ULiveLinkRetargetAsset* CurrentRetargetAsset;

	FAnimNode_LiveLinkPose();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;

	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext & Context) override {}

	virtual void Update_AnyThread(const FAnimationUpdateContext & Context) override;

	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	// End of FAnimNode_Base interface

	void OnLiveLinkClientRegistered(const FName& Type, class IModularFeature* ModularFeature);
	void OnLiveLinkClientUnregistered(const FName& Type, class IModularFeature* ModularFeature);

private:

	ILiveLinkClient* LiveLinkClient;
};