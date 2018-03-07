// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorModule.h"
#include "PropertyEditorModule.h"
#include "ControlRigVariableDetailsCustomization.h"
#include "BlueprintEditorModule.h"
#include "KismetEditorUtilities.h"
#include "ControlRig.h"
#include "K2Node_ControlRig.h"
#include "K2Node_ControlRigOutput.h"
#include "ControlRigInputOutputDetailsCustomization.h"
#include "UserLabeledFieldCustomization.h"
#include "ControlRigComponent.h"
#include "ISequencerModule.h"
#include "ControlRigTrackEditor.h"
#include "IAssetTools.h"
#include "ControlRigSequenceActions.h"
#include "ControlRigSequenceEditorStyle.h"
#include "LayoutExtender.h"
#include "LevelEditor.h"
#include "MovieSceneToolsProjectSettings.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "WorkflowTabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ControlRigSequence.h"
#include "EditorModeRegistry.h"
#include "ControlRigEditMode.h"
#include "HumanRig.h"
#include "HumanRigDetails.h"
#include "ControlRigEditorObjectBinding.h"
#include "ControlRigEditorObjectSpawner.h"
#include "ILevelSequenceModule.h"
#include "ControlRigBindingTrackEditor.h"
#include "EditorModeManager.h"
#include "ControlRigEditMode.h"
#include "MovieSceneControlRigSection.h"
#include "MovieSceneControlRigSectionDetailsCustomization.h"
#include "ControlRigCommands.h"
#include "Materials/Material.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigEditModeSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigSequenceExporter.h"
#include "ControlRigSequenceExporterSettingsDetailsCustomization.h"
#include "ControlRigSequenceExporterSettings.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorModule"

void FControlRigEditorModule::StartupModule()
{
	FHumanRigNodeCommand::Register();
	FControlRigCommands::Register();
	FControlRigSequenceEditorStyle::Get();

	CommandBindings = MakeShareable(new FUICommandList());

	BindCommands();

	// Register Blueprint editor variable customization
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	BlueprintEditorModule.RegisterVariableCustomization(UProperty::StaticClass(), FOnGetVariableCustomizationInstance::CreateStatic(&FControlRigVariableDetailsCustomization::MakeInstance));

	// Register to fixup newly created BPs
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UControlRig::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FControlRigEditorModule::HandleNewBlueprintCreated));

	// Register details customizations for animation controller nodes
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(UK2Node_ControlRig::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigInputOutputDetailsCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout(FUserLabeledField::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FUserLabeledFieldCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(UHumanRig::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FHumanRigDetails::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(UMovieSceneControlRigSection::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneControlRigSectionDetailsCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(UControlRigSequenceExporterSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigSequenceExporterSettingsDetailsCustomization::MakeInstance));

	// Register blueprint compiler
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&ControlRigBlueprintCompiler);

	// Register asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedRef<IAssetTypeActions> AssetTypeAction = MakeShareable(new FControlRigSequenceActions());
	RegisteredAssetTypeActions.Add(AssetTypeAction);
	AssetTools.RegisterAssetTypeActions(AssetTypeAction);

	// Register sequencer track editor
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateRaw(this, &FControlRigEditorModule::HandleSequencerCreated));
	ControlRigTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FControlRigTrackEditor::CreateTrackEditor));
	ControlRigBindingTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FControlRigBindingTrackEditor::CreateTrackEditor));
	ControlRigEditorObjectBindingHandle = SequencerModule.RegisterEditorObjectBinding(FOnCreateEditorObjectBinding::CreateStatic(&FControlRigEditorObjectBinding::CreateEditorObjectBinding));

	SequencerToolbarExtender = MakeShareable(new FExtender());
	SequencerToolbarExtender->AddToolBarExtension(
		"Level Sequence Separator",
		EExtensionHook::Before,
		CommandBindings,
		FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& ToolBarBuilder)
		{
			ToolBarBuilder.AddToolBarButton(FControlRigCommands::Get().ExportAnimSequence);
		}));

	SequencerModule.GetToolBarExtensibilityManager()->AddExtender(SequencerToolbarExtender);

	// Register for assets being opened
	AssetEditorOpenedHandle = FAssetEditorManager::Get().OnAssetEditorOpened().AddRaw(this, &FControlRigEditorModule::HandleAssetEditorOpened);

	// Register level sequence spawner
	ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
	LevelSequenceSpawnerDelegateHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FControlRigEditorObjectSpawner::CreateObjectSpawner));

	TrajectoryMaterial = LoadObject<UMaterial>(nullptr, TEXT("/ControlRig/M_Traj.M_Traj"));
	if (TrajectoryMaterial.IsValid())
	{
		TrajectoryMaterial->AddToRoot();
	}

	FEditorModeRegistry::Get().RegisterMode<FControlRigEditMode>(
		FControlRigEditMode::ModeName,
		NSLOCTEXT("AnimationModeToolkit", "DisplayName", "Animation"),
		FSlateIcon(FControlRigSequenceEditorStyle::Get().GetStyleSetName(), "ControlRigEditMode", "ControlRigEditMode.Small"),
		true);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda([this](const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		if (SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData) { return AssetData.GetClass() == UAnimSequence::StaticClass(); }))
		{
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				CommandBindings,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					const TSharedPtr<FUICommandInfo>& ImportFromRigSequence = FControlRigCommands::Get().ImportFromRigSequence;
					MenuBuilder.AddMenuEntry(
						ImportFromRigSequence->GetLabel(),
						ImportFromRigSequence->GetDescription(),
						ImportFromRigSequence->GetIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ImportFromRigSequence, SelectedAssets)));
				}));

			// only add this if we find a control rig sequence targeting this anim sequence in the asset registry
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			
			bool bCanReimport = false;
			for(const FAssetData& AssetData : SelectedAssets)
			{
				TMultiMap<FName, FString> TagsAndValues;
				TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UControlRigSequence, LastExportedToAnimationSequence), AssetData.ObjectPath.ToString());

				TArray<FAssetData> FoundAssets;
				AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, FoundAssets);

				if (FoundAssets.Num() > 0)
				{
					bCanReimport = true;
					break;
				}
			}

			if (bCanReimport)
			{
				Extender->AddMenuExtension(
					"GetAssetActions",
					EExtensionHook::After,
					CommandBindings,
					FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
					{
						const TSharedPtr<FUICommandInfo>& ReImportFromRigSequence = FControlRigCommands::Get().ReImportFromRigSequence;
						MenuBuilder.AddMenuEntry(
							ReImportFromRigSequence->GetLabel(),
							ReImportFromRigSequence->GetDescription(),
							ReImportFromRigSequence->GetIcon(),
							FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ReImportFromRigSequence, SelectedAssets)));
					}));
			}
		}
		else if (SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData) { return AssetData.GetClass() == UControlRigSequence::StaticClass(); }))
		{
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::Before,
				CommandBindings,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					MenuBuilder.BeginSection("ControlRigActions", LOCTEXT("ControlRigActions", "Control Rig Sequence Actions"));
					{
						const TSharedPtr<FUICommandInfo>& ExportAnimSequence = FControlRigCommands::Get().ExportAnimSequence;
						MenuBuilder.AddMenuEntry(
							ExportAnimSequence->GetLabel(),
							ExportAnimSequence->GetDescription(),
							ExportAnimSequence->GetIcon(),
							FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ExportToAnimSequence, SelectedAssets)));

						bool bCanReExport = false;
						for (const FAssetData& AssetData : SelectedAssets)
						{
							if(UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset()))
							{
								if (ControlRigSequence->LastExportedToAnimationSequence.IsValid())
								{
									bCanReExport = true;
									break;
								}
							}
						}

						if (bCanReExport)
						{
							const TSharedPtr<FUICommandInfo>& ReExportAnimSequence = FControlRigCommands::Get().ReExportAnimSequence;
							MenuBuilder.AddMenuEntry(
								ReExportAnimSequence->GetLabel(),
								ReExportAnimSequence->GetDescription(),
								ReExportAnimSequence->GetIcon(),
								FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ReExportToAnimSequence, SelectedAssets)));
						}
					}
					MenuBuilder.EndSection();
				}));
		}
	
		return Extender;
	}));
	ContentBrowserMenuExtenderHandle = ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Last().GetHandle();
}

void FControlRigEditorModule::ShutdownModule()
{
	FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (ContentBrowserModule)
	{
		ContentBrowserModule->GetAllAssetViewContextMenuExtenders().RemoveAll([=](const FContentBrowserMenuExtender_SelectedAssets& InDelegate) { return ContentBrowserMenuExtenderHandle == InDelegate.GetHandle(); });
	}

	if (TrajectoryMaterial.IsValid())
	{
		TrajectoryMaterial->RemoveFromRoot();
	}

	FAssetEditorManager::Get().OnAssetEditorOpened().Remove(AssetEditorOpenedHandle);

	FEditorModeRegistry::Get().UnregisterMode(FControlRigEditMode::ModeName);

	ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
	if (LevelSequenceModule)
	{
		LevelSequenceModule->UnregisterObjectSpawner(LevelSequenceSpawnerDelegateHandle);
	}

	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule)
	{
		SequencerModule->UnregisterOnSequencerCreated(SequencerCreatedHandle);
		SequencerModule->UnRegisterTrackEditor(ControlRigTrackCreateEditorHandle);
		SequencerModule->UnRegisterTrackEditor(ControlRigBindingTrackCreateEditorHandle);
		SequencerModule->UnRegisterEditorObjectBinding(ControlRigEditorObjectBindingHandle);

		SequencerModule->GetToolBarExtensibilityManager()->RemoveExtender(SequencerToolbarExtender);
		SequencerToolbarExtender = nullptr;
	}

	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		for (TSharedRef<IAssetTypeActions> RegisteredAssetTypeAction : RegisteredAssetTypeActions)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(RegisteredAssetTypeAction);
		}
	}

	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("Kismet");
	if (BlueprintEditorModule)
	{
		BlueprintEditorModule->UnregisterVariableCustomization(UProperty::StaticClass());
	}

	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomClassLayout(UK2Node_ControlRig::StaticClass()->GetFName());
		PropertyEditorModule->UnregisterCustomClassLayout(FUserLabeledField::StaticStruct()->GetFName());
		PropertyEditorModule->UnregisterCustomClassLayout(UHumanRig::StaticClass()->GetFName());
		PropertyEditorModule->UnregisterCustomClassLayout(UMovieSceneControlRigSection::StaticClass()->GetFName());
	}

	IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler");
	if (KismetCompilerModule)
	{
		KismetCompilerModule->GetCompilers().Remove(&ControlRigBlueprintCompiler);
	}

	CommandBindings = nullptr;
}

void FControlRigEditorModule::HandleNewBlueprintCreated(UBlueprint* InBlueprint)
{
	if (ensure(InBlueprint->UbergraphPages.Num() > 0))
	{
		UEdGraph* EventGraph = InBlueprint->UbergraphPages[0];

		// Add animation output node
		UK2Node_ControlRigOutput* OutputNode = NewObject<UK2Node_ControlRigOutput>(EventGraph);
		OutputNode->CreateNewGuid();
		OutputNode->PostPlacedNewNode();
		OutputNode->SetFlags(RF_Transactional);
		OutputNode->AllocateDefaultPins();
		OutputNode->ReconstructNode();
		OutputNode->NodePosX = 0;
		OutputNode->NodePosY = 0;
		UEdGraphSchema_K2::SetNodeMetaData(OutputNode, FNodeMetadata::DefaultGraphNode);
		OutputNode->MakeAutomaticallyPlacedGhostNode();
		OutputNode->NodeComment = LOCTEXT("AnimationOutputComment", "This node acts as the output for this animation controller.\nTo add or remove an output pin, enable or disable the \"Animation Output\" checkbox for a variable.").ToString();
		OutputNode->bCommentBubbleVisible = true;
		OutputNode->bCommentBubblePinned = true;

		EventGraph->AddNode(OutputNode);
	}
}

void FControlRigEditorModule::HandleSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	TWeakPtr<ISequencer> LocalSequencer = InSequencer;

	// Record the last sequencer we opened that was editing a control rig sequence
	UMovieSceneSequence* FocusedSequence = InSequencer->GetFocusedMovieSceneSequence();
	if (UControlRigSequence* FocusedControlRigSequence = ExactCast<UControlRigSequence>(FocusedSequence))
	{
		WeakSequencer = InSequencer;
	}

	// We want to be informed of sequence activations (subsequences or not)
	auto HandleActivateSequence = [this, LocalSequencer](FMovieSceneSequenceIDRef Ref)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				WeakSequencer = LocalSequencer;

				GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);

				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->SetSequencer(Sequencer);
				}
			}
			else
			{
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->SetSequencer(nullptr);
					TArray<TWeakObjectPtr<>> SelectedObjects;
					TArray<FGuid> ObjectBindings;
					ControlRigEditMode->SetObjects(SelectedObjects, ObjectBindings);
				}
			}
		}
	};

	InSequencer->OnActivateSequence().AddLambda(HandleActivateSequence);

	// Call into activation callback to handle initial activation
	FMovieSceneSequenceID SequenceID = MovieSceneSequenceID::Root;
	HandleActivateSequence(SequenceID);

	InSequencer->GetSelectionChangedObjectGuids().AddLambda([LocalSequencer](TArray<FGuid> InObjectBindings)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				TArray<TWeakObjectPtr<>> SelectedObjects;

				// first make a list of unique objects
				TArray<FGuid> UniqueBindings;
				for (const FGuid& Guid : InObjectBindings)
				{
					UniqueBindings.AddUnique(Guid);
				}

				for (const FGuid& Guid : UniqueBindings)
				{
					TArrayView<TWeakObjectPtr<>> BoundObjects = Sequencer->FindBoundObjects(Guid, Sequencer->GetFocusedTemplateID());
					SelectedObjects.Append(BoundObjects.GetData(), BoundObjects.Num());
				}

				// @TODO: allow binding to sub-editor mode tools for use in animation editors
				if (SelectedObjects.Num() > 0)
				{
					GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
					if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
					{
						ControlRigEditMode->SetObjects(SelectedObjects, UniqueBindings);
					}
				}
			}
		}
	});

	InSequencer->OnMovieSceneDataChanged().AddLambda([LocalSequencer](EMovieSceneDataChangeType DataChangeType)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->RefreshObjects();
					ControlRigEditMode->RefreshTrajectoryCache();
				}
			}
		}
	});

	InSequencer->GetSelectionChangedTracks().AddLambda([LocalSequencer](TArray<UMovieSceneTrack*> InTracks)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				TArray<FString> PropertyPaths;

				// Look for any property tracks that might drive our rig manipulators
				for (UMovieSceneTrack* Track : InTracks)
				{
					if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track))
					{
						PropertyPaths.Add(PropertyTrack->GetPropertyPath());
					}
				}

				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->SetNodeSelectionByPropertyPath(PropertyPaths);
				}
			}
		}
	});

	InSequencer->OnPostSave().AddLambda([](ISequencer& InSequencerThatSaved)
	{
		UMovieSceneSequence* Sequence = InSequencerThatSaved.GetFocusedMovieSceneSequence();
		if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
		{
			if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
			{
				ControlRigEditMode->ReBindToActor();
			}
		}
	});
}

void FControlRigEditorModule::HandleAssetEditorOpened(UObject* InAsset)
{
	if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(InAsset))
	{
		GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);

		if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
		{
			ControlRigEditMode->ReBindToActor();
		}
	}
}

void FControlRigEditorModule::OnInitializeSequence(UControlRigSequence* Sequence)
{
	auto* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	Sequence->GetMovieScene()->SetPlaybackRange(ProjectSettings->DefaultStartTime, ProjectSettings->DefaultStartTime + ProjectSettings->DefaultDuration);
}

void FControlRigEditorModule::BindCommands()
{
	const FControlRigCommands& Commands = FControlRigCommands::Get();

	CommandBindings->MapAction(
		Commands.ExportAnimSequence,
		FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ExportAnimSequenceFromSequencer),
		FCanExecuteAction(),
		FGetActionCheckState(), 
		FIsActionButtonVisible::CreateRaw(this, &FControlRigEditorModule::CanExportAnimSequenceFromSequencer));
}

bool FControlRigEditorModule::CanExportAnimSequenceFromSequencer() const
{
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		return ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence()) != nullptr;
	}

	return false;
}

void FControlRigEditorModule::ExportAnimSequenceFromSequencer()
{
	// if we have an active sequencer, get the sequence
	UControlRigSequence* ControlRigSequence = nullptr;
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		ControlRigSequence = ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence());
	}

	// If we are bound to an actor in the edit mode, auto pick skeletal mesh to use for binding
	USkeletalMesh* SkeletalMesh = nullptr;
	if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
	{
		TLazyObjectPtr<AActor> ActorPtr = ControlRigEditMode->GetSettings()->Actor;
		if (ActorPtr.IsValid())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = ActorPtr->FindComponentByClass<USkeletalMeshComponent>())
			{
				SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
			}
		}
	}

	if (ControlRigSequence)
	{
		ControlRigSequenceConverter::Convert(ControlRigSequence, nullptr, SkeletalMesh);
	}
}

void FControlRigEditorModule::ExportToAnimSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset());
		if (ControlRigSequence)
		{
			ControlRigSequenceConverter::Convert(ControlRigSequence, nullptr, nullptr);
		}
	}
}

void FControlRigEditorModule::ReExportToAnimSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset());
		if (ControlRigSequence)
		{
			UAnimSequence* AnimSequence = ControlRigSequence->LastExportedToAnimationSequence.LoadSynchronous();
			USkeletalMesh* SkeletalMesh = ControlRigSequence->LastExportedUsingSkeletalMesh.LoadSynchronous();
			bool bShowDialog = AnimSequence == nullptr || SkeletalMesh == nullptr;

			ControlRigSequenceConverter::Convert(ControlRigSequence, AnimSequence, SkeletalMesh, bShowDialog);
		}
	}
}

void FControlRigEditorModule::ImportFromRigSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		if (AnimSequence)
		{
			ControlRigSequenceConverter::Convert(nullptr, AnimSequence, nullptr);
		}
	}
}

void FControlRigEditorModule::ReImportFromRigSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		USkeletalMesh* SkeletalMesh = nullptr;
		UControlRigSequence* ControlRigSequence = nullptr;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		TMultiMap<FName, FString> TagsAndValues;
		TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UControlRigSequence, LastExportedToAnimationSequence), AssetData.ObjectPath.ToString());

		TArray<FAssetData> FoundAssets;
		AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, FoundAssets);

		if (FoundAssets.Num() > 0)
		{
			ControlRigSequence = Cast<UControlRigSequence>(FoundAssets[0].GetAsset());
			if (ControlRigSequence)
			{
				SkeletalMesh = ControlRigSequence->LastExportedUsingSkeletalMesh.LoadSynchronous();
			}
		}

		bool bShowDialog = ControlRigSequence == nullptr || AnimSequence == nullptr || SkeletalMesh == nullptr;

		ControlRigSequenceConverter::Convert(ControlRigSequence, AnimSequence, SkeletalMesh, bShowDialog);
	}
}

IMPLEMENT_MODULE(FControlRigEditorModule, ControlRigEditor)

#undef LOCTEXT_NAMESPACE
