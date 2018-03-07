// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorStyleSet.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

class FText;
struct FSlateBrush;


/* Static helpers
 *****************************************************************************/

// @todo gmp: move this into a shared library or create a SImageButton widget.
static TSharedRef<SButton> MakeImageButton(const FSlateBrush* ButtonImage, const FText& ButtonTip, const FOnClicked& OnClickedDelegate)
{
	return SNew(SButton)
		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		.OnClicked(OnClickedDelegate)
		.ToolTipText(ButtonTip)
		.ContentPadding(2)
		.VAlign(VAlign_Center)
		.ForegroundColor(FSlateColor::UseForeground())
		.Content()
		[
			SNew(SImage)
				.Image(ButtonImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
		];
}
