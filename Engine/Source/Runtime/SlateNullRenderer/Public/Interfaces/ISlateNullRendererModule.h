// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Fonts/FontTypes.h"
#include "Rendering/SlateRenderer.h"

/**
 * Interface for the Slate RHI Renderer module.
 */
class ISlateNullRendererModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a Slate RHI renderer.
	 *
	 * @return A new renderer which renders nothing.
	 */
	virtual TSharedRef<FSlateRenderer> CreateSlateNullRenderer( ) = 0;

	virtual TSharedRef<ISlateFontAtlasFactory> CreateSlateFontAtlasFactory() = 0;
};
