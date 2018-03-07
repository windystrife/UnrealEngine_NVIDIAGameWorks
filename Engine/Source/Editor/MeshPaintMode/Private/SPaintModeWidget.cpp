// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SPaintModeWidget.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "SScrollBox.h"
#include "SBoxPanel.h"
#include "SWrapBox.h"
#include "SBox.h"
#include "SButton.h"
#include "SImage.h"
#include "MultiBoxBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PaintModeSettingsCustomization.h"
#include "PaintModePainter.h"
#include "PaintModeSettings.h"

#include "Modules/ModuleManager.h"
#include "PaintModeCommands.h"

#define LOCTEXT_NAMESPACE "PaintModePainter"

void SPaintModeWidget::Construct(const FArguments& InArgs, FPaintModePainter* InPainter)
{
	MeshPainter = InPainter;
	PaintModeSettings = Cast<UPaintModeSettings>(MeshPainter->GetPainterSettings());
	SettingsObjects.Add(MeshPainter->GetBrushSettings());
	SettingsObjects.Add(PaintModeSettings);
	CreateDetailsView();
	
	FMargin StandardPadding(0.0f, 4.0f, 0.0f, 4.0f);
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			/** Toolbar containing buttons to switch between different paint modes */
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.HAlign(HAlign_Center)
				[
					CreateToolBarWidget()->AsShared()
				]
			]
				
			/** (Instance) Vertex paint action buttons widget */
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreateVertexPaintWidget()->AsShared()
			]
				
			/** Texture paint action buttons widget */
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CreateTexturePaintWidget()->AsShared()
			]

			/** DetailsView containing brush and paint settings */
			+ SVerticalBox::Slot()
			.AutoHeight()				
			[
				SettingsDetailsView->AsShared()
			]
		]
	];
}

void SPaintModeWidget::CreateDetailsView()
{
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs(
		/*bUpdateFromSelection=*/ false,
		/*bLockable=*/ false,
		/*bAllowSearch=*/ false,
		FDetailsViewArgs::HideNameArea,
		/*bHideSelectionTip=*/ true,
		/*InNotifyHook=*/ nullptr,
		/*InSearchInitialKeyFocus=*/ false,
		/*InViewIdentifier=*/ NAME_None);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = true;

	SettingsDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->SetRootObjectCustomizationInstance(MakeShareable(new FPaintModeSettingsRootObjectCustomization));
	SettingsDetailsView->SetObjects(SettingsObjects);
}

TSharedPtr<SWidget> SPaintModeWidget::CreateVertexPaintWidget()
{
	FMargin StandardPadding(0.0f, 4.0f, 0.0f, 4.0f);

	TSharedPtr<SWidget> VertexColorWidget;
	TSharedPtr<SHorizontalBox> VertexColorActionBox;
	TSharedPtr<SHorizontalBox> InstanceColorActionBox;
		
	SAssignNew(VertexColorWidget, SVerticalBox)
	.Visibility(this, &SPaintModeWidget::IsVertexPaintModeVisible)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(StandardPadding)
	.HAlign(HAlign_Center)
	[	
		SAssignNew(VertexColorActionBox, SHorizontalBox)
	]
	
	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding(StandardPadding)	
	.HAlign(HAlign_Center)
	[
		SAssignNew(InstanceColorActionBox, SHorizontalBox)
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	.Padding(StandardPadding)
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("SettingsEditor.CheckoutWarningBorder"))
		.BorderBackgroundColor(FColor(166,137,0))				
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() -> EVisibility 
			{
				const bool bVisible = MeshPainter && MeshPainter->GetSelectedComponents<USkeletalMeshComponent>().Num();
				return bVisible ? EVisibility::Visible : EVisibility::Collapsed;
			})
		
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("ClassIcon.SkeletalMeshComponent"))
			]
		
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(.8f)
			.Padding(StandardPadding)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(LOCTEXT("SkelMeshAssetPaintInfo", "Paint is directly propagated to Skeletal Mesh Asset(s)"))
			]
		]
	];
	
	FToolBarBuilder ColorToolbarBuilder(MeshPainter->GetUICommandList(), FMultiBoxCustomization::None);
	ColorToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ColorToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Fill, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Fill"));
	ColorToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Propagate, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Propagate"));
	ColorToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Import, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Import"));
	ColorToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Save, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Save"));

	VertexColorActionBox->AddSlot()
	.FillWidth(1.0f)
	[
		ColorToolbarBuilder.MakeWidget()
	];

	FToolBarBuilder InstanceToolbarBuilder(MeshPainter->GetUICommandList(), FMultiBoxCustomization::None);
	InstanceToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	InstanceToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Copy, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Copy"));
	InstanceToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Paste, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Paste"));
	InstanceToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Remove, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Remove"));
	InstanceToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().Fix, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Fix"));

	InstanceColorActionBox->AddSlot()
	.FillWidth(1.0f)
	[
		InstanceToolbarBuilder.MakeWidget()
	];

	return VertexColorWidget->AsShared();
}
 
TSharedPtr<SWidget> SPaintModeWidget::CreateTexturePaintWidget()
{
	FMargin StandardPadding(0.0f, 4.0f, 0.0f, 4.0f);
	TSharedPtr<SWidget> TexturePaintWidget;
	TSharedPtr<SHorizontalBox> ActionBox;

	SAssignNew(TexturePaintWidget, SVerticalBox)
	.Visibility(this, &SPaintModeWidget::IsTexturePaintModeVisible)
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(StandardPadding)
	.HAlign(HAlign_Center)
	[
		SAssignNew(ActionBox, SHorizontalBox)
	];
	 
	FToolBarBuilder TexturePaintToolbarBuilder(MeshPainter->GetUICommandList(), FMultiBoxCustomization::None);
	TexturePaintToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	TexturePaintToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().PropagateTexturePaint, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Propagate"));
	TexturePaintToolbarBuilder.AddToolBarButton(FPaintModeCommands::Get().SaveTexturePaint, NAME_None, FText::GetEmpty(), TAttribute<FText>(), FSlateIcon(FEditorStyle::GetStyleSetName(), "MeshPaint.Save"));

	ActionBox->AddSlot()
	.FillWidth(1.0f)
	[
		TexturePaintToolbarBuilder.MakeWidget()
	];

	return TexturePaintWidget->AsShared();
}

TSharedPtr<SWidget> SPaintModeWidget::CreateToolBarWidget()
{
	FToolBarBuilder ModeSwitchButtons(MakeShareable(new FUICommandList()), FMultiBoxCustomization::None);
	{
		FSlateIcon ColorPaintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode.ColorPaint");
		ModeSwitchButtons.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([=]()
		{
			PaintModeSettings->PaintMode = EPaintMode::Vertices;
			PaintModeSettings->VertexPaintSettings.MeshPaintMode = EMeshPaintMode::PaintColors;
			SettingsDetailsView->SetObjects(SettingsObjects, true);
		}), FCanExecuteAction(), FIsActionChecked::CreateLambda([=]() -> bool { return PaintModeSettings->PaintMode == EPaintMode::Vertices && PaintModeSettings->VertexPaintSettings.MeshPaintMode == EMeshPaintMode::PaintColors; })), NAME_None, LOCTEXT("Mode.VertexColorPainting", "Colors"), LOCTEXT("Mode.VertexColor.Tooltip", "Vertex Color Painting mode allows painting of Vertex Colors"), ColorPaintIcon, EUserInterfaceActionType::ToggleButton);

		FSlateIcon WeightPaintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode.WeightPaint");
		ModeSwitchButtons.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([=]()
		{
			PaintModeSettings->PaintMode = EPaintMode::Vertices;
			PaintModeSettings->VertexPaintSettings.MeshPaintMode = EMeshPaintMode::PaintWeights;
			SettingsDetailsView->SetObjects(SettingsObjects, true);
		}), FCanExecuteAction(), FIsActionChecked::CreateLambda([=]() -> bool { return PaintModeSettings->PaintMode == EPaintMode::Vertices && PaintModeSettings->VertexPaintSettings.MeshPaintMode == EMeshPaintMode::PaintWeights; })), NAME_None, LOCTEXT("Mode.VertexWeightPainting", " Weights"), LOCTEXT("Mode.VertexWeight.Tooltip", "Vertex Weight Painting mode allows painting of Vertex Weights"), WeightPaintIcon, EUserInterfaceActionType::ToggleButton);

		FSlateIcon TexturePaintIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.MeshPaintMode.TexturePaint");
		ModeSwitchButtons.AddToolBarButton(FUIAction(FExecuteAction::CreateLambda([=]()
		{
			PaintModeSettings->PaintMode = EPaintMode::Textures;
			SettingsDetailsView->SetObjects(SettingsObjects, true);
		}), FCanExecuteAction(), FIsActionChecked::CreateLambda([=]() -> bool { return PaintModeSettings->PaintMode == EPaintMode::Textures; })), NAME_None, LOCTEXT("Mode.TexturePainting", "Textures"), LOCTEXT("Mode.Texture.Tooltip", "Texture Weight Painting mode allows painting on Textures"), TexturePaintIcon, EUserInterfaceActionType::ToggleButton);
	}

	return ModeSwitchButtons.MakeWidget();
}

EVisibility SPaintModeWidget::IsVertexPaintModeVisible() const
{
	UPaintModeSettings* MeshPaintSettings = (UPaintModeSettings*)MeshPainter->GetPainterSettings();
	return (MeshPaintSettings->PaintMode == EPaintMode::Vertices) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SPaintModeWidget::IsTexturePaintModeVisible() const
{
	UPaintModeSettings* MeshPaintSettings = (UPaintModeSettings*)MeshPainter->GetPainterSettings();
	return (MeshPaintSettings->PaintMode == EPaintMode::Textures) ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE // "PaintModePainter"