// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorModeManager.h"
#include "InputCoreTypes.h"
#include "VREditorMode.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Editor.h"
#include "HAL/PlatformApplicationMisc.h"

#include "EngineGlobals.h"
#include "LevelEditor.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "EditorWorldExtension.h"
#include "ViewportWorldInteraction.h"
#include "VRModeSettings.h"
#include "Dialogs.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IProjectManager.h"
#include "UnrealEdMisc.h"

#define LOCTEXT_NAMESPACE "VREditor"

FVREditorModeManager::FVREditorModeManager() :
	CurrentVREditorMode( nullptr ),
	bEnableVRRequest( false ),
	HMDWornState( EHMDWornState::Unknown )
{
}

FVREditorModeManager::~FVREditorModeManager()
{
	CurrentVREditorMode = nullptr;
}

void FVREditorModeManager::Tick( const float DeltaTime )
{
	// You can only auto-enter VR if the setting is enabled. Other criteria are that the VR Editor is enabled in experimental settings, that you are not in PIE, and that the editor is foreground.
	IHeadMountedDisplay * const HMD = GEngine != nullptr && GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
	if (GetDefault<UVRModeSettings>()->bEnableAutoVREditMode
		&& HMD
		&& (GEditor->PlayWorld == nullptr || (CurrentVREditorMode != nullptr && CurrentVREditorMode->GetStartedPlayFromVREditor()))
		&& FPlatformApplicationMisc::IsThisApplicationForeground())
	{
		const EHMDWornState::Type LatestHMDWornState = HMD->GetHMDWornState();
		if (HMDWornState != LatestHMDWornState)
		{
			HMDWornState = LatestHMDWornState;
			if (HMDWornState == EHMDWornState::Worn && CurrentVREditorMode == nullptr)
			{
				EnableVREditor( true, false );
			}
			else if (HMDWornState == EHMDWornState::NotWorn && CurrentVREditorMode != nullptr)
			{
				if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
				{
					CurrentVREditorMode->TogglePIEAndVREditor(); //-V595
				}

				EnableVREditor( false, false );
			}
		}
	}

	if(CurrentVREditorMode != nullptr && CurrentVREditorMode->WantsToExitMode())
	{
		// For a standard exit, also take the HMD out of stereo mode
		const bool bShouldDisableStereo = true;
		CloseVREditor( bShouldDisableStereo );
	}

	// Only check for input if we started this play session from the VR Editor
	if( GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor && CurrentVREditorMode != nullptr)
	{
		// Shutdown PIE if we came from the VR Editor and we are not already requesting to start the VR Editor and when any of the players is holding down the required input
		const float ShutDownInputKeyTime = 1.0f;
		TArray<APlayerController*> PlayerControllers;
		GEngine->GetAllLocalPlayerControllers(PlayerControllers);
		for(APlayerController* PlayerController : PlayerControllers)
		{
			if((PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Left_Grip1 ) > ShutDownInputKeyTime || PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Left_Grip2 ) > ShutDownInputKeyTime ) &&
				(PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Right_Grip1 ) > ShutDownInputKeyTime || PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Right_Grip2 ) > ShutDownInputKeyTime ) &&
				PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Right_Trigger ) > ShutDownInputKeyTime &&
				PlayerController->GetInputKeyTimeDown( EKeys::MotionController_Left_Trigger ) > ShutDownInputKeyTime)
			{
				CurrentVREditorMode->TogglePIEAndVREditor();
				// We need to clear the input of the playercontroller when exiting PIE. 
				// Otherwise the input will still be pressed down causing toggle between PIE and VR Editor to be called instantly whenever entering PIE a second time.
				PlayerController->PlayerInput->FlushPressedKeys();
				break;
			}
		}
	}
	else
	{
		// Start the VR Editor mode
		if( bEnableVRRequest )
		{
			EnableVREditor( true, false );
			bEnableVRRequest = false;
		}
	}
}

bool FVREditorModeManager::IsTickable() const
{
	const FProjectDescriptor* CurrentProject = IProjectManager::Get().GetCurrentProject();
	return CurrentProject != nullptr;
}

void FVREditorModeManager::EnableVREditor( const bool bEnable, const bool bForceWithoutHMD )
{
	// Don't do anything when the current VR Editor is already in the requested state
	if( bEnable != IsVREditorActive() )
	{
		if( bEnable && ( IsVREditorAvailable() || bForceWithoutHMD ))
		{
			FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("VRModeEntry_Message", "VR Mode enables you to work on your project in virtual reality using motion controllers. This feature is still under development, so you may experience bugs or crashes while using it."),
				LOCTEXT("VRModeEntry_Title", "Entering VR Mode - Experimental"), "Warning_VRModeEntry", GEditorSettingsIni);

			SetupInfo.ConfirmText = LOCTEXT("VRModeEntry_ConfirmText", "Continue");
			SetupInfo.CancelText = LOCTEXT("VRModeEntry_CancelText", "Cancel");
			SetupInfo.bDefaultToSuppressInTheFuture = true;
			FSuppressableWarningDialog VRModeEntryWarning(SetupInfo);

			if (VRModeEntryWarning.ShowModal() != FSuppressableWarningDialog::Cancel)
			{
				StartVREditorMode(bForceWithoutHMD);
			}
		}
		else if( !bEnable )
		{
			// For a standard exit, take the HMD out of stereo mode
			const bool bShouldDisableStereo = true;
			CloseVREditor( bShouldDisableStereo );
		}
	}
}

bool FVREditorModeManager::IsVREditorActive() const
{
	return CurrentVREditorMode != nullptr && CurrentVREditorMode->IsActive();
}


bool FVREditorModeManager::IsVREditorAvailable() const
{
	const bool bHasHMDDevice = GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDEnabled();
	return bHasHMDDevice && !GEditor->bIsSimulatingInEditor;
}


UVREditorMode* FVREditorModeManager::GetCurrentVREditorMode()
{
	return CurrentVREditorMode;
}

void FVREditorModeManager::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CurrentVREditorMode );
}

void FVREditorModeManager::StartVREditorMode( const bool bForceWithoutHMD )
{
	if (!GIsRequestingExit)
	{
		UVREditorMode* VRMode = nullptr;
		{
			UWorld* World = GEditor->bIsSimulatingInEditor ? GEditor->PlayWorld : GWorld;
			UEditorWorldExtensionCollection* ExtensionCollection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(World);
			check(ExtensionCollection != nullptr);
		
			UViewportWorldInteraction* ViewportWorldInteraction = Cast<UViewportWorldInteraction>(ExtensionCollection->AddExtension(UViewportWorldInteraction::StaticClass()));

			// Create vr editor mode.
			VRMode = NewObject<UVREditorMode>();
			check(VRMode != nullptr);
			ExtensionCollection->AddExtension(VRMode);
		}

		// Tell the level editor we want to be notified when selection changes
		{
			FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
			LevelEditor.OnMapChanged().AddRaw( this, &FVREditorModeManager::OnMapChanged );
		}
	
		CurrentVREditorMode = VRMode;
		CurrentVREditorMode->SetActuallyUsingVR( !bForceWithoutHMD );

		CurrentVREditorMode->Enter();
	}
}

void FVREditorModeManager::CloseVREditor( const bool bShouldDisableStereo )
{
	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>( "LevelEditor" );
	if( LevelEditor != nullptr )
	{
		LevelEditor->OnMapChanged().RemoveAll( this );
	}

	if( CurrentVREditorMode != nullptr )
	{
		UViewportWorldInteraction* WorldInteraction = &CurrentVREditorMode->GetWorldInteraction();
		CurrentVREditorMode->Exit( bShouldDisableStereo );

		UEditorWorldExtensionCollection* Collection = CurrentVREditorMode->GetOwningCollection();
		check(Collection != nullptr);
		Collection->RemoveExtension(CurrentVREditorMode);
		Collection->RemoveExtension(WorldInteraction);

		CurrentVREditorMode = nullptr;
	}
}

void FVREditorModeManager::SetDirectWorldToMeters( const float NewWorldToMeters )
{
	GWorld->GetWorldSettings()->WorldToMeters = NewWorldToMeters; //@todo VREditor: Do not use GWorld
	ENGINE_API extern float GNewWorldToMetersScale;
	GNewWorldToMetersScale = 0.0f;
}

void FVREditorModeManager::OnMapChanged( UWorld* World, EMapChangeType MapChangeType )
{
	if( CurrentVREditorMode && CurrentVREditorMode->IsActive() )
	{
		// When changing maps, we are going to close VR editor mode but then reopen it, so don't take the HMD out of stereo mode
		const bool bShouldDisableStereo = false;
		CloseVREditor( bShouldDisableStereo );
		if (MapChangeType != EMapChangeType::SaveMap)
		{
			bEnableVRRequest = true;
		}
	}
	CurrentVREditorMode = nullptr;
}

#undef LOCTEXT_NAMESPACE
