// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

class UTexture2D;

/*-----------------------------------------------------------------------------
	FInterpEdViewportClient
-----------------------------------------------------------------------------*/

/** Struct for passing commonly used draw params into draw routines */
struct FInterpTrackLabelDrawParams
{
	/** Textures used to represent buttons */
	UTexture2D* CamUnlockedIcon;
	UTexture2D* CamLockedIcon;
	UTexture2D* ForwardEventOnTex;
	UTexture2D* ForwardEventOffTex;
	UTexture2D* BackwardEventOnTex;
	UTexture2D* BackwardEventOffTex;
	UTexture2D* DisableTrackTex;
	UTexture2D* GraphOnTex;
	UTexture2D* GraphOffTex;
	UTexture2D* TrajectoryOnTex;
	/** The color of the group label */
	FColor GroupLabelColor;
	/** The height of the area for drawing tracks */
	int32 TrackAreaHeight;
	/** The number of pixels to indent a track by */
	int32 IndentPixels;
	/** The size of the view in X */	 
	int32 ViewX;
	/** The size of the view in Y */
	int32 ViewY;
	/** The offset in Y from the previous track*/
	int32 YOffset;
	/** True if a track is selected*/
	bool bTrackSelected;
};

/** Struct for containing all information to draw keyframes */
struct FKeyframeDrawInfo
{
	/** Position of the keyframe in the editor*/
	FVector2D KeyPos;
	/** Time of the keyframe */
	float KeyTime;
	/** Color of the keyframe */
	FColor KeyColor;
	/** True if the keyframe is selected */
	bool bSelected;

	/** Comparison routines.  
	 * Two keyframes are said to be equal (for drawing purposes) if they are at the same time
	 * Does not do any tolerance comparison for now.
	 */
	bool operator==( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime == Other.KeyTime;
	}

	bool operator<( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime < Other.KeyTime;
	}

	bool operator>( const FKeyframeDrawInfo& Other ) const
	{
		return KeyTime > Other.KeyTime;
	}

};
