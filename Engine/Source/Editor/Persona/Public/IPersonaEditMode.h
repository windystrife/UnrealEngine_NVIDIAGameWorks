// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

class IPersonaEditMode : public FEdMode
{
public:
	/** 
	 * Get a camera target used to focus the viewport on an object when a user presses 'F' (default). 
	 * @param	OutTarget	The target object
	 * @return true if the target sphere was filled-in
	 */
	virtual bool GetCameraTarget(FSphere& OutTarget) const = 0;

	/** @return the anim preview scene */
	virtual class IPersonaPreviewScene& GetAnimPreviewScene() const = 0;

	/**
	* Function to collect strings from nodes to display in the viewport.
	* Use this rather than DrawHUD when adding general text to the viewport.
	* Display of this assumes that this will be mostly used by skeletal controls.
	* @param	OutDebugInfo	Text to display
	*/
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugInfo) const = 0;
};
