// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SSeparator.h"

/////////////////////////////////////////////////////
// SSeparator

// Construct this widget
void SSeparator::Construct( const FArguments& InArgs )
{
	SBorder::Construct(
		SBorder::FArguments()
		.BorderImage(InArgs._SeparatorImage)
		.Padding(0.0f)
		.BorderBackgroundColor(InArgs._ColorAndOpacity)
	);

	Orientation = InArgs._Orientation;
	Thickness = InArgs._Thickness;
}

FVector2D SSeparator::ComputeDesiredSize( float ) const
{
	const float Length = 16.0f;
	return (Orientation == Orient_Horizontal) ? FVector2D(Length, Thickness) : FVector2D(Thickness, Length);
}
