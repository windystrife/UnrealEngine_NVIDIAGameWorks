// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "Serialization/ArchiveTraceRoute.h"
#include "Misc/PackageName.h"
#include "InputCoreTypes.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "EditorStyleSettings.h"
#include "Engine/EngineTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/ProjectPackagingSettings.h"
#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "Engine/NavigationObjectBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "Components/AudioComponent.h"
#include "Engine/Note.h"
#include "Engine/Selection.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "UnrealEdMisc.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "EditorAnalytics.h"
#include "AudioDevice.h"
#include "BusyCursor.h"
#include "ScopedTransaction.h"
#include "PackageTools.h"
#include "Slate/SceneViewport.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "ILevelViewport.h"
#include "BlueprintEditorModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "ITargetDeviceServicesModule.h"
#include "ILauncherServicesModule.h"
#include "GameProjectGenerationModule.h"
#include "SourceCodeNavigation.h"
#include "PhysicsPublic.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/LocalPlayer.h"
#include "Slate/SGameLayerManager.h"

#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Engine/LevelStreaming.h"
#include "Components/ModelComponent.h"
#include "GameDelegates.h"
#include "Net/OnlineEngineInterface.h"
#include "Kismet2/DebuggerCommands.h"
#include "Misc/ScopeExit.h"


DEFINE_LOG_CATEGORY_STATIC(LogPlayLevel, Log, All);

#define LOCTEXT_NAMESPACE "PlayLevel"

const static FName NAME_CategoryPIE("PIE");

// This class listens to output log messages, and forwards warnings and errors to the message log
class FOutputLogErrorsToMessageLogProxy : public FOutputDevice
{
public:
	FOutputLogErrorsToMessageLogProxy()
	{
		GLog->AddOutputDevice(this);
	}

	~FOutputLogErrorsToMessageLogProxy()
{
		GLog->RemoveOutputDevice(this);
}

	// FOutputDevice interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		//@TODO: Remove IsInGameThread() once the message log is thread safe
		if ((Verbosity <= ELogVerbosity::Warning) && IsInGameThread())
		{
			const FText Message = FText::Format(LOCTEXT("OutputLogToMessageLog", "{0}: {1}"), FText::FromName(Category), FText::AsCultureInvariant(FString(V)));

			switch (Verbosity)
			{
			case ELogVerbosity::Warning:
				FMessageLog(NAME_CategoryPIE).SuppressLoggingToOutputLog(true).Warning(Message);
				break;
			case ELogVerbosity::Error:
				FMessageLog(NAME_CategoryPIE).SuppressLoggingToOutputLog(true).Error(Message);
				break;
			case ELogVerbosity::Fatal:
				FMessageLog(NAME_CategoryPIE).SuppressLoggingToOutputLog(true).CriticalError(Message);
				break;
			}
		}
	}
	// End of FOutputDevice interface
};

void UEditorEngine::EndPlayMap()
{
	if ( bIsEndingPlay )
	{
		return;
	}

	TGuardValue<bool> GuardIsEndingPlay(bIsEndingPlay, true);

	FEditorDelegates::PrePIEEnded.Broadcast( bIsSimulatingInEditor );

	// Clean up Soft Object Path remaps
	FSoftObjectPath::ClearPIEPackageNames();

	FlushAsyncLoading();

	// Monitoring when PIE corrupts references between the World and the PIE generated World for UE-20486
	{
		TArray<ULevel*> Levels = EditorWorld->GetLevels();

		for (ULevel* Level : Levels)
		{
			TArray<UBlueprint*> LevelBlueprints = Level->GetLevelBlueprints();

			if (LevelBlueprints.Num() > 0)
			{
				UBlueprint* LevelScriptBlueprint = LevelBlueprints[0];
				if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass && LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s' with ClassGeneratedBy '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName(), *LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy->GetPathName());
				}
				else if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName());
				}
				else if (LevelScriptBlueprint)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName());
				}
			}
		}
	}

	if (GEngine->XRSystem.IsValid() && !bIsSimulatingInEditor)
	{
		GEngine->XRSystem->OnEndPlay(*GEngine->GetWorldContextFromWorld(PlayWorld));
	}

	// Matinee must be closed before PIE can stop - matinee during PIE will be editing a PIE-world actor
	if( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit) )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "PIENeedsToCloseMatineeMessage", "Closing 'Play in Editor' must close UnrealMatinee.") );
		GLevelEditorModeTools().DeactivateMode( FBuiltinEditorModes::EM_InterpEdit );
	}

	EndPlayOnLocalPc();

	const FScopedBusyCursor BusyCursor;
	check(PlayWorld);

	// Enable screensavers when ending PIE.
	EnableScreenSaver( true );

	// Make a list of all the actors that should be selected
	TArray<UObject *> SelectedActors;
	if ( ActorsThatWereSelected.Num() > 0 )
	{
		for ( int32 ActorIndex = 0; ActorIndex < ActorsThatWereSelected.Num(); ++ActorIndex )
		{
			TWeakObjectPtr<AActor> Actor = ActorsThatWereSelected[ ActorIndex ].Get();
			if (Actor.IsValid())
			{
				SelectedActors.Add( Actor.Get() );
			}
		}
		ActorsThatWereSelected.Empty();
	}
	else
	{
		for ( FSelectionIterator It( GetSelectedActorIterator() ); It; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			if (Actor)
			{
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				AActor* EditorActor = EditorUtilities::GetEditorWorldCounterpartActor(Actor);
				if (EditorActor)
				{
					SelectedActors.Add( EditorActor );
				}
			}
		}
	}

	// Deselect all objects, to avoid problems caused by property windows still displaying
	// properties for an object that gets garbage collected during the PIE clean-up phase.
	GEditor->SelectNone( true, true, false );
	GetSelectedActors()->DeselectAll();
	GetSelectedObjects()->DeselectAll();
	GetSelectedComponents()->DeselectAll();

	// For every actor that was selected previously, make sure it's editor equivalent is selected
	GEditor->GetSelectedActors()->BeginBatchSelectOperation();
	for ( int32 ActorIndex = 0; ActorIndex < SelectedActors.Num(); ++ActorIndex )
	{
		AActor* Actor = Cast<AActor>( SelectedActors[ ActorIndex ] );
		if (Actor)
		{
			// We need to notify or else the manipulation transform widget won't appear, but only notify once at the end because OnEditorSelectionChanged is expensive for large groups. 
			SelectActor( Actor, false, false );
		}
	}	
	GEditor->GetSelectedActors()->EndBatchSelectOperation(true);

	// let the editor know
	FEditorDelegates::EndPIE.Broadcast(bIsSimulatingInEditor);

	// clean up any previous Play From Here sessions
	if ( GameViewport != NULL && GameViewport->Viewport != NULL )
	{
		// Remove debugger commands handler binding
		GameViewport->OnGameViewportInputKey().Unbind();

		// Remove close handler binding
		GameViewport->OnCloseRequested().Remove(ViewportCloseRequestedDelegateHandle);

		GameViewport->CloseRequested(GameViewport->Viewport);
	}
	CleanupGameViewport();

	// find objects like Textures in the playworld levels that won't get garbage collected as they are marked RF_Standalone
	for( FObjectIterator It; It; ++It )
	{
		UObject* Object = *It;

		if (Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			if (Object->HasAnyFlags(RF_Standalone))
			{
				// Clear RF_Standalone flag from objects in the levels used for PIE so they get cleaned up.
				Object->ClearFlags(RF_Standalone);
			}
			// Close any asset editors that are currently editing this object
			FAssetEditorManager::Get().CloseAllEditorsForAsset(Object);
		}
	}

	// Clean up each world individually
	TArray<FName> OnlineIdentifiers;
	TArray<UWorld*> WorldsBeingCleanedUp;
	bool bSeamlessTravelActive = false;

	for (int32 WorldIdx = WorldList.Num()-1; WorldIdx >= 0; --WorldIdx)
	{
		FWorldContext &ThisContext = WorldList[WorldIdx];
		if (ThisContext.WorldType == EWorldType::PIE)
		{
			if (ThisContext.World())
			{
				WorldsBeingCleanedUp.Add(ThisContext.World());
			}

			if (ThisContext.SeamlessTravelHandler.IsInTransition())
			{
				bSeamlessTravelActive = true;
			}

			if (ThisContext.World())
			{
				TeardownPlaySession(ThisContext);
			}

			// Cleanup online subsystems instantiated during PIE
			FName OnlineIdentifier = UOnlineEngineInterface::Get()->GetOnlineIdentifier(ThisContext);
			if (UOnlineEngineInterface::Get()->DoesInstanceExist(OnlineIdentifier))
			{
				// Stop ticking and clean up, but do not destroy as we may be in a failed online delegate
				UOnlineEngineInterface::Get()->ShutdownOnlineSubsystem(OnlineIdentifier);
				OnlineIdentifiers.Add(OnlineIdentifier);
			}
		
			// Remove world list after online has shutdown in case any async actions require the world context
			WorldList.RemoveAt(WorldIdx);
		}
	}

	// If seamless travel is happening then there is likely additional PIE worlds that need tearing down so seek them out
	if (bSeamlessTravelActive)
	{
		for (TObjectIterator<UWorld> WorldIt; WorldIt; ++WorldIt)
		{
			if (WorldIt->IsPlayInEditor())
			{
				WorldsBeingCleanedUp.AddUnique(*WorldIt);
			}
		}
	}

	if (OnlineIdentifiers.Num())
	{
		UE_LOG(LogPlayLevel, Display, TEXT("Shutting down PIE online subsystems"));
		// Cleanup online subsystem shortly as we might be in a failed delegate 
		// have to do this in batch because timer delegate doesn't recognize bound data 
		// as a different delegate
		FTimerDelegate DestroyTimer;
		DestroyTimer.BindUObject(this, &UEditorEngine::CleanupPIEOnlineSessions, OnlineIdentifiers);
		GetTimerManager()->SetTimer(CleanupPIEOnlineSessionsTimerHandle, DestroyTimer, 0.1f, false);
	}
	
	{
		// Clear out viewport index
		PlayInEditorViewportIndex = -1; 

		// We could have been toggling back and forth between simulate and pie before ending the play map
		// Make sure the property windows are cleared of any pie actors
		GUnrealEd->UpdateFloatingPropertyWindows();

		// Clean up any pie actors being referenced 
		GEngine->BroadcastLevelActorListChanged();
	}

	// Lose the EditorWorld pointer (this is only maintained while PIEing)
	if (EditorWorld->GetNavigationSystem())
	{
		EditorWorld->GetNavigationSystem()->OnPIEEnd();
	}

	FGameDelegates::Get().GetEndPlayMapDelegate().Broadcast();

	// Monitoring when PIE corrupts references between the World and the PIE generated World for UE-20486
	{
		TArray<ULevel*> Levels = EditorWorld->GetLevels();

		for (ULevel* Level : Levels)
		{
			TArray<UBlueprint*> LevelBlueprints = Level->GetLevelBlueprints();

			if (LevelBlueprints.Num() > 0)
			{
				UBlueprint* LevelScriptBlueprint = LevelBlueprints[0];
				if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass && LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s' with ClassGeneratedBy '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName(), *LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy->GetPathName());
				}
				else if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName());
				}
				else if (LevelScriptBlueprint)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late EndPlayMap Detection: Level '%s' has LevelScriptBlueprint '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName());
				}
			}
		}
	}

	EditorWorld->bAllowAudioPlayback = true;
	EditorWorld = NULL;

	// mark everything contained in the PIE worlds to be deleted
	for (UWorld* World : WorldsBeingCleanedUp)
	{
		// Occasionally during seamless travel the Levels array won't yet be populated so mark this world first
		// then pick up the sub-levels via the level iterator
		World->MarkObjectsPendingKill();
		
		// Because of the seamless travel the world might still be in the root set too, so also clear that
		World->RemoveFromRoot();

		for (auto LevelIt(World->GetLevelIterator()); LevelIt; ++LevelIt)
		{
			if (const ULevel* Level = *LevelIt)
			{
				// We already picked up the persistent level with the top level mark objects
				if (Level->GetOuter() != World)
				{
					CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
				}
			}
		}

		for (ULevelStreaming* LevelStreaming : World->StreamingLevels)
		{
			// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
			if ((!LevelStreaming->bShouldBeLoaded || !LevelStreaming->bShouldBeVisible) && LevelStreaming->GetLoadedLevel())
			{
				CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter())->MarkObjectsPendingKill();
			}
		}
	}

	// mark all objects contained within the PIE game instances to be deleted
	for (TObjectIterator<UGameInstance> It; It; ++It)
	{
		auto MarkObjectPendingKill = [](UObject* Object)
		{
			Object->MarkPendingKill();
		};
		ForEachObjectWithOuter(*It, MarkObjectPendingKill, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
	}

	// Flush any render commands and released accessed UTextures and materials to give them a chance to be collected.
	if ( FSlateApplication::IsInitialized() )
	{
		FSlateApplication::Get().FlushRenderState();
	}

	// Clean up any PIE world objects
	{
		// The trans buffer should never have a PIE object in it.  If it does though, as a s
		if( GEditor->Trans->ContainsPieObject() )
		{
			GEditor->ResetTransaction( NSLOCTEXT("UnrealEd", "TransactionContainedPIEObject", "A PIE object was in the transaction buffer and had to be destroyed") );
		}

		// Garbage Collect
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	// Make sure that all objects in the temp levels were entirely garbage collected.
	for( FObjectIterator ObjectIt; ObjectIt; ++ObjectIt )
	{
		UObject* Object = *ObjectIt;
		if( Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			UWorld* TheWorld = UWorld::FindWorldInPackage(Object->GetOutermost());
			if ( TheWorld )
			{
				StaticExec(nullptr, *FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s"), *TheWorld->GetPathName()));
			}
			else
			{
				UE_LOG(LogPlayLevel, Error, TEXT("No PIE world was found when attempting to gather references after GC."));
			}

			TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( Object, true, GARBAGE_COLLECTION_KEEPFLAGS );
			FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, Object );

			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Path"), FText::FromString(ErrorString));
				
			// We cannot safely recover from this.
			FMessageLog(NAME_CategoryPIE).CriticalError()
				->AddToken(FUObjectToken::Create(Object, FText::FromString(Object->GetFullName())))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("PIEObjectStillReferenced", "Object from PIE level still referenced. Shortest path from root: {Path}"), Arguments)));
		}
	}

	// Final cleanup/reseting
	FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
	UPackage* Package = EditorWorldContext.World()->GetOutermost();

	// Spawn note actors dropped in PIE.
	if(GEngine->PendingDroppedNotes.Num() > 0)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "CreatePIENoteActors", "Create PIE Notes") );

		for(int32 i=0; i<GEngine->PendingDroppedNotes.Num(); i++)
		{
			FDropNoteInfo& NoteInfo = GEngine->PendingDroppedNotes[i];
			ANote* NewNote = EditorWorldContext.World()->SpawnActor<ANote>(NoteInfo.Location, NoteInfo.Rotation);
			if(NewNote)
			{
				NewNote->Text = NoteInfo.Comment;
				if( NewNote->GetRootComponent() != NULL )
				{
					NewNote->GetRootComponent()->SetRelativeScale3D( FVector(2.f) );
				}
			}
		}
		Package->MarkPackageDirty();
		GEngine->PendingDroppedNotes.Empty();
	}

	// Restores realtime viewports that have been disabled for PIE.
	RestoreRealtimeViewports();

	// Don't actually need to reset this delegate but doing so allows is to check invalid attempts to execute the delegate
	FScopedConditionalWorldSwitcher::SwitchWorldForPIEDelegate = FOnSwitchWorldForPIE();

	// Set the autosave timer to have at least 10 seconds remaining before autosave
	const static float SecondsWarningTillAutosave = 10.0f;
	GUnrealEd->GetPackageAutoSaver().ForceMinimumTimeTillAutoSave(SecondsWarningTillAutosave);

	for(TObjectIterator<UAudioComponent> It; It; ++It)
	{
		UAudioComponent* AudioComp = *It;
		if (AudioComp->GetWorld() == EditorWorldContext.World())
		{
			AudioComp->ReregisterComponent();
		}
	}

	// no longer queued
	CancelRequestPlaySession();
	bIsSimulateInEditorQueued = false;
	bRequestEndPlayMapQueued = false;
	bUseVRPreviewForPlayWorld = false;

	// Tear down the output log to message log thunker
	OutputLogErrorsToMessageLogProxyPtr.Reset();

	// Remove undo barrier
	GUnrealEd->Trans->RemoveUndoBarrier();

	// display any info if required.
	FMessageLog(NAME_CategoryPIE).Notify(LOCTEXT("PIEErrorsPresent", "Errors/warnings reported while playing in editor."));
}

void UEditorEngine::CleanupPIEOnlineSessions(TArray<FName> OnlineIdentifiers)
{
	for (FName& OnlineIdentifier : OnlineIdentifiers)
	{
		UE_LOG(LogPlayLevel, Display, TEXT("Destroying online subsystem %s"), *OnlineIdentifier.ToString());
		UOnlineEngineInterface::Get()->DestroyOnlineSubsystem(OnlineIdentifier);
		NumOnlinePIEInstances--;
	}

	NumOnlinePIEInstances = 0;
}

void UEditorEngine::TeardownPlaySession(FWorldContext& PieWorldContext)
{
	check(PieWorldContext.WorldType == EWorldType::PIE);
	PlayWorld = PieWorldContext.World();
	PlayWorld->bIsTearingDown = true;

	if (!PieWorldContext.RunAsDedicated)
	{
		// Slate data for this pie world
		FSlatePlayInEditorInfo* const SlatePlayInEditorSession = SlatePlayInEditorMap.Find(PieWorldContext.ContextHandle);

		// Destroy Viewport
		if ( PieWorldContext.GameViewport != NULL && PieWorldContext.GameViewport->Viewport != NULL )
		{
			PieWorldContext.GameViewport->CloseRequested(PieWorldContext.GameViewport->Viewport);
		}
		CleanupGameViewport();
	
		// Clean up the slate PIE viewport if we have one
		if (SlatePlayInEditorSession)
		{
			if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
			{
				TSharedPtr<ILevelViewport> Viewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();

				if( !bIsSimulatingInEditor)
				{
					// Set the editor viewport location to match that of Play in Viewport if we aren't simulating in the editor, we have a valid player to get the location from (unless we're going back to VR Editor, in which case we won't teleport the user.)
					if (bLastViewAndLocationValid == true && !GEngine->IsStereoscopic3D( Viewport->GetActiveViewport() ) )
					{
						bLastViewAndLocationValid = false;
						Viewport->GetLevelViewportClient().SetViewLocation( LastViewLocation );

						if( Viewport->GetLevelViewportClient().IsPerspective() )
						{
							// Rotation only matters for perspective viewports not orthographic
							Viewport->GetLevelViewportClient().SetViewRotation( LastViewRotation );
						}
					}
				}

				// No longer simulating in the viewport
				Viewport->GetLevelViewportClient().SetIsSimulateInEditorViewport( false );

				// Clear out the hit proxies before GC'ing
				Viewport->GetLevelViewportClient().Viewport->InvalidateHitProxy();
			}
			else if (SlatePlayInEditorSession->SlatePlayInEditorWindow.IsValid())
			{
				// Unregister the game viewport from slate.  This sends a final message to the viewport
				// so it can have a chance to release mouse capture, mouse lock, etc.		
				FSlateApplication::Get().UnregisterGameViewport();

				// Viewport client is cleaned up.  Make sure its not being accessed
				SlatePlayInEditorSession->SlatePlayInEditorWindowViewport->SetViewportClient(NULL);

				// The window may have already been destroyed in the case that the PIE window close box was pressed 
				if (SlatePlayInEditorSession->SlatePlayInEditorWindow.IsValid())
				{
					// Destroy the SWindow
					FSlateApplication::Get().DestroyWindowImmediately(SlatePlayInEditorSession->SlatePlayInEditorWindow.Pin().ToSharedRef());
				}
			}
		}
	
		// Disassociate the players from their PlayerControllers.
		// This is done in the GameEngine path in UEngine::LoadMap.
		// But since PIE is just shutting down, and not loading a 
		// new map, we need to do it manually here for now.
		//for (auto It = GEngine->GetLocalPlayerIterator(PlayWorld); It; ++It)
		for (FLocalPlayerIterator It(GEngine, PlayWorld); It; ++It)
		{
			if(It->PlayerController)
			{
				if(It->PlayerController->GetPawn())
				{
					PlayWorld->DestroyActor(It->PlayerController->GetPawn(), true);
				}
				PlayWorld->DestroyActor(It->PlayerController, true);
				It->PlayerController = NULL;
			}
		}

	}

	// Change GWorld to be the play in editor world during cleanup.
	ensureMsgf( EditorWorld == GWorld, TEXT("TearDownPlaySession current world: %s"), GWorld ? *GWorld->GetName() : TEXT("No World"));
	GWorld = PlayWorld;
	GIsPlayInEditorWorld = true;
	
	// Remember Simulating flag so that we know if OnSimulateSessionFinished is required after everything has been cleaned up. 
	bool bWasSimulatingInEditor = bIsSimulatingInEditor;
	// Clear Simulating In Editor bit
	bIsSimulatingInEditor = false;

	
	// Stop all audio and remove references to temp level.
	if (FAudioDevice* AudioDevice = PlayWorld->GetAudioDevice())
	{
		AudioDevice->Flush(PlayWorld);
		AudioDevice->ResetInterpolation();
		AudioDevice->OnEndPIE(false); // TODO: Should this have been bWasSimulatingInEditor?
		AudioDevice->SetTransientMasterVolume(1.0f);
	}

	// Clean up all streaming levels
	PlayWorld->bIsLevelStreamingFrozen = false;
	PlayWorld->bShouldForceUnloadStreamingLevels = true;
	PlayWorld->FlushLevelStreaming();

	// cleanup refs to any duplicated streaming levels
	for ( int32 LevelIndex=0; LevelIndex<PlayWorld->StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* StreamingLevel = PlayWorld->StreamingLevels[LevelIndex];
		if( StreamingLevel != NULL )
		{
			const ULevel* PlayWorldLevel = StreamingLevel->GetLoadedLevel();
			if ( PlayWorldLevel != NULL )
			{
				UWorld* World = Cast<UWorld>( PlayWorldLevel->GetOuter() );
				if( World != NULL )
				{
					// Attempt to move blueprint debugging references back to the editor world
					if( EditorWorld != NULL && EditorWorld->StreamingLevels.IsValidIndex(LevelIndex) )
					{
						const ULevel* EditorWorldLevel = EditorWorld->StreamingLevels[LevelIndex]->GetLoadedLevel();
						if ( EditorWorldLevel != NULL )
						{
							UWorld* SublevelEditorWorld  = Cast<UWorld>(EditorWorldLevel->GetOuter());
							if( SublevelEditorWorld != NULL )
							{
								World->TransferBlueprintDebugReferences(SublevelEditorWorld);
							}	
						}
					}
				}
			}
		}
	}

	// Construct a list of editors that are active for objects being debugged. We will refresh these when we have cleaned up to ensure no invalid objects exist in them
	TArray< IBlueprintEditor* > Editors;
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
	const UWorld::FBlueprintToDebuggedObjectMap& EditDebugObjectsPre = PlayWorld->GetBlueprintObjectsBeingDebugged();
	for (UWorld::FBlueprintToDebuggedObjectMap::TConstIterator EditIt(EditDebugObjectsPre); EditIt; ++EditIt)
	{
		if (UBlueprint* TargetBP = EditIt.Key().Get())
		{
			if(IBlueprintEditor* EachEditor = static_cast<IBlueprintEditor*>(AssetEditorManager.FindEditorForAsset(TargetBP, false)))
			{
				Editors.AddUnique( EachEditor );
			}
		}
	}

	// Go through and let all the PlayWorld Actor's know they are being destroyed
	for (FActorIterator ActorIt(PlayWorld); ActorIt; ++ActorIt)
	{
		ActorIt->RouteEndPlay(EEndPlayReason::EndPlayInEditor);
	}

	PieWorldContext.OwningGameInstance->Shutdown();

	// Move blueprint debugging pointers back to the objects in the editor world
	PlayWorld->TransferBlueprintDebugReferences(EditorWorld);

	FPhysScene* PhysScene = PlayWorld->GetPhysicsScene();
	if (PhysScene)
	{
		PhysScene->WaitPhysScenes();
		PhysScene->KillVisualDebugger();
	}

	// Clean up the temporary play level.
	PlayWorld->CleanupWorld();

	// Remove from root (Seamless travel may have done this)
	PlayWorld->RemoveFromRoot();
		
	PlayWorld = NULL;

	// Refresh any editors we had open in case they referenced objects that no longer exist.
	for (int32 iEditors = 0; iEditors <  Editors.Num(); iEditors++)
	{
		Editors[ iEditors ]->RefreshEditors();
	}
	
	// Restore GWorld.
	GWorld = EditorWorld;
	GIsPlayInEditorWorld = false;

	FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();

	// Let the viewport know about leaving PIE/Simulate session. Do it after everything's been cleaned up
	// as the viewport will play exit sound here and this has to be done after GetAudioDevice()->Flush
	// otherwise all sounds will be immediately stopped.
	if (!PieWorldContext.RunAsDedicated)
	{
		// Slate data for this pie world
		FSlatePlayInEditorInfo* const SlatePlayInEditorSession = SlatePlayInEditorMap.Find(PieWorldContext.ContextHandle);
		if (SlatePlayInEditorSession && SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
		{
			TSharedPtr<ILevelViewport> Viewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();

			if( Viewport->HasPlayInEditorViewport() )
			{
				Viewport->EndPlayInEditorSession();
			}

			// Let the Slate viewport know that we're leaving Simulate mode
			if( bWasSimulatingInEditor )
			{
				Viewport->OnSimulateSessionFinished();
			}

			Viewport->GetLevelViewportClient().SetReferenceToWorldContext(EditorWorldContext);
		}

		// Remove the slate info from the map (note that the UWorld* is long gone at this point, but the WorldContext still exists. It will be removed outside of this function)
		SlatePlayInEditorMap.Remove(PieWorldContext.ContextHandle);
	}
}

void UEditorEngine::PlayMap( const FVector* StartLocation, const FRotator* StartRotation, int32 Destination, int32 InPlayInViewportIndex, bool bUseMobilePreview )
{
	// queue up a Play From Here request, this way the load/save won't conflict with the TransBuffer, which doesn't like 
	// loading and saving to happen during a transaction

	// save the StartLocation if we have one
	if (StartLocation)
	{
		PlayWorldLocation = *StartLocation;
		PlayWorldRotation = StartRotation ? *StartRotation : FRotator::ZeroRotator;
		bHasPlayWorldPlacement = true;
	}
	else
	{
		bHasPlayWorldPlacement = false;
	}

	// remember where to send the play map request
	PlayWorldDestination = Destination;

	// Set whether or not we want to use mobile preview mode (PC platform only)
	bUseMobilePreviewForPlayWorld = bUseMobilePreview;
	bUseVRPreviewForPlayWorld = false;

	// tell the editor to kick it off next Tick()
	bIsPlayWorldQueued = true;

	// Not wanting to simulate
	bIsSimulateInEditorQueued = false;

	// Unless we've been asked to play in a specific viewport window, this index will be -1
	PlayInEditorViewportIndex = InPlayInViewportIndex;
}


void UEditorEngine::RequestPlaySession( bool bAtPlayerStart, TSharedPtr<class ILevelViewport> DestinationViewport, bool bInSimulateInEditor, const FVector* StartLocation, const FRotator* StartRotation, int32 DestinationConsole, bool bUseMobilePreview, bool bUseVRPreview, bool bUseVulkanPreview )
{
	// Remember whether or not we were attempting to play from playerstart or from viewport
	GIsPIEUsingPlayerStart = bAtPlayerStart;

	// queue up a Play From Here request, this way the load/save won't conflict with the TransBuffer, which doesn't like 
	// loading and saving to happen during a transaction

	// save the StartLocation if we have one
	if (!bInSimulateInEditor && StartLocation != NULL)
	{
		PlayWorldLocation = *StartLocation;
		PlayWorldRotation = StartRotation ? *StartRotation : FRotator::ZeroRotator;
		bHasPlayWorldPlacement = true;
	}
	else
	{
		bHasPlayWorldPlacement = false;
	}

	// remember where to send the play map request
	PlayWorldDestination = DestinationConsole;

	RequestedDestinationSlateViewport = DestinationViewport;

	// Set whether or not we want to use mobile preview mode (PC platform only)
	bUseMobilePreviewForPlayWorld = bUseMobilePreview;

	bUseVRPreviewForPlayWorld = bUseVRPreview;

	// tell the editor to kick it off next Tick()
	bIsPlayWorldQueued = true;

	// Store whether we want to play in editor, or only simulate in editor
	bIsSimulateInEditorQueued = bInSimulateInEditor;

	// Unless we have been asked to play in a specific viewport window, this index will be -1
	PlayInEditorViewportIndex = -1;

	bPlayOnLocalPcSession = false;
	bPlayUsingLauncher = false;
}

void UEditorEngine::RequestPlaySession( const FVector* StartLocation, const FRotator* StartRotation, bool MobilePreview, bool VulkanPreview , const FString& MobilePreviewTargetDevice, FString AdditionalLaunchParameters)
{
	bPlayOnLocalPcSession = true;
	bPlayUsingLauncher = false;
	bPlayUsingMobilePreview = MobilePreview;
	bPlayUsingVulkanPreview = VulkanPreview;
	RequestedAdditionalStandaloneLaunchOptions = AdditionalLaunchParameters;
	PlayUsingMobilePreviewTargetDevice = MobilePreviewTargetDevice;

	if (StartLocation != NULL)
	{
		PlayWorldLocation = *StartLocation;
		PlayWorldRotation = StartRotation ? *StartRotation : FRotator::ZeroRotator;
		bHasPlayWorldPlacement = true;
	}
	else
	{
		bHasPlayWorldPlacement = false;
	}

	bIsPlayWorldQueued = true;
}

void UEditorEngine::RequestPlaySession(const FString& DeviceId, const FString& DeviceName)
{
	bPlayOnLocalPcSession = false;
	bPlayUsingLauncher = true;

	// always use player start on remote devices (for now?)
	bHasPlayWorldPlacement = false;

	// remember the platform name to run on
	PlayUsingLauncherDeviceId = DeviceId;
	PlayUsingLauncherDeviceName = DeviceName;

	bIsPlayWorldQueued = true;
}

void UEditorEngine::CancelRequestPlaySession()
{
	bIsPlayWorldQueued = false;
	bPlayOnLocalPcSession = false;
	bPlayUsingLauncher = false;
	bPlayUsingMobilePreview = false;
	bPlayUsingVulkanPreview = false;
	RequestedAdditionalStandaloneLaunchOptions = FString();
	PlayUsingMobilePreviewTargetDevice.Reset();
}

bool UEditorEngine::SaveMapsForPlaySession()
{
	// Prompt the user to save the level if it has not been saved before. 
	// An unmodified but unsaved blank template level does not appear in the dirty packages check below.
	if (FEditorFileUtils::GetFilename(GWorld).Len() == 0)
	{
		if (!FEditorFileUtils::SaveCurrentLevel())
		{
			CancelRequestPlaySession();
			return false;
		}
	}

	// Also save dirty packages, this is required because we're going to be launching a session outside of our normal process
	bool bPromptUserToSave = true;
	bool bSaveMapPackages = true;
	bool bSaveContentPackages = true;
	if (!FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
	{
		CancelRequestPlaySession();
		return false;
	}

	return true;
}

void UEditorEngine::PlaySessionPaused()
{
	FEditorDelegates::PausePIE.Broadcast(bIsSimulatingInEditor);
}

void UEditorEngine::PlaySessionResumed()
{
	FEditorDelegates::ResumePIE.Broadcast(bIsSimulatingInEditor);
}

void UEditorEngine::PlaySessionSingleStepped()
{
	FEditorDelegates::SingleStepPIE.Broadcast(bIsSimulatingInEditor);
}

bool UEditorEngine::ProcessDebuggerCommands(const FKey InKey, const FModifierKeysState ModifierKeyState, EInputEvent EventType )
{
	if( EventType == IE_Pressed )
	{
		return FPlayWorldCommands::GlobalPlayWorldActions->ProcessCommandBindings(InKey, ModifierKeyState, false);
	}
	
	return false;
}

/* fits the window position to make sure it falls within the confines of the desktop */
void FitWindowPositionToWorkArea(FIntPoint &WinPos, FIntPoint &WinSize, const FMargin &WinPadding)
{
	const int32 HorzPad = WinPadding.GetTotalSpaceAlong<Orient_Horizontal>();
	const int32 VertPad = WinPadding.GetTotalSpaceAlong<Orient_Vertical>();
	FIntPoint TotalSize( WinSize.X + HorzPad, WinSize.Y + VertPad );

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

	// Limit the size, to make sure it fits within the desktop area
	{
		FIntPoint NewWinSize;
		NewWinSize.X = FMath::Min(TotalSize.X, DisplayMetrics.VirtualDisplayRect.Right - DisplayMetrics.VirtualDisplayRect.Left );
		NewWinSize.Y = FMath::Min(TotalSize.Y, DisplayMetrics.VirtualDisplayRect.Bottom - DisplayMetrics.VirtualDisplayRect.Top );
		if( NewWinSize != TotalSize )
		{
			TotalSize = NewWinSize;
			WinSize.X = NewWinSize.X - HorzPad;
			WinSize.Y = NewWinSize.Y - VertPad;
		}
	}

	const FSlateRect PreferredWorkArea( DisplayMetrics.VirtualDisplayRect.Left, 
										DisplayMetrics.VirtualDisplayRect.Top, 
										DisplayMetrics.VirtualDisplayRect.Right - TotalSize.X, 
										DisplayMetrics.VirtualDisplayRect.Bottom - TotalSize.Y );

	// if no more windows fit horizontally, place them in a new row
	if (WinPos.X > PreferredWorkArea.Right)
	{
		WinPos.X = PreferredWorkArea.Left;
		WinPos.Y += TotalSize.Y;
		if (WinPos.Y > PreferredWorkArea.Bottom)
		{
			WinPos.Y = PreferredWorkArea.Top;
		}
	}
	
	// if no more rows fit vertically, stack windows on top of each other
	else if (WinPos.Y > PreferredWorkArea.Bottom)
	{
		WinPos.Y = PreferredWorkArea.Top;
		WinPos.X += TotalSize.X;
		if (WinPos.X > PreferredWorkArea.Right)
		{
			WinPos.X = PreferredWorkArea.Left;
		}
	}

	// Clamp values to make sure they fall within the desktop area
	WinPos.X = FMath::Clamp(WinPos.X, (int32)PreferredWorkArea.Left, (int32)PreferredWorkArea.Right);
	WinPos.Y = FMath::Clamp(WinPos.Y, (int32)PreferredWorkArea.Top, (int32)PreferredWorkArea.Bottom);
}

/* advances the windows position to the next location and fits */
void AdvanceWindowPositionForNextPIEWindow(FIntPoint &WinPos, const FIntPoint &WinSize, const FMargin &WinPadding, bool bVertical)
{
	const int32 HorzPad = WinPadding.GetTotalSpaceAlong<Orient_Horizontal>();
	const int32 VertPad = WinPadding.GetTotalSpaceAlong<Orient_Vertical>();
	const FIntPoint TotalSize( WinSize.X + HorzPad, WinSize.Y + VertPad );

	if(bVertical)
	{
		WinPos.Y += TotalSize.Y;
	}
	else
	{
		WinPos.X += TotalSize.X;
	}
}

/* returns the size of the window depending on the net mode. */
void GetWindowSizeForInstanceType(FIntPoint &WindowSize, const ULevelEditorPlaySettings* PlayInSettings)
{
	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	if (PlayNetMode == PIE_Standalone)
	{
		WindowSize.X = PlayInSettings->StandaloneWindowWidth;
		WindowSize.Y = PlayInSettings->StandaloneWindowHeight;
	}
	else
	{
		PlayInSettings->GetClientWindowSize(WindowSize);
	}
}

/* sets the size of the window depending on the net mode */
void SetWindowSizeForInstanceType(const FIntPoint &WindowSize, ULevelEditorPlaySettings* PlayInSettings)
{
	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	if (PlayNetMode == PIE_Standalone)
	{
		PlayInSettings->StandaloneWindowWidth = WindowSize.X;
		PlayInSettings->StandaloneWindowHeight = WindowSize.Y;
	}
	else
	{
		PlayInSettings->SetClientWindowSize(WindowSize);
	}
}

/** 
 * Generate the command line for pie instance. Window position, size etc. 
 *
 * @param	WinPos			Window position. This will contain the X & Y position to use for the next window. (Not changed for dedicated server window).
 * @param	InstanceNum		PIE instance index.
 * @param	bIsDedicatedServer	Is this instance a dedicate server. true if so else false.
 */
FString GenerateCmdLineForNextPieInstance(FIntPoint &WinPos, int32 &InstanceNum, bool bIsDedicatedServer)
{
	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
	// Get GameSettings INI override
	FString GameUserSettingsOverride = GGameUserSettingsIni.Replace(TEXT("GameUserSettings"), *FString::Printf(TEXT("PIEGameUserSettings%d"), InstanceNum++));
	
	// Construct parms:
	//	-Override GameUserSettings.ini
	//	-Force no steam
	//	-Allow saving of config files (since we are giving them an override INI)
	const FString AdditionalLaunchOptions = [&PlayInSettings]{ FString LaunchOptions; return (PlayInSettings->GetAdditionalLaunchOptions(LaunchOptions) ? LaunchOptions : FString()); }();
	FString CmdLine = FString::Printf(TEXT("GameUserSettingsINI=\"%s\" -MultiprocessSaveConfig %s -MultiprocessOSS "), *GameUserSettingsOverride, *AdditionalLaunchOptions);

	if (bIsDedicatedServer)
	{
		// Append dedicated server options
		CmdLine += TEXT("-server -log ");
	}
	else
	{
		// Default to what we expect the border to be (on windows at least) to prevent it occurring offscreen if TLW call fails
		FMargin WindowBorderSize(8.0f, 30.0f, 8.0f, 8.0f);
		TSharedPtr<SWindow> TopLevelWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

		if (TopLevelWindow.IsValid())
		{
			WindowBorderSize = TopLevelWindow->GetWindowBorderSize(true);
		}
		
		// Get the size of the window based on the type
		FIntPoint WinSize(0,0);
		GetWindowSizeForInstanceType(WinSize, PlayInSettings);

		// Make sure the window is going to fit where we want it
		FitWindowPositionToWorkArea(WinPos, WinSize, WindowBorderSize);

		// Set the size, in case it was modified
		SetWindowSizeForInstanceType(WinSize, GetMutableDefault<ULevelEditorPlaySettings>());

		// Listen server or clients: specify default win position and SAVEWINPOS so the final positions are saved
		// in order to preserve PIE networking window setup
		CmdLine += FString::Printf(TEXT("WinX=%d WinY=%d SAVEWINPOS=1"), WinPos.X + (int32)WindowBorderSize.Left, WinPos.Y + (int32)WindowBorderSize.Top);

		// Advance window for next PIE instance...
		AdvanceWindowPositionForNextPIEWindow(WinPos, WinSize, WindowBorderSize, false);
	}
	
	return CmdLine;
}

void GetMultipleInstancePositions(int32 index, int32 &LastX, int32 &LastY)
{
	ULevelEditorPlaySettings* PlayInSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());

	if (PlayInSettings->MultipleInstancePositions.IsValidIndex(index) &&
		(PlayInSettings->MultipleInstanceLastHeight == PlayInSettings->NewWindowHeight) &&
		(PlayInSettings->MultipleInstanceLastWidth == PlayInSettings->NewWindowWidth))
	{
		PlayInSettings->NewWindowPosition = PlayInSettings->MultipleInstancePositions[index];

		LastX = PlayInSettings->NewWindowPosition.X;
		LastY = PlayInSettings->NewWindowPosition.Y;
	}
	else
	{
		PlayInSettings->NewWindowPosition = FIntPoint(LastX, LastY);
	}

	FIntPoint WinPos(LastX, LastY);

	// Get the size of the window based on the type
	FIntPoint WinSize(0, 0);
	GetWindowSizeForInstanceType(WinSize, PlayInSettings);

	// Advance window and make sure the window is going to fit where we want it
	const FMargin WinPadding(16, 16);
	AdvanceWindowPositionForNextPIEWindow(WinPos, WinSize, WinPadding, false);
	FitWindowPositionToWorkArea(WinPos, WinSize, WinPadding);

	// Set the size, incase it was modified
	SetWindowSizeForInstanceType(WinSize, PlayInSettings);

	LastX = WinPos.X;
	LastY = WinPos.Y;
}

void UEditorEngine::StartQueuedPlayMapRequest()
{
	const bool bWantSimulateInEditor = bIsSimulateInEditorQueued;

	EndPlayOnLocalPc();

	ON_SCOPE_EXIT
	{
		// note that we no longer have a queued request
		bIsPlayWorldQueued = false;
		bIsSimulateInEditorQueued = false;
	};

	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();

	// Launch multi-player instances if necessary
	// (note that if you have 'RunUnderOneProcess' checked and do a bPlayOnLocalPcSession (standalone) - play standalone 'wins' - multiple instances will be launched for multiplayer)
	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	const bool CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();

	const bool bRequestSave = bPlayOnLocalPcSession || bPlayUsingLauncher || (!CanRunUnderOneProcess && PlayNetMode != PIE_Standalone);
	if (bRequestSave && !SaveMapsForPlaySession())
	{
		// Maps did not save, print a warning
		FText ErrorMsg = LOCTEXT("PIEWorldSaveFail", "PIE failed because map save was canceled");
		UE_LOG(LogPlayLevel, Warning, TEXT("%s"), *ErrorMsg.ToString());
		FMessageLog(NAME_CategoryPIE).Warning(ErrorMsg);
		FMessageLog(NAME_CategoryPIE).Open();
		return;
	}

	if (PlayNetMode != PIE_Standalone && (!CanRunUnderOneProcess || bPlayOnLocalPcSession) && !bPlayUsingLauncher)
	{
		int32 NumClients = 0;

		// If we start to the right of the editor work area, call FitToWorkArea and it will find the next place we can place a new instance window if that's not preferable.
		const FSlateRect PreferredWorkArea = FSlateApplication::Get().GetPreferredWorkArea();		
		FIntPoint WinPosition((int32)PreferredWorkArea.Right, (int32)PreferredWorkArea.Top);

		// We'll need to spawn a server if we're playing outside the editor or the editor wants to run as a client
		if (bPlayOnLocalPcSession || PlayNetMode == PIE_Client)
		{			
			PlayStandaloneLocalPc(TEXT(""), &WinPosition, NumClients, true);
			
			const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
			if (!CanPlayNetDedicated)
			{
				// Listen server counts as a client
				NumClients++;
			}
		}
		
		// If we're playing in the editor
		if (!bPlayOnLocalPcSession)
		{
			PlayInEditor(GetEditorWorldContext().World(), bWantSimulateInEditor);

			// Editor counts as a client
			NumClients++;
		}

		// Build the connection String
		FString ConnectionAddr(TEXT("127.0.0.1"));

		// Ignore the user's settings if the autoconnect option is inaccessible due to settings conflicts.
		const bool WillAutoConnectToServer = [&PlayInSettings] { bool AutoConnectToServer(false); 
			return (PlayInSettings->GetAutoConnectToServerVisibility() == EVisibility::Visible) ? 
				(PlayInSettings->GetAutoConnectToServer(AutoConnectToServer) && AutoConnectToServer) : true; }();

		if (WillAutoConnectToServer)
		{
			uint16 ServerPort = 0;
			if (PlayInSettings->GetServerPort(ServerPort))
			{
				ConnectionAddr += FString::Printf(TEXT(":%hu"), ServerPort);
			}
		}

		// Spawn number of clients
		const int32 PlayNumberOfClients = [&PlayInSettings]{ int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();
		for (int32 i = NumClients; i < PlayNumberOfClients; ++i)
		{
			PlayStandaloneLocalPc(*ConnectionAddr, &WinPosition, i, false);
		}
	}
	else
	{
		// Launch standalone PIE session
		if (bPlayOnLocalPcSession)
		{
			PlayStandaloneLocalPc();
		}
		else if (bPlayUsingLauncher)
		{
			PlayUsingLauncher();
		}
		else
		{
			PlayInEditor( GetEditorWorldContext().World(), bWantSimulateInEditor );
		}
	}

	// note that we no longer have a queued request
	CancelRequestPlaySession();
}

void UEditorEngine::EndPlayOnLocalPc( )
{
	for (int32 i=0; i < PlayOnLocalPCSessions.Num(); ++i)
	{
		if (PlayOnLocalPCSessions[i].ProcessHandle.IsValid())
		{
			if ( FPlatformProcess::IsProcRunning(PlayOnLocalPCSessions[i].ProcessHandle) )
			{
				FPlatformProcess::TerminateProc(PlayOnLocalPCSessions[i].ProcessHandle);
			}
			PlayOnLocalPCSessions[i].ProcessHandle.Reset();
		}
	}

	PlayOnLocalPCSessions.Empty();
}

void UEditorEngine::PlayStandaloneLocalPc(FString MapNameOverride, FIntPoint* WindowPos, int32 PIENum, bool bIsServer)
{
	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
	//const ULevelEditorPlaySettings* PlayInSettings = InPlaySettings != NULL ? InPlaySettings : GetDefault<ULevelEditorPlaySettings>();
	const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();

	FString CmdLine;
	if (WindowPos != NULL)	// If WindowPos == NULL, we're just launching one instance
	{
		CmdLine = GenerateCmdLineForNextPieInstance(*WindowPos, PIENum, bIsServer && CanPlayNetDedicated);
	}

	const FString URLParms = bIsServer && !CanPlayNetDedicated ? TEXT("?Listen") : FString();

	// select map to play
	TArray<FString> SavedMapNames;
	if (MapNameOverride.IsEmpty())
	{
		FWorldContext & EditorContext = GetEditorWorldContext();

		// Open original map
		FString MapName = EditorContext.World()->GetOutermost()->GetName();
		SavedMapNames.Add(MapName);
	}
	else
	{
		SavedMapNames.Add(MapNameOverride);
	}

	if (SavedMapNames.Num() == 0)
	{
		return;
	}

	FString GameNameOrProjectFile;
	if (FPaths::IsProjectFilePathSet())
	{
		GameNameOrProjectFile = FString::Printf(TEXT("\"%s\""), *FPaths::GetProjectFilePath());
	}
	else
	{
		GameNameOrProjectFile = FApp::GetProjectName();
	}

	FString AdditionalParameters(TEXT(" -messaging -SessionName=\"Play in Standalone Game\""));
	if (FApp::IsRunningDebug())
	{
		AdditionalParameters += TEXT(" -debug");
	}

	const FString PreviewGameLanguage = FTextLocalizationManager::Get().GetConfiguredGameLocalizationPreviewLanguage();
	if (!PreviewGameLanguage.IsEmpty())
	{
		AdditionalParameters += TEXT(" -culture=");
		AdditionalParameters += PreviewGameLanguage;
	}

	// apply additional settings
	if (bPlayUsingMobilePreview)
	{
		if (PlayUsingMobilePreviewTargetDevice.IsEmpty() == false)
		{
			AdditionalParameters += TEXT(" -MobileTargetDevice=") + PlayUsingMobilePreviewTargetDevice;
		}
		else
		{
			AdditionalParameters += TEXT(" -featureleveles2");
		}

		if (IsOpenGLPlatform(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]))
		{
			AdditionalParameters += TEXT(" -opengl");
		}
		AdditionalParameters += TEXT(" -faketouches");
	}

	if (bPlayUsingVulkanPreview)
	{
		ensure(!bPlayUsingMobilePreview);
		AdditionalParameters += TEXT(" -vulkan -faketouches -featureleveles31");
	}

	// Disable the HMD device in the new process if present. The editor process owns the HMD resource.
	if (!bPlayUsingMobilePreview && !bPlayUsingVulkanPreview && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDConnected())
	{
		AdditionalParameters += TEXT(" -nohmd");
		UE_LOG(LogHMD, Warning, TEXT("Standalone game VR not supported, please use VR Preview."));
	}

	if (PlayInSettings->DisableStandaloneSound)
	{
		AdditionalParameters += TEXT(" -nosound");
	}

	if (PlayInSettings->AdditionalLaunchParameters.Len() > 0)
	{
		AdditionalParameters += TEXT(" ");
		AdditionalParameters += PlayInSettings->AdditionalLaunchParameters;
	}

	uint16 ServerPort = 0;
	if (bIsServer && PlayInSettings->GetServerPort(ServerPort))
	{
		AdditionalParameters += FString::Printf(TEXT(" -port=%hu"), ServerPort);
	}

	// Decide if fullscreen or windowed based on what is specified in the params
	if (!AdditionalParameters.Contains(TEXT("-fullscreen")) && !AdditionalParameters.Contains(TEXT("-windowed")))
	{
		// Nothing specified fallback to window otherwise keep what is specified
		AdditionalParameters += TEXT(" -windowed");		
	}

	if (RequestedAdditionalStandaloneLaunchOptions.Len() > 0)
	{
		AdditionalParameters += TEXT(" ");
		AdditionalParameters += RequestedAdditionalStandaloneLaunchOptions;
		//clear it now it's been used
		RequestedAdditionalStandaloneLaunchOptions = FString();
	}

	FIntPoint WinSize(0, 0);
	GetWindowSizeForInstanceType(WinSize, PlayInSettings);

	// Get desktop metrics
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

	// Force resolution
	if ((WinSize.X <= 0 || WinSize.X > DisplayMetrics.PrimaryDisplayWidth) || (WinSize.Y <= 0 || WinSize.Y > DisplayMetrics.PrimaryDisplayHeight))
	{
		AdditionalParameters += TEXT(" -ForceRes");
	}
	
	// Check if centered
	FString Params;
	if (PlayInSettings->CenterStandaloneWindow)
	{
		Params = FString::Printf(TEXT("%s %s -game -PIEVIACONSOLE -ResX=%d -ResY=%d %s%s %s"),
			*GameNameOrProjectFile,
			*BuildPlayWorldURL(*SavedMapNames[0], false, URLParms),
			WinSize.X,
			WinSize.Y,
			*FCommandLine::GetSubprocessCommandline(),
			*AdditionalParameters,
			*CmdLine
			);
	}
	else
	{
		FIntPoint WinPos(0, 0);

		Params = FString::Printf(TEXT("%s %s -game -PIEVIACONSOLE -WinX=%d -WinY=%d -ResX=%d -ResY=%d %s%s %s"),
			*GameNameOrProjectFile,
			*BuildPlayWorldURL(*SavedMapNames[0], false, URLParms),
			PlayInSettings->StandaloneWindowPosition.X,
			PlayInSettings->StandaloneWindowPosition.Y,
			WinSize.X,
			WinSize.Y,
			*FCommandLine::GetSubprocessCommandline(),
			*AdditionalParameters,
			*CmdLine
		);
	}

	// launch the game process
	FString GamePath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());
	FPlayOnPCInfo *NewSession = new (PlayOnLocalPCSessions) FPlayOnPCInfo();

	uint32 ProcessID = 0;
	NewSession->ProcessHandle = FPlatformProcess::CreateProc(*GamePath, *Params, true, false, false, &ProcessID, 0, NULL, NULL);

	if (!NewSession->ProcessHandle.IsValid())
	{
		UE_LOG(LogPlayLevel, Error, TEXT("Failed to run a copy of the game on this PC."));
	}

	FEditorDelegates::BeginStandaloneLocalPlay.Broadcast(ProcessID);
}

static void HandleOutputReceived(const FString& InMessage)
{
	UE_LOG(LogPlayLevel, Log, TEXT("%s"), *InMessage);
}

static void HandleCancelButtonClicked(ILauncherWorkerPtr LauncherWorker)
{
	if (LauncherWorker.IsValid())
	{
		LauncherWorker->Cancel();
	}
}

/* FMainFrameActionCallbacks callbacks
 *****************************************************************************/

class FLauncherNotificationTask
{
public:

	FLauncherNotificationTask( TWeakPtr<SNotificationItem> InNotificationItemPtr, SNotificationItem::ECompletionState InCompletionState, const FText& InText )
		: CompletionState(InCompletionState)
		, NotificationItemPtr(InNotificationItemPtr)
		, Text(InText)
	{ }

	void DoTask( ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent )
	{
		if (NotificationItemPtr.IsValid())
		{
			if (CompletionState == SNotificationItem::CS_Fail)
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
			}
			else
			{
				GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
			}

			TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
			NotificationItem->SetText(Text);
			NotificationItem->SetCompletionState(CompletionState);
			NotificationItem->ExpireAndFadeout();
		}
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread( ) { return ENamedThreads::GameThread; }
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FLauncherNotificationTask, STATGROUP_TaskGraphTasks);
	}

private:

	SNotificationItem::ECompletionState CompletionState;
	TWeakPtr<SNotificationItem> NotificationItemPtr;
	FText Text;
};


void UEditorEngine::HandleStageStarted(const FString& InStage, TWeakPtr<SNotificationItem> NotificationItemPtr)
{
	bool bSetNotification = true;
	FFormatNamedArguments Arguments;
	FText NotificationText;
	if (InStage.Contains(TEXT("Cooking")) || InStage.Contains(TEXT("Cook Task")))
	{
		FString PlatformName = PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@")));
		if (PlatformName.Contains(TEXT("NoEditor")))
		{
			PlatformName = PlatformName.Left(PlatformName.Find(TEXT("NoEditor")));
		}
		Arguments.Add(TEXT("PlatformName"), FText::FromString(PlatformName));
		NotificationText = FText::Format(LOCTEXT("LauncherTaskProcessingNotification", "Processing Assets for {PlatformName}..."), Arguments);
	}
	else if (InStage.Contains(TEXT("Build Task")))
	{
		FString PlatformName = PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@")));
		if (PlatformName.Contains(TEXT("NoEditor")))
		{
			PlatformName = PlatformName.Left(PlatformName.Find(TEXT("NoEditor")));
		}
		Arguments.Add(TEXT("PlatformName"), FText::FromString(PlatformName));
		if (!bPlayUsingLauncherBuild)
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskValidateNotification", "Validating Executable for {PlatformName}..."), Arguments);
		}
		else
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskBuildNotification", "Building Executable for {PlatformName}..."), Arguments);
		}
	}
	else if (InStage.Contains(TEXT("Deploy Task")))
	{
		Arguments.Add(TEXT("DeviceName"), FText::FromString(PlayUsingLauncherDeviceName));
		if (PlayUsingLauncherDeviceName.Len() == 0)
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskStageNotificationNoDevice", "Deploying Executable and Assets..."), Arguments);
		}
		else
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskStageNotification", "Deploying Executable and Assets to {DeviceName}..."), Arguments);
		}
	}
	else if (InStage.Contains(TEXT("Run Task")))
	{
		Arguments.Add(TEXT("GameName"), FText::FromString(FApp::GetProjectName()));
		Arguments.Add(TEXT("DeviceName"), FText::FromString(PlayUsingLauncherDeviceName));
		if (PlayUsingLauncherDeviceName.Len() == 0)
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskRunNotificationNoDevice", "Running {GameName}..."), Arguments);
		}
		else
		{
			NotificationText = FText::Format(LOCTEXT("LauncherTaskRunNotification", "Running {GameName} on {DeviceName}..."), Arguments);
		}
	}
	else
	{
		bSetNotification = false;
	}

	if (bSetNotification)
	{
		NotificationItemPtr.Pin()->SetText(NotificationText);
	}
}

void UEditorEngine::HandleStageCompleted(const FString& InStage, double StageTime, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr)
{
	UE_LOG(LogPlayLevel, Log, TEXT("Completed Launch On Stage: %s, Time: %f"), *InStage, StageTime);

	// analytics for launch on
	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), StageTime));
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("StageName"), InStage));
	FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.StageComplete" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bHasCode, ParamArray);
}

void UEditorEngine::HandleLaunchCanceled(double TotalTime, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr)
{
	TGraphTask<FLauncherNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
		NotificationItemPtr,
		SNotificationItem::CS_Fail,
		LOCTEXT("LaunchtaskFailedNotification", "Launch canceled!")
	);

	// analytics for launch on
	TArray<FAnalyticsEventAttribute> ParamArray;
	ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TotalTime));
	FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.Canceled" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bHasCode, ParamArray);

	bPlayUsingLauncher = false;	
}

void UEditorEngine::HandleLaunchCompleted(bool Succeeded, double TotalTime, int32 ErrorCode, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr, TSharedPtr<class FMessageLog> MessageLog)
{
	const FString DummyIOSDeviceName(FString::Printf(TEXT("All_iOS_On_%s"), FPlatformProcess::ComputerName()));
	const FString DummyTVOSDeviceName(FString::Printf(TEXT("All_tvOS_On_%s"), FPlatformProcess::ComputerName()));
	if (Succeeded)
	{
		FText CompletionMsg;
		if ((PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) == TEXT("IOS") && PlayUsingLauncherDeviceName.Contains(DummyIOSDeviceName)) ||
			(PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) == TEXT("TVOS") && PlayUsingLauncherDeviceName.Contains(DummyTVOSDeviceName)))
		{
			CompletionMsg = LOCTEXT("DeploymentTaskCompleted", "Deployment complete! Open the app on your device to launch.");
			TSharedPtr<SNotificationItem> NotificationItem = NotificationItemPtr.Pin();
//			NotificationItem->SetExpireDuration(30.0f);
		}
		else
		{
			CompletionMsg = LOCTEXT("LauncherTaskCompleted", "Launch complete!!");
		}

		TGraphTask<FLauncherNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Success,
			CompletionMsg
		);

		// analytics for launch on
		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TotalTime));
		FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.Completed" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bHasCode, ParamArray);

		UE_LOG(LogPlayLevel, Log, TEXT("Launch On Completed. Time: %f"), TotalTime);
	}
	else
	{
		FText CompletionMsg;
		if ((PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) == TEXT("IOS") && PlayUsingLauncherDeviceName.Contains(DummyIOSDeviceName)) ||
			(PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) == TEXT("TVOS") && PlayUsingLauncherDeviceName.Contains(DummyTVOSDeviceName)))
		{
			CompletionMsg = LOCTEXT("DeploymentTaskFailed", "Deployment failed!");
		}
		else
		{
			CompletionMsg = LOCTEXT("LauncherTaskFailed", "Launch failed!");
		}
		
		MessageLog->Error()
			->AddToken(FTextToken::Create(CompletionMsg))
			->AddToken(FTextToken::Create(FText::FromString(FEditorAnalytics::TranslateErrorCode(ErrorCode))));

		// flush log, because it won't be destroyed until the notification popup closes
		MessageLog->NumMessages(EMessageSeverity::Info);

		TGraphTask<FLauncherNotificationTask>::CreateTask().ConstructAndDispatchWhenReady(
			NotificationItemPtr,
			SNotificationItem::CS_Fail,
			CompletionMsg
		);

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), TotalTime));
		FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.Failed" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bHasCode, ErrorCode, ParamArray);
	}
	bPlayUsingLauncher = false;
}

static void HandleHyperlinkNavigate()
{
	FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog"));
}

struct FInternalPlayLevelUtils
{
	static int32 ResolveDirtyBlueprints(const bool bPromptForCompile, TArray<UBlueprint*>& ErroredBlueprints, const bool bForceLevelScriptRecompile = true)
	{
		const bool bAutoCompile = !bPromptForCompile;
		FString PromptDirtyList;

		TArray<UBlueprint*> InNeedOfRecompile;
		ErroredBlueprints.Empty();

		double BPRegenStartTime = FPlatformTime::Seconds();
		for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
		{
			UBlueprint* Blueprint = *BlueprintIt;

			// ignore up-to-date BPs
			if (Blueprint->IsUpToDate())
			{
				continue;
			}

			// do not try to recompile BPs that have not changed since they last failed to compile, so don't check Blueprint->IsUpToDate()
			const bool bIsDirtyAndShouldBeRecompiled = Blueprint->IsPossiblyDirty();
			if (!FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
				&& (bIsDirtyAndShouldBeRecompiled || (FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint) && bForceLevelScriptRecompile))
				&& (Blueprint->Status != BS_Unknown)
				&& !Blueprint->IsPendingKill())
			{
				InNeedOfRecompile.Add(Blueprint);

				if (bPromptForCompile)
				{
					PromptDirtyList += FString::Printf(TEXT("\n   %s"), *Blueprint->GetName());
				}
			}
			else if (BS_Error == Blueprint->Status && Blueprint->bDisplayCompilePIEWarning)
			{
				ErroredBlueprints.Add(Blueprint);
			}
		}

		bool bRunCompilation = bAutoCompile;
		if (bPromptForCompile)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("DirtyBlueprints"), FText::FromString(PromptDirtyList));
			const FText PromptMsg = FText::Format(NSLOCTEXT("PlayInEditor", "PrePIE_BlueprintsDirty", "One or more blueprints have been modified without being recompiled. Do you want to compile them now? \n{DirtyBlueprints}"), Args);

			EAppReturnType::Type PromptResponse = FMessageDialog::Open(EAppMsgType::YesNo, PromptMsg);
			bRunCompilation = (PromptResponse == EAppReturnType::Yes);
		}
		int32 RecompiledCount = 0;

		FMessageLog BlueprintLog("BlueprintLog");
		if (bRunCompilation && (InNeedOfRecompile.Num() > 0))
		{
			const FText LogPageLabel = (bAutoCompile) ? LOCTEXT("BlueprintAutoCompilationPageLabel", "Pre-Play auto-recompile") :
				LOCTEXT("BlueprintCompilationPageLabel", "Pre-Play recompile");
			BlueprintLog.NewPage(LogPageLabel);

			TArray<UBlueprint*> CompiledBlueprints;
			auto OnBlueprintPreCompileLambda = [&CompiledBlueprints](UBlueprint* InBlueprint)
			{
				check(InBlueprint != nullptr);

				if (CompiledBlueprints.Num() == 0)
				{
					UE_LOG(LogPlayLevel, Log, TEXT("[PlayLevel] Compiling %s before play..."), *InBlueprint->GetName());
				}
				else
				{
					UE_LOG(LogPlayLevel, Log, TEXT("[PlayLevel]   Compiling %s as a dependent..."), *InBlueprint->GetName());
				}

				CompiledBlueprints.Add(InBlueprint);
			};

			// Register compile callback
			FDelegateHandle PreCompileDelegateHandle = GEditor->OnBlueprintPreCompile().AddLambda(OnBlueprintPreCompileLambda);

			// Recompile all necessary blueprints in a single loop, saving GC until the end
			for (auto BlueprintIt = InNeedOfRecompile.CreateIterator(); BlueprintIt; ++BlueprintIt)
			{
				UBlueprint* Blueprint = *BlueprintIt;

				int32 CurrItIndex = BlueprintIt.GetIndex();

				// Compile the Blueprint (note: re-instancing may trigger additional compiles for child/dependent Blueprints; see callback above)
				FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);

				// Check for errors after compiling
				for (UBlueprint* CompiledBlueprint : CompiledBlueprints)
				{
					if (CompiledBlueprint != Blueprint)
					{
						int32 ExistingIndex = InNeedOfRecompile.Find(CompiledBlueprint);
						// if this dependent blueprint is already set up to compile 
						// later in this loop, then there is no need to add it to be recompiled again
						if (ExistingIndex > CurrItIndex)
						{
							InNeedOfRecompile.RemoveAt(ExistingIndex);
						}
					}

					const bool bHadError = (!CompiledBlueprint->IsUpToDate() && CompiledBlueprint->Status != BS_Unknown);

					// Check if the Blueprint has already been added to the error list to prevent it from being added again
					if (bHadError && ErroredBlueprints.Find(CompiledBlueprint) == INDEX_NONE)
					{
						ErroredBlueprints.Add(CompiledBlueprint);

						FFormatNamedArguments Arguments;
						Arguments.Add(TEXT("Name"), FText::FromString(CompiledBlueprint->GetName()));

						BlueprintLog.Info(FText::Format(LOCTEXT("BlueprintCompileFailed", "Blueprint {Name} failed to compile"), Arguments));
					}

					++RecompiledCount;
				}

				// Reset for next pass
				CompiledBlueprints.Empty();
			}

			// Now that all Blueprints have been compiled, run a single GC pass to clean up artifacts
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Unregister compile callback
			GEditor->OnBlueprintPreCompile().Remove(PreCompileDelegateHandle);

			UE_LOG(LogPlayLevel, Log, TEXT("PlayLevel: Blueprint regeneration took %d ms (%i blueprints)"), (int32)((FPlatformTime::Seconds() - BPRegenStartTime) * 1000), RecompiledCount);
		}
		else if (bAutoCompile)
		{
			UE_LOG(LogPlayLevel, Log, TEXT("PlayLevel: No blueprints needed recompiling"));
		}

		return RecompiledCount;
	}
};

FString UEditorEngine::GetPlayOnTargetPlatformName() const
{
	return PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@")));
}

void UEditorEngine::PlayUsingLauncher()
{
	if (!PlayUsingLauncherDeviceId.IsEmpty())
	{
		ILauncherServicesModule& LauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>(TEXT("LauncherServices"));
		ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");

		//if the device is not authorized to be launched to, we need to pop an error instead of trying to launch
		ITargetPlatform* LaunchPlatform = GetTargetPlatformManagerRef().FindTargetPlatform(PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))));
		if (LaunchPlatform != nullptr)
		{
			ITargetDevicePtr PlayDevice = LaunchPlatform->GetDefaultDevice();
			if (PlayDevice.IsValid() && !PlayDevice->IsAuthorized())
			{
				CancelRequestPlaySession();

				FText LaunchingText = LOCTEXT("LauncherTaskInProgressNotificationNotAuthorized", "Cannot launch to this device until this computer is authorized from the device");
				FNotificationInfo Info(LaunchingText);
				Info.ExpireDuration = 5.0f;
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if (Notification.IsValid())
				{
					Notification->SetCompletionState(SNotificationItem::CS_Fail);
					Notification->ExpireAndFadeout();
				}
				return;
			}
		}

		// create a temporary device group and launcher profile
		ILauncherDeviceGroupRef DeviceGroup = LauncherServicesModule.CreateDeviceGroup(FGuid::NewGuid(), TEXT("PlayOnDevices"));
		DeviceGroup->AddDevice(PlayUsingLauncherDeviceId);

		UE_LOG(LogPlayLevel, Log, TEXT("Launcher Device ID: %s"), *PlayUsingLauncherDeviceId);

		// does the project have any code?
		FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
		bPlayUsingLauncherHasCode = GameProjectModule.Get().ProjectRequiresBuild(FName(*PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@")))));

		const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
		// Setup launch profile, keep the setting here to a minimum.
		ILauncherProfileRef LauncherProfile = LauncherServicesModule.CreateProfile(TEXT("Launch On Device"));
		if(PlayInSettings->BuildGameBeforeLaunch == EPlayOnBuildMode::PlayOnBuild_Always)
		{
			bPlayUsingLauncherBuild = true;
		}
		else if(PlayInSettings->BuildGameBeforeLaunch == EPlayOnBuildMode::PlayOnBuild_Never)
		{
			bPlayUsingLauncherBuild = false;
		}
		else if(PlayInSettings->BuildGameBeforeLaunch == EPlayOnBuildMode::PlayOnBuild_Default)
		{
			bPlayUsingLauncherBuild = bPlayUsingLauncherHasCode || !FApp::GetEngineIsPromotedBuild();
		}
		else if(PlayInSettings->BuildGameBeforeLaunch == EPlayOnBuildMode::PlayOnBuild_IfEditorBuiltLocally)
		{
			bPlayUsingLauncherBuild = !FApp::GetEngineIsPromotedBuild();
		}
		LauncherProfile->SetBuildGame(bPlayUsingLauncherBuild);

		// set the build/launch configuration 
		switch (PlayInSettings->LaunchConfiguration)
		{
		case LaunchConfig_Debug:
			LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Debug);
			break;
		case LaunchConfig_Development:
			LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Development);
			break;
		case LaunchConfig_Test:
			LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Test);
			break;
		case LaunchConfig_Shipping:
			LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Shipping);
			break;
		default:
			// same as the running editor
			FString ExeName = FUnrealEdMisc::Get().GetExecutableForCommandlets();
			if (ExeName.Contains(TEXT("Debug")))
			{
				LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Debug);
			}
			else
			{
				LauncherProfile->SetBuildConfiguration(EBuildConfigurations::Development);
			}
			break;
		}

		// select the quickest cook mode based on which in editor cook mode is enabled
		bool bIncrimentalCooking = true;
		LauncherProfile->AddCookedPlatform(PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))));
		ELauncherProfileCookModes::Type CurrentLauncherCookMode = ELauncherProfileCookModes::ByTheBook;
		bool bCanCookByTheBookInEditor = true;
		bool bCanCookOnTheFlyInEditor = true;
		for ( const FString& PlatformName : LauncherProfile->GetCookedPlatforms() )
		{
			if ( CanCookByTheBookInEditor(PlatformName) == false )
			{
				bCanCookByTheBookInEditor = false;
			}
			if ( CanCookOnTheFlyInEditor(PlatformName)== false )
			{
				bCanCookOnTheFlyInEditor = false;
			}
		}
		if ( bCanCookByTheBookInEditor )
		{
			CurrentLauncherCookMode = ELauncherProfileCookModes::ByTheBookInEditor;
		}
		if ( bCanCookOnTheFlyInEditor )
		{
			CurrentLauncherCookMode = ELauncherProfileCookModes::OnTheFlyInEditor;
			bIncrimentalCooking = false;
		}
		LauncherProfile->SetCookMode( CurrentLauncherCookMode );
		LauncherProfile->SetUnversionedCooking(!bIncrimentalCooking);
		LauncherProfile->SetIncrementalCooking(bIncrimentalCooking);
		LauncherProfile->SetDeployedDeviceGroup(DeviceGroup);
		LauncherProfile->SetIncrementalDeploying(bIncrimentalCooking);
		LauncherProfile->SetEditorExe(FUnrealEdMisc::Get().GetExecutableForCommandlets());

		const FString DummyIOSDeviceName(FString::Printf(TEXT("All_iOS_On_%s"), FPlatformProcess::ComputerName()));
		const FString DummyTVOSDeviceName(FString::Printf(TEXT("All_tvOS_On_%s"), FPlatformProcess::ComputerName()));

		if ((PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) != TEXT("IOS") && PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))) != TEXT("TVOS")) ||
			(!PlayUsingLauncherDeviceName.Contains(DummyIOSDeviceName) && !PlayUsingLauncherDeviceName.Contains(DummyTVOSDeviceName)))
		{
			LauncherProfile->SetLaunchMode(ELauncherProfileLaunchModes::DefaultRole);
		}

		if ( LauncherProfile->GetCookMode() == ELauncherProfileCookModes::OnTheFlyInEditor || LauncherProfile->GetCookMode() == ELauncherProfileCookModes::OnTheFly )
		{
			LauncherProfile->SetDeploymentMode(ELauncherProfileDeploymentModes::FileServer);
		}

		TArray<UBlueprint*> ErroredBlueprints;
		FInternalPlayLevelUtils::ResolveDirtyBlueprints(!PlayInSettings->bAutoCompileBlueprintsOnLaunch, ErroredBlueprints, false);

		TArray<FString> MapNames;
		FWorldContext & EditorContext = GetEditorWorldContext();

		// Load maps in place as we saved them above
		FString EditorMapName = EditorContext.World()->GetOutermost()->GetName();
		MapNames.Add(EditorMapName);
	
		FString InitialMapName;
		if (MapNames.Num() > 0)
		{
			InitialMapName = MapNames[0];
		}

		LauncherProfile->GetDefaultLaunchRole()->SetInitialMap(InitialMapName);

		for (const FString& MapName : MapNames)
		{
			LauncherProfile->AddCookedMap(MapName);
		}

		if ( LauncherProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor )
		{
			TArray<ITargetPlatform*> TargetPlatforms;
			for ( const FString& PlatformName : LauncherProfile->GetCookedPlatforms() )
			{
				ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(PlatformName);
				// todo pass in all the target platforms instead of just the single platform
				// crashes if two requests are inflight but we can support having multiple platforms cooking at once
				TargetPlatforms.Add( TargetPlatform ); 
			}
			const TArray<FString> &CookedMaps = LauncherProfile->GetCookedMaps();

			// const TArray<FString>& CookedMaps = ChainState.Profile->GetCookedMaps();
			TArray<FString> CookDirectories;
			TArray<FString> IniMapSections;

			StartCookByTheBookInEditor(TargetPlatforms, CookedMaps, CookDirectories, GetDefault<UProjectPackagingSettings>()->CulturesToStage, IniMapSections );

			FIsCookFinishedDelegate &CookerFinishedDelegate = LauncherProfile->OnIsCookFinished();

			CookerFinishedDelegate.BindUObject(this, &UEditorEngine::IsCookByTheBookInEditorFinished);

			FCookCanceledDelegate &CookCancelledDelegate = LauncherProfile->OnCookCanceled();

			CookCancelledDelegate.BindUObject(this, &UEditorEngine::CancelCookByTheBookInEditor);
		}

		ILauncherPtr Launcher = LauncherServicesModule.CreateLauncher();
		GEditor->LauncherWorker = Launcher->Launch(TargetDeviceServicesModule.GetDeviceProxyManager(), LauncherProfile);

		// create notification item
		FText LaunchingText = LOCTEXT("LauncherTaskInProgressNotificationNoDevice", "Launching...");
		FNotificationInfo Info(LaunchingText);

		Info.Image = FEditorStyle::GetBrush(TEXT("MainFrame.CookContent"));
		Info.bFireAndForget = false;
		Info.ExpireDuration = 10.0f;
		Info.Hyperlink = FSimpleDelegate::CreateStatic(HandleHyperlinkNavigate);
		Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
		Info.ButtonDetails.Add(
			FNotificationButtonInfo(
				LOCTEXT("LauncherTaskCancel", "Cancel"),
				LOCTEXT("LauncherTaskCancelToolTip", "Cancels execution of this task."),
				FSimpleDelegate::CreateStatic(HandleCancelButtonClicked, GEditor->LauncherWorker)
			)
		);

		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

		if (!NotificationItem.IsValid())
		{
			return;
		}

		// analytics for launch on
		int32 ErrorCode = 0;
		FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.Started" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bPlayUsingLauncherHasCode);

		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		
		TWeakPtr<SNotificationItem> NotificationItemPtr(NotificationItem);
		if (GEditor->LauncherWorker.IsValid() && GEditor->LauncherWorker->GetStatus() != ELauncherWorkerStatus::Completed)
		{
			TSharedPtr<FMessageLog> MessageLog = MakeShareable(new FMessageLog("PackagingResults"));

			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));
			GEditor->LauncherWorker->OnOutputReceived().AddStatic(HandleOutputReceived);
			GEditor->LauncherWorker->OnStageStarted().AddUObject(this, &UEditorEngine::HandleStageStarted, NotificationItemPtr);
			GEditor->LauncherWorker->OnStageCompleted().AddUObject(this, &UEditorEngine::HandleStageCompleted, bPlayUsingLauncherHasCode, NotificationItemPtr);
			GEditor->LauncherWorker->OnCompleted().AddUObject(this, &UEditorEngine::HandleLaunchCompleted, bPlayUsingLauncherHasCode, NotificationItemPtr, MessageLog);
			GEditor->LauncherWorker->OnCanceled().AddUObject(this, &UEditorEngine::HandleLaunchCanceled, bPlayUsingLauncherHasCode, NotificationItemPtr);
		}
		else
		{
			GEditor->LauncherWorker.Reset();
			GEditor->PlayEditorSound(TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));

			NotificationItem->SetText(LOCTEXT("LauncherTaskFailedNotification", "Failed to launch task!"));
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->ExpireAndFadeout();
			bPlayUsingLauncher = false;

			// analytics for launch on
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), 0.0));
			FEditorAnalytics::ReportEvent(TEXT( "Editor.LaunchOn.Failed" ), PlayUsingLauncherDeviceId.Left(PlayUsingLauncherDeviceId.Find(TEXT("@"))), bPlayUsingLauncherHasCode, EAnalyticsErrorCodes::LauncherFailed, ParamArray );
		}
	}
}


void UEditorEngine::RequestEndPlayMap()
{
	if( PlayWorld )
	{
		bRequestEndPlayMapQueued = true;

		// Cache the position and rotation of the camera (the controller may be destroyed before we end the pie session and we need them to preserve the camera position)
		if (bLastViewAndLocationValid == false)
		{
			for (int32 WorldIdx = WorldList.Num() - 1; WorldIdx >= 0; --WorldIdx)
			{
				FWorldContext &ThisContext = WorldList[WorldIdx];
				if (ThisContext.WorldType == EWorldType::PIE)
				{
					FSlatePlayInEditorInfo* const SlatePlayInEditorSession = SlatePlayInEditorMap.Find(ThisContext.ContextHandle);
					if ((SlatePlayInEditorSession != nullptr) && (SlatePlayInEditorSession->EditorPlayer.IsValid() == true) )
					{
						if( SlatePlayInEditorSession->EditorPlayer.Get()->PlayerController != nullptr )
						{
							SlatePlayInEditorSession->EditorPlayer.Get()->PlayerController->GetPlayerViewPoint( LastViewLocation, LastViewRotation );
							bLastViewAndLocationValid = true;
							break;
						}
					}
				}
			}
		}
	}
}

FString UEditorEngine::BuildPlayWorldURL(const TCHAR* MapName, bool bSpectatorMode, FString AdditionalURLOptions)
{
	// the URL we are building up
	FString URL(MapName);

	// If we hold down control, start in spectating mode
	if (bSpectatorMode)
	{
		// Start in spectator mode
		URL += TEXT("?SpectatorOnly=1");
	}

	// Add any game-specific options set in the INI file
	URL += InEditorGameURLOptions;

	// Add any additional options that were specified for this call
	URL += AdditionalURLOptions;

	// Add any additional options that are set in the Play In Settings menu
	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
	FString AdditionalServerGameOptions;
	if(PlayInSettings->GetAdditionalServerGameOptions(AdditionalServerGameOptions))
	{
		URL += *AdditionalServerGameOptions;
	}

	return URL;
}

bool UEditorEngine::SpawnPlayFromHereStart( UWorld* World, AActor*& PlayerStart, const FVector& StartLocation, const FRotator& StartRotation )
{
	// null it out in case we don't need to spawn one, and the caller relies on us setting it
	PlayerStart = NULL;

	if( bHasPlayWorldPlacement )
	{
		// spawn the PlayerStartPIE in the given world
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.OverrideLevel = World->PersistentLevel;
		PlayerStart = World->SpawnActor<AActor>(PlayFromHerePlayerStartClass, StartLocation, StartRotation, SpawnParameters);

		// make sure we were able to spawn the PlayerStartPIE there
		if(!PlayerStart)
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Prompt_22", "Failed to create entry point. Try another location, or you may have to rebuild your level."));
			return false;
		}
		// tag the start
		ANavigationObjectBase* NavPlayerStart = Cast<ANavigationObjectBase>(PlayerStart);
		if (NavPlayerStart)
		{
			NavPlayerStart->bIsPIEPlayerStart = true;
		}
	}
	// true means we didn't need to spawn, or we succeeded
	return true;
}

void UEditorEngine::PlayInEditor( UWorld* InWorld, bool bInSimulateInEditor )
{
	// Broadcast PreBeginPIE before checks that might block PIE below (BeginPIE is broadcast below after the checks)
	FEditorDelegates::PreBeginPIE.Broadcast(bInSimulateInEditor);

	double PIEStartTime = FPlatformTime::Seconds();

	// Block PIE when there is a transaction recording into the undo buffer
	if (GEditor->IsTransactionActive())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("TransactionName"), GEditor->GetTransactionName());

		FText NotificationText;
		if (bInSimulateInEditor)
		{
			NotificationText = FText::Format(NSLOCTEXT("UnrealEd", "SIECantStartDuringTransaction", "Can't Simulate when performing {TransactionName} operation"), Args);
		}
		else
		{
			NotificationText = FText::Format(NSLOCTEXT("UnrealEd", "PIECantStartDuringTransaction", "Can't Play In Editor when performing {TransactionName} operation"), Args);
		}

		FNotificationInfo Info(NotificationText);
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = true;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Prompt the user that Matinee must be closed before PIE can occur.
	if( GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit) )
	{
		const bool bContinuePIE = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "PIENeedsToCloseMatineeQ", "'Play in Editor' must close UnrealMatinee.  Continue?") );
		if ( !bContinuePIE )
		{
			return;
		}
		GLevelEditorModeTools().DeactivateMode( FBuiltinEditorModes::EM_InterpEdit );
	}

	// Make sure there's no outstanding load requests
	FlushAsyncLoading();

	FBlueprintEditorUtils::FindAndSetDebuggableBlueprintInstances();

	// Monitoring when PIE corrupts references between the World and the PIE generated World for UE-20486
	{
		TArray<ULevel*> Levels = InWorld->GetLevels();

		for (ULevel* Level : Levels)
		{
			TArray<UBlueprint*> LevelBlueprints = Level->GetLevelBlueprints();

			if (LevelBlueprints.Num() > 0)
			{
				UBlueprint* LevelScriptBlueprint = LevelBlueprints[0];
				if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass && LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s' with ClassGeneratedBy '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName(), *LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy->GetPathName());
				}
				else if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName());
				}
				else if (LevelScriptBlueprint)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Early PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName());
				}
			}
		}
	}

	// Broadcast BeginPIE after checks that might block PIE above (PreBeginPIE is broadcast above before the checks)
	FEditorDelegates::BeginPIE.Broadcast(bInSimulateInEditor);

	// let navigation know PIE starts so it can avoid any blueprint creation/deletion/instantiation affect editor map's navmesh changes
	if (InWorld->GetNavigationSystem())
	{
		InWorld->GetNavigationSystem()->OnPIEStart();
	}

	ULevelEditorPlaySettings* PlayInSettings = GetMutableDefault<ULevelEditorPlaySettings>();
	check(PlayInSettings);

	TArray<UBlueprint*> ErroredBlueprints;
	FInternalPlayLevelUtils::ResolveDirtyBlueprints(!PlayInSettings->AutoRecompileBlueprints, ErroredBlueprints);

	if (ErroredBlueprints.Num() && !GIsDemoMode)
	{
		FString ErroredBlueprintList;
		for (UBlueprint* Blueprint : ErroredBlueprints)
		{
			ErroredBlueprintList += FString::Printf(TEXT("\n   %s"), *Blueprint->GetName());
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("ErrorBlueprints"), FText::FromString(ErroredBlueprintList));

		// There was at least one blueprint with an error, make sure the user is OK with that.
		const bool bContinuePIE = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, FText::Format( NSLOCTEXT("PlayInEditor", "PrePIE_BlueprintErrors", "One or more blueprints has an unresolved compiler error, are you sure you want to Play in Editor?{ErrorBlueprints}"), Args ) );
		if ( !bContinuePIE )
		{
			FEditorDelegates::EndPIE.Broadcast(bInSimulateInEditor);
			if (InWorld->GetNavigationSystem())
			{
				InWorld->GetNavigationSystem()->OnPIEEnd();
			}

			return;
		}
		else
		{
			// The user wants to ignore the compiler errors, mark the Blueprints and do not warn them again unless the Blueprint attempts to compile
			for (UBlueprint* Blueprint : ErroredBlueprints)
			{
				Blueprint->bDisplayCompilePIEWarning = false;
			}
		}
	}


	const FScopedBusyCursor BusyCursor;

	// If there's level already being played, close it. (This may change GWorld)
	if(PlayWorld)
	{
		// immediately end the playworld
		EndPlayMap();
	}

	// Register for log processing so we can promote errors/warnings to the message log
	if (GetDefault<UEditorStyleSettings>()->bPromoteOutputLogWarningsDuringPIE)
	{
		OutputLogErrorsToMessageLogProxyPtr = MakeShareable(new FOutputLogErrorsToMessageLogProxy());
	}

	if (GEngine->XRSystem.IsValid() && !bInSimulateInEditor)
	{
		GEngine->XRSystem->OnBeginPlay(*GEngine->GetWorldContextFromWorld(InWorld));
	}

	// remember old GWorld
	EditorWorld = InWorld;

	// Clear any messages from last time
	GEngine->ClearOnScreenDebugMessages();

	// Flush all audio sources from the editor world
	if (FAudioDevice* AudioDevice = EditorWorld->GetAudioDevice())
	{
		AudioDevice->Flush(EditorWorld);
		AudioDevice->ResetInterpolation();
		AudioDevice->OnBeginPIE(bInSimulateInEditor);
	}
	EditorWorld->bAllowAudioPlayback = false;

	if (!GEditor->bAllowMultiplePIEWorlds)
	{
		PlayInSettings->SetRunUnderOneProcess( false );
	}

	EPlayNetMode PlayNetMode( PIE_Standalone );
	PlayInSettings->GetPlayNetMode(PlayNetMode);	// Ignore disabled state here
	const EPlayNetMode OrigPlayNetMode( PlayNetMode );

	bool CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();
	if (CanRunUnderOneProcess)
	{
		const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
		const int32 PlayNumberOfClients = [&PlayInSettings]{ int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();
		const bool WillAutoConnectToServer = [&PlayInSettings]{ bool AutoConnectToServer(false); return (PlayInSettings->GetAutoConnectToServer(AutoConnectToServer) && AutoConnectToServer); }();
		if (!CanPlayNetDedicated && (PlayNumberOfClients == 1 || !WillAutoConnectToServer))
		{
			// Since we don't expose PlayNetMode as an option when doing RunUnderOnProcess,
			// we take 1 player and !PlayNetdedicated and being standalone.
			// If auto connect is off, launch as standalone unless there is a dedicated server
			PlayNetMode = EPlayNetMode::PIE_Standalone;
		}
		else
		{
			// We are doing multi-player under one process so make sure the NetMode is ListenServer
			PlayNetMode = EPlayNetMode::PIE_ListenServer;
		}
		PlayInSettings->SetPlayNetMode(PlayNetMode);
	}


	bool bAnyBlueprintErrors = ErroredBlueprints.Num() ? true : false;
	bool bStartInSpectatorMode = false;
	bool bSupportsOnlinePIE = false;
	const int32 PlayNumberOfClients = [&PlayInSettings]{ int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();

	// Can't allow realtime viewports whilst in PIE so disable it for ALL viewports here.
	DisableRealtimeViewports();

	// Online PIE is disabled in SIE
	if (SupportsOnlinePIE() && !bInSimulateInEditor)
	{
		bool bHasRequiredLogins = PlayNumberOfClients <= UOnlineEngineInterface::Get()->GetNumPIELogins();
		if (bHasRequiredLogins)
		{
			// If we support online PIE use it even if we're standalone
			bSupportsOnlinePIE = true;
		}
		else
		{
			FText ErrorMsg = LOCTEXT("PIELoginFailure", "Not enough login credentials to launch all PIE instances, change editor settings");
			UE_LOG(LogPlayLevel, Verbose, TEXT("%s"), *ErrorMsg.ToString());
			FMessageLog(NAME_CategoryPIE).Warning(ErrorMsg);
		}
	}

	UOnlineEngineInterface::Get()->SetShouldTryOnlinePIE(bSupportsOnlinePIE);

	FModifierKeysState KeysState = FSlateApplication::Get().GetModifierKeys();
	if (bInSimulateInEditor || KeysState.IsControlDown())
	{
		// if control is pressed, start in spectator mode
		bStartInSpectatorMode = true;
	}

	CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();
	if (bInSimulateInEditor || (PlayNetMode == EPlayNetMode::PIE_Standalone && PlayNumberOfClients <= 1 && !bSupportsOnlinePIE) || !CanRunUnderOneProcess)
	{
		// Only spawning 1 PIE instance under this process, only set the PIEInstance value if we're not connecting to another local instance of the game, otherwise it will run the wrong streaming levels
		PIEInstance = ( !CanRunUnderOneProcess && PlayNetMode == EPlayNetMode::PIE_Client ) ? INDEX_NONE : 0;
		UGameInstance* const GameInstance = CreatePIEGameInstance(PIEInstance, bInSimulateInEditor, bAnyBlueprintErrors, bStartInSpectatorMode, false, PIEStartTime);

		if (!PlayInSettings->EnableGameSound)
		{
			UWorld* GameInstanceWorld = GameInstance->GetWorld();
			if (FAudioDevice* GameInstanceAudioDevice = GameInstanceWorld->GetAudioDevice())
			{
				GameInstanceAudioDevice->SetTransientMasterVolume(0.0f);
			}
		}

		if (bInSimulateInEditor)
		{
			ToggleBetweenPIEandSIE( true );
		}
	}
	else
	{
		if (bSupportsOnlinePIE)
		{
			// Make sure all instances of PIE are logged in before creating/launching worlds
			LoginPIEInstances(bAnyBlueprintErrors, bStartInSpectatorMode, PIEStartTime);
		}
		else
		{
			// Normal, non-online creation/launching of worlds
			SpawnIntraProcessPIEWorlds(bAnyBlueprintErrors, bStartInSpectatorMode);
		}
	}

	PlayInSettings->MultipleInstanceLastHeight = PlayInSettings->NewWindowHeight;
	PlayInSettings->MultipleInstanceLastWidth = PlayInSettings->NewWindowWidth;
	PlayInSettings->SetPlayNetMode(OrigPlayNetMode);

	// Monitoring when PIE corrupts references between the World and the PIE generated World for UE-20486
	if (EditorWorld)
	{
		TArray<ULevel*> Levels = EditorWorld->GetLevels();

		for (ULevel* Level : Levels)
		{
			TArray<UBlueprint*> LevelBlueprints = Level->GetLevelBlueprints();

			if (LevelBlueprints.Num() > 0)
			{
				UBlueprint* LevelScriptBlueprint = LevelBlueprints[0];
				if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass && LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s' with ClassGeneratedBy '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName(), *LevelScriptBlueprint->GeneratedClass->ClassGeneratedBy->GetPathName());
				}
				else if (LevelScriptBlueprint && LevelScriptBlueprint->GeneratedClass)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s' with GeneratedClass '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName(), *LevelScriptBlueprint->GeneratedClass->GetPathName());
				}
				else if (LevelScriptBlueprint)
				{
					UE_LOG(LogBlueprintUserMessages, Log, TEXT("Late PlayInEditor Detection: Level '%s' has LevelScriptBlueprint '%s'"), *Level->GetPathName(), *LevelScriptBlueprint->GetPathName());
				}
			}
		}
	}

	// Make sure to focus the game viewport.
	if (!bInSimulateInEditor)
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}

	FEditorDelegates::PostPIEStarted.Broadcast( bInSimulateInEditor );
}

void UEditorEngine::SpawnIntraProcessPIEWorlds(bool bAnyBlueprintErrors, bool bStartInSpectatorMode)
{
	double PIEStartTime = FPlatformTime::Seconds();
	bStartLateJoinersInSpectatorMode = bStartInSpectatorMode;

	// Has to be false or this function wouldn't be called
	bool bInSimulateInEditor = false;
	ULevelEditorPlaySettings* PlayInSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());

	// Spawning multiple PIE instances
	if (PlayInSettings->MultipleInstancePositions.Num() == 0)
	{
		PlayInSettings->MultipleInstancePositions.SetNum(1);
	}

	PlayInSettings->MultipleInstancePositions[0] = PlayInSettings->NewWindowPosition;

	int32 NextX = 0;
	int32 NextY = 0;
	SettingsIndex = 1;
	int32 ClientNum = 0;

	PIEInstance = 1;
	const bool WillAutoConnectToServer = [&PlayInSettings]{ bool AutoConnectToServer(false); return (PlayInSettings->GetAutoConnectToServer(AutoConnectToServer) && AutoConnectToServer); }();
	const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();

	// Server
	if (CanPlayNetDedicated || WillAutoConnectToServer)
	{
		PlayInSettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);

		if (!CanPlayNetDedicated)
		{
			ClientNum++;
			GetMultipleInstancePositions(SettingsIndex++, NextX, NextY);
		}

		UGameInstance* const ServerGameInstance = CreatePIEGameInstance(PIEInstance, bInSimulateInEditor, bAnyBlueprintErrors, bStartInSpectatorMode, CanPlayNetDedicated, PIEStartTime);
		if (ServerGameInstance)
		{
			ServerPrefix = ServerGameInstance->GetWorldContext()->PIEPrefix;
		}
		else
		{
			// Failed, abort
			return;
		}

		PIEInstance++;
	}

	// Clients
	const int32 PlayNumberOfClients = [&PlayInSettings]{ int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();
	for (; ClientNum < PlayNumberOfClients; ++ClientNum)
	{
		// Only launch as clients if they should connect
		if (WillAutoConnectToServer)
		{
			PlayInSettings->SetPlayNetMode(EPlayNetMode::PIE_Client);
		}
		else
		{
			PlayInSettings->SetPlayNetMode(EPlayNetMode::PIE_Standalone);
		}

		// For legacy reasons, single player PIE uses ULevelEditorPlaySettings::NewWindowPosition as its window position.
		// Multiple PIE uses the ULevelEditorPlaySettings::MultipleInstancePositions array, starting with index 1.
		// If this is a single player PIE, with dedicated server, don't set NewWindowPosition from the MultipleInstancePositions array - leave it as is.
		if (PlayNumberOfClients > 1)
		{
			GetMultipleInstancePositions(SettingsIndex++, NextX, NextY);
		}

		UGameInstance* const ClientGameInstance = CreatePIEGameInstance(PIEInstance, bInSimulateInEditor, bAnyBlueprintErrors, bStartInSpectatorMode, false, PIEStartTime);
		if (!ClientGameInstance)
		{
			// Failed, abort
			return;
		}

		PIEInstance++;
	}

	// Restore window settings
	GetMultipleInstancePositions(0, NextX, NextY);	// restore cached settings

	// Give focus to the first client
	GiveFocusToFirstClientPIEViewport();
}

bool UEditorEngine::CreatePIEWorldFromLogin(FWorldContext& PieWorldContext, EPlayNetMode PlayNetMode, FPieLoginStruct& DataStruct)
{
	ULevelEditorPlaySettings* PlayInSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());
	PlayInSettings->SetPlayNetMode(PlayNetMode);

	// Set window position
	GetMultipleInstancePositions(DataStruct.SettingsIndex, DataStruct.NextX, DataStruct.NextY);
	
	const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
	const bool ActAsClient = PlayNetMode == EPlayNetMode::PIE_Client || PlayNetMode == EPlayNetMode::PIE_Standalone;
	UGameInstance* const GameInstance = CreatePIEGameInstance(PieWorldContext.PIEInstance, false, DataStruct.bAnyBlueprintErrors, DataStruct.bStartInSpectatorMode, ActAsClient ? false : CanPlayNetDedicated, DataStruct.PIEStartTime);
	
	// Restore window settings
	GetMultipleInstancePositions(0, DataStruct.NextX, DataStruct.NextY);	// restore cached settings

	if (GameInstance)
	{
		GameInstance->GetWorldContext()->bWaitingOnOnlineSubsystem = false;

		return true;
	}
	else
	{
		return false;
	}
}

void UEditorEngine::CancelPlayingViaLauncher()
{
	if (LauncherWorker.IsValid())
	{
		LauncherWorker->CancelAndWait();
	}
}

bool UEditorEngine::SupportsOnlinePIE() const
{
	return UOnlineEngineInterface::Get()->SupportsOnlinePIE();
}

void UEditorEngine::LoginPIEInstances(bool bAnyBlueprintErrors, bool bStartInSpectatorMode, double PIEStartTime)
{
	ULevelEditorPlaySettings* PlayInSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());
	SettingsIndex = 0;

	/** Setup the common data values for each login instance */
	FPieLoginStruct DataStruct;
	DataStruct.SettingsIndex = ++SettingsIndex;
	DataStruct.bAnyBlueprintErrors = bAnyBlueprintErrors;
	DataStruct.bStartInSpectatorMode = bStartInSpectatorMode;
	DataStruct.PIEStartTime = PIEStartTime;

	ensure(UOnlineEngineInterface::Get()->GetNumPIELogins() > 0);

	int32 ClientNum = 0;
	PIEInstance = 1;
	int32 NextX = 0;
	int32 NextY = 0;

	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
	const bool WillAutoConnectToServer = [&PlayInSettings]{ bool AutoConnectToServer(false); return (PlayInSettings->GetAutoConnectToServer(AutoConnectToServer) && AutoConnectToServer); }();
	const int32 PlayNumberOfClients = [&PlayInSettings] { int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();

	PIEInstancesToLogInCount = PlayNumberOfClients;

	// Server
	if (WillAutoConnectToServer || CanPlayNetDedicated)
	{
		FWorldContext &PieWorldContext = CreateNewWorldContext(EWorldType::PIE);
		PieWorldContext.PIEInstance = PIEInstance++;
		PieWorldContext.RunAsDedicated = CanPlayNetDedicated;
		PieWorldContext.bWaitingOnOnlineSubsystem = true;

		// Update login struct parameters
		DataStruct.WorldContextHandle = PieWorldContext.ContextHandle;
		DataStruct.NetMode = PlayNetMode;

		// Always get the interface (it will create the subsystem regardless)
		FName OnlineIdentifier = UOnlineEngineInterface::Get()->GetOnlineIdentifier(PieWorldContext);
		UE_LOG(LogPlayLevel, Display, TEXT("Creating online subsystem for server %s"), *OnlineIdentifier.ToString());
		NumOnlinePIEInstances++;

		if (!CanPlayNetDedicated)
		{
			DataStruct.NextX = NextX;
			DataStruct.NextY = NextY;
			GetMultipleInstancePositions(DataStruct.SettingsIndex, NextX, NextY);

			// Login to online platform before creating world
			FOnPIELoginComplete Delegate;
			Delegate.BindUObject(this, &UEditorEngine::OnLoginPIEComplete, DataStruct);

			// Login first and continue the flow later
			UOnlineEngineInterface::Get()->LoginPIEInstance(OnlineIdentifier, 0, ClientNum, Delegate);

			ClientNum++;
		}
		else
		{
			// Dedicated servers don't use a login
			UOnlineEngineInterface::Get()->SetForceDedicated(OnlineIdentifier, true);
			if (CreatePIEWorldFromLogin(PieWorldContext, EPlayNetMode::PIE_ListenServer, DataStruct))
			{
				FMessageLog(NAME_CategoryPIE).Info(LOCTEXT("LoggingInDedicated", "Dedicated Server logged in"));
			}
			else
			{
				// Failed to create world, this creates a dialog elsewhere
				return;
			}
		}
	}

	// Clients
	for (; ClientNum < PlayNumberOfClients; ++ClientNum)
	{
		PlayInSettings->SetPlayNetMode(PlayNetMode);
		FWorldContext &PieWorldContext = CreateNewWorldContext(EWorldType::PIE);
		PieWorldContext.PIEInstance = PIEInstance++;
		PieWorldContext.bWaitingOnOnlineSubsystem = true;

		// Update login struct parameters
		DataStruct.WorldContextHandle = PieWorldContext.ContextHandle;
		DataStruct.SettingsIndex = ++SettingsIndex;
		DataStruct.NextX = NextX;
		DataStruct.NextY = NextY;
		GetMultipleInstancePositions(DataStruct.SettingsIndex, NextX, NextY);
		DataStruct.NetMode = WillAutoConnectToServer ? EPlayNetMode::PIE_Client : EPlayNetMode::PIE_Standalone;

		FName OnlineIdentifier = UOnlineEngineInterface::Get()->GetOnlineIdentifier(PieWorldContext);
		UE_LOG(LogPlayLevel, Display, TEXT("Creating online subsystem for client %s"), *OnlineIdentifier.ToString());
		NumOnlinePIEInstances++;

		// Login to online platform before creating world
		FOnPIELoginComplete Delegate;
		Delegate.BindUObject(this, &UEditorEngine::OnLoginPIEComplete, DataStruct);

		UOnlineEngineInterface::Get()->LoginPIEInstance(OnlineIdentifier, 0, ClientNum, Delegate);
	}

	// Restore window settings
	GetMultipleInstancePositions(0, NextX, NextY);	// restore cached settings
}

void UEditorEngine::OnLoginPIEComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ErrorString, FPieLoginStruct DataStruct)
{
	// This is needed because pie login may change the state of the online objects that called this function
	GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UEditorEngine::OnLoginPIEComplete_Deferred, LocalUserNum, bWasSuccessful, ErrorString, DataStruct));
}

void UEditorEngine::OnLoginPIEComplete_Deferred(int32 LocalUserNum, bool bWasSuccessful, FString ErrorString, FPieLoginStruct DataStruct)
{
	UE_LOG(LogPlayLevel, Verbose, TEXT("OnLoginPIEComplete LocalUserNum: %d bSuccess: %d %s"), LocalUserNum, bWasSuccessful, *ErrorString);
	FWorldContext* PieWorldContext = GetWorldContextFromHandle(DataStruct.WorldContextHandle);

	if (!PieWorldContext)
	{
		// This will fail if PIE was ended before this callback happened, silently return
		return;
	}

	// Create the new world
	if (CreatePIEWorldFromLogin(*PieWorldContext, DataStruct.NetMode, DataStruct))
	{
		// Logging after the create so a new MessageLog Page is created
		if (bWasSuccessful)
		{
			if (DataStruct.NetMode != EPlayNetMode::PIE_Client)
			{
				FMessageLog(NAME_CategoryPIE).Info(LOCTEXT("LoggedInServer", "Server logged in"));
			}
			else
			{
				FMessageLog(NAME_CategoryPIE).Info(LOCTEXT("LoggedInClient", "Client logged in"));
			}
		}
		else
		{
			if (DataStruct.NetMode != EPlayNetMode::PIE_Client)
			{
				FMessageLog(NAME_CategoryPIE).Warning(LOCTEXT("LoggedInServerFailure", "Server failed to login"));
			}
			else
			{
				FMessageLog(NAME_CategoryPIE).Warning(LOCTEXT("LoggedInClientFailure", "Client failed to login"));
			}
		}
	}

	PIEInstancesToLogInCount--;
	if (PIEInstancesToLogInCount == 0)
	{
		OnLoginPIEAllComplete();
	}
}

void UEditorEngine::OnLoginPIEAllComplete()
{
	GiveFocusToFirstClientPIEViewport();
}

void UEditorEngine::GiveFocusToFirstClientPIEViewport()
{
	// Find the non-dedicated server or first client window to give focus to
	int32 LowestPIEInstance = TNumericLimits<int32>::Max();
	UGameViewportClient* ViewportClient = nullptr;
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE && !WorldContext.RunAsDedicated)
		{
			if (WorldContext.PIEInstance < LowestPIEInstance)
			{
				LowestPIEInstance = WorldContext.PIEInstance;
				ViewportClient = WorldContext.GameViewport;
			}
		}
	}

	// Give focus to the first client
	if (ViewportClient && ViewportClient->GetGameViewportWidget().IsValid())
	{
		FSlateApplication::Get().RegisterGameViewport(ViewportClient->GetGameViewportWidget().ToSharedRef());
	}
}

void UEditorEngine::RequestLateJoin()
{
	bool bSupportsOnlinePIE = false;

	if (SupportsOnlinePIE())
	{
// 		bool bHasRequiredLogins = PlayNumberOfClients <= UOnlineEngineInterface::Get()->GetNumPIELogins();
// 		if (bHasRequiredLogins)
		{
			// If we support online PIE use it even if we're standalone
			bSupportsOnlinePIE = true;
		}
	}

	ULevelEditorPlaySettings* PlayInSettings = Cast<ULevelEditorPlaySettings>(ULevelEditorPlaySettings::StaticClass()->GetDefaultObject());
	double PIEStartTime = FPlatformTime::Seconds();
	int32 NextX = 0;
	int32 NextY = 0;

	PlayInSettings->SetPlayNetMode(EPlayNetMode::PIE_Client);


	if (bSupportsOnlinePIE)
	{
		FPieLoginStruct DataStruct;
		DataStruct.bAnyBlueprintErrors = false;
		DataStruct.bStartInSpectatorMode = bStartLateJoinersInSpectatorMode;
		DataStruct.PIEStartTime = PIEStartTime;

		FWorldContext &PieWorldContext = CreateNewWorldContext(EWorldType::PIE);
		PieWorldContext.PIEInstance = PIEInstance++;
		PieWorldContext.bWaitingOnOnlineSubsystem = true;

		// Update login struct parameters
		DataStruct.WorldContextHandle = PieWorldContext.ContextHandle;
		DataStruct.SettingsIndex = ++SettingsIndex;
		DataStruct.NextX = NextX;
		DataStruct.NextY = NextY;
		GetMultipleInstancePositions(DataStruct.SettingsIndex, NextX, NextY);
		DataStruct.NetMode = EPlayNetMode::PIE_Client;

		FName OnlineIdentifier = UOnlineEngineInterface::Get()->GetOnlineIdentifier(PieWorldContext);
		UE_LOG(LogPlayLevel, Display, TEXT("Creating online subsystem for client %s"), *OnlineIdentifier.ToString());

		// Login to online platform before creating world
		FOnPIELoginComplete Delegate;
		Delegate.BindUObject(this, &UEditorEngine::OnLoginPIEComplete, DataStruct);
		// The server doesn't get a PIE login but counts as an online instance
		UOnlineEngineInterface::Get()->LoginPIEInstance(OnlineIdentifier, 0, NumOnlinePIEInstances - 1, Delegate);

		NumOnlinePIEInstances++;
	}
	else
	{
		GetMultipleInstancePositions(SettingsIndex++, NextX, NextY);

		UGameInstance* const ClientGameInstance = CreatePIEGameInstance(PIEInstance, false, false, bStartLateJoinersInSpectatorMode, false, PIEStartTime);
		if (!ClientGameInstance)
		{
			// Failed, abort
			return;
		}

		PIEInstance++;
	}

	GetMultipleInstancePositions(0, NextX, NextY);
}

UGameInstance* UEditorEngine::CreatePIEGameInstance(int32 InPIEInstance, bool bInSimulateInEditor, bool bAnyBlueprintErrors, bool bStartInSpectatorMode, bool bRunAsDedicated, float PIEStartTime)
{
	const FString WorldPackageName = EditorWorld->GetOutermost()->GetName();
	
	const bool bUseVRPreview = bUseVRPreviewForPlayWorld && (InPIEInstance >= 0 && InPIEInstance <= 1);

	// Start a new PIE log page
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Package"), FText::FromString(FPackageName::GetLongPackageAssetName(WorldPackageName)));
		Arguments.Add(TEXT("TimeStamp"), FText::AsDateTime(FDateTime::Now()));

		FText PIESessionLabel = bInSimulateInEditor ?
			FText::Format(LOCTEXT("SIESessionLabel", "SIE session: {Package} ({TimeStamp})"), Arguments) : 
			FText::Format(LOCTEXT("PIESessionLabel", "PIE session: {Package} ({TimeStamp})"), Arguments);

		FMessageLog(NAME_CategoryPIE).NewPage(PIESessionLabel);
	}

	// create a new GameInstance
	FSoftClassPath GameInstanceClassName = GetDefault<UGameMapsSettings>()->GameInstanceClass;
	UClass* GameInstanceClass = (GameInstanceClassName.IsValid() ? LoadObject<UClass>(NULL, *GameInstanceClassName.ToString()) : UGameInstance::StaticClass());

	// If the GameInstance class from the settings cannot be found, fall back to the base class
	if(GameInstanceClass == nullptr)
	{
		GameInstanceClass = UGameInstance::StaticClass();
	}
	UGameInstance* GameInstance = NewObject<UGameInstance>(this, GameInstanceClass);

	// We need to temporarily add the GameInstance to the root because the InitPIE call can do garbage collection wiping out the GameInstance
	GameInstance->AddToRoot();

	FGameInstancePIEParameters GameInstanceParams;
	GameInstanceParams.bAnyBlueprintErrors = bAnyBlueprintErrors;
	GameInstanceParams.bSimulateInEditor = bInSimulateInEditor;
	GameInstanceParams.bStartInSpectatorMode = bStartInSpectatorMode;
	GameInstanceParams.bRunAsDedicated = bRunAsDedicated;

	
	const FGameInstancePIEResult InitializeResult = GameInstance->InitializeForPlayInEditor(InPIEInstance, GameInstanceParams);
	if (!InitializeResult.IsSuccess())
	{
		FMessageDialog::Open(EAppMsgType::Ok, InitializeResult.FailureReason);

		FEditorDelegates::EndPIE.Broadcast(bInSimulateInEditor);

		if (EditorWorld->GetNavigationSystem())
		{
			EditorWorld->GetNavigationSystem()->OnPIEEnd();
		}

		return nullptr;
	}
	
	FWorldContext* const PieWorldContext = GameInstance->GetWorldContext();
	check(PieWorldContext);
	PlayWorld = PieWorldContext->World();

	GWorld = PlayWorld;
	SetPlayInEditorWorld( PlayWorld );

#if PLATFORM_64BITS
	const FString PlatformBitsString( TEXT( "64" ) );
#else
	const FString PlatformBitsString( TEXT( "32" ) );
#endif

	const FText WindowTitleOverride = GetDefault<UGeneralProjectSettings>()->ProjectDisplayedTitle;

	FFormatNamedArguments Args;
	Args.Add( TEXT("GameName"), FText::FromString( FString( WindowTitleOverride.IsEmpty() ? FApp::GetProjectName() : WindowTitleOverride.ToString() ) ) );
	Args.Add( TEXT("PlatformBits"), FText::FromString( PlatformBitsString ) );
	Args.Add( TEXT("RHIName"), FText::FromName( LegacyShaderPlatformToShaderFormat( GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel] ) ) );

	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
	const EPlayNetMode PlayNetMode = [&PlayInSettings]{ EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	if (PlayNetMode == PIE_Client)
	{
		Args.Add(TEXT("NetMode"), FText::FromString(FString::Printf(TEXT("Client %d"), PieWorldContext->PIEInstance - 1)));
	}
	else if (PlayNetMode == PIE_ListenServer)
	{
		Args.Add( TEXT("NetMode"), FText::FromString( TEXT("Server")));
	}
	else
	{
		Args.Add( TEXT("NetMode"), FText::FromString( TEXT("Standalone")));
	}

	const FText ViewportName = FText::Format( NSLOCTEXT("UnrealEd", "PlayInEditor_RHI_F", "{GameName} Game Preview {NetMode} ({PlatformBits}-bit/{RHIName})" ), Args );

	// Make a list of all the selected actors
	TArray<UObject *> SelectedActors;
	TArray<UObject*> SelectedComponents;
	for ( FSelectionIterator It( GetSelectedActorIterator() ); It; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		if (Actor)
		{
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			SelectedActors.Add( Actor );
		}
	}


	// Unselect everything
	GEditor->SelectNone( true, true, false );
	GetSelectedActors()->DeselectAll();
	GetSelectedObjects()->DeselectAll();
	GetSelectedComponents()->DeselectAll();

	// For every actor that was selected previously, make sure it's sim equivalent is selected
	for ( int32 ActorIndex = 0; ActorIndex < SelectedActors.Num(); ++ActorIndex )
	{
		AActor* Actor = Cast<AActor>( SelectedActors[ ActorIndex ] );
		if (Actor)
		{
			ActorsThatWereSelected.Add( Actor );

			AActor* SimActor = EditorUtilities::GetSimWorldCounterpartActor(Actor);
			if (SimActor && !SimActor->bHidden && bInSimulateInEditor)
			{
				SelectActor( SimActor, true, false );
			}
		}
	}

	// For play in editor, this is the viewport widget where the game is being displayed
	TSharedPtr<SViewport> PieViewportWidget;

	// Initialize the viewport client.
	UGameViewportClient* ViewportClient = NULL;
	ULocalPlayer *NewLocalPlayer = NULL;
	
	if (GEngine->XRSystem.IsValid() && !bInSimulateInEditor )
	{
		GEngine->XRSystem->OnBeginPlay(*PieWorldContext);
	}

	if (!PieWorldContext->RunAsDedicated)
	{
		bool bCreateNewAudioDevice = PlayInSettings->IsCreateAudioDeviceForEveryPlayer();

		ViewportClient = NewObject<UGameViewportClient>(this, GameViewportClientClass);
		ViewportClient->Init(*PieWorldContext, GameInstance, bCreateNewAudioDevice);

		GameViewport = ViewportClient;
		GameViewport->bIsPlayInEditorViewport = true;
		PieWorldContext->GameViewport = ViewportClient;

		// Add a handler for game client input key
		ViewportClient->OnGameViewportInputKey().BindUObject(this, &UEditorEngine::ProcessDebuggerCommands);

		// Add a handler for viewport close requests
		ViewportCloseRequestedDelegateHandle = ViewportClient->OnCloseRequested().AddUObject(this, &UEditorEngine::OnViewportCloseRequested);
		FSlatePlayInEditorInfo& SlatePlayInEditorSession = SlatePlayInEditorMap.Add(PieWorldContext->ContextHandle, FSlatePlayInEditorInfo());
		SlatePlayInEditorSession.DestinationSlateViewport = RequestedDestinationSlateViewport;	// Might be invalid depending how pie was launched. Code below handles this.
		RequestedDestinationSlateViewport = NULL;

		FString Error;
		NewLocalPlayer = ViewportClient->SetupInitialLocalPlayer(Error);
		if(!NewLocalPlayer)
		{
			FMessageDialog::Open( EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntSpawnPlayer", "Couldn't spawn player: {0}"), FText::FromString(Error)) );
			// go back to using the real world as GWorld
			RestoreEditorWorld( EditorWorld );
			EndPlayMap();
			return nullptr;
		}

		if (!bInSimulateInEditor)
		{
			SlatePlayInEditorSession.EditorPlayer = NewLocalPlayer;
		}
			
		// Note: For K2 debugging purposes this MUST be created before beginplay is called because beginplay can trigger breakpoints
		// and we need to be able to refocus the pie viewport afterwards so it must be created first in order for us to find it
		{
			// Only create a separate viewport and window if we aren't playing in a current viewport
			if( SlatePlayInEditorSession.DestinationSlateViewport.IsValid() )
			{
				TSharedPtr<ILevelViewport> LevelViewportRef = SlatePlayInEditorSession.DestinationSlateViewport.Pin();

				LevelViewportRef->StartPlayInEditorSession( ViewportClient, bInSimulateInEditor );

				//SlatePlayInEditorSession.SlatePlayInEditorWindowViewport = MakeShareable<FSceneViewport>((FSceneViewport*)LevelViewportRef->GetActiveViewport());
			}
			else
			{
				const int32 PlayNumberOfClients = [&PlayInSettings] { int32 NumberOfClients(0); return (PlayInSettings->GetPlayNumberOfClients(NumberOfClients) ? NumberOfClients : 0); }();

				// Create the top level pie window and add it to Slate
				uint32 NewWindowHeight = PlayInSettings->NewWindowHeight;
				uint32 NewWindowWidth = PlayInSettings->NewWindowWidth;
				FIntPoint NewWindowPosition = PlayInSettings->NewWindowPosition;
				bool CenterNewWindow = PlayInSettings->CenterNewWindow && (PlayNumberOfClients == 1);

				// Setup size for PIE window
				if ((NewWindowWidth <= 0) || (NewWindowHeight <= 0))
				{
					// Get desktop metrics
					FDisplayMetrics DisplayMetrics;
					FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

					const FVector2D DisplaySize(
						DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
						DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top
					);

					// Use a centered window at the default window size
					NewWindowPosition.X = 0;
					NewWindowPosition.Y = 0;
					NewWindowWidth = 0.75 * DisplaySize.X;
					NewWindowHeight = 0.75 * DisplaySize.Y;
					CenterNewWindow = true;
				}

				bool bUseOSWndBorder = false;
				bool bRenderDirectlyToWindow = false;
				bool bEnableStereoRendering = false;
				if (bUseVRPreview)	// @todo vreditor: Is not having an OS window border a problem?  We could spawn a dedicated VR window if so.  What about true fullscreen in VR?
				{
					// modify window and viewport properties for VR.
					bUseOSWndBorder = true;
					bRenderDirectlyToWindow = true;
					bEnableStereoRendering = true;
					CenterNewWindow = true;
				}

				TSharedPtr<SWindow> PieWindow = PlayInSettings->CustomPIEWindow.Pin();

				const bool bHasCustomWindow = PieWindow.IsValid();
				if (!bHasCustomWindow)
				{
					PieWindow = SNew(SWindow)
					.Title(ViewportName)
					.ScreenPosition(FVector2D( NewWindowPosition.X, NewWindowPosition.Y ))
					.ClientSize(FVector2D( NewWindowWidth, NewWindowHeight ))
					.AutoCenter(CenterNewWindow ? EAutoCenter::PreferredWorkArea : EAutoCenter::None)
					.UseOSWindowBorder(bUseOSWndBorder)
					.SaneWindowPlacement(!CenterNewWindow)
					.SizingRule(ESizingRule::UserSized);
				}


				// Setup a delegate for switching to the play world on slate input events, drawing and ticking
				FOnSwitchWorldHack OnWorldSwitch = FOnSwitchWorldHack::CreateUObject( this, &UEditorEngine::OnSwitchWorldForSlatePieWindow );
				PieWindow->SetOnWorldSwitchHack( OnWorldSwitch );

				if (!bHasCustomWindow)
				{
				// Mac does not support parenting, do not keep on top
#if PLATFORM_MAC
					FSlateApplication::Get().AddWindow(PieWindow.ToSharedRef());
#else
				TSharedRef<SWindow, ESPMode::Fast> MainWindow = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetParentWindow().ToSharedRef();
				if (PlayInSettings->PIEAlwaysOnTop)
				{
						FSlateApplication::Get().AddWindowAsNativeChild(PieWindow.ToSharedRef(), MainWindow, true);
				}
				else
				{
						FSlateApplication::Get().AddWindow(PieWindow.ToSharedRef());
				}
#endif
				}

				TSharedRef<SOverlay> ViewportOverlayWidgetRef = SNew(SOverlay);

				TSharedRef<SGameLayerManager> GameLayerManagerRef = SNew(SGameLayerManager)
					.SceneViewport_UObject(this, &UEditorEngine::GetGameSceneViewport, ViewportClient)
					[
						ViewportOverlayWidgetRef
					];

				PieViewportWidget = 
					SNew( SViewport )
						.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
						.EnableGammaCorrection( false )// Gamma correction in the game is handled in post processing in the scene renderer
						.RenderDirectlyToWindow( bRenderDirectlyToWindow )
						.EnableStereoRendering( bEnableStereoRendering )
						[
							GameLayerManagerRef
						];

				// Create a wrapper widget for PIE viewport to process play world actions
				TSharedRef<SGlobalPlayWorldActions> GlobalPlayWorldActionsWidgetRef = SNew(SGlobalPlayWorldActions)
					[
						PieViewportWidget.ToSharedRef()
					];

				PieWindow->SetContent(GlobalPlayWorldActionsWidgetRef);

				if (!bHasCustomWindow)
				{
					// Ensure the PIE window appears does not appear behind other windows.
					PieWindow->BringToFront();
				}

				ViewportClient->SetViewportOverlayWidget( PieWindow, ViewportOverlayWidgetRef );
				ViewportClient->SetGameLayerManager(GameLayerManagerRef);
				bool bShouldMinimizeRootWindow = bUseVRPreview && GEngine->XRSystem.IsValid() && GetDefault<ULevelEditorPlaySettings>()->ShouldMinimizeEditorOnVRPIE;
				// Set up a notification when the window is closed so we can clean up PIE
				{
					struct FLocal
					{
						static void OnPIEWindowClosed( const TSharedRef< SWindow >& WindowBeingClosed, TWeakPtr< SViewport > PIEViewportWidget, int32 index, bool bRestoreRootWindow )
						{
							// Save off the window position
							const FVector2D PIEWindowPos = WindowBeingClosed->GetPositionInScreen();

							ULevelEditorPlaySettings* LevelEditorPlaySettings = ULevelEditorPlaySettings::StaticClass()->GetDefaultObject<ULevelEditorPlaySettings>();

							if (index <= 0)
							{
								LevelEditorPlaySettings->NewWindowPosition.X = FPlatformMath::RoundToInt(PIEWindowPos.X);
								LevelEditorPlaySettings->NewWindowPosition.Y = FPlatformMath::RoundToInt(PIEWindowPos.Y);
							}
							else
							{
								if (index >= LevelEditorPlaySettings->MultipleInstancePositions.Num())
								{
									LevelEditorPlaySettings->MultipleInstancePositions.SetNum(index + 1);
								}

								LevelEditorPlaySettings->MultipleInstancePositions[index] = FIntPoint(PIEWindowPos.X, PIEWindowPos.Y); 
							}

							LevelEditorPlaySettings->PostEditChange();
							LevelEditorPlaySettings->SaveConfig();

							// Route the callback
							PIEViewportWidget.Pin()->OnWindowClosed( WindowBeingClosed );

							if (bRestoreRootWindow)
							{
								// restore previously minimized root window.
								TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
								if (RootWindow.IsValid())
								{
									RootWindow->Restore();
								}
							}
						}
					};
				
					const bool CanPlayNetDedicated = [&PlayInSettings]{ bool PlayNetDedicated(false); return (PlayInSettings->GetPlayNetDedicated(PlayNetDedicated) && PlayNetDedicated); }();
					PieWindow->SetOnWindowClosed(FOnWindowClosed::CreateStatic(&FLocal::OnPIEWindowClosed, TWeakPtr<SViewport>(PieViewportWidget), 
						(PlayNumberOfClients == 1) ? 0 : PieWorldContext->PIEInstance - (CanPlayNetDedicated ? 1 : 0), bShouldMinimizeRootWindow));
				}

				// Create a new viewport that the viewport widget will use to render the game
				SlatePlayInEditorSession.SlatePlayInEditorWindowViewport = MakeShareable( new FSceneViewport( ViewportClient, PieViewportWidget ) );

				const bool bShouldGameGetMouseControl = GetDefault<ULevelEditorPlaySettings>()->GameGetsMouseControl || (bUseVRPreview && GEngine->XRSystem.IsValid());
				SlatePlayInEditorSession.SlatePlayInEditorWindowViewport->SetPlayInEditorGetsMouseControl(bShouldGameGetMouseControl);
				PieViewportWidget->SetViewportInterface( SlatePlayInEditorSession.SlatePlayInEditorWindowViewport.ToSharedRef() );
				
				FSlateApplication::Get().RegisterViewport(PieViewportWidget.ToSharedRef());
				
				SlatePlayInEditorSession.SlatePlayInEditorWindow = PieWindow;

				// Let the viewport client know what viewport is using it.  We need to set the Viewport Frame as 
				// well (which in turn sets the viewport) so that SetRes command will work.
				ViewportClient->SetViewportFrame(SlatePlayInEditorSession.SlatePlayInEditorWindowViewport.Get());
				// Mark the viewport as PIE viewport
				ViewportClient->Viewport->SetPlayInEditorViewport( ViewportClient->bIsPlayInEditorViewport );

				// Ensure the window has a valid size before calling BeginPlay
				SlatePlayInEditorSession.SlatePlayInEditorWindowViewport->ResizeFrame( NewWindowWidth, NewWindowHeight, EWindowMode::Windowed );

				// Change the system resolution to match our window, to make sure game and slate window are kept syncronised
				FSystemResolution::RequestResolutionChange(NewWindowWidth, NewWindowHeight, EWindowMode::Windowed);

				if (bUseVRPreview)
				{
					GEngine->StereoRenderingDevice->EnableStereo(true);

					// minimize the root window to provide max performance for the preview.
					TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
					if (RootWindow.IsValid() && bShouldMinimizeRootWindow)
					{
						RootWindow->Minimize();
					}
				}
			}

			UGameViewportClient::OnViewportCreated().Broadcast();
		}
	}

	if ( GameViewport != NULL && GameViewport->Viewport != NULL )
	{
		// Set the game viewport that was just created as a pie viewport.
		GameViewport->Viewport->SetPlayInEditorViewport( true );
	}

	// Disable the screensaver when PIE is running.
	EnableScreenSaver( false );


	EditorWorld->TransferBlueprintDebugReferences(PlayWorld);

	// By this point it is safe to remove the GameInstance from the root and allow it to garbage collected as per usual
	GameInstance->RemoveFromRoot();

	// Start the game instance, make sure to set the PIE instance global as this is basically a tick
	GPlayInEditorID = InPIEInstance;
	const FGameInstancePIEResult StartResult = GameInstance->StartPlayInEditorGameInstance(NewLocalPlayer, GameInstanceParams);
	GPlayInEditorID = -1;

	if (!StartResult.IsSuccess())
	{
		FMessageDialog::Open(EAppMsgType::Ok, StartResult.FailureReason);
		RestoreEditorWorld( EditorWorld );
		EndPlayMap();
		return nullptr;
	}

	// Set up a delegate to be called in Slate when GWorld needs to change.  Slate does not have direct access to the playworld to switch itself
	FScopedConditionalWorldSwitcher::SwitchWorldForPIEDelegate = FOnSwitchWorldForPIE::CreateUObject( this, &UEditorEngine::OnSwitchWorldsForPIE );

	if( PieViewportWidget.IsValid() )
	{
		// Register the new viewport widget with Slate for viewport specific message routing.
		FSlateApplication::Get().RegisterGameViewport( PieViewportWidget.ToSharedRef() );
	}

	// go back to using the real world as GWorld
	RestoreEditorWorld( EditorWorld );

	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("MapName"), FText::FromString(GameInstance->PIEMapName));
		Arguments.Add(TEXT("StartTime"), FPlatformTime::Seconds() - PIEStartTime);
		FMessageLog(NAME_CategoryPIE).Info(FText::Format(LOCTEXT("PIEStartTime", "Play in editor start time for {MapName} {StartTime}"), Arguments));
	}

	// Update the details window with the actors we have just selected
	GUnrealEd->UpdateFloatingPropertyWindows();

	// Clean up any editor actors being referenced 
	GEngine->BroadcastLevelActorListChanged();

	// Set an undo barrier so that transactions prior to PIE can't be undone
	GUnrealEd->Trans->SetUndoBarrier();

	return GameInstance;
}

void UEditorEngine::OnViewportCloseRequested(FViewport* InViewport)
{
	RequestEndPlayMap();
}

const FSceneViewport* UEditorEngine::GetGameSceneViewport(UGameViewportClient* ViewportClient) const
{
	return ViewportClient->GetGameViewport();
}

FViewport* UEditorEngine::GetActiveViewport()
{
	// Get the Level editor module and request the Active Viewport.
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );

	TSharedPtr<ILevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

	if ( ActiveLevelViewport.IsValid() )
	{
		return ActiveLevelViewport->GetActiveViewport();
	}
	
	return nullptr;
}

FViewport* UEditorEngine::GetPIEViewport()
{
	// Check both cases where the PIE viewport may be, otherwise return NULL if none are found.
	if( GameViewport )
	{
		return GameViewport->Viewport;
	}
	else
	{
		for (const FWorldContext& WorldContext : WorldList)
		{
			if (WorldContext.WorldType == EWorldType::PIE)
			{
				// We can't use FindChecked here because when using the dedicated server option we don't initialize this map 
				//	(we don't use a viewport for the PIE context in this case)
				FSlatePlayInEditorInfo * SlatePlayInEditorSessionPtr = SlatePlayInEditorMap.Find(WorldContext.ContextHandle);
				if ( SlatePlayInEditorSessionPtr != nullptr && SlatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.IsValid() )
				{
					return SlatePlayInEditorSessionPtr->SlatePlayInEditorWindowViewport.Get();
				}
			}
		}
	}

	return nullptr;
}

void UEditorEngine::ToggleBetweenPIEandSIE( bool bNewSession )
{
	bIsToggleBetweenPIEandSIEQueued = false;

	FEditorDelegates::OnPreSwitchBeginPIEAndSIE.Broadcast(bIsSimulatingInEditor);

	// The first PIE world context is the one that can toggle between PIE and SIE
	// Network PIE/SIE toggling is not really meant to be supported.
	FSlatePlayInEditorInfo * SlateInfoPtr = nullptr;
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE && !WorldContext.RunAsDedicated)
		{
			SlateInfoPtr = SlatePlayInEditorMap.Find(WorldContext.ContextHandle);
			break;
		}
	}

	if (!SlateInfoPtr)
	{
		return;
	}

	if( FEngineAnalytics::IsAvailable() && !bNewSession )
	{
		FString ToggleType = bIsSimulatingInEditor ? TEXT("SIEtoPIE") : TEXT("PIEtoSIE");

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.PIE"), TEXT("ToggleBetweenPIEandSIE"), ToggleType );
	}

	FSlatePlayInEditorInfo & SlatePlayInEditorSession = *SlateInfoPtr;

	// This is only supported inside SLevelEditor viewports currently
	TSharedPtr<ILevelViewport> LevelViewport = SlatePlayInEditorSession.DestinationSlateViewport.Pin();
	if( ensure(LevelViewport.IsValid()) )
	{
		FLevelEditorViewportClient& EditorViewportClient = LevelViewport->GetLevelViewportClient();

		// Toggle to pie if currently simulating
		if( bIsSimulatingInEditor )
		{
			// The undo system may have a reference to a SIE object that is about to be destroyed, so clear the transactions
			ResetTransaction( NSLOCTEXT("UnrealEd", "ToggleBetweenPIEandSIE", "Toggle Between PIE and SIE") );

			// The Game's viewport needs to know about the change away from simluate before the PC is (potentially) created
			GameViewport->GetGameViewport()->SetPlayInEditorIsSimulate(false);

			// The editor viewport client wont be visible so temporarily disable it being realtime
			EditorViewportClient.SetRealtime( false, true );

			if (!SlatePlayInEditorSession.EditorPlayer.IsValid())
			{
				OnSwitchWorldsForPIE(true);

				UWorld* World = GameViewport->GetWorld();
				AGameModeBase* AuthGameMode = World->GetAuthGameMode();
				if (AuthGameMode && GameViewport->GetGameInstance())	// If there is no GameMode, we are probably the client and cannot RestartPlayer.
				{
					AuthGameMode->SpawnPlayerFromSimulate(EditorViewportClient.GetViewLocation(), EditorViewportClient.GetViewRotation());
				}

				OnSwitchWorldsForPIE(false);
			}

			// A game viewport already exists, tell the level viewport its in to swap to it
			LevelViewport->SwapViewportsForPlayInEditor();

			// No longer simulating
			GameViewport->SetIsSimulateInEditorViewport(false);
			EditorViewportClient.SetIsSimulateInEditorViewport(false);
			bIsSimulatingInEditor = false;
		}
		else
		{
			// Swap to simulate from PIE
			LevelViewport->SwapViewportsForSimulateInEditor();
	
			GameViewport->SetIsSimulateInEditorViewport(true);
			GameViewport->GetGameViewport()->SetPlayInEditorIsSimulate(true);
			EditorViewportClient.SetIsSimulateInEditorViewport(true);
			bIsSimulatingInEditor = true;

			// Make sure the viewport is in real-time mode
			EditorViewportClient.SetRealtime( true );

			// The Simulate window should show stats
			EditorViewportClient.SetShowStats( true );

			if( SlatePlayInEditorSession.EditorPlayer.IsValid() )
			{
				// Move the editor camera to where the player was.  
				FVector ViewLocation;
				FRotator ViewRotation;
				SlatePlayInEditorSession.EditorPlayer.Get()->PlayerController->GetPlayerViewPoint( ViewLocation, ViewRotation );
				EditorViewportClient.SetViewLocation( ViewLocation );

				if( EditorViewportClient.IsPerspective() )
				{
					// Rotation only matters for perspective viewports not orthographic
					EditorViewportClient.SetViewRotation( ViewRotation );
				}
			}
		}
	}

	// Backup ActorsThatWereSelected as this will be cleared whilst deselecting
	TArray<TWeakObjectPtr<class AActor> > BackupOfActorsThatWereSelected(ActorsThatWereSelected);

	// Unselect everything
	GEditor->SelectNone( true, true, false );
	GetSelectedActors()->DeselectAll();
	GetSelectedObjects()->DeselectAll();

	// restore the backup
	ActorsThatWereSelected = BackupOfActorsThatWereSelected;

	// make sure each selected actors sim equivalent is selected if we're Simulating but not if we're Playing
	for ( int32 ActorIndex = 0; ActorIndex < ActorsThatWereSelected.Num(); ++ActorIndex )
	{
		TWeakObjectPtr<AActor> Actor = ActorsThatWereSelected[ ActorIndex ].Get();
		if (Actor.IsValid())
		{
			AActor* SimActor = EditorUtilities::GetSimWorldCounterpartActor(Actor.Get());
			if (SimActor && !SimActor->bHidden)
			{
				SelectActor( SimActor, bIsSimulatingInEditor, false );
			}
		}
	}

	FEditorDelegates::OnSwitchBeginPIEAndSIE.Broadcast( bIsSimulatingInEditor );
}

int32 UEditorEngine::OnSwitchWorldForSlatePieWindow( int32 WorldID )
{
	static const int32 EditorWorldID = 0;
	static const int32 PieWorldID = 1;

	int32 RestoreID = -1;
	if( WorldID == -1 && GWorld != PlayWorld && PlayWorld != NULL)
	{
		// When we have an invalid world id we always switch to the pie world in the PIE window
		const bool bSwitchToPIE = true; 
		OnSwitchWorldsForPIE( bSwitchToPIE );
		// The editor world was active restore it later
		RestoreID = EditorWorldID;
	}
	else if( WorldID == PieWorldID && GWorld != PlayWorld)
	{
		const bool bSwitchToPIE = true;
		// Want to restore the PIE world and the current world is not already the pie world
		OnSwitchWorldsForPIE( bSwitchToPIE );
	}
	else if( WorldID == EditorWorldID && GWorld != EditorWorld)
	{
		const bool bSwitchToPIE = false;
		// Want to restore the editor world and the current world is not already the editor world
		OnSwitchWorldsForPIE( bSwitchToPIE );
	}
	else
	{
		// Current world is already the same as the world being switched to (nested calls to this for example)
	}

	return RestoreID;
}

void UEditorEngine::OnSwitchWorldsForPIE( bool bSwitchToPieWorld )
{
	if( bSwitchToPieWorld )
	{
		SetPlayInEditorWorld( PlayWorld );
	}
	else
	{
		RestoreEditorWorld( EditorWorld );
	}
}

bool UEditorEngine::PackageUsingExternalObjects( ULevel* LevelToCheck, bool bAddForMapCheck )
{
	check(LevelToCheck);
	bool bFoundExternal = false;
	TArray<UObject*> ExternalObjects;
	if(PackageTools::CheckForReferencesToExternalPackages(NULL, NULL, LevelToCheck, &ExternalObjects ))
	{
		for(int32 ObjectIndex = 0; ObjectIndex < ExternalObjects.Num(); ++ObjectIndex)
		{
			// If the object in question has external references and is not pending deletion, add it to the log and tell the user about it below
			UObject* ExternalObject = ExternalObjects[ObjectIndex];

			if(!ExternalObject->IsPendingKill())
			{
				bFoundExternal = true;
				if( bAddForMapCheck ) 
				{
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("ObjectName"), FText::FromString(ExternalObject->GetFullName()));
					FMessageLog("MapCheck").Warning()
						->AddToken(FUObjectToken::Create(ExternalObject))
						->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_UsingExternalObject", "{ObjectName} : Externally referenced"), Arguments ) ))
						->AddToken(FMapErrorToken::Create(FMapErrors::UsingExternalObject));
				}
			}
		}
	}
	return bFoundExternal;
}

UWorld* UEditorEngine::CreatePIEWorldByDuplication(FWorldContext &WorldContext, UWorld* InWorld, FString &PlayWorldMapName)
{
	double StartTime = FPlatformTime::Seconds();
	UPackage* InPackage = InWorld->GetOutermost();
	UWorld* NewPIEWorld = NULL;
	
	const FString WorldPackageName = InPackage->GetName();

	// Preserve the old path keeping EditorWorld name the same
	PlayWorldMapName = UWorld::ConvertToPIEPackageName(WorldPackageName, WorldContext.PIEInstance);

	// Display a busy cursor while we prepare the PIE world
	const FScopedBusyCursor BusyCursor;

	// Before loading the map, we need to set these flags to true so that postload will work properly
	GIsPlayInEditorWorld = true;

	const FName PlayWorldMapFName = FName(*PlayWorldMapName);
	UWorld::WorldTypePreLoadMap.FindOrAdd(PlayWorldMapFName) = EWorldType::PIE;

	// Create a package for the PIE world
	UE_LOG( LogPlayLevel, Log, TEXT("Creating play world package: %s"),  *PlayWorldMapName );	

	UPackage* PlayWorldPackage = CreatePackage(nullptr,*PlayWorldMapName);
	PlayWorldPackage->SetPackageFlags(PKG_PlayInEditor);
	PlayWorldPackage->PIEInstanceID = WorldContext.PIEInstance;
	PlayWorldPackage->FileName = InPackage->FileName;
	PlayWorldPackage->SetGuid( InPackage->GetGuid() );
	PlayWorldPackage->MarkAsFullyLoaded();

	// check(GPlayInEditorID == -1 || GPlayInEditorID == WorldContext.PIEInstance);
	// Currently GPlayInEditorID is not correctly reset after map loading, so it's not safe to assert here
	GPlayInEditorID = WorldContext.PIEInstance;

	{
		double SDOStart = FPlatformTime::Seconds();

		// Reset any GUID fixups with lazy pointers
		FLazyObjectPtr::ResetPIEFixups();

		// Prepare string asset references for fixup
		FSoftObjectPath::AddPIEPackageName(FName(*PlayWorldMapName));
		for (ULevelStreaming* StreamingLevel : InWorld->StreamingLevels)
		{
			if ( StreamingLevel )
			{
				FString StreamingLevelPIEName = UWorld::ConvertToPIEPackageName(StreamingLevel->GetWorldAssetPackageName(), WorldContext.PIEInstance);
				FSoftObjectPath::AddPIEPackageName(FName(*StreamingLevelPIEName));
			}
		}

		// NULL GWorld before various PostLoad functions are called, this makes it easier to debug invalid GWorld accesses
		GWorld = NULL;

		// Duplicate the editor world to create the PIE world
		NewPIEWorld = CastChecked<UWorld>( StaticDuplicateObject(
			InWorld,				// Source root
			PlayWorldPackage,		// Destination root
			InWorld->GetFName(),	// Name for new object
			RF_AllFlags,			// FlagMask
			NULL,					// DestClass
			EDuplicateMode::PIE
			) );

		// Store prefix we used to rename this world and streaming levels package names
		NewPIEWorld->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(WorldContext.PIEInstance);
		// Fixup model components. The index buffers have been created for the components in the source world and the order
		// in which components were post-loaded matters. So don't try to guarantee a particular order here, just copy the
		// elements over.
		if ( NewPIEWorld->PersistentLevel->Model != NULL
			&& NewPIEWorld->PersistentLevel->Model == InWorld->PersistentLevel->Model
			&& NewPIEWorld->PersistentLevel->ModelComponents.Num() == InWorld->PersistentLevel->ModelComponents.Num() )
		{
			NewPIEWorld->PersistentLevel->Model->ClearLocalMaterialIndexBuffersData();
			for (int32 ComponentIndex = 0; ComponentIndex < NewPIEWorld->PersistentLevel->ModelComponents.Num(); ++ComponentIndex)
			{
				UModelComponent* SrcComponent = InWorld->PersistentLevel->ModelComponents[ComponentIndex];
				UModelComponent* DestComponent = NewPIEWorld->PersistentLevel->ModelComponents[ComponentIndex];
				DestComponent->CopyElementsFrom(SrcComponent);
			}
		}

		UE_LOG(LogPlayLevel, Log, TEXT("PIE: StaticDuplicateObject took: (%fs)"),  float(FPlatformTime::Seconds() - SDOStart));		
	}

	// Clean up the world type list now that PostLoad has occurred
	UWorld::WorldTypePreLoadMap.Remove(PlayWorldMapFName);

	GPlayInEditorID = -1;
	check( NewPIEWorld );
	NewPIEWorld->FeatureLevel = InWorld->FeatureLevel;
	PostCreatePIEWorld(NewPIEWorld);

	// After loading the map, reset these so that things continue as normal
	GIsPlayInEditorWorld = false;
	
	UE_LOG(LogPlayLevel, Log, TEXT("PIE: Created PIE world by copying editor world from %s to %s (%fs)"), *InWorld->GetPathName(), *NewPIEWorld->GetPathName(), float(FPlatformTime::Seconds() - StartTime));
	return NewPIEWorld;
}

void UEditorEngine::PostCreatePIEWorld(UWorld *NewPIEWorld)
{
	double WorldInitStart = FPlatformTime::Seconds();
	
	// Init the PIE world
	NewPIEWorld->WorldType = EWorldType::PIE;
	NewPIEWorld->InitWorld();
	UE_LOG(LogPlayLevel, Log, TEXT("PIE: World Init took: (%fs)"),  float(FPlatformTime::Seconds() - WorldInitStart));

	// Tag PlayWorld Actors that also exist in EditorWorld.  At this point, no temporary/run-time actors exist in PlayWorld
	for( FActorIterator PlayActorIt(NewPIEWorld); PlayActorIt; ++PlayActorIt )
	{
		GEditor->ObjectsThatExistInEditorWorld.Set(*PlayActorIt);
	}
}

UWorld* UEditorEngine::CreatePIEWorldFromEntry(FWorldContext &WorldContext, UWorld* InWorld, FString &PlayWorldMapName)
{
	double StartTime = FPlatformTime::Seconds();

	// Create the world
	UWorld *LoadedWorld = UWorld::CreateWorld( EWorldType::PIE, false );
	check(LoadedWorld);
	if (LoadedWorld->GetOutermost() != GetTransientPackage())
	{
		LoadedWorld->GetOutermost()->PIEInstanceID = WorldContext.PIEInstance;
	}
	// Force default GameMode class so project specific code doesn't fire off. 
	// We want this world to truly remain empty while we wait for connect!
	check(LoadedWorld->GetWorldSettings());
	LoadedWorld->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();

	PlayWorldMapName = UGameMapsSettings::GetGameDefaultMap();
	return LoadedWorld;
}

bool UEditorEngine::WorldIsPIEInNewViewport(UWorld *InWorld)
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	if (WorldContext.WorldType == EWorldType::PIE)
	{
		FSlatePlayInEditorInfo * SlateInfoPtr = SlatePlayInEditorMap.Find(WorldContext.ContextHandle);
		if (SlateInfoPtr)
		{
			return SlateInfoPtr->SlatePlayInEditorWindow.IsValid();
		}
	}
	
	return false;
}

void UEditorEngine::SetPIEInstanceWindowSwitchDelegate(FPIEInstanceWindowSwitch InSwitchDelegate)
{
	PIEInstanceWindowSwitchDelegate = InSwitchDelegate;
}

void UEditorEngine::FocusNextPIEWorld(UWorld *CurrentPieWorld, bool previous)
{
	// Get the current world's idx
	int32 CurrentIdx = 0;
	for (CurrentIdx = 0; CurrentPieWorld && CurrentIdx < WorldList.Num(); ++CurrentIdx)
	{
		if (WorldList[CurrentIdx].World() == CurrentPieWorld)
		{
			break;
		}
	}

	// Step through the list to find the next or previous
	int32 step = previous? -1 : 1;
	CurrentIdx += (WorldList.Num() + step);
	
	while ( CurrentPieWorld && WorldList[ CurrentIdx % WorldList.Num() ].World() != CurrentPieWorld )
	{
		FWorldContext &Context = WorldList[CurrentIdx % WorldList.Num()];
		if (Context.World() && Context.WorldType == EWorldType::PIE && Context.GameViewport != NULL)
		{
			break;
		}

		CurrentIdx += step;
	}
	
	if (WorldList[CurrentIdx % WorldList.Num()].World())
	{
		// Bring new window to front and activate new viewport
		FSlatePlayInEditorInfo* SlateInfoPtr = SlatePlayInEditorMap.Find(WorldList[CurrentIdx % WorldList.Num()].ContextHandle);
		if (SlateInfoPtr && SlateInfoPtr->SlatePlayInEditorWindowViewport.IsValid())
		{
			FSceneViewport* SceneViewport = SlateInfoPtr->SlatePlayInEditorWindowViewport.Get();

			FSlateApplication& SlateApp = FSlateApplication::Get();
			TSharedRef<SViewport> ViewportWidget = SceneViewport->GetViewportWidget().Pin().ToSharedRef();

			FWidgetPath WindowWidgetPath;
			TSharedPtr<SWindow> ViewportWindow = SlateApp.FindWidgetWindow(ViewportWidget, WindowWidgetPath);
			check(ViewportWindow.IsValid());

			// Force window to front
			ViewportWindow->BringToFront();

			// Execute notification delegate in case game code has to do anything else
			PIEInstanceWindowSwitchDelegate.ExecuteIfBound();
		}
	}
}
void UEditorEngine::ResetPIEAudioSetting(UWorld *CurrentPieWorld)
{
	ULevelEditorPlaySettings* PlayInSettings = GetMutableDefault<ULevelEditorPlaySettings>();
	if (!PlayInSettings->EnableGameSound)
	{
		if (FAudioDevice* AudioDevice = CurrentPieWorld->GetAudioDevice())
		{
			AudioDevice->SetTransientMasterVolume(0.0f);
		}
	}
}
UGameViewportClient * UEditorEngine::GetNextPIEViewport(UGameViewportClient * CurrentViewport)
{
	// Get the current world's idx
	int32 CurrentIdx = 0;
	for (CurrentIdx = 0; CurrentViewport && CurrentIdx < WorldList.Num(); ++CurrentIdx)
	{
		if (WorldList[CurrentIdx].GameViewport == CurrentViewport)
		{
			break;
		}
	}

	// Step through the list to find the next or previous
	int32 step = 1;
	CurrentIdx += (WorldList.Num() + step);

	while ( CurrentViewport && WorldList[ CurrentIdx % WorldList.Num() ].GameViewport != CurrentViewport )
	{
		FWorldContext &Context = WorldList[CurrentIdx % WorldList.Num()];
		if (Context.GameViewport && Context.WorldType == EWorldType::PIE)
		{
			return Context.GameViewport;
		}

		CurrentIdx += step;
	}

	return NULL;
}

void UEditorEngine::RemapGamepadControllerIdForPIE(class UGameViewportClient* InGameViewport, int32 &ControllerId)
{
	// Increment the controller id if we are the focused window, and RouteGamepadToSecondWindow is true (and we are running multiple clients).
	// This cause the focused window to NOT handle the input, decrement controllerID, and pass it to the next window.
	const ULevelEditorPlaySettings* PlayInSettings = GetDefault<ULevelEditorPlaySettings>();
	const bool CanRouteGamepadToSecondWindow = [&PlayInSettings]{ bool RouteGamepadToSecondWindow(false); return (PlayInSettings->GetRouteGamepadToSecondWindow(RouteGamepadToSecondWindow) && RouteGamepadToSecondWindow); }();
	const bool CanRunUnderOneProcess = [&PlayInSettings]{ bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }();
	if ( CanRouteGamepadToSecondWindow && CanRunUnderOneProcess && InGameViewport->GetWindow().IsValid() && InGameViewport->GetWindow()->HasFocusedDescendants())
	{
		ControllerId++;
	}
}

void UEditorEngine::AutomationPlayUsingLauncher(const FString& InLauncherDeviceId)
{
	PlayUsingLauncherDeviceId = InLauncherDeviceId;
	PlayUsingLauncherDeviceName = PlayUsingLauncherDeviceId.Right(PlayUsingLauncherDeviceId.Find(TEXT("@")));
	PlayUsingLauncher();
}

#undef LOCTEXT_NAMESPACE
