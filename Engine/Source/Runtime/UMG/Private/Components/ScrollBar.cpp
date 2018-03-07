// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ScrollBar.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UScrollBar

UScrollBar::UScrollBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	bAlwaysShowScrollbar = true;
	Orientation = Orient_Vertical;
	Thickness = FVector2D(12.0f, 12.0f);

	SScrollBar::FArguments Defaults;
	WidgetStyle = *Defaults._Style;
}

void UScrollBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyScrollBar.Reset();
}

TSharedRef<SWidget> UScrollBar::RebuildWidget()
{
	MyScrollBar = SNew(SScrollBar)
		.Style(&WidgetStyle)
		.AlwaysShowScrollbar(bAlwaysShowScrollbar)
		.Orientation(Orientation)
		.Thickness(Thickness);

	//SLATE_EVENT(FOnUserScrolled, OnUserScrolled)

	return MyScrollBar.ToSharedRef();
}

void UScrollBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	//MyScrollBar->SetScrollOffset(DesiredScrollOffset);
}

void UScrollBar::SetState(float InOffsetFraction, float InThumbSizeFraction)
{
	if ( MyScrollBar.IsValid() )
	{
		MyScrollBar->SetState(InOffsetFraction, InThumbSizeFraction);
	}
}

void UScrollBar::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FScrollBarStyle* StylePtr = Style_DEPRECATED->GetStyle<FScrollBarStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FText UScrollBar::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
