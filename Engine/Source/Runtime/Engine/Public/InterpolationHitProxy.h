// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InterpolationHitProxy.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "Interpolation.h"

/** Input interface hit proxy */
struct HInterpEdInputInterface : public HHitProxy
{
	FInterpEdInputInterface* ClickedObject;
	FInterpEdInputData InputData;

	DECLARE_HIT_PROXY( ENGINE_API );
	HInterpEdInputInterface(FInterpEdInputInterface* InObject, const FInterpEdInputData &InData): HHitProxy(HPP_UI), ClickedObject(InObject), InputData(InData) {}

	/** @return Returns a mouse cursor from the input interface. */
	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return ClickedObject->GetMouseCursor(InputData);
	}
};

struct HInterpTrackKeypointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );

	/** Tracks group */
	class UInterpGroup*		Group;
	/** Track which had a keyframe hit */
	class UInterpTrack*		Track;
	/** Index of hit keyframe */
	int32						KeyIndex;

	HInterpTrackKeypointProxy(class UInterpGroup* InGroup, class UInterpTrack* InTrack, int32 InKeyIndex):
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack),
		KeyIndex(InKeyIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

/** Hit proxy for keyframes drawn directly subgroups and not tracks. */
struct HInterpTrackSubGroupKeypointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );
	
	/* Parent track of the sub group */
	class UInterpTrack*		Track;
	/* Time of the key that was hit */
	float					KeyTime;
	/* SubGroup index */
	int32						GroupIndex;

	HInterpTrackSubGroupKeypointProxy(class UInterpTrack* InTrack, float InKeyTime, int32 InGroupIndex ):
	HHitProxy(HPP_UI),
		Track(InTrack),
		KeyTime(InKeyTime),
		GroupIndex(InGroupIndex)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct HInterpTrackKeyHandleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( ENGINE_API );

	class UInterpGroup*		Group;
	int32						TrackIndex;
	int32						KeyIndex;
	bool					bArriving;

	HInterpTrackKeyHandleProxy(class UInterpGroup* InGroup, int32 InTrackIndex, int32 InKeyIndex, bool bInArriving):
		HHitProxy(HPP_UI),
		Group(InGroup),
		TrackIndex(InTrackIndex),
		KeyIndex(InKeyIndex),
		bArriving(bInArriving)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};


