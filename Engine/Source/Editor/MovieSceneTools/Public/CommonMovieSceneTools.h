// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

/**
 * Utility for converting time units to slate pixel units and vice versa
 */
struct FTimeToPixel
{
public:
	FTimeToPixel( const FGeometry& AllottedGeometry, TRange<float> InLocalViewRange )
		: MaxPixelsPerInput(1000.0f)
		, LocalViewRange( InLocalViewRange )
	{
		const float ViewRange = LocalViewRange.Size<float>();
		PixelsPerInput = ViewRange > 0 ? AllottedGeometry.GetLocalSize().X / ViewRange : MaxPixelsPerInput;
	}

	/**
	 * Converts a time to a pixel point relative to the geometry of a widget (passed into the constructor)
	 *
	 * @param Time	The time to convert
	 * @return The pixel equivalent of the time
	 */
	float TimeToPixel( float Time ) const
	{
		return (Time - LocalViewRange.GetLowerBoundValue()) * PixelsPerInput;
	}

	/**
	 * Converts a pixel value to time 
	 *
	 * @param PixelX The x value of a pixel coordinate relative to the geometry that was passed into the constructor.
	 * @return The time where the pixel is located
	 */
	float PixelToTime( float PixelX ) const
	{
		return (PixelX/PixelsPerInput) + LocalViewRange.GetLowerBoundValue();
	}

	/**
	 * @return The pixels per input
	 */
	float GetPixelsPerInput() const 
	{
		return PixelsPerInput;
	}
private:
	const float MaxPixelsPerInput;

	/** time range of the sequencer */
	TRange<float> LocalViewRange;
	/** The number of pixels in the view range */
	float PixelsPerInput;
};
