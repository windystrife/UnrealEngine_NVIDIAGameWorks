// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SCompoundWidget.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/LayoutUtils.h"
#include "SlateGlobals.h"

DECLARE_CYCLE_STAT(TEXT("Child Paint"), STAT_ChildPaint, STATGROUP_SlateVeryVerbose);

int32 SCompoundWidget::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{

	// A CompoundWidget just draws its children
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	{
		this->ArrangeChildren(AllottedGeometry, ArrangedChildren);
	}

	// There may be zero elements in this array if our child collapsed/hidden
	if( ArrangedChildren.Num() > 0 )
	{
		check( ArrangedChildren.Num() == 1 );
		FArrangedWidget& TheChild = ArrangedChildren[0];

		FWidgetStyle CompoundedWidgetStyle = FWidgetStyle(InWidgetStyle)
			.BlendColorAndOpacityTint(ColorAndOpacity.Get())
			.SetForegroundColor( GetForegroundColor() );

		int32 Layer = 0;
		{
#if WITH_VERY_VERBOSE_SLATE_STATS
			SCOPE_CYCLE_COUNTER(STAT_ChildPaint);
#endif
			Layer = TheChild.Widget->Paint( Args.WithNewParent(this), TheChild.Geometry, MyCullingRect, OutDrawElements, LayerId + 1, CompoundedWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
		}
		return Layer;
	}
	return LayerId;
}

FChildren* SCompoundWidget::GetChildren()
{
	return &ChildSlot;
}


FVector2D SCompoundWidget::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ChildVisibility != EVisibility::Collapsed )
	{
		return ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
	}
	
	return FVector2D::ZeroVector;
}

void SCompoundWidget::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	ArrangeSingleChild( AllottedGeometry, ArrangedChildren, ChildSlot, ContentScale );
}

FSlateColor SCompoundWidget::GetForegroundColor() const
{
	return ForegroundColor.Get();
}

SCompoundWidget::SCompoundWidget()
	: ChildSlot()
	, ContentScale( FVector2D(1.0f,1.0f) )
	, ColorAndOpacity( FLinearColor::White )
	, ForegroundColor( FSlateColor::UseForeground() )
{
}

void SCompoundWidget::SetVisibility( TAttribute<EVisibility> InVisibility )
{
	SWidget::SetVisibility( InVisibility );
}
