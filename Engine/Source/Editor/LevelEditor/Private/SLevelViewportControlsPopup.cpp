// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLevelViewportControlsPopup.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "UnrealEdMisc.h"

void SLevelViewportControlsPopup::Construct(const FArguments& InArgs)
{
	TAttribute<FText> ToolTipText = NSLOCTEXT("LevelViewportControlsPopup", "ViewportControlsToolTip", "Click to show Viewport Controls");
	Default = FEditorStyle::GetBrush("HelpIcon");
	Hovered = FEditorStyle::GetBrush("HelpIcon.Hovered");
	Pressed = FEditorStyle::GetBrush("HelpIcon.Pressed");

	ChildSlot
	[
		SAssignNew(Button, SButton)
		.ContentPadding(5)
		.ButtonStyle(FEditorStyle::Get(), "HelpButton")
		.OnClicked(this, &SLevelViewportControlsPopup::OnClicked)
		.ClickMethod(EButtonClickMethod::MouseDown)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(ToolTipText)
		[
			SAssignNew(ButtonImage, SImage)
			.Image(this, &SLevelViewportControlsPopup::GetButtonImage)
		]
	];
}

const FSlateBrush* SLevelViewportControlsPopup::GetButtonImage() const
{
	if (Button->IsPressed())
	{
		return Pressed;
	}

	if (ButtonImage->IsHovered())
	{
		return Hovered;
	}

	return Default;
}

FReply SLevelViewportControlsPopup::OnClicked() const
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("ViewportControlsURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}

	return FReply::Handled();
}
