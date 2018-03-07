// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ActorLayerViewModel.h"
#include "Editor/EditorEngine.h"
#include "Layers/Layer.h"

#define LOCTEXT_NAMESPACE "Layer"


FActorLayerViewModel::FActorLayerViewModel( const TWeakObjectPtr< ULayer >& InLayer, const TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedRef< ILayers >& InWorldLayers, const TWeakObjectPtr< UEditorEngine >& InEditor )
	: WorldLayers( InWorldLayers )
	, Editor( InEditor )
	, Layer( InLayer )
{
	Actors.Append( InActors );
}


void FActorLayerViewModel::Initialize()
{
	WorldLayers->OnLayersChanged().AddSP( this, &FActorLayerViewModel::OnLayersChanged );

	if ( Editor.IsValid() )
	{
		Editor->RegisterForUndo(this);
	}
}


FActorLayerViewModel::~FActorLayerViewModel()
{
	WorldLayers->OnLayersChanged().RemoveAll( this );

	if ( Editor.IsValid() )
	{
		Editor->UnregisterForUndo(this);
	}
}


FName FActorLayerViewModel::GetFName() const
{
	if( !Layer.IsValid() )
	{
		return NAME_None;
	}

	return Layer->LayerName;
}


FText FActorLayerViewModel::GetName() const
{
	if( !Layer.IsValid() )
	{
		return LOCTEXT("Invalid layer Name", "");
	}

	return FText::FromName(Layer->LayerName);
}


bool FActorLayerViewModel::IsVisible() const
{
	if( !Layer.IsValid() )
	{
		return false;
	}

	return Layer->bIsVisible;
}


void FActorLayerViewModel::OnLayersChanged( const ELayersAction::Type Action, const TWeakObjectPtr< ULayer >& ChangedLayer, const FName& ChangedProperty )
{
	if( Action != ELayersAction::Modify && Action != ELayersAction::Reset )
	{
		return;
	}

	if( ChangedLayer.IsValid() && ChangedLayer != Layer )
	{
		return;
	}

	Changed.Broadcast();
}


void FActorLayerViewModel::Refresh()
{
	OnLayersChanged( ELayersAction::Reset, NULL, NAME_None );
}


#undef LOCTEXT_NAMESPACE
