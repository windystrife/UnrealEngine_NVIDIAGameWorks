// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetEditorModeManager.h"

/** Persona-specific extensions to the asset editor mode manager */
class IPersonaEditorModeManager : public FAssetEditorModeManager
{
public:
	/** 
	 * Get a camera target for when the user focuses the viewport
	 * @param OutTarget		The target object bounds
	 * @return true if the location is valid
	 */
	virtual bool GetCameraTarget(FSphere& OutTarget) const = 0;

	/** 
	 * Get debug info for any editor modes that are active
	 * @param	OutDebugText	The text to draw
	 */
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const = 0;
};
