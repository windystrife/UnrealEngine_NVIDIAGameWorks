// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportClickHandlers.h"
#include "UObject/Class.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/Brush.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "LevelEditorViewport.h"
#include "Components/PrimitiveComponent.h"
#include "Model.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TargetPoint.h"
#include "AssetData.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Dialogs/Dialogs.h"
#include "ScopedTransaction.h"
#include "ILevelEditor.h"
#include "SnappingUtils.h"
#include "Editor/GeometryMode/Public/EditorGeometry.h"
#include "Editor/GeometryMode/Public/GeometryEdMode.h"
#include "Logging/MessageLog.h"
#include "ActorEditorUtils.h"
#include "Editor/ActorPositioning.h"
#include "StaticLightingSystem/StaticLightingPrivate.h"
#include "LightMap.h"

#define LOCTEXT_NAMESPACE "ClickHandlers"

namespace ClickHandlers
{
	static void PrivateSummonContextMenu( FLevelEditorViewportClient* ViewportClient )
	{
		if( ViewportClient->ParentLevelEditor.IsValid() )
		{
			ViewportClient->ParentLevelEditor.Pin()->SummonLevelViewportContextMenu();
		}
	}	

	static void PrivateSummonViewportMenu( FLevelEditorViewportClient* ViewportClient )
	{
		if (ViewportClient->ParentLevelEditor.IsValid())
		{
			ViewportClient->ParentLevelEditor.Pin()->SummonLevelViewportViewOptionMenu(LVT_Perspective);
		}
	}

	/**
 	 * Creates an actor of the specified type, trying first to find an actor factory,
	 * falling back to "ACTOR ADD" exec and SpawnActor if no factory is found.
	 * Does nothing if ActorClass is NULL.
	 */
	static AActor* PrivateAddActor(UClass* ActorClass)
	{
		if ( ActorClass )
		{
			// use an actor factory if possible
			UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass( ActorClass );
			if( ActorFactory )
			{
				return GEditor->UseActorFactory( ActorFactory, FAssetData(), nullptr );
			}
			// otherwise use AddActor so that we can return the newly created actor
			else
			{
				const FTransform ActorTransform = FActorPositioning::GetCurrentViewportPlacementTransform(*ActorClass->GetDefaultObject<AActor>());
				return GEditor->AddActor( GCurrentLevelEditingViewportClient->GetWorld()->GetCurrentLevel(), ActorClass, ActorTransform );
			}
		}
		return NULL;
	}

	/**
	 * This function picks a color from under the mouse in the viewport and adds a light with that color.
	 * This is to make it easy for LDs to add lights that fake radiosity.
	 * @param Viewport	Viewport to pick color from.
	 * @param Click		A class that has information about where and how the user clicked on the viewport.
	 */
	void PickColorAndAddLight(FViewport* Viewport, const FViewportClick &Click)
	{
		// Read pixels from viewport.
		TArray<FColor> OutputBuffer;

		// We need to redraw the viewport before reading pixels otherwise we may be reading back from an old buffer.
		Viewport->Draw();
		Viewport->ReadPixels(OutputBuffer);

		// Sample the color we want.
		const int32 ClickX = Click.GetClickPos().X;
		const int32 ClickY = Click.GetClickPos().Y;
		const int32 PixelIdx = ClickX + ClickY * (int32)Viewport->GetSizeXY().X;

		if(PixelIdx < OutputBuffer.Num())
		{
			const FColor PixelColor = OutputBuffer[PixelIdx];

			AActor* NewActor = PrivateAddActor( APointLight::StaticClass() );

			APointLight* Light = CastChecked<APointLight>(NewActor);
			Light->SetMobility(EComponentMobility::Stationary);
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>( Light->GetLightComponent() );
			PointLightComponent->LightColor = PixelColor;
		}
	}

	bool ClickViewport(FLevelEditorViewportClient* ViewportClient, const FViewportClick& Click)
	{
		if (Click.GetKey() == EKeys::MiddleMouseButton && Click.IsControlDown())
		{
			PrivateSummonViewportMenu(ViewportClient);
			return true;
		}
		return false;
	}

	bool ClickActor(FLevelEditorViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,bool bAllowSelectionChange)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		// Handle selection.
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(EKeys::LeftMouseButton) )
		{
			bool bNeedViewportRefresh = false;
			if( Actor )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActorsContextMenu", "Clicking on Actors (context menu)") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (context menu): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel() );

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// If the actor the user clicked on was already selected, then we won't bother clearing the selection
					if( !Actor->IsSelected() )
					{
						GEditor->SelectNone( false, true );
						bNeedViewportRefresh = true;
					}

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, true, true );
				}
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			PrivateSummonContextMenu(ViewportClient);
			return true;
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == EKeys::LeftMouseButton && !Click.IsControlDown() && !Click.IsShiftDown() )
		{
			if( Actor )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActorsDouble-Click", "Clicking on Actors (double-click)") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (double click): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());

				GEditor->GetSelectedActors()->Modify();

				if( bAllowSelectionChange )
				{
					// Clear the selection
					GEditor->SelectNone( false, true );

					// Select the actor the user clicked on
					GEditor->SelectActor( Actor, true, true );
				}
			}

			return true;
		}
		else if( Click.GetKey() != EKeys::RightMouseButton )
		{
			if ( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) && Actor )
			{
				TArray<UActorComponent*> Components;
				Actor->GetComponents(Components);
				SetDebugLightmapSample(&Components, NULL, 0, GEditor->ClickLocation);
			}
			else 
			if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
			{
				// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
				if(Click.IsControlDown())
				{
					PickColorAndAddLight(ViewportClient->Viewport, Click);
				}
				else
				{
					// Create a point light (they default to stationary)
					PrivateAddActor( APointLight::StaticClass() );
				}

				return true;
			}
			else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
			{
				// Create a static mesh.
				PrivateAddActor( AStaticMeshActor::StaticClass() );

				return true;
			}
			else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
			{
				// Create an actor of the selected class.
				UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
				if( SelectedClass )
				{
					PrivateAddActor( SelectedClass );
				}

				return true;
			}
			else if ( Actor )
			{
				if( bAllowSelectionChange )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnActors", "Clicking on Actors") );
					GEditor->GetSelectedActors()->Modify();

					// Ctrl- or shift- clicking an actor is the same as regular clicking when components are selected
					const bool bComponentSelected = GEditor->GetSelectedComponentCount() > 0;

					if( Click.IsControlDown() && !bComponentSelected )
					{
						const bool bSelect = !Actor->IsSelected();
						if( bSelect )
						{
							UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (CTRL LMB): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
						}
						GEditor->SelectActor( Actor, bSelect, true, true );
					}
					else if( Click.IsShiftDown() && !bComponentSelected )
					{
						if( !Actor->IsSelected() )
						{
							const bool bSelect = true;
							GEditor->SelectActor( Actor, bSelect, true, true );
						}
					}
					else
					{
						// check to see how many actors need deselecting first - and warn as appropriate
						int32 NumSelectedActors = GEditor->GetSelectedActors()->Num();
						if( NumSelectedActors >= EditorActorSelectionDefs::MaxActorsToSelectBeforeWarning )
						{
							const FText ConfirmText = FText::Format( NSLOCTEXT( "UnrealEd", "Warning_ManyActorsToSelectOne", "There are {0} selected actors. Selecting this actor will deselect them all. Are you sure?" ), FText::AsNumber(NumSelectedActors) );

							FSuppressableWarningDialog::FSetupInfo Info( ConfirmText, NSLOCTEXT( "UnrealEd", "Warning_ManyActors", "Warning: Many Actors" ), "Warning_ManyActors" );
							Info.ConfirmText = NSLOCTEXT("ModalDialogs", "ManyActorsToSelectOneConfirm", "Continue Selection");
							Info.CancelText = NSLOCTEXT("ModalDialogs", "ManyActorsToSelectOneCancel", "Keep Current Selection");

							FSuppressableWarningDialog ManyActorsWarning( Info );
							if( ManyActorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
							{
								return false;
							}
						}

						GEditor->SelectNone( false, true, false );
						UE_LOG(LogEditorViewport, Log,  TEXT("Clicking on Actor (LMB): %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
						GEditor->SelectActor( Actor, true, true, true );
					}
				}

				return false;
			}
		}

		return false;
	}

	bool ClickComponent(FLevelEditorViewportClient* ViewportClient, HActor* ActorHitProxy, const FViewportClick& Click)
	{
		//@todo hotkeys for component placement?
		
		bool bComponentClicked = false;

		USceneComponent* Component = nullptr;

		if (ActorHitProxy->Actor->IsChildActor())
		{
			AActor* TestActor = ActorHitProxy->Actor;
			do
			{
				Component = TestActor->GetParentComponent();
				TestActor = TestActor->GetParentActor();
			} while (TestActor->IsChildActor());
		}
		else
		{
			UPrimitiveComponent* TestComponent = const_cast<UPrimitiveComponent*>(ActorHitProxy->PrimComponent);
			if (ActorHitProxy->Actor->GetComponents().Contains(TestComponent))
			{
				Component = TestComponent;
			}
		}

		//If the component selected is editor-only, we want to select the non-editor-only component it's attached to
		while (Component != nullptr && Component->IsEditorOnly())
		{
			Component = Component->GetAttachParent();
		}
		
		if (!ensure(Component != nullptr))
		{
			return false;
		}

		// Pivot snapping
		if (Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown())
		{
			GEditor->SetPivot(GEditor->ClickLocation, true, false);

			return true;
		}
		// Selection + context menu
		else if (Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(EKeys::LeftMouseButton))
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnComponentContextMenu", "Clicking on Component (context menu)"));
			UE_LOG(LogEditorViewport, Log, TEXT("Clicking on Component (context menu): %s (%s)"), *Component->GetClass()->GetName(), *Component->GetName());

			auto const EditorComponentSelection = GEditor->GetSelectedComponents();
			EditorComponentSelection->Modify();

			// If the component the user clicked on was already selected, then we won't bother clearing the selection
			bool bNeedViewportRefresh = false;
			if (!EditorComponentSelection->IsSelected(Component))
			{
				EditorComponentSelection->DeselectAll();
				bNeedViewportRefresh = true;
			}

			GEditor->SelectComponent(Component, true, true);
			
			if (bNeedViewportRefresh)
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			PrivateSummonContextMenu(ViewportClient);
			bComponentClicked = true;
		}
		// Selection only
		else if (Click.GetKey() == EKeys::LeftMouseButton)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "ClickingOnComponents", "Clicking on Components"));
			GEditor->GetSelectedComponents()->Modify();

			if (Click.IsControlDown())
			{
				const bool bSelect = !Component->IsSelected();
				if (bSelect)
				{
					UE_LOG(LogEditorViewport, Log, TEXT("Clicking on Component (CTRL LMB): %s (%s)"), *Component->GetClass()->GetName(), *Component->GetName());
				}
				GEditor->SelectComponent(Component, bSelect, true, true);
				bComponentClicked = true;
			}
			else if (Click.IsShiftDown())
			{
				if (!Component->IsSelected())
				{
					UE_LOG(LogEditorViewport, Log, TEXT("Clicking on Component (SHIFT LMB): %s (%s)"), *Component->GetClass()->GetName(), *Component->GetName());
					GEditor->SelectComponent(Component, true, true, true);
				}
				bComponentClicked = true;
			}
			else
			{
				GEditor->GetSelectedComponents()->DeselectAll();
				UE_LOG(LogEditorViewport, Log, TEXT("Clicking on Component (LMB): %s (%s)"), *Component->GetClass()->GetName(), *Component->GetName());
				GEditor->SelectComponent(Component, true, true, true);
				bComponentClicked = true;
			}
		}
		
		return bComponentClicked;
	}

	void ClickBrushVertex(FLevelEditorViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::RightMouseButton )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnBrushVertex", "Clicking on Brush Vertex") );
			const FTransform ActorToWorld = InBrush->ActorToWorld();
			GEditor->SetPivot( ActorToWorld.TransformPosition(*InVertex), false, false );

			const FVector World = ActorToWorld.TransformPosition(*InVertex);
			FVector Snapped = World;
			FSnappingUtils::SnapPointToGrid( Snapped, FVector(GEditor->GetGridSize()) );
			const FVector Delta = Snapped - World;
			GEditor->SetPivot( Snapped, false, false );

			if( GLevelEditorModeTools().IsDefaultModeActive() )
			{		
				// All selected actors need to move by the delta.
				for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
				{
					AActor* Actor = static_cast<AActor*>( *It );
					checkSlow( Actor->IsA(AActor::StaticClass()) );

					Actor->Modify();

					FVector ActorLocation = Actor->GetActorLocation() + Delta;
					Actor->SetActorLocation(ActorLocation, false);
				}
			}

			ViewportClient->Invalidate( true, true );

			// Update Bsp
			GEditor->RebuildAlteredBSP();
		}
	}

	void ClickStaticMeshVertex(FLevelEditorViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::RightMouseButton )
		{
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingOnStaticMeshVertex", "Clicking on Static Mesh Vertex") );

			FVector Snapped = InVertex;
			FSnappingUtils::SnapPointToGrid( Snapped, FVector(GEditor->GetGridSize()) );
			const FVector Delta = Snapped - InVertex;
			GEditor->SetPivot( Snapped, false, true );

			// All selected actors need to move by the delta.
			for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
			{
				AActor* Actor = static_cast<AActor*>( *It );
				checkSlow( Actor->IsA(AActor::StaticClass()) );

				Actor->Modify();

				FVector ActorLocation = Actor->GetActorLocation() + Delta;
				Actor->SetActorLocation(ActorLocation, false);
			}

			ViewportClient->Invalidate( true, true );
		}
	}

	bool ClickGeomPoly(FLevelEditorViewportClient* ViewportClient, HGeomPolyProxy* InHitProxy, const FViewportClick& Click)
	{
		//something is wrong with the hitproxy relating to this click - create a debug log to help identify what
		if( InHitProxy == NULL )
		{
			UE_LOG(LogEditorViewport, Warning, TEXT("Invalid hitproxy"));
			return false;
		}

		if( !InHitProxy->GeomObjectWeakPtr.IsValid() )
		{
			return false;
		}

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GetGeomObject()->GetActualBrush(), false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			// This should only happen in geometry mode
			FEdMode* Mode = GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );
			if( Mode )
			{
				if( ( InHitProxy->GetGeomObject() != NULL ) && (InHitProxy->GetGeomObject()->PolyPool.IsValidIndex( InHitProxy->PolyIndex ) == true ) )					
				{
					Mode->GetCurrentTool()->StartTrans();

					if( !Click.IsControlDown() )
					{
						Mode->GetCurrentTool()->SelectNone();
					}

					FGeomPoly& gp = InHitProxy->GetGeomObject()->PolyPool[ InHitProxy->PolyIndex ];
					gp.Select( Click.IsControlDown() ? !gp.IsSelected() : true );

					Mode->SelectionChanged();

					Mode->GetCurrentTool()->EndTrans();
					ViewportClient->Invalidate( true, false );
				}
				else
				{
					//try to get the name of the object also
					FString name = TEXT("UNKNOWN");
					if( InHitProxy->GetGeomObject()->GetActualBrush() != NULL )
					{
						name = InHitProxy->GetGeomObject()->GetActualBrush()->GetName();
					}
					UE_LOG(LogEditorViewport, Warning, TEXT("Invalid PolyIndex %d on %s" ) ,InHitProxy->PolyIndex , *name );
				}
			}
		}

		return false;
	}

	/**
	 * Utility method used by ClickGeomEdge and ClickGeomVertex.  Returns true if the projections of
	 * the vectors onto the specified viewport plane are equal within the given tolerance.
	 */
	bool OrthoEqual(ELevelViewportType ViewportType, const FVector& Vec0, const FVector& Vec1, float Tolerance=0.1f)
	{
		bool bResult = false;
		switch( ViewportType )
		{
		case LVT_OrthoXY:
		case LVT_OrthoNegativeXY:
			bResult = FMath::Abs(Vec0.X - Vec1.X) < Tolerance && FMath::Abs(Vec0.Y - Vec1.Y) < Tolerance;
			break;
		case LVT_OrthoXZ:
		case LVT_OrthoNegativeXZ:
			bResult = FMath::Abs(Vec0.X - Vec1.X) < Tolerance && FMath::Abs(Vec0.Z - Vec1.Z) < Tolerance;
			break;
		case LVT_OrthoYZ:
		case LVT_OrthoNegativeYZ:
			bResult = FMath::Abs(Vec0.Y - Vec1.Y) < Tolerance && FMath::Abs(Vec0.Z - Vec1.Z) < Tolerance;
			break;
		default:
			check( 0 );
			break;
		}
		return bResult;
	}

	bool ClickGeomEdge(FLevelEditorViewportClient* ViewportClient, HGeomEdgeProxy* InHitProxy, const FViewportClick& Click)
	{
		if( !InHitProxy->GetGeomObject() )
		{
			return false;
		}

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GetGeomObject()->GetActualBrush(), false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			FEdMode* Mode = GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );

			if( Mode )
			{ 
				Mode->GetCurrentTool()->StartTrans();

				const bool bControlDown = Click.IsControlDown();
				if( !bControlDown )
				{
					Mode->GetCurrentTool()->SelectNone();
				}

				FGeomEdge& HitEdge = InHitProxy->GetGeomObject()->EdgePool[ InHitProxy->EdgeIndex ];
				HitEdge.Select( bControlDown ? !HitEdge.IsSelected() : true );

				if( ViewportClient->IsOrtho() )
				{
					// Select all edges in the brush that match the projected mid point of the original edge.
					for( int32 EdgeIndex = 0 ; EdgeIndex < InHitProxy->GetGeomObject()->EdgePool.Num() ; ++EdgeIndex )
					{
						if ( EdgeIndex != InHitProxy->EdgeIndex )
						{
							FGeomEdge& GeomEdge = InHitProxy->GetGeomObject()->EdgePool[ EdgeIndex ];
							if ( OrthoEqual( ViewportClient->ViewportType, GeomEdge.GetMid(), HitEdge.GetMid() ) )
							{
								GeomEdge.Select( bControlDown ? !GeomEdge.IsSelected() : true );
							}
						}
					}
				}

				Mode->SelectionChanged();

				Mode->GetCurrentTool()->EndTrans();
				ViewportClient->Invalidate( true, true );
				return true;
			}

			return false;


		}

		return false;
	}

	bool ClickGeomVertex(FLevelEditorViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click)
	{
		if(InHitProxy->GetGeomObject() == nullptr)
		{
			return false;
		}

		if( !GLevelEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_Geometry ) )
		{
			return false;
		}

		FEdModeGeometry* Mode = static_cast<FEdModeGeometry*>( GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Geometry) );

		// Note: The expected behavior is that right clicking on a vertex will snap the vertex that was
		// right-clicked on to the nearest grid point, then move all SELECTED verts by the appropriate
		// delta.  So, we need to handle the right mouse button click BEFORE we change the selection set below.

		if( Click.GetKey() == EKeys::RightMouseButton )
		{

			if (InHitProxy->VertexIndex < 0 || InHitProxy->VertexIndex >= InHitProxy->GetGeomObject()->VertexPool.Num() )
			{
				UE_LOG(LogEditorViewport, Warning, TEXT("Invalid InHitProxy->VertexIndex" ) );		
				return false;
			}

			FModeTool_GeometryModify* Tool = static_cast<FModeTool_GeometryModify*>( Mode->GetCurrentTool() );
			Tool->StartTrans();

			// Compute out far to move to get back on the grid.
			const FVector WorldLoc = InHitProxy->GetGeomObject()->GetActualBrush()->ActorToWorld().TransformPosition( InHitProxy->GetGeomObject()->VertexPool[ InHitProxy->VertexIndex ] );

			FVector SnappedLoc = WorldLoc;
			FSnappingUtils::SnapPointToGrid( SnappedLoc, FVector(GEditor->GetGridSize()) );

			const FVector Delta = SnappedLoc - WorldLoc;
			GEditor->SetPivot( SnappedLoc, false, false );

			for( int32 VertexIndex = 0 ; VertexIndex < InHitProxy->GetGeomObject()->VertexPool.Num() ; ++VertexIndex )
			{
				FGeomVertex& GeomVertex = InHitProxy->GetGeomObject()->VertexPool[VertexIndex];
				if( GeomVertex.IsSelected() )
				{
					GeomVertex += Delta;
				}
			}

			Tool->EndTrans();
			InHitProxy->GetGeomObject()->SendToSource();
			ViewportClient->Invalidate( true, true );

			// HACK: The Bsp update has to occur after SendToSource() updates the vert pool, putting it outside
			// of the mode tool's transaction, therefore, the Bsp update requires a transaction of its own
			{
				FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GeoModeVertexSnap", "Vertex Snap") );

				// Update Bsp
				GEditor->RebuildAlteredBSP();
			}
		}

		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			// Pivot snapping

			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );

			return true;
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsControlDown() && Click.IsShiftDown() && !Click.IsAltDown() )
		{
			GEditor->SelectActor( InHitProxy->GetGeomObject()->GetActualBrush(), false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			Mode->GetCurrentTool()->StartTrans();

			// Disable Ctrl+clicking for selection if selecting with RMB.
			const bool bControlDown = Click.IsControlDown();
			if( !bControlDown )
			{
				Mode->GetCurrentTool()->SelectNone();
			}

			FGeomVertex& HitVertex = InHitProxy->GetGeomObject()->VertexPool[ InHitProxy->VertexIndex ];
			bool bSelect = bControlDown ? !HitVertex.IsSelected() : true;

			HitVertex.Select( bSelect );

			if( ViewportClient->IsOrtho() )
			{
				// Select all vertices that project to the same location.
				for( int32 VertexIndex = 0 ; VertexIndex < InHitProxy->GetGeomObject()->VertexPool.Num() ; ++VertexIndex )
				{
					if ( VertexIndex != InHitProxy->VertexIndex )
					{
						FGeomVertex& GeomVertex = InHitProxy->GetGeomObject()->VertexPool[VertexIndex];
						if ( OrthoEqual( ViewportClient->ViewportType, GeomVertex, HitVertex ) )
						{
							GeomVertex.Select( bSelect );
						}
					}
				}
			}

			Mode->SelectionChanged();

			Mode->GetCurrentTool()->EndTrans();

			ViewportClient->Invalidate( true, true );

			return true;
		}

		return false;
	}


	static FBspSurf GSaveSurf;

	void ClickSurface(FLevelEditorViewportClient* ViewportClient,UModel* Model,int32 iSurf,const FViewportClick& Click)
	{
		// Gizmos can cause BSP surfs to become selected without this check
		if(Click.GetKey() == EKeys::RightMouseButton && Click.IsControlDown())
		{
			return;
		}

		// Remember hit location for actor-adding.
		FBspSurf& Surf			= Model->Surfs[iSurf];

		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsShiftDown() && Click.IsControlDown() )
		{

			if( !GetDefault<ULevelEditorViewportSettings>()->bClickBSPSelectsBrush )
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if( Surf.Actor )
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectBrushFromSurface", "Select Brush from Surface") );

					// If the builder brush is selected, first deselect it.
					USelection* SelectedActors = GEditor->GetSelectedActors();
					for ( FSelectionIterator It( *SelectedActors ) ; It ; ++It )
					{
						ABrush* Brush = Cast<ABrush>( *It );
						if ( Brush && FActorEditorUtils::IsABuilderBrush( Brush ) )
						{
							GEditor->SelectActor( Brush, false, false );
							break;
						}
					}

					GEditor->SelectActor( Surf.Actor, true, true );
				}
			}
			else
			{
				// Select or deselect surfaces.
				{
					const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurfaces", "Select Surfaces") );
					Model->ModifySurf( iSurf, false );
					Surf.PolyFlags ^= PF_Selected;
				}
				GEditor->NoteSelectionChange();
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && Click.IsShiftDown() )
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			// Apply texture to all selected.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ApplyMaterialToSelectedSurfaces", "Apply Material to Selected Surfaces") );

			UMaterialInterface* SelectedMaterialInstance = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			for( int32 i=0; i<Model->Surfs.Num(); i++ )
			{
				if( Model->Surfs[i].PolyFlags & PF_Selected )
				{
					Model->ModifySurf( i, 1 );
					Model->Surfs[i].Material = SelectedMaterialInstance;
					const bool bUpdateTexCoords = false;
					const bool bOnlyRefreshSurfaceMaterials = true;
					GEditor->polyUpdateMaster(Model, i, bUpdateTexCoords, bOnlyRefreshSurfaceMaterials);
				}
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass );
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light (they default to stationary)
				PrivateAddActor( APointLight::StaticClass() );
			}
		}
		else if( IsTexelDebuggingEnabled() && Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) )
		{
			SetDebugLightmapSample(NULL, Model, iSurf, GEditor->ClickLocation);
		}

		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass() );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::Semicolon) )
		{
			PrivateAddActor( ATargetPoint::StaticClass() );
		}
		else if( Click.IsAltDown() && Click.GetKey() == EKeys::RightMouseButton )
		{
			// Grab the texture.
			GEditor->GetSelectedObjects()->DeselectAll(UMaterialInterface::StaticClass());

			if(Surf.Material)
			{
				GEditor->GetSelectedObjects()->Select(Surf.Material);
			}
			GSaveSurf = Surf;
		}
		else if( Click.IsAltDown() && Click.GetKey() == EKeys::LeftMouseButton)
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			// Apply texture to the one polygon clicked on.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ApplyMaterialToSurface", "Apply Material to Surface") );
			Model->ModifySurf( iSurf, true );
			Surf.Material = GEditor->GetSelectedObjects()->GetTop<UMaterialInterface>();
			if( Click.IsControlDown() )
			{
				Surf.vTextureU	= GSaveSurf.vTextureU;
				Surf.vTextureV	= GSaveSurf.vTextureV;
				if( Surf.vNormal == GSaveSurf.vNormal )
				{
					UE_LOG(LogEditorViewport, Log, TEXT("WARNING: the texture coordinates were not parallel to the surface.") );
				}
				Surf.PolyFlags	= GSaveSurf.PolyFlags;
				const bool bUpdateTexCoords = true;
				const bool bOnlyRefreshSurfaceMaterials = true;
				GEditor->polyUpdateMaster(Model, iSurf, bUpdateTexCoords, bOnlyRefreshSurfaceMaterials);
			}
			else
			{
				const bool bUpdateTexCoords = false;
				const bool bOnlyRefreshSurfaceMaterials = true;
				GEditor->polyUpdateMaster(Model, iSurf, bUpdateTexCoords, bOnlyRefreshSurfaceMaterials);
			}
		}
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() )
		{
			// Select surface and display context menu
			check( Model );

			bool bNeedViewportRefresh = false;
			bool bSelectionChanged = !Surf.Actor || !Surf.Actor->IsSelected();
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurfaces", "Select Surfaces") );

				USelection* SelectedActors = GEditor->GetSelectedActors();
				SelectedActors->BeginBatchSelectOperation();

				// We only need to unselect surfaces if the surface the user clicked on was not already selected
				if( !( Surf.PolyFlags & PF_Selected ) )
				{
					GEditor->SelectNone( false, true );
					bNeedViewportRefresh = true;
					bSelectionChanged = true;
				}

				// Select the surface the user clicked on
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags |= PF_Selected;

				GEditor->SelectActor(Surf.Actor, true, false);
				SelectedActors->EndBatchSelectOperation(false);

				if (bSelectionChanged)
				{
					GEditor->NoteSelectionChange();
				}
			}

			if( bNeedViewportRefresh )
			{
				// Redraw the viewport so the user can see which object was right clicked on
				ViewportClient->Viewport->Draw();
				FlushRenderingCommands();
			}

			PrivateSummonContextMenu(ViewportClient);
		}
		else if( Click.GetEvent() == IE_DoubleClick && Click.GetKey() == EKeys::LeftMouseButton && !Click.IsControlDown() )
		{
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectSurface", "Select Surface") );

				// Clear the selection
				GEditor->SelectNone( false, true );

				// Select the surface
				const uint32 SelectMask = Surf.PolyFlags & PF_Selected;
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags = ( Surf.PolyFlags & ~PF_Selected ) | ( SelectMask ^ PF_Selected );				
			}
			GEditor->NoteSelectionChange();

			// Display the surface properties window
			GEditor->Exec( ViewportClient->GetWorld(), TEXT("EDCALLBACK SURFPROPS") );
		}
		else
		{	
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "SelectBrushSurface", "Select Brush Surface") );
			bool bDeselectAlreadyHandled = false;
			bool bSelectionChanged = !Surf.Actor || !Surf.Actor->IsSelected();

			USelection* SelectedActors = GEditor->GetSelectedActors();
			SelectedActors->BeginBatchSelectOperation();

			// We are going to handle the notification ourselves
			const bool bNotify = false;
			if(GetDefault<ULevelEditorViewportSettings>()->bClickBSPSelectsBrush)
			{
				// Add to the actor selection set the brush actor that belongs to this BSP surface.
				// Check Surf.Actor, as it can be NULL after deleting brushes and before rebuilding BSP.
				if(Surf.Actor)
				{
					if(!Click.IsControlDown())
					{
						GEditor->SelectNone(false, true);
						bDeselectAlreadyHandled = true;
					}
					// If the builder brush is selected, first deselect it.
					for(FSelectionIterator It(*SelectedActors); It; ++It)
					{
						ABrush* Brush = Cast<ABrush>(*It);
						if(Brush && FActorEditorUtils::IsABuilderBrush(Brush))
						{
							GEditor->SelectActor(Brush, false, bNotify);
							break;
						}
					}

					GEditor->SelectActor(Surf.Actor, true, bNotify);
				}
			}

			// Select or deselect surfaces.
			{
				if (Click.IsControlDown() || !(Surf.PolyFlags & PF_Selected))
				{
					bSelectionChanged = true;
				}

				if( !Click.IsControlDown() && !bDeselectAlreadyHandled)
				{
					GEditor->SelectNone( false, true );
				}
				Model->ModifySurf( iSurf, false );
				Surf.PolyFlags ^= PF_Selected;

				// If there are no surfaces selected now, deselect the actor
				if (!Model->HasSelectedSurfaces())
				{
					GEditor->SelectActor(Surf.Actor, false, bNotify);
					bSelectionChanged = true;
				}
			}

			SelectedActors->EndBatchSelectOperation(false);

			if (bSelectionChanged)
			{
				GEditor->NoteSelectionChange();
			}
		}
	}

	void ClickBackdrop(FLevelEditorViewportClient* ViewportClient,const FViewportClick& Click)
	{
		// Pivot snapping
		if( Click.GetKey() == EKeys::MiddleMouseButton && Click.IsAltDown() )
		{
			GEditor->SetPivot( GEditor->ClickLocation, true, false, true );
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::A) )
		{
			// Create an actor of the selected class.
			UClass* SelectedClass = GEditor->GetSelectedObjects()->GetTop<UClass>();
			if( SelectedClass )
			{
				PrivateAddActor( SelectedClass );
			}
		}
		else if( IsTexelDebuggingEnabled() && Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::T) )
		{
			SetDebugLightmapSample(NULL, NULL, 0, GEditor->ClickLocation);
		}

		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::L) )
		{
			// If shift is down, we pick a color from under the mouse in the viewport and create a light with that color.
			if(Click.IsControlDown())
			{
				PickColorAndAddLight(ViewportClient->Viewport, Click);
			}
			else
			{
				// Create a point light (they default to stationary)
				PrivateAddActor( APointLight::StaticClass() );
			}
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton && ViewportClient->Viewport->KeyState(EKeys::S) )
		{
			// Create a static mesh.
			PrivateAddActor( AStaticMeshActor::StaticClass() );
		}
		else if( Click.GetKey() == EKeys::RightMouseButton && !Click.IsControlDown() && !ViewportClient->Viewport->KeyState(EKeys::LeftMouseButton) )
		{
			// NOTE: We intentionally do not deselect selected actors here even though the user right
			//		clicked on an empty background.  This is because LDs often use wireframe modes to
			//		interact with brushes and such, and it's easier to summon the context menu for
			//		these actors when right clicking *anywhere* will not deselect things.

			// Redraw the viewport so the user can see which object was right clicked on
			ViewportClient->Viewport->Draw();
			FlushRenderingCommands();

			PrivateSummonContextMenu(ViewportClient);
		}
		else if( Click.GetKey() == EKeys::LeftMouseButton )
		{
			if( !Click.IsControlDown() )
			{
				const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ClickingBackground", "Clicking Background") );
				UE_LOG(LogEditorViewport, Log,  TEXT("Clicking Background") );
				GEditor->SelectNone( true, true );
			}
		}
	}

	void ClickLevelSocket(FLevelEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "LevelSocketClicked", "Level Socket Clicked") );

		FMessageLog EditorErrors("EditorErrors");
		EditorErrors.NewPage(LOCTEXT("SocketClickedNewPage", "Socket Clicked"));

		// Attach the selected actors to the socket that was clicked
		HLevelSocketProxy* SocketProxy = static_cast<HLevelSocketProxy*>( HitProxy );
		check( SocketProxy->SceneComponent );
		check( SocketProxy->Actor );

		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			if (AActor* Actor = Cast<AActor>( *It ))
			{
				// Parent actors and handle socket snapping.
				// Will cause editor to refresh viewport.
				FText ReasonText;
				if( GEditor->CanParentActors( SocketProxy->Actor, Actor, &ReasonText ) == false )
				{
					EditorErrors.Error(ReasonText);
				}
				else
				{
					GEditor->ParentActors(SocketProxy->Actor, Actor, SocketProxy->SocketName, SocketProxy->SceneComponent);
				}
			}
		}

		// Report errors
		EditorErrors.Notify(NSLOCTEXT("ActorAttachmentError", "AttachmentsFailed", "Attachments Failed!"));
	}
}

#undef LOCTEXT_NAMESPACE
