// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IMeshPaintMode.h"
#include "SceneView.h"
#include "EditorViewportClient.h"
#include "Modules/ModuleManager.h"
#include "EditorReimportHandler.h"
#include "EngineUtils.h"
#include "Utils.h"
#include "UnrealEdGlobals.h"
#include "Engine/Selection.h"
#include "EditorModeManager.h"
#include "ToolkitManager.h"

#include "VREditorMode.h"
#include "IVREditorModule.h"

#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "EditorSupportDelegates.h"

#include "MeshPaintHelpers.h"

//Slate dependencies
#include "LevelEditor.h"
#include "ILevelViewport.h"

#include "MeshPaintAdapterFactory.h"

#include "ViewportWorldInteraction.h"
#include "ViewportInteractableInterface.h"
#include "VREditorInteractor.h"
#include "EditorWorldExtension.h"

#include "Components/PrimitiveComponent.h"

#include "IMeshPainter.h"
#include "MeshPaintSettings.h"

#define LOCTEXT_NAMESPACE "IMeshPaint_Mode"

DEFINE_LOG_CATEGORY_STATIC(LogMeshPaintEdMode, Log, All);

/** Constructor */
IMeshPaintEdMode::IMeshPaintEdMode() 
	: FEdMode(),
	  PaintingWithInteractorInVR( nullptr )
{
	GEditor->OnEditorClose().AddRaw(this, &IMeshPaintEdMode::OnResetViewMode);
}

IMeshPaintEdMode::~IMeshPaintEdMode()
{
	GEditor->OnEditorClose().RemoveAll(this);
}

/** FGCObject interface */
void IMeshPaintEdMode::AddReferencedObjects( FReferenceCollector& Collector )
{
	// Call parent implementation
	FEdMode::AddReferencedObjects(Collector);
	MeshPainter->AddReferencedObjects(Collector);
}

void IMeshPaintEdMode::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	checkf(MeshPainter != nullptr, TEXT("Mesh Paint was not initialized"));

	// The user can manipulate the editor selection lock flag in paint mode so we save off the value here so it can be restored later
	bWasSelectionLockedOnStart = GEdSelectionLock;	

	// Catch when objects are replaced when a construction script is rerun
	GEditor->OnObjectsReplaced().AddSP(this, &IMeshPaintEdMode::OnObjectsReplaced);

	// Hook into pre/post world save, so that the original collision volumes can be temporarily reinstated
	FEditorDelegates::PreSaveWorld.AddSP(this, &IMeshPaintEdMode::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.AddSP(this, &IMeshPaintEdMode::OnPostSaveWorld);

	// Catch assets if they are about to be (re)imported
	FEditorDelegates::OnAssetPostImport.AddSP(this, &IMeshPaintEdMode::OnPostImportAsset);
	FReimportManager::Instance()->OnPostReimport().AddSP(this, &IMeshPaintEdMode::OnPostReimportAsset);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddSP(this, &IMeshPaintEdMode::OnAssetRemoved);

	// Initialize adapter globals
	FMeshPaintAdapterFactory::InitializeAdapterGlobals();

	SelectionChangedHandle = USelection::SelectionChangedEvent.AddLambda([=](UObject* Object) { MeshPainter->Refresh();  });

	if (UsesToolkits() && !Toolkit.IsValid())
	{
		Toolkit = GetToolkit();
		Toolkit->Init(Owner->GetToolkitHost());
	}
	
	// Change the engine to draw selected objects without a color boost, but unselected objects will
	// be darkened slightly.  This just makes it easier to paint on selected objects without the
	// highlight effect distorting the appearance.
	GEngine->OverrideSelectedMaterialColor( FLinearColor::Black );

	UEditorWorldExtensionCollection* ExtensionCollection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld());
	if (ExtensionCollection != nullptr)
	{
		// Register to find out about VR input events
		UViewportWorldInteraction* ViewportWorldInteraction = Cast<UViewportWorldInteraction>(ExtensionCollection->FindExtension(UViewportWorldInteraction::StaticClass()));
		if (ViewportWorldInteraction != nullptr)
		{
			ViewportWorldInteraction->OnViewportInteractionInputAction().RemoveAll(this);
			ViewportWorldInteraction->OnViewportInteractionInputAction().AddRaw(this, &IMeshPaintEdMode::OnVRAction);

			// Hide the VR transform gizmo while we're in mesh paint mode.  It sort of gets in the way of painting.
			ViewportWorldInteraction->SetTransformGizmoVisible(false);
		}
	}

	if (UsesToolkits())
	{
		MeshPainter->RegisterCommands(Toolkit->GetToolkitCommands());
	}
		
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::Exit()
{
	/** Finish up painting if we still are */
	if (MeshPainter->IsPainting())
	{
		MeshPainter->FinishPainting();
	}

	/** Reset paint state and unregister commands */
	MeshPainter->Reset();	
	if (UsesToolkits())
	{
		MeshPainter->UnregisterCommands(Toolkit->GetToolkitCommands());
	}

	UEditorWorldExtensionCollection* ExtensionCollection = GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld());
	if (ExtensionCollection != nullptr)
	{
		UViewportWorldInteraction* ViewportWorldInteraction = Cast<UViewportWorldInteraction>(ExtensionCollection->FindExtension(UViewportWorldInteraction::StaticClass()));
		if (ViewportWorldInteraction != nullptr)
		{
			// Restore the transform gizmo visibility
			ViewportWorldInteraction->SetTransformGizmoVisible(true);

			// Unregister from event handlers
			ViewportWorldInteraction->OnViewportInteractionInputAction().RemoveAll(this);
		}
	}

	// The user can manipulate the editor selection lock flag in paint mode so we make sure to restore it here
	GEdSelectionLock = bWasSelectionLockedOnStart;
	
	OnResetViewMode();

	// Restore selection color
	GEngine->RestoreSelectedMaterialColor();

	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	// Unbind delegates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
	FEditorDelegates::PreSaveWorld.RemoveAll(this);
	FEditorDelegates::PostSaveWorld.RemoveAll(this);
	GEditor->OnObjectsReplaced().RemoveAll(this);
	USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);

	// Call parent implementation
	FEdMode::Exit();
}

bool IMeshPaintEdMode::CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	// We only care about perspective viewpo1rts
	bool bPaintApplied = false;
	if( InViewportClient->IsPerspective() )
	{
		if( MeshPainter->IsPainting() )
		{
			// Compute a world space ray from the screen space mouse coordinates
			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( 
				InViewportClient->Viewport, 
				InViewportClient->GetScene(),
				InViewportClient->EngineShowFlags)
				.SetRealtimeUpdate( InViewportClient->IsRealtime() ));
			FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
			FViewportCursorLocation MouseViewportRay( View, (FEditorViewportClient*)InViewport->GetClient(), InMouseX, InMouseY );
			 					
			bPaintApplied = MeshPainter->Paint(InViewport, View->ViewMatrices.GetViewOrigin(), MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection());
		}
	}

	return bPaintApplied;
}

/** FEdMode: Called when the a mouse button is released */
bool IMeshPaintEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	InViewportClient->bLockFlightCamera = false;
	MeshPainter->FinishPainting();
	PaintingWithInteractorInVR = nullptr;
	return true;
}

/** FEdMode: Called when a key is pressed */
bool IMeshPaintEdMode::InputKey( FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent )
{
	bool bHandled = MeshPainter->InputKey(InViewportClient, InViewport, InKey, InEvent);

	if (bHandled)
	{
		return bHandled;
	}
	const bool bIsLeftButtonDown = ( InKey == EKeys::LeftMouseButton && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftMouseButton );
	const bool bIsRightButtonDown = (InKey == EKeys::RightMouseButton && InEvent != IE_Released) || InViewport->KeyState(EKeys::RightMouseButton);
	const bool bIsCtrlDown = ((InKey == EKeys::LeftControl || InKey == EKeys::RightControl) && InEvent != IE_Released) || InViewport->KeyState(EKeys::LeftControl) || InViewport->KeyState(EKeys::RightControl);
	const bool bIsShiftDown = ( ( InKey == EKeys::LeftShift || InKey == EKeys::RightShift ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftShift ) || InViewport->KeyState( EKeys::RightShift );
	const bool bIsAltDown = ( ( InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt ) && InEvent != IE_Released ) || InViewport->KeyState( EKeys::LeftAlt ) || InViewport->KeyState( EKeys::RightAlt );

	// When painting we only care about perspective viewports
	if( !bIsAltDown && InViewportClient->IsPerspective() )
	{
		// Does the user want to paint right now?
		const bool bUserWantsPaint = bIsLeftButtonDown && !bIsRightButtonDown && !bIsAltDown;
		bool bPaintApplied = false;

		// Stop current tracking if the user is no longer painting
		if( MeshPainter->IsPainting() && !bUserWantsPaint &&
			( InKey == EKeys::LeftMouseButton || InKey == EKeys::RightMouseButton || InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt ) )
		{
			bHandled = true;
			MeshPainter->FinishPainting();
			PaintingWithInteractorInVR = nullptr;
		}
		else if( !MeshPainter->IsPainting() && bUserWantsPaint && !InViewportClient->IsMovingCamera())
		{	
			bHandled = true;

			// Compute a world space ray from the screen space mouse coordinates
			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( 
				InViewportClient->Viewport, 
				InViewportClient->GetScene(),
				InViewportClient->EngineShowFlags )
				.SetRealtimeUpdate( InViewportClient->IsRealtime() ));

			FSceneView* View = InViewportClient->CalcSceneView( &ViewFamily );
			const FViewportCursorLocation MouseViewportRay( View, (FEditorViewportClient*)InViewport->GetClient(), InViewport->GetMouseX(), InViewport->GetMouseY() );

			// Paint!
			bPaintApplied = MeshPainter->Paint(InViewport, View->ViewMatrices.GetViewOrigin(), MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection());
			
		}
		else if (MeshPainter->IsPainting() && bUserWantsPaint)
		{
			bHandled = true;
		}

		if( !bPaintApplied && !MeshPainter->IsPainting())
		{
			bHandled = false;
		}
		else
		{
			InViewportClient->bLockFlightCamera = true;
		}

		// Also absorb other mouse buttons, and Ctrl/Alt/Shift events that occur while we're painting as these would cause
		// the editor viewport to start panning/dollying the camera
		{
			const bool bIsOtherMouseButtonEvent = ( InKey == EKeys::MiddleMouseButton || InKey == EKeys::RightMouseButton );
			const bool bCtrlButtonEvent = (InKey == EKeys::LeftControl || InKey == EKeys::RightControl);
			const bool bShiftButtonEvent = (InKey == EKeys::LeftShift || InKey == EKeys::RightShift);
			const bool bAltButtonEvent = (InKey == EKeys::LeftAlt || InKey == EKeys::RightAlt);
			if( MeshPainter->IsPainting() && ( bIsOtherMouseButtonEvent || bShiftButtonEvent || bAltButtonEvent ) )
			{
				bHandled = true;
			}

			if( bCtrlButtonEvent && !MeshPainter->IsPainting())
			{
				bHandled = false;
			}
			else if( bIsCtrlDown)
			{
				//default to assuming this is a paint command
				bHandled = true;

				// Allow Ctrl+B to pass through so we can support the finding of a selected static mesh in the content browser.
				if ( !(bShiftButtonEvent || bAltButtonEvent || bIsOtherMouseButtonEvent) && ( (InKey == EKeys::B) && (InEvent == IE_Pressed) ) )
				{
					bHandled = false;
				}

				// If we are not painting, we will let the CTRL-Z and CTRL-Y key presses through to support undo/redo.
				if ( !MeshPainter->IsPainting() && ( InKey == EKeys::Z || InKey == EKeys::Y ) )
				{
					bHandled = false;
				}
			}
		}
	}

	return bHandled;
}

void IMeshPaintEdMode::OnPreSaveWorld(uint32 SaveFlags, UWorld* World)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnPostImportAsset(UFactory* Factory, UObject* Object)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnPostReimportAsset(UObject* Object, bool bSuccess)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnAssetRemoved(const FAssetData& AssetData)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	MeshPainter->Refresh();
}

void IMeshPaintEdMode::OnResetViewMode()
{
	// Reset viewport color mode for all active viewports
	for (int32 ViewIndex = 0; ViewIndex < GEditor->AllViewportClients.Num(); ++ViewIndex)
	{
		FEditorViewportClient* ViewportClient = GEditor->AllViewportClients[ViewIndex];
		if (!ViewportClient || ViewportClient->GetModeTools() != GetModeManager())
		{
			continue;
		}

		MeshPaintHelpers::SetViewportColorMode(EMeshPaintColorViewMode::Normal, ViewportClient);
	}
}

/** FEdMode: Called after an Undo operation */
void IMeshPaintEdMode::PostUndo()
{
	FEdMode::PostUndo();
	MeshPainter->Refresh();
}

/** FEdMode: Render the mesh paint tool */
void IMeshPaintEdMode::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	/** Call parent implementation */
	FEdMode::Render( View, Viewport, PDI );
	MeshPainter->Render(View, Viewport, PDI);

	// Flow painting
	if (MeshPainter->IsPainting())
	{
		/** If we are currently painting with a VR interactor, apply paint for the current vr interactor state/position */
		if (PaintingWithInteractorInVR != nullptr)
		{
			UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>(PaintingWithInteractorInVR);
			FVector LaserPointerStart, LaserPointerEnd;
			if (VRInteractor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd))
			{
				const FVector LaserPointerDirection = (LaserPointerEnd - LaserPointerStart).GetSafeNormal();
				UVREditorMode* VREditorMode = Cast<UVREditorMode>(GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld())->FindExtension(UVREditorMode::StaticClass()));
				MeshPainter->PaintVR(Viewport, VREditorMode->GetHeadTransform().GetLocation(), LaserPointerStart, LaserPointerDirection, VRInteractor);
			}
		}
		else if (MeshPainter->GetBrushSettings()->bEnableFlow)
		{
			// Make sure the cursor is visible OR we're flood filling.  No point drawing a paint cue when there's no cursor.
			if (Viewport->IsCursorVisible())
			{				
				// Grab the mouse cursor position
				FIntPoint MousePosition;
				Viewport->GetMousePos(MousePosition);

				// Is the mouse currently over the viewport? or flood filling
				if ((MousePosition.X >= 0 && MousePosition.Y >= 0 && MousePosition.X < (int32)Viewport->GetSizeXY().X && MousePosition.Y < (int32)Viewport->GetSizeXY().Y))
				{
					// Compute a world space ray from the screen space mouse coordinates
					FViewportCursorLocation MouseViewportRay(View, static_cast<FEditorViewportClient*>(Viewport->GetClient()), MousePosition.X, MousePosition.Y);
					MeshPainter->Paint(Viewport, View->ViewMatrices.GetViewOrigin(), MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection());
				}
			}
		}
	}

}

/** FEdMode: Handling SelectActor */
bool IMeshPaintEdMode::Select( AActor* InActor, bool bInSelected )
{
	if (bInSelected)
	{
		MeshPainter->ActorSelected(InActor);
	}
	else
	{
		MeshPainter->ActorDeselected(InActor);
	}
	
	return false;
}

/** FEdMode: Called when the currently selected actor has changed */
void IMeshPaintEdMode::ActorSelectionChangeNotify()
{
	MeshPainter->Refresh();
}

/** IMeshPaintEdMode: Called once per frame */
void IMeshPaintEdMode::Tick(FEditorViewportClient* ViewportClient,float DeltaTime)
{
	FEdMode::Tick(ViewportClient,DeltaTime);
	MeshPainter->Tick(ViewportClient, DeltaTime);
}

IMeshPainter* IMeshPaintEdMode::GetMeshPainter()
{
	checkf(MeshPainter != nullptr, TEXT("Invalid Mesh painter ptr"));
	return MeshPainter;
}

bool IMeshPaintEdMode::ProcessEditDelete()
{
	MeshPainter->Refresh();
	return false;
}

void IMeshPaintEdMode::OnVRAction( class FEditorViewportClient& ViewportClient, UViewportInteractor* Interactor, 
	const FViewportActionKeyInput& Action, bool& bOutIsInputCaptured, bool& bWasHandled )
{
	UVREditorMode* VREditorMode = Cast<UVREditorMode>(GEditor->GetEditorWorldExtensionsManager()->GetEditorWorldExtensions(GetWorld())->FindExtension(UVREditorMode::StaticClass()));
	UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>(Interactor);
	if (VREditorMode != nullptr && VREditorMode->IsActive() && VRInteractor != nullptr && VRInteractor->GetDraggingMode() == EViewportInteractionDraggingMode::Nothing)
	{
		if (Action.ActionType == ViewportWorldActionTypes::SelectAndMove)
		{
			if (!MeshPainter->IsPainting() && Action.Event == IE_Pressed && !VRInteractor->IsHoveringOverPriorityType())
			{
				// Check to see that we're clicking on a selected object.  You can only paint on selected things.  Otherwise,
				// we'll fall through to the normal interaction code which might cause the object to become selected.
				bool bIsClickingOnSelectedObject = false;
				{
					FHitResult HitResult = VRInteractor->GetHitResultFromLaserPointer();
					if( HitResult.Actor.IsValid() )
					{
						UViewportWorldInteraction& WorldInteraction = VREditorMode->GetWorldInteraction();
						
						if( WorldInteraction.IsInteractableComponent( HitResult.GetComponent() ) )
						{
							AActor* Actor = HitResult.Actor.Get();

							// Make sure we're not hovering over some other viewport interactable, such as a dockable window selection bar or close button
							IViewportInteractableInterface* ActorInteractable = Cast<IViewportInteractableInterface>( Actor );
							if( ActorInteractable == nullptr )
							{
								if( Actor != WorldInteraction.GetTransformGizmoActor() )  // Don't change any actor selection state if the user clicked on a gizmo
								{
									if( Actor->IsSelected() )
									{
										bIsClickingOnSelectedObject = true;
									}
								}
							}
						}
					}
				}

				if( bIsClickingOnSelectedObject )
				{
					bWasHandled = true;
					bOutIsInputCaptured = true;

					// Go ahead and paint immediately
					FVector LaserPointerStart, LaserPointerEnd;
					if( Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd ) )
					{
						const FVector LaserPointerDirection = ( LaserPointerEnd - LaserPointerStart ).GetSafeNormal();

						/** Apply painting using the current state/position of the VR interactor */
						const bool bAnyPaintableActorsUnderCursor = MeshPainter->PaintVR( ViewportClient.Viewport, VREditorMode->GetHeadTransform().GetLocation(), LaserPointerStart, LaserPointerDirection, VRInteractor);
						if (bAnyPaintableActorsUnderCursor)
						{
							PaintingWithInteractorInVR = VRInteractor;
							ViewportClient.bLockFlightCamera = true;
						}
					}
				}
			}
			// Stop current tracking if the user is no longer painting
			else if( MeshPainter->IsPainting() && Action.Event == IE_Released && PaintingWithInteractorInVR && PaintingWithInteractorInVR == Interactor )
			{
				MeshPainter->FinishPainting();
				ViewportClient.bLockFlightCamera = false;
				PaintingWithInteractorInVR = nullptr;

				bWasHandled = true;
				bOutIsInputCaptured = false;
			}

			else if (MeshPainter->IsPainting())
			{
				// A different hand might be painting, so absorb the input
				bOutIsInputCaptured = bWasHandled = (Action.Event == IE_Released ? false : true);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
