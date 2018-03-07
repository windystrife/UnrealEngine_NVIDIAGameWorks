// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WorkflowOrientedApp/SModeWidget.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "EditorStyleSet.h"

//////////////////////////////////////////////////////////////////////////
// SModeWidget

void SModeWidget::Construct(const FArguments& InArgs, const FText& InText, const FName InMode)
{
	// Copy arguments
	ModeText = InText;
	ThisMode = InMode;
	OnGetActiveMode = InArgs._OnGetActiveMode;
	CanBeSelected = InArgs._CanBeSelected;
	OnSetActiveMode = InArgs._OnSetActiveMode;

	// Load resources
	InactiveModeBorderImage = FEditorStyle::GetBrush("ModeSelector.ToggleButton.Normal");
	ActiveModeBorderImage = FEditorStyle::GetBrush("ModeSelector.ToggleButton.Pressed");
	HoverBorderImage = FEditorStyle::GetBrush("ModeSelector.ToggleButton.Hovered");
	
	TSharedRef<SHorizontalBox> InnerRow = SNew(SHorizontalBox);

	FMargin IconPadding(4.0f, 0.0f, 4.0f, 0.0f);
	FMargin BodyPadding(0.0f, 0.0f, 0.0f, 0.0f);
	
	// Large Icon
	if (InArgs._IconImage.IsSet())
	{
		InnerRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(IconPadding)
			[
				SNew(SImage)
				.Image(InArgs._IconImage)
				.Visibility(this, &SModeWidget::GetLargeIconVisibility)
			];
	}

	// Small Icon
	if (InArgs._SmallIconImage.IsSet())
	{
		InnerRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(IconPadding)
			[
				SNew(SImage)
				.Image(InArgs._SmallIconImage)
				.Visibility(this, &SModeWidget::GetSmallIconVisibility)
			];
	}

	// Label + content
	InnerRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(BodyPadding)
		[
			SNew(SVerticalBox)

			// Mode 'tab'
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				//Mode Name
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(ModeText)
					.Font(this, &SModeWidget::GetDesiredTitleFont)
				]

				//Dirty flag
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3)
				[
					SNew(SImage)
					.Image( InArgs._DirtyMarkerBrush )
				]
			]

			// Body of 'ribbon'
			+SVerticalBox::Slot()
			.AutoHeight()
			[

				InArgs._ShortContents.Widget
			]
		];


	// Create the widgets
	ChildSlot
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(this, &SModeWidget::GetModeNameBorderImage)
		.OnMouseButtonDown(this, &SModeWidget::OnModeTabClicked)
		[
			InnerRow
		]
	];

	SetEnabled(CanBeSelected);
}

EVisibility SModeWidget::GetLargeIconVisibility() const
{
	return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SModeWidget::GetSmallIconVisibility() const
{
	return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SModeWidget::GetModeNameBorderImage() const
{
	if (IsActiveMode())
	{
		return ActiveModeBorderImage;
	}
	else if (IsHovered())
	{
		return HoverBorderImage;
	}
	else
	{
		return InactiveModeBorderImage;
	}
}

bool SModeWidget::IsActiveMode() const
{
	return OnGetActiveMode.Get() == ThisMode;
}

FReply SModeWidget::OnModeTabClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Try to change the mode
		if (!IsActiveMode() && (CanBeSelected.Get() == true))
		{
			OnSetActiveMode.ExecuteIfBound(ThisMode);
		}
	}

	return FReply::Handled();
}

FSlateFontInfo SModeWidget::GetDesiredTitleFont() const
{
	const bool bSmallIcons = FMultiBoxSettings::UseSmallToolBarIcons.Get();
	
	const int16 FontSize = bSmallIcons ? 10 : 14;

	FString FontName;
	if (IsActiveMode())
	{
		FontName = bSmallIcons ? TEXT("Roboto-Bold.ttf") : TEXT("Roboto-Regular.ttf");
	}
	else
	{
		FontName = bSmallIcons ? TEXT("Roboto-Regular.ttf") : TEXT("Roboto-Light.ttf");
	}
	
	return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts") / FontName, FontSize);
}
