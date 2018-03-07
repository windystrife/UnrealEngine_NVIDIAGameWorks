// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/CircularThrobber.h"
#include "Slate/SlateBrushAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Components/CanvasPanelSlot.h"
#include "Widgets/Images/SThrobber.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UCircularThrobber

UCircularThrobber::UCircularThrobber(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  bEnableRadius(true)
{
	SCircularThrobber::FArguments DefaultArgs;
	Image = *DefaultArgs._PieceImage;

	NumberOfPieces = DefaultArgs._NumPieces;
	Period = DefaultArgs._Period;
	Radius = DefaultArgs._Radius;
}

void UCircularThrobber::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyCircularThrobber.Reset();
}

TSharedRef<SWidget> UCircularThrobber::RebuildWidget()
{
	SCircularThrobber::FArguments DefaultArgs;

	MyCircularThrobber = SNew(SCircularThrobber)
		.PieceImage(&Image)
		.NumPieces(FMath::Clamp(NumberOfPieces, 1, 25))
		.Period(FMath::Max(Period, SCircularThrobber::MinimumPeriodValue))
		.Radius(Radius);

	return MyCircularThrobber.ToSharedRef();
}

void UCircularThrobber::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyCircularThrobber->SetNumPieces(FMath::Clamp(NumberOfPieces, 1, 25));
	MyCircularThrobber->SetPeriod(FMath::Max(Period, SCircularThrobber::MinimumPeriodValue));
	MyCircularThrobber->SetRadius(Radius);

	// If widget is child of Canvas Panel and 'Size to Content' is enabled, we allow user to modify radius.
	bEnableRadius = true;
	if (UCanvasPanelSlot* Panel = Cast<UCanvasPanelSlot>(Slot))
	{
		if (!Panel->GetAutoSize())
		{
			bEnableRadius = false;
		}
	}
}

void UCircularThrobber::SetNumberOfPieces(int32 InNumberOfPieces)
{
	NumberOfPieces = InNumberOfPieces;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetNumPieces(FMath::Clamp(NumberOfPieces, 1, 25));
	}
}

void UCircularThrobber::SetPeriod(float InPeriod)
{
	Period = InPeriod;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetPeriod(FMath::Max(InPeriod, SCircularThrobber::MinimumPeriodValue));
	}
}

void UCircularThrobber::SetRadius(float InRadius)
{
	Radius = InRadius;
	if (MyCircularThrobber.IsValid())
	{
		MyCircularThrobber->SetRadius(InRadius);
	}
}

void UCircularThrobber::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( PieceImage_DEPRECATED != nullptr )
		{
			Image = PieceImage_DEPRECATED->Brush;
			PieceImage_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FText UCircularThrobber::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
