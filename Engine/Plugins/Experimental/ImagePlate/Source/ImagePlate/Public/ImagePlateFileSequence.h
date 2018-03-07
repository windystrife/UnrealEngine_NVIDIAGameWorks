// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Future.h"
#include "ImagePlateFileSequence.generated.h"

class UTexture;


struct FImagePlateAsyncCache;
namespace ImagePlateFrameCache { struct FImagePlateSequenceCache; }

/**
* Implements the settings for the ImagePlate plugin.
*/
UCLASS(config=Engine, defaultconfig)
class IMAGEPLATE_API UImagePlateSettings : public UObject
{
public:
	GENERATED_BODY()

	/** Specifies a sub-directory to append to any image plate file sequences */
	UPROPERTY(GlobalConfig, EditAnywhere, config, Category=Settings)
	FString ProxyName;
};


UCLASS()
class IMAGEPLATE_API UImagePlateFileSequence : public UObject
{
public:
	GENERATED_BODY()

	UImagePlateFileSequence(const FObjectInitializer& Init);

	/** Path to the directory in which the image sequence resides */
	UPROPERTY(EditAnywhere, Category="General", meta=(ContentDir))
	FDirectoryPath SequencePath;

	/** Wildcard used to find images within the directory (ie *.exr) */
	UPROPERTY(EditAnywhere, Category="General")
	FString FileWildcard;

	/** Framerate at which to display the images */
	UPROPERTY(EditAnywhere, Category="General", meta=(ClampMin=0))
	float Framerate;

public:

	/** Create a new image cache for this sequence */
	FImagePlateAsyncCache GetAsyncCache();
};

/** Uncompressed source data for a single frame of a sequence */
struct IMAGEPLATE_API FImagePlateSourceFrame
{
	/** Default constructor */
	FImagePlateSourceFrame();
	/** Construction from an array of data, and a given width/height/bitdepth */
	FImagePlateSourceFrame(const TArray<uint8>& InData, uint32 InWidth, uint32 InHeight, uint32 InBitDepth);

	/** Check whether this source frame has valid data */
	bool IsValid() const;

	/** Copy the contents of this frame to the specified texture */
	TFuture<void> CopyTo(UTexture* DestinationTexture);

private:

	/** Ensure the specified texture metrics match this frame */
	bool EnsureTextureMetrics(UTexture* DestinationTexture) const;

	/** Metrics for the texture */
	uint32 Width, Height, BitDepth, Pitch;

	/** Threadsafe, shared data buffer. Shared so that this type can be copied around without incurring a copy-cost for large frames. */
	TSharedPtr<uint8, ESPMode::ThreadSafe> Buffer;
};

/** A wrapper for an asynchronous cache of image frames */
struct IMAGEPLATE_API FImagePlateAsyncCache
{
	/** Make a new cache for the specified folder, wildcard and framerate */
	static FImagePlateAsyncCache MakeCache(const FString& InSequencePath, const FString& InWildcard, float Framerate);

	/** Request a frame of data from the cache, whilst also caching leading and trailing frames if necessary */
	TSharedFuture<FImagePlateSourceFrame> RequestFrame(float Time, int32 LeadingPrecacheFrames, int32 TrailingPrecacheFrames);

	/** Get the length of the sequence in frames */
	int32 Length() const;

private:
	FImagePlateAsyncCache(){}

	/** Shared implementation */
	TSharedPtr<ImagePlateFrameCache::FImagePlateSequenceCache, ESPMode::ThreadSafe> Impl;
};
