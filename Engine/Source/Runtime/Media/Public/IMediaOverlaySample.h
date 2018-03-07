// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "Misc/Timespan.h"


/**
 * Type of text overlay samples.
 */
enum class EMediaOverlaySampleType
{
	/** Caption text for hearing impaired users. */
	Caption,

	/** Subtitle text for non-native speakers. */
	Subtitle,

	/** Generic text. */
	Text,
};


/**
 * Interface for media overlay text samples.
 */
class IMediaOverlaySample
{
public:

	/**
	 * Get the amount of time for which the sample should be displayed.
	 *
	 * @return Sample duration.
	 * @see GetTimecode
	 */
	virtual FTimespan GetDuration() const = 0;

	/**
	 * Get the position at which to display the text.
	 *
	 * @return Display position (relative to top-left corner, in pixels).
	 * @see GetText
	 */
	virtual TOptional<FVector2D> GetPosition() const = 0;

	/**
	 * Get the sample's text.
	 *
	 * @return The overlay text.
	 * @see GetPosition, GetType
	 */
	virtual FText GetText() const = 0;

	/**
	 * Get the sample time (in the player's local clock).
	 *
	 * This value is used primarily for debugging purposes.
	 *
	 * @return Sample time.
	 * @see GetTimecode
	 */
	virtual FTimespan GetTime() const = 0;

	/**
	 * Get the sample type.
	 *
	 * @return Sample type.
	 * @see GetText
	 */
	virtual EMediaOverlaySampleType GetType() const = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaOverlaySample() { }

};
