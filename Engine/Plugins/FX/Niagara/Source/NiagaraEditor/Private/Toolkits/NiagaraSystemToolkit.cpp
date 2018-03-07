// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemToolkit.h"
#include "NiagaraEditorModule.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraObjectSelection.h"
#include "NiagaraSystemViewModel.h"
#include "NiagaraEmitterHandleviewModel.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "Widgets/SNiagaraCurveEditor.h"
#include "Widgets/SNiagaraSystemScript.h"
#include "Widgets/SNiagaraSystemViewport.h"
#include "Widgets/SNiagaraSelectedObjectsDetails.h"
#include "Widgets/SNiagaraSelectedEmitterHandle.h"
#include "Widgets/SNiagaraSpreadsheetView.h"
#include "Widgets/SNiagaraGeneratedCodeView.h"
#include "Widgets/SNiagaraScriptGraph.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraSystemFactoryNew.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "EditorStyleSet.h"
#include "AssetEditorToolkit.h"
#include "WorkspaceItem.h"
#include "ScopedTransaction.h"

#include "SlateApplication.h"
#include "SBoxPanel.h"
#include "SBox.h"
#include "SDockTab.h"
#include "AdvancedPreviewSceneModule.h"
#include "BusyCursor.h"
#include "Misc/FeedbackContext.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemEditor"

const FName FNiagaraSystemToolkit::ViewportTabID(TEXT("NiagaraSystemEditor_Viewport"));
const FName FNiagaraSystemToolkit::CurveEditorTabID(TEXT("NiagaraSystemEditor_CurveEditor"));
const FName FNiagaraSystemToolkit::SequencerTabID(TEXT("NiagaraSystemEditor_Sequencer"));
const FName FNiagaraSystemToolkit::SystemScriptTabID(TEXT("NiagaraSystemEditor_SystemScript"));
const FName FNiagaraSystemToolkit::SystemDetailsTabID(TEXT("NiagaraSystemEditor_SystemDetails"));
const FName FNiagaraSystemToolkit::SelectedEmitterStackTabID(TEXT("NiagaraSystemEditor_SelectedEmitterStack"));
const FName FNiagaraSystemToolkit::SelectedEmitterGraphTabID(TEXT("NiagaraSystemEditor_SelectedEmitterGraph"));
const FName FNiagaraSystemToolkit::DebugSpreadsheetTabID(TEXT("NiagaraSystemEditor_DebugAttributeSpreadsheet"));
const FName FNiagaraSystemToolkit::PreviewSettingsTabId(TEXT("NiagaraSystemEditor_PreviewSettings"));
const FName FNiagaraSystemToolkit::GeneratedCodeTabID(TEXT("NiagaraSystemEditor_GeneratedCode"));

void FNiagaraSystemToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraSystemEditor", "Niagara System"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("Preview", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(CurveEditorTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_CurveEd))
		.SetDisplayName(LOCTEXT("Curves", "Curves"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SequencerTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_Sequencer))
		.SetDisplayName(LOCTEXT("Timeline", "Timeline"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SystemScriptTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SystemScript))
		.SetDisplayName(LOCTEXT("SystemScript", "System Script"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SystemDetailsTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SystemDetails))
		.SetDisplayName(LOCTEXT("SystemDetails", "System Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SelectedEmitterStackTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SelectedEmitterStack))
		.SetDisplayName(LOCTEXT("SelectedEmitterStack", "Selected Emitter"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SelectedEmitterGraphTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SelectedEmitterGraph))
		.SetDisplayName(LOCTEXT("SelectedEmitterGraph", "Selected Emitter Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DebugSpreadsheetTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_DebugSpreadsheet))
		.SetDisplayName(LOCTEXT("DebugSpreadsheet", "Attribute Spreadsheet"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(GeneratedCodeTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_GeneratedCode))
		.SetDisplayName(LOCTEXT("GeneratedCode", "Generated Code"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FNiagaraSystemToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabID);
	InTabManager->UnregisterTabSpawner(CurveEditorTabID);
	InTabManager->UnregisterTabSpawner(SequencerTabID);
	InTabManager->UnregisterTabSpawner(SystemScriptTabID);
	InTabManager->UnregisterTabSpawner(SystemDetailsTabID);
	InTabManager->UnregisterTabSpawner(SelectedEmitterStackTabID);
	InTabManager->UnregisterTabSpawner(SelectedEmitterGraphTabID);
	InTabManager->UnregisterTabSpawner(DebugSpreadsheetTabID);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(GeneratedCodeTabID);
}


FNiagaraSystemToolkit::~FNiagaraSystemToolkit()
{
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->Cleanup();
	}
	SystemViewModel.Reset();
}

void FNiagaraSystemToolkit::AddReferencedObjects(FReferenceCollector& Collector) 
{
	Collector.AddReferencedObject(System);
}

void FNiagaraSystemToolkit::InitializeWithSystem(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraSystem& InSystem)
{
	System = &InSystem;
	Emitter = nullptr;

	// In the FNiagaraCustomVersion::UpdateSpawnEventGraphCombination we merged graphs. We update the graph source here because there isn't a good place to do it in the postload pipeline.
	bool bConverted = false;
	for (int32 i = 0; i < System->GetNumEmitters(); i++)
	{
		FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(i);
		if (Handle.GetSource() == nullptr)
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Missing source emitter!"));
			break;
		}

		if (Handle.GetSource()->GraphSource == nullptr)
		{
			if (FNiagaraEditorUtilities::ConvertToMergedGraph((UNiagaraEmitter*)Handle.GetSource()))
			{
				bConverted = true;
			}
			else
			{
				UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to merge emitter!"));
			}
		}
	}

	if (bConverted)
	{
		System->ResynchronizeAllHandles();
	}

	FNiagaraSystemViewModelOptions SystemOptions;
	SystemOptions.bCanRemoveEmittersFromTimeline = true;
	SystemOptions.bCanRenameEmittersFromTimeline = true;
	SystemOptions.bCanAddEmittersFromTimeline = true;
	SystemOptions.bUseSystemExecStateForTimelineReset = true;
	SystemOptions.OnGetSequencerAddMenuContent.BindSP(this, &FNiagaraSystemToolkit::GetSequencerAddMenuContent);

	System->CheckForUpdates();

	SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, SystemOptions));
	SystemToolkitMode = ESystemToolkitMode::System;
	InitializeInternal(Mode, InitToolkitHost);
}

void FNiagaraSystemToolkit::InitializeWithEmitter(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEmitter& InEmitter)
{
	// In the FNiagaraCustomVersion::UpdateSpawnEventGraphCombination we merged graphs. We update the graph source here because there isn't a good place to do it in the postload pipeline.
	if (InEmitter.GraphSource == nullptr)
	{
		if (false == FNiagaraEditorUtilities::ConvertToMergedGraph(&InEmitter))
		{
			UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to merge emitter!"));
		}
	}

	System = NewObject<UNiagaraSystem>(GetTransientPackage(), NAME_None, RF_Transient);
	UNiagaraSystemFactoryNew::InitializeSystem(System);

	Emitter = &InEmitter;

	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	UNiagaraEmitter* EditableEmitter = (UNiagaraEmitter*)StaticDuplicateObject(Emitter, GetTransientPackage(), NAME_None, ~RF_Standalone, UNiagaraEmitter::StaticClass());
	System->AddEmitterHandleWithoutCopying(*EditableEmitter);

	FNiagaraSystemViewModelOptions SystemOptions;
	SystemOptions.bCanRemoveEmittersFromTimeline = false;
	SystemOptions.bCanRenameEmittersFromTimeline = false;
	SystemOptions.bCanAddEmittersFromTimeline = false;
	SystemOptions.bUseSystemExecStateForTimelineReset = false;

	SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, SystemOptions));
	SystemViewModel->GetSystemScriptViewModel()->RebuildEmitterNodes();
	SystemToolkitMode = ESystemToolkitMode::Emitter;
	InitializeInternal(Mode, InitToolkitHost);
}

void FNiagaraSystemToolkit::InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	if (SystemViewModel->GetEmitterHandleViewModels().Num() > 0)
	{
		SystemViewModel->SetSelectedEmitterHandleById(SystemViewModel->GetEmitterHandleViewModels()[0]->GetId());
	}

	const float InTime = -0.02f;
	const float OutTime = 3.2f;

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_System_Layout_v17")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
				->SetHideTabWell(true)
				)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(.75f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(.75f)
						->AddTab(ViewportTabID, ETabState::OpenedTab)
						)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.25f)
						->AddTab(CurveEditorTabID, ETabState::OpenedTab)
						->AddTab(SequencerTabID, ETabState::OpenedTab)
						)
					)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(SelectedEmitterStackTabID, ETabState::OpenedTab)
					->AddTab(SelectedEmitterGraphTabID, ETabState::ClosedTab)
					->AddTab(SystemScriptTabID, ETabState::ClosedTab)
					->AddTab(SystemDetailsTabID, ETabState::ClosedTab)
					->AddTab(DebugSpreadsheetTabID, ETabState::ClosedTab)
					->AddTab(PreviewSettingsTabId, ETabState::ClosedTab)
					->AddTab(GeneratedCodeTabID, ETabState::ClosedTab)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	UObject* ToolkitObject = SystemToolkitMode == ESystemToolkitMode::System ? (UObject*)System : (UObject*)Emitter;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier,
		StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ToolkitObject);
	
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FName FNiagaraSystemToolkit::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraSystemToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraSystemToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraSystemToolkit::GetWorldCentricTabColorScale() const
{
	return FNiagaraEditorModule::WorldCentricTabColorScale;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == ViewportTabID);

	Viewport = SNew(SNiagaraSystemViewport);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Viewport.ToSharedRef()
		];

	Viewport->SetPreviewComponent(SystemViewModel->GetPreviewComponent());
	Viewport->OnAddedToTab(SpawnedTab);

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	TSharedRef<SWidget> InWidget = SNullWidget::NullWidget;
	if (Viewport.IsValid())
	{
		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		InWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(Viewport->GetPreviewScene());
	}

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			InWidget
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_CurveEd(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == CurveEditorTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraCurveEditor, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_Sequencer(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SequencerTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SystemViewModel->GetSequencer()->GetSequencerWidget()
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SystemScript(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SystemScriptTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSystemScript, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SystemDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SystemDetailsTabID);

	TSharedRef<FNiagaraObjectSelection> SystemSelection = MakeShareable(new FNiagaraObjectSelection());
	SystemSelection->SetSelectedObject(System);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedObjectsDetails, SystemSelection)
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SelectedEmitterStack(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SelectedEmitterStackTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedEmitterHandle, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

class SNiagaraSelectedEmitterGraph : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSelectedEmitterGraph)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
	{
		SystemViewModel = InSystemViewModel;
		SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraSelectedEmitterGraph::SelectedEmitterHandlesChanged);
		ChildSlot
		[
			SAssignNew(GraphWidgetContainer, SBox)
		];
		UpdateGraphWidget();
	}

	~SNiagaraSelectedEmitterGraph()
	{
		if (SystemViewModel.IsValid())
		{
			SystemViewModel->OnCurveOwnerChanged().RemoveAll(this);
		}
	}

private:
	void SelectedEmitterHandlesChanged()
	{
		UpdateGraphWidget();
	}

	void UpdateGraphWidget()
	{
		TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
		SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
		if (SelectedEmitterHandles.Num() == 1)
		{
			GraphWidgetContainer->SetContent(SNew(SNiagaraScriptGraph, SelectedEmitterHandles[0]->GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()));
		}
		else
		{
			GraphWidgetContainer->SetContent(SNullWidget::NullWidget);
		}
	}

private:
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;
	TSharedPtr<SBox> GraphWidgetContainer;
};

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SelectedEmitterGraph(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SelectedEmitterGraphTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedEmitterGraph, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_DebugSpreadsheet(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == DebugSpreadsheetTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSpreadsheetView, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_GeneratedCode(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GeneratedCodeTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraGeneratedCodeView, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

void FNiagaraSystemToolkit::SetupCommands()
{
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleUnlockToChanges,
		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::ToggleUnlockToChanges),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FNiagaraSystemToolkit::IsToggleUnlockToChangesChecked));
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().Compile,
		FExecuteAction::CreateRaw(this, &FNiagaraSystemToolkit::CompileSystem));
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ResetSimulation,
		FExecuteAction::CreateRaw(this, &FNiagaraSystemToolkit::ResetSimulation));
}

void FNiagaraSystemToolkit::ResetSimulation()
{
	SystemViewModel->ResetSystem();
}

void FNiagaraSystemToolkit::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FNiagaraSystemToolkit* Toolkit)
		{
			ToolbarBuilder.BeginSection("Compile");
			{
				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().Compile,
					NAME_None,
					TAttribute<FText>(),
					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetCompileStatusTooltip),
					TAttribute<FSlateIcon>(Toolkit, &FNiagaraSystemToolkit::GetCompileStatusImage),
					FName(TEXT("CompileNiagaraSystem")));
				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateStatic(&FNiagaraSystemToolkit::GenerateCompileMenuContent),
					LOCTEXT("BuildCombo_Label", "Auto-Compile Options"),
					LOCTEXT("BuildComboToolTip", "Auto-Compile options menu"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Build"),
					true);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("LockEmitters");
			{
				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().ToggleUnlockToChanges, NAME_None,
					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesLabel),
					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesLabelTooltip),
					TAttribute<FSlateIcon>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesIcon));
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this)
		);

	AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FNiagaraSystemToolkit::GetSequencerAddMenuContent(FMenuBuilder& MenuBuilder, TSharedRef<ISequencer> Sequencer)
{
	MenuBuilder.AddSubMenu(
		LOCTEXT("EmittersLabel", "Emitters..."),
		LOCTEXT("EmittersToolTip", "Add an existing emitter..."),
		FNewMenuDelegate::CreateLambda([&](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddWidget(CreateAddEmitterMenuContent(), FText());
		}));
}

TSharedRef<SWidget> FNiagaraSystemToolkit::CreateAddEmitterMenuContent()
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FNiagaraSystemToolkit::EmitterAssetSelected);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(UNiagaraEmitter::StaticClass()->GetFName());
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	return SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

TSharedRef<SWidget> FNiagaraSystemToolkit::GenerateCompileMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FUIAction Action(
		FExecuteAction::CreateStatic(&FNiagaraSystemToolkit::ToggleCompileEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FNiagaraSystemToolkit::IsAutoCompileEnabled));

	MenuBuilder.AddMenuEntry(LOCTEXT("AutoCompile", "Automatically compile when graph changes"), FText(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);

	return MenuBuilder.MakeWidget();
}

FSlateIcon FNiagaraSystemToolkit::GetCompileStatusImage() const
{
	ENiagaraScriptCompileStatus Status = SystemViewModel->GetLatestCompileStatus();

	switch (Status)
	{
	default:
	case ENiagaraScriptCompileStatus::NCS_Unknown:
	case ENiagaraScriptCompileStatus::NCS_Dirty:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Unknown");
	case ENiagaraScriptCompileStatus::NCS_Error:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Error");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Good");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Warning");
	}
}

FText FNiagaraSystemToolkit::GetCompileStatusTooltip() const
{
	ENiagaraScriptCompileStatus Status = SystemViewModel->GetLatestCompileStatus();
	return FNiagaraEditorUtilities::StatusToText(Status);
}


void FNiagaraSystemToolkit::CompileSystem()
{
	SystemViewModel->CompileSystem();
}

void FNiagaraSystemToolkit::UpdateOriginalEmitter()
{
	checkf(SystemToolkitMode == ESystemToolkitMode::Emitter, TEXT("There is no original emitter to update in system mode."));

	const FScopedBusyCursor BusyCursor;
	const FText LocalizedScriptEditorApply = NSLOCTEXT("UnrealEd", "ToolTip_NiagaraEmitterEditorApply", "Apply changes to original emitter and its use in the world.");
	GWarn->BeginSlowTask(LocalizedScriptEditorApply, true);
	GWarn->StatusUpdate(1, 1, LocalizedScriptEditorApply);

	if (Emitter->IsSelected())
	{
		GEditor->GetSelectedObjects()->Deselect(Emitter);
	}

	ResetLoaders(Emitter->GetOutermost()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	Emitter->GetOutermost()->LinkerCustomVersion.Empty();

	TSharedPtr<FNiagaraEmitterViewModel> EditableEmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
	UNiagaraEmitter* EditableEmitter = EditableEmitterViewModel->GetEmitter();

	// overwrite the original script in place by constructing a new one with the same name
	Emitter = (UNiagaraEmitter*)StaticDuplicateObject(EditableEmitter, Emitter->GetOuter(), Emitter->GetFName(),
		RF_AllFlags,
		Emitter->GetClass());

	// Restore RF_Standalone on the original material, as it had been removed from the preview material so that it could be GC'd.
	Emitter->SetFlags(RF_Standalone);

	TArray<UNiagaraEmitter*> AffectedEmitters;
	AffectedEmitters.Add(Emitter);
	UpdateExistingEmitters(AffectedEmitters);

	GWarn->EndSlowTask();
	EditableEmitterViewModel->SetDirty(false);
}

void FNiagaraSystemToolkit::UpdateExistingEmitters(TArray<UNiagaraEmitter*> AffectedEmitters)
{
	// Compile the existing emitters. Also determine which Systems need to be properly updated.
	TArray<UNiagaraSystem*> AffectedSystemSystems;
	for (UNiagaraEmitter* AffectedEmitter : AffectedEmitters)
	{
		if (AffectedEmitter->IsPendingKillOrUnreachable())
		{
			continue;
		}

		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = FNiagaraEmitterViewModel::GetExistingViewModelForObject(AffectedEmitter);
		if (!EmitterViewModel.IsValid())
		{
			EmitterViewModel = MakeShareable(new FNiagaraEmitterViewModel(AffectedEmitter, nullptr));
		}
		EmitterViewModel->CompileScripts();

		for (TObjectIterator<UNiagaraSystem> It; It; ++It)
		{
			if (It->GetAutoImportChangedEmitters() && It->ReferencesSourceEmitter(AffectedEmitter))
			{
				AffectedSystemSystems.AddUnique(*It);
			}
		}
	}

	// Now iterate over the affected Systems.
	for (UNiagaraSystem* System : AffectedSystemSystems)
	{
		TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = FNiagaraSystemViewModel::GetExistingViewModelForObject(System);
		if (!SystemViewModel.IsValid())
		{
			FNiagaraSystemViewModelOptions Options;
			Options.bCanRemoveEmittersFromTimeline = false;
			Options.bCanRenameEmittersFromTimeline = false;
			Options.bCanAddEmittersFromTimeline = false;
			Options.bUseSystemExecStateForTimelineReset = false;
			SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, Options));
		}

		SystemViewModel->ResynchronizeAllHandles();
	}
}

void FNiagaraSystemToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		OutObjects.Add(Emitter);
	}
	else
	{
		FAssetEditorToolkit::GetSaveableObjects(OutObjects);
	}
}

void FNiagaraSystemToolkit::SaveAsset_Execute()
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraEmitter %s"), *GetEditingObjects()[0]->GetName());
		UpdateOriginalEmitter();
	}

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FNiagaraSystemToolkit::SaveAssetAs_Execute()
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraEmitter %s"), *GetEditingObjects()[0]->GetName());
		UpdateOriginalEmitter();
	}

	FAssetEditorToolkit::SaveAssetAs_Execute();
}

bool FNiagaraSystemToolkit::OnRequestClose()
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
		if (EmitterViewModel->GetDirty())
		{
			// find out the user wants to do with this dirty NiagaraScript
			EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel,
				FText::Format(
					NSLOCTEXT("UnrealEd", "Prompt_NiagaraEmitterEditorClose", "Would you like to apply changes to this Emitter to the original Emitter?\n{0}\n(No will lose all changes!)"),
					FText::FromString(Emitter->GetPathName())));

			// act on it
			switch (YesNoCancelReply)
			{
			case EAppReturnType::Yes:
				// update NiagaraScript and exit
				UpdateOriginalEmitter();
				break;

			case EAppReturnType::No:
				// exit
				// do nothing.. bNiagaraScriptDirty = false;
				break;

			case EAppReturnType::Cancel:
				// don't exit
				return false;
			}
		}
		return true;
	}

	return FAssetEditorToolkit::OnRequestClose();
}

void FNiagaraSystemToolkit::EmitterAssetSelected(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissAllMenus();
	SystemViewModel->AddEmitterFromAssetData(AssetData);
}

void FNiagaraSystemToolkit::ToggleUnlockToChanges()
{
	const FScopedTransaction ToggleUnlockToChangesTransaction(LOCTEXT("ToggleUnlockToChanges", "Toggle System Unlock To Changes"));
	System->Modify();

	System->SetAutoImportChangedEmitters(!System->GetAutoImportChangedEmitters());

	if (System->GetAutoImportChangedEmitters())
	{
		SystemViewModel->ResynchronizeAllHandles();
	}
}

bool FNiagaraSystemToolkit::IsToggleUnlockToChangesChecked()
{
	return System->GetAutoImportChangedEmitters();
}

FText FNiagaraSystemToolkit::GetEmitterLockToChangesLabel() const
{
	if (System->GetAutoImportChangedEmitters())
	{
		return LOCTEXT("EmitterUnlockToChangesLabel", "Changes Unlocked");
	}
	else
	{
		return LOCTEXT("EmitterLockToChangesLabel", "Changes Locked");
	}
}

FText FNiagaraSystemToolkit::GetEmitterLockToChangesLabelTooltip() const
{
	if (System->GetAutoImportChangedEmitters())
	{
		return LOCTEXT("EmitterUnlockToChangesLabelTooltip", "If a source emitter changes, the changes will be imported into this System automatically.");
	}
	else
	{
		return LOCTEXT("EmitterLockToChangesLabelTooltip", "If a source emitter changes, the changes will NOT be imported into this System automatically.");
	}
}

FSlateIcon FNiagaraSystemToolkit::GetEmitterLockToChangesIcon() const
{
	if (System->GetAutoImportChangedEmitters())
	{
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.UnlockToChanges");
	}
	else
	{
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.LockToChanges");
	}
}

void FNiagaraSystemToolkit::ToggleCompileEnabled()
{
	UNiagaraEditorSettings* Settings = GetMutableDefault<UNiagaraEditorSettings>();
	Settings->bAutoCompile = !Settings->bAutoCompile;
}

bool FNiagaraSystemToolkit::IsAutoCompileEnabled()
{
	return GetDefault<UNiagaraEditorSettings>()->bAutoCompile;
}

#undef LOCTEXT_NAMESPACE
