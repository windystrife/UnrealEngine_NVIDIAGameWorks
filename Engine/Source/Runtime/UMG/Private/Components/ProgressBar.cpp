// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ProgressBar.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UProgressBar

UProgressBar::UProgressBar(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SProgressBar::FArguments SlateDefaults;
	WidgetStyle = *SlateDefaults._Style;
	WidgetStyle.FillImage.TintColor = FLinearColor::White;

	BarFillType = EProgressBarFillType::LeftToRight;
	bIsMarquee = false;
	Percent = 0;
	FillColorAndOpacity = FLinearColor::White;
	BorderPadding = FVector2D(0, 0);
}

void UProgressBar::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyProgressBar.Reset();
}

TSharedRef<SWidget> UProgressBar::RebuildWidget()
{
	MyProgressBar = SNew(SProgressBar);

	return MyProgressBar.ToSharedRef();
}

void UProgressBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute< TOptional<float> > PercentBinding = OPTIONAL_BINDING_CONVERT(float, Percent, TOptional<float>, ConvertFloatToOptionalFloat);
	TAttribute<FSlateColor> FillColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, FillColorAndOpacity);

	MyProgressBar->SetStyle(&WidgetStyle);

	MyProgressBar->SetBarFillType(BarFillType);
	MyProgressBar->SetPercent(bIsMarquee ? TOptional<float>() : PercentBinding);
	MyProgressBar->SetFillColorAndOpacity(FillColorAndOpacityBinding);
	MyProgressBar->SetBorderPadding(BorderPadding);
}

void UProgressBar::SetIsMarquee(bool InbIsMarquee)
{
	bIsMarquee = InbIsMarquee;
	if (MyProgressBar.IsValid())
	{
		MyProgressBar->SetPercent(bIsMarquee ? TOptional<float>() : Percent);
	}
}

void UProgressBar::SetFillColorAndOpacity(FLinearColor Color)
{
	FillColorAndOpacity = Color;
	if (MyProgressBar.IsValid())
	{
		MyProgressBar->SetFillColorAndOpacity(FillColorAndOpacity);
	}
}

void UProgressBar::SetPercent(float InPercent)
{
	Percent = InPercent;
	if (MyProgressBar.IsValid())
	{
		MyProgressBar->SetPercent(InPercent);
	}
}

void UProgressBar::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS)
	{
		if (Style_DEPRECATED != nullptr)
		{
			const FProgressBarStyle* StylePtr = Style_DEPRECATED->GetStyle<FProgressBarStyle>();
			if (StylePtr != nullptr)
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if (BackgroundImage_DEPRECATED != nullptr)
		{
			WidgetStyle.BackgroundImage = BackgroundImage_DEPRECATED->Brush;
			BackgroundImage_DEPRECATED = nullptr;
		}

		if (FillImage_DEPRECATED != nullptr)
		{
			WidgetStyle.FillImage = FillImage_DEPRECATED->Brush;
			FillImage_DEPRECATED = nullptr;
		}

		if (MarqueeImage_DEPRECATED != nullptr)
		{
			WidgetStyle.MarqueeImage = MarqueeImage_DEPRECATED->Brush;
			MarqueeImage_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FText UProgressBar::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

void UProgressBar::OnCreationFromPalette()
{
	FillColorAndOpacity = FLinearColor(0, 0.5f, 1.0f);
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
