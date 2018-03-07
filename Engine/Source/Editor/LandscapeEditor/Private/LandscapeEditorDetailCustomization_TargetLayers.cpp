// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_TargetLayers.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Brushes/SlateColorBrush.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorObject.h"

#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "SLandscapeEditor.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "DesktopPlatformModule.h"
#include "AssetRegistryModule.h"

#include "LandscapeRender.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "LandscapeEdit.h"
#include "IDetailGroup.h"
#include "SBoxPanel.h"
#include "Private/SlateEditorStyle.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.TargetLayers"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_TargetLayers::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_TargetLayers);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_TargetLayers::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> PropertyHandle_PaintingRestriction = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, PaintingRestriction));
	TSharedRef<IPropertyHandle> PropertyHandle_TargetDisplayOrder = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, TargetDisplayOrder));
	PropertyHandle_TargetDisplayOrder->MarkHiddenByCustomization();

	TSharedRef<IPropertyHandle> PropertyHandle_TargetShowUnusedLayers = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, ShowUnusedLayers));
	PropertyHandle_TargetShowUnusedLayers->MarkHiddenByCustomization();	

	if (!ShouldShowTargetLayers())
	{
		PropertyHandle_PaintingRestriction->MarkHiddenByCustomization();

		return;
	}

	IDetailCategoryBuilder& TargetsCategory = DetailBuilder.EditCategory("Target Layers");

	TargetsCategory.AddProperty(PropertyHandle_PaintingRestriction)
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_PaintingRestriction)));

	TargetsCategory.AddCustomRow(FText())
	.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_VisibilityTip)))
	[
		SNew(SErrorText)
		.Font(DetailBuilder.GetDetailFontBold())
		.AutoWrapText(true)
		.ErrorText(LOCTEXT("Visibility_Tip","Note: You must add a \"Landscape Visibility Mask\" node to your material before you can paint visibility."))
	];

	TargetsCategory.AddCustomBuilder(MakeShareable(new FLandscapeEditorCustomNodeBuilder_TargetLayers(DetailBuilder.GetThumbnailPool().ToSharedRef(), PropertyHandle_TargetDisplayOrder, PropertyHandle_TargetShowUnusedLayers)));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool FLandscapeEditorDetailCustomization_TargetLayers::ShouldShowTargetLayers()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolMode)
	{
		//bool bSupportsHeightmap = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Heightmap) != 0;
		//bool bSupportsWeightmap = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Weightmap) != 0;
		//bool bSupportsVisibility = (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Visibility) != 0;

		//// Visible if there are possible choices
		//if (bSupportsWeightmap || bSupportsHeightmap || bSupportsVisibility)
		if (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes != 0)
		{
			return true;
		}
	}

	return false;
}

bool FLandscapeEditorDetailCustomization_TargetLayers::ShouldShowPaintingRestriction()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if (LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap ||
			LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Visibility)
		{
			return true;
		}
	}

	return false;
}

EVisibility FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_PaintingRestriction()
{
	return ShouldShowPaintingRestriction() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool FLandscapeEditorDetailCustomization_TargetLayers::ShouldShowVisibilityTip()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		if (LandscapeEdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Visibility)
		{
			ALandscapeProxy* Proxy = LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy();
			UMaterialInterface* HoleMaterial = Proxy->GetLandscapeHoleMaterial();
			if (!HoleMaterial)
			{
				HoleMaterial = Proxy->GetLandscapeMaterial();
			}
			if (!HoleMaterial->GetMaterial()->HasAnyExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionLandscapeVisibilityMask>())
			{
				return true;
			}
		}
	}

	return false;
}

EVisibility FLandscapeEditorDetailCustomization_TargetLayers::GetVisibility_VisibilityTip()
{
	return ShouldShowVisibilityTip() ? EVisibility::Visible : EVisibility::Collapsed;
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorCustomNodeBuilder_TargetLayers::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FLandscapeEditorCustomNodeBuilder_TargetLayers::FLandscapeEditorCustomNodeBuilder_TargetLayers(TSharedRef<FAssetThumbnailPool> InThumbnailPool, TSharedRef<IPropertyHandle> InTargetDisplayOrderPropertyHandle, TSharedRef<IPropertyHandle> InTargetShowUnusedLayersPropertyHandle)
	: ThumbnailPool(InThumbnailPool)
	, TargetDisplayOrderPropertyHandle(InTargetDisplayOrderPropertyHandle)
	, TargetShowUnusedLayersPropertyHandle(InTargetShowUnusedLayersPropertyHandle)
{
}

FLandscapeEditorCustomNodeBuilder_TargetLayers::~FLandscapeEditorCustomNodeBuilder_TargetLayers()
{
	FEdModeLandscape::TargetsListUpdated.RemoveAll(this);
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	FEdModeLandscape::TargetsListUpdated.RemoveAll(this);
	if (InOnRegenerateChildren.IsBound())
	{
		FEdModeLandscape::TargetsListUpdated.Add(InOnRegenerateChildren);
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	
	if (LandscapeEdMode == NULL)
	{
		return;	
	}

	NodeRow.NameWidget
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString(TEXT("Layers")))
		];

	if (LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::Weightmap)
	{
		NodeRow.ValueWidget
		[
			SNew(SHorizontalBox)
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
				.ForegroundColor(FSlateColor::UseForeground())
				.HasDownArrow(true)
				.ContentPadding(FMargin(1, 0))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("TargetLayerSortButtonTooltip", "Define how we want to sort the displayed layers"))
				.OnGetMenuContent(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDisplayOrderButtonMenuContent)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew( SOverlay )
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_DisplayOrder.Default"))
						]	
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDisplayOrderBrush)
						]
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
				.ForegroundColor(FSlateColor::UseForeground())
				.HasDownArrow(true)
				.ContentPadding(FMargin(1, 0))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.ToolTipText(LOCTEXT("TargetLayerUnusedLayerButtonTooltip", "Define if we want to display unused layers"))
				.OnGetMenuContent(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerShowUnusedButtonMenuContent)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(16.0f)
						.HeightOverride(16.0f)
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("GenericViewButton"))
						]
					]
				]
			]
		];
	}
}

TSharedRef<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDisplayOrderButtonMenuContent()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr, nullptr, /*bCloseSelfOnly=*/ true);

	MenuBuilder.BeginSection("TargetLayerSortType", LOCTEXT("SortTypeHeading", "Sort Type"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("TargetLayerDisplayOrderDefault", "Default"),
			LOCTEXT("TargetLayerDisplayOrderDefaultToolTip", "Sort using order defined in the material."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::SetSelectedDisplayOrder, ELandscapeLayerDisplayMode::Default),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::IsSelectedDisplayOrder, ELandscapeLayerDisplayMode::Default)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TargetLayerDisplayOrderAlphabetical", "Alphabetical"),
			LOCTEXT("TargetLayerDisplayOrderAlphabeticalToolTip", "Sort using alphabetical order."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::SetSelectedDisplayOrder, ELandscapeLayerDisplayMode::Alphabetical),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::IsSelectedDisplayOrder, ELandscapeLayerDisplayMode::Alphabetical)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TargetLayerDisplayOrderCustom", "Custom"),
			LOCTEXT("TargetLayerDisplayOrderCustomToolTip", "This sort options will be set when changing manually display order by dragging layers"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::SetSelectedDisplayOrder, ELandscapeLayerDisplayMode::UserSpecific),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::IsSelectedDisplayOrder, ELandscapeLayerDisplayMode::UserSpecific)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerShowUnusedButtonMenuContent()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr, nullptr, /*bCloseSelfOnly=*/ true);

	MenuBuilder.BeginSection("TargetLayerUnusedType", LOCTEXT("UnusedTypeHeading", "Layer Visilibity"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("TargetLayerShowUnusedLayer", "Show all layers"),
			LOCTEXT("TargetLayerShowUnusedLayerToolTip", "Show all layers"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShowUnusedLayers, true),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowUnusedLayers, true)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("TargetLayerHideUnusedLayer", "Hide unused layers"),
			LOCTEXT("TargetLayerHideUnusedLayerToolTip", "Only show used layer"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShowUnusedLayers, false),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowUnusedLayers, false)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}

	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::ShowUnusedLayers(bool Result)
{
	TargetShowUnusedLayersPropertyHandle->SetValue(Result);
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowUnusedLayers(bool Result) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->UISettings->ShowUnusedLayers == Result;
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::SetSelectedDisplayOrder(ELandscapeLayerDisplayMode InDisplayOrder)
{
	TargetDisplayOrderPropertyHandle->SetValue((uint8)InDisplayOrder);	
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::IsSelectedDisplayOrder(ELandscapeLayerDisplayMode InDisplayOrder) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		return LandscapeEdMode->UISettings->TargetDisplayOrder == InDisplayOrder;
	}

	return false;
}

const FSlateBrush* FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDisplayOrderBrush() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		switch (LandscapeEdMode->UISettings->TargetDisplayOrder)
		{
			case ELandscapeLayerDisplayMode::Alphabetical: return FEditorStyle::Get().GetBrush("LandscapeEditor.Target_DisplayOrder.Alphabetical");
			case ELandscapeLayerDisplayMode::UserSpecific: return FEditorStyle::Get().GetBrush("LandscapeEditor.Target_DisplayOrder.Custom");
		}
	}

	return nullptr;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowLayer(TSharedRef<FLandscapeTargetListInfo> Target) const
{
	if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();

		if (LandscapeEdMode != nullptr)
		{
			return LandscapeEdMode->ShouldShowLayer(Target) ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL)
	{
		const TArray<TSharedRef<FLandscapeTargetListInfo>>& TargetList = LandscapeEdMode->GetTargetList();
		const TArray<FName>* TargetDisplayOrderList = LandscapeEdMode->GetTargetDisplayOrderList();
		const TArray<FName>& TargetShownLayerList = LandscapeEdMode->GetTargetShownList();

		if (TargetDisplayOrderList == nullptr)
		{
			return;
		}

		TSharedPtr<SDragAndDropVerticalBox> TargetLayerList = SNew(SDragAndDropVerticalBox)
			.OnCanAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleCanAcceptDrop)
			.OnAcceptDrop(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleAcceptDrop)
			.OnDragDetected(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleDragDetected);

		TargetLayerList->SetDropIndicator_Above(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Above"));
		TargetLayerList->SetDropIndicator_Below(*FEditorStyle::GetBrush("LandscapeEditor.TargetList.DropZone.Below"));

		ChildrenBuilder.AddCustomRow(FText::FromString(FString(TEXT("Layers"))))
			.Visibility(EVisibility::Visible)
			[
				TargetLayerList.ToSharedRef()
			];

		for (int32 i = 0; i < TargetDisplayOrderList->Num(); ++i)
		{
			for (const TSharedRef<FLandscapeTargetListInfo>& TargetInfo : TargetList)
			{
				if (TargetInfo->LayerName == (*TargetDisplayOrderList)[i] && (TargetInfo->TargetType != ELandscapeToolTargetType::Weightmap || TargetShownLayerList.Find(TargetInfo->LayerName) != INDEX_NONE))
				{
					TSharedPtr<SWidget> GeneratedRowWidget = GenerateRow(TargetInfo);

					if (GeneratedRowWidget.IsValid())
					{
						TargetLayerList->AddSlot()
						.AutoHeight()						
						[
							GeneratedRowWidget.ToSharedRef()
						];
					}

					break;
				}
			}
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::GenerateRow(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	TSharedPtr<SWidget> RowWidget;

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		if ((LandscapeEdMode->CurrentTool->GetSupportedTargetTypes() & LandscapeEdMode->CurrentToolMode->SupportedTargetTypes & ELandscapeToolTargetTypeMask::FromType(Target->TargetType)) == 0)
		{
			return RowWidget;
		}
	}
	
	if (Target->TargetType != ELandscapeToolTargetType::Weightmap)
	{
		RowWidget = SNew(SLandscapeEditorSelectableBorder)
			.Padding(0)
			.VAlign(VAlign_Center)
			.OnContextMenuOpening_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening, Target)
			.OnSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged, Target)
			.IsSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected, Target)			
			.Visibility(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowLayer, Target)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2))
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush(Target->TargetType == ELandscapeToolTargetType::Heightmap ? TEXT("LandscapeEditor.Target_Heightmap") : TEXT("LandscapeEditor.Target_Visibility")))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					.Padding(0, 2)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(Target->TargetName)
						.ShadowOffset(FVector2D::UnitVector)
					]
				]
			];
	}
	else
	{
		static const FSlateColorBrush SolidWhiteBrush = FSlateColorBrush(FColorList::White);

		RowWidget = SNew(SLandscapeEditorSelectableBorder)
			.Padding(0)
			.VAlign(VAlign_Center)
			.OnContextMenuOpening_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening, Target)
			.OnSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged, Target)
			.IsSelected_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected, Target)
			.Visibility(this, &FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldShowLayer, Target)
			[				
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
					[
						SNew(SImage)
						.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicator"))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2))
				[
					SNew(SBox)
					.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeLayerUsageVisibility, Target)
					.WidthOverride(48)
					.HeightOverride(48)
					[
						SNew(SImage)
						.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.ColorAndOpacity_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetLayerUsageDebugColor, Target)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2))
				[
					(Target->bValid)
					? (TSharedRef<SWidget>)(
					SNew(SLandscapeAssetThumbnail, Target->ThumbnailMIC.Get(), ThumbnailPool)
					.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeLayerUsageVisibility_Invert, Target)
					.ThumbnailSize(FIntPoint(48, 48))
					)
					: (TSharedRef<SWidget>)(
					SNew(SImage)
					.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeLayerUsageVisibility_Invert, Target)
					.Image(FEditorStyle::GetBrush(TEXT("LandscapeEditor.Target_Invalid")))
					)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4, 0)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					.Padding(0, 2, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(Target->TargetName)
							.ShadowOffset(FVector2D::UnitVector)
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Visibility((Target->LayerInfoObj.IsValid() && Target->LayerInfoObj->bNoWeightBlend) ? EVisibility::Visible : EVisibility::Collapsed)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(LOCTEXT("NoWeightBlend", "No Weight-Blend"))
							.ShadowOffset(FVector2D::UnitVector)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerInfoSelectorVisibility, Target)
						+ SHorizontalBox::Slot()
						.FillWidth(1)
						.VAlign(VAlign_Center)
						[
							SNew(SObjectPropertyEntryBox)
							.IsEnabled((bool)Target->bValid)
							.ObjectPath(Target->LayerInfoObj != NULL ? Target->LayerInfoObj->GetPathName() : FString())
							.AllowedClass(ULandscapeLayerInfoObject::StaticClass())
							.OnObjectChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerSetObject, Target)
							.OnShouldFilterAsset_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldFilterLayerInfo, Target->LayerName)
							.AllowClear(false)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SComboButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.HasDownArrow(false)
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_Create", "Create Layer Info"))
							.IsEnabled_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerCreateEnabled, Target)
							.OnGetMenuContent_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnGetTargetLayerCreateMenu, Target)
							.ButtonContent()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Create"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_MakePublic", "Make Layer Public (move layer info into asset file)"))
							.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerMakePublicVisibility, Target)
							.OnClicked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerMakePublicClicked, Target)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_MakePublic"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
							.ContentPadding(4.0f)
							.ForegroundColor(FSlateColor::UseForeground())
							.IsFocusable(false)
							.ToolTipText(LOCTEXT("Tooltip_Delete", "Delete Layer"))
							.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDeleteVisibility, Target)
							.OnClicked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerDeleteClicked, Target)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("LandscapeEditor.Target_Delete"))
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						.Visibility_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeColorChannelVisibility, Target)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0, 2, 2, 2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 0)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 0)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_None", "None"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 1)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 1)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_R", "R"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 2)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 2)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_G", "G"))
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2)
						[
							SNew(SCheckBox)
							.IsChecked_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked, Target, 4)
							.OnCheckStateChanged_Static(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged, Target, 4)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ViewMode.Debug_B", "B"))
							]
						]
					]
				]
			];
	}

	return RowWidget;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();

	if (LandscapeEdMode != nullptr)
	{
		const TArray<FName>& TargetShownList = LandscapeEdMode->GetTargetShownList();

		if (TargetShownList.IsValidIndex(SlotIndex))
		{
			const TArray<FName>* TargetDisplayOrderList = LandscapeEdMode->GetTargetDisplayOrderList();

			if (TargetDisplayOrderList != nullptr)
			{
				FName ShownTargetName = LandscapeEdMode->UISettings->ShowUnusedLayers && TargetShownList.IsValidIndex(SlotIndex + LandscapeEdMode->GetTargetLayerStartingIndex()) ? TargetShownList[SlotIndex + LandscapeEdMode->GetTargetLayerStartingIndex()] : TargetShownList[SlotIndex];
				int32 DisplayOrderLayerIndex = TargetDisplayOrderList->Find(ShownTargetName);

				if (TargetDisplayOrderList->IsValidIndex(DisplayOrderLayerIndex))
				{
					const TArray<TSharedRef<FLandscapeTargetListInfo>>& TargetList = LandscapeEdMode->GetTargetList();

					for (const TSharedRef<FLandscapeTargetListInfo>& TargetInfo : TargetList)
					{
						if (TargetInfo->LayerName == (*TargetDisplayOrderList)[DisplayOrderLayerIndex])
						{
							TSharedPtr<SWidget> Row = GenerateRow(TargetInfo);

							if (Row.IsValid())
							{
								return FReply::Handled().BeginDragDrop(FTargetLayerDragDropOp::New(SlotIndex, Slot, Row));
							}
						}
					}
				}
			}
		}
	}

	return FReply::Unhandled();
}

TOptional<SDragAndDropVerticalBox::EItemDropZone> FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleCanAcceptDrop(const FDragDropEvent& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FTargetLayerDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FTargetLayerDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		return DropZone;
	}

	return TOptional<SDragAndDropVerticalBox::EItemDropZone>();
}

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::HandleAcceptDrop(FDragDropEvent const& DragDropEvent, SDragAndDropVerticalBox::EItemDropZone DropZone, int32 SlotIndex, SVerticalBox::FSlot* Slot)
{
	TSharedPtr<FTargetLayerDragDropOp> DragDropOperation = DragDropEvent.GetOperationAs<FTargetLayerDragDropOp>();

	if (DragDropOperation.IsValid())
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();

		if (LandscapeEdMode != nullptr)
		{
			const TArray<FName>& TargetShownList = LandscapeEdMode->GetTargetShownList();

			if (TargetShownList.IsValidIndex(DragDropOperation->SlotIndexBeingDragged) && TargetShownList.IsValidIndex(SlotIndex))
			{
				const TArray<FName>* TargetDisplayOrderList = LandscapeEdMode->GetTargetDisplayOrderList();

				if (TargetDisplayOrderList != nullptr && TargetShownList.IsValidIndex(DragDropOperation->SlotIndexBeingDragged + LandscapeEdMode->GetTargetLayerStartingIndex()) && TargetShownList.IsValidIndex(SlotIndex + LandscapeEdMode->GetTargetLayerStartingIndex()))
				{
					int32 StartingLayerIndex = TargetDisplayOrderList->Find(LandscapeEdMode->UISettings->ShowUnusedLayers ? TargetShownList[DragDropOperation->SlotIndexBeingDragged + LandscapeEdMode->GetTargetLayerStartingIndex()] : TargetShownList[DragDropOperation->SlotIndexBeingDragged]);
					int32 DestinationLayerIndex = TargetDisplayOrderList->Find(LandscapeEdMode->UISettings->ShowUnusedLayers ? TargetShownList[SlotIndex + LandscapeEdMode->GetTargetLayerStartingIndex()] : TargetShownList[SlotIndex]);

					if (StartingLayerIndex != INDEX_NONE && DestinationLayerIndex != INDEX_NONE)
					{
						LandscapeEdMode->MoveTargetLayerDisplayOrder(StartingLayerIndex, DestinationLayerIndex);

						return FReply::Handled();
					}
				}
			}
		}
	}

	return FReply::Unhandled();
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerIsSelected(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return
			LandscapeEdMode->CurrentToolTarget.TargetType == Target->TargetType &&
			LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName &&
			LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj; // may be null
	}

	return false;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetSelectionChanged(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->CurrentToolTarget.TargetType = Target->TargetType;
		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			checkSlow(Target->LayerInfoObj == NULL);
			LandscapeEdMode->CurrentToolTarget.LayerInfo = NULL;
			LandscapeEdMode->CurrentToolTarget.LayerName = NAME_None;
		}
		else
		{
			LandscapeEdMode->CurrentToolTarget.LayerInfo = Target->LayerInfoObj;
			LandscapeEdMode->CurrentToolTarget.LayerName = Target->LayerName;
		}
	}
}

TSharedPtr<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerContextMenuOpening(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->TargetType == ELandscapeToolTargetType::Heightmap || Target->LayerInfoObj != NULL)
	{
		FMenuBuilder MenuBuilder(true, NULL);

		MenuBuilder.BeginSection("LandscapeEditorLayerActions", LOCTEXT("LayerContextMenu.Heading", "Layer Actions"));
		{
			// Export
			FUIAction ExportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnExportLayer, Target));
			MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Export", "Export to file"), FText(), FSlateIcon(), ExportAction);

			// Import
			FUIAction ImportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnImportLayer, Target));
			MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Import", "Import from file"), FText(), FSlateIcon(), ImportAction);

			// Reimport
			const FString& ReimportPath = Target->ReimportFilePath();

			if (!ReimportPath.IsEmpty())
			{
				FUIAction ReImportAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnReimportLayer, Target));
				MenuBuilder.AddMenuEntry(FText::Format(LOCTEXT("LayerContextMenu.ReImport", "Reimport from {0}"), FText::FromString(ReimportPath)), FText(), FSlateIcon(), ReImportAction);
			}

			if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
			{
				MenuBuilder.AddMenuSeparator();

				// Fill
				FUIAction FillAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnFillLayer, Target));
				MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Fill", "Fill Layer"), LOCTEXT("LayerContextMenu.Fill_Tooltip", "Fills this layer to 100% across the entire landscape. If this is a weight-blended (normal) layer, all other weight-blended layers will be cleared."), FSlateIcon(), FillAction);

				// Clear
				FUIAction ClearAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnClearLayer, Target));
				MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.Clear", "Clear Layer"), LOCTEXT("LayerContextMenu.Clear_Tooltip", "Clears this layer to 0% across the entire landscape. If this is a weight-blended (normal) layer, other weight-blended layers will be adjusted to compensate."), FSlateIcon(), ClearAction);
			}
			else if (Target->TargetType == ELandscapeToolTargetType::Visibility)
			{
				MenuBuilder.AddMenuSeparator();

				// Clear
				FUIAction ClearAction = FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnClearLayer, Target));
				MenuBuilder.AddMenuEntry(LOCTEXT("LayerContextMenu.ClearHoles", "Remove all Holes"), FText(), FSlateIcon(), ClearAction);
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return NULL;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnExportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
		ULandscapeLayerInfoObject* LayerInfoObj = Target->LayerInfoObj.Get(); // NULL for heightmaps

		// Prompt for filename
		FString SaveDialogTitle;
		FString DefaultFileName;
		const TCHAR* FileTypes = nullptr;

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			SaveDialogTitle = *LOCTEXT("ExportHeightmap", "Export Landscape Heightmap").ToString();
			DefaultFileName = TEXT("Heightmap.png");
			FileTypes = LandscapeEditorModule.GetHeightmapExportDialogTypeString();
		}
		else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
		{
			SaveDialogTitle = *FText::Format(LOCTEXT("ExportLayer", "Export Landscape Layer: {0}"), FText::FromName(LayerInfoObj->LayerName)).ToString();
			DefaultFileName = *FString::Printf(TEXT("%s.png"), *(LayerInfoObj->LayerName.ToString()));
			FileTypes = LandscapeEditorModule.GetWeightmapExportDialogTypeString();
		}

		// Prompt the user for the filenames
		TArray<FString> SaveFilenames;
		bool bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			*SaveDialogTitle,
			*LandscapeEdMode->UISettings->LastImportPath,
			*DefaultFileName,
			FileTypes,
			EFileDialogFlags::None,
			SaveFilenames
			);

		if (bOpened)
		{
			const FString& SaveFilename(SaveFilenames[0]);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(SaveFilename);

			// Actually do the export
			if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
			{
				LandscapeInfo->ExportHeightmap(SaveFilename);
			}
			else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
			{
				LandscapeInfo->ExportLayer(LayerInfoObj, SaveFilename);
			}

			Target->ReimportFilePath() = SaveFilename;
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnImportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
		ULandscapeLayerInfoObject* LayerInfoObj = Target->LayerInfoObj.Get(); // NULL for heightmaps

		// Prompt for filename
		FString OpenDialogTitle;
		FString DefaultFileName;
		const TCHAR* FileTypes = nullptr;

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");

		if (Target->TargetType == ELandscapeToolTargetType::Heightmap)
		{
			OpenDialogTitle = *LOCTEXT("ImportHeightmap", "Import Landscape Heightmap").ToString();
			DefaultFileName = TEXT("Heightmap.png");
			FileTypes = LandscapeEditorModule.GetHeightmapImportDialogTypeString();
		}
		else //if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
		{
			OpenDialogTitle = *FText::Format(LOCTEXT("ImportLayer", "Import Landscape Layer: {0}"), FText::FromName(LayerInfoObj->LayerName)).ToString();
			DefaultFileName = *FString::Printf(TEXT("%s.png"), *(LayerInfoObj->LayerName.ToString()));
			FileTypes = LandscapeEditorModule.GetWeightmapImportDialogTypeString();
		}

		// Prompt the user for the filenames
		TArray<FString> OpenFilenames;
		bool bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			*OpenDialogTitle,
			*LandscapeEdMode->UISettings->LastImportPath,
			*DefaultFileName,
			FileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);

		if (bOpened)
		{
			const FString& OpenFilename(OpenFilenames[0]);
			LandscapeEdMode->UISettings->LastImportPath = FPaths::GetPath(OpenFilename);

			// Actually do the Import
			LandscapeEdMode->ImportData(*Target, OpenFilename);

			Target->ReimportFilePath() = OpenFilename;
		}
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnReimportLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->ReimportData(*Target);
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnFillLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FScopedTransaction Transaction(LOCTEXT("Undo_FillLayer", "Filling Landscape Layer"));
	if (Target->LandscapeInfo.IsValid() && Target->LayerInfoObj.IsValid())
	{
		FLandscapeEditDataInterface LandscapeEdit(Target->LandscapeInfo.Get());
		LandscapeEdit.FillLayer(Target->LayerInfoObj.Get());
	}
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::FillEmptyLayers(ULandscapeInfo* LandscapeInfo, ULandscapeLayerInfoObject* LandscapeInfoObject)
{
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.FillEmptyLayers(LandscapeInfoObject);
}


void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnClearLayer(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FScopedTransaction Transaction(LOCTEXT("Undo_ClearLayer", "Clearing Landscape Layer"));
	if (Target->LandscapeInfo.IsValid() && Target->LayerInfoObj.IsValid())
	{
		FLandscapeEditDataInterface LandscapeEdit(Target->LandscapeInfo.Get());
		LandscapeEdit.DeleteLayer(Target->LayerInfoObj.Get());
	}
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::ShouldFilterLayerInfo(const FAssetData& AssetData, FName LayerName)
{
	const FName LayerNameMetaData = AssetData.GetTagValueRef<FName>("LayerName");
	if (!LayerNameMetaData.IsNone())
	{
		return LayerNameMetaData != LayerName;
	}

	ULandscapeLayerInfoObject* LayerInfo = CastChecked<ULandscapeLayerInfoObject>(AssetData.GetAsset());
	return LayerInfo->LayerName != LayerName;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerSetObject(const FAssetData& AssetData, const TSharedRef<FLandscapeTargetListInfo> Target)
{
	// Can't assign null to a layer
	UObject* Object = AssetData.GetAsset();
	if (Object == NULL)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("Undo_UseExisting", "Assigning Layer to Landscape"));

	ULandscapeLayerInfoObject* SelectedLayerInfo = const_cast<ULandscapeLayerInfoObject*>(CastChecked<ULandscapeLayerInfoObject>(Object));

	if (SelectedLayerInfo != Target->LayerInfoObj.Get())
	{
		if (ensure(SelectedLayerInfo->LayerName == Target->GetLayerName()))
		{
			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			LandscapeInfo->Modify();
			if (Target->LayerInfoObj.IsValid())
			{
				int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerInfoObj.Get(), Target->Owner.Get());
				if (ensure(Index != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];

					LandscapeInfo->ReplaceLayer(LayerSettings.LayerInfoObj, SelectedLayerInfo);

					LayerSettings.LayerInfoObj = SelectedLayerInfo;
				}
			}
			else
			{
				int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerName, Target->Owner.Get());
				if (ensure(Index != INDEX_NONE))
				{
					FLandscapeInfoLayerSettings& LayerSettings = LandscapeInfo->Layers[Index];
					LayerSettings.LayerInfoObj = SelectedLayerInfo;

					Target->LandscapeInfo->CreateLayerEditorSettingsFor(SelectedLayerInfo);
				}
			}

			FEdModeLandscape* LandscapeEdMode = GetEditorMode();
			if (LandscapeEdMode)
			{
				if (LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName
					&& LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj)
				{
					LandscapeEdMode->CurrentToolTarget.LayerInfo = SelectedLayerInfo;
				}
				LandscapeEdMode->UpdateTargetList();
			}

			FillEmptyLayers(LandscapeInfo, SelectedLayerInfo);
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_LayerNameMismatch", "Can't use this layer info because the layer name does not match"));
		}
	}
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerInfoSelectorVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->TargetType == ELandscapeToolTargetType::Weightmap)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

bool FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerCreateEnabled(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (!Target->LayerInfoObj.IsValid())
	{
		return true;
	}

	return false;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerMakePublicVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (Target->bValid && Target->LayerInfoObj.IsValid() && Target->LayerInfoObj->GetOutermost()->ContainsMap())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetTargetLayerDeleteVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (!Target->bValid)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

TSharedRef<SWidget> FLandscapeEditorCustomNodeBuilder_TargetLayers::OnGetTargetLayerCreateMenu(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(LOCTEXT("Menu_Create_Blended", "Weight-Blended Layer (normal)"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked, Target, false)));

	MenuBuilder.AddMenuEntry(LOCTEXT("Menu_Create_NoWeightBlend", "Non Weight-Blended Layer"), FText(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked, Target, true)));

	return MenuBuilder.MakeWidget();
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerCreateClicked(const TSharedRef<FLandscapeTargetListInfo> Target, bool bNoWeightBlend)
{
	check(!Target->LayerInfoObj.IsValid());

	FScopedTransaction Transaction(LOCTEXT("Undo_Create", "Creating New Landscape Layer"));

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		FName LayerName = Target->GetLayerName();
		ULevel* Level = Target->Owner->GetLevel();

		// Build default layer object name and package name
		FName LayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *LayerName.ToString()));
		FString Path = Level->GetOutermost()->GetName() + TEXT("_sharedassets/");
		if (Path.StartsWith("/Temp/"))
		{
			Path = FString("/Game/") + Path.RightChop(FString("/Temp/").Len());
		}
		FString PackageName = Path + LayerObjectName.ToString();

		TSharedRef<SDlgPickAssetPath> NewLayerDlg =
			SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("CreateNewLayerInfo", "Create New Landscape Layer Info Object"))
			.DefaultAssetPath(FText::FromString(PackageName));

		if (NewLayerDlg->ShowModal() != EAppReturnType::Cancel)
		{
			PackageName = NewLayerDlg->GetFullAssetPath().ToString();
			LayerObjectName = FName(*NewLayerDlg->GetAssetName().ToString());

			UPackage* Package = CreatePackage(NULL, *PackageName);
			ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, LayerObjectName, RF_Public | RF_Standalone | RF_Transactional);
			LayerInfo->LayerName = LayerName;
			LayerInfo->bNoWeightBlend = bNoWeightBlend;

			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			LandscapeInfo->Modify();
			int32 Index = LandscapeInfo->GetLayerInfoIndex(LayerName, Target->Owner.Get());
			if (Index == INDEX_NONE)
			{
				LandscapeInfo->Layers.Add(FLandscapeInfoLayerSettings(LayerInfo, Target->Owner.Get()));
			}
			else
			{
				LandscapeInfo->Layers[Index].LayerInfoObj = LayerInfo;
			}

			if (LandscapeEdMode->CurrentToolTarget.LayerName == Target->LayerName
				&& LandscapeEdMode->CurrentToolTarget.LayerInfo == Target->LayerInfoObj)
			{
				LandscapeEdMode->CurrentToolTarget.LayerInfo = LayerInfo;
			}

			Target->LayerInfoObj = LayerInfo;
			Target->LandscapeInfo->CreateLayerEditorSettingsFor(LayerInfo);

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(LayerInfo);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			// Show in the content browser
			TArray<UObject*> Objects;
			Objects.Add(LayerInfo);
			GEditor->SyncBrowserToObjects(Objects);
			
			LandscapeEdMode->TargetsListUpdated.Broadcast();

			FillEmptyLayers(LandscapeInfo, LayerInfo);
		}
	}
}

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerMakePublicClicked(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	FScopedTransaction Transaction(LOCTEXT("Undo_MakePublic", "Make Layer Public"));
	TArray<UObject*> Objects;
	Objects.Add(Target->LayerInfoObj.Get());

	FString Path = Target->Owner->GetOutermost()->GetName() + TEXT("_sharedassets");
	bool bSucceed = ObjectTools::RenameObjects(Objects, false, TEXT(""), Path);
	if (bSucceed)
	{
		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode)
		{
			LandscapeEdMode->UpdateTargetList();
		}
	}
	else
	{
		Transaction.Cancel();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorCustomNodeBuilder_TargetLayers::OnTargetLayerDeleteClicked(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	check(Target->LandscapeInfo.IsValid());

	if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Prompt_DeleteLayer", "Are you sure you want to delete this layer?")) == EAppReturnType::Yes)
	{
		FScopedTransaction Transaction(LOCTEXT("Undo_Delete", "Delete Layer"));

		Target->LandscapeInfo->DeleteLayer(Target->LayerInfoObj.Get(), Target->LayerName);

		FEdModeLandscape* LandscapeEdMode = GetEditorMode();
		if (LandscapeEdMode)
		{
			LandscapeEdMode->UpdateTargetList();
			LandscapeEdMode->UpdateShownLayerList();
		}
	}

	return FReply::Handled();
}

FSlateColor FLandscapeEditorCustomNodeBuilder_TargetLayers::GetLayerUsageDebugColor(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (GLandscapeViewMode == ELandscapeViewMode::LayerUsage && Target->TargetType != ELandscapeToolTargetType::Heightmap && ensure(Target->LayerInfoObj.IsValid()))
	{
		return FSlateColor(Target->LayerInfoObj->LayerUsageDebugColor);
	}
	return FSlateColor(FLinearColor(0, 0, 0, 0));
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeLayerUsageVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (GLandscapeViewMode == ELandscapeViewMode::LayerUsage && Target->TargetType != ELandscapeToolTargetType::Heightmap && Target->LayerInfoObj.IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeLayerUsageVisibility_Invert(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (GLandscapeViewMode == ELandscapeViewMode::LayerUsage && Target->TargetType != ELandscapeToolTargetType::Heightmap && Target->LayerInfoObj.IsValid())
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility FLandscapeEditorCustomNodeBuilder_TargetLayers::GetDebugModeColorChannelVisibility(const TSharedRef<FLandscapeTargetListInfo> Target)
{
	if (GLandscapeViewMode == ELandscapeViewMode::DebugLayer && Target->TargetType != ELandscapeToolTargetType::Heightmap && Target->LayerInfoObj.IsValid())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

ECheckBoxState FLandscapeEditorCustomNodeBuilder_TargetLayers::DebugModeColorChannelIsChecked(const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel)
{
	if (Target->DebugColorChannel == Channel)
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}

void FLandscapeEditorCustomNodeBuilder_TargetLayers::OnDebugModeColorChannelChanged(ECheckBoxState NewCheckedState, const TSharedRef<FLandscapeTargetListInfo> Target, int32 Channel)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		// Enable on us and disable colour channel on other targets
		if (ensure(Target->LayerInfoObj != NULL))
		{
			ULandscapeInfo* LandscapeInfo = Target->LandscapeInfo.Get();
			int32 Index = LandscapeInfo->GetLayerInfoIndex(Target->LayerInfoObj.Get(), Target->Owner.Get());
			if (ensure(Index != INDEX_NONE))
			{
				for (auto It = LandscapeInfo->Layers.CreateIterator(); It; It++)
				{
					FLandscapeInfoLayerSettings& LayerSettings = *It;
					if (It.GetIndex() == Index)
					{
						LayerSettings.DebugColorChannel = Channel;
					}
					else
					{
						LayerSettings.DebugColorChannel &= ~Channel;
					}
				}
				LandscapeInfo->UpdateDebugColorMaterial();

				FEdModeLandscape* LandscapeEdMode = GetEditorMode();
				if (LandscapeEdMode)
				{
					LandscapeEdMode->UpdateTargetList();
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SLandscapeEditorSelectableBorder::Construct(const FArguments& InArgs)
{
	SBorder::Construct(
		SBorder::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
		.BorderImage(this, &SLandscapeEditorSelectableBorder::GetBorder)
		.Content()
		[
			InArgs._Content.Widget
		]
	);

	OnContextMenuOpening = InArgs._OnContextMenuOpening;
	OnSelected = InArgs._OnSelected;
	IsSelected = InArgs._IsSelected;
}

FReply SLandscapeEditorSelectableBorder::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton &&
			OnSelected.IsBound())
		{
			OnSelected.Execute();

			return FReply::Handled().ReleaseMouseCapture();
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton &&
			OnContextMenuOpening.IsBound())
		{
			TSharedPtr<SWidget> Content = OnContextMenuOpening.Execute();
			if (Content.IsValid())
			{
				FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

				FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, Content.ToSharedRef(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			}

			return FReply::Handled().ReleaseMouseCapture();
		}
	}

	return FReply::Unhandled();
}

const FSlateBrush* SLandscapeEditorSelectableBorder::GetBorder() const
{
	const bool bIsSelected = IsSelected.Get();
	const bool bHovered = IsHovered() && OnSelected.IsBound();

	if (bIsSelected)
	{
		return bHovered
			? FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowSelectedHovered")
			: FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowSelected");
	}
	else
	{
		return bHovered
			? FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowBackgroundHovered")
			: FEditorStyle::GetBrush("LandscapeEditor.TargetList", ".RowBackground");
	}
}

TSharedRef<FTargetLayerDragDropOp> FTargetLayerDragDropOp::New(int32 InSlotIndexBeingDragged, SVerticalBox::FSlot* InSlotBeingDragged, TSharedPtr<SWidget> WidgetToShow)
{
	TSharedRef<FTargetLayerDragDropOp> Operation = MakeShareable(new FTargetLayerDragDropOp);

	Operation->MouseCursor = EMouseCursor::GrabHandClosed;
	Operation->SlotIndexBeingDragged = InSlotIndexBeingDragged;
	Operation->SlotBeingDragged = InSlotBeingDragged;
	Operation->WidgetToShow = WidgetToShow;

	Operation->Construct();

	return Operation;
}

FTargetLayerDragDropOp::~FTargetLayerDragDropOp()
{
}

TSharedPtr<SWidget> FTargetLayerDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
			.Content()
			[
				WidgetToShow.ToSharedRef()
			];
		
}

#undef LOCTEXT_NAMESPACE
