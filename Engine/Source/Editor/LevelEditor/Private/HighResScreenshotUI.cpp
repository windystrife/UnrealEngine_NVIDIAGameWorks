// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HighresScreenshotUI.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SSlider.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericEntryBox.h"

TWeakPtr<class SWindow> SHighResScreenshotDialog::CurrentWindow = NULL;
TWeakPtr<class SHighResScreenshotDialog> SHighResScreenshotDialog::CurrentDialog = NULL;
bool SHighResScreenshotDialog::bMaskVisualizationWasEnabled = false;

SHighResScreenshotDialog::SHighResScreenshotDialog()
: Config(GetHighResScreenshotConfig())
{
}

void SHighResScreenshotDialog::Construct( const FArguments& InArgs )
{
	this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(5)
					[
						SNew(SSplitter)
						.Orientation(Orient_Horizontal)
						+SSplitter::Slot()
						.Value(1)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew( STextBlock )
								.Text( NSLOCTEXT("HighResScreenshot", "ScreenshotSizeMultiplier", "Screenshot Size Multiplier") )
							]
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew( STextBlock )
								.Text( NSLOCTEXT("HighResScreenshot", "IncludeBufferVisTargets", "Include Buffer Visualization Targets") )
							]
							+ SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(HDRLabel, STextBlock)
								.Text(NSLOCTEXT("HighResScreenshot", "CaptureHDR", "Write HDR format visualization targets"))
							]
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(Force128BitRenderingLabel, STextBlock)
								.Text(NSLOCTEXT("HighResScreenshot", "Force128BitPipeline", "Force 128-bit buffers for rendering pipeline"))
							]
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew( STextBlock )
								.Text( NSLOCTEXT("HighResScreenshot", "UseCustomDepth", "Use custom depth as mask") )
							]
						]
						+SSplitter::Slot()
						.Value(1)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew (SHorizontalBox)
								+SHorizontalBox::Slot()
								.FillWidth(1)
								[
									SNew( SNumericEntryBox<float> )
									.Value(this, &SHighResScreenshotDialog::GetResolutionMultiplier)
									.OnValueCommitted(this, &SHighResScreenshotDialog::OnResolutionMultiplierChanged)
								]
								+SHorizontalBox::Slot()
								.HAlign(HAlign_Fill)
								.Padding(5,0,0,0)
								.FillWidth(3)
								[
									SNew( SSlider )
									.Value(this, &SHighResScreenshotDialog::GetResolutionMultiplierSlider)
									.OnValueChanged(this, &SHighResScreenshotDialog::OnResolutionMultiplierSliderChanged)
								]
							]
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew( SCheckBox )
								.OnCheckStateChanged(this, &SHighResScreenshotDialog::OnBufferVisualizationDumpEnabledChanged)
								.IsChecked(this, &SHighResScreenshotDialog::GetBufferVisualizationDumpEnabled)
							]
							+ SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(HDRCheckBox, SCheckBox)
								.OnCheckStateChanged(this, &SHighResScreenshotDialog::OnHDREnabledChanged)
								.IsChecked(this, &SHighResScreenshotDialog::GetHDRCheckboxUIState)
							]
							+ SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(Force128BitRenderingCheckBox, SCheckBox)
								.OnCheckStateChanged(this, &SHighResScreenshotDialog::OnForce128BitRenderingChanged)
								.IsChecked(this, &SHighResScreenshotDialog::GetForce128BitRenderingCheckboxUIState)
							]
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew( SCheckBox )
								.OnCheckStateChanged(this, &SHighResScreenshotDialog::OnMaskEnabledChanged)
								.IsChecked(this, &SHighResScreenshotDialog::GetMaskEnabled)
							]
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("HighresScreenshot.WarningStrip"))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew( SBorder )
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						SNew( STextBlock )
						.Text( NSLOCTEXT("HighResScreenshot", "CaptureWarningText", "Due to the high system requirements of a high resolution screenshot, very large multipliers might cause the graphics driver to become unresponsive and possibly crash. In these circumstances, please try using a lower multiplier") )
						.AutoWrapText(true)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("HighresScreenshot.WarningStrip"))
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SGridPanel)
						+SGridPanel::Slot(0, 0)
						[
							SAssignNew(CaptureRegionButton, SButton)
							.IsEnabled(this, &SHighResScreenshotDialog::IsCaptureRegionEditingAvailable)
							.Visibility(this, &SHighResScreenshotDialog::GetSpecifyCaptureRegionVisibility)
							.ToolTipText(NSLOCTEXT("HighResScreenshot", "ScreenshotSpecifyCaptureRectangleTooltip", "Specify the region which will be captured by the screenshot"))
							.OnClicked(this, &SHighResScreenshotDialog::OnSelectCaptureRegionClicked)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("HighresScreenshot.SpecifyCaptureRectangle"))
							]
						]
						+SGridPanel::Slot(0, 0)
						[
							SNew( SButton )
							.Visibility(this, &SHighResScreenshotDialog::GetCaptureRegionControlsVisibility)
							.ToolTipText( NSLOCTEXT("HighResScreenshot", "ScreenshotAcceptCaptureRegionTooltip", "Accept any changes made to the capture region") )
							.OnClicked( this, &SHighResScreenshotDialog::OnSelectCaptureAcceptRegionClicked )
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("HighresScreenshot.AcceptCaptureRegion"))
							]
						]
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew( SButton )
						.ToolTipText( NSLOCTEXT("HighResScreenshot", "ScreenshotDiscardCaptureRegionTooltip", "Discard any changes made to the capture region") )
						.Visibility(this, &SHighResScreenshotDialog::GetCaptureRegionControlsVisibility)
						.OnClicked( this, &SHighResScreenshotDialog::OnSelectCaptureCancelRegionClicked )
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("HighresScreenshot.DiscardCaptureRegion"))
						]
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew( SButton )
						.ToolTipText( NSLOCTEXT("HighResScreenshot", "ScreenshotFullViewportCaptureRegionTooltip", "Set the capture rectangle to the whole viewport") )
						.Visibility(this, &SHighResScreenshotDialog::GetCaptureRegionControlsVisibility)
						.OnClicked( this, &SHighResScreenshotDialog::OnSetFullViewportCaptureRegionClicked )
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("HighresScreenshot.FullViewportCaptureRegion"))
						]
					]
					+SHorizontalBox::Slot()
						// for padding
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					[
						SNew( SButton )
						.ToolTipText( NSLOCTEXT("HighResScreenshot", "ScreenshotCaptureTooltop", "Take a screenshot") )
						.OnClicked( this, &SHighResScreenshotDialog::OnCaptureClicked )
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("HighresScreenshot.Capture"))
						]
					]
				]
			]
		];

	SetHDRUIEnableState(Config.bDumpBufferVisualizationTargets);
	SetForce128BitRenderingState(Config.bDumpBufferVisualizationTargets);
	bCaptureRegionControlsVisible = false;
}

void SHighResScreenshotDialog::WindowClosedHandler(const TSharedRef<SWindow>& InWindow)
{
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();

	ResetViewport();

	// Cleanup the config after each usage as it is a static and we don't want it to keep pointers or settings around between runs.
	Config.bDisplayCaptureRegion = false;
	Config.ChangeViewport(TWeakPtr<FSceneViewport>());
	CurrentWindow.Reset();
	CurrentDialog.Reset();
}

void SHighResScreenshotDialog::ResetViewport()
{
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	auto CurrentDialogPinned = CurrentDialog.Pin();
	auto ConfigViewport = Config.TargetViewport.Pin();

	if (CurrentDialogPinned.IsValid())
	{
		// Deactivate capture region widget from old viewport
		TSharedPtr<SCaptureRegionWidget> CaptureRegionWidget = CurrentDialogPinned->GetCaptureRegionWidget();
		if (CaptureRegionWidget.IsValid())
		{
			CaptureRegionWidget->Deactivate(false);
		}

		if (ConfigViewport.IsValid())
		{
			// Restore mask visualization state from before window was opened
			if (ConfigViewport->GetClient() &&
				ConfigViewport->GetClient()->GetEngineShowFlags())
			{
				ConfigViewport->GetClient()->GetEngineShowFlags()->SetHighResScreenshotMask(bMaskVisualizationWasEnabled);
			}
		}
	}
}

TWeakPtr<class SWindow> SHighResScreenshotDialog::OpenDialog(const TSharedPtr<FSceneViewport>& InViewport, TSharedPtr<SCaptureRegionWidget> InCaptureRegionWidget)
{
	FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
	auto ConfigViewport = Config.TargetViewport.Pin();
	auto CurrentWindowPinned = CurrentWindow.Pin();

	bool bInitializeDialog = false;

	if (CurrentWindowPinned.IsValid())
	{
		// Dialog window is already open - if it is being pointed at a new viewport, reset the old one
		if (InViewport != ConfigViewport)
		{
			ResetViewport();
			bInitializeDialog = true;
		}
	}
	else
	{
		// No dialog window currently open - need to create one
		TSharedRef<SHighResScreenshotDialog> Dialog = SNew(SHighResScreenshotDialog);
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title( NSLOCTEXT("HighResScreenshot", "HighResolutionScreenshot", "High Resolution Screenshot") )
			.ClientSize(FVector2D(484,231))
			.SupportsMinimize(false)
			.SupportsMaximize(false)
			.FocusWhenFirstShown(true)
			[
				Dialog
			];

		Window->SetOnWindowClosed(FOnWindowClosed::CreateStatic(&WindowClosedHandler));
		Dialog->SetWindow(Window);


		TSharedPtr<SWindow> ParentWindow = FGlobalTabmanager::Get()->GetRootWindow();

		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(Window, ParentWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(Window);
		}

		CurrentWindow = TWeakPtr<SWindow>(Window);
		CurrentDialog = TWeakPtr<SHighResScreenshotDialog>(Dialog);

		Config.bDisplayCaptureRegion = true;

		bInitializeDialog = true;
	}

	if (bInitializeDialog)
	{
		auto CurrentDialogPinned = CurrentDialog.Pin();
		if (CurrentDialogPinned.IsValid())
		{
			CurrentDialogPinned->SetCaptureRegionWidget(InCaptureRegionWidget);
			CurrentDialogPinned->SetCaptureRegionControlsVisibility(false);
		}

		CurrentWindow.Pin()->BringToFront();
		Config.ChangeViewport(InViewport);

		// Enable mask visualization if the mask is enabled
		if (InViewport.IsValid())
		{
			bMaskVisualizationWasEnabled = InViewport->GetClient()->GetEngineShowFlags()->HighResScreenshotMask;
			InViewport->GetClient()->GetEngineShowFlags()->SetHighResScreenshotMask(Config.bMaskEnabled);
		}
	}

	return CurrentWindow;
}

FReply SHighResScreenshotDialog::OnSelectCaptureRegionClicked()
{
	// Only enable the capture region widget if the owning viewport gave us one
	if (CaptureRegionWidget.IsValid())
	{
		CaptureRegionWidget->Activate(Config.UnscaledCaptureRegion.Area() > 0);
		bCaptureRegionControlsVisible = true;
	}
	return FReply::Handled();
}

FReply SHighResScreenshotDialog::OnCaptureClicked()
{
	if (!GIsHighResScreenshot)
	{
		auto ConfigViewport = Config.TargetViewport.Pin();
		if (ConfigViewport.IsValid())
		{
			GScreenshotResolutionX = ConfigViewport->GetSizeXY().X * Config.ResolutionMultiplier;
			GScreenshotResolutionY = ConfigViewport->GetSizeXY().Y * Config.ResolutionMultiplier;
			FIntRect ScaledCaptureRegion = Config.UnscaledCaptureRegion;

			if (ScaledCaptureRegion.Area() > 0)
			{
				ScaledCaptureRegion.Clip(FIntRect(FIntPoint::ZeroValue, ConfigViewport->GetSizeXY()));
				ScaledCaptureRegion *= Config.ResolutionMultiplier;
			}

			Config.CaptureRegion = ScaledCaptureRegion;

			// Trigger the screenshot on the owning viewport
			ConfigViewport->TakeHighResScreenShot();
		}
	}

	return FReply::Handled();
}

FReply SHighResScreenshotDialog::OnSelectCaptureCancelRegionClicked()
{
	if (CaptureRegionWidget.IsValid())
	{
		auto ConfigViewport = Config.TargetViewport.Pin();
		if (ConfigViewport.IsValid())
		{
			ConfigViewport->Invalidate();
		}

		CaptureRegionWidget->Deactivate(false);
	}

	// Hide the Cancel/Accept buttons, show the Edit button
	bCaptureRegionControlsVisible = false;
	return FReply::Handled();
}

FReply SHighResScreenshotDialog::OnSelectCaptureAcceptRegionClicked()
{
	if (CaptureRegionWidget.IsValid())
	{
		CaptureRegionWidget->Deactivate(true);
	}

	// Hide the Cancel/Accept buttons, show the Edit button
	bCaptureRegionControlsVisible = false;
	return FReply::Handled();
}

FReply SHighResScreenshotDialog::OnSetFullViewportCaptureRegionClicked()
{
	auto ConfigViewport = Config.TargetViewport.Pin();
	if (ConfigViewport.IsValid())
	{
		ConfigViewport->Invalidate();
	}

	Config.UnscaledCaptureRegion = FIntRect(0, 0, 0, 0);
	CaptureRegionWidget->Reset();
	return FReply::Handled();
}

FReply SHighResScreenshotDialog::OnSetCameraSafeAreaCaptureRegionClicked()
{
	FIntRect NewCaptureRegion;

	if (Config.TargetViewport.IsValid())
	{
		auto ConfigViewport = Config.TargetViewport.Pin();
		if (ConfigViewport->GetClient()->OverrideHighResScreenshotCaptureRegion(NewCaptureRegion))
		{
			Config.UnscaledCaptureRegion = NewCaptureRegion;
			ConfigViewport->Invalidate();
		}
	}

	return FReply::Handled();
}

bool SHighResScreenshotDialog::IsSetCameraSafeAreaCaptureRegionEnabled() const
{
	if (Config.TargetViewport.IsValid())
	{
		FViewportClient* Client = Config.TargetViewport.Pin()->GetClient();
		if (Client)
		{
			FIntRect Rect;
			if (Client->OverrideHighResScreenshotCaptureRegion(Rect))
			{
				return true;
			}
		}
	}

	return false;
}
