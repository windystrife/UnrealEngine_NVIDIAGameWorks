// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/GCObject.h"
#include "UnrealWidget.h"
#include "Editor.h"
#include "EditorUndoClient.h"

class FCanvas;
class FEditorViewportClient;
class FEdMode;
class FModeTool;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class IToolkitHost;
class USelection;
struct FConvexVolume;
struct FViewportClick;

/**
 * A helper class to store the state of the various editor modes.
 */
class UNREALED_API FEditorModeTools : public FGCObject, public FEditorUndoClient
{
public:
	FEditorModeTools();
	virtual ~FEditorModeTools();

	/**
	 * Set the default editor mode for these tools
	 * 
	 * @param	DefaultModeID		The mode ID for the new default mode
	 */
	void SetDefaultMode( const FEditorModeID DefaultModeID );

	/**
	 * Adds a new default mode to this tool's list of default modes.  You can have multiple default modes, but they all must be compatible with each other.
	 * 
	 * @param	DefaultModeID		The mode ID for the new default mode
	 */
	void AddDefaultMode( const FEditorModeID DefaultModeID );

	/**
	 * Removes a default mode
	 * 
	 * @param	DefaultModeID		The mode ID for the default mode to remove
	 */
	void RemoveDefaultMode( const FEditorModeID DefaultModeID );

	/**
	 * Activates the default modes defined by this class.  Note that there can be more than one default mode, and this call will activate them all in sequence.
	 */
	void ActivateDefaultMode();

	/** 
	 * Returns true if the default modes are active.  Note that there can be more than one default mode, and this will only return true if all default modes are active.
	 */
	bool IsDefaultModeActive() const;

	/**
	 * Activates an editor mode. Shuts down all other active modes which cannot run with the passed in mode.
	 * 
	 * @param InID		The ID of the editor mode to activate.
	 * @param bToggle	true if the passed in editor mode should be toggled off if it is already active.
	 */
	void ActivateMode( FEditorModeID InID, bool bToggle = false );

	/**
	 * Deactivates an editor mode. 
	 * 
	 * @param InID		The ID of the editor mode to deactivate.
	 */
	void DeactivateMode(FEditorModeID InID);

	/**
	 * Deactivate the mode and entirely purge it from memory. Used when a mode type is unregistered
	 */
	void DestroyMode(FEditorModeID InID);

protected:
	
	/** Deactivates the editor mode at the specified index */
	void DeactivateModeAtIndex( int32 InIndex );
		
public:

	/**
	 * Deactivates all modes, note some modes can never be deactivated.
	 */
	void DeactivateAllModes();

	/** 
	 * Returns the editor mode specified by the passed in ID
	 */
	FEdMode* FindMode( FEditorModeID InID );

	/**
	 * Returns true if the current mode is not the specified ModeID.  Also optionally warns the user.
	 *
	 * @param	ModeID			The editor mode to query.
	 * @param	ErrorMsg		If specified, inform the user the reason why this is a problem
	 * @param	bNotifyUser		If true, display the error as a notification, instead of a dialog
	 * @return					true if the current mode is not the specified mode.
	 */
	bool EnsureNotInMode(FEditorModeID ModeID, const FText& ErrorMsg = FText::GetEmpty(), bool bNotifyUser = false) const;

	FMatrix GetCustomDrawingCoordinateSystem();
	FMatrix GetCustomInputCoordinateSystem();
	
	/** 
	 * Returns true if the passed in editor mode is active 
	 */
	bool IsModeActive( FEditorModeID InID ) const;

	/**
	 * Returns a pointer to an active mode specified by the passed in ID
	 * If the editor mode is not active, NULL is returned
	 */
	FEdMode* GetActiveMode( FEditorModeID InID );
	const FEdMode* GetActiveMode( FEditorModeID InID ) const;

	template <typename SpecificModeType>
	SpecificModeType* GetActiveModeTyped( FEditorModeID InID )
	{
		return static_cast<SpecificModeType*>(GetActiveMode(InID));
	}

	template <typename SpecificModeType>
	const SpecificModeType* GetActiveModeTyped( FEditorModeID InID ) const
	{
		return static_cast<SpecificModeType*>(GetActiveMode(InID));
	}

	/**
	 * Returns the active tool of the passed in editor mode.
	 * If the passed in editor mode is not active or the mode has no active tool, NULL is returned
	 */
	const FModeTool* GetActiveTool( FEditorModeID InID ) const;

	/** 
	 * Returns an array of all active modes
	 */
	void GetActiveModes( TArray<FEdMode*>& OutActiveModes );

	void SetShowWidget( bool InShowWidget )	{ bShowWidget = InShowWidget; }
	bool GetShowWidget() const				{ return bShowWidget; }

	/** Cycle the widget mode, forwarding queries to modes */
	void CycleWidgetMode (void);

	/** Check with modes to see if the widget mode can be cycled */
	bool CanCycleWidgetMode() const;

	/**Save Widget Settings to Ini file*/
	void SaveWidgetSettings();
	/**Load Widget Settings from Ini file*/
	void LoadWidgetSettings();

	/** Gets the widget axis to be drawn */
	EAxisList::Type GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;

	/** Mouse tracking interface.  Passes tracking messages to all active modes */
	bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport);
	bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport);
	bool IsTracking() const { return bIsTracking; }

	bool AllowsViewportDragTool() const;

	/** Notifies all active modes that a map change has occured */
	void MapChangeNotify();

	/** Notifies all active modes to empty their selections */
	void SelectNone();

	/** Notifies all active modes of box selection attempts */
	bool BoxSelect( FBox& InBox, bool InSelect );

	/** Notifies all active modes of frustum selection attempts */
	bool FrustumSelect( const FConvexVolume& InFrustum, bool InSelect );

	/** true if any active mode uses a transform widget */
	bool UsesTransformWidget() const;

	/** true if any active mode uses the passed in transform widget */
	bool UsesTransformWidget( FWidget::EWidgetMode CheckMode ) const;

	/** Sets the current widget axis */
	void SetCurrentWidgetAxis( EAxisList::Type NewAxis );

	/** Notifies all active modes of mouse click messages. */
	bool HandleClick(FEditorViewportClient* InViewportClient,  HHitProxy *HitProxy, const FViewportClick &Click );

	/** true if the passed in brush actor should be drawn in wireframe */	
	bool ShouldDrawBrushWireframe( AActor* InActor ) const;

	/** true if brush vertices should be drawn */
	bool ShouldDrawBrushVertices() const;

	/** Ticks all active modes */
	void Tick( FEditorViewportClient* ViewportClient, float DeltaTime );

	/** Notifies all active modes of any change in mouse movement */
	bool InputDelta( FEditorViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale );

	/** Notifies all active modes of captured mouse movement */	
	bool CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY );

	/** Notifies all active modes of keyboard input */
	bool InputKey( FEditorViewportClient* InViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);

	/** Notifies all active modes of axis movement */
	bool InputAxis( FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime);

	bool MouseEnter( FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 X, int32 Y );
	
	bool MouseLeave( FEditorViewportClient* InViewportClient, FViewport* Viewport );

	/** Notifies all active modes that the mouse has moved */
	bool MouseMove( FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 X, int32 Y );

	/** Notifies all active modes that a viewport has received focus */
	bool ReceivedFocus( FEditorViewportClient* InViewportClient, FViewport* Viewport );

	/** Notifies all active modes that a viewport has lost focus */
	bool LostFocus( FEditorViewportClient* InViewportClient, FViewport* Viewport );

	/** Draws all active modes */	
	void DrawActiveModes( const FSceneView* InView, FPrimitiveDrawInterface* PDI );

	/** Renders all active modes */
	void Render( const FSceneView* InView, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

	/** Draws the HUD for all active modes */
	void DrawHUD( FEditorViewportClient* InViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas );

	/** Calls PostUndo on all active modes */
	// Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

	/** True if we should allow widget move */
	bool AllowWidgetMove() const;

	/** True if we should disallow mouse delta tracking. */
	bool DisallowMouseDeltaTracking() const;

	/** Get a cursor to override the default with, if any */
	bool GetCursor(EMouseCursor::Type& OutCursor) const;

	/**
	 * Returns a good location to draw the widget at.
	 */
	FVector GetWidgetLocation() const;

	/**
	 * Changes the current widget mode.
	 */
	void SetWidgetMode( FWidget::EWidgetMode InWidgetMode );

	/**
	 * Allows you to temporarily override the widget mode.  Call this function again
	 * with WM_None to turn off the override.
	 */
	void SetWidgetModeOverride( FWidget::EWidgetMode InWidgetMode );

	/**
	 * Retrieves the current widget mode, taking overrides into account.
	 */
	FWidget::EWidgetMode GetWidgetMode() const;

	/**
	 * Gets the current state of script editor usage of show friendly names
	 * @ return - If true, replace variable names with sanatized ones
	 */
	bool GetShowFriendlyVariableNames() const;

	/**
	 * Sets a bookmark in the levelinfo file, allocating it if necessary.
	 * 
	 * @param InIndex			Index of the bookmark to set.
	 * @param InViewportClient	Level editor viewport client used to get pointer the world that the bookmark is for.
	 */
	void SetBookmark( uint32 InIndex, FEditorViewportClient* InViewportClient );

	/**
	 * Checks to see if a bookmark exists at a given index
	 * 
	 * @param InIndex			Index of the bookmark to set.
	 * @param InViewportClient	Level editor viewport client used to get pointer the world to check for the bookmark in.
	 */
	bool CheckBookmark( uint32 InIndex, FEditorViewportClient* InViewportClient );

	/**
	 * Retrieves a bookmark from the list.
	 * 
	 * @param InIndex			Index of the bookmark to set.
	 * @param					Set to true to restore the visibility of any streaming levels.
	 * @param InViewportClient	Level editor viewport client used to get pointer the world that the bookmark is relevant to.
	 */
	void JumpToBookmark( uint32 InIndex, bool bShouldRestoreLevelVisibility, FEditorViewportClient* InViewportClient );

	/**
	 * Clears a bookmark from the list.
	 * 
	 * @param InIndex			Index of the bookmark to clear.
	 * @param InViewportClient	Level editor viewport client used to get pointer the world that the bookmark is relevant to.
	 */
	void ClearBookmark( uint32 InIndex, FEditorViewportClient* InViewportClient );

	/**
	 * Clears all book marks
	 * 
	 * @param InViewportClient	Level editor viewport client used to get pointer the world to clear the bookmarks from.
	 */
	void ClearAllBookmarks( FEditorViewportClient* InViewportClient );

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// End of FGCObject interface

	/**
	 * Loads the state that was saved in the INI file
	 */
	void LoadConfig(void);

	/**
	 * Saves the current state to the INI file
	 */
	void SaveConfig(void);

	/** 
	 * Sets the pivot locations
	 * 
	 * @param Location 		The location to set
	 * @param bIncGridBase	Whether or not to also set the GridBase
	 */
	void SetPivotLocation( const FVector& Location, const bool bIncGridBase );

	/**
	 * Multicast delegate for OnModeEntered and OnModeExited callbacks.
	 *
	 * First parameter:  The editor mode that was changed
	 * Second parameter:  True if entering the mode, or false if exiting the mode
	 */
	DECLARE_EVENT_TwoParams( FEditorModeTools, FEditorModeChangedEvent, FEdMode*, bool );
	FEditorModeChangedEvent& OnEditorModeChanged() { return EditorModeChangedEvent; }

	/** delegate type for triggering when widget mode changed */
	DECLARE_EVENT_OneParam( FEditorModeTools, FWidgetModeChangedEvent, FWidget::EWidgetMode );
	FWidgetModeChangedEvent& OnWidgetModeChanged() { return WidgetModeChangedEvent; }

	/**	Broadcasts the WidgetModeChanged event */
	void BroadcastWidgetModeChanged(FWidget::EWidgetMode InWidgetMode) { WidgetModeChangedEvent.Broadcast(InWidgetMode); }

	/**	Broadcasts the EditorModeChanged event */
	void BroadcastEditorModeChanged(FEdMode* Mode, bool IsEnteringMode) { EditorModeChangedEvent.Broadcast(Mode, IsEnteringMode); }

	/**
	 * Returns the current CoordSystem
	 * 
	 * @param bGetRawValue true when you want the actual value of CoordSystem, not the value modified by the state.
	 */
	ECoordSystem GetCoordSystem(bool bGetRawValue = false);

	/** Sets the current CoordSystem */
	void SetCoordSystem(ECoordSystem NewCoordSystem);

	/** Sets the hide viewport UI state */
	void SetHideViewportUI( bool bInHideViewportUI ) { bHideViewportUI = bInHideViewportUI; }

	/** Is the viewport UI hidden? */
	bool IsViewportUIHidden() const { return bHideViewportUI; }

	bool PivotShown;
	bool Snapping;
	bool SnappedActor;

	FVector CachedLocation;
	FVector PivotLocation;
	FVector SnappedLocation;
	FVector GridBase;

	/** The angle for the translate rotate widget */
	float TranslateRotateXAxisAngle;

	/** The angles for the 2d translate rotate widget */
	float TranslateRotate2DAngle;

	/** Draws in the top level corner of all FEditorViewportClient windows (can be used to relay info to the user). */
	FString InfoString;

	/** Sets the host for toolkits created via modes from this mode manager (can only be called once) */
	void SetToolkitHost(TSharedRef<IToolkitHost> Host);

	/** Returns the host for toolkits created via modes from this mode manager */
	TSharedPtr<IToolkitHost> GetToolkitHost() const;

	/** Check if toolkit host exists */
	bool HasToolkitHost() const;

	/**
	 * Returns the set of selected actors.
	 */
	virtual USelection* GetSelectedActors() const;

	/**
	 * @return the set of selected non-actor objects.
	 */
	virtual USelection* GetSelectedObjects() const;

	/**
	 * Returns the set of selected components.
	 */
	virtual USelection* GetSelectedComponents() const;

	/**
	 * Returns the world that is being edited by this mode manager
	 */ 
	virtual UWorld* GetWorld() const;

	/**
	 * Whether or not the current selection has a scene component selected
 	 */
	bool SelectionHasSceneComponent() const;
protected:
	/** 
	 * Delegate handlers
	 **/
	void OnEditorSelectionChanged(UObject* NewSelection);
	void OnEditorSelectNone();

	/** List of default modes for this tool.  These must all be compatible with each other. */
	TArray<FEditorModeID> DefaultModeIDs;

	/** A list of active editor modes. */
	TArray< TSharedPtr<FEdMode> > Modes;

	/** The host of the toolkits created by these modes */
	TWeakPtr<IToolkitHost> ToolkitHost;

	/** A list of previously active editor modes that we will potentially recycle */
	TMap< FEditorModeID, TSharedPtr<FEdMode> > RecycledModes;

	/** The mode that the editor viewport widget is in. */
	FWidget::EWidgetMode WidgetMode;

	/** If the widget mode is being overridden, this will be != WM_None. */
	FWidget::EWidgetMode OverrideWidgetMode;

	/** If 1, draw the widget and let the user interact with it. */
	bool bShowWidget;

	/** if true, the viewports will hide all UI overlays */
	bool bHideViewportUI;

	/** if true the current selection has a scene component */
	bool bSelectionHasSceneComponent;
private:

	/** The coordinate system the widget is operating within. */
	ECoordSystem CoordSystem;

	/** Multicast delegate that is broadcast when a mode is entered or exited */
	FEditorModeChangedEvent EditorModeChangedEvent;

	/** Multicast delegate that is broadcast when a widget mode is changed */
	FWidgetModeChangedEvent WidgetModeChangedEvent;

	/** Flag set between calls to StartTracking() and EndTracking() */
	bool bIsTracking;
};
