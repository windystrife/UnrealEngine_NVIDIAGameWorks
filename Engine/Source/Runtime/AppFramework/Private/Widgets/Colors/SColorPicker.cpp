// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Colors/SColorPicker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Colors/SEyeDropperButton.h"
#include "Widgets/Colors/SColorWheel.h"
#include "Widgets/Colors/SColorSpectrum.h"
#include "Widgets/Colors/SColorThemes.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "MenuStack.h"

#define LOCTEXT_NAMESPACE "ColorPicker"


/** A default window size for the color picker which looks nice */
const FVector2D SColorPicker::DEFAULT_WINDOW_SIZE = FVector2D(441, 537);

/** The max time allowed for updating before we shut off auto-updating */
const double SColorPicker::MAX_ALLOWED_UPDATE_TIME = 0.1;

TWeakPtr<SColorThemesViewer> SColorPicker::ColorThemesViewer;


/* SColorPicker structors
 *****************************************************************************/

SColorPicker::~SColorPicker()
{
	TSharedPtr<SColorThemesViewer> ThemesViewer = ColorThemesViewer.Pin();

	if (ThemesViewer.IsValid())
	{
		ThemesViewer->OnCurrentThemeChanged().RemoveAll(this);
	}
}


/* SColorPicker methods
 *****************************************************************************/

void SColorPicker::Construct( const FArguments& InArgs )
{
	TargetColorAttribute = InArgs._TargetColorAttribute;
	CurrentColorHSV = OldColor = TargetColorAttribute.Get().LinearRGBToHSV();
	CurrentColorRGB = TargetColorAttribute.Get();
	CurrentMode = EColorPickerModes::Wheel;
	TargetFColors = InArgs._TargetFColors.Get();
	TargetLinearColors = InArgs._TargetLinearColors.Get();
	TargetColorChannels = InArgs._TargetColorChannels.Get();
	bUseAlpha = InArgs._UseAlpha;
	bOnlyRefreshOnMouseUp = InArgs._OnlyRefreshOnMouseUp.Get();
	bOnlyRefreshOnOk = InArgs._OnlyRefreshOnOk.Get();
	OnColorCommitted = InArgs._OnColorCommitted;
	PreColorCommitted = InArgs._PreColorCommitted;
	OnColorPickerCancelled = InArgs._OnColorPickerCancelled;
	OnInteractivePickBegin = InArgs._OnInteractivePickBegin;
	OnInteractivePickEnd = InArgs._OnInteractivePickEnd;
	OnColorPickerWindowClosed = InArgs._OnColorPickerWindowClosed;
	ParentWindowPtr = InArgs._ParentWindow.Get();
	DisplayGamma = InArgs._DisplayGamma;
	bClosedViaOkOrCancel = false;
	bValidCreationOverrideExists = InArgs._OverrideColorPickerCreation;

	if ( InArgs._sRGBOverride.IsSet() )
	{
		OriginalSRGBOption = SColorThemesViewer::bSRGBEnabled;
		SColorThemesViewer::bSRGBEnabled = InArgs._sRGBOverride.GetValue();
	}

	RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SColorPicker::AnimatePostConstruct ) );

	// We need a parent window to set the close callback
	if (ParentWindowPtr.IsValid())
	{
		ParentWindowPtr.Pin()->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SColorPicker::HandleParentWindowClosed));
	}

	bColorPickerIsInlineVersion = InArgs._DisplayInlineVersion;
	bIsInteractive = false;
	bPerfIsTooSlowToUpdate = false;
	

	BackupColors();

	BeginAnimation(FLinearColor(ForceInit), CurrentColorHSV);

	bool bAdvancedSectionExpanded = false;

	if (!FPaths::FileExists(GEditorPerProjectIni))
	{
		bool WheelMode = true;

		GConfig->GetBool(TEXT("ColorPickerUI"), TEXT("bWheelMode"), WheelMode, GEditorPerProjectIni);
		GConfig->GetBool(TEXT("ColorPickerUI"), TEXT("bAdvancedSectionExpanded"), bAdvancedSectionExpanded, GEditorPerProjectIni);
		GConfig->GetBool(TEXT("ColorPickerUI"), TEXT("bSRGBEnabled"), SColorThemesViewer::bSRGBEnabled, GEditorPerProjectIni);

		CurrentMode = WheelMode ? EColorPickerModes::Wheel : EColorPickerModes::Spectrum;
	}

	bAdvancedSectionExpanded |= InArgs._ExpandAdvancedSection;

	if (bColorPickerIsInlineVersion)
	{
		GenerateInlineColorPickerContent();
	}
	else
	{
		GenerateDefaultColorPickerContent(bAdvancedSectionExpanded);
	}
}


/* SColorPicker implementation
 *****************************************************************************/

void SColorPicker::BackupColors()
{
	OldTargetFColors.Empty();
	for (int32 i = 0; i < TargetFColors.Num(); ++i)
	{
		OldTargetFColors.Add( *TargetFColors[i] );
	}

	OldTargetLinearColors.Empty();
	for (int32 i = 0; i < TargetLinearColors.Num(); ++i)
	{
		OldTargetLinearColors.Add( *TargetLinearColors[i] );
	}

	OldTargetColorChannels.Empty();
	for (int32 i = 0; i < TargetColorChannels.Num(); ++i)
	{
		// Remap the color channel as a linear color for ease
		const FColorChannels& Channel = TargetColorChannels[i];
		const FLinearColor Color( Channel.Red ? *Channel.Red : 0.f, Channel.Green ? *Channel.Green : 0.f, Channel.Blue ? *Channel.Blue : 0.f, Channel.Alpha ? *Channel.Alpha : 0.f );
		OldTargetColorChannels.Add( Color );
	}
}


void SColorPicker::RestoreColors()
{
	check(TargetFColors.Num() == OldTargetFColors.Num());

	for (int32 i = 0; i < TargetFColors.Num(); ++i)
	{
		*TargetFColors[i] = OldTargetFColors[i];
	}

	check(TargetLinearColors.Num() == OldTargetLinearColors.Num());

	for (int32 i = 0; i < TargetLinearColors.Num(); ++i)
	{
		*TargetLinearColors[i] = OldTargetLinearColors[i];
	}

	check(TargetColorChannels.Num() == OldTargetColorChannels.Num());

	for (int32 i = 0; i < TargetColorChannels.Num(); ++i)
	{
		// Copy back out of the linear to the color channel
		FColorChannels& Channel = TargetColorChannels[i];
		const FLinearColor& OldChannel = OldTargetColorChannels[i];
		if (Channel.Red)
		{
			*Channel.Red = OldChannel.R;
		}
		if (Channel.Green)
		{
			*Channel.Green = OldChannel.G;
		}
		if (Channel.Blue)
		{
			*Channel.Blue = OldChannel.B;
		}
		if (Channel.Alpha)
		{
			*Channel.Alpha = OldChannel.A;
		}
	}
}


void SColorPicker::SetColors(const FLinearColor& InColor)
{
	for (int32 i = 0; i < TargetFColors.Num(); ++i)
	{
		*TargetFColors[i] = InColor.ToFColor(true);
	}

	for (int32 i = 0; i < TargetLinearColors.Num(); ++i)
	{
		*TargetLinearColors[i] = InColor;
	}

	for (int32 i = 0; i < TargetColorChannels.Num(); ++i)
	{
		// Only set those channels who have a valid ptr
		FColorChannels& Channel = TargetColorChannels[i];
		if (Channel.Red)
		{
			*Channel.Red = InColor.R;
		}
		if (Channel.Green)
		{
			*Channel.Green = InColor.G;
		}
		if (Channel.Blue)
		{
			*Channel.Blue = InColor.B;
		}
		if (Channel.Alpha)
		{
			*Channel.Alpha = InColor.A;
		}
	}
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SColorPicker::GenerateDefaultColorPickerContent( bool bAdvancedSectionExpanded )
{	
	// The height of the gradient bars beneath the sliders
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::Get().GetFontStyle("ColorPicker.Font");

	TSharedPtr<SColorThemesViewer> ThemesViewer = ColorThemesViewer.Pin();

	if (!ThemesViewer.IsValid())
	{
		ThemesViewer = SNew(SColorThemesViewer);
		ColorThemesViewer = ThemesViewer;
	}

	ThemesViewer->OnCurrentThemeChanged().AddSP(this, &SColorPicker::HandleThemesViewerThemeChanged);
	ThemesViewer->SetUseAlpha(bUseAlpha);
	ThemesViewer->MenuToStandardNoReturn();

	// The standard button to open the color themes can temporarily become a trash for colors
	ColorThemeComboButton = SNew(SComboButton)
		.ContentPadding(3.0f)
		.MenuPlacement(MenuPlacement_ComboBox)
		.ToolTipText(LOCTEXT("OpenThemeManagerToolTip", "Open Color Theme Manager"));

	ColorThemeComboButton->SetMenuContent(ThemesViewer.ToSharedRef());

	SmallTrash = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SColorTrash)
				.UsesSmallIcon(true)
		];
	
	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SGridPanel)
					.FillColumn(0, 1.0f)

				+ SGridPanel::Slot(0, 0)
					.Padding(0.0f, 1.0f, 20.0f, 1.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(0.0f, 1.0f)
							[
								SNew(SOverlay)

								+ SOverlay::Slot()
									[
										// color theme bar
										SAssignNew(CurrentThemeBar, SThemeColorBlocksBar)
											.ColorTheme(this, &SColorPicker::HandleThemeBarColorTheme)
											.EmptyText(LOCTEXT("EmptyBarHint", "Drag & drop colors here to save"))
											.HideTrashCallback(this, &SColorPicker::HideSmallTrash)
											.ShowTrashCallback(this, &SColorPicker::ShowSmallTrash)
											.ToolTipText(LOCTEXT("CurrentThemeBarToolTip", "Current Color Theme"))
											.UseAlpha(SharedThis(this), &SColorPicker::HandleThemeBarUseAlpha)
											.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
											.OnSelectColor(this, &SColorPicker::HandleThemeBarColorSelected)
									]

								// hack: need to fix SThemeColorBlocksBar::EmptyText to render properly
								+ SOverlay::Slot()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("EmptyBarHint", "Drag & drop colors here to save"))
										.Visibility(this, &SColorPicker::HandleThemeBarHintVisibility)
									]
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								// color theme selector
								SAssignNew(ColorThemeButtonOrSmallTrash, SBorder)
									.BorderImage(FStyleDefaults::GetNoBrush())
									.Padding(0.0f)
							]
					]

				+ SGridPanel::Slot(1, 0)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						// sRGB check box
						SNew(SCheckBox)
							.ToolTipText(LOCTEXT("SRGBCheckboxToolTip", "Toggle gamma corrected sRGB previewing"))
							.IsChecked(this, &SColorPicker::HandleSRGBCheckBoxIsChecked)
							.OnCheckStateChanged(this, &SColorPicker::HandleSRGBCheckBoxCheckStateChanged)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("SRGBCheckboxLabel", "sRGB Preview"))
							]
					]

				+ SGridPanel::Slot(0, 1)
					.Padding(0.0f, 8.0f, 20.0f, 0.0f)
					[
						SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
							.Padding(0.0f)
							.OnMouseButtonDown(this, &SColorPicker::HandleColorAreaMouseDown)
							[
								SNew(SOverlay)

								// color wheel
								+ SOverlay::Slot()
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.HAlign(HAlign_Center)
											[
												SNew(SColorWheel)
													.SelectedColor(this, &SColorPicker::GetCurrentColor)
													.Visibility(this, &SColorPicker::HandleColorPickerModeVisibility, EColorPickerModes::Wheel)
													.OnValueChanged(this, &SColorPicker::HandleColorSpectrumValueChanged)
													.OnMouseCaptureBegin(this, &SColorPicker::HandleInteractiveChangeBegin)
													.OnMouseCaptureEnd(this, &SColorPicker::HandleInteractiveChangeEnd)
											]

										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(4.0f, 0.0f)
											[
												// saturation slider
												MakeColorSlider(EColorPickerChannels::Saturation)
											]

										+ SHorizontalBox::Slot()
											.AutoWidth()
											[
												// value slider
												MakeColorSlider(EColorPickerChannels::Value)
											]
									]

								// color spectrum
								+ SOverlay::Slot()
									[
										SNew(SBox)
											.HeightOverride(200.0f)
											.WidthOverride(292.0f)
											[
												SNew(SColorSpectrum)
													.SelectedColor(this, &SColorPicker::GetCurrentColor)
													.Visibility(this, &SColorPicker::HandleColorPickerModeVisibility, EColorPickerModes::Spectrum)
													.OnValueChanged(this, &SColorPicker::HandleColorSpectrumValueChanged)
													.OnMouseCaptureBegin(this, &SColorPicker::HandleInteractiveChangeBegin)
													.OnMouseCaptureEnd(this, &SColorPicker::HandleInteractiveChangeEnd)
											]
									]
							]
					]

				+ SGridPanel::Slot(1, 1)
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
									.HeightOverride(100.0f)
									.WidthOverride(70.0f)
									[
										// color preview
										MakeColorPreviewBox()
									]
							]

						+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 16.0f, 0.0f, 0.0f)
							.VAlign(VAlign_Top)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									.HAlign(HAlign_Left)
									[
										// mode selector
										SNew(SButton)
											.OnClicked(this, &SColorPicker::HandleColorPickerModeButtonClicked)
											.Content()
											[
												SNew(SImage)
													.Image(FCoreStyle::Get().GetBrush("ColorPicker.Mode"))
													.ToolTipText(LOCTEXT("ColorPickerModeEToolTip", "Toggle between color wheel and color spectrum."))
											]									
									]

								+ SHorizontalBox::Slot()
									.HAlign(HAlign_Right)
									[
										// eye dropper
										SNew(SEyeDropperButton)
											.OnValueChanged(this, &SColorPicker::HandleRGBColorChanged)
											.OnBegin(this, &SColorPicker::HandleInteractiveChangeBegin)
											.OnComplete(this, &SColorPicker::HandleEyeDropperButtonComplete)
											.DisplayGamma(DisplayGamma)
											.Visibility(bValidCreationOverrideExists ? EVisibility::Collapsed : EVisibility::Visible)
									]
							]
					]
			]

		// advanced settings
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("AdvancedAreaTitle", "Advanced"))
					.BorderBackgroundColor(FLinearColor::Transparent)
					.InitiallyCollapsed(!bAdvancedSectionExpanded)
					.OnAreaExpansionChanged(this, &SColorPicker::HandleAdvancedAreaExpansionChanged)
					.Padding(FMargin(0.0f, 1.0f, 0.0f, 8.0f))
					.BodyContent()
					[
						SNew(SHorizontalBox)

						// RGBA inputs
						+ SHorizontalBox::Slot()
							.Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(SVerticalBox)

								// Red
								+ SVerticalBox::Slot()
									[
										MakeColorSpinBox(EColorPickerChannels::Red)
									]

								// Green
								+ SVerticalBox::Slot()
									.Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										MakeColorSpinBox(EColorPickerChannels::Green)
									]

								// Blue
								+ SVerticalBox::Slot()
									.Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										MakeColorSpinBox(EColorPickerChannels::Blue)
									]

								// Alpha
								+ SVerticalBox::Slot()
									.Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										MakeColorSpinBox(EColorPickerChannels::Alpha)
									]
							]

						// HSV & Hex inputs
						+ SHorizontalBox::Slot()
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SVerticalBox)
							
								// Hue
								+ SVerticalBox::Slot()
								[
									MakeColorSpinBox(EColorPickerChannels::Hue)
								]

								// Saturation
								+ SVerticalBox::Slot()
								.Padding(0.0f, 8.0f, 0.0f, 0.0f)
								[
									MakeColorSpinBox(EColorPickerChannels::Saturation)
								]

								// Value
								+ SVerticalBox::Slot()
								.Padding(0.0f, 8.0f, 0.0f, 0.0f)
								[
									MakeColorSpinBox(EColorPickerChannels::Value)
								]

								// Hex linear
								+ SVerticalBox::Slot()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Top)
									.Padding(0.0f, 12.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
											.ToolTipText(LOCTEXT("HexLinearSliderToolTip", "Hexadecimal Linear Value"))

										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(0.0f, 0.0f, 4.0f, 0.0f)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
													.Text(LOCTEXT("HexLinearInputLabel", "Hex Linear"))
											]

										+ SHorizontalBox::Slot()
											.AutoWidth()
											.MaxWidth(72.0f)
											[
												SNew(SEditableTextBox)
													.MinDesiredWidth(72.0f)
													.Text(this, &SColorPicker::HandleHexLinearBoxText)
													.OnTextCommitted(this, &SColorPicker::HandleHexLinearInputTextCommitted)
											]
								]

								// Hex sRGB
								+ SVerticalBox::Slot()
									.HAlign(HAlign_Right)
									.VAlign(VAlign_Top)
									.Padding(0.0f, 8.0f, 0.0f, 0.0f)
									[
										SNew(SHorizontalBox)
										.ToolTipText(LOCTEXT("HexSRGBSliderToolTip", "Hexadecimal sRGB Value"))

										+ SHorizontalBox::Slot()
											.AutoWidth()
											.Padding(0.0f, 0.0f, 4.0f, 0.0f)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("HexSRGBInputLabel", "Hex sRGB"))
											]

										+ SHorizontalBox::Slot()
											.AutoWidth()
											.MaxWidth(72.0f)
											[
												SNew(SEditableTextBox)
												.MinDesiredWidth(72.0f)
												.Text(this, &SColorPicker::HandleHexSRGBBoxText)
												.OnTextCommitted(this, &SColorPicker::HandleHexSRGBInputTextCommitted)
											]
								]
						]
				]
			]

		// dialog buttons
		+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SUniformGridPanel)
					.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))
					.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.Visibility((ParentWindowPtr.IsValid() || bValidCreationOverrideExists) ? EVisibility::Visible : EVisibility::Collapsed)

				+ SUniformGridPanel::Slot(0, 0)
					[
						// ok button
						SNew(SButton)
							.ContentPadding( FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding") )
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("OKButton", "OK"))
							.OnClicked(this, &SColorPicker::HandleOkButtonClicked)
					]

				+ SUniformGridPanel::Slot(1, 0)
					[
						// cancel button
						SNew(SButton)
							.ContentPadding( FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding") )
							.HAlign(HAlign_Center)
							.Text(LOCTEXT("CancelButton", "Cancel"))
							.OnClicked(this, &SColorPicker::HandleCancelButtonClicked)
					]
			]
	];
	
	HideSmallTrash();
}

EActiveTimerReturnType SColorPicker::AnimatePostConstruct( double InCurrentTime, float InDeltaTime )
{
	static const float AnimationTime = 0.25f;

	EActiveTimerReturnType TickReturnVal = EActiveTimerReturnType::Continue;
	if ( CurrentTime < AnimationTime )
	{
		CurrentColorHSV = FMath::Lerp( ColorBegin, ColorEnd, CurrentTime / AnimationTime );
		if ( CurrentColorHSV.R < 0.f )
		{
			CurrentColorHSV.R += 360.f;
		}
		else if ( CurrentColorHSV.R > 360.f )
		{
			CurrentColorHSV.R -= 360.f;
		}

		CurrentTime += InDeltaTime;
		if ( CurrentTime >= AnimationTime )
		{
			CurrentColorHSV = ColorEnd;
			TickReturnVal = EActiveTimerReturnType::Stop;
		}

		CurrentColorRGB = CurrentColorHSV.HSVToLinearRGB();
	}

	return TickReturnVal;
}

void SColorPicker::GenerateInlineColorPickerContent()
{
	TSharedRef<SWidget> AlphaSlider = SNullWidget::NullWidget;
	if (bUseAlpha.Get())
	{
		AlphaSlider = MakeColorSlider(EColorPickerChannels::Alpha);
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Center)
			[
				SNew(SColorWheel)
					.SelectedColor(this, &SColorPicker::GetCurrentColor)
					.Visibility(this, &SColorPicker::HandleColorPickerModeVisibility, EColorPickerModes::Wheel)
					.OnValueChanged(this, &SColorPicker::HandleColorSpectrumValueChanged)
					.OnMouseCaptureBegin(this, &SColorPicker::HandleInteractiveChangeBegin)
					.OnMouseCaptureEnd(this, &SColorPicker::HandleInteractiveChangeEnd)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			[
				// saturation slider
				MakeColorSlider(EColorPickerChannels::Saturation)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// value slider
				MakeColorSlider(EColorPickerChannels::Value)
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Alpha slider
				AlphaSlider
			]
	];
}





void SColorPicker::DiscardColor()
{
	if (OnColorPickerCancelled.IsBound())
	{
		// let the user decide what to do about cancel
		OnColorPickerCancelled.Execute( OldColor.HSVToLinearRGB() );
	}
	else
	{	
		SetNewTargetColorHSV(OldColor, true);
		RestoreColors();
	}
}


bool SColorPicker::SetNewTargetColorHSV( const FLinearColor& NewValue, bool bForceUpdate /*= false*/ )
{
	CurrentColorHSV = NewValue;
	CurrentColorRGB = NewValue.HSVToLinearRGB();

	return ApplyNewTargetColor(bForceUpdate);
}


bool SColorPicker::SetNewTargetColorRGB( const FLinearColor& NewValue, bool bForceUpdate /*= false*/ )
{
	CurrentColorRGB = NewValue;
	CurrentColorHSV = NewValue.LinearRGBToHSV();

	return ApplyNewTargetColor(bForceUpdate);
}


bool SColorPicker::ApplyNewTargetColor( bool bForceUpdate /*= false*/ )
{
	bool bUpdated = false;

	if ((bForceUpdate || (!bOnlyRefreshOnMouseUp && !bPerfIsTooSlowToUpdate)) && (!bOnlyRefreshOnOk || bColorPickerIsInlineVersion))
	{
		double StartUpdateTime = FPlatformTime::Seconds();
		UpdateColorPickMouseUp();
		double EndUpdateTime = FPlatformTime::Seconds();

		if (EndUpdateTime - StartUpdateTime > MAX_ALLOWED_UPDATE_TIME)
		{
			bPerfIsTooSlowToUpdate = true;
		}

		bUpdated = true;
	}

	return bUpdated;
}


void SColorPicker::UpdateColorPickMouseUp()
{
	if (!bOnlyRefreshOnOk || bColorPickerIsInlineVersion)
	{
		UpdateColorPick();
	}
}


void SColorPicker::UpdateColorPick()
{
	bPerfIsTooSlowToUpdate = false;
	FLinearColor OutColor = CurrentColorRGB;

	PreColorCommitted.ExecuteIfBound(OutColor);

	SetColors(OutColor);
	OnColorCommitted.ExecuteIfBound(OutColor);
	
	// This callback is only necessary for wx backwards compatibility
	FCoreDelegates::ColorPickerChanged.Broadcast();
}


void SColorPicker::BeginAnimation( FLinearColor Start, FLinearColor End )
{
	ColorEnd = End;
	ColorBegin = Start;
	CurrentTime = 0.f;
	
	// wraparound with hue
	float HueDif = FMath::Abs(ColorBegin.R - ColorEnd.R);
	if (FMath::Abs(ColorBegin.R + 360.f - ColorEnd.R) < HueDif)
	{
		ColorBegin.R += 360.f;
	}
	else if (FMath::Abs(ColorBegin.R - 360.f - ColorEnd.R) < HueDif)
	{
		ColorBegin.R -= 360.f;
	}
}


void SColorPicker::HideSmallTrash()
{
	if (ColorThemeComboButton.IsValid())
	{
		ColorThemeButtonOrSmallTrash->SetContent(ColorThemeComboButton.ToSharedRef());
	}
	else
	{
		ColorThemeButtonOrSmallTrash->ClearContent();
	}
}


void SColorPicker::ShowSmallTrash()
{
	if ( SmallTrash.IsValid() )
	{
		ColorThemeButtonOrSmallTrash->SetContent(SmallTrash.ToSharedRef());
	}
	else
	{
		ColorThemeButtonOrSmallTrash->ClearContent();
	}
}


/* SColorPicker implementation
 *****************************************************************************/

void SColorPicker::CycleMode()
{
	if (CurrentMode == EColorPickerModes::Spectrum)
	{
		CurrentMode = EColorPickerModes::Wheel;
	}
	else
	{
		CurrentMode = EColorPickerModes::Spectrum;
	}
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SColorPicker::MakeColorSlider( EColorPickerChannels Channel ) const
{
	FText SliderTooltip;

	switch (Channel)
	{
	case EColorPickerChannels::Red:			SliderTooltip = LOCTEXT("RedSliderToolTip", "Red"); break;
	case EColorPickerChannels::Green:		SliderTooltip = LOCTEXT("GreenSliderToolTip", "Green"); break;
	case EColorPickerChannels::Blue:		SliderTooltip = LOCTEXT("BlueSliderToolTip", "Blue"); break;
	case EColorPickerChannels::Alpha:		SliderTooltip = LOCTEXT("AlphaSliderToolTip", "Alpha"); break;
	case EColorPickerChannels::Hue:			SliderTooltip = LOCTEXT("HueSliderToolTip", "Hue"); break;
	case EColorPickerChannels::Saturation:	SliderTooltip = LOCTEXT("SaturationSliderToolTip", "Saturation"); break;
	case EColorPickerChannels::Value:		SliderTooltip = LOCTEXT("ValueSliderToolTip", "Value"); break;
	default:
		return SNullWidget::NullWidget;
	}

	return SNew(SOverlay)
		.ToolTipText(SliderTooltip)

	+ SOverlay::Slot()
		.Padding(FMargin(4.0f, 0.0f))
		[
			SNew(SSimpleGradient)
				.EndColor(this, &SColorPicker::HandleColorSliderEndColor, Channel)
				.StartColor(this, &SColorPicker::HandleColorSliderStartColor, Channel)
				.Orientation(Orient_Horizontal)
				.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
		]

	+ SOverlay::Slot()
		[
			SNew(SSlider)
				.IndentHandle(false)
				.Orientation(Orient_Vertical)
				.SliderBarColor(FLinearColor::Transparent)
				.Style(&FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("ColorPicker.Slider"))
				.Value(this, &SColorPicker::HandleColorSpinBoxValue, Channel)
				.OnMouseCaptureBegin(this, &SColorPicker::HandleInteractiveChangeBegin)
				.OnMouseCaptureEnd(this, &SColorPicker::HandleInteractiveChangeEnd)
				.OnValueChanged(this, &SColorPicker::HandleColorSpinBoxValueChanged, Channel)
		];
}


TSharedRef<SWidget> SColorPicker::MakeColorSpinBox( EColorPickerChannels Channel ) const
{
	if ((Channel == EColorPickerChannels::Alpha) && !bUseAlpha.Get())
	{
		return SNullWidget::NullWidget;
	}

	const int32 GradientHeight = 6;
	const float HDRMaxValue = (TargetFColors.Num()) ? 1.f : FLT_MAX;
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::Get().GetFontStyle("ColorPicker.Font");

	// create gradient widget
	TSharedPtr<SWidget> GradientWidget;
	
	if (Channel == EColorPickerChannels::Hue)
	{
		TArray<FLinearColor> HueGradientColors;

		for (int32 i = 0; i < 7; ++i)
		{
			HueGradientColors.Add( FLinearColor((i % 6) * 60.f, 1.f, 1.f).HSVToLinearRGB() );
		}

		GradientWidget = SNew(SComplexGradient)
			.GradientColors(HueGradientColors);
	}
	else
	{
		GradientWidget = SNew(SSimpleGradient)
			.StartColor(this, &SColorPicker::GetGradientStartColor, Channel)
			.EndColor(this, &SColorPicker::GetGradientEndColor, Channel)
			.HasAlphaBackground(Channel == EColorPickerChannels::Alpha)
			.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB);
	}
	
	// create spin box
	float MaxValue;
	FText SliderLabel;
	FText SliderTooltip;

	switch (Channel)
	{
	case EColorPickerChannels::Red:
		MaxValue = HDRMaxValue;
		SliderLabel = LOCTEXT("RedSliderLabel", "R");
		SliderTooltip = LOCTEXT("RedSliderToolTip", "Red");
		break;
		
	case EColorPickerChannels::Green:
		MaxValue = HDRMaxValue;
		SliderLabel = LOCTEXT("GreenSliderLabel", "G");
		SliderTooltip = LOCTEXT("GreenSliderToolTip", "Green");
		break;
		
	case EColorPickerChannels::Blue:
		MaxValue = HDRMaxValue;
		SliderLabel = LOCTEXT("BlueSliderLabel", "B");
		SliderTooltip = LOCTEXT("BlueSliderToolTip", "Blue");
		break;
		
	case EColorPickerChannels::Alpha:
		MaxValue = HDRMaxValue;
		SliderLabel = LOCTEXT("AlphaSliderLabel", "A");
		SliderTooltip = LOCTEXT("AlphaSliderToolTip", "Alpha");
		break;
		
	case EColorPickerChannels::Hue:
		MaxValue = 359.0f;
		SliderLabel = LOCTEXT("HueSliderLabel", "H");
		SliderTooltip = LOCTEXT("HueSliderToolTip", "Hue");
		break;
		
	case EColorPickerChannels::Saturation:
		MaxValue = 1.0f;
		SliderLabel = LOCTEXT("SaturationSliderLabel", "S");
		SliderTooltip = LOCTEXT("SaturationSliderToolTip", "Saturation");
		break;
		
	case EColorPickerChannels::Value:
		MaxValue = HDRMaxValue;
		SliderLabel = LOCTEXT("ValueSliderLabel", "V");
		SliderTooltip = LOCTEXT("ValueSliderToolTip", "Value");
		break;

	default:
		return SNullWidget::NullWidget;
	}

	// Define a maximum size for the spin box containers to prevent them from stretching out the color picker window
	static const float MaxSpinBoxSize = 192.0f;

	return SNew(SHorizontalBox)

	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(SliderLabel)
		]

	+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.MaxWidth(MaxSpinBoxSize)
		[
			SNew(SVerticalBox)
				.ToolTipText(SliderTooltip)

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSpinBox<float>)
						.MinValue(0.0f)
						.MaxValue(MaxValue)
						.MinSliderValue(0.0f)
						.MaxSliderValue(Channel == EColorPickerChannels::Hue ? 359.0f : 1.0f)
						.Delta(Channel == EColorPickerChannels::Hue ? 1.0f : 0.001f)
						.Font(SmallLayoutFont)
						.Value(this, &SColorPicker::HandleColorSpinBoxValue, Channel)
						.OnBeginSliderMovement(this, &SColorPicker::HandleInteractiveChangeBegin)
						.OnEndSliderMovement(this, &SColorPicker::HandleInteractiveChangeEnd)
						.OnValueChanged(this, &SColorPicker::HandleColorSpinBoxValueChanged, Channel)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
						.HeightOverride(GradientHeight)
						[
							GradientWidget.ToSharedRef()
						]
				]
		];
}


TSharedRef<SWidget> SColorPicker::MakeColorPreviewBox() const
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
		[
			// preview blocks
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("OldColorLabel", "Old"))
				]

			+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						[
							// old color
							SNew(SColorBlock) 
								.ColorIsHSV(true) 
								.IgnoreAlpha(true)
								.ToolTipText(LOCTEXT("OldColorToolTip", "Old color without alpha (drag to theme bar to save)"))
								.Color(OldColor) 
								.OnMouseButtonDown(this, &SColorPicker::HandleOldColorBlockMouseButtonDown, false)
								.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
								.Cursor(EMouseCursor::GrabHand)
						]

					+ SHorizontalBox::Slot()
						[
							// old color (alpha)
							SNew(SColorBlock) 
								.ColorIsHSV(true) 
								.ShowBackgroundForAlpha(true)
								.ToolTipText(LOCTEXT("OldColorAlphaToolTip", "Old color with alpha (drag to theme bar to save)"))
								.Color(OldColor) 
								.Visibility(SharedThis(this), &SColorPicker::HandleAlphaColorBlockVisibility)
								.OnMouseButtonDown(this, &SColorPicker::HandleOldColorBlockMouseButtonDown, true)
								.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
								.Cursor(EMouseCursor::GrabHand)
						]
				]

			+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						[
							// new color
							SNew(SColorBlock) 
								.ColorIsHSV(true) 
								.IgnoreAlpha(true)
								.ToolTipText(LOCTEXT("NewColorToolTip", "New color without alpha (drag to theme bar to save)"))
								.Color(this, &SColorPicker::GetCurrentColor)
								.OnMouseButtonDown(this, &SColorPicker::HandleNewColorBlockMouseButtonDown, false)
								.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
								.Cursor(EMouseCursor::GrabHand)
						]

					+ SHorizontalBox::Slot()
						[
							// new color (alpha)
							SNew(SColorBlock) 
								.ColorIsHSV(true) 
								.ShowBackgroundForAlpha(true)
								.ToolTipText(LOCTEXT("NewColorAlphaToolTip", "New color with alpha (drag to theme bar to save)"))
								.Color(this, &SColorPicker::GetCurrentColor)
								.Visibility(SharedThis(this), &SColorPicker::HandleAlphaColorBlockVisibility)
								.OnMouseButtonDown(this, &SColorPicker::HandleNewColorBlockMouseButtonDown, true)
								.UseSRGB(SharedThis(this), &SColorPicker::HandleColorPickerUseSRGB)
								.Cursor(EMouseCursor::GrabHand)
						]
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("NewColorLabel", "New"))
				]
		]

	+ SOverlay::Slot()
		.VAlign(VAlign_Center)
		[
			// block separators
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
						.HeightOverride(2.0f)
						.WidthOverride(4.0f)
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("ColorPicker.Separator"))
								.Padding(0.0f)
						]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
						.HeightOverride(2.0f)
						.WidthOverride(4.0f)
						[
							SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("ColorPicker.Separator"))
								.Padding(0.0f)
						]
				]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


/* SColorPicker callbacks
 *****************************************************************************/

FLinearColor SColorPicker::GetGradientEndColor( EColorPickerChannels Channel ) const
{
	switch (Channel)
	{
	case EColorPickerChannels::Red:			return FLinearColor(1.0f, CurrentColorRGB.G, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Green:		return FLinearColor(CurrentColorRGB.R, 1.0f, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Blue:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, 1.0f, 1.0f);
	case EColorPickerChannels::Alpha:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Saturation:	return FLinearColor(CurrentColorHSV.R, 1.0f, CurrentColorHSV.B, 1.0f).HSVToLinearRGB();
	case EColorPickerChannels::Value:		return FLinearColor(CurrentColorHSV.R, CurrentColorHSV.G, 1.0f, 1.0f).HSVToLinearRGB();
	default:								return FLinearColor();
	}
}


FLinearColor SColorPicker::GetGradientStartColor( EColorPickerChannels Channel ) const
{
	switch (Channel)
	{
	case EColorPickerChannels::Red:			return FLinearColor(0.0f, CurrentColorRGB.G, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Green:		return FLinearColor(CurrentColorRGB.R, 0.0f, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Blue:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, 0.0f, 1.0f);
	case EColorPickerChannels::Alpha:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, CurrentColorRGB.B, 0.0f);
	case EColorPickerChannels::Saturation:	return FLinearColor(CurrentColorHSV.R, 0.0f, CurrentColorHSV.B, 1.0f).HSVToLinearRGB();
	case EColorPickerChannels::Value:		return FLinearColor(CurrentColorHSV.R, CurrentColorHSV.G, 0.0f, 1.0f).HSVToLinearRGB();
	default:								return FLinearColor();
	}
}


void SColorPicker::HandleAdvancedAreaExpansionChanged( bool Expanded )
{
	if (FPaths::FileExists(GEditorPerProjectIni))
	{
		GConfig->SetBool(TEXT("ColorPickerUI"), TEXT("bAdvancedSectionExpanded"), Expanded, GEditorPerProjectIni);
	}
}


EVisibility SColorPicker::HandleAlphaColorBlockVisibility() const
{
	return bUseAlpha.Get() ? EVisibility::Visible : EVisibility::Collapsed;
}


FReply SColorPicker::HandleCancelButtonClicked()
{
	bClosedViaOkOrCancel = true;

	DiscardColor();
	if (SColorPicker::OnColorPickerDestroyOverride.IsBound())
	{
		SColorPicker::OnColorPickerDestroyOverride.Execute();
	}
	else
	{
		ParentWindowPtr.Pin()->RequestDestroyWindow();
	}

	return FReply::Handled();
}


EVisibility SColorPicker::HandleColorPickerModeVisibility( EColorPickerModes Mode ) const
{
	return (CurrentMode == Mode) ? EVisibility::Visible : EVisibility::Hidden;
}


FLinearColor SColorPicker::HandleColorSliderEndColor( EColorPickerChannels Channel ) const
{
	switch (Channel)
	{
	case EColorPickerChannels::Red:			return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	case EColorPickerChannels::Green:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	case EColorPickerChannels::Blue:		return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	case EColorPickerChannels::Alpha:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, CurrentColorRGB.B, 0.0f);
	case EColorPickerChannels::Saturation:	return FLinearColor(CurrentColorHSV.R, 0.0f, 1.0f, 1.0f).HSVToLinearRGB();
	case EColorPickerChannels::Value:		return FLinearColor(CurrentColorHSV.R, CurrentColorHSV.G, 0.0f, 1.0f).HSVToLinearRGB();
	default:								return FLinearColor();
	}
}


FLinearColor SColorPicker::HandleColorSliderStartColor( EColorPickerChannels Channel ) const
{
	switch (Channel)
	{
	case EColorPickerChannels::Red:			return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	case EColorPickerChannels::Green:		return FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	case EColorPickerChannels::Blue:		return FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	case EColorPickerChannels::Alpha:		return FLinearColor(CurrentColorRGB.R, CurrentColorRGB.G, CurrentColorRGB.B, 1.0f);
	case EColorPickerChannels::Saturation:	return FLinearColor(CurrentColorHSV.R, 1.0f, 1.0f, 1.0f).HSVToLinearRGB();
	case EColorPickerChannels::Value:		return FLinearColor(CurrentColorHSV.R, CurrentColorHSV.G, 1.0f, 1.0f).HSVToLinearRGB();
	default:								return FLinearColor();
	}
}


void SColorPicker::HandleColorSpectrumValueChanged( FLinearColor NewValue )
{
	SetNewTargetColorHSV(NewValue);
}


float SColorPicker::HandleColorSpinBoxValue( EColorPickerChannels Channel ) const
{
	switch (Channel)
	{
	case EColorPickerChannels::Red:			return CurrentColorRGB.R;
	case EColorPickerChannels::Green:		return CurrentColorRGB.G;
	case EColorPickerChannels::Blue:		return CurrentColorRGB.B;
	case EColorPickerChannels::Alpha:		return CurrentColorRGB.A;
	case EColorPickerChannels::Hue:			return CurrentColorHSV.R;
	case EColorPickerChannels::Saturation:	return CurrentColorHSV.G;
	case EColorPickerChannels::Value:		return CurrentColorHSV.B;
	default:								return 0.0f;
	}
}


void SColorPicker::HandleColorSpinBoxValueChanged( float NewValue, EColorPickerChannels Channel )
{
	int32 ComponentIndex;
	bool IsHSV = false;

	switch (Channel)
	{
	case EColorPickerChannels::Red:			ComponentIndex = 0; break;
	case EColorPickerChannels::Green:		ComponentIndex = 1; break;
	case EColorPickerChannels::Blue:		ComponentIndex = 2; break;
	case EColorPickerChannels::Alpha:		ComponentIndex = 3; break;
	case EColorPickerChannels::Hue:			ComponentIndex = 0; IsHSV = true; break;
	case EColorPickerChannels::Saturation:	ComponentIndex = 1; IsHSV = true; break;
	case EColorPickerChannels::Value:		ComponentIndex = 2; IsHSV = true; break;
	default:								
		return;
	}

	FLinearColor& NewColor = IsHSV ? CurrentColorHSV : CurrentColorRGB;

	if (FMath::IsNearlyEqual(NewValue, NewColor.Component(ComponentIndex), KINDA_SMALL_NUMBER))
	{
		return;
	}

	NewColor.Component(ComponentIndex) = NewValue;

	if (IsHSV)
	{
		SetNewTargetColorHSV(NewColor, !bIsInteractive);
	}
	else
	{
		SetNewTargetColorRGB(NewColor, !bIsInteractive);
	}
}


void SColorPicker::HandleEyeDropperButtonComplete(bool bCancelled)
{
	bIsInteractive = false;

	if (bCancelled)
	{
		SetNewTargetColorHSV(OldColor, true);
		RestoreColors();
	}

	if (bOnlyRefreshOnMouseUp || bPerfIsTooSlowToUpdate)
	{
		UpdateColorPick();
	}

	OnInteractivePickEnd.ExecuteIfBound();
}


FText SColorPicker::HandleHexLinearBoxText() const
{
	return FText::FromString(CurrentColorRGB.ToFColor(false).ToHex());
}


FText SColorPicker::HandleHexSRGBBoxText() const
{
	return FText::FromString(CurrentColorRGB.ToFColor(true).ToHex());
}


void SColorPicker::HandleHexLinearInputTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (!Text.IsEmpty() && ((CommitType == ETextCommit::OnEnter) || (CommitType == ETextCommit::OnUserMovedFocus)))
	{
		FColor Color = FColor::FromHex(Text.ToString());
		SetNewTargetColorRGB(FLinearColor(Color.R / 255.0f, Color.G / 255.0f, Color.B / 255.0f, Color.A / 255.0f), false);
	}	
}

void SColorPicker::HandleHexSRGBInputTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (!Text.IsEmpty() && ((CommitType == ETextCommit::OnEnter) || (CommitType == ETextCommit::OnUserMovedFocus)))
	{
		FColor Color = FColor::FromHex(Text.ToString());
		float red = Color.R / 255.0f;
		float green = Color.G / 255.0f;
		float blue = Color.B / 255.0f;
		float alpha = Color.A / 255.0f;

		red = red <= 0.04045f ? red / 12.92f : FMath::Pow((red + 0.055f) / 1.055f, 2.4f);
		green = green <= 0.04045f ? green / 12.92f : FMath::Pow((green + 0.055f) / 1.055f, 2.4f);
		blue = blue <= 0.04045f ? blue / 12.92f : FMath::Pow((blue + 0.055f) / 1.055f, 2.4f);
		
		SetNewTargetColorRGB(FLinearColor(red, green, blue, alpha), false);
	}
}


void SColorPicker::HandleHSVColorChanged( FLinearColor NewValue )
{
	SetNewTargetColorHSV(NewValue);
}


void SColorPicker::HandleInteractiveChangeBegin()
{
	if( bIsInteractive && OnInteractivePickEnd.IsBound() )
	{
		OnInteractivePickEnd.Execute();
	}

	OnInteractivePickBegin.ExecuteIfBound();
	bIsInteractive = true;
}


void SColorPicker::HandleInteractiveChangeEnd()
{
	HandleInteractiveChangeEnd(0.0f);
}


void SColorPicker::HandleInteractiveChangeEnd( float NewValue )
{
	bIsInteractive = false;

	UpdateColorPickMouseUp();
	OnInteractivePickEnd.ExecuteIfBound();
}


FReply SColorPicker::HandleColorAreaMouseDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CycleMode();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SColorPicker::HandleColorPickerModeButtonClicked()
{
	CycleMode();

	if (FPaths::FileExists(GEditorPerProjectIni))
	{
		GConfig->SetBool(TEXT("ColorPickerUI"), TEXT("bWheelMode"), CurrentMode == EColorPickerModes::Wheel, GEditorPerProjectIni);
	}

	return FReply::Handled();
}


FReply SColorPicker::HandleNewColorBlockMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bCheckAlpha )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TSharedRef<FColorDragDrop> Operation = FColorDragDrop::New(
			CurrentColorHSV,
			SColorThemesViewer::bSRGBEnabled,
			bCheckAlpha ? bUseAlpha.Get() : false,
			FSimpleDelegate::CreateSP(this, &SColorPicker::ShowSmallTrash),
			FSimpleDelegate::CreateSP(this, &SColorPicker::HideSmallTrash)
		);

		return FReply::Handled().BeginDragDrop(Operation);
	}

	return FReply::Unhandled();
}


FReply SColorPicker::HandleOkButtonClicked()
{
	bClosedViaOkOrCancel = true;

	UpdateColorPick();

	if (SColorPicker::OnColorPickerDestroyOverride.IsBound())
	{
		SColorPicker::OnColorPickerDestroyOverride.Execute();
	}
	else
	{
		ParentWindowPtr.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}


FReply SColorPicker::HandleOldColorBlockMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bCheckAlpha )
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TSharedRef<FColorDragDrop> Operation = FColorDragDrop::New(
			OldColor,
			SColorThemesViewer::bSRGBEnabled,
			bCheckAlpha ? bUseAlpha.Get() : false,
			FSimpleDelegate::CreateSP(this, &SColorPicker::ShowSmallTrash),
			FSimpleDelegate::CreateSP(this, &SColorPicker::HideSmallTrash)
		);

		return FReply::Handled().BeginDragDrop(Operation);
	}

	return FReply::Unhandled();
}


bool SColorPicker::HandleColorPickerUseSRGB() const
{
	return SColorThemesViewer::bSRGBEnabled;
}


void SColorPicker::HandleParentWindowClosed( const TSharedRef<SWindow>& Window )
{
	check(Window == ParentWindowPtr.Pin());

	// End picking interaction if still active
	if( bIsInteractive && OnInteractivePickEnd.IsBound() )
	{
		OnInteractivePickEnd.Execute();
		bIsInteractive = false;
	}

	// We always have to call the close callback
	if (OnColorPickerWindowClosed.IsBound())
	{
		OnColorPickerWindowClosed.Execute(Window);
	}

	// If we weren't closed via the OK or Cancel button, we need to perform the default close action
	if (!bClosedViaOkOrCancel && bOnlyRefreshOnOk)
	{
		DiscardColor();
	}

	if ( OriginalSRGBOption.IsSet() )
	{
		SColorThemesViewer::bSRGBEnabled = OriginalSRGBOption.GetValue();
	}
}


void SColorPicker::HandleRGBColorChanged( FLinearColor NewValue )
{
	SetNewTargetColorRGB(NewValue);
}


void SColorPicker::HandleSRGBCheckBoxCheckStateChanged( ECheckBoxState InIsChecked )
{
	SColorThemesViewer::bSRGBEnabled = (InIsChecked == ECheckBoxState::Checked);

	if (FPaths::FileExists(GEditorPerProjectIni))
	{
		GConfig->SetBool(TEXT("ColorPickerUI"), TEXT("bSRGBEnabled"), SColorThemesViewer::bSRGBEnabled, GEditorPerProjectIni);
	}
}


ECheckBoxState SColorPicker::HandleSRGBCheckBoxIsChecked() const
{
	return SColorThemesViewer::bSRGBEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void SColorPicker::HandleThemeBarColorSelected( FLinearColor NewValue )
{
	// Force the alpha component to 1 when we don't care about the alpha
	if (!bUseAlpha.Get())
	{
		NewValue.A = 1.0f;
	}

	BeginAnimation(CurrentColorHSV, NewValue);
	SetNewTargetColorHSV(NewValue, true);
}


TSharedPtr<FColorTheme> SColorPicker::HandleThemeBarColorTheme() const
{
	TSharedPtr<SColorThemesViewer> ThemesViewer = ColorThemesViewer.Pin();

	if (ThemesViewer.IsValid())
	{
		return ThemesViewer->GetCurrentColorTheme();
	}

	return nullptr;
}


EVisibility SColorPicker::HandleThemeBarHintVisibility() const
{
	TSharedPtr<SColorThemesViewer> ThemesViewer = ColorThemesViewer.Pin();

	if (ThemesViewer.IsValid())
	{
		TSharedPtr<FColorTheme> SelectedTheme = ThemesViewer->GetCurrentColorTheme();

		if (SelectedTheme.IsValid() && SelectedTheme->GetColors().Num() == 0)
		{
			return EVisibility::HitTestInvisible;
		}
	}

	return EVisibility::Hidden;
}


bool SColorPicker::HandleThemeBarUseAlpha() const
{
	return bUseAlpha.Get();
}


void SColorPicker::HandleThemesViewerThemeChanged()
{
	if (CurrentThemeBar.IsValid())
	{
		CurrentThemeBar->RemoveRefreshCallback();
		CurrentThemeBar->AddRefreshCallback();
		CurrentThemeBar->Refresh();
	}
}

// Static delegates to access whether or not the override is bound in the global Open/Destroy functions
SColorPicker::FOnColorPickerCreationOverride SColorPicker::OnColorPickerNonModalCreateOverride;
SColorPicker::FOnColorPickerDestructionOverride SColorPicker::OnColorPickerDestroyOverride;

/* Global functions
 *****************************************************************************/

/** A static color picker that everything should use. */
static TWeakPtr<SWindow> ColorPickerWindow;


bool OpenColorPicker(const FColorPickerArgs& Args)
{
	DestroyColorPicker();

	bool Result = false;

	// Consoles do not support opening new windows
#if PLATFORM_DESKTOP
	FLinearColor OldColor = Args.InitialColorOverride;

	if (Args.ColorArray && Args.ColorArray->Num() > 0)
	{
		OldColor = FLinearColor(*(*Args.ColorArray)[0]);
	}
	else if (Args.LinearColorArray && Args.LinearColorArray->Num() > 0)
	{
		OldColor = *(*Args.LinearColorArray)[0];
	}
	else if (Args.ColorChannelsArray && Args.ColorChannelsArray->Num() > 0)
	{
		OldColor.R = (*Args.ColorChannelsArray)[0].Red ? *(*Args.ColorChannelsArray)[0].Red : 0.0f;
		OldColor.G = (*Args.ColorChannelsArray)[0].Green ? *(*Args.ColorChannelsArray)[0].Green : 0.0f;
		OldColor.B = (*Args.ColorChannelsArray)[0].Blue ? *(*Args.ColorChannelsArray)[0].Blue : 0.0f;
		OldColor.A = (*Args.ColorChannelsArray)[0].Alpha ? *(*Args.ColorChannelsArray)[0].Alpha : 0.0f;
	}
	else
	{
		check(Args.OnColorCommitted.IsBound());
	}
		
	// Determine the position of the window so that it will spawn near the mouse, but not go off the screen.
	FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	FSlateRect Anchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);

	FVector2D AdjustedSummonLocation = FSlateApplication::Get().CalculatePopupWindowPosition( Anchor, SColorPicker::DEFAULT_WINDOW_SIZE, true, FVector2D::ZeroVector, Orient_Horizontal );


	// Only override the color picker window creation behavior if we are not creating a modal color picker
	const bool bOverrideNonModalCreation = (SColorPicker::OnColorPickerNonModalCreateOverride.IsBound() && !Args.bIsModal);

	TSharedPtr<SWindow> Window = nullptr;
	TSharedRef<SBorder> WindowContent = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f, 8.0f));
	
	bool bNeedToAddWindow = true;
	if (!bOverrideNonModalCreation)
	{
		if (Args.bOpenAsMenu && !Args.bIsModal && Args.ParentWidget.IsValid())
		{
			Window = FSlateApplication::Get().PushMenu(
				Args.ParentWidget.ToSharedRef(),
				FWidgetPath(),
				WindowContent,
				AdjustedSummonLocation,
				FPopupTransitionEffect(FPopupTransitionEffect::None),
				false,
				FVector2D(0.f,0.f),
				EPopupMethod::CreateNewWindow,
				false)->GetOwnedWindow();

			bNeedToAddWindow = false;
		}
		else
		{
			Window = SNew(SWindow)
				.AutoCenter(EAutoCenter::None)
				.ScreenPosition(AdjustedSummonLocation)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				.SizingRule(ESizingRule::Autosized)
				.Title(LOCTEXT("WindowHeader", "Color Picker"))
				[
					WindowContent
				];
		}
	}

	TSharedRef<SColorPicker> ColorPicker = SNew(SColorPicker)
		.TargetColorAttribute(OldColor)
		.TargetFColors(Args.ColorArray ? *Args.ColorArray : TArray<FColor*>())
		.TargetLinearColors(Args.LinearColorArray ? *Args.LinearColorArray : TArray<FLinearColor*>())
		.TargetColorChannels(Args.ColorChannelsArray ? *Args.ColorChannelsArray : TArray<FColorChannels>())
		.UseAlpha(Args.bUseAlpha)
		.ExpandAdvancedSection(Args.bExpandAdvancedSection)
		.OnlyRefreshOnMouseUp(Args.bOnlyRefreshOnMouseUp && !Args.bIsModal)
		.OnlyRefreshOnOk(Args.bOnlyRefreshOnOk || Args.bIsModal)
		.OnColorCommitted(Args.OnColorCommitted)
		.PreColorCommitted(Args.PreColorCommitted)
		.OnColorPickerCancelled(Args.OnColorPickerCancelled)
		.OnInteractivePickBegin(Args.OnInteractivePickBegin)
		.OnInteractivePickEnd(Args.OnInteractivePickEnd)
		.OnColorPickerWindowClosed(Args.OnColorPickerWindowClosed)
		.ParentWindow(Window)
		.DisplayGamma(Args.DisplayGamma)
		.sRGBOverride(Args.sRGBOverride)
		.OverrideColorPickerCreation(bOverrideNonModalCreation);
	
	// If the color picker requested is modal, don't override the behavior even if the delegate is bound
	if (bOverrideNonModalCreation)
	{
		SColorPicker::OnColorPickerNonModalCreateOverride.Execute(ColorPicker);

		Result = true;

		//hold on to the window created for external use...
		ColorPickerWindow = Window;
	}
	else
	{
		WindowContent->SetContent(ColorPicker);

		if (Args.bIsModal)
		{
			FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), Args.ParentWidget);
		}
		else if (bNeedToAddWindow)
		{
			if (Args.ParentWidget.IsValid())
			{
				// Find the window of the parent widget
				FWidgetPath WidgetPath;
				FSlateApplication::Get().GeneratePathToWidgetChecked(Args.ParentWidget.ToSharedRef(), WidgetPath);
				Window = FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), WidgetPath.GetWindow());
			}
			else
			{
				Window = FSlateApplication::Get().AddWindow(Window.ToSharedRef());
			}

		}

		Result = true;

		//hold on to the window created for external use...
		ColorPickerWindow = Window;
	}
#endif

	return Result;
}


/**
 * Destroys the current color picker. Necessary if the values the color picker
 * currently targets become invalid.
 */
void DestroyColorPicker()
{
	if (ColorPickerWindow.IsValid())
	{
		if (SColorPicker::OnColorPickerDestroyOverride.IsBound())
		{
			SColorPicker::OnColorPickerDestroyOverride.Execute();
		}
		else
		{
			ColorPickerWindow.Pin()->RequestDestroyWindow();
		}
		ColorPickerWindow.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
