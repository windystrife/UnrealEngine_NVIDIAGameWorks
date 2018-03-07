// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layers/Layers.h"
#include "Engine/Brush.h"
#include "Components/PrimitiveComponent.h"
#include "Layers/Layer.h"
#include "LevelEditorViewport.h"
#include "Misc/IFilter.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Editor.h"

#include "ActorEditorUtils.h"

FLayers::FLayers( const TWeakObjectPtr< UEditorEngine >& InEditor )
{
	check( InEditor.IsValid() );

	Editor = InEditor;
}


FLayers::~FLayers()
{
	FEditorDelegates::MapChange.RemoveAll(this);
}


void FLayers::Initialize()
{
	FEditorDelegates::MapChange.AddRaw(this, &FLayers::OnEditorMapChange);
}

void FLayers::OnEditorMapChange(uint32 MapChangeFlags)
{
	LayersChanged.Broadcast( ELayersAction::Reset, NULL, NAME_None );
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper functions.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TWeakObjectPtr< ULayer > FLayers::EnsureLayerExists( const FName& LayerName )
{
	TWeakObjectPtr< ULayer > Layer;
	if( !TryGetLayer( LayerName, Layer ) )
	{
		Layer = CreateLayer( LayerName );
	}

	return Layer;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on Levels
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FLayers::AddLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level )
{
	if( !Level.IsValid() )
	{
		return;
	}

	for( auto ActorIter = Level->Actors.CreateConstIterator(); ActorIter; ++ActorIter )
	{
		InitializeNewActorLayers( *ActorIter );
	}
}

void FLayers::RemoveLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level )
{
	if( !Level.IsValid() )
	{
		return;
	}

	for( auto ActorIter = Level->Actors.CreateConstIterator(); ActorIter; ++ActorIter )
	{
		DisassociateActorFromLayers( *ActorIter );
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on an individual actor.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FLayers::IsActorValidForLayer( const TWeakObjectPtr< AActor >& Actor )
{
	if( !Actor.IsValid() || Actor->GetClass() == nullptr || Actor->GetWorld() == nullptr)
	{
		return false;
	}

	const bool bIsBuilderBrush			= FActorEditorUtils::IsABuilderBrush(Actor.Get());
	const bool bIsHidden				= ( Actor->GetClass()->GetDefaultObject<AActor>()->bHiddenEd == true );
	const bool bIsInEditorWorld			= ( Actor->GetWorld()->WorldType == EWorldType::Editor );
	const bool bIsValid					= !bIsHidden && !bIsBuilderBrush && bIsInEditorWorld;

	return bIsValid;
}

bool FLayers::InitializeNewActorLayers( const TWeakObjectPtr< AActor >& Actor )
{
	if(	!IsActorValidForLayer( Actor ) )
	{
		return false;
	}

	for( auto LayerNameIt = Actor->Layers.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
	{
		const FName LayerName = *LayerNameIt;
		TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( LayerName );

		Layer->Modify();
		AddActorToStats( Layer, Actor);
	}

	return Actor->Layers.Num() > 0;
}

bool FLayers::DisassociateActorFromLayers( const TWeakObjectPtr< AActor >& Actor )
{
	if(	!IsActorValidForLayer( Actor ) )
	{
		return false;
	}

	bool bChangeOccurred = false;
	for( auto LayerNameIt = Actor->Layers.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
	{
		const FName LayerName = *LayerNameIt;
		TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( LayerName );

		Layer->Modify();
		RemoveActorFromStats( Layer, Actor);
		bChangeOccurred = true;
	}

	return bChangeOccurred;
}


void FLayers::AddActorToStats( const TWeakObjectPtr< ULayer >& Layer, const TWeakObjectPtr< AActor >& Actor )
{
	if( !Actor.IsValid() )
	{
		return;
	}

	UClass* ActorClass = Actor->GetClass();

	bool bFoundClassStats = false;
	for( auto StatsIt = Layer->ActorStats.CreateIterator(); StatsIt; ++StatsIt )
	{
		FLayerActorStats& Stats = *StatsIt;

		if( Stats.Type == ActorClass )
		{
			Stats.Total++;
			bFoundClassStats = true;
			break;
		}
	}

	if( !bFoundClassStats )
	{
		FLayerActorStats NewActorStats;
		NewActorStats.Total = 1;
		NewActorStats.Type = ActorClass;

		Layer->ActorStats.Add( NewActorStats );
	}

	LayersChanged.Broadcast( ELayersAction::Modify, Layer, TEXT( "ActorStats" ) );
}

void FLayers::RemoveActorFromStats( const TWeakObjectPtr< ULayer >& Layer, const TWeakObjectPtr< AActor >& Actor )
{
	if( !Actor.IsValid() )
	{
		return;
	}

	UClass* ActorClass = Actor->GetClass();

	bool bFoundClassStats = false;
	for (int StatsIndex = 0; StatsIndex < Layer->ActorStats.Num() ; StatsIndex++)
	{
		FLayerActorStats& Stats = Layer->ActorStats[ StatsIndex ];

		if( Stats.Type == ActorClass )
		{
			bFoundClassStats = true;
			--Stats.Total;

			if( Stats.Total == 0 )
			{
				Layer->ActorStats.RemoveAt( StatsIndex );
			}
			break;
		}
	}

	if( bFoundClassStats )
	{
		LayersChanged.Broadcast( ELayersAction::Modify, Layer, TEXT( "ActorStats" ) );
	}
}


bool FLayers::AddActorToLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerName )
{
	TArray< TWeakObjectPtr< AActor > > Actors;
	Actors.Add(Actor);

	TArray< FName > LayerNames;
	LayerNames.Add(LayerName);

	return AddActorsToLayers( Actors, LayerNames );
}

bool FLayers::AddActorToLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames )
{
	TArray< TWeakObjectPtr< AActor > > Actors;
	Actors.Add(Actor);

	return AddActorsToLayers( Actors, LayerNames );
}

bool FLayers::AddActorsToLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName )
{
	TArray< FName > LayerNames;
	LayerNames.Add(LayerName);

	return AddActorsToLayers( Actors, LayerNames );
}

bool FLayers::AddActorsToLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames )
{
	bool bChangesOccurred = false;

	if ( LayerNames.Num() > 0 ) 
	{
		Editor->GetSelectedActors()->BeginBatchSelectOperation();

		for( auto ActorIt = Actors.CreateConstIterator(); ActorIt; ++ActorIt )
		{
			const TWeakObjectPtr< AActor > Actor = *ActorIt;

			if ( !IsActorValidForLayer( Actor ) )
			{
				continue;
			}

			bool bActorWasModified = false;
			for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
			{
				const FName& LayerName = *LayerNameIt;

				if( !Actor->Layers.Contains( LayerName ) )
				{
					if( !bActorWasModified )
					{
						Actor->Modify();
						bActorWasModified = true;
					}

					TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( LayerName );
					Actor->Layers.Add( LayerName );

					Layer->Modify();
					AddActorToStats( Layer, Actor);
				}
			} //END Iteration over Layers

			if( bActorWasModified )
			{
				// update per-view visibility info
				UpdateActorAllViewsVisibility(Actor);

				// update general actor visibility
				bool bActorModified = false;
				bool bActorSelectionChanged = false;
				const bool bActorNotifySelectionChange = true;
				const bool bActorRedrawViewports = false;
				UpdateActorVisibility( Actor, bActorSelectionChanged, bActorModified, bActorNotifySelectionChange, bActorRedrawViewports );

				bChangesOccurred = true;
			}
		} //END Iteration over Actors

		Editor->GetSelectedActors()->EndBatchSelectOperation();
	}

	return bChangesOccurred;
}


bool FLayers::RemoveActorFromLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerName, const bool bUpdateStats )
{
	TArray< TWeakObjectPtr< AActor > > Actors;
	Actors.Add(Actor);

	TArray< FName > LayerNames;
	LayerNames.Add(LayerName);

	return RemoveActorsFromLayers( Actors, LayerNames, bUpdateStats );
}

bool FLayers::RemoveActorFromLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames, const bool bUpdateStats )
{
	TArray< TWeakObjectPtr< AActor > > Actors;
	Actors.Add(Actor);

	return RemoveActorsFromLayers( Actors, LayerNames, bUpdateStats );
}

bool FLayers::RemoveActorsFromLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName, const bool bUpdateStats )
{
	TArray< FName > LayerNames;
	LayerNames.Add(LayerName);

	return RemoveActorsFromLayers( Actors, LayerNames, bUpdateStats );
}

bool FLayers::RemoveActorsFromLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames, const bool bUpdateStats )
{
	Editor->GetSelectedActors()->BeginBatchSelectOperation();

	bool bChangesOccurred = false;
	for( auto ActorIt = Actors.CreateConstIterator(); ActorIt; ++ActorIt )
	{
		const TWeakObjectPtr< AActor > Actor = *ActorIt;

		if ( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		bool ActorWasModified = false;
		for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
		{
			const FName& LayerName = *LayerNameIt;
			if( Actor->Layers.Contains( LayerName ) )
			{
				if( !ActorWasModified )
				{
					Actor->Modify();
					ActorWasModified = true;
				}

				Actor->Layers.Remove( LayerName );

				TWeakObjectPtr< ULayer > Layer;
				if( bUpdateStats && TryGetLayer( LayerName, Layer ))
				{
					Layer->Modify();
					RemoveActorFromStats( Layer, Actor);
				}
			}
		} //END Iteration over Layers

		if( ActorWasModified )
		{
			// update per-view visibility info
			UpdateActorAllViewsVisibility(Actor);

			// update general actor visibility
			bool bActorModified = false;
			bool bActorSelectionChanged = false;
			const bool bActorNotifySelectionChange = true;
			const bool bActorRedrawViewports = false;
			UpdateActorVisibility( Actor, bActorSelectionChanged, bActorModified, bActorNotifySelectionChange, bActorRedrawViewports );

			bChangesOccurred = true;
		}
	} //END Iteration over Actors

	Editor->GetSelectedActors()->EndBatchSelectOperation();

	return bChangesOccurred;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on selected actors.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TArray< TWeakObjectPtr< AActor > > FLayers::GetSelectedActors() const
{
	// Unfortunately, the batch selection operation is not entirely effective
	// and the result can be that the iterator becomes invalid when adding an actor to a layer
	// due to unintended selection change notifications being fired.
	TArray< TWeakObjectPtr< AActor > > CurrentlySelectedActors;
	for( FSelectionIterator It( Editor->GetSelectedActorIterator() ); It; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = static_cast< AActor* >( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		CurrentlySelectedActors.Add( Actor );
	}

	return CurrentlySelectedActors;
}

bool FLayers::AddSelectedActorsToLayer( const FName& LayerName )
{
	return AddActorsToLayer( GetSelectedActors(), LayerName );
}

bool FLayers::RemoveSelectedActorsFromLayer( const FName& LayerName )
{
	return RemoveActorsFromLayer( GetSelectedActors(), LayerName );
}

bool FLayers::AddSelectedActorsToLayers( const TArray< FName >& LayerNames )
{
	return AddActorsToLayers( GetSelectedActors(), LayerNames );
}

bool FLayers::RemoveSelectedActorsFromLayers( const TArray< FName >& LayerNames )
{
	return RemoveActorsFromLayers( GetSelectedActors(), LayerNames );
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on actors in layers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FLayers::SelectActorsInLayers( const TArray< FName >& LayerNames, bool bSelect, bool bNotify, bool bSelectEvenIfHidden, const TSharedPtr< ActorFilter >& Filter )
{
	if ( LayerNames.Num() == 0 )
	{
		return true;
	}

	Editor->GetSelectedActors()->BeginBatchSelectOperation();
	bool bChangesOccurred = false;

	// Iterate over all actors, looking for actors in the specified layers.
	for( const TWeakObjectPtr< AActor > Actor : FActorRange(GetWorld()) )
	{
		if( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		if( Filter.IsValid() && !Filter->PassesFilter( Actor ) )
		{
			continue;
		}

		for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
		{
			if ( Actor->Layers.Contains( *LayerNameIt ) )
			{
				// The actor was found to be in a specified layer.
				// Set selection state and move on to the next actor.
				bool bNotifyForActor = false;
				Editor->GetSelectedActors()->Modify();
				Editor->SelectActor( Actor.Get(), bSelect, bNotifyForActor, bSelectEvenIfHidden );
				bChangesOccurred = true;
				break;
			}
		}
	}

	Editor->GetSelectedActors()->EndBatchSelectOperation();

	if( bNotify )
	{
		Editor->NoteSelectionChange();
	}

	return bChangesOccurred;
}


bool FLayers::SelectActorsInLayer( const FName& LayerName, bool bSelect, bool bNotify, bool bSelectEvenIfHidden, const TSharedPtr< ActorFilter >& Filter )
{
	Editor->GetSelectedActors()->BeginBatchSelectOperation();
	bool bChangesOccurred = false;
	// Iterate over all actors, looking for actors in the specified layers.
	for( FActorIterator It(GetWorld()); It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;
		if( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		if( Filter.IsValid() && !Filter->PassesFilter( Actor ) )
		{
			continue;
		}

		if ( Actor->Layers.Contains( LayerName ) )
		{
			// The actor was found to be in a specified layer.
			// Set selection state and move on to the next actor.
			bool bNotifyForActor = false;
			Editor->GetSelectedActors()->Modify();
			Editor->SelectActor( Actor.Get(), bSelect, bNotifyForActor, bSelectEvenIfHidden );
			bChangesOccurred = true;
		}
	}

	Editor->GetSelectedActors()->EndBatchSelectOperation();

	if( bNotify )
	{
		Editor->NoteSelectionChange();
	}

	return bChangesOccurred;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on actor viewport visibility regarding layers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FLayers::UpdatePerViewVisibility( FLevelEditorViewportClient* ViewportClient, const FName& LayerThatChanged )
{
	const int32 ViewIndex = ViewportClient->ViewIndex;
	// get the viewport client
	// Iterate over all actors, looking for actors in the specified layers.
	if( ViewportClient->GetWorld() == NULL )
	{
		return;
	}
	for( FActorIterator It(ViewportClient->GetWorld()) ; It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;
		if( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		// if the view has nothing hidden, just quickly mark the actor as visible in this view 
		if ( ViewportClient->ViewHiddenLayers.Num() == 0)
		{
			// if the actor had this view hidden, then unhide it
			if ( Actor->HiddenEditorViews & ( (uint64)1 << ViewIndex ) )
			{
				// make sure this actor doesn't have the view set
				Actor->HiddenEditorViews &= ~( (uint64)1 << ViewIndex );
				Actor->MarkComponentsRenderStateDirty();
			}
		}
		// else if we were given a name that was changed, only update actors with that name in their layers,
		// otherwise update all actors
		else if ( LayerThatChanged == NAME_Skip || Actor->Layers.Contains( LayerThatChanged ) )
		{
			UpdateActorViewVisibility(ViewportClient, Actor);
		}
	}

	// make sure we redraw the viewport
	ViewportClient->Invalidate();
}


void FLayers::UpdateAllViewVisibility( const FName& LayerThatChanged )
{
	// update all views's hidden layers if they had this one
	for ( int32 ViewIndex = 0; ViewIndex < Editor->LevelViewportClients.Num(); ++ViewIndex )
	{
		UpdatePerViewVisibility( Editor->LevelViewportClients[ViewIndex], LayerThatChanged );
	}
}


void FLayers::UpdateActorViewVisibility( FLevelEditorViewportClient* ViewportClient, const TWeakObjectPtr< AActor >& Actor, bool bReregisterIfDirty )
{
	// get the viewport client
	const int32 ViewIndex = ViewportClient->ViewIndex;

	int32 NumHiddenLayers = 0;
	// look for which of the actor layers are hidden
	for (int32 LayerIndex = 0; LayerIndex < Actor->Layers.Num(); LayerIndex++)
	{
		// if its in the view hidden list, this layer is hidden for this actor
		if (ViewportClient->ViewHiddenLayers.Find( Actor->Layers[ LayerIndex ] ) != -1)
		{
			NumHiddenLayers++;
			// right now, if one is hidden, the actor is hidden
			break;
		}
	}

	uint64 OriginalHiddenViews = Actor->HiddenEditorViews;

	// right now, if one is hidden, the actor is hidden
	if (NumHiddenLayers)
	{
		Actor->HiddenEditorViews |= ((uint64)1 << ViewIndex);
	}
	else
	{
		Actor->HiddenEditorViews &= ~((uint64)1 << ViewIndex);
	}

	// reregister if we changed the visibility bits, as the rendering thread needs them
	if (bReregisterIfDirty && OriginalHiddenViews != Actor->HiddenEditorViews)
	{
		Actor->MarkComponentsRenderStateDirty();

		// make sure we redraw the viewport
		ViewportClient->Invalidate();
	}
}


void FLayers::UpdateActorAllViewsVisibility( const TWeakObjectPtr< AActor >& Actor )
{
	uint64 OriginalHiddenViews = Actor->HiddenEditorViews;

	for ( int32 ViewIndex = 0; ViewIndex < Editor->LevelViewportClients.Num(); ++ViewIndex )
	{
		// don't have this reattach, as we can do it once for all views
		UpdateActorViewVisibility(Editor->LevelViewportClients[ViewIndex], Actor, false);
	}

	// reregister if we changed the visibility bits, as the rendering thread needs them
	if (OriginalHiddenViews != Actor->HiddenEditorViews)
	{
		return;
	}

	Actor->MarkComponentsRenderStateDirty();

	// redraw all viewports if the actor
	for (int32 ViewIndex = 0; ViewIndex < Editor->LevelViewportClients.Num(); ViewIndex++)
	{
		// make sure we redraw all viewports
		Editor->LevelViewportClients[ViewIndex]->Invalidate();
	}
}


void FLayers::RemoveViewFromActorViewVisibility( FLevelEditorViewportClient* ViewportClient )
{
	const int32 ViewIndex = ViewportClient->ViewIndex;

	// get the bit for the view index
	uint64 ViewBit = ((uint64)1 << ViewIndex);
	// get all bits under that that we want to keep
	uint64 KeepBits = ViewBit - 1;

	// Iterate over all actors, looking for actors in the specified layers.
	for( FActorIterator It(ViewportClient->GetWorld()) ; It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;

		if( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		// remember original bits
		uint64 OriginalHiddenViews = Actor->HiddenEditorViews;

		uint64 Was = Actor->HiddenEditorViews;

		// slide all bits higher than ViewIndex down one since the view is being removed from Editor
		uint64 LowBits = Actor->HiddenEditorViews & KeepBits;

		// now slide the top bits down by ViewIndex + 1 (chopping off ViewBit)
		uint64 HighBits = Actor->HiddenEditorViews >> (ViewIndex + 1);
		// then slide back up by ViewIndex, which will now have erased ViewBit, as well as leaving 0 in the low bits
		HighBits = HighBits << ViewIndex;

		// put it all back together
		Actor->HiddenEditorViews = LowBits | HighBits;

		// reregister if we changed the visibility bits, as the rendering thread needs them
		if (OriginalHiddenViews == Actor->HiddenEditorViews)
		{
			continue;
		}

		// Find all registered primitive components and update the scene proxy with the actors updated visibility map
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);

		for( int32 ComponentIdx = 0; ComponentIdx < Components.Num(); ++ComponentIdx )
		{
			UPrimitiveComponent* PrimitiveComponent = Components[ComponentIdx];
			if (PrimitiveComponent->IsRegistered())
			{
				// Push visibility to the render thread
				PrimitiveComponent->PushEditorVisibilityToProxy( Actor->HiddenEditorViews );
			}
		}
	}
}

static void UpdateBrushLayerVisibility(ABrush* Brush, bool bIsHidden)
{
	ULevel* Level = Brush->GetLevel();
	if (!Level)
	{
		return;
	}

	UModel* Model = Level->Model;
	if (!Model)
	{
		return;
	}

	bool bAnySurfaceWasFound = false;
	for (FBspSurf& Surf : Model->Surfs)
	{
		if (Surf.Actor == Brush)
		{
			Surf.bHiddenEdLayer = bIsHidden;
			bAnySurfaceWasFound = true;
		}
	}

	if (bAnySurfaceWasFound)
	{
		Level->UpdateModelComponents();
		Model->InvalidSurfaces = true;
	}
}

bool FLayers::UpdateActorVisibility( const TWeakObjectPtr< AActor >& Actor, bool& bOutSelectionChanged, bool& bOutActorModified, bool bNotifySelectionChange, bool bRedrawViewports )
{
	bOutActorModified = false;
	bOutSelectionChanged = false;

	if(	!IsActorValidForLayer( Actor ) )
	{
		return false;
	}

	// If the actor doesn't belong to any layers
	if( Actor->Layers.Num() == 0)
	{
		// If the actor is also hidden
		if( Actor->bHiddenEdLayer )
		{
			Actor->Modify();

			// Actors that don't belong to any layer shouldn't be hidden
			Actor->bHiddenEdLayer = false;
			Actor->MarkComponentsRenderStateDirty();
			bOutActorModified = true;
		}

		return bOutActorModified;
	}

	bool bActorBelongsToVisibleLayer = false;
	for( int32 LayerIndex = 0 ; LayerIndex < GetWorld()->Layers.Num() ; ++LayerIndex )
	{
		const TWeakObjectPtr< ULayer > Layer =  GetWorld()->Layers[ LayerIndex ];

		if( !Layer->bIsVisible )
		{
			continue;
		}

		if( Actor->Layers.Contains( Layer->LayerName ) )
		{
			if ( Actor->bHiddenEdLayer )
			{
				Actor->Modify();
				Actor->bHiddenEdLayer = false;
				Actor->MarkComponentsRenderStateDirty();
				bOutActorModified = true;

				if (ABrush* Brush = Cast<ABrush>(Actor.Get()))
				{
					const bool bIsHidden = false;
					UpdateBrushLayerVisibility(Brush, bIsHidden);
				}
			}

			// Stop, because we found at least one visible layer the actor belongs to
			bActorBelongsToVisibleLayer = true;
			break;
		}
	}

	// If the actor isn't part of a visible layer, hide and de-select it.
	if( !bActorBelongsToVisibleLayer )
	{
		if ( !Actor->bHiddenEdLayer )
		{
			Actor->Modify();
			Actor->bHiddenEdLayer = true;
			Actor->MarkComponentsRenderStateDirty();
			bOutActorModified = true;

			if (ABrush* Brush = Cast<ABrush>(Actor.Get()))
			{
				const bool bIsHidden = true;
				UpdateBrushLayerVisibility(Brush, bIsHidden);
			}
		}

		//if the actor was selected, mark it as unselected
		if ( Actor->IsSelected() )
		{
			bool bSelect = false;
			bool bNotify = false;
			bool bIncludeHidden = true;
			Editor->SelectActor( Actor.Get(), bSelect, bNotify, bIncludeHidden );

			bOutSelectionChanged = true;
			bOutActorModified = true;
		}
	}

	if ( bNotifySelectionChange && bOutSelectionChanged )
	{
		Editor->NoteSelectionChange();
	}

	if( bRedrawViewports )
	{
		Editor->RedrawLevelEditingViewports();
	}

	return bOutActorModified || bOutSelectionChanged;
}


bool FLayers::UpdateAllActorsVisibility( bool bNotifySelectionChange, bool bRedrawViewports )
{
	bool bSelectionChanged = false;
	bool bChangesOccurred = false;
	for( FActorIterator It(GetWorld()) ; It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;

		bool bActorModified = false;
		bool bActorSelectionChanged = false;
		const bool bActorNotifySelectionChange = false;
		const bool bActorRedrawViewports = false;

		bChangesOccurred |= UpdateActorVisibility( Actor, bActorSelectionChanged /*OUT*/, bActorModified /*OUT*/, bActorNotifySelectionChange, bActorRedrawViewports );
		bSelectionChanged |= bActorSelectionChanged;
	}

	if ( bNotifySelectionChange && bSelectionChanged )
	{
		Editor->NoteSelectionChange();
	}

	if( bRedrawViewports )
	{
		Editor->RedrawLevelEditingViewports();
	}

	return bChangesOccurred;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Operations on layers
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FLayers::AppendActorsForLayer( const FName& LayerName, TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedPtr< ActorFilter >& Filter ) const
{
	for( FActorIterator ActorIt(GetWorld()); ActorIt; ++ActorIt ) 
	{
		const TWeakObjectPtr< AActor > Actor = *ActorIt;

		if( Filter.IsValid() && !Filter->PassesFilter( Actor ))
		{
			continue;
		}

		if( Actor->Layers.Contains( LayerName ) )
		{
			InActors.Add( Actor );
		}
	}
}


void FLayers::AppendActorsForLayers( const TArray< FName >& LayerNames, TArray< TWeakObjectPtr< AActor > >& OutActors, const TSharedPtr< ActorFilter >& Filter ) const
{
	for( FActorIterator ActorIt(GetWorld()); ActorIt; ++ActorIt )
	{
		const TWeakObjectPtr< AActor > Actor = *ActorIt;

		if( Filter.IsValid() && !Filter->PassesFilter( Actor ))
		{
			continue;
		}

		for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
		{
			const FName& LayerName = *LayerNameIt;

			if( Actor->Layers.Contains( LayerName ) )
			{
				OutActors.Add( Actor );
				break;
			}
		}
	}
}


void FLayers::SetLayerVisibility( const FName& LayerName, bool bIsVisible )
{
	const TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( LayerName );
	check( Layer != NULL );
	
	Layer->Modify();
	Layer->bIsVisible = bIsVisible;
	LayersChanged.Broadcast( ELayersAction::Modify, Layer, "bIsVisible" );

	UpdateAllActorsVisibility( true, true );
}


void FLayers::SetLayersVisibility( const TArray< FName >& LayerNames, bool bIsVisible )
{
	if( LayerNames.Num() == 0 )
	{
		return;
	}

	bool bChangeOccurred = false;
	for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
	{
		const TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( *LayerNameIt );
		check( Layer != NULL );

		if( Layer->bIsVisible != bIsVisible )
		{
			Layer->Modify();
			Layer->bIsVisible = bIsVisible;
			LayersChanged.Broadcast( ELayersAction::Modify, Layer, "bIsVisible" );
			bChangeOccurred = true;
		}
	}

	if( bChangeOccurred )
	{
		UpdateAllActorsVisibility( true, true );
	}
}


void FLayers::ToggleLayerVisibility( const FName& LayerName )
{
	const TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( LayerName );
	check( Layer != NULL );

	Layer->Modify();
	Layer->bIsVisible = !Layer->bIsVisible;

	LayersChanged.Broadcast( ELayersAction::Modify, Layer, "bIsVisible" );
	UpdateAllActorsVisibility( true, true );
}


void FLayers::ToggleLayersVisibility( const TArray< FName >& LayerNames )
{
	if( LayerNames.Num() == 0 )
	{
		return;
	}

	for( auto LayerNameIt = LayerNames.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
	{
		const TWeakObjectPtr< ULayer > Layer = EnsureLayerExists( *LayerNameIt );
		check( Layer != NULL );

		Layer->Modify();
		Layer->bIsVisible = !Layer->bIsVisible;
		LayersChanged.Broadcast( ELayersAction::Modify, Layer, "bIsVisible" );
	}

	UpdateAllActorsVisibility( true, true );
}


void FLayers::MakeAllLayersVisible()
{
	TArray< FName > AllLayerNames;
	FLayers::AddAllLayerNamesTo( AllLayerNames );
	for( auto LayerIt = GetWorld()->Layers.CreateIterator(); LayerIt; ++LayerIt )
	{
		TWeakObjectPtr< ULayer > Layer = *LayerIt;

		if( !Layer->bIsVisible )
		{
			Layer->Modify();
			Layer->bIsVisible = true;
			LayersChanged.Broadcast( ELayersAction::Modify, Layer, "bIsVisible" );
		}
	}

	UpdateAllActorsVisibility( true, true );
}


TWeakObjectPtr< ULayer > FLayers::GetLayer( const FName& LayerName ) const
{
	for( auto LayerIt = GetWorld()->Layers.CreateConstIterator(); LayerIt; ++LayerIt )
	{
		TWeakObjectPtr< ULayer > Layer = *LayerIt;
		if( Layer->LayerName == LayerName )
		{
			return Layer;
		}
	}

	return NULL;
}


bool FLayers::TryGetLayer( const FName& LayerName, TWeakObjectPtr< ULayer >& OutLayer )
{
	OutLayer = GetLayer( LayerName );
	return OutLayer != NULL;
}


void FLayers::AddAllLayerNamesTo( TArray< FName >& OutLayers ) const
{
	for( auto LayerIt = GetWorld()->Layers.CreateConstIterator(); LayerIt; ++LayerIt )
	{
		TWeakObjectPtr< ULayer > Layer = *LayerIt;
		OutLayers.Add( Layer->LayerName );
	}
}


void FLayers::AddAllLayersTo( TArray< TWeakObjectPtr< ULayer > >& OutLayers ) const
{
	for( auto LayerIt = GetWorld()->Layers.CreateConstIterator(); LayerIt; ++LayerIt )
	{
		OutLayers.Add( *LayerIt );
	}
}


TWeakObjectPtr< ULayer > FLayers::CreateLayer( const FName& LayerName )
{
	ULayer* NewLayer = NewObject<ULayer>(GetWorld(), NAME_None, RF_Transactional);
	check( NewLayer != NULL );

	GetWorld()->Modify();
	GetWorld()->Layers.Add( NewLayer );

	NewLayer->LayerName = LayerName;
	NewLayer->bIsVisible = true;

	LayersChanged.Broadcast( ELayersAction::Add, NewLayer, NAME_None );

	return NewLayer;
}


void FLayers::DeleteLayers( const TArray< FName >& LayersToDelete )
{
	TArray< FName > ValidLayersToDelete;
	for( auto LayerNameIt = LayersToDelete.CreateConstIterator(); LayerNameIt; ++LayerNameIt )
	{
		TWeakObjectPtr< ULayer > Layer;
		if( TryGetLayer( *LayerNameIt, OUT Layer ) )
		{
			ValidLayersToDelete.Add( *LayerNameIt );
		}
	}

	// Iterate over all actors, looking for actors in the specified layers.
	for( FActorIterator It(GetWorld()); It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;

		//The Layer must exist in order to remove actors from it,
		//so we have to wait to delete the ULayer object till after
		//all the actors have been disassociated with it.
		RemoveActorFromLayers( Actor, ValidLayersToDelete, false );
	}

	bool bValidLayerExisted = false;
	for (int LayerIndex = GetWorld()->Layers.Num() - 1; LayerIndex >= 0 ; LayerIndex--)
	{
		if( LayersToDelete.Contains( GetWorld()->Layers[ LayerIndex]->LayerName ) )
		{
			GetWorld()->Modify();
			GetWorld()->Layers.RemoveAt( LayerIndex );
			bValidLayerExisted = true;
		}
	}

	LayersChanged.Broadcast( ELayersAction::Delete, NULL, NAME_None );
}


void FLayers::DeleteLayer( const FName& LayerToDelete )
{
	TWeakObjectPtr< ULayer > Layer;
	if( !TryGetLayer( LayerToDelete, Layer ) )
	{
		return;
	}
	// Iterate over all actors, looking for actors in the specified layer.
	for( FActorIterator It(GetWorld()) ; It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;

		//The Layer must exist in order to remove actors from it,
		//so we have to wait to delete the ULayer object till after
		//all the actors have been disassociated with it.
		RemoveActorFromLayer( Actor, LayerToDelete, false );
	}

	bool bValidLayerExisted = false;
	for (int LayerIndex = GetWorld()->Layers.Num() - 1; LayerIndex >= 0 ; LayerIndex--)
	{
		if( LayerToDelete == GetWorld()->Layers[ LayerIndex]->LayerName )
		{
			GetWorld()->Modify();
			GetWorld()->Layers.RemoveAt( LayerIndex );
			bValidLayerExisted = true;
		}
	}

	LayersChanged.Broadcast( ELayersAction::Delete, NULL, NAME_None );
}


bool FLayers::RenameLayer( const FName OriginalLayerName, const FName& NewLayerName )
{
	// We specifically don't pass the original LayerName by reference to avoid it changing
	// it's original value, in case, it would be the reference of the Layer's actually FName
	if ( OriginalLayerName == NewLayerName )
	{
		return false;
	}

	TWeakObjectPtr< ULayer > Layer;
	if( !TryGetLayer( OriginalLayerName, Layer ) )
	{
		return false;
	}

	Layer->Modify();
	Layer->LayerName = NewLayerName;
	Layer->ActorStats.Empty();
	// Iterate over all actors, swapping layers.
	for( FActorIterator It(GetWorld()) ; It ; ++It )
	{
		const TWeakObjectPtr< AActor > Actor = *It;
		if( !IsActorValidForLayer( Actor ) )
		{
			continue;
		}

		if ( FLayers::RemoveActorFromLayer( Actor, OriginalLayerName ) )
		{
			// No need to mark the actor as modified these functions take care of that
			AddActorToLayer( Actor, NewLayerName );
		}
	}

	// update all views's hidden layers if they had this one
	for ( int32 ViewIndex = 0; ViewIndex < Editor->LevelViewportClients.Num(); ViewIndex++ )
	{
		FLevelEditorViewportClient* ViewportClient = Editor->LevelViewportClients[ ViewIndex ];
		if ( ViewportClient->ViewHiddenLayers.Remove( OriginalLayerName ) > 0 )
		{
			ViewportClient->ViewHiddenLayers.AddUnique( NewLayerName );
			ViewportClient->Invalidate();
		}
	}

	LayersChanged.Broadcast( ELayersAction::Rename, Layer, "LayerName" );

	return true;
} 
