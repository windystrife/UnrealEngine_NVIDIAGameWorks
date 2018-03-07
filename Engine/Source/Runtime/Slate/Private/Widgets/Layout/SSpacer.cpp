// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SSpacer.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SSpacer::Construct( const FArguments& InArgs )
{
	SpacerSize = InArgs._Size;
}


int32 SSpacer::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// We did not paint anything. The parent's current LayerId is probably the max we were looking for.
	return LayerId;
}

FVector2D SSpacer::ComputeDesiredSize( float ) const
{
	return SpacerSize.Get();
}
