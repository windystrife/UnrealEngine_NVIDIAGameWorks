// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateRenderer.h"

class FSlateFontCache;
class FSlateFontMeasure;

/** 
 * A shim around FSlateFontServices that provides access from the render thread (where FSlateApplication::Get() would assert)
 */
class ENGINE_API FEngineFontServices
{
public:
	/** Create the singular instance of this class - must be called from the game thread */
	static void Create();

	/** Destroy the singular instance of this class - must be called from the game thread */
	static void Destroy();

	/** Check to see if the singular instance of this class is currently initialized and ready */
	static bool IsInitialized();

	/** Get the singular instance of this class */
	static FEngineFontServices& Get();

	/** Get the font cache to use for the current thread */
	TSharedPtr<FSlateFontCache> GetFontCache();

	/** Get the font measure to use for the current thread */
	TSharedPtr<FSlateFontMeasure> GetFontMeasure();

	/** Update the cache for the current thread */
	void UpdateCache();

private:
	/** Constructor - must be called from the game thread */
	FEngineFontServices();

	/** Destructor - must be called from the game thread */
	~FEngineFontServices();

	/** Slate font services instance being wrapped */
	TSharedPtr<class FSlateFontServices> SlateFontServices;

	/** Singular instance of this class */
	static FEngineFontServices* Instance;
};
