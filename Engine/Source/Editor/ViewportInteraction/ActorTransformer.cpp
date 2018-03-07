// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ActorTransformer.h"
#include "ActorViewportTransformable.h"
#include "ViewportWorldInteraction.h"
#include "Engine/Selection.h"
#include "ViewportInteractionAssetContainer.h"
#include "ViewportInteractor.h"
#include "Editor.h"

void UActorTransformer::Init( UViewportWorldInteraction* InitViewportWorldInteraction )
{
	Super::Init( InitViewportWorldInteraction );

	// Find out about selection changes
	USelection::SelectionChangedEvent.AddUObject( this, &UActorTransformer::OnActorSelectionChanged );
}


void UActorTransformer::Shutdown()
{
	USelection::SelectionChangedEvent.RemoveAll( this );

	Super::Shutdown();
}


void UActorTransformer::OnStartDragging(class UViewportInteractor* Interactor)
{
	const UViewportInteractionAssetContainer& AssetContainer = ViewportWorldInteraction->GetAssetContainer();
	const FVector& SoundLocation = Interactor->GetInteractorData().GizmoLastTransform.GetLocation();
	const EViewportInteractionDraggingMode DraggingMode = Interactor->GetDraggingMode();

	if (DraggingMode == EViewportInteractionDraggingMode::TransformablesWithGizmo)
	{
		ViewportWorldInteraction->PlaySound(AssetContainer.GizmoHandleSelectedSound, SoundLocation);
	}
	else if (DraggingMode == EViewportInteractionDraggingMode::TransformablesFreely ||
		DraggingMode == EViewportInteractionDraggingMode::TransformablesAtLaserImpact)
	{
		ViewportWorldInteraction->PlaySound(AssetContainer.SelectionStartDragSound, SoundLocation);
	}
}

void UActorTransformer::OnStopDragging(class UViewportInteractor* Interactor)
{
	const UViewportInteractionAssetContainer& AssetContainer = ViewportWorldInteraction->GetAssetContainer();
	const FVector& SoundLocation = Interactor->GetInteractorData().GizmoLastTransform.GetLocation();
	const EViewportInteractionDraggingMode DraggingMode = Interactor->GetDraggingMode();

	if (DraggingMode == EViewportInteractionDraggingMode::TransformablesWithGizmo)
	{
		ViewportWorldInteraction->PlaySound(AssetContainer.GizmoHandleDropSound, SoundLocation);
	}
	else if (DraggingMode == EViewportInteractionDraggingMode::TransformablesFreely ||
		DraggingMode == EViewportInteractionDraggingMode::TransformablesAtLaserImpact)
	{
		ViewportWorldInteraction->PlaySound(AssetContainer.SelectionDropSound, SoundLocation);
	}
}

void UActorTransformer::UpdateTransformables()
{
	TArray<TUniquePtr<FViewportTransformable>> NewTransformables;

	USelection* ActorSelectionSet = GEditor->GetSelectedActors();

	static TArray<UObject*> SelectedActorObjects;
	SelectedActorObjects.Reset();
	ActorSelectionSet->GetSelectedObjects( AActor::StaticClass(), /* Out */ SelectedActorObjects );

	for( UObject* SelectedActorObject : SelectedActorObjects )
	{
		AActor* SelectedActor = Cast<AActor>( SelectedActorObject );
		if( SelectedActor != nullptr )
		{
			// We only are able to move objects that have a root scene component
			if( SelectedActor->GetRootComponent() != nullptr )
			{
				FActorViewportTransformable* Transformable = new FActorViewportTransformable();

				NewTransformables.Add( TUniquePtr<FViewportTransformable>( Transformable ) );

				Transformable->ActorWeakPtr = SelectedActor;
				Transformable->StartTransform = SelectedActor->GetTransform();
			}
		}
	}

	ViewportWorldInteraction->SetTransformables( MoveTemp( NewTransformables ) );
}


void UActorTransformer::OnActorSelectionChanged( UObject* ChangedObject )
{
	UpdateTransformables();
}

