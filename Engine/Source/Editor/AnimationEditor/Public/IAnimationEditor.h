// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "IHasPersonaToolkit.h"

class UAnimationAsset;

class IAnimationEditor : public FWorkflowCentricApplication, public IHasPersonaToolkit
{
public:
	/** Set the animation asset of the editor. */
	virtual void SetAnimationAsset(UAnimationAsset* AnimAsset) = 0;
};
