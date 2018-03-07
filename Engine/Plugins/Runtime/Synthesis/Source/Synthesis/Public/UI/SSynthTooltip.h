// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"

// Special tooltip that doesn't follow the mouse position
// Creates a simple window at the designated position.
class SSynthTooltip : public SOverlay
{
public:
	SLATE_BEGIN_ARGS(SSynthTooltip)
	{}
	SLATE_SUPPORTS_SLOT(SOverlay::FOverlaySlot)
	SLATE_END_ARGS()

	~SSynthTooltip();
	void Construct(const FArguments& InArgs);
	void SetWindowContainerVisibility(bool bShowVisibility);
	void SetOverlayWindowPosition(FVector2D Position);
	void SetOverlayText(const FText& InText);

private:
	TSharedPtr<SWindow> WindowContainer;
	TSharedPtr<STextBlock> TooltipText;
	FVector2D WindowPosition;
	bool bIsVisible;

};
