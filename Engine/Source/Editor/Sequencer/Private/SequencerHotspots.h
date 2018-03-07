// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "SequencerSelectedKey.h"
#include "ISequencerHotspot.h"

class FMenuBuilder;
class ISequencer;
class ISequencerEditToolDragOperation;
class FSequencerTrackNode;
class ISequencerSection;

enum class ESequencerEasingType
{
	In, Out
};

/** A hotspot representing a key */
struct FKeyHotspot
	: ISequencerHotspot
{
	FKeyHotspot(FSequencerSelectedKey InKey)
		: Key(InKey)
	{ }

	virtual ESequencerHotspot GetType() const override { return ESequencerHotspot::Key; }
	virtual void UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const override;
	virtual TOptional<float> GetTime() const override;
	virtual bool PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;

	/** The key itself */
	FSequencerSelectedKey Key;
};


/** Structure used to encapsulate a section and its track node */
struct FSectionHandle
{
	FSectionHandle(TSharedPtr<FSequencerTrackNode> InTrackNode, int32 InSectionIndex)
		: SectionIndex(InSectionIndex), TrackNode(MoveTemp(InTrackNode))
	{ }

	friend bool operator==(const FSectionHandle& A, const FSectionHandle& B)
	{
		return A.SectionIndex == B.SectionIndex && A.TrackNode == B.TrackNode;
	}
	
	SEQUENCER_API TSharedRef<ISequencerSection> GetSectionInterface() const;

	SEQUENCER_API UMovieSceneSection* GetSectionObject() const;

	int32 SectionIndex;
	TSharedPtr<FSequencerTrackNode> TrackNode;
};


/** A hotspot representing a section */
struct FSectionHotspot
	: ISequencerHotspot
{
	FSectionHotspot(FSectionHandle InSection)
		: Section(InSection)
	{ }

	virtual ESequencerHotspot GetType() const override { return ESequencerHotspot::Section; }
	virtual void UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const override;
	virtual TOptional<float> GetTime() const override;
	virtual TOptional<float> GetOffsetTime() const override;
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer&) override { return nullptr; }
	virtual bool PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;

	/** Handle to the section */
	FSectionHandle Section;
};


/** A hotspot representing a resize handle on a section */
struct FSectionResizeHotspot
	: ISequencerHotspot
{
	enum EHandle
	{
		Left,
		Right
	};

	FSectionResizeHotspot(EHandle InHandleType, FSectionHandle InSection) : Section(InSection), HandleType(InHandleType) {}

	virtual ESequencerHotspot GetType() const override { return HandleType == Left ? ESequencerHotspot::SectionResize_L : ESequencerHotspot::SectionResize_R; }
	virtual void UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const override;
	virtual TOptional<float> GetTime() const override;
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer& Sequencer) override;
	virtual FCursorReply GetCursor() const { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }

	/** Handle to the section */
	FSectionHandle Section;

private:

	EHandle HandleType;
};


/** A hotspot representing a resize handle on a section's easing */
struct FSectionEasingHandleHotspot
	: ISequencerHotspot
{
	FSectionEasingHandleHotspot(ESequencerEasingType InHandleType, FSectionHandle InSection) : Section(InSection), HandleType(InHandleType) {}

	virtual ESequencerHotspot GetType() const override { return HandleType == ESequencerEasingType::In ? ESequencerHotspot::EaseInHandle : ESequencerHotspot::EaseOutHandle; }
	virtual void UpdateOnHover(SSequencerTrackArea& InTrackArea, ISequencer& InSequencer) const override;
	virtual bool PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;
	virtual TOptional<float> GetTime() const override;
	virtual TSharedPtr<ISequencerEditToolDragOperation> InitiateDrag(ISequencer& Sequencer) override;
	virtual FCursorReply GetCursor() const { return FCursorReply::Cursor( EMouseCursor::ResizeLeftRight ); }

	/** Handle to the section */
	FSectionHandle Section;

private:

	ESequencerEasingType HandleType;
};


struct FEasingAreaHandle
{
	FSectionHandle Section;
	ESequencerEasingType EasingType;
};

/** A hotspot representing an easing area for multiple sections */
struct FSectionEasingAreaHotspot
	: FSectionHotspot
{
	FSectionEasingAreaHotspot(const TArray<FEasingAreaHandle>& InEasings, FSectionHandle InVisibleSection) : FSectionHotspot(InVisibleSection), Easings(InEasings) {}

	virtual ESequencerHotspot GetType() const override { return ESequencerHotspot::EasingArea; }
	virtual bool PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& Sequencer, float MouseDownTime) override;

	bool Contains(FSectionHandle InSection) const { return Easings.ContainsByPredicate([=](const FEasingAreaHandle& InHandle){ return InHandle.Section == InSection; }); }

	/** Handles to the easings that exist on this hotspot */
	TArray<FEasingAreaHandle> Easings;
};