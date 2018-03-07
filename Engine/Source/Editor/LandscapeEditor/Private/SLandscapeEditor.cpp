// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLandscapeEditor.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SErrorText.h"
#include "EditorStyleSet.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEditorCommands.h"
#include "LandscapeEditorObject.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "IIntroTutorials.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor"

void SLandscapeAssetThumbnail::Construct(const FArguments& InArgs, UObject* Asset, TSharedRef<FAssetThumbnailPool> ThumbnailPool)
{
	FIntPoint ThumbnailSize = InArgs._ThumbnailSize;

	AssetThumbnail = MakeShareable(new FAssetThumbnail(Asset, ThumbnailSize.X, ThumbnailSize.Y, ThumbnailPool));

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ThumbnailSize.X)
		.HeightOverride(ThumbnailSize.Y)
		[
			AssetThumbnail->MakeThumbnailWidget()
		]
	];

	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(Asset);
	if (MaterialInterface)
	{
		UMaterial::OnMaterialCompilationFinished().AddSP(this, &SLandscapeAssetThumbnail::OnMaterialCompilationFinished);
	}
}

SLandscapeAssetThumbnail::~SLandscapeAssetThumbnail()
{
	UMaterial::OnMaterialCompilationFinished().RemoveAll(this);
}

void SLandscapeAssetThumbnail::OnMaterialCompilationFinished(UMaterialInterface* MaterialInterface)
{
	UMaterialInterface* MaterialAsset = Cast<UMaterialInterface>(AssetThumbnail->GetAsset());
	if (MaterialAsset)
	{
		if (MaterialAsset->IsDependent(MaterialInterface))
		{
			// Refresh thumbnail
			AssetThumbnail->SetAsset(AssetThumbnail->GetAsset());
		}
	}
}

void SLandscapeAssetThumbnail::SetAsset(UObject* Asset)
{
	AssetThumbnail->SetAsset(Asset);
}

//////////////////////////////////////////////////////////////////////////

void FLandscapeToolKit::RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager)
{
}

void FLandscapeToolKit::UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager)
{
}

void FLandscapeToolKit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	auto NameToCommandMap = FLandscapeEditorCommands::Get().NameToCommandMap;

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	TSharedRef<FUICommandList> CommandList = LandscapeEdMode->GetUICommandList();

#define MAP_MODE(ModeName) CommandList->MapAction(NameToCommandMap.FindChecked(ModeName), FUIAction(FExecuteAction::CreateSP(this, &FLandscapeToolKit::OnChangeMode, FName(ModeName)), FCanExecuteAction::CreateSP(this, &FLandscapeToolKit::IsModeEnabled, FName(ModeName)), FIsActionChecked::CreateSP(this, &FLandscapeToolKit::IsModeActive, FName(ModeName))));
	MAP_MODE("ToolMode_Manage");
	MAP_MODE("ToolMode_Sculpt");
	MAP_MODE("ToolMode_Paint");
#undef MAP_MODE

#define MAP_TOOL(ToolName) CommandList->MapAction(NameToCommandMap.FindChecked("Tool_" ToolName), FUIAction(FExecuteAction::CreateSP(this, &FLandscapeToolKit::OnChangeTool, FName(ToolName)), FCanExecuteAction::CreateSP(this, &FLandscapeToolKit::IsToolEnabled, FName(ToolName)), FIsActionChecked::CreateSP(this, &FLandscapeToolKit::IsToolActive, FName(ToolName))));
	MAP_TOOL("NewLandscape");
	MAP_TOOL("ResizeLandscape");

	MAP_TOOL("Sculpt");
	MAP_TOOL("Paint");
	MAP_TOOL("Smooth");
	MAP_TOOL("Flatten");
	MAP_TOOL("Ramp");
	MAP_TOOL("Erosion");
	MAP_TOOL("HydraErosion");
	MAP_TOOL("Noise");
	MAP_TOOL("Retopologize");
	MAP_TOOL("Visibility");

	MAP_TOOL("Select");
	MAP_TOOL("AddComponent");
	MAP_TOOL("DeleteComponent");
	MAP_TOOL("MoveToLevel");

	MAP_TOOL("Mask");
	MAP_TOOL("CopyPaste");
	MAP_TOOL("Mirror");

	MAP_TOOL("Splines");
#undef MAP_TOOL

#define MAP_BRUSH_SET(BrushSetName) CommandList->MapAction(NameToCommandMap.FindChecked(BrushSetName), FUIAction(FExecuteAction::CreateSP(this, &FLandscapeToolKit::OnChangeBrushSet, FName(BrushSetName)), FCanExecuteAction::CreateSP(this, &FLandscapeToolKit::IsBrushSetEnabled, FName(BrushSetName)), FIsActionChecked::CreateSP(this, &FLandscapeToolKit::IsBrushSetActive, FName(BrushSetName))));
	MAP_BRUSH_SET("BrushSet_Circle");
	MAP_BRUSH_SET("BrushSet_Alpha");
	MAP_BRUSH_SET("BrushSet_Pattern");
	MAP_BRUSH_SET("BrushSet_Component");
	MAP_BRUSH_SET("BrushSet_Gizmo");
#undef MAP_BRUSH_SET

#define MAP_BRUSH(BrushName) CommandList->MapAction(NameToCommandMap.FindChecked(BrushName), FUIAction(FExecuteAction::CreateSP(this, &FLandscapeToolKit::OnChangeBrush, FName(BrushName)), FCanExecuteAction(), FIsActionChecked::CreateSP(this, &FLandscapeToolKit::IsBrushActive, FName(BrushName))));
	MAP_BRUSH("Circle_Smooth");
	MAP_BRUSH("Circle_Linear");
	MAP_BRUSH("Circle_Spherical");
	MAP_BRUSH("Circle_Tip");
#undef MAP_BRUSH

	LandscapeEditorWidgets = SNew(SLandscapeEditor, SharedThis(this));

	FModeToolkit::Init(InitToolkitHost);
}

FName FLandscapeToolKit::GetToolkitFName() const
{
	return FName("LandscapeEditor");
}

FText FLandscapeToolKit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Landscape Editor");
}

FEdModeLandscape* FLandscapeToolKit::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

TSharedPtr<SWidget> FLandscapeToolKit::GetInlineContent() const
{
	return LandscapeEditorWidgets;
}

void FLandscapeToolKit::NotifyToolChanged()
{
	LandscapeEditorWidgets->NotifyToolChanged();
}

void FLandscapeToolKit::NotifyBrushChanged()
{
	LandscapeEditorWidgets->NotifyBrushChanged();
}

void FLandscapeToolKit::RefreshDetailPanel()
{
	LandscapeEditorWidgets->RefreshDetailPanel();
}

void FLandscapeToolKit::OnChangeMode(FName ModeName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetCurrentToolMode(ModeName);
	}
}

bool FLandscapeToolKit::IsModeEnabled(FName ModeName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// Manage is the only mode enabled if we have no landscape
		if (ModeName == "ToolMode_Manage" || LandscapeEdMode->GetLandscapeList().Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool FLandscapeToolKit::IsModeActive(FName ModeName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentTool)
	{
		return LandscapeEdMode->CurrentToolMode->ToolModeName == ModeName;
	}

	return false;
}

void FLandscapeToolKit::OnChangeTool(FName ToolName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->SetCurrentTool(ToolName);
	}
}

bool FLandscapeToolKit::IsToolEnabled(FName ToolName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		if (ToolName == "NewLandscape" || LandscapeEdMode->GetLandscapeList().Num() > 0)
		{
			return true;
		}
	}

	return false;
}

bool FLandscapeToolKit::IsToolActive(FName ToolName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && LandscapeEdMode->CurrentTool != nullptr)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();
		return CurrentToolName == ToolName;
	}

	return false;
}

void FLandscapeToolKit::OnChangeBrushSet(FName BrushSetName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->SetCurrentBrushSet(BrushSetName);
	}
}

bool FLandscapeToolKit::IsBrushSetEnabled(FName BrushSetName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentTool)
	{
		return LandscapeEdMode->CurrentTool->ValidBrushes.Contains(BrushSetName);
	}

	return false;
}

bool FLandscapeToolKit::IsBrushSetActive(FName BrushSetName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentBrushSetIndex >= 0)
	{
		const FName CurrentBrushSetName = LandscapeEdMode->LandscapeBrushSets[LandscapeEdMode->CurrentBrushSetIndex].BrushSetName;
		return CurrentBrushSetName == BrushSetName;
	}

	return false;
}

void FLandscapeToolKit::OnChangeBrush(FName BrushName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr)
	{
		LandscapeEdMode->SetCurrentBrush(BrushName);
	}
}

bool FLandscapeToolKit::IsBrushActive(FName BrushName) const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentBrush)
	{
		const FName CurrentBrushName = LandscapeEdMode->CurrentBrush->GetBrushName();
		return CurrentBrushName == BrushName;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SLandscapeEditor::Construct(const FArguments& InArgs, TSharedRef<FLandscapeToolKit> InParentToolkit)
{
	TSharedRef<FUICommandList> CommandList = InParentToolkit->GetToolkitCommands();

	// Modes:
	FToolBarBuilder ModeSwitchButtons(CommandList, FMultiBoxCustomization::None);
	{
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().ManageMode, NAME_None, LOCTEXT("Mode.Manage", "Manage"), LOCTEXT("Mode.Manage.Tooltip", "Contains tools to add a new landscape, import/export landscape, add/remove components and manage streaming"));
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().SculptMode, NAME_None, LOCTEXT("Mode.Sculpt", "Sculpt"), LOCTEXT("Mode.Sculpt.Tooltip", "Contains tools that modify the shape of a landscape"));
		ModeSwitchButtons.AddToolBarButton(FLandscapeEditorCommands::Get().PaintMode,  NAME_None, LOCTEXT("Mode.Paint",  "Paint"),  LOCTEXT("Mode.Paint.Tooltip",  "Contains tools that paint materials on to a landscape"));
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, false,FDetailsViewArgs::HideNameArea);

	DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsPanel->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SLandscapeEditor::GetIsPropertyVisible));

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		DetailsPanel->SetObject(LandscapeEdMode->UISettings);
	}

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 5)
		[
			SAssignNew(Error, SErrorText)
		]
		+ SVerticalBox::Slot()
		.Padding(0)
		[
			SNew(SVerticalBox)
			.IsEnabled(this, &SLandscapeEditor::GetLandscapeEditorIsEnabled)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4, 0, 4, 5)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.HAlign(HAlign_Center)
					[

						ModeSwitchButtons.MakeWidget()
					]
				]

				// Tutorial link
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(4)
				[
					IntroTutorials.CreateTutorialsWidget(TEXT("LandscapeMode"))
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0)
			[
				DetailsPanel.ToSharedRef()
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FEdModeLandscape* SLandscapeEditor::GetEditorMode() const
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

FText SLandscapeEditor::GetErrorText() const
{
	const FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	ELandscapeEditingState EditState = LandscapeEdMode->GetEditingState();
	switch (EditState)
	{
		case ELandscapeEditingState::SIEWorld:
		{

			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsSimulatingError_create", "Can't create landscape while simulating!");
			}
			else
			{
				return LOCTEXT("IsSimulatingError_edit", "Can't edit landscape while simulating!");
			}
			break;
		}
		case ELandscapeEditingState::PIEWorld:
		{
			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsPIEError_create", "Can't create landscape in PIE!");
			}
			else
			{
				return LOCTEXT("IsPIEError_edit", "Can't edit landscape in PIE!");
			}
			break;
		}
		case ELandscapeEditingState::BadFeatureLevel:
		{
			if (LandscapeEdMode->NewLandscapePreviewMode != ENewLandscapePreviewMode::None)
			{
				return LOCTEXT("IsFLError_create", "Can't create landscape with a feature level less than SM4!");
			}
			else
			{
				return LOCTEXT("IsFLError_edit", "Can't edit landscape with a feature level less than SM4!");
			}
			break;
		}
		case ELandscapeEditingState::NoLandscape:
		{
			return LOCTEXT("NoLandscapeError", "No Landscape!");
		}
		case ELandscapeEditingState::Enabled:
		{
			return FText::GetEmpty();
		}
		default:
			checkNoEntry();
	}

	return FText::GetEmpty();
}

bool SLandscapeEditor::GetLandscapeEditorIsEnabled() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		Error->SetError(GetErrorText());
		return LandscapeEdMode->GetEditingState() == ELandscapeEditingState::Enabled;
	}
	return false;
}

bool SLandscapeEditor::GetIsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const UProperty& Property = PropertyAndParent.Property;

	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && LandscapeEdMode->CurrentTool != nullptr)
	{
		if (Property.HasMetaData("ShowForMask"))
		{
			const bool bMaskEnabled = LandscapeEdMode->CurrentTool &&
				LandscapeEdMode->CurrentTool->SupportsMask() &&
				LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() &&
				LandscapeEdMode->CurrentToolTarget.LandscapeInfo->SelectedRegion.Num() > 0;

			if (bMaskEnabled)
			{
				return true;
			}
		}
		if (Property.HasMetaData("ShowForTools"))
		{
			const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();

			TArray<FString> ShowForTools;
			Property.GetMetaData("ShowForTools").ParseIntoArray(ShowForTools, TEXT(","), true);
			if (!ShowForTools.Contains(CurrentToolName.ToString()))
			{
				return false;
			}
		}
		if (Property.HasMetaData("ShowForBrushes"))
		{
			const FName CurrentBrushSetName = LandscapeEdMode->LandscapeBrushSets[LandscapeEdMode->CurrentBrushSetIndex].BrushSetName;
			// const FName CurrentBrushName = LandscapeEdMode->CurrentBrush->GetBrushName();

			TArray<FString> ShowForBrushes;
			Property.GetMetaData("ShowForBrushes").ParseIntoArray(ShowForBrushes, TEXT(","), true);
			if (!ShowForBrushes.Contains(CurrentBrushSetName.ToString()))
				//&& !ShowForBrushes.Contains(CurrentBrushName.ToString())
			{
				return false;
			}
		}
		if (Property.HasMetaData("ShowForTargetTypes"))
		{
			static const TCHAR* TargetTypeNames[] = { TEXT("Heightmap"), TEXT("Weightmap"), TEXT("Visibility") };

			TArray<FString> ShowForTargetTypes;
			Property.GetMetaData("ShowForTargetTypes").ParseIntoArray(ShowForTargetTypes, TEXT(","), true);

			const ELandscapeToolTargetType::Type CurrentTargetType = LandscapeEdMode->CurrentToolTarget.TargetType;
			if (CurrentTargetType == ELandscapeToolTargetType::Invalid ||
				ShowForTargetTypes.FindByKey(TargetTypeNames[CurrentTargetType]) == nullptr)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void SLandscapeEditor::NotifyToolChanged()
{
	RefreshDetailPanel();
}

void SLandscapeEditor::NotifyBrushChanged()
{
	RefreshDetailPanel();
}

void SLandscapeEditor::RefreshDetailPanel()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		// Refresh details panel
		DetailsPanel->SetObject(LandscapeEdMode->UISettings, true);
	}
}

#undef LOCTEXT_NAMESPACE
