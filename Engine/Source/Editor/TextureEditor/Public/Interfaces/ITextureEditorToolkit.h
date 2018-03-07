// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneTypes.h"
#include "Toolkits/AssetEditorToolkit.h"

class UTexture;

/**
 * Interface for texture editor tool kits.
 */
class ITextureEditorToolkit
	: public FAssetEditorToolkit
{
public:

	/** Returns the Texture asset being inspected by the Texture editor */
	virtual UTexture* GetTexture() const = 0;

	/** Returns if the Texture asset being inspected has a valid texture resource */
	virtual bool HasValidTextureResource() const = 0;

	/** Refreshes the quick info panel */
	virtual void PopulateQuickInfo() = 0;

	/** Calculates the display size of the texture */
	virtual void CalculateTextureDimensions(uint32& Width, uint32& Height) const = 0;

	/** Accessors */
	virtual int32 GetMipLevel() const = 0;
	virtual ESimpleElementBlendMode GetColourChannelBlendMode() const = 0;
	virtual bool GetUseSpecifiedMip() const = 0;
	virtual double GetZoom() const = 0;
	virtual void SetZoom( double ZoomValue ) = 0;
	virtual void ZoomIn() = 0;
	virtual void ZoomOut() = 0;
	virtual bool GetFitToViewport() const = 0;
	virtual void SetFitToViewport( const bool bFitToViewport ) = 0;
	
public:

	/**
	 * Toggles the fit-to-viewport mode.
	 */
	void ToggleFitToViewport()
	{
		const bool bFitToViewport = GetFitToViewport();
		SetFitToViewport(!bFitToViewport);
	}
};
