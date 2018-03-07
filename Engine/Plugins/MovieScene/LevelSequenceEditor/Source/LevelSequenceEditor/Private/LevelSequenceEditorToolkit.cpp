// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorToolkit.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "EngineGlobals.h"
#include "AssetData.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Containers/ArrayBuilder.h"
#include "Modules/ModuleManager.h"
#include "KeyParams.h"
#include "MovieSceneSequence.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/Selection.h"
#include "LevelSequenceEditorModule.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/LevelSequenceEditorSettings.h"
#include "Misc/LevelSequenceEditorSpawnRegister.h"
#include "Misc/LevelSequenceEditorHelpers.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "CineCameraActor.h"
#include "Styling/SlateIconFinder.h"
#include "KeyPropertyParams.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneToolsProjectSettings.h"
#include "MovieSceneToolHelpers.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "Widgets/Docking/SDockTab.h"
#include "SequencerSettings.h"
#include "LevelEditorSequencerIntegration.h"
#include "MovieSceneCaptureDialogModule.h"
#include "MovieScene.h"
#include "UnrealEdMisc.h"

// @todo sequencer: hack: setting defaults for transform tracks

#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"

// To override Sequencer editor behavior for VR Editor 
#include "EditorWorldExtension.h"
#include "VREditorMode.h"


#define LOCTEXT_NAMESPACE "LevelSequenceEditor"


/* Local constants
 *****************************************************************************/

const FName FLevelSequenceEditorToolkit::SequencerMainTabId(TEXT("Sequencer_SequencerMain"));

namespace SequencerDefs
{
	static const FName SequencerAppIdentifier(TEXT("SequencerApp"));
}

// Defer to ULevelSequencePlayer's implementation for getting event contexts from the current world
TArray<UObject*> GetLevelSequenceEditorEventContexts()
{
	TArray<UObject*> Contexts;

	// Return PIE worlds if there are any
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			ULevelSequencePlayer::GetEventContexts(*Context.World(), Contexts);
		}
	}

	if (Contexts.Num())
	{
		return Contexts;
	}

	// Else just return the editor world
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			ULevelSequencePlayer::GetEventContexts(*Context.World(), Contexts);
			break;
		}
	}

	return Contexts;
}

UObject* GetLevelSequenceEditorPlaybackContext()
{
	UWorld* PIEWorld = nullptr;
	UWorld* EditorWorld = nullptr;

	IMovieSceneCaptureDialogModule* CaptureDialogModule = FModuleManager::GetModulePtr<IMovieSceneCaptureDialogModule>("MovieSceneCaptureDialog");
	UWorld* RecordingWorld = CaptureDialogModule ? CaptureDialogModule->GetCurrentlyRecordingWorld() : nullptr;

	bool bIsSimulatingInEditor = GEditor && GEditor->bIsSimulatingInEditor;
	bool bUsePIEWorld = (!bIsSimulatingInEditor && GetDefault<ULevelEditorPlaySettings>()->bBindSequencerToPIE)
					|| (bIsSimulatingInEditor && GetDefault<ULevelEditorPlaySettings>()->bBindSequencerToSimulate);

	// Return PIE worlds if there are any
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			UWorld* ThisWorld = Context.World();
			if (bUsePIEWorld && RecordingWorld != ThisWorld)
			{
				PIEWorld = ThisWorld;
			}
		}
		else if (Context.WorldType == EWorldType::Editor)
		{
			// We can always animate PIE worlds
			EditorWorld = Context.World();
			if (!bUsePIEWorld)
			{
				return EditorWorld;
			}
		}
	}

	return PIEWorld ? PIEWorld : EditorWorld;
}

static TArray<FLevelSequenceEditorToolkit*> OpenToolkits;

void FLevelSequenceEditorToolkit::IterateOpenToolkits(TFunctionRef<bool(FLevelSequenceEditorToolkit&)> Iter)
{
	for (FLevelSequenceEditorToolkit* Toolkit : OpenToolkits)
	{
		if (!Iter(*Toolkit))
		{
			return;
		}
	}
}

FLevelSequenceEditorToolkit::FLevelSequenceEditorToolkitOpened& FLevelSequenceEditorToolkit::OnOpened()
{
	static FLevelSequenceEditorToolkitOpened OnOpenedEvent;
	return OnOpenedEvent;
}

/* FLevelSequenceEditorToolkit structors
 *****************************************************************************/

FLevelSequenceEditorToolkit::FLevelSequenceEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: LevelSequence(nullptr)
	, Style(InStyle)
{
	// register sequencer menu extenders
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	int32 NewIndex = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates().Add(
		FAssetEditorExtender::CreateRaw(this, &FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender));
	SequencerExtenderHandle = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates()[NewIndex].GetHandle();


	OpenToolkits.Add(this);
}


FLevelSequenceEditorToolkit::~FLevelSequenceEditorToolkit()
{
	FLevelEditorSequencerIntegration::Get().RemoveSequencer(Sequencer.ToSharedRef());

	Sequencer->Close();

	// unregister delegates
	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		auto& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelEditorModule.OnMapChanged().RemoveAll(this);
	}

	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelSequenceEditor")))
	{
		auto& LevelSequenceEditorModule = FModuleManager::LoadModuleChecked<ILevelSequenceEditorModule>(TEXT("LevelSequenceEditor"));
		LevelSequenceEditorModule.OnMasterSequenceCreated().RemoveAll(this);
	}

	// unregister sequencer menu extenders
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.GetAddTrackMenuExtensibilityManager()->GetExtenderDelegates().RemoveAll([this](const FAssetEditorExtender& Extender)
	{
		return SequencerExtenderHandle == Extender.GetHandle();
	});
}


/* FLevelSequenceEditorToolkit interface
 *****************************************************************************/

void FLevelSequenceEditorToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, ULevelSequence* InLevelSequence)
{
	// create tab layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_LevelSequenceEditor")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->Split
				(
					FTabManager::NewStack()
						->AddTab(SequencerMainTabId, ETabState::OpenedTab)
				)
		);

	LevelSequence = InLevelSequence;

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = false;

	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, SequencerDefs::SequencerAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, LevelSequence);

	TSharedRef<FLevelSequenceEditorSpawnRegister> SpawnRegister = MakeShareable(new FLevelSequenceEditorSpawnRegister);

	// initialize sequencer
	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.RootSequence = LevelSequence;
		SequencerInitParams.bEditWithinLevelEditor = true;
		SequencerInitParams.ToolkitHost = InitToolkitHost;
		SequencerInitParams.SpawnRegister = SpawnRegister;

		SequencerInitParams.EventContexts.BindStatic(GetLevelSequenceEditorEventContexts);
		SequencerInitParams.PlaybackContext.BindStatic(GetLevelSequenceEditorPlaybackContext);

		SequencerInitParams.ViewParams.UniqueName = "LevelSequenceEditor";
		SequencerInitParams.ViewParams.OnReceivedFocus.BindRaw(this, &FLevelSequenceEditorToolkit::OnSequencerReceivedFocus);
	}

	Sequencer = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer").CreateSequencer(SequencerInitParams);
	SpawnRegister->SetSequencer(Sequencer);
	Sequencer->OnActorAddedToSequencer().AddSP(this, &FLevelSequenceEditorToolkit::HandleActorAddedToSequencer);

	FLevelEditorSequencerIntegrationOptions Options;
	Options.bRequiresLevelEvents = true;
	Options.bRequiresActorEvents = true;
	Options.bCanRecord = true;

	FLevelEditorSequencerIntegration::Get().AddSequencer(Sequencer.ToSharedRef(), Options);

	// @todo remove when world-centric mode is added
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	LevelEditorModule.AttachSequencer(Sequencer->GetSequencerWidget(), SharedThis(this));

	// @todo reopen the scene outliner so that is refreshed with the sequencer info column
	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
	if (LevelEditorTabManager->FindExistingLiveTab(FName("LevelEditorSceneOutliner")).IsValid())
	{
		LevelEditorTabManager->InvokeTab(FName("LevelEditorSceneOutliner"))->RequestCloseTab();
		LevelEditorTabManager->InvokeTab(FName("LevelEditorSceneOutliner"));
	}

	// We need to find out when the user loads a new map, because we might need to re-create puppet actors
	// when previewing a MovieScene
	LevelEditorModule.OnMapChanged().AddRaw(this, &FLevelSequenceEditorToolkit::HandleMapChanged);

	ILevelSequenceEditorModule& LevelSequenceEditorModule = FModuleManager::LoadModuleChecked<ILevelSequenceEditorModule>("LevelSequenceEditor");
	LevelSequenceEditorModule.OnMasterSequenceCreated().AddRaw(this, &FLevelSequenceEditorToolkit::HandleMasterSequenceCreated);

	FLevelSequenceEditorToolkit::OnOpened().Broadcast(*this);

	{
		UWorld* World = CastChecked<UWorld>( GetLevelSequenceEditorPlaybackContext() );
		UVREditorMode* VRMode = Cast<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( World )->FindExtension( UVREditorMode::StaticClass() ) );
		if (VRMode != nullptr)
		{
			VRMode->OnVREditingModeExit_Handler.BindSP(this, &FLevelSequenceEditorToolkit::HandleVREditorModeExit);
			USequencerSettings& SavedSequencerSettings = *Sequencer->GetSequencerSettings();
			VRMode->SaveSequencerSettings(Sequencer->GetKeyAllEnabled(), Sequencer->GetAutoChangeMode(), SavedSequencerSettings);
			// Override currently set auto-change behavior to always autokey
			Sequencer->SetAutoChangeMode(EAutoChangeMode::All);
			Sequencer->SetKeyAllEnabled(true);
			// Tell the VR Editor mode that Sequencer has refreshed
			VRMode->RefreshVREditorSequencer(Sequencer.Get());
		}
	}
}


/* IToolkit interface
 *****************************************************************************/

FText FLevelSequenceEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Level Sequence Editor");
}


FName FLevelSequenceEditorToolkit::GetToolkitFName() const
{
	static FName SequencerName("LevelSequenceEditor");
	return SequencerName;
}


FLinearColor FLevelSequenceEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.7, 0.0f, 0.0f, 0.5f);
}


FString FLevelSequenceEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Sequencer ").ToString();
}


void FLevelSequenceEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	if (IsWorldCentricAssetEditor())
	{
		return;
	}

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_SequencerAssetEditor", "Sequencer"));

	InTabManager->RegisterTabSpawner(SequencerMainTabId, FOnSpawnTab::CreateSP(this, &FLevelSequenceEditorToolkit::HandleTabManagerSpawnTab))
		.SetDisplayName(LOCTEXT("SequencerMainTab", "Sequencer"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "LevelSequenceEditor.Tabs.Sequencer"));
}


void FLevelSequenceEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	if (!IsWorldCentricAssetEditor())
	{
		InTabManager->UnregisterTabSpawner(SequencerMainTabId);
	}

	// @todo remove when world-centric mode is added
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.AttachSequencer(SNullWidget::NullWidget, nullptr);
}


/* FLevelSequenceEditorToolkit implementation
 *****************************************************************************/

void FLevelSequenceEditorToolkit::AddDefaultTracksForActor(AActor& Actor, const FGuid Binding)
{
	// get focused movie scene
	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();

	if (Sequence == nullptr)
	{
		return;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();

	if (MovieScene == nullptr)
	{
		return;
	}

	// add default tracks
	for (const FLevelSequenceTrackSettings& TrackSettings : GetDefault<ULevelSequenceEditorSettings>()->TrackSettings)
	{
		UClass* MatchingActorClass = TrackSettings.MatchingActorClass.ResolveClass();

		if ((MatchingActorClass == nullptr) || !Actor.IsA(MatchingActorClass))
		{
			continue;
		}

		// add tracks by type
		for (const FSoftClassPath& DefaultTrack : TrackSettings.DefaultTracks)
		{
			UClass* TrackClass = DefaultTrack.ResolveClass();

			// exclude any tracks explicitly marked for exclusion
			for (const FLevelSequenceTrackSettings& ExcludeTrackSettings : GetDefault<ULevelSequenceEditorSettings>()->TrackSettings)
			{
				UClass* ExcludeMatchingActorClass = ExcludeTrackSettings.MatchingActorClass.ResolveClass();

				if ((ExcludeMatchingActorClass == nullptr) || !Actor.IsA(ExcludeMatchingActorClass))
				{
					continue;
				}
				
				for (const FSoftClassPath& ExcludeDefaultTrack : ExcludeTrackSettings.ExcludeDefaultTracks)
				{
					if (ExcludeDefaultTrack == DefaultTrack)
					{
						TrackClass = nullptr;
						break;
					}
				}				
			}

			if (TrackClass != nullptr)
			{
				UMovieSceneTrack* NewTrack = MovieScene->FindTrack(TrackClass, Binding);
				if (!NewTrack)
				{
					NewTrack = MovieScene->AddTrack(TrackClass, Binding);
				}

				// Create a section for any property tracks
				if (Cast<UMovieScenePropertyTrack>(NewTrack))
				{
					UMovieSceneSection* NewSection;
					if (NewTrack->GetAllSections().Num() > 0)
					{
						NewSection = NewTrack->GetAllSections()[0];
					}
					else
					{
						NewSection = NewTrack->CreateNewSection();
						NewTrack->AddSection(*NewSection);
					}

					// @todo sequencer: hack: setting defaults for transform tracks
					if (NewTrack->IsA(UMovieScene3DTransformTrack::StaticClass()) && Sequencer->GetAutoSetTrackDefaults())
					{
						auto TransformSection = Cast<UMovieScene3DTransformSection>(NewSection);

						FVector Location = Actor.GetActorLocation();
						FRotator Rotation = Actor.GetActorRotation();
						FVector Scale = Actor.GetActorScale();

						if (Actor.GetRootComponent())
						{
							FTransform ActorRelativeTransform = Actor.GetRootComponent()->GetRelativeTransform();

							Location = ActorRelativeTransform.GetTranslation();
							Rotation = ActorRelativeTransform.GetRotation().Rotator();
							Scale = ActorRelativeTransform.GetScale3D();
						}

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, Location.X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, Location.Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, Location.Z, false /*bUnwindRotation*/));

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::X, Rotation.Euler().X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Y, Rotation.Euler().Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Z, Rotation.Euler().Z, false /*bUnwindRotation*/));

						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::X, Scale.X, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Y, Scale.Y, false /*bUnwindRotation*/));
						TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Z, Scale.Z, false /*bUnwindRotation*/));
					}

					NewSection->SetIsInfinite(GetSequencer()->GetInfiniteKeyAreas());
				}

				Sequencer->UpdateRuntimeInstances();
			}
		}

		// construct a map of the properties that should be excluded per component
		TMap<UObject*, TArray<FString> > ExcludePropertyTracksMap;
		for (const FLevelSequenceTrackSettings& ExcludeTrackSettings : GetDefault<ULevelSequenceEditorSettings>()->TrackSettings)
		{
			UClass* ExcludeMatchingActorClass = ExcludeTrackSettings.MatchingActorClass.ResolveClass();

			if ((ExcludeMatchingActorClass == nullptr) || !Actor.IsA(ExcludeMatchingActorClass))
			{
				continue;
			}

			for (const FLevelSequencePropertyTrackSettings& PropertyTrackSettings : ExcludeTrackSettings.ExcludeDefaultPropertyTracks)
			{
				UObject* PropertyOwner = &Actor;

				// determine object hierarchy
				TArray<FString> ComponentNames;
				PropertyTrackSettings.ComponentPath.ParseIntoArray(ComponentNames, TEXT("."));

				for (const FString& ComponentName : ComponentNames)
				{
					PropertyOwner = FindObjectFast<UObject>(PropertyOwner, *ComponentName);

					if (PropertyOwner == nullptr)
					{
						continue;
					}
				}

				if (PropertyOwner)
				{
					TArray<FString> PropertyNames;
					PropertyTrackSettings.PropertyPath.ParseIntoArray(PropertyNames, TEXT("."));

					ExcludePropertyTracksMap.Add(PropertyOwner, PropertyNames);
				}
			}
		}

		// add tracks by property
		for (const FLevelSequencePropertyTrackSettings& PropertyTrackSettings : TrackSettings.DefaultPropertyTracks)
		{
			TSharedRef<FPropertyPath> PropertyPath = FPropertyPath::CreateEmpty();
			UObject* PropertyOwner = &Actor;

			// determine object hierarchy
			TArray<FString> ComponentNames;
			PropertyTrackSettings.ComponentPath.ParseIntoArray(ComponentNames, TEXT("."));

			for (const FString& ComponentName : ComponentNames)
			{
				PropertyOwner = FindObjectFast<UObject>(PropertyOwner, *ComponentName);

				if (PropertyOwner == nullptr)
				{
					return;
				}
			}

			UStruct* PropertyOwnerClass = PropertyOwner->GetClass();

			// determine property path
			TArray<FString> PropertyNames;
			PropertyTrackSettings.PropertyPath.ParseIntoArray(PropertyNames, TEXT("."));

			for (const FString& PropertyName : PropertyNames)
			{
				// skip past excluded properties
				if (ExcludePropertyTracksMap.Contains(PropertyOwner) && ExcludePropertyTracksMap[PropertyOwner].Contains(PropertyName))
				{
					PropertyPath = FPropertyPath::CreateEmpty();
					break;
				}

				UProperty* Property = PropertyOwnerClass->FindPropertyByName(*PropertyName);

				if (Property != nullptr)
				{
					PropertyPath->AddProperty(FPropertyInfo(Property));
				}

				UStructProperty* StructProperty = Cast<UStructProperty>(Property);

				if (StructProperty != nullptr)
				{
					PropertyOwnerClass = StructProperty->Struct;
					continue;
				}

				UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);

				if (ObjectProperty != nullptr)
				{
					PropertyOwnerClass = ObjectProperty->PropertyClass;
					continue;
				}

				break;
			}

			if (!Sequencer->CanKeyProperty(FCanKeyPropertyParams(Actor.GetClass(), *PropertyPath)))
			{
				continue;
			}

			// key property
			FKeyPropertyParams KeyPropertyParams(TArrayBuilder<UObject*>().Add(PropertyOwner), *PropertyPath, ESequencerKeyMode::ManualKey);

			Sequencer->KeyProperty(KeyPropertyParams);

			Sequencer->UpdateRuntimeInstances();
		}
	}
}


/* FLevelSequenceEditorToolkit callbacks
 *****************************************************************************/

void FLevelSequenceEditorToolkit::OnSequencerReceivedFocus()
{
	if (Sequencer.IsValid())
	{
		FLevelEditorSequencerIntegration::Get().OnSequencerReceivedFocus(Sequencer.ToSharedRef());
	}
}

void FLevelSequenceEditorToolkit::HandleAddComponentActionExecute(UActorComponent* Component)
{
	Sequencer->GetHandleToObject(Component);
}


void FLevelSequenceEditorToolkit::HandleAddComponentMaterialActionExecute(UPrimitiveComponent* Component, int32 MaterialIndex)
{
	FGuid ObjectHandle = Sequencer->GetHandleToObject(Component);
	UMovieScene* FocusedMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	FName IndexName( *FString::FromInt(MaterialIndex) );
	if ( FocusedMovieScene->FindTrack( UMovieSceneComponentMaterialTrack::StaticClass(), ObjectHandle, IndexName ) == nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT( "AddComponentMaterialTrack", "Add component material track" ) );

		FocusedMovieScene->Modify();

		UMovieSceneComponentMaterialTrack* MaterialTrack = FocusedMovieScene->AddTrack<UMovieSceneComponentMaterialTrack>( ObjectHandle );
		MaterialTrack->Modify();
		MaterialTrack->SetMaterialIndex( MaterialIndex );

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}


void FLevelSequenceEditorToolkit::HandleActorAddedToSequencer(AActor* Actor, const FGuid Binding)
{
	AddDefaultTracksForActor(*Actor, Binding);
}


void FLevelSequenceEditorToolkit::HandleVREditorModeExit()
{
	UWorld* World = Cast<UWorld>(GetLevelSequenceEditorPlaybackContext());
	UVREditorMode* VRMode = CastChecked<UVREditorMode>( GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions( World )->FindExtension( UVREditorMode::StaticClass() ) );

	// Reset sequencer settings
	Sequencer->SetAutoChangeMode(VRMode->GetSavedEditorState().AutoChangeMode);
	Sequencer->SetKeyAllEnabled(VRMode->GetSavedEditorState().bKeyAllEnabled);
	VRMode->OnVREditingModeExit_Handler.Unbind();
}

void FLevelSequenceEditorToolkit::HandleMapChanged(class UWorld* NewWorld, EMapChangeType MapChangeType)
{
	// @todo sequencer: We should only wipe/respawn puppets that are affected by the world that is being changed! (multi-UWorld support)
	if( ( MapChangeType == EMapChangeType::LoadMap || MapChangeType == EMapChangeType::NewMap || MapChangeType == EMapChangeType::TearDownWorld) )
	{
		Sequencer->GetSpawnRegister().CleanUp(*Sequencer);
		Sequencer->UpdateRuntimeInstances();
	}
}

void FLevelSequenceEditorToolkit::AddShot(UMovieSceneCinematicShotTrack* ShotTrack, const FString& ShotAssetName, const FString& ShotPackagePath, float ShotStartTime, float ShotEndTime, UObject* AssetToDuplicate, const FString& FirstShotAssetName)
{
	// Create a level sequence asset for the shot
	UObject* ShotAsset = LevelSequenceEditorHelpers::CreateLevelSequenceAsset(ShotAssetName, ShotPackagePath, AssetToDuplicate);
	UMovieSceneSequence* ShotSequence = Cast<UMovieSceneSequence>(ShotAsset);
	UMovieSceneSubSection* ShotSubSection = ShotTrack->AddSequence(ShotSequence, ShotStartTime, ShotEndTime-ShotStartTime);

	// Focus on the new shot
	GetSequencer()->UpdateRuntimeInstances();
	GetSequencer()->ForceEvaluate();
	GetSequencer()->FocusSequenceInstance(*ShotSubSection);

	const ULevelSequenceMasterSequenceSettings* MasterSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	// Create any subshots
	if (MasterSequenceSettings->SubSequenceNames.Num())
	{
		UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(ShotSequence->GetMovieScene()->FindMasterTrack(UMovieSceneSubTrack::StaticClass()));
		if (!SubTrack)
		{
			SubTrack = Cast<UMovieSceneSubTrack>(ShotSequence->GetMovieScene()->AddMasterTrack(UMovieSceneSubTrack::StaticClass()));
		}
	
		int32 RowIndex = 0;
		for (auto SubSequenceName : MasterSequenceSettings->SubSequenceNames)
		{
			FString SubSequenceAssetName = ShotAssetName + ProjectSettings->SubSequenceSeparator + SubSequenceName.ToString();

			UMovieSceneSequence* SubSequence = nullptr;
			if (!MasterSequenceSettings->bInstanceSubSequences || ShotTrack->GetAllSections().Num() == 1)
			{
				UObject* SubSequenceAsset = LevelSequenceEditorHelpers::CreateLevelSequenceAsset(SubSequenceAssetName, ShotPackagePath);
				SubSequence = Cast<UMovieSceneSequence>(SubSequenceAsset);
			}
			else
			{
				// Get the corresponding sequence from the first shot
				UMovieSceneSubSection* FirstShotSubSection = Cast<UMovieSceneSubSection>(ShotTrack->GetAllSections()[0]);
				UMovieSceneSequence* FirstShotSequence = FirstShotSubSection->GetSequence();
				UMovieSceneSubTrack* FirstShotSubTrack = Cast<UMovieSceneSubTrack>(FirstShotSequence->GetMovieScene()->FindMasterTrack(UMovieSceneSubTrack::StaticClass()));
			
				FString FirstShotSubSequenceAssetName = FirstShotAssetName + ProjectSettings->SubSequenceSeparator + SubSequenceName.ToString();

				for (auto Section : FirstShotSubTrack->GetAllSections())
				{
					UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
					if (SubSection->GetSequence()->GetDisplayName().ToString() == FirstShotSubSequenceAssetName)
					{
						SubSequence = SubSection->GetSequence();
						break;
					}
				}
			}

			if (SubSequence != nullptr)
			{
				UMovieSceneSubSection* SubSection = SubTrack->AddSequence(SubSequence, 0.f, ShotEndTime-ShotStartTime);
				SubSection->SetRowIndex(RowIndex++);
				SubSection->SetStartTime(0.f);
			}
		}
	}

	// Create a camera cut track with a camera if it doesn't already exist
	UMovieSceneTrack* CameraCutTrack = ShotSequence->GetMovieScene()->GetCameraCutTrack();
	if (!CameraCutTrack)
	{	
		// Create a cine camera asset
		ACineCameraActor* NewCamera = GCurrentLevelEditingViewportClient->GetWorld()->SpawnActor<ACineCameraActor>();
		
		const USequencerSettings* SequencerSettings = GetDefault<USequencerSettings>();
		bool bCreateSpawnableCamera = SequencerSettings->GetCreateSpawnableCameras();

		FGuid CameraGuid;
		if (bCreateSpawnableCamera)
		{
			CameraGuid = GetSequencer()->MakeNewSpawnable(*NewCamera);
			GetSequencer()->UpdateRuntimeInstances();
			UObject* SpawnedCamera = GetSequencer()->FindSpawnedObjectOrTemplate(CameraGuid);
			if (SpawnedCamera)
			{
				GCurrentLevelEditingViewportClient->GetWorld()->EditorDestroyActor(NewCamera, true);
				NewCamera = Cast<ACineCameraActor>(SpawnedCamera);
			}
		}
		else
		{
			CameraGuid = GetSequencer()->CreateBinding(*NewCamera, NewCamera->GetActorLabel());
		}
		NewCamera->SetActorLocation( GCurrentLevelEditingViewportClient->GetViewLocation(), false );
		NewCamera->SetActorRotation( GCurrentLevelEditingViewportClient->GetViewRotation() );
		//pNewCamera->CameraComponent->FieldOfView = ViewportClient->ViewFOV; //@todo set the focal length from this field of view
		
		AddDefaultTracksForActor(*NewCamera, CameraGuid);

		// Create a new camera cut section and add it to the camera cut track
		CameraCutTrack = ShotSequence->GetMovieScene()->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());
		UMovieSceneCameraCutSection* CameraCutSection = NewObject<UMovieSceneCameraCutSection>(CameraCutTrack, NAME_None, RF_Transactional);
		CameraCutSection->SetStartTime(ShotSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue()); 
		CameraCutSection->SetEndTime(ShotSequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue());
		CameraCutSection->SetCameraGuid(CameraGuid);
		CameraCutTrack->AddSection(*CameraCutSection);
	}
}

void FLevelSequenceEditorToolkit::HandleMasterSequenceCreated(UObject* MasterSequenceAsset)
{
	const FScopedTransaction Transaction( LOCTEXT( "CreateMasterSequence", "Create Master Sequence" ) );
	
	const ULevelSequenceMasterSequenceSettings* MasterSequenceSettings = GetDefault<ULevelSequenceMasterSequenceSettings>();
	uint32 NumShots = MasterSequenceSettings->MasterSequenceNumShots;
	ULevelSequence* AssetToDuplicate = MasterSequenceSettings->MasterSequenceLevelSequenceToDuplicate.Get();

	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();

	UMovieSceneSequence* MasterSequence = Cast<UMovieSceneSequence>(MasterSequenceAsset);
	UMovieSceneCinematicShotTrack* ShotTrack = MasterSequence->GetMovieScene()->AddMasterTrack<UMovieSceneCinematicShotTrack>();
		
	// Create shots with a camera cut and a camera for each
	float SequenceStartTime = ProjectSettings->DefaultStartTime;
	float ShotStartTime = SequenceStartTime;
	float ShotEndTime = ShotStartTime;
	FString FirstShotName; 
	for (uint32 ShotIndex = 0; ShotIndex < NumShots; ++ShotIndex)
	{
		ShotEndTime += ProjectSettings->DefaultDuration;

		FString ShotName = MovieSceneToolHelpers::GenerateNewShotName(ShotTrack->GetAllSections(), ShotStartTime);
		FString ShotPackagePath = MovieSceneToolHelpers::GenerateNewShotPath(MasterSequence->GetMovieScene(), ShotName);

		if (ShotIndex == 0)
		{
			FirstShotName = ShotName;
		}

		AddShot(ShotTrack, ShotName, ShotPackagePath, ShotStartTime, ShotEndTime, AssetToDuplicate, FirstShotName);
		GetSequencer()->ResetToNewRootSequence(*MasterSequence);

		ShotStartTime = ShotEndTime;
	}

	MasterSequence->GetMovieScene()->SetPlaybackRange(SequenceStartTime, ShotEndTime);

#if WITH_EDITORONLY_DATA
	const float OutputViewSize = ShotEndTime - SequenceStartTime;
	const float OutputChange = OutputViewSize * 0.1f;
	struct FMovieSceneEditorData EditorData;
	EditorData.ViewRange = FFloatRange(SequenceStartTime - OutputChange, ShotEndTime + OutputChange);
	EditorData.WorkingRange = FFloatRange(SequenceStartTime - OutputChange, ShotEndTime + OutputChange);
	MasterSequence->GetMovieScene()->SetEditorData(EditorData);
#endif
		
	GetSequencer()->ResetToNewRootSequence(*MasterSequence);

	UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass());
	if (!ensure(ActorFactory))
	{
		return;
	}

	ALevelSequenceActor* NewActor = CastChecked<ALevelSequenceActor>(GEditor->UseActorFactory(ActorFactory, FAssetData(MasterSequenceAsset), &FTransform::Identity));
	if (GCurrentLevelEditingViewportClient != nullptr && GCurrentLevelEditingViewportClient->IsPerspective())
	{
		GEditor->MoveActorInFrontOfCamera(*NewActor, GCurrentLevelEditingViewportClient->GetViewLocation(), GCurrentLevelEditingViewportClient->GetViewRotation().Vector());
	}
	else
	{
		GEditor->MoveViewportCamerasToActor(*NewActor, false);
	}
}


TSharedRef<FExtender> FLevelSequenceEditorToolkit::HandleMenuExtensibilityGetExtender(const TSharedRef<FUICommandList> CommandList, const TArray<UObject*> ContextSensitiveObjects)
{
	TSharedRef<FExtender> AddTrackMenuExtender(new FExtender());
	AddTrackMenuExtender->AddMenuExtension(
		SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection,
		EExtensionHook::Before,
		CommandList,
		FMenuExtensionDelegate::CreateRaw(this, &FLevelSequenceEditorToolkit::HandleTrackMenuExtensionAddTrack, ContextSensitiveObjects));

	return AddTrackMenuExtender;
}


TSharedRef<SDockTab> FLevelSequenceEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (Args.GetTabId() == SequencerMainTabId)
	{
		TabWidget = Sequencer->GetSequencerWidget();
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("SequencerMainTitle", "Sequencer"))
		.TabColorScale(GetTabColorScale())
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


void FLevelSequenceEditorToolkit::HandleTrackMenuExtensionAddTrack(FMenuBuilder& AddTrackMenuBuilder, TArray<UObject*> ContextObjects)
{
	if (ContextObjects.Num() != 1)
	{
		return;
	}

	AActor* Actor = Cast<AActor>(ContextObjects[0]);
	if (Actor != nullptr)
	{
		AddTrackMenuBuilder.BeginSection("Components", LOCTEXT("ComponentsSection", "Components"));
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (Component)
				{
					FUIAction AddComponentAction(FExecuteAction::CreateSP(this, &FLevelSequenceEditorToolkit::HandleAddComponentActionExecute, Component));
					FText AddComponentLabel = FText::FromString(Component->GetName());
					FText AddComponentToolTip = FText::Format(LOCTEXT("ComponentToolTipFormat", "Add {0} component"), FText::FromString(Component->GetName()));
					AddTrackMenuBuilder.AddMenuEntry(AddComponentLabel, AddComponentToolTip, FSlateIcon(), AddComponentAction);
				}
			}
		}
		AddTrackMenuBuilder.EndSection();
	}
	else
	{
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(ContextObjects[0]);
		if (Component != nullptr)
		{
			int32 NumMaterials = Component->GetNumMaterials();
			if (NumMaterials > 0)
			{
				AddTrackMenuBuilder.BeginSection("Materials", LOCTEXT("MaterialSection", "Materials"));
				{
					for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; MaterialIndex++)
					{
						FUIAction AddComponentMaterialAction(FExecuteAction::CreateSP(this, &FLevelSequenceEditorToolkit::HandleAddComponentMaterialActionExecute, Component, MaterialIndex));
						FText AddComponentMaterialLabel = FText::Format(LOCTEXT("ComponentMaterialIndexLabelFormat", "Element {0}"), FText::AsNumber(MaterialIndex));
						FText AddComponentMaterialToolTip = FText::Format(LOCTEXT("ComponentMaterialIndexToolTipFormat", "Add material element {0}"), FText::AsNumber(MaterialIndex));
						AddTrackMenuBuilder.AddMenuEntry(AddComponentMaterialLabel, AddComponentMaterialToolTip, FSlateIcon(), AddComponentMaterialAction);
					}
				}
				AddTrackMenuBuilder.EndSection();
			}
		}
	}
}

bool FLevelSequenceEditorToolkit::OnRequestClose()
{
	UWorld* World = CastChecked<UWorld>(GetLevelSequenceEditorPlaybackContext());
	UVREditorMode* VRMode = Cast<UVREditorMode>(GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(World)->FindExtension(UVREditorMode::StaticClass()));
	if (VRMode != nullptr)
	{
		// Null out the VR Mode's sequencer pointer
		VRMode->RefreshVREditorSequencer(nullptr);
	}
	OpenToolkits.Remove(this);

	OnClosedEvent.Broadcast();
	return true;
}

bool FLevelSequenceEditorToolkit::CanFindInContentBrowser() const
{
	// False so that sequencer doesn't take over Find In Content Browser functionality and always find the level sequence asset
	return false;
}

#undef LOCTEXT_NAMESPACE
