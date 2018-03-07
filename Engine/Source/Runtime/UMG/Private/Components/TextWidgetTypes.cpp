// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/TextWidgetTypes.h"
#include "Fonts/FontCache.h"

FShapedTextOptions::FShapedTextOptions()
{
	bOverride_TextShapingMethod = false;
	TextShapingMethod = ETextShapingMethod::Auto;

	bOverride_TextFlowDirection = false;
	TextFlowDirection = ETextFlowDirection::Auto;
}


UTextLayoutWidget::UTextLayoutWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Justification = ETextJustify::Left;
	AutoWrapText = false;
	WrapTextAt = 0.0f;
	WrappingPolicy = ETextWrappingPolicy::DefaultWrapping;
	Margin = FMargin(0.0f);
	LineHeightPercentage = 1.0f;
}
