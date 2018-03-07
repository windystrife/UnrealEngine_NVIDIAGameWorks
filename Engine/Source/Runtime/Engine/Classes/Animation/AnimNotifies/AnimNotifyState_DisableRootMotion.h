// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_DisableRootMotion.generated.h"

UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, MinimalAPI, meta = (DisplayName = "Disable Root Motion"))
class UAnimNotifyState_DisableRootMotion : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif

protected:

};
