// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPersonaEditorModeManager.h"

/** Persona-specific extensions to the asset editor mode manager */
class FPersonaEditorModeManager : public IPersonaEditorModeManager
{
public:
	/** IPersonaEditorModeManager interface  */
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const override;
};
