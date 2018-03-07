// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRetargetAsset.h"

#include "LiveLinkRemapAsset.generated.h"

// Remap asset for data coming from Live Link. Allows simple application of bone transforms into current pose based on name remapping only
UCLASS(Blueprintable)
class ULiveLinkRemapAsset : public ULiveLinkRetargetAsset
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin ULiveLinkRetargetAsset interface
	virtual void BuildPoseForSubject(const FLiveLinkSubjectFrame& InFrame, FCompactPose& OutPose, FBlendedCurve& OutCurve) override;
	//~ End ULiveLinkRetargetAsset interface

	/** Blueprint Implementable function for getting a remapped bone name from the original */
	UFUNCTION(BlueprintNativeEvent, Category = "Live Link Remap")
	FName GetRemappedBoneName(FName BoneName) const;

private:

	void OnBlueprintClassCompiled(UBlueprint* TargetBlueprint);

	// Name mapping between source bone name and transformed bone name
	// (returned from GetRemappedBoneName)
	TMap<FName, FName> NameMap;

	/** Blueprint.OnCompiled delegate handle */
	FDelegateHandle OnBlueprintCompiledDelegate;
};