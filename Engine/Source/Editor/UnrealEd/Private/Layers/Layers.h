// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Layers/ILayers.h"

class FLevelEditorViewportClient;
class UEditorEngine;
class ULayer;

class FLayers : public ILayers
{

public:

	/**
	 *	Creates a new FLayers
	 */
	static TSharedRef< FLayers > Create( const TWeakObjectPtr< UEditorEngine >& InEditor )
	{
		TSharedRef< FLayers > Layers = MakeShareable( new FLayers( InEditor ) );
		Layers->Initialize();
		return Layers;
	}

	/**
	 *	Destructor
	 */
	~FLayers();

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//	Begin ILayer Implementation -- See ILayers for documentation

	DECLARE_DERIVED_EVENT( FLayers, ILayers::FOnLayersChanged, FOnLayersChanged );
	virtual FOnLayersChanged& OnLayersChanged() override { return LayersChanged; }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Operations on Levels

	virtual void AddLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level ) override;
	virtual void RemoveLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level ) override;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Operations on an individual actor.

	virtual bool IsActorValidForLayer( const TWeakObjectPtr< AActor >& Actor ) override;
	
	virtual bool InitializeNewActorLayers( const TWeakObjectPtr< AActor >& Actor ) override;
	virtual bool DisassociateActorFromLayers( const TWeakObjectPtr< AActor >& Actor ) override;

	virtual bool AddActorToLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerName ) override;
	virtual bool AddActorToLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames ) override;
	
	virtual bool RemoveActorFromLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerToRemove, const bool bUpdateStats = true ) override;
	virtual bool RemoveActorFromLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames, const bool bUpdateStats = true  ) override;


	/////////////////////////////////////////////////
	// Operations on multiple actors.
	
	virtual bool AddActorsToLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName ) override;
	virtual bool AddActorsToLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames ) override;
	
	virtual bool RemoveActorsFromLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName, const bool bUpdateStats = true ) override;
	virtual bool RemoveActorsFromLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames, const bool bUpdateStats = true ) override;


	/////////////////////////////////////////////////
	// Operations on selected actors.
	TArray< TWeakObjectPtr< AActor > > GetSelectedActors() const;

	virtual bool AddSelectedActorsToLayer( const FName& LayerName ) override;
	virtual bool AddSelectedActorsToLayers( const TArray< FName >& LayerNames ) override;
	
	virtual bool RemoveSelectedActorsFromLayer( const FName& LayerName ) override;
	virtual bool RemoveSelectedActorsFromLayers( const TArray< FName >& LayerNames ) override;


	/////////////////////////////////////////////////
	// Operations on actors in layers

	virtual bool SelectActorsInLayers( const TArray< FName >& LayerNames, bool bSelect, bool bNotify, bool bSelectEvenIfHidden = false, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL ) ) override;
	virtual bool SelectActorsInLayer( const FName& LayerName, bool bSelect, bool bNotify, bool bSelectEvenIfHidden = false, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL ) ) override;


	/////////////////////////////////////////////////
	// Operations on actor viewport visibility regarding layers

	virtual void UpdateAllViewVisibility( const FName& LayerThatChanged ) override;
	virtual void UpdatePerViewVisibility( FLevelEditorViewportClient* ViewportClient, const FName& LayerThatChanged=NAME_Skip ) override;
	
	virtual void UpdateActorViewVisibility( FLevelEditorViewportClient* ViewportClient, const TWeakObjectPtr< AActor >& Actor, bool bReregisterIfDirty=true ) override;
	virtual void UpdateActorAllViewsVisibility( const TWeakObjectPtr< AActor >& Actor ) override;
	
	virtual void RemoveViewFromActorViewVisibility( FLevelEditorViewportClient* ViewportClient ) override;
	
	virtual bool UpdateActorVisibility( const TWeakObjectPtr< AActor >& Actor, bool& bOutSelectionChanged, bool& bOutActorModified, bool bNotifySelectionChange, bool bRedrawViewports ) override;
	virtual bool UpdateAllActorsVisibility( bool bNotifySelectionChange, bool bRedrawViewports ) override;


	/////////////////////////////////////////////////
	// Operations on layers

	virtual void AppendActorsForLayer( const FName& LayerName, TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL )  ) const override;
	virtual void AppendActorsForLayers( const TArray< FName >& LayerNames, TArray< TWeakObjectPtr< AActor > >& OutActors, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL )  ) const override;

	virtual void SetLayerVisibility( const FName& LayerName, bool bIsVisible ) override;
	virtual void SetLayersVisibility( const TArray< FName >& LayerNames, bool bIsVisible ) override;

	virtual void ToggleLayerVisibility( const FName& LayerName ) override;
	virtual void ToggleLayersVisibility( const TArray< FName >& LayerNames ) override;

	virtual void MakeAllLayersVisible() override;

	virtual TWeakObjectPtr< ULayer > GetLayer( const FName& LayerName ) const override;
	virtual bool TryGetLayer( const FName& LayerName, TWeakObjectPtr< ULayer >& OutLayer ) override;

	virtual void AddAllLayerNamesTo( TArray< FName >& OutLayerNames ) const override;
	virtual void AddAllLayersTo( TArray< TWeakObjectPtr< ULayer > >& OutLayers ) const override;

	virtual TWeakObjectPtr< ULayer > CreateLayer( const FName& LayerName ) override;

	virtual void DeleteLayers( const TArray< FName >& LayersToDelete ) override;
	virtual void DeleteLayer( const FName& LayerToDelete ) override;

	virtual bool RenameLayer( const FName OriginalLayerName, const FName& NewLayerName ) override;


	UWorld* GetWorld() const { return GWorld;} // Fallback to GWorld
protected:

	void AddActorToStats( const TWeakObjectPtr< ULayer >& Layer, const TWeakObjectPtr< AActor >& Actor );
	void RemoveActorFromStats( const TWeakObjectPtr< ULayer >& Layer, const TWeakObjectPtr< AActor >& Actor );

	//	End ILayer Implementation
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:

	/** 
	 * Delegate handlers
	 **/ 
	void OnEditorMapChange(uint32 MapChangeFlags);

	/**
	 *	FLayers Constructor
	 *
	 *	@param	InEditor	The UEditorEngine to affect
	 */
	FLayers( const TWeakObjectPtr< UEditorEngine >& InEditor );

	/**
	 *	Prepares for use
	 */
	void Initialize();

	/** 
	 *	Checks to see if the named layer exists, and if it doesn't creates it.
	 *
	 * @param	LayerName	A valid layer name
	 * @return				The ULayer Object of the named layer
	 */
	TWeakObjectPtr< ULayer > EnsureLayerExists( const FName& LayerName );


private:

	/** The associated UEditorEngine */
	TWeakObjectPtr< UEditorEngine > Editor;

	/**	Fires whenever one or more layer changes */
	FOnLayersChanged LayersChanged;
};
