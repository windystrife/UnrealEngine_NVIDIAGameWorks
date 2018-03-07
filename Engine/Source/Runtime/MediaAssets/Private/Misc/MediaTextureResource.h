// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Queue.h"
#include "Math/Color.h"
#include "MediaSampleSource.h"
#include "Misc/Guid.h"
#include "Misc/Timespan.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "TextureResource.h"
#include "UnrealClient.h"

class FMediaPlayerFacade;
class IMediaPlayer;
class IMediaTextureSample;
class UMediaTexture;

enum class EMediaTextureSinkFormat;
enum class EMediaTextureSinkMode;


/**
 * Texture resource type for media textures.
 */
class FMediaTextureResource
	: public FRenderTarget
	, public FTextureResource
{
public:

	/** 
	 * Creates and initializes a new instance.
	 *
	 * @param InOwner The Movie texture object to create a resource for (must not be nullptr).
	 * @param InSink The sink that receives texture samples from the media player.
	 */
	FMediaTextureResource(UMediaTexture& InOwner, FIntPoint& InOwnerDim, SIZE_T& InOwnerSize);

	/** Virtual destructor. */
	virtual ~FMediaTextureResource() { }

public:

	/** Parameters for the Render method. */
	struct FRenderParams
	{
		/** The clear color to use when clearing the texture. */
		FLinearColor ClearColor;

		/** Guid associated with media player. */
		FGuid PlayerGuid;

		/** The player's play rate. */
		float Rate;

		/** The player facade that provides the video samples to render. */
		TWeakPtr<FMediaTextureSampleSource, ESPMode::ThreadSafe> SampleSource;

		/** Whether output should be in sRGB color space. */
		bool SrgbOutput;

		/** The time of the video frame to render (in player's clock). */
		FTimespan Time;
	};

	/**
	 * Render the texture resource.
	 *
	 * This method is called on the render thread by the MediaTexture that owns this
	 * texture resource to clear or redraw the resource using the given parameters.
	 *
	 * @param Params Render parameters.
	 */
	void Render(const FRenderParams& Params);

public:

	//~ FRenderTarget interface

	virtual FIntPoint GetSizeXY() const override;

public:

	//~ FTextureResource interface

	virtual FString GetFriendlyName() const override;
	virtual uint32 GetSizeX() const override;
	virtual uint32 GetSizeY() const override;
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

protected:

	/**
	 * Clear the texture using the given clear color.
	 *
	 * @param ClearColor The clear color to use.
	 * @param SrgbOutput Whether the output texture is in sRGB color space.
	 */
	void ClearTexture(const FLinearColor& ClearColor, bool SrgbOutput);

	/**
	 * Render the given texture sample by converting it on the GPU.
	 *
	 * @param Sample The texture sample to convert.
	 * @param ClearColor The clear color to use for the output texture.
	 * @param SrgbOutput Whether the output texture is in sRGB color space.
	 * @see CopySample
	 */
	void ConvertSample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, bool SrgbOutput);

	/**
	 * Render the given texture sample by using it as or copying it to the render target.
	 *
	 * @param Sample The texture sample to copy.
	 * @param ClearColor The clear color to use for the output texture.
	 * @param SrgbOutput Whether the output texture is in sRGB color space.
	 * @see ConvertSample
	 */
	void CopySample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, bool SrgbOutput);

	/** Calculates the current resource size and notifies the owner texture. */
	void UpdateResourceSize();

	/**
	 * Set the owner's texture reference to the given texture.
	 *
	 * @param NewTexture The texture to set.
	 */
	void UpdateTextureReference(FRHITexture2D* NewTexture);

private:

	/** Whether the texture has been cleared. */
	bool Cleared;

	/** Tracks the current clear color. */
	FLinearColor CurrentClearColor;

	/** Input render target if the texture samples don't provide one (for conversions). */
	TRefCountPtr<FRHITexture2D> InputTarget;

	/** Output render target if the texture samples don't provide one. */
	TRefCountPtr<FRHITexture2D> OutputTarget;

	/** The media texture that owns this resource. */
	UMediaTexture& Owner;

	/** Reference to the owner's texture dimensions field. */
	FIntPoint& OwnerDim;

	/** Reference to the owner's texture size field. */
	SIZE_T& OwnerSize;

	/** The current media player facade to get video samples from. */
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> PlayerFacadePtr;
};
