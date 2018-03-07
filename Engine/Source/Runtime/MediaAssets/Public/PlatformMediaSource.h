// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "MediaSource.h"

#include "PlatformMediaSource.generated.h"


/**
 * A media source that selects other media sources based on target platform.
 *
 * Use this asset to override media sources on a per-platform basis.
 */
UCLASS(BlueprintType)
class MEDIAASSETS_API UPlatformMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	/** Media sources per platform. */
	UPROPERTY(EditAnywhere, Category=Sources, Meta=(DisplayName="Media Sources"))
	TMap<FString, UMediaSource*> PlatformMediaSources;

#endif

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool Validate() const override;

public:

	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

private:

	/**
	 * Get the media source for the running target platform.
	 *
	 * @return The media source, or nullptr if not set.
	 */
	UMediaSource* GetMediaSource() const;

private:

	/**
	 * Default media source.
	 *
	 * This media source will be used if no source was specified for a target platform.
	 */
	UPROPERTY()
	UMediaSource* MediaSource;
};
