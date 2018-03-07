// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class AActor;
class ULayer;
class ULevel;
template< typename TItemType > class IFilter;

namespace ELayersAction
{
	enum Type
	{
		/**	The specified ChangedLayer is a newly created ULayer, if ChangedLayer is invalid then multiple Layers were added */	
		Add,

		/**	
		 *	The specified ChangedLayer was just modified, if ChangedLayer is invalid then multiple Layers were modified. 
		 *  ChangedProperty specifies what field on the ULayer was changed, if NAME_None then multiple fields were changed 
		 */
		Modify,

		/**	A ULayer was deleted */
		Delete,

		/**	The specified ChangedLayer was just renamed */
		Rename,

		/**	A large amount of changes have occurred to a number of Layers. A full rebind will be required. */
		Reset,
	};
}

class ILayers 
{

public:
	typedef IFilter< const TWeakObjectPtr< AActor >& > ActorFilter;

	virtual ~ILayers() {}

	/** Broadcasts whenever one or more Layers are modified*/
	DECLARE_EVENT_ThreeParams( ILayers, FOnLayersChanged, const ELayersAction::Type /*Action*/, const TWeakObjectPtr< ULayer >& /*ChangedLayer*/, const FName& /*ChangedProperty*/);
	virtual FOnLayersChanged& OnLayersChanged() = 0;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Operations on Levels

	/**
	 *	Aggregates any information regarding layers associated with the level and it contents
	 *
	 *	@param	Level	The process
	 */
	virtual void AddLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level ) = 0;

	/**
	 *	Purges any information regarding layers associated with the level and it contents
	 *
	 *	@param	Level	The process
	 */
	virtual void RemoveLevelLayerInformation( const TWeakObjectPtr< ULevel >& Level ) = 0;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Operations on an individual actor.

	/**
	 *	Checks to see if the specified actor is in an appropriate state to interact with layers
	 *
	 *	@param	Actor	The actor to validate
	 */
	virtual bool IsActorValidForLayer( const TWeakObjectPtr< AActor >& Actor ) = 0;

	/**
	 *	Synchronizes an newly created Actor's layers with the layer system
	 *
	 *	@param	Actor	The actor to initialize
	 */
	virtual bool InitializeNewActorLayers( const TWeakObjectPtr< AActor >& Actor ) = 0;

	/**
	 *	Disassociates an Actor's layers from the layer system, general used before deleting the Actor
	 *
	 *	@param	Actor	The actor to disassociate from the layer system
	 */
	virtual bool DisassociateActorFromLayers( const TWeakObjectPtr< AActor >& Actor ) = 0;

	/**
	 * Adds the actor to the named layer.
	 *
	 * @param	Actor		The actor to add to the named layer
	 * @param	LayerName	The name of the layer to add the actor to
	 * @return				true if the actor was added.  false is returned if the actor already belongs to the layer.
	 */
	virtual bool AddActorToLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerName ) = 0;

	/**
	 * Adds the provided actor to the named layers.
	 *
	 * @param	Actor		The actor to add to the provided layers
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if the actor was added to at least one of the provided layers.
	 */
	virtual bool AddActorToLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames ) = 0;

	/**
	 * Removes an actor from the specified layer.
	 *
	 * @param	Actor			The actor to remove from the provided layer
	 * @param	LayerToRemove	The name of the layer to remove the actor from
	 * @return					true if the actor was removed from the layer.  false is returned if the actor already belonged to the layer.
	 */
	virtual bool RemoveActorFromLayer( const TWeakObjectPtr< AActor >& Actor, const FName& LayerToRemove, const bool bUpdateStats = true ) = 0;
	
	/**
	 * Removes the provided actor from the named layers.
	 *
	 * @param	Actor		The actor to remove from the provided layers
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if the actor was removed from at least one of the provided layers.
	 */
	virtual bool RemoveActorFromLayers( const TWeakObjectPtr< AActor >& Actor, const TArray< FName >& LayerNames, const bool bUpdateStats = true ) = 0;


	/////////////////////////////////////////////////
	// Operations on multiple actors.
	
	/**
	 * Add the actors to the named layer
	 *
	 * @param	Actors		The actors to add to the named layer
	 * @param	LayerName	The name of the layer to add to
	 * @return				true if at least one actor was added to the layer.  false is returned if all the actors already belonged to the layer.
	 */
	virtual bool AddActorsToLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName ) = 0;

	/**
	 * Add the actors to the named layers
	 *
	 * @param	Actors		The actors to add to the named layers
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if at least one actor was added to at least one layer.  false is returned if all the actors already belonged to all specified layers.
	 */
	virtual bool AddActorsToLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames ) = 0;

	/**
	 * Removes the actors from the specified layer.
	 *
	 * @param	Actors			The actors to remove from the provided layer
	 * @param	LayerToRemove	The name of the layer to remove the actors from
	 * @return					true if at least one actor was removed from the layer.  false is returned if all the actors already belonged to the layer.
	 */
	virtual bool RemoveActorsFromLayer( const TArray< TWeakObjectPtr< AActor > >& Actors, const FName& LayerName, const bool bUpdateStats = true ) = 0;

	/**
	 * Remove the actors to the named layers
	 *
	 * @param	Actors		The actors to remove to the named layers
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if at least one actor was removed from at least one layer.  false is returned if none of the actors belonged to any of the specified layers.
	 */
	virtual bool RemoveActorsFromLayers( const TArray< TWeakObjectPtr< AActor > >& Actors, const TArray< FName >& LayerNames, const bool bUpdateStats = true ) = 0;


	/////////////////////////////////////////////////
	// Operations on selected actors.

	/**
	 * Adds selected actors to the named layer.
	 *
	 * @param	LayerName	A layer name.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the named layer.
	 */
	virtual bool AddSelectedActorsToLayer( const FName& LayerName ) = 0;

	/**
	 * Adds selected actors to the named layers.
	 *
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the named layers.
	 */
	virtual bool AddSelectedActorsToLayers( const TArray< FName >& LayerNames ) = 0;

	/**
	 * Removes the selected actors from the named layer.
	 *
	 * @param	LayerName	A layer name.
	 * @return				true if at least one actor was added.  false is returned if all selected actors already belong to the named layer.
	 */
	virtual bool RemoveSelectedActorsFromLayer( const FName& LayerName ) = 0;

	/**
	 * Removes selected actors from the named layers.
	 *
	 * @param	LayerNames	A valid list of layer names.
	 * @return				true if at least one actor was removed.
	 */
	virtual bool RemoveSelectedActorsFromLayers( const TArray< FName >& LayerNames ) = 0;


	/////////////////////////////////////////////////
	// Operations on actors in layers

	/**
	 * Selects/de-selects actors belonging to the named layers.
	 *
	 * @param	LayerNames						A valid list of layer names.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected
	 * @param	Filter	[optional]				Actor that don't pass the specified filter restrictions won't be selected
	 * @return									true if at least one actor was selected/deselected.
	 */
	virtual bool SelectActorsInLayers( const TArray< FName >& LayerNames, bool bSelect, bool bNotify, bool bSelectEvenIfHidden = false, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL ) ) = 0;

	/**
	 * Selects/de-selects actors belonging to the named layer.
	 *
	 * @param	LayerName						A valid layer name.
	 * @param	bSelect							If true actors are selected; if false, actors are deselected.
	 * @param	bNotify							If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bSelectEvenIfHidden	[optional]	If true even hidden actors will be selected; if false, hidden actors won't be selected
	 * @param	Filter	[optional]				Actor that don't pass the specified filter restrictions won't be selected
	 * @return									true if at least one actor was selected/deselected.
	 */
	virtual bool SelectActorsInLayer( const FName& LayerName, bool bSelect, bool bNotify, bool bSelectEvenIfHidden = false, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL ) ) = 0;


	/////////////////////////////////////////////////
	// Operations on actor viewport visibility regarding layers

	/**
	 * Updates the visibility for all actors for all views
	 * 
	 * @param LayerThatChanged  If one layer was changed (toggled in view pop-up, etc), then we only need to modify actors that use that layer
	 */
	virtual void UpdateAllViewVisibility( const FName& LayerThatChanged ) = 0;

	/**
	 * Updates the per-view visibility for all actors for the given view
	 *
	 * @param ViewportClient				The viewport client to update visibility on
	 * @param LayerThatChanged [optional]	If one layer was changed (toggled in view pop-up, etc), then we only need to modify actors that use that layer
	 */
	virtual void UpdatePerViewVisibility( class FLevelEditorViewportClient* ViewportClient, const FName& LayerThatChanged = NAME_Skip ) = 0;

	/**
	 * Updates per-view visibility for the given actor in the given view
	 *
	 * @param ViewportClient				The viewport client to update visibility on
	 * @param Actor								Actor to update
	 * @param bReregisterIfDirty [optional]		If true, the actor will reregister itself to give the rendering thread updated information
	 */
	virtual void UpdateActorViewVisibility( class FLevelEditorViewportClient* ViewportClient, const TWeakObjectPtr< AActor >& Actor, bool bReregisterIfDirty = true ) = 0;

	/**
	 * Updates per-view visibility for the given actor for all views
	 *
	 * @param Actor		Actor to update
	 */
	virtual void UpdateActorAllViewsVisibility( const TWeakObjectPtr< AActor >& Actor ) = 0;

	/**
	 * Removes the corresponding visibility bit from all actors (slides the later bits down 1)
	 *
	 * @param ViewportClient	The viewport client to update visibility on
	 */
	virtual void RemoveViewFromActorViewVisibility( class FLevelEditorViewportClient* ViewportClient ) = 0;

	/**
	 * Updates the provided actors visibility in the viewports
	 *
	 * @param	Actor						Actor to update
	 * @param	bOutSelectionChanged [OUT]	Whether the Editors selection changed	
	 * @param	bOutActorModified [OUT]		Whether the actor was modified
	 * @param	bNotifySelectionChange		If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bRedrawViewports			If true the viewports will be redrawn; if false, they will not
	 */
	virtual bool UpdateActorVisibility( const TWeakObjectPtr< AActor >& Actor, bool& bOutSelectionChanged, bool& bOutActorModified, bool bNotifySelectionChange, bool bRedrawViewports ) = 0;

	/**
	 * Updates the visibility of all actors in the viewports
	 *
	 * @param	bNotifySelectionChange		If true the Editor is notified of the selection change; if false, the Editor will not be notified
	 * @param	bRedrawViewports			If true the viewports will be redrawn; if false, they will not
	 */
	virtual bool UpdateAllActorsVisibility( bool bNotifySelectionChange, bool bRedrawViewports ) = 0;


	/////////////////////////////////////////////////
	// Operations on layers

	/**
	 *	Appends all the actors associated with the specified layer
	 *
	 *	@param	LayerName	The layer to find actors for
	 *	@param	InActors	The list to append the found actors to
	 */
	virtual void AppendActorsForLayer( const FName& LayerName, TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL )  ) const = 0;

	/**
	 *	Appends all the actors associated with ANY of the specified layers
	 *
	 *	@param	LayerNames	The layers to find actors for
	 *	@param	InActors	The list to append the found actors to
	 */
	virtual void AppendActorsForLayers( const TArray< FName >& LayerNames, TArray< TWeakObjectPtr< AActor > >& InActors, const TSharedPtr< ActorFilter >& Filter = TSharedPtr< ActorFilter >( NULL )  ) const = 0;

	/**
	 * Changes the named layer's visibility to the provided state
	 *
	 * @param	LayerName	The name of the layer to affect
	 * @param	bIsVisible	If true the layer will be visible; if false, the layer will not be visible
	 */
	virtual void SetLayerVisibility( const FName& LayerName, bool bIsVisible ) = 0;

	/**
	 * Changes visibility of the named layers to the provided state
	 *
	 * @param	LayerNames	The names of the layers to affect
	 * @param	bIsVisible	If true the layers will be visible; if false, the layers will not be visible
	 */
	virtual void SetLayersVisibility( const TArray< FName >& LayerNames, bool bIsVisible ) = 0;

	/**
	 * Toggles the named layer's visibility
	 *
	 * @param LayerName	The name of the layer to affect
	 */
	virtual void ToggleLayerVisibility( const FName& LayerName ) = 0;

	/**
	 * Toggles the visibility of all of the named layers
	 *
	 * @param	LayerNames	The names of the layers to affect
	 */
	virtual void ToggleLayersVisibility( const TArray< FName >& LayerNames ) = 0;

	/**
	 * Set the visibility of all layers to true
	 */
	virtual void MakeAllLayersVisible() = 0;

	/**
	 * Gets the ULayer Object of the named layer
	 *
	 * @param	LayerName	The name of the layer whose ULayer Object is returned
	 * @return				The ULayer Object of the provided layer name
	 */
	virtual TWeakObjectPtr< ULayer > GetLayer( const FName& LayerName ) const = 0;

	/**
	 * Attempts to get the ULayer Object of the provided layer name. 
	 *
	 * @param	LayerName		The name of the layer whose ULayer Object to retrieve
	 * @param	OutLayer[OUT] 	Set to the ULayer Object of the named layer. Set to Invalid if no ULayer Object exists.
	 * @return					If true a valid ULayer Object was found and set to OutLayer; if false, a valid ULayer object was not found and invalid set to OutLayer
	 */
	virtual bool TryGetLayer( const FName& LayerName, TWeakObjectPtr< ULayer >& OutLayer ) = 0;

	/**
	 * Gets all known layers and appends their names to the provide array
	 *
	 * @param OutLayers[OUT] Output array to store all known layers
	 */
	virtual void AddAllLayerNamesTo( TArray< FName >& OutLayerNames ) const = 0;

	/**
	 * Gets all known layers and appends them to the provided array
	 *
	 * @param OutLayers[OUT] Output array to store all known layers
	 */
	virtual void AddAllLayersTo( TArray< TWeakObjectPtr< ULayer > >& OutLayers ) const = 0;

	/**
	 * Creates a ULayer Object for the named layer
	 *
	 * @param	LayerName	The name of the layer to create
	 * @return				The newly created ULayer Object for the named layer
	 */
	virtual TWeakObjectPtr< ULayer > CreateLayer( const FName& LayerName ) = 0;

	/**
	 * Deletes all of the provided layers, disassociating all actors from them
	 *
	 * @param LayersToDelete	A valid list of layer names.
	 */
	virtual void DeleteLayers( const TArray< FName >& LayersToDelete ) = 0;

	/**
	 * Deletes the provided layer, disassociating all actors from them
	 *
	 * @param LayerToDelete		A valid layer name
	 */
	virtual void DeleteLayer( const FName& LayerToDelete ) = 0;

	/**
	 * Renames the provided originally named layer to the provided new name
	 *
	 * @param	OriginalLayerName	The name of the layer to be renamed
	 * @param	NewLayerName		The new name for the layer to be renamed
	 */
	virtual bool RenameLayer( const FName OriginalLayerName, const FName& NewLayerName ) = 0;

};
