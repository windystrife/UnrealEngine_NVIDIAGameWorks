// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SFoliageEdit.h"
#include "Fonts/SlateFontInfo.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "EditorModeManager.h"
#include "EditorModes.h"

#include "FoliageEditActions.h"
#include "IIntroTutorials.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "SFoliagePalette.h"
#include "SHeader.h"
#include "SSeparator.h"

#define LOCTEXT_NAMESPACE "FoliageEd_Mode"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SFoliageEdit::Construct(const FArguments& InArgs)
{
	FoliageEditMode = (FEdModeFoliage*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Foliage);

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));

	// Everything (or almost) uses this padding, change it to expand the padding.
	FMargin StandardPadding(6.f, 3.f);
	FMargin StandardLeftPadding(6.f, 3.f, 3.f, 3.f);
	FMargin StandardRightPadding(3.f, 3.f, 6.f, 3.f);

	FSlateFontInfo StandardFont = FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));

	const FText BlankText = LOCTEXT("Blank", "");

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.f, 5.f, 0.f, 5.f)
			[
				BuildToolBar()
			]

			+ SHorizontalBox::Slot()
			.Padding(0.f, 2.f, 2.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(StandardPadding)
				[
					SNew(SVerticalBox)

					// Active Tool Title
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(StandardLeftPadding)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(this, &SFoliageEdit::GetActiveToolName)
							.TextStyle(FEditorStyle::Get(), "FoliageEditMode.ActiveToolName.Text")
						]

						+ SHorizontalBox::Slot()
						.Padding(StandardRightPadding)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							// Tutorial link
							IntroTutorials.CreateTutorialsWidget(TEXT("FoliageMode"))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(StandardPadding)
					[
						SNew(SHeader)
						.Visibility(this, &SFoliageEdit::GetVisibility_Options)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("OptionHeader", "Brush Options"))
							.Font(StandardFont)
						]
					]

					// Brush Size
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.ToolTipText(LOCTEXT("BrushSize_Tooltip", "The size of the foliage brush"))
						.Visibility(this, &SFoliageEdit::GetVisibility_Radius)

						+ SHorizontalBox::Slot()
						.Padding(StandardLeftPadding)
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("BrushSize", "Brush Size"))
							.Font(StandardFont)
						]
						+ SHorizontalBox::Slot()
						.Padding(StandardRightPadding)
						.FillWidth(2.0f)
						.MaxWidth(100.f)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.Font(StandardFont)
							.AllowSpin(true)
							.MinValue(0.0f)
							.MaxValue(65536.0f)
							.MaxSliderValue(8192.0f)
							.MinDesiredValueWidth(50.0f)
							.SliderExponent(3.0f)
							.Value(this, &SFoliageEdit::GetRadius)
							.OnValueChanged(this, &SFoliageEdit::SetRadius)
							.IsEnabled(this, &SFoliageEdit::IsEnabled_BrushSize)
						]						
					]

					// Paint Density
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.ToolTipText(LOCTEXT("PaintDensity_Tooltip", "The density of foliage to paint. This is a multiplier for the individual foliage type's density specifier."))
						.Visibility(this, &SFoliageEdit::GetVisibility_PaintDensity)

						+ SHorizontalBox::Slot()
						.Padding(StandardLeftPadding)
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PaintDensity", "Paint Density"))
							.Font(StandardFont)
						]
						+ SHorizontalBox::Slot()
						.Padding(StandardRightPadding)
						.FillWidth(2.0f)
						.MaxWidth(100.f)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.Font(StandardFont)
							.AllowSpin(true)
							.MinValue(0.0f)
							.MaxValue(2.0f)
							.MaxSliderValue(1.0f)
							.MinDesiredValueWidth(50.0f)
							.Value(this, &SFoliageEdit::GetPaintDensity)
							.OnValueChanged(this, &SFoliageEdit::SetPaintDensity)
							.IsEnabled(this, &SFoliageEdit::IsEnabled_PaintDensity)
						]
					]

					// Erase Density
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.ToolTipText(LOCTEXT("EraseDensity_Tooltip", "The density of foliage to leave behind when erasing with the Shift key held. 0 will remove all foliage."))
						.Visibility(this, &SFoliageEdit::GetVisibility_EraseDensity)

						+ SHorizontalBox::Slot()
						.Padding(StandardLeftPadding)
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EraseDensity", "Erase Density"))
							.Font(StandardFont)
						]
						+ SHorizontalBox::Slot()
						.Padding(StandardRightPadding)
						.FillWidth(2.0f)
						.MaxWidth(100.f)
						.VAlign(VAlign_Center)
						[
							SNew(SNumericEntryBox<float>)
							.Font(StandardFont)
							.AllowSpin(true)
							.MinValue(0.0f)
							.MaxValue(2.0f)
							.MaxSliderValue(1.0f)
							.MinDesiredValueWidth(50.0f)
							.Value(this, &SFoliageEdit::GetEraseDensity)
							.OnValueChanged(this, &SFoliageEdit::SetEraseDensity)
							.IsEnabled(this, &SFoliageEdit::IsEnabled_EraseDensity)
						]
					]					
					
					+ SVerticalBox::Slot()
					.Padding(StandardPadding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Options)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(StandardPadding)
						[
							SNew(SWrapBox)
							.UseAllottedWidth(true)
							.InnerSlotPadding({6, 5})

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(150)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_SingleInstantiationMode)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_SingleInstantiationMode)
									.IsChecked(this, &SFoliageEdit::GetCheckState_SingleInstantiationMode)
									.ToolTipText(LOCTEXT("SingleInstantiationModeTooltips", "Paint a single foliage instance at the mouse cursor location (i + Mouse Click)"))
									[
										SNew(STextBlock)
										.Text(LOCTEXT("SingleInstantiationMode", "Single Instance Mode"))
										.Font(StandardFont)
									]
								]
							]

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(150)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_SpawnInCurrentLevelMode)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_SpawnInCurrentLevelMode)
									.IsChecked(this, &SFoliageEdit::GetCheckState_SpawnInCurrentLevelMode)
									.ToolTipText(LOCTEXT("SpawnInCurrentLevelModeTooltips", "Wether to place foliage meshes in the current level or in the level containing the mesh being painted on."))
									[
										SNew(STextBlock)
										.Text(LOCTEXT("SpawnInCurrentLevelMode", "Place in Current Level"))
										.Font(StandardFont)
									]
								]
							]
						]
					]

					// Filters
					+ SVerticalBox::Slot()
					.Padding(StandardPadding)
					.AutoHeight()
					[
						SNew(SHeader)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FiltersHeader", "Filters"))
							.Font(StandardFont)
						]
					]

					+ SVerticalBox::Slot()
					.Padding(StandardPadding)
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility(this, &SFoliageEdit::GetVisibility_Filters)

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(StandardPadding)
						[
							SNew(SWrapBox)
							.UseAllottedWidth(true)
							.InnerSlotPadding({6, 5})

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(91)
								.Visibility(this, &SFoliageEdit::GetVisibility_LandscapeFilter)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_Landscape)
									.IsChecked(this, &SFoliageEdit::GetCheckState_Landscape)
									.ToolTipText(this, &SFoliageEdit::GetTooltipText_Landscape)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("Landscape", "Landscape"))
										.Font(StandardFont)
									]
								]
							]

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(91)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_StaticMesh)
									.IsChecked(this, &SFoliageEdit::GetCheckState_StaticMesh)
									.ToolTipText(this, &SFoliageEdit::GetTooltipText_StaticMesh)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("StaticMeshes", "Static Meshes"))
										.Font(StandardFont)
									]
								]
							]

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(91)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_BSP)
									.IsChecked(this, &SFoliageEdit::GetCheckState_BSP)
									.ToolTipText(this, &SFoliageEdit::GetTooltipText_BSP)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("BSP", "BSP"))
										.Font(StandardFont)
									]
								]
							]

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(91)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_Foliage)
									.IsChecked(this, &SFoliageEdit::GetCheckState_Foliage)
									.ToolTipText(this, &SFoliageEdit::GetTooltipText_Foliage)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("Foliage", "Foliage"))
										.Font(StandardFont)
									]
								]
							]

							+ SWrapBox::Slot()
							[
								SNew(SBox)
								.MinDesiredWidth(91)
								[
									SNew(SCheckBox)
									.Visibility(this, &SFoliageEdit::GetVisibility_Filters)
									.OnCheckStateChanged(this, &SFoliageEdit::OnCheckStateChanged_Translucent)
									.IsChecked(this, &SFoliageEdit::GetCheckState_Translucent)
									.ToolTipText(this, &SFoliageEdit::GetTooltipText_Translucent)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("Translucent", "Translucent"))
										.Font(StandardFont)
									]
								]
							]

						]
					]

					// Actions
					+ SVerticalBox::Slot()
					.Padding(StandardPadding)
					.AutoHeight()
					[
						SNew(SHeader)
						.Visibility(this, &SFoliageEdit::GetVisibility_Actions)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ActionsHeader", "Actions"))
							.Font(StandardFont)
						]
					]

					+ SVerticalBox::Slot()
					.Padding(StandardPadding)
					.AutoHeight()
					[
						SNew(SWrapBox)
						.UseAllottedWidth(true)
						.Visibility(this, &SFoliageEdit::GetVisibility_SelectionOptions)

						// Select all instances
						+ SWrapBox::Slot()
						.Padding(FMargin(0.f, 0.f, 6.f, 3.f))
						[
							SNew(SBox)
							.WidthOverride(100.f)
							.HeightOverride(25.f)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked(this, &SFoliageEdit::OnSelectAllInstances)
								.Text(LOCTEXT("SelectAllInstances", "Select All"))
								.ToolTipText(LOCTEXT("SelectAllInstances_Tooltip", "Selects all foliage instances"))
							]
						]

						// Select all invalid instances
						+ SWrapBox::Slot()
						.Padding(FMargin(0.f, 0.f, 6.f, 3.f))
						[
							SNew(SBox)
							.WidthOverride(100.f)
							.HeightOverride(25.f)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked(this, &SFoliageEdit::OnSelectInvalidInstances)
								.Text(LOCTEXT("SelectInvalidInstances", "Select Invalid"))
								.ToolTipText(LOCTEXT("SelectInvalidInstances_Tooltip", "Selects all foliage instances that are not placed in a valid location"))
						
							]
						]

						// Deselect all
						+ SWrapBox::Slot()
						.Padding(FMargin(0.f, 0.f, 6.f, 3.f))
						[
							SNew(SBox)
							.WidthOverride(100.f)
							.HeightOverride(25.f)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked(this, &SFoliageEdit::OnDeselectAllInstances)
								.Text(LOCTEXT("DeselectAllInstances", "Deselect All"))
								.ToolTipText(LOCTEXT("DeselectAllInstances_Tooltip", "Deselects all foliage instances"))
							]
						]
					]
				]
			]
		]

		// Foliage Palette
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.VAlign(VAlign_Fill)
		.Padding(0.f, 5.f, 0.f, 0.f)
		[
			SAssignNew(FoliagePalette, SFoliagePalette)
			.FoliageEdMode(FoliageEditMode)
		]
	];

	RefreshFullList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SFoliageEdit::RefreshFullList()
{
	FoliagePalette->UpdatePalette(true);
}

void SFoliageEdit::NotifyFoliageTypeMeshChanged(UFoliageType* FoliageType)
{
	FoliagePalette->UpdateThumbnailForType(FoliageType);
}

TSharedRef<SWidget> SFoliageEdit::BuildToolBar()
{
	FToolBarBuilder Toolbar(FoliageEditMode->UICommandList, FMultiBoxCustomization::None, nullptr, Orient_Vertical);
	Toolbar.SetLabelVisibility(EVisibility::Collapsed);
	Toolbar.SetStyle(&FEditorStyle::Get(), "FoliageEditToolbar");
	{
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetPaint);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetReapplySettings);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetSelect);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetLassoSelect);
		Toolbar.AddToolBarButton(FFoliageEditCommands::Get().SetPaintBucket);
	}

	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				[
					Toolbar.MakeWidget()
				]
			]
		];
}

bool SFoliageEdit::IsPaintTool() const
{
	return FoliageEditMode->UISettings.GetPaintToolSelected();
}

bool SFoliageEdit::IsReapplySettingsTool() const
{
	return FoliageEditMode->UISettings.GetReapplyToolSelected();
}

bool SFoliageEdit::IsSelectTool() const
{
	return FoliageEditMode->UISettings.GetSelectToolSelected();
}

bool SFoliageEdit::IsLassoSelectTool() const
{
	return FoliageEditMode->UISettings.GetLassoSelectToolSelected();
}

bool SFoliageEdit::IsPaintFillTool() const
{
	return FoliageEditMode->UISettings.GetPaintBucketToolSelected();
}

FText SFoliageEdit::GetActiveToolName() const
{
	FText OutText;
	if (IsPaintTool())
	{
		OutText = LOCTEXT("FoliageToolName_Paint", "Paint");
	}
	else if (IsReapplySettingsTool())
	{
		OutText = LOCTEXT("FoliageToolName_Reapply", "Reapply");
	}
	else if (IsSelectTool())
	{
		OutText = LOCTEXT("FoliageToolName_Select", "Select");
	}
	else if (IsLassoSelectTool())
	{
		OutText = LOCTEXT("FoliageToolName_LassoSelect", "Lasso Select");
	}
	else if (IsPaintFillTool())
	{
		OutText = LOCTEXT("FoliageToolName_Fill", "Fill");
	}

	return OutText;
}

void SFoliageEdit::SetRadius(float InRadius)
{
	FoliageEditMode->UISettings.SetRadius(InRadius);
}

TOptional<float> SFoliageEdit::GetRadius() const
{
	return FoliageEditMode->UISettings.GetRadius();
}

bool SFoliageEdit::IsEnabled_BrushSize() const
{
	return !FoliageEditMode->UISettings.IsInAnySingleInstantiationMode();
}

void SFoliageEdit::SetPaintDensity(float InDensity)
{
	FoliageEditMode->UISettings.SetPaintDensity(InDensity);
}

TOptional<float> SFoliageEdit::GetPaintDensity() const
{
	return FoliageEditMode->UISettings.GetPaintDensity();
}

bool SFoliageEdit::IsEnabled_PaintDensity() const
{
	return !FoliageEditMode->UISettings.IsInAnySingleInstantiationMode();
}

void SFoliageEdit::SetEraseDensity(float InDensity)
{
	FoliageEditMode->UISettings.SetUnpaintDensity(InDensity);
}

TOptional<float> SFoliageEdit::GetEraseDensity() const
{
	return FoliageEditMode->UISettings.GetUnpaintDensity();
}

bool SFoliageEdit::IsEnabled_EraseDensity() const
{
	return !FoliageEditMode->UISettings.IsInAnySingleInstantiationMode();
}

EVisibility SFoliageEdit::GetVisibility_SelectionOptions() const
{
	return IsSelectTool() || IsLassoSelectTool() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

FReply SFoliageEdit::OnSelectAllInstances()
{
	for (FFoliageMeshUIInfoPtr& TypeInfo : FoliageEditMode->GetFoliageMeshList())
	{
		if (TypeInfo->InstanceCountCurrentLevel > 0)
		{
			UFoliageType* FoliageType = TypeInfo->Settings;
			FoliageEditMode->SelectInstances(FoliageType, true);
		}
	}

	return FReply::Handled();
}

FReply SFoliageEdit::OnSelectInvalidInstances()
{
	for (FFoliageMeshUIInfoPtr& TypeInfo : FoliageEditMode->GetFoliageMeshList())
	{
		if (TypeInfo->InstanceCountCurrentLevel > 0)
		{
			const UFoliageType* FoliageType = TypeInfo->Settings;
			FoliageEditMode->SelectInstances(FoliageType, false);
			FoliageEditMode->SelectInvalidInstances(FoliageType);
		}
	}

	return FReply::Handled();
}

FReply SFoliageEdit::OnDeselectAllInstances()
{
	for (FFoliageMeshUIInfoPtr& TypeInfo : FoliageEditMode->GetFoliageMeshList())
	{
		if (TypeInfo->InstanceCountCurrentLevel > 0)
		{
			UFoliageType* FoliageType = TypeInfo->Settings;
			FoliageEditMode->SelectInstances(FoliageType, false);
		}
	}

	return FReply::Handled();
}

FText SFoliageEdit::GetFilterText() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("PlacementFilter", "Placement Filter");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("ReapplyFilter", "Reapply Filter");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("SelectionFilter", "Selection Filter");
	}

	return TooltipText;
}

void SFoliageEdit::OnCheckStateChanged_Landscape(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterLandscape(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_Landscape() const
{
	return FoliageEditMode->UISettings.GetFilterLandscape() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEdit::OnCheckStateChanged_SingleInstantiationMode(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetIsInSingleInstantiationMode(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_SingleInstantiationMode() const
{
	return FoliageEditMode->UISettings.GetIsInSingleInstantiationMode() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SFoliageEdit::OnCheckStateChanged_SpawnInCurrentLevelMode(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetSpawnInCurrentLevelMode(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_SpawnInCurrentLevelMode() const
{
	return FoliageEditMode->UISettings.GetIsInSpawnInCurrentLevelMode() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SFoliageEdit::GetTooltipText_Landscape() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("FilterLandscapeTooltip_Placement", "Place foliage on landscapes");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("FilterLandscapeTooltip_Reapply", "Reapply to instances on landscapes");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("FilterLandscapeTooltip_Select", "Select instances on landscapes");
	}

	return TooltipText;
}

void SFoliageEdit::OnCheckStateChanged_StaticMesh(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterStaticMesh(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_StaticMesh() const
{
	return FoliageEditMode->UISettings.GetFilterStaticMesh() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SFoliageEdit::GetTooltipText_StaticMesh() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("FilterStaticMeshTooltip_Placement", "Place foliage on static meshes");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("FilterStaticMeshTooltip_Reapply", "Reapply to instances on static meshes");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("FilterStaticMeshTooltip_Select", "Select instances on static meshes");
	}

	return TooltipText;
}

void SFoliageEdit::OnCheckStateChanged_BSP(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterBSP(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_BSP() const
{
	return FoliageEditMode->UISettings.GetFilterBSP() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SFoliageEdit::GetTooltipText_BSP() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("FilterBSPTooltip_Placement", "Place foliage on BSP");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("FilterBSPTooltip_Reapply", "Reapply to instances on BSP");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("FilterBSPTooltip_Select", "Select instances on BSP");
	}

	return TooltipText;
}

void SFoliageEdit::OnCheckStateChanged_Foliage(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterFoliage(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_Foliage() const
{
	return FoliageEditMode->UISettings.GetFilterFoliage() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SFoliageEdit::GetTooltipText_Foliage() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("FilterFoliageTooltip_Placement", "Place foliage on other blocking foliage geometry");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("FilterFoliageTooltip_Reapply", "Reapply to instances on blocking foliage geometry");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("FilterFoliageTooltip_Select", "Select instances on blocking foliage geometry");
	}

	return TooltipText;
}

void SFoliageEdit::OnCheckStateChanged_Translucent(ECheckBoxState InState)
{
	FoliageEditMode->UISettings.SetFilterTranslucent(InState == ECheckBoxState::Checked ? true : false);
}

ECheckBoxState SFoliageEdit::GetCheckState_Translucent() const
{
	return FoliageEditMode->UISettings.GetFilterTranslucent() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SFoliageEdit::GetTooltipText_Translucent() const
{
	FText TooltipText;
	if (IsPaintTool() || IsPaintFillTool())
	{
		TooltipText = LOCTEXT("FilterTranslucentTooltip_Placement", "Place foliage on translucent geometry");
	}
	else if (IsReapplySettingsTool())
	{
		TooltipText = LOCTEXT("FilterTranslucentTooltip_Reapply", "Reapply to instances on translucent geometry");
	}
	else if (IsLassoSelectTool())
	{
		TooltipText = LOCTEXT("FilterTranslucentTooltip_Select", "Select instances on translucent geometry");
	}

	return TooltipText;
}

EVisibility SFoliageEdit::GetVisibility_Radius() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetReapplyPaintBucketToolSelected() || FoliageEditMode->UISettings.GetPaintBucketToolSelected() )
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_PaintDensity() const
{
	if (!FoliageEditMode->UISettings.GetPaintToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_EraseDensity() const
{
	if (!FoliageEditMode->UISettings.GetPaintToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_Filters() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_LandscapeFilter() const
{
	// Fill tool doesn't support Landscape
	if (FoliageEditMode->UISettings.GetPaintBucketToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SFoliageEdit::GetVisibility_Actions() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetLassoSelectToolSelected())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility SFoliageEdit::GetVisibility_SingleInstantiationMode() const
{
	if (FoliageEditMode->UISettings.GetPaintToolSelected() || FoliageEditMode->UISettings.GetReapplyToolSelected() || FoliageEditMode->UISettings.GetLassoSelectToolSelected())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility SFoliageEdit::GetVisibility_SpawnInCurrentLevelMode() const
{
	if (FoliageEditMode->UISettings.GetPaintToolSelected())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility SFoliageEdit::GetVisibility_Options() const
{
	if (FoliageEditMode->UISettings.GetSelectToolSelected() || FoliageEditMode->UISettings.GetPaintBucketToolSelected())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
