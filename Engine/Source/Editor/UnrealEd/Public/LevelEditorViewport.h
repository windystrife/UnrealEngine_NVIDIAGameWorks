// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "UnrealWidget.h"
#include "EditorViewportClient.h"

struct FAssetData;
class FCanvas;
class FDragTool;
class HModel;
class ILevelEditor;
class SLevelViewport;
class UActorFactory;
class UModel;
struct FWorldContext;

/** Describes an object that's currently hovered over in the level viewport */
struct FViewportHoverTarget
{
	/** The actor we're drawing the hover effect for, or NULL */
	AActor* HoveredActor;

	/** The BSP model we're drawing the hover effect for, or NULL */
	UModel* HoveredModel;

	/** Surface index on the BSP model that currently has a hover effect */
	uint32 ModelSurfaceIndex;


	/** Construct from an actor */
	FViewportHoverTarget( AActor* InActor )
		: HoveredActor( InActor ),
			HoveredModel( NULL ),
			ModelSurfaceIndex( INDEX_NONE )
	{
	}

	/** Construct from an BSP model and surface index */
	FViewportHoverTarget( UModel* InModel, int32 InSurfaceIndex )
		: HoveredActor( NULL ),
			HoveredModel( InModel ),
			ModelSurfaceIndex( InSurfaceIndex )
	{
	}

	/** Equality operator */
	bool operator==( const FViewportHoverTarget& RHS ) const
	{
		return RHS.HoveredActor == HoveredActor &&
				RHS.HoveredModel == HoveredModel &&
				RHS.ModelSurfaceIndex == ModelSurfaceIndex;
	}

	friend uint32 GetTypeHash( const FViewportHoverTarget& Key )
	{
		return Key.HoveredActor ? GetTypeHash(Key.HoveredActor) : GetTypeHash(Key.HoveredModel)+Key.ModelSurfaceIndex;
	}
};

struct UNREALED_API FTrackingTransaction
{
	/** State of this transaction */
	struct ETransactionState
	{
		enum Enum
		{
			Inactive,
			Active,
			Pending,
		};
	};

	FTrackingTransaction();
	~FTrackingTransaction();

	/**
	 * Initiates a transaction.
	 */
	void Begin(const FText& Description);

	void End();

	void Cancel();

	/** Begin a pending transaction, which won't become a real transaction until PromotePendingToActive is called */
	void BeginPending(const FText& Description);

	/** Promote a pending transaction (if any) to an active transaction */
	void PromotePendingToActive();

	bool IsActive() const { return TrackingTransactionState == ETransactionState::Active; }

	bool IsPending() const { return TrackingTransactionState == ETransactionState::Pending; }

	int32 TransCount;

private:
	/** The current transaction. */
	class FScopedTransaction*	ScopedTransaction;

	/** This is set to Active if TrackingStarted() has initiated a transaction, Pending if a transaction will begin before the next delta change */
	ETransactionState::Enum TrackingTransactionState;

	/** The description to use if a pending transaction turns into a real transaction */
	FText PendingDescription;
};


/** */
class UNREALED_API FLevelEditorViewportClient : public FEditorViewportClient
{
public:

	/** @return Returns the current global drop preview actor, or a NULL pointer if we don't currently have one */
	static const TArray< TWeakObjectPtr<AActor> >& GetDropPreviewActors()
	{
		return DropPreviewActors;
	}

	FVector2D GetDropPreviewLocation() const { return FVector2D( DropPreviewMouseX, DropPreviewMouseY ); }

	/**
	 * Constructor
	 */
	FLevelEditorViewportClient(const TSharedPtr<class SLevelViewport>& InLevelViewport);

	/**
	 * Destructor
	 */
	virtual ~FLevelEditorViewportClient();

	////////////////////////////
	// FViewElementDrawer interface
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	// End of FViewElementDrawer interface
	
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const EStereoscopicPass StereoPass = eSSP_FULL) override;

	////////////////////////////
	// FEditorViewportClient interface
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) override;
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport,int32 X,int32 Y) override;
	virtual void CapturedMouseMove( FViewport* InViewport, int32 InMouseX, int32 InMouseY ) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual TSharedPtr<FDragTool> MakeDragTool( EDragTool::Type DragToolType ) override;
	virtual bool IsLevelEditorClient() const override { return ParentLevelEditor.IsValid(); }
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge ) override;
	virtual void TrackingStopped() override;
	virtual void AbortTracking() override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual void SetupViewForRendering( FSceneViewFamily& ViewFamily, FSceneView& View ) override;
	virtual FLinearColor GetBackgroundColor() const override;
	virtual int32 GetCameraSpeedSetting() const override;
	virtual void SetCameraSpeedSetting(int32 SpeedSetting) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual UWorld* GetWorld() const override;
	virtual void BeginCameraMovement(bool bHasMovement) override;
	virtual void EndCameraMovement() override;

	virtual bool OverrideHighResScreenshotCaptureRegion(FIntRect& OutCaptureRegion) override;

	/** Sets a flag for this frame indicating that the camera has been cut, and temporal effects (such as motion blur) should be reset */
	void SetIsCameraCut()
	{
		bEditorCameraCut = true;
		bWasEditorCameraCut = false;
	}

	/** 
	 * Initialize visibility flags
	 */
	void InitializeVisibilityFlags();

	/**
	 * Reset the camera position and rotation.  Used when creating a new level.
	 */
	void ResetCamera();

	/**
	 * Reset the view for a new map 
	 */
	void ResetViewForNewMap();

	/**
	 * Stores camera settings that may be adversely affected by PIE, so that they may be restored later
	 */
	void PrepareCameraForPIE();

	/**
	 * Restores camera settings that may be adversely affected by PIE
	 */
	void RestoreCameraFromPIE();

	/**
	 * Updates the audio listener for this viewport 
	 *
	 * @param View	The scene view to use when calculate the listener position
	 */
	void UpdateAudioListener( const FSceneView& View );

	/** Determines if the new MoveCanvas movement should be used */
	bool ShouldUseMoveCanvasMovement (void);

	/** 
	 * Returns true if the passed in volume is visible in the viewport (due to volume actor visibility flags)
	 *
	 * @param VolumeActor	The volume to check
	 */
	bool IsVolumeVisibleInViewport( const AActor& VolumeActor ) const;

	/**
	 * Updates or resets view properties such as aspect ratio, FOV, location etc to match that of any actor we are locked to
	 */
	void UpdateViewForLockedActor(float DeltaTime=0.f);

	/**
	 * Returns the horizontal axis for this viewport.
	 */
	EAxisList::Type GetHorizAxis() const;

	/**
	 * Returns the vertical axis for this viewport.
	 */
	EAxisList::Type GetVertAxis() const;

	virtual void NudgeSelectedObjects( const struct FInputEventState& InputState ) override;

	/**
	 * Moves the viewport camera according to the locked actors location and rotation
	 */
	void MoveCameraToLockedActor();

	/**
	 * Check to see if this actor is locked by the viewport
	 */
	bool IsActorLocked(const TWeakObjectPtr<AActor> InActor) const;

	/**
	 * Check to see if any actor is locked by the viewport
	 */
	bool IsAnyActorLocked() const;

	void ApplyDeltaToActors( const FVector& InDrag, const FRotator& InRot, const FVector& InScale );
	void ApplyDeltaToActor( AActor* InActor, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale );
	void ApplyDeltaToComponent(USceneComponent* InComponent, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale);

	virtual void SetIsSimulateInEditorViewport( bool bInIsSimulateInEditorViewport ) override;

	/**
	 *	Draw the texture streaming bounds.
	 */
	void DrawTextureStreamingBounds(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/** GC references. */
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	
	/**
	 * Copies layout and camera settings from the specified viewport
	 *
	 * @param InViewport The viewport to copy settings from
	 */
	void CopyLayoutFromViewport( const FLevelEditorViewportClient& InViewport );

	/**
	 * Returns whether the provided unlocalized sprite category is visible in the viewport or not
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 *
	 * @return	true if the specified category is visible in the viewport; false if it is not
	 */
	bool GetSpriteCategoryVisibility( const FName& InSpriteCategory ) const;

	/**
	 * Returns whether the sprite category specified by the provided index is visible in the viewport or not
	 *
	 * @param	Index	Index of the sprite category to check
	 *
	 * @return	true if the category specified by the index is visible in the viewport; false if it is not
	 */
	bool GetSpriteCategoryVisibility( int32 Index ) const;

	/**
	 * Sets the visibility of the provided unlocalized category to the provided value
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 * @param	bVisible			true if the category should be made visible, false if it should be hidden
	 */
	void SetSpriteCategoryVisibility( const FName& InSpriteCategory, bool bVisible );

	/**
	 * Sets the visibility of the category specified by the provided index to the provided value
	 *
	 * @param	Index		Index of the sprite category to set the visibility of
	 * @param	bVisible	true if the category should be made visible, false if it should be hidden
	 */
	void SetSpriteCategoryVisibility( int32 Index, bool bVisible );

	/**
	 * Sets the visibility of all sprite categories to the provided value
	 *
	 * @param	bVisible	true if all the categories should be made visible, false if they should be hidden
	 */
	void SetAllSpriteCategoryVisibility( bool bVisible );

	void SetReferenceToWorldContext(FWorldContext& WorldContext);

	void RemoveReferenceToWorldContext(FWorldContext& WorldContext);

	/** Returns true if a placement dragging actor exists */
	virtual bool HasDropPreviewActors() const override;

	/**
	 * If dragging an actor for placement, this function updates its position.
	 *
	 * @param MouseX						The position of the mouse's X coordinate
	 * @param MouseY						The position of the mouse's Y coordinate
	 * @param DroppedObjects				The Objects that were used to create preview objects
	 * @param out_bDroppedObjectsVisible	Output, returns if preview objects are visible or not
	 *
	 * Returns true if preview actors were updated
	 */
	virtual bool UpdateDropPreviewActors(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, bool& out_bDroppedObjectsVisible, class UActorFactory* FactoryToUse = NULL) override;

	/**
	 * If dragging an actor for placement, this function destroys the actor.
	 */
	virtual void DestroyDropPreviewActors() override;

	/**
	 * Checks the viewport to see if the given object can be dropped using the given mouse coordinates local to this viewport
	 *
	 * @param MouseX			The position of the mouse's X coordinate
	 * @param MouseY			The position of the mouse's Y coordinate
	 * @param AssetInfo			Asset in question to be dropped
	 */
	virtual FDropQuery CanDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const FAssetData& AssetInfo) override;

	/**
	 * Attempts to intelligently drop the given objects in the viewport, using the given mouse coordinates local to this viewport
	 *
	 * @param MouseX			 The position of the mouse's X coordinate
	 * @param MouseY			 The position of the mouse's Y coordinate
	 * @param DroppedObjects	 The Objects to be placed into the editor via this viewport
	 * @param OutNewActors		 The new actor objects that were created
	 * @param bOnlyDropOnTarget  Flag that when True, will only attempt a drop on the actor targeted by the Mouse position. Defaults to false.
	 * @param bCreateDropPreview If true, a drop preview actor will be spawned instead of a normal actor.
	 * @param bSelectActors		 If true, select the newly dropped actors (defaults: true)
	 * @param FactoryToUse		 The preferred actor factory to use (optional)
	 */
	virtual bool DropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors, bool bOnlyDropOnTarget = false, bool bCreateDropPreview = false, bool bSelectActors = true, UActorFactory* FactoryToUse = NULL ) override;

	/**
	 * Sets GWorld to the appropriate world for this client
	 * 
	 * @return the previous GWorld
	 */
	virtual UWorld* ConditionalSetWorld() override;

	/**
	 * Restores GWorld to InWorld
	 *
	 * @param InWorld	The world to restore
	 */
	virtual void ConditionalRestoreWorld( UWorld* InWorld  ) override;

	/**
	 *	Called to check if a material can be applied to an object, given the hit proxy
	 */
	bool CanApplyMaterialToHitProxy( const HHitProxy* HitProxy ) const;

	/**
	 * Static: Adds a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to add the effect to
	 */
	static void AddHoverEffect( const struct FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Removes a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to remove the effect from
	 */
	static void RemoveHoverEffect( const struct FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Clears viewport hover effects from any objects that currently have that
	 */
	static void ClearHoverFromObjects();


	/**
	 * Helper function for ApplyDeltaTo* functions - modifies scale based on grid settings.
	 * Currently public so it can be re-used in FEdModeBlueprint.
	 */
	void ModifyScale( USceneComponent* InComponent, FVector& ScaleDelta ) const;

	/** Set the global ptr to the current viewport */
	void SetCurrentViewport();

	/** Set the global ptr to the last viewport to receive a key press */
	void SetLastKeyViewport();

	/** 
	 * Gets the world space cursor info from the current mouse position
	 * 
	 * @param InViewportClient	The viewport client to check for mouse position and to set up the scene view.
	 * @return					An FViewportCursorLocation containing information about the mouse position in world space.
	 */
	FViewportCursorLocation GetCursorWorldLocationFromMousePos();
	
	/** 
	 * Access the 'active' actor lock. This is the actor locked to the viewport via the viewport menus.
	 * It is forced to be inactive if Matinee is controlling locking.
	 * 
	 * @return  The actor currently locked to the viewport and actively linked to the camera movements.
	 */
	TWeakObjectPtr<AActor> GetActiveActorLock() const
	{
		if (ActorLockedByMatinee.IsValid())
		{
			return TWeakObjectPtr<AActor>();
		}
		return ActorLockedToCamera;
	}
	
	/**
	 * Find a view component to use for the specified actor. Prioritizes selected 
	 * components first, followed by camera components (then falls through to the first component that implements GetEditorPreviewInfo)
	 */
	static USceneComponent* FindViewComponentForActor(AActor const* Actor);

	/** 
	 * Find the camera component that is driving this viewport, in the following order of preference:
	 *		1. Matinee locked actor
	 *		2. User actor lock (if (bLockedCameraView is true)
	 * 
	 * @return  Pointer to a camera component to use for this viewport's view
	 */
	UCameraComponent* GetCameraComponentForView() const
	{
		const AActor* LockedActor = ActorLockedByMatinee.Get();

		if (!LockedActor && bLockedCameraView)
		{
			LockedActor = ActorLockedToCamera.Get();
		}

		return Cast<UCameraComponent>(FindViewComponentForActor(LockedActor));
	}

	/** 
	 * Set the actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	void SetActorLock(AActor* Actor);

	/** 
	 * Set the actor locked to the viewport by Matinee.
	 */
	void SetMatineeActorLock(AActor* Actor);

	/** 
	 * Check whether this viewport is locked to the specified actor
	 */
	bool IsLockedToActor(AActor* Actor) const
	{
		return ActorLockedToCamera.Get() == Actor || ActorLockedByMatinee.Get() == Actor;
	}

	/** 
	 * Check whether this viewport is locked to display the matinee view
	 */
	bool IsLockedToMatinee() const
	{
		return ActorLockedByMatinee.IsValid();
	}

	/**
	 * Get the sound stat flags enabled for this viewport
	 */
	virtual ESoundShowFlags::Type GetSoundShowFlags() const override
	{ 
		return SoundShowFlags;
	}

	/**
	 * Set the sound stat flags enabled for this viewport
	 */
	virtual void SetSoundShowFlags(const ESoundShowFlags::Type InSoundShowFlags) override
	{
		SoundShowFlags = InSoundShowFlags;
	}

	void UpdateHoveredObjects( const TSet<FViewportHoverTarget>& NewHoveredObjects );

	/**
	 * Calling SetViewportType from Dragtool_ViewportChange
	 */
	void SetViewportTypeFromTool(ELevelViewportType InViewportType);

	/**
	 * Static: Attempts to place the specified object in the level, returning one or more newly-created actors if successful.
	 * IMPORTANT: The placed actor's location must be first set using GEditor->ClickLocation and GEditor->ClickPlane.
	 *
	 * @param	InLevel			Level in which to drop actor
	 * @param	ObjToUse		Asset to attempt to use for an actor to place
	 * @param	CursorLocation	Location of the cursor while dropping
	 * @param	bSelectActors	If true, select the newly dropped actors (defaults: true)
	 * @param	ObjectFlags		The flags to place on the actor when it is spawned
	 * @param	FactoryToUse	The preferred actor factory to use (optional)
	 *
	 * @return	true if the object was successfully used to place an actor; false otherwise
	 */
	static TArray<AActor*> TryPlacingActorFromObject( ULevel* InLevel, UObject* ObjToUse, bool bSelectActors, EObjectFlags ObjectFlags, UActorFactory* FactoryToUse, const FName Name = NAME_None );

	/** 
	 * Returns true if creating a preview actor in the viewport. 
	 */
	static bool IsDroppingPreviewActor()
	{
		return bIsDroppingPreviewActor;
	}

	/**
	 * Static: Given a texture, returns a material for that texture, creating a new asset if necessary.  This is used
	 * for dragging and dropping assets into the scene
	 *
	 * @param	UnrealTexture	Texture that we need a material for
	 *
	 * @return	The material that uses this texture, or null if we couldn't find or create one
	 */
	static UObject* GetOrCreateMaterialFromTexture( UTexture* UnrealTexture );

	/** Whether transport controls can be attached */
	virtual bool CanAttachTransportControls() const { return true; }

protected:
	/** 
	 * Checks the viewport to see if the given blueprint asset can be dropped on the viewport.
	 * @param AssetInfo		The blueprint Asset in question to be dropped
	 *
	 * @return true if asset can be dropped, false otherwise
	 */
	bool CanDropBlueprintAsset ( const struct FSelectedAssetInfo& );

	/** Called when editor cleanse event is triggered */
	void OnEditorCleanse();

	/** Called before the editor tries to begin PIE */
	void OnPreBeginPIE(const bool bIsSimulating);

	/** Callback for when an editor user setting has changed */
	void HandleViewportSettingChanged(FName PropertyName);

	/** Delegate handler for ActorMoved events */
	void OnActorMoved(AActor* InActor);

	/** FEditorViewportClient Interface*/
	virtual void UpdateLinkedOrthoViewports( bool bInvalidate = false ) override;
	virtual ELevelViewportType GetViewportType() const override;
	virtual void SetViewportType( ELevelViewportType InViewportType ) override;
	virtual void RotateViewportType() override;
	virtual void OverridePostProcessSettings( FSceneView& View ) override;
	virtual void PerspectiveCameraMoved() override;
	virtual bool ShouldLockPitch() const override;
	virtual void CheckHoveredHitProxy( HHitProxy* HoveredHitProxy ) override;
	virtual bool GetActiveSafeFrame(float& OutAspectRatio) const override;
	virtual void RedrawAllViewportsIntoThisScene() override;

private:
	/**
	 * Checks to see the viewports locked actor need updating
	 */
	void UpdateLockedActorViewports(const AActor* InActor, const bool bCheckRealtime);
	void UpdateLockedActorViewport(const AActor* InActor, const bool bCheckRealtime);

	/**
	 * Moves the locked actor according to the viewport cameras location and rotation
	 */
	void MoveLockedActorToCamera();
	
	/** @return	Returns true if the delta tracker was used to modify any selected actors or BSP.  Must be called before EndTracking(). */
	bool HaveSelectedObjectsBeenChanged() const;

	/**
	 * Called when to attempt to apply an object to a BSP surface
	 *
	 * @param	ObjToUse			The object to attempt to apply
	 * @param	ModelHitProxy		The hitproxy of the BSP model whose surface the user is clicking on
	 * @param	Cursor				Mouse cursor location
	 *
	 * @return	true if the object was applied to the object
	 */
	bool AttemptApplyObjAsMaterialToSurface( UObject* ObjToUse, class HModel* ModelHitProxy, FViewportCursorLocation& Cursor );

	/**
	 * Called when an asset is dropped onto the blank area of a viewport.
	 *
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	 * @param	OutNewActors		The list of actors created while dropping
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 * @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	 * @param	FactoryToUse		The preferred actor factory to use (optional)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnBackground( struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, class UActorFactory* FactoryToUse = NULL );

	/**
	* Called when an asset is dropped upon an existing actor.
	*
	* @param	Cursor				Mouse cursor location
	* @param	DroppedObjects		Array of objects dropped into the viewport
	* @param	DroppedUponActor	The actor that we are dropping upon
	* @param    DroppedUponSlot     The material slot/submesh that was identified as the drop location.  If unknown use -1.
	* @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	* @param	OutNewActors		The list of actors created while dropping
	* @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	* @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	* @param	FactoryToUse		The preferred actor factory to use (optional)
	*
	* @return	true if the drop operation was successfully handled; false otherwise
	*/
	bool DropObjectsOnActor(struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, AActor* DroppedUponActor, int32 DroppedUponSlot, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, class UActorFactory* FactoryToUse = NULL);

	/**
	 * Called when an asset is dropped upon a BSP surface.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	TargetProxy			Hit proxy representing the dropped upon model
	 * @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	 * @param	OutNewActors		The list of actors created while dropping
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 * @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	 * @param	FactoryToUse		The preferred actor factory to use (optional)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnBSPSurface(FSceneView* View, struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, HModel* TargetProxy, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, UActorFactory* FactoryToUse = NULL);

	/**
	 * Called when an asset is dropped upon a manipulation widget.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnWidget(FSceneView* View, struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, bool bCreateDropPreview = false);

	/** Helper functions for ApplyDeltaTo* functions - modifies scale based on grid settings */
	void ModifyScale( AActor* InActor, FVector& ScaleDelta, bool bCheckSmallExtent = false ) const;
	void ValidateScale( const FVector& InOriginalPreDragScale, const FVector& CurrentScale, const FVector& BoxExtent, FVector& ScaleDelta, bool bCheckSmallExtent = false ) const;

	/** Project the specified actors into the world according to the current drag parameters */
	void ProjectActorsIntoWorld(const TArray<AActor*>& Actors, FViewport* Viewport, const FVector& Drag, const FRotator& Rot);

	/** Draw additional details for brushes in the world */
	void DrawBrushDetails(const FSceneView* View, FPrimitiveDrawInterface* PDI);

public:
	/** Static: List of objects we're hovering over */
	static TSet< FViewportHoverTarget > HoveredObjects;
	
	/** Parent level editor that owns this viewport.  Currently, this may be null if the parent doesn't happen to be a level editor. */
	TWeakPtr< class ILevelEditor > ParentLevelEditor;

	/** List of layers that are hidden in this view */
	TArray<FName>			ViewHiddenLayers;

	/** Special volume actor visibility settings. Each bit represents a visibility state for a specific volume class. 1 = visible, 0 = hidden */
	TBitArray<>				VolumeActorVisibility;

	/** The viewport location that is restored when exiting PIE */
	FVector					LastEditorViewLocation;
	/** The viewport orientation that is restored when exiting PIE */
	FRotator				LastEditorViewRotation;

	FVector					ColorScale;

	FColor					FadeColor;

	float					FadeAmount;

	bool					bEnableFading;

	bool					bEnableColorScaling;

	/** Indicates whether, of not, the base attachment volume should be drawn for this viewport. */
	bool bDrawBaseInfo;

	/**
	 * Used for drag duplication. Set to true on Alt+LMB so that the selected
	 * objects (components or actors) will be duplicated as soon as the widget is displaced.
	 */
	bool					bDuplicateOnNextDrag;

	/**
	* bDuplicateActorsOnNextDrag will not be set again while bDuplicateActorsInProgress is true.
	* The user needs to release ALT and all mouse buttons to clear bDuplicateActorsInProgress.
	*/
	bool					bDuplicateActorsInProgress;

	/**
	 * true when a brush is being transformed by its Widget
	 */
	bool					bIsTrackingBrushModification;

	/**
	 * true if only the pivot position has been moved
	 */
	bool					bOnlyMovedPivot;

	/** True if this viewport is to change its view (aspect ratio, post processing, FOV etc) to match that of the currently locked camera, if applicable */
	bool					bLockedCameraView;

	/** Whether this viewport recently received focus. Used to determine whether component selection is permissible. */
	bool bReceivedFocusRecently;

	/** When enabled, the Unreal transform widget will become visible after an actor is selected, even if it was turned off via a show flag */
	bool bAlwaysShowModeWidgetAfterSelectionChanges;
private:
	/** The actors that are currently being placed in the viewport via dragging */
	static TArray< TWeakObjectPtr< AActor > > DropPreviewActors;

	/** If currently creating a preview actor. */
	static bool bIsDroppingPreviewActor;

	/** A map of actor locations before a drag operation */
	mutable TMap<TWeakObjectPtr<AActor>, FTransform> PreDragActorTransforms;

	/** Bit array representing the visibility of every sprite category in the current viewport */
	TBitArray<>	SpriteCategoryVisibility;

	UWorld* World;

	FTrackingTransaction TrackingTransaction;

	/** Represents the last known drop preview mouse position. */
	int32 DropPreviewMouseX;
	int32 DropPreviewMouseY;

	/** If this view was controlled by another view this/last frame, don't update itself */
	bool bWasControlledByOtherViewport;

	/**
	 * When locked to an actor this view will be positioned in the same location and rotation as the actor.
	 * If the actor has a camera component the view will also inherit camera settings such as aspect ratio, FOV, post processing settings, and the like.
	 * A viewport locked to an actor by Matinee will always take precedent over any other.
	 */
	TWeakObjectPtr<AActor>	ActorLockedByMatinee;
	TWeakObjectPtr<AActor>	ActorLockedToCamera;

	/** Those sound stat flags which are enabled on this viewport */
	ESoundShowFlags::Type	SoundShowFlags;

	/** If true, we switched between two different cameras. Set by matinee, used by the motion blur to invalidate this frames motion vectors */
	bool					bEditorCameraCut;

	/** Stores the previous frame's value of bEditorCameraCut in order to reset it back to false on the next frame */
	bool					bWasEditorCameraCut;
};
