// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptToolkit.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraScript.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraObjectSelection.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraScriptInputCollectionViewModel.h"
#include "NiagaraScriptOutputCollectionViewModel.h"
#include "SNiagaraScriptGraph.h"
#include "SNiagaraSelectedObjectsDetails.h"
#include "SNiagaraParameterCollection.h"
#include "Package.h"
#include "NiagaraEditorStyle.h"
#include "BusyCursor.h"
#include "EngineGlobals.h"
#include "FeedbackContext.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "UObjectIterator.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNode.h"
#include "NiagaraGraph.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraModule.h"

#include "ModuleManager.h"

#include "MultiBoxBuilder.h"
#include "SScrollBox.h"
#include "SDockTab.h"
#include "Misc/MessageDialog.h"
#include "Customizations/NiagaraScriptDetails.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptToolkit"

static TAutoConsoleVariable<int32> CVarDevDetails(
	TEXT("fx.DevDetailsPanels"),
	0,
	TEXT("Whether to enable the development details panels inside Niagara."));


const FName FNiagaraScriptToolkit::NodeGraphTabId(TEXT("NiagaraEditor_NodeGraph")); 
const FName FNiagaraScriptToolkit::DetailsTabId(TEXT("NiagaraEditor_Details"));
const FName FNiagaraScriptToolkit::ParametersTabId(TEXT("NiagaraEditor_Parameters"));

FNiagaraScriptToolkit::FNiagaraScriptToolkit()
{
}

FNiagaraScriptToolkit::~FNiagaraScriptToolkit()
{
}


void FNiagaraScriptToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraEditor", "Niagara"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	InTabManager->RegisterTabSpawner(NodeGraphTabId, FOnSpawnTab::CreateSP(this, &FNiagaraScriptToolkit::SpawnTabNodeGraph))
		.SetDisplayName( LOCTEXT("NodeGraph", "Node Graph") )
		.SetGroup(WorkspaceMenuCategoryRef); 

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FNiagaraScriptToolkit::SpawnTabNodeDetails))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FNiagaraScriptToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(NodeGraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

void FNiagaraScriptToolkit::Initialize( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraScript* InputScript )
{	
	check(InputScript != NULL);
	OriginalNiagaraScript = InputScript;
	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	GetTransientPackage()->LinkerCustomVersion.Empty();
	EditedNiagaraScript = (UNiagaraScript*)StaticDuplicateObject(OriginalNiagaraScript, GetTransientPackage(), NAME_None, ~RF_Standalone, UNiagaraScript::StaticClass());

	ScriptViewModel = MakeShareable(new FNiagaraScriptViewModel(EditedNiagaraScript, LOCTEXT("NiagaraScriptDisplayName", "Niagara Script"), ENiagaraParameterEditMode::EditAll));
	DetailsSelection = MakeShareable(new FNiagaraObjectSelection());
	DetailsSelection->SetSelectedObject(EditedNiagaraScript);
		
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_Niagara_Layout_v6" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab) 
			->SetHideTabWell( true )
		)
		->Split
		(
			FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(DetailsTabId, ETabState::OpenedTab)
				->SetForegroundTab(DetailsTabId)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->AddTab(NodeGraphTabId, ETabState::OpenedTab)
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InputScript );

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>( "NiagaraEditor" );
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// @todo toolkit world centric editing
	/*// Setup our tool's layout
	if( IsWorldCentricAssetEditor() )
	{
		const FString TabInitializationPayload(TEXT(""));		// NOTE: Payload not currently used for table properties
		SpawnToolkitTab( NodeGraphTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/
}

FName FNiagaraScriptToolkit::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraScriptToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraScriptToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraScriptToolkit::GetWorldCentricTabColorScale() const
{
	return FNiagaraEditorModule::WorldCentricTabColorScale;
}

TSharedRef<SDockTab> FNiagaraScriptToolkit::SpawnTabNodeGraph( const FSpawnTabArgs& Args )
{
	checkf(Args.GetTabId().TabType == NodeGraphTabId, TEXT("Wrong tab ID in NiagaraScriptToolkit"));
	checkf(ScriptViewModel.IsValid(), TEXT("NiagaraScriptToolkit - Script editor view model is invalid"));

	return
		SNew(SDockTab)
		[
			SNew(SNiagaraScriptGraph, ScriptViewModel->GetGraphViewModel())
			.GraphTitle(LOCTEXT("SpawnGraphTitle", "Script"))
		];
}

void FNiagaraScriptToolkit::OnDetailsSelectionPropertyFinishedChanging(const FPropertyChangedEvent& InEvent)
{
	// We need to synchronize the Usage field in the property editor with the actual node
	// in the graph.
	if (InEvent.Property != nullptr && InEvent.Property->GetName().Equals(TEXT("Usage")))
	{
		if (EditedNiagaraScript != nullptr && EditedNiagaraScript->GetSource() != nullptr)
		{
			UNiagaraScriptSource* ScriptSource = Cast<UNiagaraScriptSource>(EditedNiagaraScript->GetSource());
			if (ScriptSource != nullptr)
			{
				TArray<UNiagaraNodeOutput*> OutputNodes;
				ScriptSource->NodeGraph->FindOutputNodes(OutputNodes);

				bool bChanged = false;
				for (UNiagaraNodeOutput* Output : OutputNodes)
				{
					if (Output->GetUsage() != EditedNiagaraScript->GetUsage())
					{
						Output->Modify();
						Output->SetUsage(EditedNiagaraScript->GetUsage());
						bChanged = true;
					}
				}

				if (bChanged)
				{
					ScriptSource->NodeGraph->NotifyGraphChanged();
				}
			}
		}
	}
}

TSharedRef<SDockTab> FNiagaraScriptToolkit::SpawnTabNodeDetails(const FSpawnTabArgs& Args)
{
	checkf(Args.GetTabId().TabType == DetailsTabId, TEXT("Wrong tab ID in NiagaraScriptToolkit"));
	checkf(ScriptViewModel.IsValid(), TEXT("NiagaraScriptToolkit - Script editor view model is invalid"));

	IConsoleVariable* DevDetailsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fx.DevDetailsPanels"));

	TWeakPtr<FNiagaraScriptViewModel> ScriptViewModelWeakPtr = ScriptViewModel;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, true, FDetailsViewArgs::HideNameArea, true);
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsView->OnFinishedChangingProperties().AddRaw(this, &FNiagaraScriptToolkit::OnDetailsSelectionPropertyFinishedChanging);
	DetailsView->RegisterInstancedCustomPropertyLayout(
		UNiagaraScript::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraScriptDetails::MakeInstance, ScriptViewModelWeakPtr)
	);

	DetailsView->SetObjects(DetailsSelection->GetSelectedObjects().Array());

	if (DevDetailsCVar && DevDetailsCVar->GetInt() != 0)
	{
		return SNew(SDockTab)
			.Label(LOCTEXT("ScriptNodeDetailsTabLabel", "Details"))
			.TabColorScale(GetTabColorScale())
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(0, 3, 0, 0)
				[
					DetailsView
				]
				+ SScrollBox::Slot()
				.Padding(0, 3, 0, 0)
				[
					SNew(SNiagaraSelectedObjectsDetails, ScriptViewModel->GetGraphViewModel()->GetSelection())
				]
			];
	}
	else
	{
		return SNew(SDockTab)
			.Label(LOCTEXT("ScriptNodeDetailsTabLabel", "Details"))
			.TabColorScale(GetTabColorScale())
			[
				DetailsView
			];
	}
}

void FNiagaraScriptToolkit::SetupCommands()
{
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().Apply,
		FExecuteAction::CreateSP(this, &FNiagaraScriptToolkit::OnApply),
		FCanExecuteAction::CreateSP(this, &FNiagaraScriptToolkit::OnApplyEnabled));
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().Compile,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptToolkit::CompileScript));
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().RefreshNodes,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptToolkit::RefreshNodes));
}

void FNiagaraScriptToolkit::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FNiagaraScriptToolkit* ScriptToolkit)
		{
			ToolbarBuilder.BeginSection("Apply");
			ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().Apply,
				NAME_None, TAttribute<FText>(), TAttribute<FText>(), 
				FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Apply"),
				FName(TEXT("ApplyNiagaraScript")));
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Compile");
			ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().Compile,
				NAME_None,
				TAttribute<FText>(),
				TAttribute<FText>(ScriptToolkit, &FNiagaraScriptToolkit::GetCompileStatusTooltip),
				TAttribute<FSlateIcon>(ScriptToolkit, &FNiagaraScriptToolkit::GetCompileStatusImage),
				FName(TEXT("CompileNiagaraScript")));
			ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().RefreshNodes,
				NAME_None,
				TAttribute<FText>(),
				TAttribute<FText>(ScriptToolkit, &FNiagaraScriptToolkit::GetRefreshStatusTooltip),
				TAttribute<FSlateIcon>(ScriptToolkit, &FNiagaraScriptToolkit::GetRefreshStatusImage),
				FName(TEXT("RefreshScriptReferences")));
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this));

	AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>( "NiagaraEditor" );
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

FSlateIcon FNiagaraScriptToolkit::GetCompileStatusImage() const
{
	ENiagaraScriptCompileStatus Status = ScriptViewModel->GetLatestCompileStatus();

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

FText FNiagaraScriptToolkit::GetCompileStatusTooltip() const
{
	ENiagaraScriptCompileStatus Status = ScriptViewModel->GetLatestCompileStatus();
	return FNiagaraEditorUtilities::StatusToText(Status);
}

FSlateIcon FNiagaraScriptToolkit::GetRefreshStatusImage() const
{
	return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.Asset.ReimportAsset.Default");
}

FText FNiagaraScriptToolkit::GetRefreshStatusTooltip() const
{
	return LOCTEXT("Refresh_Status", "Currently dependencies up-to-date. Consider refreshing if status isn't accurate.");
}

void FNiagaraScriptToolkit::CompileScript()
{
	ScriptViewModel->CompileStandaloneScript();
}

void FNiagaraScriptToolkit::RefreshNodes()
{
	ScriptViewModel->RefreshNodes();
}

void FNiagaraScriptToolkit::OnApply()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("Applying Niagara Script %s"), *GetEditingObjects()[0]->GetName());

	UpdateOriginalNiagaraScript();
}

bool FNiagaraScriptToolkit::OnApplyEnabled() const
{
	return ScriptViewModel->GetScriptDirty();
}

void FNiagaraScriptToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(OriginalNiagaraScript);
	Collector.AddReferencedObject(EditedNiagaraScript);
}



void FNiagaraScriptToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	OutObjects.Add(OriginalNiagaraScript);
}


void FNiagaraScriptToolkit::SaveAsset_Execute()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraScript %s"), *GetEditingObjects()[0]->GetName());

	UpdateOriginalNiagaraScript();

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FNiagaraScriptToolkit::SaveAssetAs_Execute()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraScript %s"), *GetEditingObjects()[0]->GetName());

	UpdateOriginalNiagaraScript();

	FAssetEditorToolkit::SaveAssetAs_Execute();
}

void FNiagaraScriptToolkit::UpdateOriginalNiagaraScript()
{
	const FScopedBusyCursor BusyCursor;

	const FText LocalizedScriptEditorApply = NSLOCTEXT("UnrealEd", "ToolTip_NiagaraScriptEditorApply", "Apply changes to original script and its use in the world.");
	GWarn->BeginSlowTask(LocalizedScriptEditorApply, true);
	GWarn->StatusUpdate(1, 1, LocalizedScriptEditorApply);

	if (OriginalNiagaraScript->IsSelected())
	{
		GEditor->GetSelectedObjects()->Deselect(OriginalNiagaraScript);
	}

	ResetLoaders(OriginalNiagaraScript->GetOutermost()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	OriginalNiagaraScript->GetOutermost()->LinkerCustomVersion.Empty();

	// overwrite the original script in place by constructing a new one with the same name
	OriginalNiagaraScript = (UNiagaraScript*)StaticDuplicateObject(EditedNiagaraScript, OriginalNiagaraScript->GetOuter(), OriginalNiagaraScript->GetFName(),
		RF_AllFlags,
		OriginalNiagaraScript->GetClass());

	// Restore RF_Standalone on the original material, as it had been removed from the preview material so that it could be GC'd.
	OriginalNiagaraScript->SetFlags(RF_Standalone);

	// Now there might be other Scripts with functions that referenced this script. So let's update them. They'll need a recompile.
	// Note that we don't discriminate between the version that are open in transient packages (likely duplicates for editing) and the
	// original in-scene versions.
	TArray<UNiagaraScript*> AffectedScripts;
	UNiagaraGraph* OriginalGraph = CastChecked<UNiagaraScriptSource>(OriginalNiagaraScript->GetSource())->NodeGraph;
	for (TObjectIterator<UNiagaraScript> It; It; ++It)
	{
		if (*It == OriginalNiagaraScript || It->IsPendingKillOrUnreachable())
		{
			continue;
		}

		// First see if it is directly called, as this will force a need to refresh from external changes...
		UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(It->GetSource());
		TArray<UNiagaraNode*> NiagaraNodes;
		Source->NodeGraph->GetNodesOfClass<UNiagaraNode>(NiagaraNodes);
		bool bRefreshed = false;
		for (UNiagaraNode* NiagaraNode : NiagaraNodes)
		{
			UObject* ReferencedAsset = NiagaraNode->GetReferencedAsset();
			if (ReferencedAsset == OriginalNiagaraScript)
			{
				NiagaraNode->RefreshFromExternalChanges();
				bRefreshed = true;
			}
		}

		if (bRefreshed)
		{
			Source->NodeGraph->NotifyGraphNeedsRecompile();
			AffectedScripts.Add(*It);
		}
		else
		{
			// Now check to see if our graph is anywhere in the dependency chain for a given graph. If it is, 
			// then it will need to be recompiled against the latest version.
			TArray<const UNiagaraGraph*> ReferencedGraphs;
			Source->NodeGraph->GetAllReferencedGraphs(ReferencedGraphs);
			for (const UNiagaraGraph* Graph : ReferencedGraphs)
			{
				if (Graph == OriginalGraph)
				{
					Source->NodeGraph->NotifyGraphNeedsRecompile();
					AffectedScripts.Add(*It);
					break;
				}
			}
		}
	}

	// Now determine if any of these scripts were in Emitters. If so, those emitters should be compiled together. If not, go ahead and compile individually.
	// Use the existing view models if they exist,as they are already wired into the correct UI.
	TArray<UNiagaraEmitter*> AffectedEmitters;
	for (UNiagaraScript* Script : AffectedScripts)
	{
		if (Script->IsParticleUpdateScript() || Script->IsParticleSpawnScript() || Script->IsParticleEventScript())
		{
			UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(Script->GetOuter());
			if (Emitter)
			{
				AffectedEmitters.AddUnique(Emitter);
			}
		}
		else
		{
			TSharedPtr<FNiagaraScriptViewModel> AffectedScriptViewModel = FNiagaraScriptViewModel::GetExistingViewModelForObject(Script);
			if (!AffectedScriptViewModel.IsValid())
			{
				AffectedScriptViewModel = MakeShareable(new FNiagaraScriptViewModel(Script, FText::FromString(Script->GetName()), ENiagaraParameterEditMode::EditValueOnly));
			}
			AffectedScriptViewModel->CompileStandaloneScript();
		}
	}

	FNiagaraEditorUtilities::UpdateExistingEmitters(AffectedEmitters);

	GWarn->EndSlowTask();
	ScriptViewModel->SetScriptDirty(false);
}


bool FNiagaraScriptToolkit::OnRequestClose()
{
	if (ScriptViewModel->GetScriptDirty())
	{
		// find out the user wants to do with this dirty NiagaraScript
		EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(
				NSLOCTEXT("UnrealEd", "Prompt_NiagaraScriptEditorClose", "Would you like to apply changes to this NiagaraScript to the original NiagaraScript?\n{0}\n(No will lose all changes!)"),
				FText::FromString(OriginalNiagaraScript->GetPathName())));

		// act on it
		switch (YesNoCancelReply)
		{
		case EAppReturnType::Yes:
			// update NiagaraScript and exit
			UpdateOriginalNiagaraScript();
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

#undef LOCTEXT_NAMESPACE
