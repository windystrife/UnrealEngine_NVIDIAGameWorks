// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SComposurePostMoveSettingsImportDialog.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "EditorDirectories.h"

#define LOCTEXT_NAMESPACE "PostMoveSettingsImportDialog"

void SComposurePostMoveSettingsImportDialog::Construct(const FArguments& InArgs, float InFrameInterval, int32 InStartFrame)
{
	FrameInterval = InFrameInterval;
	StartFrame = InStartFrame;
	OnImportSelected = InArgs._OnImportSelected;
	OnImportCanceled = InArgs._OnImportCanceled;

	const FButtonStyle& ButtonStyle = FCoreStyle::Get().GetWidgetStyle< FButtonStyle >("Button");

	//TODO: There is a matching list in sequencer for snapping values, they should be generated together.
	TArray<SNumericDropDown<float>::FNamedValue> FrameRateValues;
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 15.0f, LOCTEXT("15Fps", "15 fps"), LOCTEXT("Description15Fps", "15 fps")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 24.0f, LOCTEXT("24Fps", "24 fps (film)"), LOCTEXT("Description24Fps", "24 fps (film)")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 25.0f, LOCTEXT("25Fps", "25 fps (PAL/25)"), LOCTEXT("Description25Fps", "25 fps (PAL/25)")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 29.97f, LOCTEXT("29.97Fps", "29.97 fps (NTSC/30)"), LOCTEXT("Description29.97Fps", "29.97 fps (NTSC/30)")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 30.0f, LOCTEXT("30Fps", "30 fps"), LOCTEXT("Description30Fps", "30 fps")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 48.0f, LOCTEXT("48Fps", "48 fps"), LOCTEXT("Description48Fps", "48 fps")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 50.0f, LOCTEXT("50Fps", "50 fps (PAL/50)"), LOCTEXT("Description50Fps", "50 fps (PAL/50)")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 59.94f, LOCTEXT("59.94Fps", "59.94 fps (NTSC/60)"), LOCTEXT("Description59.94Fps", "59.94 fps (NTSC/60)")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 60.0f, LOCTEXT("60Fps", "60 fps"), LOCTEXT("Description60Fps", "60 fps")));
	FrameRateValues.Add(SNumericDropDown<float>::FNamedValue(1 / 120.0f, LOCTEXT("120Fps", "120 fps"), LOCTEXT("Description120Fps", "120 fps")));

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("PostMoveSettingsImportDialogTitle", "Import external post moves data"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(350, 170))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(10)
				[
					SNew(SGridPanel)
					.FillColumn(1, 0.5f)
					.FillColumn(2, 0.5f)

					// File Path
					+ SGridPanel::Slot(0, 0)
					.Padding(0, 0, 10, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FileLabel", "File name"))
					]
					+ SGridPanel::Slot(1, 0)
					.ColumnSpan(2)
					.Padding(0, 0, 0, 0)
					[
						SNew(SFilePathPicker)
						.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
						.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.BrowseButtonToolTip(LOCTEXT("FileButtonToolTipText", "Choose a post moves text file..."))
						.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
						.BrowseTitle(LOCTEXT("BrowseButtonTitle", "Choose a post moves text file"))
						.FileTypeFilter(TEXT("Text File (*.txt)|*.txt"))
						.FilePath(this, &SComposurePostMoveSettingsImportDialog::GetFilePath)
						.OnPathPicked(this, &SComposurePostMoveSettingsImportDialog::FilePathPicked)
					]

					// Frame Rate
					+ SGridPanel::Slot(0, 2)
					.Padding(0, 10, 10, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FrameRateLabel", "Frame Rate"))
					]
					+ SGridPanel::Slot(1, 2)
					.Padding(0, 10, 0, 0)
					[
						SNew(SNumericDropDown<float>)
						.DropDownValues(FrameRateValues)
						.bShowNamedValue(true)
						.Value(this, &SComposurePostMoveSettingsImportDialog::GetFrameInterval)
						.OnValueChanged(this, &SComposurePostMoveSettingsImportDialog::FrameIntervalChanged)
					]

					// Start Frame
					+ SGridPanel::Slot(0, 3)
					.Padding(0, 10, 10, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StartFrameLabel", "Start Frame"))
					]
					+ SGridPanel::Slot(1, 3)
					.Padding(0, 10, 0, 0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(TOptional<int32>())
						.MaxValue(TOptional<int32>())
						.MaxSliderValue(TOptional<int32>())
						.MinSliderValue(TOptional<int32>())
						.Delta(1)
						.Value(this, &SComposurePostMoveSettingsImportDialog::GetStartFrame)
						.OnValueChanged(this, &SComposurePostMoveSettingsImportDialog::StartFrameChanged)
					]
				]
			]

			// Buttons
			+ SVerticalBox::Slot()
			.Padding(10)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			[
				// Import button
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0, 0, 10, 0)
				.VAlign(VAlign_Bottom)
				[
					SNew(SButton)
					.OnClicked(this, &SComposurePostMoveSettingsImportDialog::OnImportPressed)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ImportButtonLabel", "Import"))
						.Justification(ETextJustify::Center)
						.MinDesiredWidth(90)
					]
				]

				// Cancel button
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew(SButton)
					.OnClicked(this, &SComposurePostMoveSettingsImportDialog::OnCancelPressed)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
						.Justification(ETextJustify::Center)
						.MinDesiredWidth(90)
					]
				]
			]
		]);
}

FString SComposurePostMoveSettingsImportDialog::GetFilePath() const
{
	return FilePath;
}

void SComposurePostMoveSettingsImportDialog::FilePathPicked(const FString& PickedPath)
{
	FilePath = PickedPath;
}

float SComposurePostMoveSettingsImportDialog::GetFrameInterval() const
{
	return FrameInterval;
}

int32  SComposurePostMoveSettingsImportDialog::GetStartFrame() const
{
	return StartFrame;
}

void  SComposurePostMoveSettingsImportDialog::StartFrameChanged(int32 Value)
{
	StartFrame = Value;
}

void SComposurePostMoveSettingsImportDialog::FrameIntervalChanged(float Value)
{
	FrameInterval = Value;
}

FReply SComposurePostMoveSettingsImportDialog::OnCancelPressed()
{
	OnImportCanceled.ExecuteIfBound();
	return FReply::Handled();
}

FReply SComposurePostMoveSettingsImportDialog::OnImportPressed()
{
	OnImportSelected.ExecuteIfBound(FilePath, FrameInterval, StartFrame);
	return FReply::Handled();
}