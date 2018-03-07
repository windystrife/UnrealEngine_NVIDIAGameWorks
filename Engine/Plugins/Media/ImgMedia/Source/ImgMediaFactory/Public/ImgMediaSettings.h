// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "ImgMediaSettings.generated.h"


/**
 * Settings for the ImgMedia module.
 */
UCLASS(config=Engine)
class IMGMEDIAFACTORY_API UImgMediaSettings
	: public UObject
{
	GENERATED_BODY()

public:

	/** Default number of frames per second, if not specified in image sequence or in media source (default = 24.0). */
	UPROPERTY(config, EditAnywhere, Category=General, meta=(ClampMin=0))
	float DefaultFps;

public:

	/** Percentage of cache to use for frames behind the play head (default = 25%). */
	UPROPERTY(config, EditAnywhere, Category=Caching, meta=(ClampMin=0.0, ClampMax=100.0))
	float CacheBehindPercentage;

	/** Maximum size of the look-ahead cache (in GB; default = 1 GB). */
	UPROPERTY(config, EditAnywhere, Category=Caching, meta=(ClampMin=0))
	float CacheSizeGB;

public:

	/** Number of worker threads to use when decoding EXR images (0 = auto). */
	UPROPERTY(config, EditAnywhere, Category=EXR, meta=(ClampMin=0))
	uint32 ExrDecoderThreads;

public:
	 
	/** Default constructor. */
	UImgMediaSettings();

public:

	/**
	 * Get the default media source proxy name tag.
	 *
	 * @return Proxy name tag, or empty string if not set or disabled.
	 */
	FString GetDefaultProxy() const
	{
		if (UseDefaultProxy)
		{
			return DefaultProxy;
		}

		return FString();
	}

private:

	/**
	 * Name of default media source proxy URLs (default = 'proxy').
	 *
	 * Image sequence media sources may contain more than one media source URL. Additional
	 * URLs are called media source proxies, and they are generally used for switching to
	 * lower resolution media content for improved performance during development and testing.
	 *
	 * Each proxy URL has a name associated with it, such as 'proxy', 'lowres', or any
	 * other user defined tag. It is up to the media source to interpret this value and
	 * map it to a media source URL. For example, a media source consisting of a sequence
	 * of uncompressed images may use a proxy name as the name of the sub-directory that
	 * contains proxy content, such as a low-res version of the image sequence.
	 *
	 * When default proxies are enabled via the UseDefaultProxy setting, media players
	 * will first try to locate the proxy content identified by the DefaultProxy tag.
	 * If no such proxy content is available, they will fall back to the media source's
	 * default URL.
	 *
	 * @see UseDefaultProxy
	 */
	UPROPERTY(config, EditAnywhere, Category=Proxies)
	FString DefaultProxy;

	/**
	 * Whether to enable image sequence proxies (default = false).
	 *
	 * Image sequence media sources may contain more than one media source URL. Additional
	 * URLs are called media source proxies, and they are generally used for switching to
	 * lower resolution media content for improved performance during development and testing.
	 *
	 * Each proxy URL has a name associated with it, such as 'proxy', 'lowres', or any
	 * other user defined tag. It is up to the media source to interpret this value and
	 * map it to a media source URL. For example, a media source consisting of a sequence
	 * of uncompressed images may use a proxy name as the name of the sub-directory that
	 * contains proxy content, such as a low-res version of the image sequence.
	 *
	 * When default proxies are enabled via the UseDefaultProxy setting, media players
	 * will first try to locate the proxy content identified by the DefaultProxy tag.
	 * If no such proxy content is available, they will fall back to the media source's
	 * default URL.
	 *
	 * @see DefaultProxy
	 */
	UPROPERTY(config, EditAnywhere, Category=Proxies)
	bool UseDefaultProxy;
};
