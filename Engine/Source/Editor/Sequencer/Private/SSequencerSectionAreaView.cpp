// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerSectionAreaView.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "CommonMovieSceneTools.h"

namespace SequencerSectionAreaConstants
{

	/** Background color of section areas */
	const FLinearColor BackgroundColor( .1f, .1f, .1f, 0.5f );
}

namespace SequencerSectionUtils
{
	/**
	 * Gets the geometry of a section, optionally inflated by some margin
	 *
	 * @param AllottedGeometry		The geometry of the area where sections are located
	 * @param NodeHeight			The height of the section area (and its children)
	 * @param SectionInterface		Interface to the section to get geometry for
	 * @param TimeToPixelConverter  Converts time to pixels and vice versa
	 */
	FGeometry GetSectionGeometry( const FGeometry& AllottedGeometry, int32 RowIndex, int32 MaxTracks, float NodeHeight, TSharedPtr<ISequencerSection> SectionInterface, const FTimeToPixel& TimeToPixelConverter )
	{
		const UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		float PixelStartX = TimeToPixelConverter.TimeToPixel( Section->GetStartTime() );
		// Note the -1 pixel at the end is because the section does not actually end at the end time if there is a section starting at that same time.  It is more important that a section lines up correctly with it's true start time
		float PixelEndX = TimeToPixelConverter.TimeToPixel( Section->GetEndTime() );

		// If the section is infinite, occupy the entire width of the geometry where the section is located.
		const bool bIsInfinite = Section->IsInfinite();
		if (bIsInfinite)
		{
			PixelStartX = AllottedGeometry.Position.X;
			PixelEndX = AllottedGeometry.Position.X + AllottedGeometry.GetLocalSize().X;
		}

		const float MinSectionWidth = 1.f;
		float SectionLength = FMath::Max(MinSectionWidth, PixelEndX-PixelStartX);

		float GripOffset = 0;
		if (!bIsInfinite)
		{
			float NewSectionLength = FMath::Max(SectionLength, MinSectionWidth + SectionInterface->GetSectionGripSize() * 2.f);

			GripOffset = (NewSectionLength - SectionLength) / 2.f;
			SectionLength = NewSectionLength;
		}

		float ActualHeight = NodeHeight / MaxTracks;

		// Compute allotted geometry area that can be used to draw the section
		return AllottedGeometry.MakeChild(
			FVector2D( PixelStartX-GripOffset, ActualHeight * RowIndex ),
			FVector2D( SectionLength, ActualHeight ) );
	}

}


void SSequencerSectionAreaView::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node )
{
	ViewRange = InArgs._ViewRange;

	check( Node->GetType() == ESequencerNode::Track );
	SectionAreaNode = StaticCastSharedRef<FSequencerTrackNode>( Node );

	// Generate widgets for sections in this view
	GenerateSectionWidgets();
};

FVector2D SSequencerSectionAreaView::ComputeDesiredSize(float) const
{
	// Note: X Size is not used
	FVector2D Size(100, 0.f);
	if (Children.Num())
	{
		for (int32 Index = 0; Index < Children.Num(); ++Index)
		{
			Size.Y = FMath::Max(Size.Y, Children[Index]->GetDesiredSize().Y);
		}
	}
	else
	{
		Size.Y = SectionAreaNode->GetNodeHeight();
	}
	return Size;
}

void SSequencerSectionAreaView::GenerateSectionWidgets()
{
	Children.Empty();

	if( SectionAreaNode.IsValid() )
	{
		TArray< TSharedRef<ISequencerSection> >& Sections = SectionAreaNode->GetSections();

		for ( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
		{
			Children.Add( 
				SNew( SSequencerSection, SectionAreaNode.ToSharedRef(), SectionIndex ) 
				.Visibility( this, &SSequencerSectionAreaView::GetSectionVisibility, Sections[SectionIndex]->GetSectionObject() )
				);
		}
	}
}

EVisibility SSequencerSectionAreaView::GetSectionVisibility( UMovieSceneSection* SectionObject ) const
{
	return EVisibility::Visible;
}


/** SWidget Interface */
int32 SSequencerSectionAreaView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		FSlateRect ChildClipRect = MyCullingRect.IntersectionWith( CurWidget.Geometry.GetLayoutBoundingRect() );
		LayerId = CurWidget.Widget->Paint( Args.WithNewParent(this), CurWidget.Geometry, ChildClipRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}

	return LayerId + 1;
}

void SSequencerSectionAreaView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (Children.Num())
	{
		StableSort(&Children[0], Children.Num(), [](const TSharedRef<SSequencerSection>& A, const TSharedRef<SSequencerSection>& B){
			UMovieSceneSection* SectionA = A->GetSectionInterface()->GetSectionObject();
			UMovieSceneSection* SectionB = B->GetSectionInterface()->GetSectionObject();

			return SectionA && SectionB && SectionA->GetOverlapPriority() < SectionB->GetOverlapPriority();
		});

		for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
		{
			Children[WidgetIndex]->CacheParentGeometry(AllottedGeometry);
		}
	}
}

void SSequencerSectionAreaView::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	int32 MaxRowIndex = 0;
	if (SectionAreaNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::None)
	{
		for (int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex)
		{
			const TSharedRef<SSequencerSection>& Widget = Children[WidgetIndex];

			TSharedPtr<ISequencerSection> SectionInterface = Widget->GetSectionInterface();

			MaxRowIndex = FMath::Max(MaxRowIndex, SectionInterface->GetSectionObject()->GetRowIndex());
		}
	}
	int32 MaxTracks = MaxRowIndex + 1;

	FTimeToPixel TimeToPixelConverter = GetTimeToPixel( AllottedGeometry );

	for( int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex )
	{
		const TSharedRef<SSequencerSection>& Widget = Children[WidgetIndex];

		TSharedPtr<ISequencerSection> SectionInterface = Widget->GetSectionInterface();

		int32 RowIndex = SectionAreaNode->GetSubTrackMode() == FSequencerTrackNode::ESubTrackMode::None ? SectionInterface->GetSectionObject()->GetRowIndex() : 0;

		EVisibility WidgetVisibility = Widget->GetVisibility();
		if( ArrangedChildren.Accepts( WidgetVisibility ) )
		{
			FGeometry SectionGeometry = SequencerSectionUtils::GetSectionGeometry( AllottedGeometry, RowIndex, MaxTracks, Widget->GetDesiredSize().Y, SectionInterface, TimeToPixelConverter );
			
			ArrangedChildren.AddWidget( 
				WidgetVisibility, 
				AllottedGeometry.MakeChild( Widget, SectionGeometry.Position, SectionGeometry.GetLocalSize() )
				);
		}
	}
}

FTimeToPixel SSequencerSectionAreaView::GetTimeToPixel( const FGeometry& AllottedGeometry ) const
{
	return FTimeToPixel( AllottedGeometry, ViewRange.Get() );
}


FSequencer& SSequencerSectionAreaView::GetSequencer() const
{
	return SectionAreaNode->GetSequencer();
}
