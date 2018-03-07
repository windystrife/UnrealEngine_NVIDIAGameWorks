// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"

class UInterpTrack;

/*-----------------------------------------------------------------------------
	Editor-specific hit proxies.
-----------------------------------------------------------------------------*/

struct HMatineeTrackBkg : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HMatineeTrackBkg(): HHitProxy(HPP_UI) {}
};

struct HMatineeGroupTitle : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;

	HMatineeGroupTitle(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

struct HMatineeGroupCollapseBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;

	HMatineeGroupCollapseBtn(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

/** Hit proxy for hitting a collapse button widget on a track with subtracks */
struct HMatineeTrackCollapseBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();
	/* Track that can be collapsed */
	class UInterpTrack* Track;
	/* Index to a subtrack group that was collapsed.  INDEX_NONE if the parent track was collapsed */
	int32 SubTrackGroupIndex;

	HMatineeTrackCollapseBtn(class UInterpTrack* InTrack, int32 InSubTrackGroupIndex = INDEX_NONE ) :
	HHitProxy(HPP_UI),
		Track(InTrack),
		SubTrackGroupIndex( InSubTrackGroupIndex )
	{}
};

struct HMatineeGroupLockCamBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;

	HMatineeGroupLockCamBtn(class UInterpGroup* InGroup) :
		HHitProxy(HPP_UI),
		Group(InGroup)
	{}
};

struct HMatineeTrackTitle : public HHitProxy
{
	DECLARE_HIT_PROXY();
	
	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HMatineeTrackTitle( class UInterpGroup* InGroup, class UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

/** Hit proxy for a subtrack group that was hit */
struct HMatineeSubGroupTitle : public HHitProxy
{
	DECLARE_HIT_PROXY();
	
	/* Track owning the group */
	class UInterpTrack* Track;
	/* Index of the group in the owning track */
	int32 SubGroupIndex;

	HMatineeSubGroupTitle( class UInterpTrack* InTrack, int32 InSubGroupIndex = INDEX_NONE ) :
	HHitProxy(HPP_UI),
		Track( InTrack ),
		SubGroupIndex( InSubGroupIndex )
	{}
};
/**
 * Represents the space in the track viewport associated to a given track. 
 */
struct HMatineeTrackTimeline : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HMatineeTrackTimeline(class UInterpGroup* InGroup, UInterpTrack* InTrack) :
	// Lowering the priority of this hit proxy to wireframe is a bit of a hack. A problem arises 
	// when the white, track position line overlaps with a key. Since the white line has no hit proxy, 
	// the hit proxy detection algorithm with check the area around the click location. The algorithm 
	// chooses the first hit proxy, which is almost always the track viewport hit proxy. Given that the 
	// key proxy was the same priority as the track viewport proxy, the key proxy is never chosen. 
	// I am lowering the priority of the track viewport proxy to compensate. 
		HHitProxy(HPP_Wireframe),
		Group(InGroup),
		Track(InTrack)
	{}
};

struct HMatineeTrackTrajectoryButton : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HMatineeTrackTrajectoryButton(class UInterpGroup* InGroup, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

struct HMatineeTrackGraphPropBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;
	class UInterpTrack* Track;
	int32 SubTrackGroupIndex;

	HMatineeTrackGraphPropBtn(class UInterpGroup* InGroup, int32 InSubTrackGroupIndex, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack),
		SubTrackGroupIndex( InSubTrackGroupIndex )
	{}
};

struct HMatineeTrackDisableTrackBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;
	class UInterpTrack* Track;

	HMatineeTrackDisableTrackBtn(class UInterpGroup* InGroup, UInterpTrack* InTrack ) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		Track(InTrack)
	{}
};

namespace EMatineeEventDirection
{
	enum Type
	{
		IED_Forward,
		IED_Backward
	};
}

struct HMatineeEventDirBtn : public HHitProxy
{
	DECLARE_HIT_PROXY();

	class UInterpGroup* Group;
	int32 TrackIndex;
	EMatineeEventDirection::Type Dir;

	HMatineeEventDirBtn(class UInterpGroup* InGroup, int32 InTrackIndex, EMatineeEventDirection::Type InDir) :
		HHitProxy(HPP_UI),
		Group(InGroup),
		TrackIndex(InTrackIndex),
		Dir(InDir)
	{}
};

struct HMatineeTimelineBkg : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HMatineeTimelineBkg(): HHitProxy(HPP_UI) {}
};

struct HMatineeNavigatorBackground : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HMatineeNavigatorBackground(): HHitProxy(HPP_UI) {}
};

struct HMatineeNavigator : public HHitProxy
{
	DECLARE_HIT_PROXY();
	HMatineeNavigator(): HHitProxy(HPP_UI) {}
};

namespace EMatineeMarkerType
{
	enum Type
	{
		ISM_SeqStart,
		ISM_SeqEnd,
		ISM_LoopStart,
		ISM_LoopEnd
	};
};

struct HMatineeMarker : public HHitProxy
{
	DECLARE_HIT_PROXY();

	EMatineeMarkerType::Type Type;

	HMatineeMarker(EMatineeMarkerType::Type InType) :
		HHitProxy(HPP_UI),
		Type(InType)
	{}

	/**
	 * Displays the cross mouse cursor when hovering over the timeline markers.
	 */
	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};
