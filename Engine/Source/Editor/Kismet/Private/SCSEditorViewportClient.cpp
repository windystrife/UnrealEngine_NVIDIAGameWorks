// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCSEditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "CanvasItem.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "EngineUtils.h"
#include "UnrealEdGlobals.h"
#include "SEditorViewport.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "SSCSEditor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SKismetInspector.h"
#include "ScopedTransaction.h"
#include "ISCSEditorCustomization.h"
#include "CanvasTypes.h"
#include "Engine/TextureCube.h"
#include "SSCSEditorViewport.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSCSEditorViewport, Log, All);

namespace
{
	/** Automatic translation applied to the camera in the default editor viewport logic when orbit mode is enabled. */
	const float AutoViewportOrbitCameraTranslate = 256.0f;

	void DrawAngles(FCanvas* Canvas, int32 XPos, int32 YPos, EAxisList::Type ManipAxis, FWidget::EWidgetMode MoveMode, const FRotator& Rotation, const FVector& Translation)
	{
		FString OutputString(TEXT(""));
		if(MoveMode == FWidget::WM_Rotate && Rotation.IsZero() == false)
		{
			//Only one value moves at a time
			const FVector EulerAngles = Rotation.Euler();
			if(ManipAxis == EAxisList::X)
			{
				OutputString += FString::Printf(TEXT("Roll: %0.2f"), EulerAngles.X);
			}
			else if(ManipAxis == EAxisList::Y)
			{
				OutputString += FString::Printf(TEXT("Pitch: %0.2f"), EulerAngles.Y);
			}
			else if(ManipAxis == EAxisList::Z)
			{
				OutputString += FString::Printf(TEXT("Yaw: %0.2f"), EulerAngles.Z);
			}
		}
		else if(MoveMode == FWidget::WM_Translate && Translation.IsZero() == false)
		{
			//Only one value moves at a time
			if(ManipAxis == EAxisList::X)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.X);
			}
			else if(ManipAxis == EAxisList::Y)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Y);
			}
			else if(ManipAxis == EAxisList::Z)
			{
				OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Z);
			}
		}

		if(OutputString.Len() > 0)
		{
			FCanvasTextItem TextItem( FVector2D(XPos, YPos), FText::FromString( OutputString ), GEngine->GetSmallFont(), FLinearColor::White );
			Canvas->DrawItem( TextItem );
		} 
	}

	// Determine whether or not the given node has a parent node that is not the root node, is movable and is selected
	bool IsMovableParentNodeSelected(const FSCSEditorTreeNodePtrType& NodePtr, const TArray<FSCSEditorTreeNodePtrType>& SelectedNodes)
	{
		if(NodePtr.IsValid())
		{
			// Check for a valid parent node
			FSCSEditorTreeNodePtrType ParentNodePtr = NodePtr->GetParent();
			if(ParentNodePtr.IsValid() && !ParentNodePtr->IsRootComponent())
			{
				if(SelectedNodes.Contains(ParentNodePtr))
				{
					// The parent node is not the root node and is also selected; success
					return true;
				}
				else
				{
					// Recursively search for any other parent nodes farther up the tree that might be selected
					return IsMovableParentNodeSelected(ParentNodePtr, SelectedNodes);
				}
			}
		}

		return false;
	}
}

/////////////////////////////////////////////////////////////////////////
// FSCSEditorViewportClient

FSCSEditorViewportClient::FSCSEditorViewportClient(TWeakPtr<FBlueprintEditor>& InBlueprintEditorPtr, FPreviewScene* InPreviewScene, const TSharedRef<SSCSEditorViewport>& InSCSEditorViewport)
	: FEditorViewportClient(nullptr, InPreviewScene, StaticCastSharedRef<SEditorViewport>(InSCSEditorViewport))
	, BlueprintEditorPtr(InBlueprintEditorPtr)
	, PreviewActorBounds(ForceInitToZero)
	, bIsManipulating(false)
	, ScopedTransaction(NULL)
	, bIsSimulateEnabled(false)
{
	WidgetMode = FWidget::WM_Translate;
	WidgetCoordSystem = COORD_Local;
	EngineShowFlags.DisableAdvancedFeatures();

	check(Widget);
	Widget->SetSnapEnabled(true);

	// Selectively set particular show flags that we need
	EngineShowFlags.SetSelectionOutline(GetDefault<ULevelEditorViewportSettings>()->bUseSelectionOutline);

	// Set if the grid will be drawn
	DrawHelper.bDrawGrid = GetDefault<UEditorPerProjectUserSettings>()->bSCSEditorShowGrid;

	// now add floor
	EditorFloorComp = NewObject<UStaticMeshComponent>(GetTransientPackage(), TEXT("EditorFloorComp"));

	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("/Engine/EditorMeshes/PhAT_FloorBox.PhAT_FloorBox"), NULL, LOAD_None, NULL);
	if (ensure(FloorMesh))
	{
		EditorFloorComp->SetStaticMesh(FloorMesh);
	}

	UMaterial* Material = LoadObject<UMaterial>(NULL, TEXT("/Engine/EditorMaterials/PersonaFloorMat.PersonaFloorMat"), NULL, LOAD_None, NULL);
	if (ensure(Material))
	{
		EditorFloorComp->SetMaterial(0, Material);
	}

	EditorFloorComp->SetRelativeScale3D(FVector(3.f, 3.f, 1.f));
	EditorFloorComp->SetVisibility(GetDefault<UEditorPerProjectUserSettings>()->bSCSEditorShowFloor);
	EditorFloorComp->SetCollisionEnabled(GetDefault<UEditorPerProjectUserSettings>()->bSCSEditorShowFloor? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	PreviewScene->AddComponent(EditorFloorComp, FTransform::Identity);

	// Turn off so that actors added to the world do not have a lifespan (so they will not auto-destroy themselves).
	PreviewScene->GetWorld()->bBegunPlay = false;

	PreviewScene->SetSkyCubemap(GUnrealEd->GetThumbnailManager()->AmbientCubemap);
}

FSCSEditorViewportClient::~FSCSEditorViewportClient()
{
	// Ensure that an in-progress transaction is ended
	EndTransaction();
}

void FSCSEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Register the selection override delegate for the preview actor's components
	TSharedPtr<SSCSEditor> SCSEditor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	AActor* PreviewActor = GetPreviewActor();
	if (PreviewActor != nullptr)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		PreviewActor->GetComponents(PrimitiveComponents, true);

		for (UPrimitiveComponent* PrimComponent : PrimitiveComponents)
		{
			if (!PrimComponent->SelectionOverrideDelegate.IsBound())
			{
				SCSEditor->SetSelectionOverride(PrimComponent);
			}
		}
	}
	else
	{
		InvalidatePreview(false);
	}

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		// Ensure that the preview actor instance is up-to-date for component editing (e.g. after compiling the Blueprint, the actor may be reinstanced outside of this class)
		if(PreviewActor != BlueprintEditorPtr.Pin()->GetBlueprintObj()->SimpleConstructionScript->GetComponentEditorActorInstance())
		{
			BlueprintEditorPtr.Pin()->GetBlueprintObj()->SimpleConstructionScript->SetComponentEditorActorInstance(PreviewActor);
		}

		// Allow full tick only if preview simulation is enabled and we're not currently in an active SIE or PIE session
		if(bIsSimulateEnabled && GEditor->PlayWorld == NULL && !GEditor->bIsSimulatingInEditor)
		{
			PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_All : LEVELTICK_TimeOnly, DeltaSeconds);
		}
		else
		{
			PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_ViewportsOnly : LEVELTICK_TimeOnly, DeltaSeconds);
		}
	}
}

void FSCSEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	bool bHitTesting = PDI->IsHitTesting();
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		if(GUnrealEd != NULL)
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			for (int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); ++SelectionIndex)
			{
				FSCSEditorTreeNodePtrType SelectedNode = SelectedNodes[SelectionIndex];

				UActorComponent* Comp = SelectedNode->FindComponentInstanceInActor(PreviewActor);
				if(Comp != NULL && Comp->IsRegistered())
				{
					// Try and find a visualizer
					TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Comp->GetClass());
					if (Visualizer.IsValid())
					{
						Visualizer->DrawVisualization(Comp, View, PDI);
					}
				}
			}
		}
	}
}

void FSCSEditorViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		if (GUnrealEd != NULL)
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
			for (int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); ++SelectionIndex)
			{
				FSCSEditorTreeNodePtrType SelectedNode = SelectedNodes[SelectionIndex];

				UActorComponent* Comp = Cast<USceneComponent>(SelectedNode->FindComponentInstanceInActor(PreviewActor));
				if (Comp != NULL && Comp->IsRegistered())
				{
					// Try and find a visualizer
					TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Comp->GetClass());
					if (Visualizer.IsValid())
					{
						Visualizer->DrawVisualizationHUD(Comp, &InViewport, &View, &Canvas);
					}
				}
			}
		}

		TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

		const int32 HalfX = 0.5f * Viewport->GetSizeXY().X;
		const int32 HalfY = 0.5f * Viewport->GetSizeXY().Y;

		auto SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(bIsManipulating && SelectedNodes.Num() > 0)
		{
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes[0]->FindComponentInstanceInActor(PreviewActor));
			if(SceneComp)
			{
				const FVector WidgetLocation = GetWidgetLocation();
				const FPlane Proj = View.Project(WidgetLocation);
				if(Proj.W > 0.0f)
				{
					const int32 XPos = HalfX + (HalfX * Proj.X);
					const int32 YPos = HalfY + (HalfY * (Proj.Y * -1));
					DrawAngles(&Canvas, XPos, YPos, GetCurrentWidgetAxis(), GetWidgetMode(), GetWidgetCoordSystem().Rotator(), WidgetLocation);
				}
			}
		}
	}
}

bool FSCSEditorViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	bool bHandled = GUnrealEd->ComponentVisManager.HandleInputKey(this, InViewport, Key, Event);;

	if( !bHandled )
	{
		bHandled = FEditorViewportClient::InputKey(InViewport, ControllerId, Key, Event, AmountDepressed, bGamepad);
	}

	return bHandled;
}

void FSCSEditorViewportClient::ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	const FViewportClick Click(&View, this, Key, Event, HitX, HitY);

	if (HitProxy)
	{
		if (HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			HInstancedStaticMeshInstance* InstancedStaticMeshInstanceProxy = ((HInstancedStaticMeshInstance*)HitProxy);

			TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(InstancedStaticMeshInstanceProxy->Component);
			if (Customization.IsValid() && Customization->HandleViewportClick(AsShared(), View, HitProxy, Key, Event, HitX, HitY))
			{
				Invalidate();
			}

			return;
		}
		else if (HitProxy->IsA(HWidgetAxis::StaticGetType()))
		{
			const bool bOldModeWidgets1 = EngineShowFlags.ModeWidgets;
			const bool bOldModeWidgets2 = View.Family->EngineShowFlags.ModeWidgets;

			EngineShowFlags.SetModeWidgets(false);
			FSceneViewFamily* SceneViewFamily = const_cast<FSceneViewFamily*>(View.Family);
			SceneViewFamily->EngineShowFlags.SetModeWidgets(false);
			bool bWasWidgetDragging = Widget->IsDragging();
			Widget->SetDragging(false);

			// Invalidate the hit proxy map so it will be rendered out again when GetHitProxy
			// is called
			Viewport->InvalidateHitProxy();

			// This will actually re-render the viewport's hit proxies!
			HHitProxy* HitProxyWithoutAxisWidgets = Viewport->GetHitProxy(HitX, HitY);
			if (HitProxyWithoutAxisWidgets != NULL && !HitProxyWithoutAxisWidgets->IsA(HWidgetAxis::StaticGetType()))
			{
				// Try this again, but without the widget this time!
				ProcessClick(View, HitProxyWithoutAxisWidgets, Key, Event, HitX, HitY);
			}

			// Undo the evil
			EngineShowFlags.SetModeWidgets(bOldModeWidgets1);
			SceneViewFamily->EngineShowFlags.SetModeWidgets(bOldModeWidgets2);

			Widget->SetDragging(bWasWidgetDragging);

			// Invalidate the hit proxy map again so that it'll be refreshed with the original
			// scene contents if we need it again later.
			Viewport->InvalidateHitProxy();
			return;
		}
		else if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorProxy = (HActor*)HitProxy;
			AActor* PreviewActor = GetPreviewActor();
			if (ActorProxy && ActorProxy->Actor && ActorProxy->PrimComponent)
			{
				USceneComponent* SelectedCompInstance = nullptr;

				if (ActorProxy->Actor == PreviewActor)
				{
					UPrimitiveComponent* TestComponent = const_cast<UPrimitiveComponent*>(ActorProxy->PrimComponent);
					if (ActorProxy->Actor->GetComponents().Contains(TestComponent))
					{
						SelectedCompInstance = TestComponent;
					}
				}
				else if (ActorProxy->Actor->IsChildActor())
				{
					AActor* TestActor = ActorProxy->Actor;
					while (TestActor->GetParentActor()->IsChildActor())
					{
						TestActor = TestActor->GetParentActor();
					}

					if (TestActor->GetParentActor() == PreviewActor)
					{
						SelectedCompInstance = TestActor->GetParentComponent();
					}
				}

				if (SelectedCompInstance)
				{
					TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(SelectedCompInstance);
					if (!(Customization.IsValid() && Customization->HandleViewportClick(AsShared(), View, HitProxy, Key, Event, HitX, HitY)))
					{
						const bool bIsCtrlKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
						if (BlueprintEditorPtr.IsValid())
						{
							// Note: This will find and select any node associated with the component instance that's attached to the proxy (including visualizers)
							BlueprintEditorPtr.Pin()->FindAndSelectSCSEditorTreeNode(SelectedCompInstance, bIsCtrlKeyDown);
						}
					}
				}
			}

			Invalidate();
			return;
		}
	}
	
	GUnrealEd->ComponentVisManager.HandleClick(this, HitProxy, Click);
}

bool FSCSEditorViewportClient::InputWidgetDelta( FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale )
{
	bool bHandled = false;
	if(bIsManipulating && CurrentAxis != EAxisList::None)
	{
		bHandled = true;
		AActor* PreviewActor = GetPreviewActor();
		auto BlueprintEditor = BlueprintEditorPtr.Pin();
		if (PreviewActor && BlueprintEditor.IsValid())
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditor->GetSelectedSCSEditorTreeNodes();
			if(SelectedNodes.Num() > 0)
			{
				FVector ModifiedScale = Scale;

				// (mirrored from Level Editor VPC) - we don't scale components when we only have a very small scale change
				if (!Scale.IsNearlyZero())
				{
					if (GEditor->UsePercentageBasedScaling())
					{
						ModifiedScale = Scale * ((GEditor->GetScaleGridSize() / 100.0f) / GEditor->GetGridSize());
					}
				}
				else
				{
					ModifiedScale = FVector::ZeroVector;
				}

				for(auto It(SelectedNodes.CreateIterator());It;++It)
				{
					FSCSEditorTreeNodePtrType SelectedNodePtr = *It;
					// Don't allow editing of a root node, inherited SCS node or child node that also has a movable (non-root) parent node selected
					const bool bCanEdit = GUnrealEd->ComponentVisManager.IsActive() ||
						(!SelectedNodePtr->IsRootComponent() && !IsMovableParentNodeSelected(SelectedNodePtr, SelectedNodes));

					if(bCanEdit)
					{
						USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodePtr->FindComponentInstanceInActor(PreviewActor));
						USceneComponent* SelectedTemplate = Cast<USceneComponent>(SelectedNodePtr->GetEditableComponentTemplate(BlueprintEditor->GetBlueprintObj()));
						if(SceneComp != NULL && SelectedTemplate != NULL)
						{
							if (GUnrealEd->ComponentVisManager.HandleInputDelta(this, InViewport, Drag, Rot, Scale))
							{
								GUnrealEd->RedrawLevelEditingViewports();
								Invalidate();
								return true;
							}

							// Cache the current default values for propagation
							FVector OldRelativeLocation = SelectedTemplate->RelativeLocation;
							FRotator OldRelativeRotation = SelectedTemplate->RelativeRotation;
							FVector OldRelativeScale3D = SelectedTemplate->RelativeScale3D;

							// Adjust the deltas as necessary
							FComponentEditorUtils::AdjustComponentDelta(SceneComp, Drag, Rot);

							TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditor->CustomizeSCSEditor(SceneComp);
							if(Customization.IsValid() && Customization->HandleViewportDrag(SceneComp, SelectedTemplate, Drag, Rot, ModifiedScale, GetWidgetLocation()))
							{
								// Handled by SCS Editor customization
							}
							else
							{
								// Apply delta to the template component object 
								// (the preview scene component will be set in one of the ArchetypeInstances loops below... to keep the two in sync) 
								GEditor->ApplyDeltaToComponent(
										SelectedTemplate,
										true,
										&Drag,
										&Rot,
										&ModifiedScale,
										SelectedTemplate->RelativeLocation );
							}

							UBlueprint* PreviewBlueprint = UBlueprint::GetBlueprintFromClass(PreviewActor->GetClass());
							if(PreviewBlueprint != NULL)
							{
								// Like PostEditMove(), but we only need to re-run construction scripts
								if(PreviewBlueprint && PreviewBlueprint->bRunConstructionScriptOnDrag)
								{
									PreviewActor->RerunConstructionScripts();
								}

								SceneComp->PostEditComponentMove(true); // @TODO HACK passing 'finished' every frame...

								// If a constraint, copy back updated constraint frames to template
								UPhysicsConstraintComponent* ConstraintComp = Cast<UPhysicsConstraintComponent>(SceneComp);
								UPhysicsConstraintComponent* TemplateComp = Cast<UPhysicsConstraintComponent>(SelectedTemplate);
								if(ConstraintComp && TemplateComp)
								{
									TemplateComp->ConstraintInstance.CopyConstraintGeometryFrom(&ConstraintComp->ConstraintInstance);
								}

								// Iterate over all the active archetype instances and propagate the change(s) to the matching component instance
								TArray<UObject*> ArchetypeInstances;
								if(SelectedTemplate->HasAnyFlags(RF_ArchetypeObject))
								{
									SelectedTemplate->GetArchetypeInstances(ArchetypeInstances);
									for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
									{
										SceneComp = Cast<USceneComponent>(ArchetypeInstances[InstanceIndex]);
										if(SceneComp != nullptr)
										{
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeLocation, OldRelativeLocation, SelectedTemplate->RelativeLocation);
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeRotation, OldRelativeRotation, SelectedTemplate->RelativeRotation);
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeScale3D,  OldRelativeScale3D,  SelectedTemplate->RelativeScale3D);
										}
									}
								}
								else if(UObject* Outer = SelectedTemplate->GetOuter())
								{
									Outer->GetArchetypeInstances(ArchetypeInstances);
									for(int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstances.Num(); ++InstanceIndex)
									{
										SceneComp = static_cast<USceneComponent*>(FindObjectWithOuter(ArchetypeInstances[InstanceIndex], SelectedTemplate->GetClass(), SelectedTemplate->GetFName()));
										if(SceneComp)
										{
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeLocation, OldRelativeLocation, SelectedTemplate->RelativeLocation);
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeRotation, OldRelativeRotation, SelectedTemplate->RelativeRotation);
											FComponentEditorUtils::ApplyDefaultValueChange(SceneComp, SceneComp->RelativeScale3D, OldRelativeScale3D, SelectedTemplate->RelativeScale3D);
										}
									}
								}
							}
						}
					}
				}
				GUnrealEd->RedrawLevelEditingViewports();
			}
		}

		Invalidate();
	}

	return bHandled;
}

void FSCSEditorViewportClient::TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	if( !bIsManipulating && bIsDraggingWidget )
	{
		// Suspend component modification during each delta step to avoid recording unnecessary overhead into the transaction buffer
		GEditor->DisableDeltaModification(true);

		// Begin transaction
		BeginTransaction( NSLOCTEXT("UnrealEd", "ModifyComponents", "Modify Component(s)") );
		bIsManipulating = true;
	}
}

void FSCSEditorViewportClient::TrackingStopped() 
{
	if( bIsManipulating )
	{
		// Re-run construction scripts if we haven't done so yet (so that the components in the preview actor can update their transforms)
		AActor* PreviewActor = GetPreviewActor();
		if(PreviewActor != nullptr)
		{
			UBlueprint* PreviewBlueprint = UBlueprint::GetBlueprintFromClass(PreviewActor->GetClass());
			if(PreviewBlueprint != nullptr && !PreviewBlueprint->bRunConstructionScriptOnDrag)
			{
				PreviewActor->RerunConstructionScripts();
			}
		}

		// End transaction
		bIsManipulating = false;
		EndTransaction();

		// Restore component delta modification
		GEditor->DisableDeltaModification(false);
	}
}

FWidget::EWidgetMode FSCSEditorViewportClient::GetWidgetMode() const
{
	// Default to not drawing the widget
	FWidget::EWidgetMode ReturnWidgetMode = FWidget::WM_None;

	AActor* PreviewActor = GetPreviewActor();
	if(!bIsSimulateEnabled && PreviewActor)
	{
		const TSharedPtr<FBlueprintEditor> BluePrintEditor = BlueprintEditorPtr.Pin();
		if ( BluePrintEditor.IsValid() )
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BluePrintEditor->GetSelectedSCSEditorTreeNodes();
			const TArray<FSCSEditorTreeNodePtrType>& RootNodes = BluePrintEditor->GetSCSEditor()->GetRootComponentNodes();

			if (GUnrealEd->ComponentVisManager.IsActive() &&
				GUnrealEd->ComponentVisManager.IsVisualizingArchetype())
			{
				// Component visualizer is active and editing the archetype
				ReturnWidgetMode = WidgetMode;
			}
			else
			{
				// if the selected nodes array is empty, or only contains entries from the
				// root nodes array, or isn't visible in the preview actor, then don't display a transform widget
				for (int32 CurrentNodeIndex = 0; CurrentNodeIndex < SelectedNodes.Num(); CurrentNodeIndex++)
				{
					FSCSEditorTreeNodePtrType CurrentNodePtr = SelectedNodes[CurrentNodeIndex];
					if ((CurrentNodePtr.IsValid() &&
						 ((!RootNodes.Contains(CurrentNodePtr) && !CurrentNodePtr->IsRootComponent()) ||
							(Cast<UInstancedStaticMeshComponent>(CurrentNodePtr->GetComponentTemplate()) && // show widget if we are editing individual instances even if it is the root component
							 CastChecked<UInstancedStaticMeshComponent>(CurrentNodePtr->FindComponentInstanceInActor(GetPreviewActor()))->SelectedInstances.Contains(true))) &&
						 CurrentNodePtr->CanEditDefaults() &&
						 CurrentNodePtr->FindComponentInstanceInActor(PreviewActor)))
					{
						// a non-NULL, non-root item is selected, draw the widget
						ReturnWidgetMode = WidgetMode;
						break;
					}
				}
			}
		}
	}

	return ReturnWidgetMode;
}


void FSCSEditorViewportClient::SetWidgetMode( FWidget::EWidgetMode NewMode )
{
	WidgetMode = NewMode;
}

void FSCSEditorViewportClient::SetWidgetCoordSystemSpace( ECoordSystem NewCoordSystem ) 
{
	WidgetCoordSystem = NewCoordSystem;
}

FVector FSCSEditorViewportClient::GetWidgetLocation() const
{
	FVector ComponentVisWidgetLocation;
	if (GUnrealEd->ComponentVisManager.IsVisualizingArchetype() &&
		GUnrealEd->ComponentVisManager.GetWidgetLocation(this, ComponentVisWidgetLocation))
	{
		return ComponentVisWidgetLocation;
	}

	FVector Location = FVector::ZeroVector;

	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(SelectedNodes.Num() > 0)
		{
			// Use the last selected item for the widget location
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes.Last().Get()->FindComponentInstanceInActor(PreviewActor));
			if( SceneComp )
			{
				TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditorPtr.Pin()->CustomizeSCSEditor(SceneComp);
				FVector CustomLocation;
				if(Customization.IsValid() && Customization->HandleGetWidgetLocation(SceneComp, CustomLocation))
				{
					Location = CustomLocation;
				}
				else
				{
					Location = SceneComp->GetComponentLocation();
				}
			}
		}
	}

	return Location;
}

FMatrix FSCSEditorViewportClient::GetWidgetCoordSystem() const
{
	FMatrix ComponentVisWidgetCoordSystem;
	if (GUnrealEd->ComponentVisManager.IsVisualizingArchetype() &&
		GUnrealEd->ComponentVisManager.GetCustomInputCoordinateSystem(this, ComponentVisWidgetCoordSystem))
	{
		return ComponentVisWidgetCoordSystem;
	}

	FMatrix Matrix = FMatrix::Identity;
	if( GetWidgetCoordSystemSpace() == COORD_Local )
	{
		AActor* PreviewActor = GetPreviewActor();
		auto BlueprintEditor = BlueprintEditorPtr.Pin();
		if (PreviewActor && BlueprintEditor.IsValid())
		{
			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditor->GetSelectedSCSEditorTreeNodes();
			if(SelectedNodes.Num() > 0)
			{
				const auto SelectedNode = SelectedNodes.Last();
				USceneComponent* SceneComp = SelectedNode.IsValid() ? Cast<USceneComponent>(SelectedNode->FindComponentInstanceInActor(PreviewActor)) : NULL;
				if( SceneComp )
				{
					TSharedPtr<ISCSEditorCustomization> Customization = BlueprintEditor->CustomizeSCSEditor(SceneComp);
					FMatrix CustomTransform;
					if(Customization.IsValid() && Customization->HandleGetWidgetTransform(SceneComp, CustomTransform))
					{
						Matrix = CustomTransform;
					}					
					else
					{
						Matrix = FQuatRotationMatrix( SceneComp->GetComponentQuat() );
					}
				}
			}
		}
	}

	if(!Matrix.Equals(FMatrix::Identity))
	{
		Matrix.RemoveScaling();
	}

	return Matrix;
}

int32 FSCSEditorViewportClient::GetCameraSpeedSetting() const
{
	return GetDefault<UEditorPerProjectUserSettings>()->SCSViewportCameraSpeed;
}

void FSCSEditorViewportClient::SetCameraSpeedSetting(int32 SpeedSetting)
{
	GetMutableDefault<UEditorPerProjectUserSettings>()->SCSViewportCameraSpeed = SpeedSetting;
}

void FSCSEditorViewportClient::InvalidatePreview(bool bResetCamera)
{
	// Ensure that the editor is valid before continuing
	if(!BlueprintEditorPtr.IsValid())
	{
		return;
	}

	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();
	check(Blueprint);

	const bool bIsPreviewActorValid = GetPreviewActor() != nullptr;

	// Create or update the Blueprint actor instance in the preview scene
	BlueprintEditorPtr.Pin()->UpdatePreviewActor(Blueprint, !bIsPreviewActorValid);

	Invalidate();
	RefreshPreviewBounds();
	
	if( bResetCamera )
	{
		ResetCamera();
	}
}

void FSCSEditorViewportClient::ResetCamera()
{
	UBlueprint* Blueprint = BlueprintEditorPtr.Pin()->GetBlueprintObj();

	// For now, loosely base default camera positioning on thumbnail preview settings
	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(Blueprint->ThumbnailInfo);
	if(ThumbnailInfo)
	{
		if(PreviewActorBounds.SphereRadius + ThumbnailInfo->OrbitZoom < 0)
		{
			ThumbnailInfo->OrbitZoom = -PreviewActorBounds.SphereRadius;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	ToggleOrbitCamera(true);
	{
		float TargetDistance = PreviewActorBounds.SphereRadius;
		if(TargetDistance <= 0.0f)
		{
			TargetDistance = AutoViewportOrbitCameraTranslate;
		}

		FRotator ThumbnailAngle(ThumbnailInfo->OrbitPitch, ThumbnailInfo->OrbitYaw, 0.0f);

		SetViewLocationForOrbiting(PreviewActorBounds.Origin);
		SetViewLocation( GetViewLocation() + FVector(0.0f, TargetDistance * 1.5f + ThumbnailInfo->OrbitZoom - AutoViewportOrbitCameraTranslate, 0.0f) );
		SetViewRotation( ThumbnailAngle );
	
	}

	Invalidate();
}

void FSCSEditorViewportClient::ToggleRealtimePreview()
{
	SetRealtime(!IsRealtime());

	Invalidate();
}

AActor* FSCSEditorViewportClient::GetPreviewActor() const
{
	return BlueprintEditorPtr.Pin()->GetPreviewActor();
}

void FSCSEditorViewportClient::FocusViewportToSelection()
{
	AActor* PreviewActor = GetPreviewActor();
	if(PreviewActor)
	{
		TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditorPtr.Pin()->GetSelectedSCSEditorTreeNodes();
		if(SelectedNodes.Num() > 0)
		{
			// Use the last selected item for the widget location
			USceneComponent* SceneComp = Cast<USceneComponent>(SelectedNodes.Last()->FindComponentInstanceInActor(PreviewActor));
			if( SceneComp )
			{
				FocusViewportOnBox( SceneComp->Bounds.GetBox() );
			}
		}
		else
		{
			FocusViewportOnBox( PreviewActor->GetComponentsBoundingBox( true ) );
		}
	}
}

bool FSCSEditorViewportClient::GetIsSimulateEnabled() 
{ 
	return bIsSimulateEnabled;
}

void FSCSEditorViewportClient::ToggleIsSimulateEnabled() 
{
	// Must destroy existing actors before we toggle the world state
	BlueprintEditorPtr.Pin()->DestroyPreview();

	bIsSimulateEnabled = !bIsSimulateEnabled;
	PreviewScene->GetWorld()->bBegunPlay = bIsSimulateEnabled;
	PreviewScene->GetWorld()->bShouldSimulatePhysics = bIsSimulateEnabled;

	TSharedPtr<SWidget> SCSEditor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	TSharedRef<SWidget> Inspector = BlueprintEditorPtr.Pin()->GetInspector();

	// When simulate is enabled, we don't want to allow the user to modify the components
	BlueprintEditorPtr.Pin()->UpdatePreviewActor(BlueprintEditorPtr.Pin()->GetBlueprintObj(), true);

	SCSEditor->SetEnabled(!bIsSimulateEnabled);
	Inspector->SetEnabled(!bIsSimulateEnabled);

	if(!IsRealtime())
	{
		ToggleRealtimePreview();
	}
}

bool FSCSEditorViewportClient::GetShowFloor() 
{
	return GetDefault<UEditorPerProjectUserSettings>()->bSCSEditorShowFloor;
}

void FSCSEditorViewportClient::ToggleShowFloor() 
{
	auto* Settings = GetMutableDefault<UEditorPerProjectUserSettings>();

	bool bShowFloor = Settings->bSCSEditorShowFloor;
	bShowFloor = !bShowFloor;
	
	EditorFloorComp->SetVisibility(bShowFloor);
	EditorFloorComp->SetCollisionEnabled(bShowFloor? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	Settings->bSCSEditorShowFloor = bShowFloor;
	Settings->PostEditChange();

	Invalidate();
}

bool FSCSEditorViewportClient::GetShowGrid() 
{
	return GetDefault<UEditorPerProjectUserSettings>()->bSCSEditorShowGrid;
}

void FSCSEditorViewportClient::ToggleShowGrid() 
{
	auto* Settings = GetMutableDefault<UEditorPerProjectUserSettings>();

	bool bShowGrid = Settings->bSCSEditorShowGrid;
	bShowGrid = !bShowGrid;

	DrawHelper.bDrawGrid = bShowGrid;

	Settings->bSCSEditorShowGrid = bShowGrid;
	Settings->PostEditChange();
	
	Invalidate();
}

void FSCSEditorViewportClient::BeginTransaction(const FText& Description)
{
	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::BeginTransaction() pre: %s %08x"), SessionName, *((uint32*)&ScopedTransaction));

	if(!ScopedTransaction)
	{
		ScopedTransaction = new FScopedTransaction(Description);

		auto BlueprintEditor = BlueprintEditorPtr.Pin();
		if (BlueprintEditor.IsValid())
		{
			UBlueprint* PreviewBlueprint = BlueprintEditor->GetBlueprintObj();
			if (PreviewBlueprint != nullptr)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(PreviewBlueprint);
			}

			TArray<FSCSEditorTreeNodePtrType> SelectedNodes = BlueprintEditor->GetSelectedSCSEditorTreeNodes();
			for(auto SelectedSCSNodeIter(SelectedNodes.CreateIterator()); SelectedSCSNodeIter; ++SelectedSCSNodeIter)
			{
				FSCSEditorTreeNodePtrType Node = *SelectedSCSNodeIter;
				if(Node.IsValid())
				{
					if(USCS_Node* SCS_Node = Node->GetSCSNode())
					{
						USimpleConstructionScript* SCS = SCS_Node->GetSCS();
						UBlueprint* Blueprint = SCS ? SCS->GetBlueprint() : nullptr;
						if (Blueprint == PreviewBlueprint)
						{
							SCS_Node->Modify();
						}
					}

					// Modify template, any instances will be reconstructed as part of PostUndo:
					UActorComponent* ComponentTemplate = Node->GetEditableComponentTemplate(PreviewBlueprint);
					if (ComponentTemplate != nullptr)
					{
						ComponentTemplate->SetFlags(RF_Transactional);
						ComponentTemplate->Modify();
					}
				}
			}
		}
	}

	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::BeginTransaction() post: %s %08x"), SessionName, *((uint32*)&ScopedTransaction));
}

void FSCSEditorViewportClient::EndTransaction()
{
	//UE_LOG(LogSCSEditorViewport, Log, TEXT("FSCSEditorViewportClient::EndTransaction(): %08x"), *((uint32*)&ScopedTransaction));

	if(ScopedTransaction)
	{
		delete ScopedTransaction;
		ScopedTransaction = NULL;
	}
}

void FSCSEditorViewportClient::RefreshPreviewBounds()
{
	AActor* PreviewActor = GetPreviewActor();

	if(PreviewActor)
	{
		// Compute actor bounds as the sum of its visible parts
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		PreviewActor->GetComponents(PrimitiveComponents);

		PreviewActorBounds = FBoxSphereBounds(ForceInitToZero);
		for(auto CompIt = PrimitiveComponents.CreateConstIterator(); CompIt; ++CompIt)
		{
			// Aggregate primitive components that either have collision enabled or are otherwise visible components in-game
			UPrimitiveComponent* PrimComp = *CompIt;
			if(PrimComp->IsRegistered() && (!PrimComp->bHiddenInGame || PrimComp->IsCollisionEnabled()) && PrimComp->Bounds.SphereRadius < HALF_WORLD_MAX)
			{
				PreviewActorBounds = PreviewActorBounds + PrimComp->Bounds;
			}
		}
	}
}
