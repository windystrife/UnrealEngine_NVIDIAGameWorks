// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/EnumAsByte.h"
#include "Engine/Texture.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "MediaSampleQueue.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "MediaTexture.generated.h"

class FMediaPlayerFacade;
class FMediaTextureClockSink;
class IMediaTextureSample;
class UMediaPlayer;


/**
 * Implements a texture asset for rendering video tracks from UMediaPlayer assets.
 */
UCLASS(hidecategories=(Adjustments, Compositing, LevelOfDetail, Object))
class MEDIAASSETS_API UMediaTexture
	: public UTexture
{
	GENERATED_UCLASS_BODY()

	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MediaTexture", meta=(DisplayName="X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MediaTexture", meta=(DisplayName="Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<TextureAddress> AddressY;

	/** Whether to clear the texture when no media is being played (default = enabled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MediaTexture")
	bool AutoClear;

	/** The color used to clear the texture if AutoClear is enabled (default = black). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MediaTexture")
	FLinearColor ClearColor;

	/** The media player asset associated with this texture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Media")
	UMediaPlayer* MediaPlayer;

public:

	/**
	 * Gets the current aspect ratio of the texture.
	 *
	 * @return Texture aspect ratio.
	 * @see GetHeight, GetWidth
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	float GetAspectRatio() const;

	/**
	 * Gets the current height of the texture.
	 *
	 * @return Texture height (in pixels).
	 * @see GetAspectRatio, GetWidth
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	int32 GetHeight() const;

	/**
	 * Gets the current width of the texture.
	 *
	 * @return Texture width (in pixels).
	 * @see GetAspectRatio, GetHeight
	 */
	UFUNCTION(BlueprintCallable, Category="Media|MediaTexture")
	int32 GetWidth() const;

public:

	//~ UTexture interface.

	virtual void BeginDestroy() override;
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override;
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual FGuid GetExternalTextureGuid() const override;

public:

	//~ UObject interface.

	virtual FString GetDesc() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	/**
	 * Tick the texture resource.
	 *
	 * @param Timecode The current timecode.
	 */
	void TickResource(FTimespan Timecode);

	/** Unregister the player's external texture GUID. */
	void UnregisterPlayerGuid();

private:

	friend class FMediaTextureClockSink;

	/** The texture's media clock sink. */
	TSharedPtr<FMediaTextureClockSink, ESPMode::ThreadSafe> ClockSink;

	/** The player facade that's currently providing texture samples. */
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> CurrentPlayerFacade;

	/** Current width and height of the resource (in pixels). */
	FIntPoint Dimensions;

	/** The previously used player GUID. */
	FGuid LastPlayerGuid;

	/** Texture sample queue. */
	TSharedPtr<FMediaTextureSampleQueue, ESPMode::ThreadSafe> SampleQueue;

	/** Current size of the resource (in bytes).*/
	SIZE_T Size;
};
