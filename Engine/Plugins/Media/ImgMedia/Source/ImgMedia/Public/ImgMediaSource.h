// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Classes/Engine/EngineTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "BaseMediaSource.h"

#include "ImgMediaSource.generated.h"


/**
 * Media source for EXR image sequences.
 *
 * Image sequence media sources point to a directory that contains a series of
 * image files in which each image represents a single frame of the sequence.
 * BMP, EXR, PNG and JPG images are currently supported. EXR image sequences
 * are optimized for performance. The first frame of an image sequence is used
 * to determine the image dimensions (all formats) and frame rate (EXR only).
 *
 * The image sequence directory may contain sub-directories, which are called
 * 'proxies'. Proxies can be used to provide alternative media for playback
 * during development and testing of a game. One common scenario is the use
 * of low resolution versions of image sequence media on computers that are
 * too slow or don't have enough storage to play the original high-res media.
 */
UCLASS(BlueprintType, hidecategories=(Overrides, Playback))
class IMGMEDIA_API UImgMediaSource
	: public UBaseMediaSource
{
	GENERATED_BODY()

public:

	/** Default constructor. */
	UImgMediaSource();

public:

	/** Overrides the default frame rate stored in the image files (0.0 = do not override). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Sequence, AdvancedDisplay)
	float FramesPerSecondOverride;

	/** Name of the proxy directory to use. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Sequence, AdvancedDisplay)
	FString ProxyOverride;

public:

	/**
	 * Get the names of available proxy directories.
	 *
	 * @param OutProxies Will contain the names of available proxy directories, if any.
	 * @see GetSequencePath
	 */
	UFUNCTION(BlueprintCallable, Category="ImgMedia|ImgMediaSource")
	void GetProxies(TArray<FString>& OutProxies) const;

	/**
	 * Get the path to the image sequence directory to be played.
	 *
	 * @return The file path.
	 * @see SetSequencePath
	 */
	UFUNCTION(BlueprintCallable, Category="ImgMedia|ImgMediaSource")
	const FString GetSequencePath() const
	{
		return SequencePath.Path;
	}

	/**
	 * Set the path to the image sequence directory this source represents.
	 *
	 * @param Path The path to set.
	 * @see GetSequencePath
	 */
	UFUNCTION(BlueprintCallable, Category="ImgMedia|ImgMediaSource")
	void SetSequencePath(const FString& Path);

public:

	//~ IMediaOptions interface

	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

protected:

	/** Get the full path to the image sequence. */
	FString GetFullPath() const;

protected:

	/** The directory that contains the image sequence files. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category=Sequence)
	FDirectoryPath SequencePath;
};
