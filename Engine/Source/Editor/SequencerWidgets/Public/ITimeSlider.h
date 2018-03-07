// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/CursorReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Editor/Sequencer/Public/ISequencerInputHandler.h"
#include "IMovieScenePlayer.h"

class FSlateWindowElementList;
class USequencerSettings;

/** Enum specifying how to interpolate to a new view range */
enum class EViewRangeInterpolation
{
	/** Use an externally defined animated interpolation */
	Animated,
	/** Set the view range immediately */
	Immediate,
};

DECLARE_DELEGATE_TwoParams( FOnScrubPositionChanged, float, bool )
DECLARE_DELEGATE_TwoParams( FOnViewRangeChanged, TRange<float>, EViewRangeInterpolation )
DECLARE_DELEGATE_OneParam( FOnRangeChanged, TRange<float> )
DECLARE_DELEGATE_RetVal_OneParam( float, FOnGetNearestKey, float )

/** Structure used to wrap up a range, and an optional animation target */
struct FAnimatedRange : public TRange<float>
{
	/** Default Construction */
	FAnimatedRange() : TRange() {}
	/** Construction from a lower and upper bound */
	FAnimatedRange( float LowerBound, float UpperBound ) : TRange( LowerBound, UpperBound ) {}
	/** Copy-construction from simple range */
	FAnimatedRange( const TRange<float>& InRange ) : TRange(InRange) {}

	/** Helper function to wrap an attribute to an animated range with a non-animated one */
	static TAttribute<TRange<float>> WrapAttribute( const TAttribute<FAnimatedRange>& InAttribute )
	{
		typedef TAttribute<TRange<float>> Attr;
		return Attr::Create(Attr::FGetter::CreateLambda([=](){ return InAttribute.Get(); }));
	}

	/** Helper function to wrap an attribute to a non-animated range with an animated one */
	static TAttribute<FAnimatedRange> WrapAttribute( const TAttribute<TRange<float>>& InAttribute )
	{
		typedef TAttribute<FAnimatedRange> Attr;
		return Attr::Create(Attr::FGetter::CreateLambda([=](){ return InAttribute.Get(); }));
	}

	/** Get the current animation target, or the whole view range when not animating */
	const TRange<float>& GetAnimationTarget() const
	{
		return AnimationTarget.IsSet() ? AnimationTarget.GetValue() : *this;
	}
	
	/** The animation target, if animating */
	TOptional<TRange<float>> AnimationTarget;
};

struct FTimeSliderArgs
{
	FTimeSliderArgs()
		: ScrubPosition(0)
		, ViewRange( FAnimatedRange(0.0f, 5.0f) )
		, ClampRange( FAnimatedRange(-FLT_MAX/2.f, FLT_MAX/2.f) )
		, OnScrubPositionChanged()
		, OnBeginScrubberMovement()
		, OnEndScrubberMovement()
		, OnViewRangeChanged()
		, OnClampRangeChanged()
		, OnGetNearestKey()
		, AllowZoom(true)
		, Settings(nullptr)
	{}

	/** The scrub position */
	TAttribute<float> ScrubPosition;

	/** View time range */
	TAttribute< FAnimatedRange > ViewRange;

	/** Clamp time range */
	TAttribute< FAnimatedRange > ClampRange;

	/** Called when the scrub position changes */
	FOnScrubPositionChanged OnScrubPositionChanged;

	/** Called right before the scrubber begins to move */
	FSimpleDelegate OnBeginScrubberMovement;

	/** Called right after the scrubber handle is released by the user */
	FSimpleDelegate OnEndScrubberMovement;

	/** Called when the view range changes */
	FOnViewRangeChanged OnViewRangeChanged;

	/** Called when the clamp range changes */
	FOnRangeChanged OnClampRangeChanged;

	/** Delegate that is called when getting the nearest key */
	FOnGetNearestKey OnGetNearestKey;

	/** Attribute defining the active sub-sequence range for this controller */
	TAttribute<TOptional<TRange<float>>> SubSequenceRange;

	/** Attribute defining the playback range for this controller */
	TAttribute<TRange<float>> PlaybackRange;

	/** Delegate that is called when the playback range wants to change */
	FOnRangeChanged OnPlaybackRangeChanged;

	/** Called right before the playback range starts to be dragged */
	FSimpleDelegate OnPlaybackRangeBeginDrag;

	/** Called right after the playback range has finished being dragged */
	FSimpleDelegate OnPlaybackRangeEndDrag;

	/** Attribute defining the selection range for this controller */
	TAttribute<TRange<float>> SelectionRange;

	/** Delegate that is called when the selection range wants to change */
	FOnRangeChanged OnSelectionRangeChanged;

	/** Called right before the selection range starts to be dragged */
	FSimpleDelegate OnSelectionRangeBeginDrag;

	/** Called right after the selection range has finished being dragged */
	FSimpleDelegate OnSelectionRangeEndDrag;
	/** Round the scrub position to an integer during playback */
	TAttribute<EMovieScenePlayerStatus::Type> PlaybackStatus;

	/** Attribute defining whether the playback range is locked */
	TAttribute<bool> IsPlaybackRangeLocked;

	/** Attribute defining the time snap interval */
	TAttribute<float> TimeSnapInterval;

	/** Called when toggling the playback range lock */
	FSimpleDelegate OnTogglePlaybackRangeLocked;

	/** If we are allowed to zoom */
	bool AllowZoom;

	/** User-supplied settings object */
	USequencerSettings* Settings;
};

class ITimeSliderController : public ISequencerInputHandler
{
public:
	virtual ~ITimeSliderController(){}
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const = 0;
	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const = 0;

	/** Get the current view range for this controller */
	virtual FAnimatedRange GetViewRange() const { return FAnimatedRange(); }

	/** Get the current clamp range for this controller */
	virtual FAnimatedRange GetClampRange() const { return FAnimatedRange(); }

	/** Get the current play range for this controller */
	virtual TRange<float> GetPlayRange() const { return TRange<float>(); }

	/** Convert a time to a frame */
	virtual int32 TimeToFrame(float Time) const { return 1; }

	/** Convert a time to a frame */
	virtual float FrameToTime(int32 Frame) const { return 1.f; }

	/**
	 * Set a new range based on a min, max and an interpolation mode
	 * 
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 * @param Interpolation		How to set the new range (either immediately, or animated)
	 */
	virtual void SetViewRange( float NewRangeMin, float NewRangeMax, EViewRangeInterpolation Interpolation ) {}

	/**
	 * Set a new clamp range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the clamp range
	 * @param NewRangeMax		The new upper bound of the clamp range
	 */
	virtual void SetClampRange( float NewRangeMin, float NewRangeMax) {}

	/**
	 * Set a new playback range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the playback range
	 * @param NewRangeMax		The new upper bound of the playback range
	 */
	virtual void SetPlayRange( float NewRangeMin, float NewRangeMax ) {}
};

/**
 * Base class for a widget that scrubs time or frames
 */
class ITimeSlider : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(ITimeSlider){}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
};
