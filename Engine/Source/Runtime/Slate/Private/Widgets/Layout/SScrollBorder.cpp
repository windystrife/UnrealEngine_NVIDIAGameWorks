// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Layout/IScrollableWidget.h"


SScrollBorder::~SScrollBorder()
{ }


void SScrollBorder::Construct(const FArguments& InArgs, TSharedRef<IScrollableWidget> InScrollableWidget)
{
	check( InArgs._Style );

	BorderFadeDistance = InArgs._BorderFadeDistance;
	ScrollableWidget = InScrollableWidget;

	TSharedRef<SOverlay> Overlay = SNew(SOverlay)
		+ SOverlay::Slot()
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
		[
			InArgs._Content.Widget
		];

	Overlay->AddSlot()
	.HAlign( HAlign_Fill )
	.VAlign( VAlign_Top )
	[
		// Shadow: Hint to scroll up
		SNew( SImage )
		.Visibility( EVisibility::HitTestInvisible )
		.ColorAndOpacity( this, &SScrollBorder::GetTopBorderOpacity )
		.Image( &InArgs._Style->TopShadowBrush )
	];

	Overlay->AddSlot()
	.HAlign( HAlign_Fill )
	.VAlign( VAlign_Bottom )
	[
		// Shadow: a hint to scroll down
		SNew( SImage )
		.Visibility( EVisibility::HitTestInvisible )
		.ColorAndOpacity( this, &SScrollBorder::GetBottomBorderOpacity )
		.Image( &InArgs._Style->BottomShadowBrush )
	];

	ChildSlot
	[
		Overlay
	];
}

FSlateColor SScrollBorder::GetTopBorderOpacity() const
{
	float ShadowOpacity = 0;

	TSharedPtr< IScrollableWidget > Scrollable = ScrollableWidget.Get().Pin();
	if ( Scrollable.IsValid() )
	{
		// The shadow should only be visible when the user needs a hint that they can scroll up.
		ShadowOpacity = FMath::Clamp( Scrollable->GetScrollDistance().Y / BorderFadeDistance.Get().Y, 0.0f, 1.0f );
	}

	return FLinearColor( 1.0f, 1.0f, 1.0f, ShadowOpacity );
}

FSlateColor SScrollBorder::GetBottomBorderOpacity() const
{
	float ShadowOpacity = 0;

	TSharedPtr< IScrollableWidget > Scrollable = ScrollableWidget.Get().Pin();
	if (Scrollable.IsValid())
	{
		// The shadow should only be visible when the user needs a hint that they can scroll down.
		ShadowOpacity = ( Scrollable->GetScrollDistanceRemaining().Y ) / BorderFadeDistance.Get().Y;
	}

	return FLinearColor( 1.0f, 1.0f, 1.0f, ShadowOpacity );
}
