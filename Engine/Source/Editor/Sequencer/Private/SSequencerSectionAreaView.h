// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "SSequencerSection.h"

class FArrangedChildren;
class FPaintArgs;
class FSequencer;
class FSlateWindowElementList;

/**
 * Visualizes a section area and its children                                        
 */
class SSequencerSectionAreaView : public SPanel
{
public:
	SLATE_BEGIN_ARGS( SSequencerSectionAreaView )
	{}		
		/** The view range of the section area */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node );

private:
	/** SWidget Interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** SPanel Interface */
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override { return &Children; }
	
	/**
	 * Returns the visibility of a section
	 *
	 * @param SectionObject The section to check for selection
	 * @return The visibility of the section
	 */
	EVisibility GetSectionVisibility( UMovieSceneSection* SectionObject ) const;

	/**
	 * Calculates a time to pixel converter from the allotted geometry
	 *
	 * @param AllottedGeometry	The geometry to use to convert from time to pixels or vice versa
	 * @return A converter used to convert between time and pixels 
	 */
	struct FTimeToPixel GetTimeToPixel( const FGeometry& AllottedGeometry ) const;

	/**
	 * Generates section widgets inside this section area
	 */
	void GenerateSectionWidgets();

	/** @return The sequencer interface */
	FSequencer& GetSequencer() const;
private:
	/** The node containing the sections we are viewing/manipulating */
	TSharedPtr<FSequencerTrackNode> SectionAreaNode;
	/** The current view range */
	TAttribute< TRange<float> > ViewRange;
	/** All the widgets in the panel */
	TSlotlessChildren<SSequencerSection> Children;
};
