// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SynthKnobStyle.h"
#include "IPluginManager.h"
#include "SynthesisModule.h"
#include "Regex.h"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Brushes/SlateDynamicImageBrush.h"

struct FSynthKnobResources
{
	FSynthKnobResources()
		: bImagesLoaded(false)
	{
	}

	void LoadImages()
	{
		if (!bImagesLoaded)
		{
			bImagesLoaded = true;

			FString SynthesisPluginBaseDir = IPluginManager::Get().FindPlugin("Synthesis")->GetBaseDir();
			SynthesisPluginBaseDir += "/Content/UI/";

			FString BrushPath = SynthesisPluginBaseDir + "SynthKnobLarge.png";
			DefaultLargeKnob = MakeShareable(new FSlateDynamicImageBrush(FName(*BrushPath), FVector2D(150.0f, 150.0f)));

			BrushPath = SynthesisPluginBaseDir + "SynthKnobLargeOverlay.png";
			DefaultLargeKnobOverlay = MakeShareable(new FSlateDynamicImageBrush(FName(*BrushPath), FVector2D(150.0f, 150.0f)));

			BrushPath = SynthesisPluginBaseDir + "SynthKnobMedium.png";
			DefaultMediumKnob = MakeShareable(new FSlateDynamicImageBrush(FName(*BrushPath), FVector2D(150.0f, 150.0f)));

			BrushPath = SynthesisPluginBaseDir + "SynthKnobMediumOverlay.png";
			DefaultMediumKnobOverlay = MakeShareable(new FSlateDynamicImageBrush(FName(*BrushPath), FVector2D(150.0f, 150.0f)));

		}
	}
		
	bool bImagesLoaded;

	TSharedPtr<FSlateDynamicImageBrush> DefaultLargeKnob;
	TSharedPtr<FSlateDynamicImageBrush> DefaultLargeKnobOverlay;
	TSharedPtr<FSlateDynamicImageBrush> DefaultMediumKnob;
	TSharedPtr<FSlateDynamicImageBrush> DefaultMediumKnobOverlay;
};

static FSynthKnobResources Resources;

FSynthKnobStyle::FSynthKnobStyle()
	: KnobSize(ESynthKnobSize::Medium)
{
	MinValueAngle = -0.4f;
	MaxValueAngle = 0.4f;

	Resources.LoadImages();

	const FSlateBrush* DefaultMediumBrush = Resources.DefaultMediumKnob.Get();
	MediumKnob = *DefaultMediumBrush;

	const FSlateBrush* DefaultMediumBrushOverlay = Resources.DefaultMediumKnobOverlay.Get();
	MediumKnobOverlay = *DefaultMediumBrushOverlay;

	const FSlateBrush* DefaultLargBrush = Resources.DefaultLargeKnob.Get();
	LargeKnob = *DefaultLargBrush;

	const FSlateBrush* DefaultLargBrushOverlay = Resources.DefaultLargeKnobOverlay.Get();
	LargeKnobOverlay = *DefaultLargBrushOverlay;


}

FSynthKnobStyle::~FSynthKnobStyle()
{
}

const FSlateBrush* FSynthKnobStyle::GetBaseBrush() const
{
	if (KnobSize == ESynthKnobSize::Medium)
	{
		return &MediumKnob;
	}
	else
	{
		return &LargeKnob;
	}

}

const FSlateBrush* FSynthKnobStyle::GetOverlayBrush() const
{
	if (KnobSize == ESynthKnobSize::Medium)
	{
		return &MediumKnobOverlay;
	}
	else
	{
		return &LargeKnobOverlay;
	}
}

void FSynthKnobStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&LargeKnob);
	OutBrushes.Add(&LargeKnobOverlay);
	OutBrushes.Add(&MediumKnob);
	OutBrushes.Add(&MediumKnobOverlay);
}

const FSynthKnobStyle& FSynthKnobStyle::GetDefault()
{
	static FSynthKnobStyle Default;
	return Default;
}

const FName FSynthKnobStyle::TypeName( TEXT("SynthKnobStyle") );