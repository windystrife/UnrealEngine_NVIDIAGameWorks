// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotify_PlayMontageNotify.generated.h"


//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotify
//////////////////////////////////////////////////////////////////////////

UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, MinimalAPI, meta = (DisplayName = "Montage Notify"))
class UAnimNotify_PlayMontageNotify : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

public:

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
protected:

	// Name of notify that is passed to the PlayMontage K2Node.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notify")
	FName NotifyName;
};


//////////////////////////////////////////////////////////////////////////
// UAnimNotify_PlayMontageNotifyWindow
//////////////////////////////////////////////////////////////////////////

UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, MinimalAPI, meta = (DisplayName = "Montage Notify Window"))
class UAnimNotify_PlayMontageNotifyWindow : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
protected:

	// Name of notify that is passed to ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Notify")
	FName NotifyName;
};
