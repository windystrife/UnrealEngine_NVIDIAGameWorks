// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "SequencerSelectedKey.h"
#include "SequencerHotspots.h"
#include "CommonMovieSceneTools.h"

class FSequencer;
class FSequencerDisplayNode;
class SSequencerTreeView;

/** Structure used for handling the virtual space of the track area */
class SEQUENCER_API FVirtualTrackArea : public FTimeToPixel
{
public:

	/** Construction responsibility is delegated to SSequencer. See SSequencer::GetVirtualTrackArea */
	FVirtualTrackArea(const FSequencer& InSequencer, SSequencerTreeView& InTreeView, const FGeometry& InTrackAreaGeometry);

	/** Convert the specified pixel position into a virtual vertical offset from the absolute top of the tree */
	float PixelToVerticalOffset(float InPixel) const;

	/** Convert the specified absolute vertical position into a physical vertical offset in the track area. */
	/** @note: Use with caution - not reliable where the specified offset is not on screen */
	float VerticalOffsetToPixel(float InOffset) const;

	/** Convert the specified physical point into a virtual point the absolute top of the tree */
	FVector2D PhysicalToVirtual(FVector2D InPosition) const;

	/** Convert the specified absolute virtual point into a physical point in the track area. */
	/** @note: Use with caution - not reliable where the specified point is not on screen */
	FVector2D VirtualToPhysical(FVector2D InPosition) const;

	/** Get the physical size of the track area */
	FVector2D GetPhysicalSize() const;

	/** Hit test at the specified physical position for a sequencer node */
	TSharedPtr<FSequencerDisplayNode> HitTestNode(float InPhysicalPosition) const;

	/** Hit test at the specified physical position for a section */
	TOptional<FSectionHandle> HitTestSection(FVector2D InPhysicalPosition) const;

	/** Hit test at the specified physical position for a key */
	FSequencerSelectedKey HitTestKey(FVector2D InPhysicalPosition) const;

	/** Cached track area geometry */
	FGeometry CachedTrackAreaGeometry() const { return TrackAreaGeometry; }

private:

	/** Reference to the sequencer tree */
	SSequencerTreeView& TreeView;

	/** Cached physical geometry of the track area */
	FGeometry TrackAreaGeometry;
};
