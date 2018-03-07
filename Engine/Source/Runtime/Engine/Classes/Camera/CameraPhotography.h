// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class APlayerCameraManager;
class ICameraPhotography;
struct FMinimalViewInfo;

/**
 * Free-camera photography manager
 */
class APlayerCameraManager;

class FCameraPhotographyManager
{
public:
	/** Get (& possibly create) singleton FCameraPhotography */
	ENGINE_API static FCameraPhotographyManager& Get();
	/** Destroy current FCameraPhotography (if any); recreated by next Get() */
	ENGINE_API static void Destroy();
	/** @return Returns false if definitely unavailable at compile-time or run-time */
	ENGINE_API static bool IsSupported(UWorld* InWorld);

	/** Modify input camera according to cumulative free-camera transforms (if any).
	* Safe to call this even if IsSupported()==false, in which case it will leave camera
	* unchanged and return false.
	* @param InOutPOV - camera info to modify
	* @param PCMgr - player camera manager (non-NULL)
	* @return Returns whether camera was cut/non-contiguous/teleported */
	ENGINE_API bool UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr);

protected:
	FCameraPhotographyManager();
	~FCameraPhotographyManager();
	static class FCameraPhotographyManager* Singleton;

	TSharedPtr<ICameraPhotography> ActiveImpl;
};


