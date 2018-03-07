// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorMode.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/ConstructorHelpers.h"
#include "SDockTab.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "VREditorUISystem.h"
#include "VIBaseTransformGizmo.h"
#include "ViewportWorldInteraction.h"
#include "VREditorPlacement.h"
#include "VREditorAvatarActor.h"
#include "VREditorTeleporter.h"
#include "VREditorAutoScaler.h"
#include "VREditorStyle.h"
#include "VREditorAssetContainer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "CameraController.h"
#include "EngineGlobals.h"
#include "ILevelEditor.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "SLevelViewport.h"
#include "MotionControllerComponent.h"
#include "EngineAnalytics.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Interfaces/IAnalyticsProvider.h"

#include "IViewportInteractionModule.h"
#include "VREditorMotionControllerInteractor.h"

#include "EditorWorldExtension.h"
#include "SequencerSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "VREditorActions.h"
#include "EditorModes.h"
#include "VRModeSettings.h"
#include "IVREditorModule.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "VREditorMode"

namespace VREd
{
	static FAutoConsoleVariable DefaultVRNearClipPlane(TEXT("VREd.DefaultVRNearClipPlane"), 5.0f, TEXT("The near clip plane to use for VR"));
	static FAutoConsoleVariable SlateDragDistanceOverride( TEXT( "VREd.SlateDragDistanceOverride" ), 40.0f, TEXT( "How many pixels you need to drag before a drag and drop operation starts in VR" ) );
	static FAutoConsoleVariable DefaultWorldToMeters(TEXT("VREd.DefaultWorldToMeters"), 100.0f, TEXT("Default world to meters scale"));

	static FAutoConsoleVariable ShowHeadVelocity( TEXT( "VREd.ShowHeadVelocity" ), 0, TEXT( "Whether to draw a debug indicator that shows how much the head is accelerating" ) );
	static FAutoConsoleVariable HeadVelocitySmoothing( TEXT( "VREd.HeadVelocitySmoothing" ), 0.95f, TEXT( "How much to smooth out head velocity data" ) );
	static FAutoConsoleVariable HeadVelocityMinRadius( TEXT( "VREd.HeadVelocityMinRadius" ), 0.0f, TEXT( "How big the inner circle of the head velocity ring should be" ) );
	static FAutoConsoleVariable HeadVelocityMaxRadius( TEXT( "VREd.HeadVelocityMaxRadius" ), 10.0f, TEXT( "How big the outer circle of the head velocity ring should be" ) );
	static FAutoConsoleVariable HeadVelocityMinLineThickness( TEXT( "VREd.HeadVelocityMinLineThickness" ), 0.05f, TEXT( "How thick the head velocity ring lines should be" ) );
	static FAutoConsoleVariable HeadVelocityMaxLineThickness( TEXT( "VREd.HeadVelocityMaxLineThickness" ), 0.4f, TEXT( "How thick the head velocity ring lines should be" ) );
	static FAutoConsoleVariable HeadLocationMaxVelocity( TEXT( "VREd.HeadLocationMaxVelocity" ), 25.0f, TEXT( "For head velocity indicator, the maximum location velocity in cm/s" ) );
	static FAutoConsoleVariable HeadRotationMaxVelocity( TEXT( "VREd.HeadRotationMaxVelocity" ), 80.0f, TEXT( "For head velocity indicator, the maximum rotation velocity in degrees/s" ) );
	static FAutoConsoleVariable HeadLocationVelocityOffset( TEXT( "VREd.HeadLocationVelocityOffset" ), TEXT( "X=20, Y=0, Z=5" ), TEXT( "Offset relative to head for location velocity debug indicator" ) );
	static FAutoConsoleVariable HeadRotationVelocityOffset( TEXT( "VREd.HeadRotationVelocityOffset" ), TEXT( "X=20, Y=0, Z=-5" ), TEXT( "Offset relative to head for rotation velocity debug indicator" ) );
	static FAutoConsoleVariable SFXMultiplier(TEXT("VREd.SFXMultiplier"), 1.5f, TEXT("Default Sound Effect Volume Multiplier"));

	static FAutoConsoleCommand ToggleDebugMode(TEXT("VREd.ToggleDebugMode"), TEXT("Toggles debug mode of the VR Mode"), FConsoleCommandDelegate::CreateStatic(&UVREditorMode::ToggleDebugMode));
}

const FString UVREditorMode::AssetContainerPath = FString("/Engine/VREditor/VREditorAssetContainerData");
bool UVREditorMode::bDebugModeEnabled = false;

UVREditorMode::UVREditorMode() :
	Super(),
	bWantsToExitMode( false ),
	bIsFullyInitialized( false ),
	AppTimeModeEntered( FTimespan::Zero() ),
	AvatarActor( nullptr ),
   	FlashlightComponent( nullptr ),
	bIsFlashlightOn( false ),
	MotionControllerID( 0 ),	// @todo vreditor minor: We only support a single controller, and we assume the first controller are the motion controls
	UISystem( nullptr ),
	TeleportActor( nullptr ),
	AutoScalerSystem( nullptr ),
	WorldInteraction( nullptr ),
	LeftHandInteractor( nullptr ),
	RightHandInteractor( nullptr ),
	bFirstTick( true ),
	SavedWorldToMetersScaleForPIE( 100.f ),
	bStartedPlayFromVREditor( false ),
	bStartedPlayFromVREditorSimulate( false ),
	AssetContainer( nullptr )
{
}

void UVREditorMode::Init()
{
	// @todo vreditor urgent: Turn on global editor hacks for VR Editor mode
	GEnableVREditorHacks = true;

	bIsFullyInitialized = false;
	bWantsToExitMode = false;

	AppTimeModeEntered = FTimespan::FromSeconds( FApp::GetCurrentTime() );

	// Take note of VREditor activation
	if( FEngineAnalytics::IsAvailable() )
	{
		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.Usage.InitVREditorMode" ) );
	}

	// Setting up colors
	Colors.SetNumZeroed( (int32)EColors::TotalCount );
	{	
		Colors[ (int32)EColors::DefaultColor ] = FLinearColor(0.701f, 0.084f, 0.075f, 1.0f);	
		Colors[ (int32)EColors::SelectionColor ] = FLinearColor(1.0f, 0.467f, 0.0f, 1.f);
		Colors[ (int32)EColors::WorldDraggingColor ] = FLinearColor(0.106, 0.487, 0.106, 1.0f);
		Colors[ (int32)EColors::UIColor ] = FLinearColor(0.22f, 0.7f, 0.98f, 1.0f);
		Colors[ (int32)EColors::UISelectionBarColor ] = FLinearColor( 0.025f, 0.025f, 0.025f, 1.0f );
		Colors[ (int32)EColors::UISelectionBarHoverColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonColor ] = FLinearColor( 0.1f, 0.1f, 0.1f, 1.0f );
		Colors[ (int32)EColors::UICloseButtonHoverColor ] = FLinearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	}

	{
		UEditorWorldExtensionCollection* Collection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld());
		check(Collection != nullptr);
		WorldInteraction = Cast<UViewportWorldInteraction>( Collection->FindExtension( UViewportWorldInteraction::StaticClass() ) );
		check( WorldInteraction != nullptr );
	}

	// Setup the asset container.
	AssetContainer = LoadObject<UVREditorAssetContainer>(nullptr, *UVREditorMode::AssetContainerPath);
	check(AssetContainer != nullptr);

	bIsFullyInitialized = true;
}

/*
* @EventName Editor.Usage.EnterVRMode
*
* @Trigger Entering VR editing mode
*
* @Type Static
*
* @EventParam HMDDevice (string) The name of the HMD Device type
*
* @Source Editor
*
* @Owner Lauren.Ridge
*
*/
void UVREditorMode::Shutdown()
{
	bIsFullyInitialized = false;
	
	AvatarActor = nullptr;
	FlashlightComponent = nullptr;
	UISystem = nullptr;
	TeleportActor = nullptr;
	AutoScalerSystem = nullptr;
	WorldInteraction = nullptr;
	LeftHandInteractor = nullptr;
	RightHandInteractor = nullptr;
	AssetContainer = nullptr;

	// @todo vreditor urgent: Disable global editor hacks for VR Editor mode
	GEnableVREditorHacks = false;

	FEditorDelegates::EndPIE.RemoveAll(this);
}

void UVREditorMode::Enter()
{
	bWantsToExitMode = false;

	{
		WorldInteraction->OnPreWorldInteractionTick().AddUObject( this, &UVREditorMode::PreTick );
		WorldInteraction->OnPostWorldInteractionTick().AddUObject( this, &UVREditorMode::PostTick );
	}

	FEditorDelegates::PostPIEStarted.AddUObject( this, &UVREditorMode::PostPIEStarted );
	FEditorDelegates::PrePIEEnded.AddUObject( this, &UVREditorMode::PrePIEEnded );
	FEditorDelegates::EndPIE.AddUObject(this, &UVREditorMode::OnEndPIE);
	FEditorDelegates::OnPreSwitchBeginPIEAndSIE.AddUObject(this, &UVREditorMode::OnPreSwitchPIEAndSIE);
	FEditorDelegates::OnSwitchBeginPIEAndSIE.AddUObject(this, &UVREditorMode::OnSwitchPIEAndSIE);


	// @todo vreditor: We need to make sure the user can never switch to orthographic mode, or activate settings that
	// would disrupt the user's ability to view the VR scene.

	// @todo vreditor: Don't bother drawing toolbars in VR, or other things that won't matter in VR

	{
		const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();

		// Do we have an active perspective viewport that is valid for VR?  If so, go ahead and use that.
		TSharedPtr<SLevelViewport> ExistingActiveLevelViewport;
		{
			TSharedPtr<ILevelViewport> ActiveLevelViewport = LevelEditor->GetActiveViewportInterface();
			if(ActiveLevelViewport.IsValid())
			{
				ExistingActiveLevelViewport = StaticCastSharedRef< SLevelViewport >(ActiveLevelViewport->AsWidget());
			}
		}

		StartViewport(ExistingActiveLevelViewport);

		if( bActuallyUsingVR )
		{
			// Tell Slate to require a larger pixel distance threshold before the drag starts.  This is important for things 
			// like Content Browser drag and drop.
			SavedEditorState.DragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
			FSlateApplication::Get().SetDragTriggerDistance( VREd::SlateDragDistanceOverride->GetFloat() );

			// When actually in VR, make sure the transform gizmo is big!
			SavedEditorState.TransformGizmoScale = WorldInteraction->GetTransformGizmoScale();
			WorldInteraction->SetTransformGizmoScale(GetDefault<UVRModeSettings>()->GizmoScale);
			WorldInteraction->SetShouldSuppressExistingCursor(true);
			WorldInteraction->SetInVR(true);

			// Take note of VREditor entering (only if actually in VR)
			if (FEngineAnalytics::IsAvailable())
			{
				TArray< FAnalyticsEventAttribute > Attributes;
				FString HMDName = GEditor->XRSystem->GetSystemName().ToString();
				Attributes.Add(FAnalyticsEventAttribute(TEXT("HMDDevice"), HMDName));
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.EnterVRMode"), Attributes);
			}
		}
	}

	// Switch us back to placement mode and close any open sequencer windows
	FVREditorActionCallbacks::ChangeEditorModes(FBuiltinEditorModes::EM_Placement);
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.GetLevelEditorTabManager()->InvokeTab(FTabId("Sequencer"))->RequestCloseTab();

	// Setup our avatar
	if (AvatarActor == nullptr)
	{
		const bool bWithSceneComponent = true;
		AvatarActor = SpawnTransientSceneActor<AVREditorAvatarActor>(TEXT("AvatarActor"), bWithSceneComponent);
		AvatarActor->Init(this);

		WorldInteraction->AddActorToExcludeFromHitTests( AvatarActor );	
	}

	// If we're actually using VR, go ahead and disable notifications.  We won't be able to see them in VR
	// currently, and they can introduce performance issues if they pop up on the desktop
	if( bActuallyUsingVR )
	{
		FSlateNotificationManager::Get().SetAllowNotifications( false );
	}

	// Setup sub systems
	{
		// Setup world interaction
		// We need input preprocessing for VR so that we can receive motion controller input without any viewports having 
		// to be focused.  This is mainly because Slate UI injected into the 3D world can cause focus to be lost unexpectedly,
		// but we need the user to still be able to interact with UI.
		WorldInteraction->SetUseInputPreprocessor( true );

		// Motion controllers
		{
			LeftHandInteractor = NewObject<UVREditorMotionControllerInteractor>();
			LeftHandInteractor->SetControllerHandSide( EControllerHand::Left );
			LeftHandInteractor->Init( this );
			WorldInteraction->AddInteractor( LeftHandInteractor );

			RightHandInteractor = NewObject<UVREditorMotionControllerInteractor>();
			RightHandInteractor->SetControllerHandSide( EControllerHand::Right );
			RightHandInteractor->Init( this );
			WorldInteraction->AddInteractor( RightHandInteractor );

			WorldInteraction->PairInteractors( LeftHandInteractor, RightHandInteractor );
		}

		if( bActuallyUsingVR )
		{
			// When actually using VR devices, we don't want a mouse cursor interactor
			WorldInteraction->ReleaseMouseCursorInteractor();
		}

		// Setup the UI system
		UISystem = NewObject<UVREditorUISystem>();
		UISystem->Init(this);

		PlacementSystem = NewObject<UVREditorPlacement>();
		PlacementSystem->Init(this);

		// Setup teleporter
		TeleportActor = SpawnTransientSceneActor<AVREditorTeleporter>( TEXT("Teleporter"), true );
		TeleportActor->Init( this );
		WorldInteraction->AddActorToExcludeFromHitTests( TeleportActor );	

		// Setup autoscaler
		AutoScalerSystem = NewObject<UVREditorAutoScaler>();
		AutoScalerSystem->Init( this );

		LeftHandInteractor->SetupComponent( AvatarActor );
		RightHandInteractor->SetupComponent( AvatarActor );
	}


	/** This will make sure this is not ticking after the editor has been closed. */
	GEditor->OnEditorClose().AddUObject( this, &UVREditorMode::OnEditorClosed );

	bFirstTick = true;
	SetActive(true);
	bStartedPlayFromVREditor = false;
	bStartedPlayFromVREditorSimulate = false;
}

void UVREditorMode::Exit(const bool bShouldDisableStereo)
{
	{
		GetLevelViewportPossessedForVR().RemoveAllPreviews();
		GEditor->SelectNone(true, true, false);
		GEditor->NoteSelectionChange();
		FVREditorActionCallbacks::ChangeEditorModes(FBuiltinEditorModes::EM_Placement);
		
		//Destroy the avatar
		{
			DestroyTransientActor(AvatarActor);
			AvatarActor = nullptr;
			FlashlightComponent = nullptr;
		}

		{
			if(bActuallyUsingVR)
			{
				// Restore Slate drag trigger distance
				FSlateApplication::Get().SetDragTriggerDistance( SavedEditorState.DragTriggerDistance );

				// Restore gizmo size
				WorldInteraction->SetTransformGizmoScale( SavedEditorState.TransformGizmoScale );
				WorldInteraction->SetShouldSuppressExistingCursor(false);

				// Take note of VREditor exiting (only if actually in VR)
				if (FEngineAnalytics::IsAvailable())
				{
					FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.ExitVRMode"));
				}
			}

			CloseViewport( bShouldDisableStereo );

			VREditorLevelViewportWeakPtr.Reset();
			OnVREditingModeExit_Handler.ExecuteIfBound();
		}

		// Kill the VR editor window
		TSharedPtr<SWindow> VREditorWindow( VREditorWindowWeakPtr.Pin() );
		if(VREditorWindow.IsValid())
		{
			VREditorWindow->RequestDestroyWindow();
			VREditorWindow.Reset();
		}
	}

	// Kill subsystems
	if( UISystem != nullptr )
	{
		UISystem->Shutdown();
		UISystem->MarkPendingKill();
		UISystem = nullptr;
	}

	if( PlacementSystem != nullptr )
	{
		PlacementSystem->Shutdown();
		PlacementSystem->MarkPendingKill();
		PlacementSystem = nullptr;
	}

	if( TeleportActor != nullptr )
	{
		DestroyTransientActor( TeleportActor );
		TeleportActor = nullptr;
	}

	if( AutoScalerSystem != nullptr )
	{
		AutoScalerSystem->Shutdown();
		AutoScalerSystem->MarkPendingKill();
		AutoScalerSystem = nullptr;
	}

	if( WorldInteraction != nullptr )
	{
		WorldInteraction->SetUseInputPreprocessor( false );

		WorldInteraction->OnHandleKeyInput().RemoveAll( this );
		WorldInteraction->OnPreWorldInteractionTick().RemoveAll( this );
		WorldInteraction->OnPostWorldInteractionTick().RemoveAll( this );

		WorldInteraction->RemoveInteractor( LeftHandInteractor );
		LeftHandInteractor->MarkPendingKill();
		LeftHandInteractor = nullptr;

		WorldInteraction->RemoveInteractor( RightHandInteractor );
		RightHandInteractor->MarkPendingKill();
		RightHandInteractor = nullptr;
		
		// Restore the mouse cursor if we removed it earlier
		if( bActuallyUsingVR )
		{
			WorldInteraction->AddMouseCursorInteractor();
			WorldInteraction->SetInVR(false);
		}
	}

	if( bActuallyUsingVR )
	{
		FSlateNotificationManager::Get().SetAllowNotifications( true);
	}

	AssetContainer = nullptr;

	FEditorDelegates::PostPIEStarted.RemoveAll( this );
	FEditorDelegates::PrePIEEnded.RemoveAll( this );
	FEditorDelegates::EndPIE.RemoveAll( this );
	FEditorDelegates::OnPreSwitchBeginPIEAndSIE.RemoveAll(this);
	FEditorDelegates::OnSwitchBeginPIEAndSIE.RemoveAll(this);

	GEditor->OnEditorClose().RemoveAll( this );

	if (GEditor->bIsSimulatingInEditor)
	{
		GEditor->RequestEndPlayMap();
	}

	bWantsToExitMode = false;
	SetActive(false);
	bFirstTick = false;
}

void UVREditorMode::OnEditorClosed()
{
	if(IsActive())
	{
		Exit( false );
		Shutdown();
	}
}

void UVREditorMode::StartExitingVRMode()
{
	bWantsToExitMode = true;
}

void UVREditorMode::OnVREditorWindowClosed( const TSharedRef<SWindow>& ClosedWindow )
{
	StartExitingVRMode();
}

void UVREditorMode::PreTick( const float DeltaTime )
{
	if( !bIsFullyInitialized || !IsActive() || bWantsToExitMode )
	{
		return;
	}

	//Setting the initial position and rotation based on the editor viewport when going into VR mode
	if( bFirstTick && bActuallyUsingVR )
	{
		const FTransform RoomToWorld = GetRoomTransform();
		const FTransform WorldToRoom = RoomToWorld.Inverse();
		FTransform ViewportToWorld = FTransform( SavedEditorState.ViewRotation, SavedEditorState.ViewLocation );
		FTransform ViewportToRoom = ( ViewportToWorld * WorldToRoom );

		FTransform ViewportToRoomYaw = ViewportToRoom;
		ViewportToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, ViewportToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform HeadToRoomYaw = GetRoomSpaceHeadTransform();
		HeadToRoomYaw.SetRotation( FQuat( FRotator( 0.0f, HeadToRoomYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform RoomToWorldYaw = RoomToWorld;
		RoomToWorldYaw.SetRotation( FQuat( FRotator( 0.0f, RoomToWorldYaw.GetRotation().Rotator().Yaw, 0.0f ) ) );

		FTransform ResultToWorld = ( HeadToRoomYaw.Inverse() * ViewportToRoomYaw ) * RoomToWorldYaw;
		SetRoomTransform( ResultToWorld );
	}
}

void UVREditorMode::PostTick( float DeltaTime )
{
	if( !bIsFullyInitialized || !IsActive() || bWantsToExitMode || !VREditorLevelViewportWeakPtr.IsValid() )
	{
		return;
	}

	TickHandle.Broadcast( DeltaTime );
	UISystem->Tick( GetLevelViewportPossessedForVR().GetViewportClient().Get(), DeltaTime );

	// Update avatar meshes
	{
		// Move our avatar mesh along with the room.  We need our hand components to remain the same coordinate space as the 
		AvatarActor->SetActorTransform( GetRoomTransform() );
		AvatarActor->TickManually( DeltaTime );


	}

	// Updating the scale and intensity of the flashlight according to the world scale
	if (FlashlightComponent)
	{
		float CurrentFalloffExponent = FlashlightComponent->LightFalloffExponent;
		//@todo vreditor tweak
		float UpdatedFalloffExponent = FMath::Clamp(CurrentFalloffExponent / GetWorldScaleFactor(), 2.0f, 16.0f);
		FlashlightComponent->SetLightFalloffExponent(UpdatedFalloffExponent);
	}

	if( WorldInteraction->HaveHeadTransform() && VREd::ShowHeadVelocity->GetInt() != 0 )
	{
		const FTransform RoomSpaceHeadToWorld = WorldInteraction->GetRoomSpaceHeadTransform();
		static FTransform LastRoomSpaceHeadToWorld = RoomSpaceHeadToWorld;

		const float WorldScaleFactor = WorldInteraction->GetWorldScaleFactor();
		static float LastWorldScaleFactor = WorldScaleFactor;

		const float MinInnerRadius = VREd::HeadVelocityMinRadius->GetFloat() * WorldScaleFactor;
		const float MaxOuterRadius = VREd::HeadVelocityMaxRadius->GetFloat() * WorldScaleFactor;
		const float MinThickness = VREd::HeadVelocityMinLineThickness->GetFloat() * WorldScaleFactor;
		const float MaxThickness = VREd::HeadVelocityMaxLineThickness->GetFloat() * WorldScaleFactor;

		const float MaxLocationVelocity = VREd::HeadLocationMaxVelocity->GetFloat();	// cm/s
		const float MaxRotationVelocity = VREd::HeadRotationMaxVelocity->GetFloat();	// degrees/s

		const float LocationVelocity = ( LastRoomSpaceHeadToWorld.GetLocation() / LastWorldScaleFactor - RoomSpaceHeadToWorld.GetLocation() / WorldScaleFactor ).Size() / DeltaTime;

		const float YawVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Yaw, RoomSpaceHeadToWorld.GetRotation().Rotator().Yaw ) ) / DeltaTime;
		const float PitchVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Pitch, RoomSpaceHeadToWorld.GetRotation().Rotator().Pitch ) ) / DeltaTime;
		const float RollVelocity = FMath::Abs( FMath::FindDeltaAngleDegrees( LastRoomSpaceHeadToWorld.GetRotation().Rotator().Roll, RoomSpaceHeadToWorld.GetRotation().Rotator().Roll ) ) / DeltaTime;
		const float RotationVelocity = YawVelocity + PitchVelocity + RollVelocity;

		static float LastLocationVelocity = LocationVelocity;
		static float LastRotationVelocity = RotationVelocity;

		const float SmoothLocationVelocity = FMath::Lerp( LocationVelocity, LastLocationVelocity, VREd::HeadVelocitySmoothing->GetFloat() );
		const float SmoothRotationVelocity = FMath::Lerp( RotationVelocity, LastRotationVelocity, VREd::HeadVelocitySmoothing->GetFloat() );

		LastLocationVelocity = SmoothLocationVelocity;
		LastRotationVelocity = SmoothRotationVelocity;
		
		LastRoomSpaceHeadToWorld = RoomSpaceHeadToWorld;
		LastWorldScaleFactor = WorldScaleFactor;

		const float LocationVelocityAlpha = FMath::Clamp( SmoothLocationVelocity / MaxLocationVelocity, 0.0f, 1.0f );
		const float RotationVelocityAlpha = FMath::Clamp( SmoothRotationVelocity / MaxRotationVelocity, 0.0f, 1.0f );

		const FTransform HeadToWorld = WorldInteraction->GetHeadTransform();

		{
			FVector HeadLocationVelocityOffset = FVector::ZeroVector;
			HeadLocationVelocityOffset.InitFromString( VREd::HeadLocationVelocityOffset->GetString() );
			HeadLocationVelocityOffset *= WorldScaleFactor;

			const FColor Color = FColor::MakeFromColorTemperature( 6000.0f - LocationVelocityAlpha * 5000.0f );
			const float Thickness = FMath::Lerp( MinThickness, MaxThickness, LocationVelocityAlpha );
			const FTransform UIToHeadTransform = FTransform( FRotator( 0.0f, 0.0f, 0.0f ).Quaternion(), HeadLocationVelocityOffset );
			const FTransform UIToWorld = UIToHeadTransform * HeadToWorld;
			DrawDebug2DDonut( GetWorld(), UIToWorld.ToMatrixNoScale(), MinInnerRadius, FMath::Lerp( MinInnerRadius, MaxOuterRadius, LocationVelocityAlpha ), 64, Color, false, 0.0f, SDPG_World, Thickness );
		}

		{
			FVector HeadRotationVelocityOffset = FVector::ZeroVector;
			HeadRotationVelocityOffset.InitFromString( VREd::HeadRotationVelocityOffset->GetString() );
			HeadRotationVelocityOffset *= WorldScaleFactor;

			const FColor Color = FColor::MakeFromColorTemperature( 6000.0f - RotationVelocityAlpha * 5000.0f );
			const float Thickness = FMath::Lerp( MinThickness, MaxThickness, RotationVelocityAlpha );
			const FTransform UIToHeadTransform = FTransform( FRotator( 0.0f, 0.0f, 0.0f ).Quaternion(), HeadRotationVelocityOffset );
			const FTransform UIToWorld = UIToHeadTransform * HeadToWorld;
			DrawDebug2DDonut( GetWorld(), UIToWorld.ToMatrixNoScale(), MinInnerRadius, FMath::Lerp( MinInnerRadius, MaxOuterRadius, RotationVelocityAlpha ), 64, Color, false, 0.0f, SDPG_World, Thickness );
		}
	}

	bFirstTick = false;
}

FTransform UVREditorMode::GetRoomTransform() const
{
	return WorldInteraction->GetRoomTransform();
}

void UVREditorMode::SetRoomTransform( const FTransform& NewRoomTransform )
{
	WorldInteraction->SetRoomTransform( NewRoomTransform );
}

FTransform UVREditorMode::GetRoomSpaceHeadTransform() const
{
	return WorldInteraction->GetRoomSpaceHeadTransform();
}

FTransform UVREditorMode::GetHeadTransform() const
{
	return WorldInteraction->GetHeadTransform();
}

const UViewportWorldInteraction& UVREditorMode::GetWorldInteraction() const
{
	return *WorldInteraction;
}

UViewportWorldInteraction& UVREditorMode::GetWorldInteraction()
{
	return *WorldInteraction;
}

bool UVREditorMode::IsFullyInitialized() const
{
	return bIsFullyInitialized;
}

bool UVREditorMode::IsShowingRadialMenu(const UVREditorInteractor* Interactor) const
{
	return UISystem->IsShowingRadialMenu(Interactor);
}

const SLevelViewport& UVREditorMode::GetLevelViewportPossessedForVR() const
{
	return *VREditorLevelViewportWeakPtr.Pin();
}

SLevelViewport& UVREditorMode::GetLevelViewportPossessedForVR()
{
	return *VREditorLevelViewportWeakPtr.Pin();
}


float UVREditorMode::GetWorldScaleFactor() const
{
	return WorldInteraction->GetWorldScaleFactor();
}

void UVREditorMode::ToggleFlashlight( UVREditorInteractor* Interactor )
{
	UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast<UVREditorMotionControllerInteractor>( Interactor );
	if ( MotionControllerInteractor )
	{
		if ( FlashlightComponent == nullptr )
		{
			FlashlightComponent = NewObject<USpotLightComponent>( AvatarActor );
			AvatarActor->AddOwnedComponent( FlashlightComponent );
			FlashlightComponent->RegisterComponent();
			FlashlightComponent->SetMobility( EComponentMobility::Movable );
			FlashlightComponent->SetCastShadows( false );
			FlashlightComponent->bUseInverseSquaredFalloff = false;
			//@todo vreditor tweak
			FlashlightComponent->SetLightFalloffExponent( 8.0f );
			FlashlightComponent->SetIntensity( 20.0f );
			FlashlightComponent->SetOuterConeAngle( 25.0f );
			FlashlightComponent->SetInnerConeAngle( 25.0f );

		}

		const FAttachmentTransformRules AttachmentTransformRules = FAttachmentTransformRules( EAttachmentRule::KeepRelative, true );
		FlashlightComponent->AttachToComponent( MotionControllerInteractor->GetMotionControllerComponent(), AttachmentTransformRules );
		bIsFlashlightOn = !bIsFlashlightOn;
		FlashlightComponent->SetVisibility( bIsFlashlightOn );
	}
}

void UVREditorMode::CycleTransformGizmoHandleType()
{
	EGizmoHandleTypes NewGizmoType = (EGizmoHandleTypes)( (uint8)WorldInteraction->GetCurrentGizmoType() + 1 );
	
	if( NewGizmoType > EGizmoHandleTypes::Scale )
	{
		NewGizmoType = EGizmoHandleTypes::All;
	}

	WorldInteraction->SetGizmoHandleType( NewGizmoType );
}

EHMDDeviceType::Type UVREditorMode::GetHMDDeviceType() const
{
	return GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() ? GEngine->XRSystem->GetHMDDevice()->GetHMDDeviceType() : EHMDDeviceType::DT_SteamVR;
}

FLinearColor UVREditorMode::GetColor( const EColors Color ) const
{
	return Colors[ (int32)Color ];
}

float UVREditorMode::GetDefaultVRNearClipPlane() const 
{
	return VREd::DefaultVRNearClipPlane->GetFloat();
}

void UVREditorMode::RefreshVREditorSequencer(class ISequencer* InCurrentSequencer)
{
	CurrentSequencer = InCurrentSequencer;
	// Tell the VR Editor UI system to refresh the Sequencer UI
	if (bActuallyUsingVR && UISystem != nullptr)
	{
		GetUISystem().UpdateSequencerUI();
	}
}

void UVREditorMode::RefreshActorPreviewWidget(TSharedRef<SWidget> InWidget)
{
	if (bActuallyUsingVR && UISystem != nullptr)
	{
		GetUISystem().UpdateActorPreviewUI(InWidget);
	}
}

class ISequencer* UVREditorMode::GetCurrentSequencer()
{
	return CurrentSequencer;
}

bool UVREditorMode::IsHandAimingTowardsCapsule(UViewportInteractor* Interactor, const FTransform& CapsuleTransform, FVector CapsuleStart, const FVector CapsuleEnd, const float CapsuleRadius, const float MinDistanceToCapsule, const FVector CapsuleFrontDirection, const float MinDotForAimingAtCapsule) const
{
	bool bIsAimingTowards = false;
	const float WorldScaleFactor = GetWorldScaleFactor();

	FVector LaserPointerStart, LaserPointerEnd;
	if( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
	{
		const FVector LaserPointerStartInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerStart );
		const FVector LaserPointerEndInCapsuleSpace = CapsuleTransform.InverseTransformPosition( LaserPointerEnd );

		FVector ClosestPointOnLaserPointer, ClosestPointOnUICapsule;
		FMath::SegmentDistToSegment(
			LaserPointerStartInCapsuleSpace, LaserPointerEndInCapsuleSpace,
			CapsuleStart, CapsuleEnd,
			/* Out */ ClosestPointOnLaserPointer,
			/* Out */ ClosestPointOnUICapsule );

		const bool bIsClosestPointInsideCapsule = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).Size() <= CapsuleRadius;

		const FVector TowardLaserPointerVector = ( ClosestPointOnLaserPointer - ClosestPointOnUICapsule ).GetSafeNormal();

		// Apply capsule radius
		ClosestPointOnUICapsule += TowardLaserPointerVector * CapsuleRadius;

		if( false )	// @todo vreditor debug
		{
			const float RenderCapsuleLength = ( CapsuleEnd - CapsuleStart ).Size() + CapsuleRadius * 2.0f;
			// @todo vreditor:  This capsule draws with the wrong orientation
			if( false )
			{
				DrawDebugCapsule( GetWorld(), CapsuleTransform.TransformPosition( CapsuleStart + ( CapsuleEnd - CapsuleStart ) * 0.5f ), RenderCapsuleLength * 0.5f, CapsuleRadius, CapsuleTransform.GetRotation() * FRotator( 90.0f, 0, 0 ).Quaternion(), FColor::Green, false, 0.0f );
			}
			DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), FColor::Green, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnLaserPointer ), 1.5f * WorldScaleFactor, 32, FColor::Red, false, 0.0f );
			DrawDebugSphere( GetWorld(), CapsuleTransform.TransformPosition( ClosestPointOnUICapsule ), 1.5f * WorldScaleFactor, 32, FColor::Green, false, 0.0f );
		}

		// If we're really close to the capsule
		if( bIsClosestPointInsideCapsule ||
			( ClosestPointOnUICapsule - ClosestPointOnLaserPointer ).Size() <= MinDistanceToCapsule )
		{
			const FVector LaserPointerDirectionInCapsuleSpace = ( LaserPointerEndInCapsuleSpace - LaserPointerStartInCapsuleSpace ).GetSafeNormal();

			if( false )	// @todo vreditor debug
			{
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( CapsuleFrontDirection * 5.0f ), FColor::Yellow, false, 0.0f );
				DrawDebugLine( GetWorld(), CapsuleTransform.TransformPosition( FVector::ZeroVector ), CapsuleTransform.TransformPosition( -LaserPointerDirectionInCapsuleSpace * 5.0f ), FColor::Purple, false, 0.0f );
			}

			const float Dot = FVector::DotProduct( CapsuleFrontDirection, -LaserPointerDirectionInCapsuleSpace );
			if( Dot >= MinDotForAimingAtCapsule )
			{
				bIsAimingTowards = true;
			}
		}
	}

	return bIsAimingTowards;
}

UVREditorInteractor* UVREditorMode::GetHandInteractor( const EControllerHand ControllerHand ) const 
{
	UVREditorInteractor* ResultInteractor = ControllerHand == EControllerHand::Left ? LeftHandInteractor : RightHandInteractor;
	check( ResultInteractor != nullptr );
	return ResultInteractor;
}

void UVREditorMode::SnapSelectedActorsToGround()
{
	TSharedPtr<SLevelViewport> LevelEditorViewport = StaticCastSharedPtr<SLevelViewport>(WorldInteraction->GetDefaultOptionalViewportClient()->GetEditorViewportWidget());
	if (LevelEditorViewport.IsValid())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		const FLevelEditorCommands& Commands = LevelEditorModule.GetLevelEditorCommands();
		const TSharedPtr< FUICommandList >& CommandList = LevelEditorViewport->GetParentLevelEditor().Pin()->GetLevelEditorActions(); //@todo vreditor: Cast on leveleditor

		CommandList->ExecuteAction(Commands.SnapBottomCenterBoundsToFloor.ToSharedRef());

		// Force transformables to refresh
		GEditor->NoteSelectionChange();
	}
}

const UVREditorMode::FSavedEditorState& UVREditorMode::GetSavedEditorState() const
{
	return SavedEditorState;
}

void UVREditorMode::SaveSequencerSettings(bool bInKeyAllEnabled, EAutoChangeMode InAutoChangeMode, const class USequencerSettings& InSequencerSettings)
{
	SavedEditorState.bKeyAllEnabled = bInKeyAllEnabled;
	SavedEditorState.AutoChangeMode = InAutoChangeMode;
}

void UVREditorMode::ToggleSIEAndVREditor()
{
	if (GEditor->EditorWorld == nullptr && !GEditor->bIsSimulatingInEditor)
	{
		const FVector* StartLoc = NULL;
		const FRotator* StartRot = NULL;
		GEditor->RequestPlaySession(false, VREditorLevelViewportWeakPtr.Pin(), true /*bSimulateInEditor*/, StartLoc, StartRot, -1);
	}
	else if (GEditor->PlayWorld != nullptr && GEditor->bIsSimulatingInEditor)
	{
		GEditor->RequestEndPlayMap();
	}
}

void UVREditorMode::TogglePIEAndVREditor()
{
	bool bRequestedPIE = false;
	if (GEditor->EditorWorld == nullptr && GEditor->PlayWorld == nullptr && !GEditor->bIsSimulatingInEditor)
	{
		const FVector* StartLoc = NULL;
		const FRotator* StartRot = NULL;
		const bool bHMDIsReady = (GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->IsHMDConnected());
		GEditor->RequestPlaySession(true, VREditorLevelViewportWeakPtr.Pin(), false /*bSimulateInEditor*/, StartLoc, StartRot, -1, false, bHMDIsReady);
		bRequestedPIE = true;
	}
	else if (GEditor->PlayWorld != nullptr)
	{
		// Since we are already in simulate, we want to toggle to PIE.
		if (GEditor->bIsSimulatingInEditor)
		{
			bStartedPlayFromVREditorSimulate = true;
			bRequestedPIE = true;

			GEditor->RequestToggleBetweenPIEandSIE();
		}
		else
		{
			// If this play started while in simulate, then toggle back to simulate.
			if (bStartedPlayFromVREditorSimulate)
			{
				GEditor->RequestToggleBetweenPIEandSIE();
			}
			else
			{
				GEditor->RequestEndPlayMap();
			}
		}
	}

	if (bRequestedPIE)
	{
		// Turn off input processing while in PIE.  We don't want any input events until the user comes back to the editor
		WorldInteraction->SetUseInputPreprocessor(false);

		SavedWorldToMetersScaleForPIE = GetWorld()->GetWorldSettings()->WorldToMeters;

		// Restore the world to meters before entering play
		RestoreWorldToMeters();

		SetActive(false);
		WorldInteraction->SetActive(false);
		bStartedPlayFromVREditor = true;
	}
}

void UVREditorMode::TransitionWorld(UWorld* NewWorld)
{
	Super::TransitionWorld(NewWorld);

	UISystem->TransitionWorld(NewWorld);
}

void UVREditorMode::StartViewport(TSharedPtr<SLevelViewport> Viewport)
{
	if (false)
	{
		const TSharedRef< ILevelEditor >& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").GetFirstLevelEditor().ToSharedRef();

		// @todo vreditor: The resolution we set here doesn't matter, as HMDs will draw at their native resolution
		// no matter what.  We should probably allow the window to be freely resizable by the user
		// @todo vreditor: Should save and restore window position and size settings
		FVector2D WindowSize;
		{
			IHeadMountedDisplay::MonitorInfo HMDMonitorInfo;
			if (bActuallyUsingVR && GEngine->XRSystem->GetHMDDevice() && GEngine->XRSystem->GetHMDDevice()->GetHMDMonitorInfo(HMDMonitorInfo))
			{
				WindowSize = FVector2D(HMDMonitorInfo.ResolutionX, HMDMonitorInfo.ResolutionY);
			}
			else
			{
				// @todo vreditor: Hard-coded failsafe window size
				WindowSize = FVector2D(1920.0f, 1080.0f);
			}
		}

		// @todo vreditor: Use SLevelEditor::GetTableTitle() for the VR window title (needs dynamic update)
		const FText VREditorWindowTitle = NSLOCTEXT("VREditor", "VRWindowTitle", "Unreal Editor VR");

		TSharedRef< SWindow > VREditorWindow = SNew(SWindow)
			.Title(VREditorWindowTitle)
			.ClientSize(WindowSize)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.UseOSWindowBorder(true)	// @todo vreditor: Allow window to be freely resized?  Shouldn't really hurt anything.  We should save position/size too.
			.SizingRule(ESizingRule::UserSized);
		this->VREditorWindowWeakPtr = VREditorWindow;

		Viewport =
			SNew(SLevelViewport)
			.ViewportType(LVT_Perspective) // Perspective
			.Realtime(true)
			//				.ParentLayout( AsShared() )	// @todo vreditor: We don't have one and we probably don't need one, right?  Make sure a null parent layout is handled properly everywhere.
			.ParentLevelEditor(LevelEditor)
			//				.ConfigKey( BottomLeftKey )	// @todo vreditor: This is for saving/loading layout.  We would need this in order to remember viewport settings like show flags, etc.
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());

		// Allow the editor to keep track of this editor viewport.  Because it's not inside of a normal tab, 
		// we need to explicitly tell the level editor about it
		LevelEditor->AddStandaloneLevelViewport(Viewport.ToSharedRef());

		VREditorWindow->SetContent(Viewport.ToSharedRef());

		// NOTE: We're intentionally not adding this window natively parented to the main frame window, because we don't want it
		// to minimize/restore when the main frame is minimized/restored
		FSlateApplication::Get().AddWindow(VREditorWindow);

		VREditorWindow->SetOnWindowClosed(FOnWindowClosed::CreateUObject(this, &UVREditorMode::OnVREditorWindowClosed));

		VREditorWindow->BringToFront();	// @todo vreditor: Not sure if this is needed, especially if we decide the window should be hidden (copied this from PIE code)
	}
	else
	{
		if (bActuallyUsingVR && !Viewport->IsImmersive())
		{
			// Switch to immersive mode
			const bool bWantImmersive = true;
			const bool bAllowAnimation = false;
			Viewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}
	}

	this->VREditorLevelViewportWeakPtr = Viewport;

	{
		FLevelEditorViewportClient& VRViewportClient = Viewport->GetLevelViewportClient();
		FEditorViewportClient& VREditorViewportClient = VRViewportClient;

		// Make sure we are in perspective mode
		// @todo vreditor: We should never allow ortho switching while in VR
		SavedEditorState.ViewportType = VREditorViewportClient.GetViewportType();
		VREditorViewportClient.SetViewportType(LVT_Perspective);

		// Set the initial camera location
		// @todo vreditor: This should instead be calculated using the currently active perspective camera's
		// location and orientation, compensating for the current HMD offset from the tracking space origin.
		// Perhaps, we also want to teleport the original viewport's camera back when we exit this mode, too!
		// @todo vreditor: Should save and restore camera position and any other settings we change (viewport type, pitch locking, etc.)
		SavedEditorState.ViewLocation = VRViewportClient.GetViewLocation();
		SavedEditorState.ViewRotation = VRViewportClient.GetViewRotation();

		// Don't allow the tracking space to pitch up or down.  People hate that in VR.
		// @todo vreditor: This doesn't seem to prevent people from pitching the camera with RMB drag
		SavedEditorState.bLockedPitch = VRViewportClient.GetCameraController()->GetConfig().bLockedPitch;
		if (bActuallyUsingVR)
		{
			VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = true;
		}

		// Set "game mode" to be enabled, to get better performance.  Also hit proxies won't work in VR, anyway
		SavedEditorState.bGameView = VREditorViewportClient.IsInGameView();
		VREditorViewportClient.SetGameView(true);

		SavedEditorState.bRealTime = VREditorViewportClient.IsRealtime();
		VREditorViewportClient.SetRealtime(true);

		SavedEditorState.ShowFlags = VREditorViewportClient.EngineShowFlags;

		// Never show the traditional Unreal transform widget.  It doesn't work in VR because we don't have hit proxies.
		VREditorViewportClient.EngineShowFlags.SetModeWidgets(false);

		// Make sure the mode widgets don't come back when users click on things
		VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = false;

		// Force tiny near clip plane distance, because user can scale themselves to be very small.
		SavedEditorState.NearClipPlane = GNearClippingPlane;
		GNearClippingPlane = GetDefaultVRNearClipPlane();

		SavedEditorState.bOnScreenMessages = GAreScreenMessagesEnabled;
		GAreScreenMessagesEnabled = false;

		// Save the world to meters scale
		{
			const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
			const float SavedWorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : VRViewportClient.GetWorld()->GetWorldSettings()->WorldToMeters;
			SavedEditorState.WorldToMetersScale = SavedWorldToMeters;
			SavedWorldToMetersScaleForPIE = SavedWorldToMeters;
		}

		if (bActuallyUsingVR)
		{
			SavedEditorState.TrackingOrigin = GEngine->XRSystem->GetTrackingOrigin();
			GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::Floor);
		}

		// Make the new viewport the active level editing viewport right away
		GCurrentLevelEditingViewportClient = &VRViewportClient;

		// Enable selection outline right away
		VREditorViewportClient.EngineShowFlags.SetSelection(true);
		VREditorViewportClient.EngineShowFlags.SetSelectionOutline(true);

		// Change viewport settings to more VR-friendly sequencer settings
		SavedEditorState.bCinematicPreviewViewport = VRViewportClient.AllowsCinematicPreview();
		VRViewportClient.SetAllowCinematicPreview(false);
		// Need to force fading and color scaling off in case we enter VR editing mode with a sequence open
		VRViewportClient.bEnableFading = false;
		VRViewportClient.bEnableColorScaling = false;
		VRViewportClient.Invalidate(true);
	}

	if (bActuallyUsingVR)
	{
		Viewport->EnableStereoRendering( bActuallyUsingVR );
		Viewport->SetRenderDirectlyToWindow( bActuallyUsingVR );

		GEngine->StereoRenderingDevice->EnableStereo(true);
	}

	if (WorldInteraction != nullptr)
	{
		TSharedPtr<FEditorViewportClient> VRViewportClient = Viewport->GetViewportClient();
		WorldInteraction->SetDefaultOptionalViewportClient(VRViewportClient);
	}
}

void UVREditorMode::CloseViewport( const bool bShouldDisableStereo )
{
	if (bActuallyUsingVR && GEngine->XRSystem.IsValid() && bShouldDisableStereo)
	{
		GEngine->StereoRenderingDevice->EnableStereo(false);
	}

	TSharedPtr<SLevelViewport> VREditorLevelViewport(VREditorLevelViewportWeakPtr.Pin());
	if (VREditorLevelViewport.IsValid())
	{
		if( bShouldDisableStereo && bActuallyUsingVR )
		{
			VREditorLevelViewport->EnableStereoRendering(false);
			VREditorLevelViewport->SetRenderDirectlyToWindow(false);
		}

		{
			FLevelEditorViewportClient& VRViewportClient = VREditorLevelViewport->GetLevelViewportClient();
			FEditorViewportClient& VREditorViewportClient = VRViewportClient;

			// Restore settings that we changed on the viewport
			VREditorViewportClient.SetViewportType(SavedEditorState.ViewportType);
			VRViewportClient.GetCameraController()->AccessConfig().bLockedPitch = SavedEditorState.bLockedPitch;
			VRViewportClient.bAlwaysShowModeWidgetAfterSelectionChanges = SavedEditorState.bAlwaysShowModeWidgetAfterSelectionChanges;
			VRViewportClient.EngineShowFlags = SavedEditorState.ShowFlags;
			VRViewportClient.SetGameView(SavedEditorState.bGameView);
			VRViewportClient.SetAllowCinematicPreview(SavedEditorState.bCinematicPreviewViewport);
			VRViewportClient.bEnableFading = true;
			VRViewportClient.bEnableColorScaling = true;
			VRViewportClient.Invalidate(true);

			if (bActuallyUsingVR)
			{
				VRViewportClient.SetViewLocation(GetHeadTransform().GetLocation());

				FRotator HeadRotationNoRoll = GetHeadTransform().GetRotation().Rotator();
				HeadRotationNoRoll.Roll = 0.0f;
				VRViewportClient.SetViewRotation(HeadRotationNoRoll); // Use SavedEditorState.ViewRotation to go back to start rot
			}

			VRViewportClient.SetRealtime(SavedEditorState.bRealTime);

			GNearClippingPlane = SavedEditorState.NearClipPlane;
			GAreScreenMessagesEnabled = SavedEditorState.bOnScreenMessages;

			if (bActuallyUsingVR)
			{
				GEngine->XRSystem->SetTrackingOrigin(SavedEditorState.TrackingOrigin);
			}

			RestoreWorldToMeters();
		}

		if (bActuallyUsingVR && bShouldDisableStereo)
		{
			// Leave immersive mode
			const bool bWantImmersive = false;
			const bool bAllowAnimation = false;
			VREditorLevelViewport->MakeImmersive(bWantImmersive, bAllowAnimation);
		}
	}
}

void UVREditorMode::RestoreFromPIE()
{
	SetActive(true);
	bStartedPlayFromVREditorSimulate = false;

	GetWorld()->GetWorldSettings()->WorldToMeters = SavedWorldToMetersScaleForPIE;
	WorldInteraction->SetWorldToMetersScale(SavedWorldToMetersScaleForPIE);

	// Re-enable input pre-processing
	WorldInteraction->SetUseInputPreprocessor(true);
	WorldInteraction->SetActive(true);

	UVREditorMotionControllerInteractor* UIInteractor = UISystem->GetUIInteractor();
	if (UIInteractor != nullptr)
	{
		UIInteractor->ResetTrackpad();
		UISystem->HideRadialMenu(false, false);
	}
}

void UVREditorMode::RestoreWorldToMeters()
{
	const float DefaultWorldToMeters = VREd::DefaultWorldToMeters->GetFloat();
	GetWorld()->GetWorldSettings()->WorldToMeters = DefaultWorldToMeters != 0.0f ? DefaultWorldToMeters : SavedEditorState.WorldToMetersScale;
	ENGINE_API extern float GNewWorldToMetersScale;
	GNewWorldToMetersScale = 0.0f;
}

UStaticMeshComponent* UVREditorMode::CreateMotionControllerMesh( AActor* OwningActor, USceneComponent* AttachmentToComponent )
{
	UStaticMesh* ControllerMesh = nullptr;
	if(GetHMDDeviceType() == EHMDDeviceType::DT_SteamVR)
	{
		ControllerMesh = AssetContainer->VivePreControllerMesh;
	}
	else if(GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift)
	{
		ControllerMesh = AssetContainer->OculusControllerMesh;
	}
	else
	{
		ControllerMesh = AssetContainer->GenericControllerMesh;
	}

	return CreateMesh(OwningActor, ControllerMesh, AttachmentToComponent);
}

UStaticMeshComponent* UVREditorMode::CreateMesh( AActor* OwningActor, const FString& MeshName, USceneComponent* AttachmentToComponent /*= nullptr */ )
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshName);
	check(Mesh != nullptr);
	return CreateMesh(OwningActor, Mesh, AttachmentToComponent);
}

UStaticMeshComponent* UVREditorMode::CreateMesh(AActor* OwningActor, UStaticMesh* Mesh, USceneComponent* AttachmentToComponent /*= nullptr */)
{
	UStaticMeshComponent* CreatedMeshComponent = NewObject<UStaticMeshComponent>(OwningActor);
	OwningActor->AddOwnedComponent(CreatedMeshComponent);
	if (AttachmentToComponent != nullptr)
	{
		CreatedMeshComponent->SetupAttachment(AttachmentToComponent);
	}

	CreatedMeshComponent->RegisterComponent();

	CreatedMeshComponent->SetStaticMesh(Mesh);
	CreatedMeshComponent->SetMobility(EComponentMobility::Movable);
	CreatedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CreatedMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	return CreatedMeshComponent;
}

void UVREditorMode::SetActionsMenuGenerator(const FOnRadialMenuGenerated NewMenuGenerator, const FText NewLabel)
{
	GetUISystem().GetRadialMenuHandler()->SetActionsMenuGenerator(NewMenuGenerator, NewLabel);
}

void UVREditorMode::ResetActionsMenuGenerator()
{
	GetUISystem().GetRadialMenuHandler()->ResetActionsMenuGenerator();
}

void UVREditorMode::RefreshRadialMenuActionsSubmenu()
{
	GetUISystem().GetRadialMenuHandler()->RegisterMenuGenerator( GetUISystem().GetRadialMenuHandler()->GetActionsMenuGenerator() );
}

bool UVREditorMode::GetStartedPlayFromVREditor() const
{
	return bStartedPlayFromVREditor;
}

const UVREditorAssetContainer& UVREditorMode::GetAssetContainer() const
{
	return *AssetContainer;
}

void UVREditorMode::PlaySound(USoundBase* SoundBase, const FVector& InWorldLocation, const float InVolume /*= 1.0f*/)
{
	if (IsActive() && bIsFullyInitialized && GEditor != nullptr && GEditor->CanPlayEditorSound())
	{
		const float Volume = InVolume*VREd::SFXMultiplier->GetFloat();
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundBase, InWorldLocation, Volume);
	}
}

bool UVREditorMode::IsAimingTeleport() const
{
	return TeleportActor->IsAiming();
}

void UVREditorMode::PostPIEStarted( bool bIsSimulatingInEditor )
{
	if (!bIsSimulatingInEditor)
	{
		GEnableVREditorHacks = false;
	}
}


void UVREditorMode::PrePIEEnded( bool bWasSimulatingInEditor )
{
	if (!bWasSimulatingInEditor && !bStartedPlayFromVREditorSimulate)
	{
		GEnableVREditorHacks = true;
	}
	else if (bStartedPlayFromVREditorSimulate)
	{
		// Pre PIE to SIE. When exiting play with escape, the delegate toggle PIE and SIE won't be called. We know that we started PIE from simulate. However simulate will also be closed.
		GEnableVREditorHacks = true;
	}
}

void UVREditorMode::OnEndPIE(bool bWasSimulatingInEditor)
{
	if (!bWasSimulatingInEditor && !bStartedPlayFromVREditorSimulate)
	{
		RestoreFromPIE();
	}
	else if (bStartedPlayFromVREditorSimulate)
	{
		// Post PIE to SIE
		RestoreFromPIE();
		GetOwningCollection()->ShowAllActors(true);
	}
}

void UVREditorMode::OnPreSwitchPIEAndSIE(bool bIsSimulatingInEditor)
{
	if (bStartedPlayFromVREditorSimulate)
	{
		if (bIsSimulatingInEditor)
		{
			// Pre SIE to PIE
			GetOwningCollection()->ShowAllActors(false);
		}
		else
		{
			// Pre PIE to SIE
			GEnableVREditorHacks = true;
		}
	}
}

void UVREditorMode::OnSwitchPIEAndSIE(bool bIsSimulatingInEditor)
{
	if (bStartedPlayFromVREditorSimulate)
	{
		if (bIsSimulatingInEditor)
		{
			// Post PIE to SIE
			RestoreFromPIE();
			GetOwningCollection()->ShowAllActors(true);
		}
		else
		{
			// Post SIE to PIE
			GEnableVREditorHacks = false;
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
		}
	}
}

void UVREditorMode::ToggleDebugMode()
{
	UVREditorMode::bDebugModeEnabled = !UVREditorMode::bDebugModeEnabled;
	IVREditorModule& VREditorModule = IVREditorModule::Get();
	UVREditorMode* VRMode = VREditorModule.GetVRMode();
	if (VRMode != nullptr)
	{
		VRMode->OnToggleDebugMode().Broadcast(UVREditorMode::bDebugModeEnabled);
	}
}


bool UVREditorMode::IsDebugModeEnabled()
{
	return UVREditorMode::bDebugModeEnabled;
}

#undef LOCTEXT_NAMESPACE
