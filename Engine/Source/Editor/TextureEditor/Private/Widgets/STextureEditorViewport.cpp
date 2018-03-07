// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/STextureEditorViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SViewport.h"
#include "Widgets/Input/SSlider.h"
#include "Engine/Texture.h"
#include "Slate/SceneViewport.h"
#include "TextureEditorConstants.h"
#include "Widgets/STextureEditorViewportToolbar.h"
#include "Widgets/Input/SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "STextureEditorViewport"

// Specifies the maximum allowed exposure bias.
const int32 MaxExposure = 10;

// Specifies the minimum allowed exposure bias.
const int32 MinExposure = -10;


/* STextureEditorViewport interface
 *****************************************************************************/

void STextureEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	ViewportClient->AddReferencedObjects(Collector);
}


void STextureEditorViewport::Construct( const FArguments& InArgs, const TSharedRef<ITextureEditorToolkit>& InToolkit )
{
	ExposureBias = 0;
	bIsRenderingEnabled = true;
	ToolkitPtr = InToolkit;
	
	// create zoom menu
	FMenuBuilder ZoomMenuBuilder(true, NULL);
	{
		FUIAction Zoom25Action(FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuEntryClicked, 0.25));
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("Zoom25Action", "25%"), LOCTEXT("Zoom25ActionHint", "Show the texture at a quarter of its size."), FSlateIcon(), Zoom25Action);

		FUIAction Zoom50Action(FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuEntryClicked, 0.5));
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("Zoom50Action", "50%"), LOCTEXT("Zoom50ActionHint", "Show the texture at half its size."), FSlateIcon(), Zoom50Action);

		FUIAction Zoom100Action(FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuEntryClicked, 1.0));
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("Zoom100Action", "100%"), LOCTEXT("Zoom100ActionHint", "Show the texture in its original size."), FSlateIcon(), Zoom100Action);

		FUIAction Zoom200Action(FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuEntryClicked, 2.0));
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("Zoom200Action", "200%"), LOCTEXT("Zoom200ActionHint", "Show the texture at twice its size."), FSlateIcon(), Zoom200Action);

		FUIAction Zoom400Action(FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuEntryClicked, 4.0));
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("Zoom400Action", "400%"), LOCTEXT("Zoom400ActionHint", "Show the texture at four times its size."), FSlateIcon(), Zoom400Action);

		ZoomMenuBuilder.AddMenuSeparator();

		FUIAction ZoomFitAction(
			FExecuteAction::CreateSP(this, &STextureEditorViewport::HandleZoomMenuFitClicked), 
			FCanExecuteAction(), 
			FIsActionChecked::CreateSP(this, &STextureEditorViewport::IsZoomMenuFitChecked)
			);
		ZoomMenuBuilder.AddMenuEntry(LOCTEXT("ZoomFitAction", "Scale To Fit"), LOCTEXT("ZoomFillActionHint", "Scale the texture to fit the viewport."), FSlateIcon(), ZoomFitAction, NAME_None, EUserInterfaceActionType::ToggleButton);
	}

	FText TextureName = FText::GetEmpty();
	
	if (InToolkit->GetTexture() != nullptr)
	{
		FText FormattedText = InToolkit->HasValidTextureResource() ? FText::FromString(TEXT("{0}")) : LOCTEXT( "InvalidTexture", "{0} (Invalid Texture)");

		TextureName = FText::Format(FormattedText, FText::FromName(InToolkit->GetTexture()->GetFName()));
	}

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.FillHeight(1)
							[
								SNew(SOverlay)

								// viewport canvas
								+ SOverlay::Slot()
									[
										SAssignNew(ViewportWidget, SViewport)
											.EnableGammaCorrection(false)
											.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
											.ShowEffectWhenDisabled(false)
											.EnableBlending(true)
											.ToolTip(SNew(SToolTip).Text(this, &STextureEditorViewport::GetDisplayedResolution))
									]
	
								// tool bar
								+ SOverlay::Slot()
									.Padding(2.0f)
									.VAlign(VAlign_Top)
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
											.AutoWidth()
											[
												SNew(STextureEditorViewportToolbar, InToolkit->GetToolkitCommands())
											]

										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.Padding(4.0f, 0.0f)
											.HAlign(HAlign_Right)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(TextureName)
											]
									]
							]
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						// vertical scroll bar
						SAssignNew(TextureViewportVerticalScrollBar, SScrollBar)
							.Visibility(this, &STextureEditorViewport::HandleVerticalScrollBarVisibility)
							.OnUserScrolled(this, &STextureEditorViewport::HandleVerticalScrollBarScrolled)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				// horizontal scrollbar
				SAssignNew(TextureViewportHorizontalScrollBar, SScrollBar)
					.Orientation( Orient_Horizontal )
					.Visibility(this, &STextureEditorViewport::HandleHorizontalScrollBarVisibility)
					.OnUserScrolled(this, &STextureEditorViewport::HandleHorizontalScrollBarScrolled)
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				// exposure bias
				+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					[
						SNew(SHorizontalBox)
							.Visibility(this, &STextureEditorViewport::HandleExposureBiasWidgetVisibility)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("ExposureBiasLabel", "Exposure Bias:"))
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.0f, 0.0f)
							[
								SNew(SNumericEntryBox<int32>)
									.AllowSpin(true)
									.MinSliderValue(MinExposure)
									.MaxSliderValue(MaxExposure)
									.Value(this, &STextureEditorViewport::HandleExposureBiasBoxValue)
									.OnValueChanged(this, &STextureEditorViewport::HandleExposureBiasBoxValueChanged)
							]
					]

				// separator
				+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					[
						SNullWidget::NullWidget
					]

				// zoom slider
				+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("ZoomLabel", "Zoom:"))
							]

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(4.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SSlider)
									.OnValueChanged(this, &STextureEditorViewport::HandleZoomSliderChanged)
									.Value(this, &STextureEditorViewport::HandleZoomSliderValue)
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(this, &STextureEditorViewport::HandleZoomPercentageText)
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2.0f, 0.0f, 0.0f, 0.0f)
							.VAlign(VAlign_Center)
							[
								SNew(SComboButton)
									.ContentPadding(FMargin(0.0))
									.MenuContent()
									[
										ZoomMenuBuilder.MakeWidget()
									]
							]
					]
			]
	];

	ViewportClient = MakeShareable(new FTextureEditorViewportClient(ToolkitPtr, SharedThis(this)));

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );
}


void STextureEditorViewport::ModifyCheckerboardTextureColors( )
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ModifyCheckerboardTextureColors();
	}
}

void STextureEditorViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void STextureEditorViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

TSharedPtr<FSceneViewport> STextureEditorViewport::GetViewport( ) const
{
	return Viewport;
}

TSharedPtr<SViewport> STextureEditorViewport::GetViewportWidget( ) const
{
	return ViewportWidget;
}

TSharedPtr<SScrollBar> STextureEditorViewport::GetVerticalScrollBar( ) const
{
	return TextureViewportVerticalScrollBar;
}

TSharedPtr<SScrollBar> STextureEditorViewport::GetHorizontalScrollBar( ) const
{
	return TextureViewportHorizontalScrollBar;
}


/* SWidget overrides
 *****************************************************************************/

void STextureEditorViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
		Viewport->InvalidateDisplay();
	}
}


/* STextureEditorViewport implementation
 *****************************************************************************/

FText STextureEditorViewport::GetDisplayedResolution( ) const
{
	return ViewportClient->GetDisplayedResolution();
}


/* STextureEditorViewport event handlers
 *****************************************************************************/

EVisibility STextureEditorViewport::HandleExposureBiasWidgetVisibility( ) const
{
	UTexture* Texture = ToolkitPtr.Pin()->GetTexture();

	if ((Texture != NULL) && (Texture->CompressionSettings == TC_HDR))
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


TOptional<int32> STextureEditorViewport::HandleExposureBiasBoxValue( ) const
{
	return ExposureBias;
}


void STextureEditorViewport::HandleExposureBiasBoxValueChanged( int32 NewExposure )
{
	ExposureBias = NewExposure;
}


void STextureEditorViewport::HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction )
{
	float Ratio = ViewportClient->GetViewportHorizontalScrollBarRatio();
	float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	TextureViewportHorizontalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}


EVisibility STextureEditorViewport::HandleHorizontalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportHorizontalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}


void STextureEditorViewport::HandleVerticalScrollBarScrolled( float InScrollOffsetFraction )
{
	float Ratio = ViewportClient->GetViewportVerticalScrollBarRatio();
	float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	TextureViewportVerticalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}


EVisibility STextureEditorViewport::HandleVerticalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportVerticalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}


void STextureEditorViewport::HandleZoomMenuEntryClicked( double ZoomValue )
{
	ToolkitPtr.Pin()->SetZoom(ZoomValue);
}


void STextureEditorViewport::HandleZoomMenuFitClicked()
{
	ToolkitPtr.Pin()->ToggleFitToViewport();
}


bool STextureEditorViewport::IsZoomMenuFitChecked() const
{
	return ToolkitPtr.Pin()->GetFitToViewport();
}

bool STextureEditorViewport::HasValidTextureResource() const
{
	return ToolkitPtr.Pin()->HasValidTextureResource();
}

FText STextureEditorViewport::HandleZoomPercentageText( ) const
{
	const bool bFitToViewport = ToolkitPtr.Pin()->GetFitToViewport();
	if(bFitToViewport)
	{
		return LOCTEXT("ZoomFitText", "Fit");
	}

	return FText::AsPercent(ToolkitPtr.Pin()->GetZoom());
}


void STextureEditorViewport::HandleZoomSliderChanged( float NewValue )
{
	ToolkitPtr.Pin()->SetZoom(NewValue * MaxZoom);
}


float STextureEditorViewport::HandleZoomSliderValue( ) const
{
	return (ToolkitPtr.Pin()->GetZoom() / MaxZoom);
}


#undef LOCTEXT_NAMESPACE
