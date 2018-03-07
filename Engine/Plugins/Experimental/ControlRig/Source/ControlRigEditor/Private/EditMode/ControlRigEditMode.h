// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EdMode.h"
#include "ControlRigTrajectoryCache.h"

class FEditorViewportClient;
class FViewport;
class UActorFactory;
struct FViewportClick;
class UControlRig;
class UHumanRig;
struct FLimbControl;
class ISequencer;
class UControlRigEditModeSettings;
class UControlManipulator;
class FUICommandList;
class FPrimitiveDrawInterface;
class FToolBarBuilder;
class FExtender;
class IMovieScenePlayer;

class FControlRigEditMode : public FEdMode
{
public:
	static FName ModeName;

	FControlRigEditMode();
	~FControlRigEditMode();

	/** Set the objects to be displayed in the details panel */
	void SetObjects(const TArray<TWeakObjectPtr<>>& InSelectedObjects, const TArray<FGuid>& InObjectBindings);

	/** Set the sequencer we are bound to */
	void SetSequencer(TSharedPtr<ISequencer> InSequencer);

	// FEdMode interface
	virtual bool UsesToolkits() const override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click) override;
	virtual bool BoxSelect(FBox& InBox, bool InSelect = true) override;
	virtual bool FrustumSelect(const FConvexVolume& InFrustum, bool InSelect = true) override;
	virtual void SelectNone() override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(FWidget::EWidgetMode CheckMode) const;
	virtual FVector GetWidgetLocation() const override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& OutMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& OutMatrix, void* InData) override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;

	/** Clear all selected nodes */
	void ClearNodeSelection();

	/** Set the nodes selection state */
	void SetNodeSelection(const FName& NodeName, bool bSelected);

	/** Set multiple nodes selection states */
	void SetNodeSelection(const TArray<FName>& NodeNames, bool bSelected);

	/** Get the selected node */
	const TArray<FName>& GetSelectedNodes() const;

	/** Check if the specified node is selected */
	bool IsNodeSelected(const FName& NodeName) const;

	/** Attempt to select by property path */
	void SetNodeSelectionByPropertyPath(const TArray<FString>& InPropertyPaths);

	/** 
	 * Lets the edit mode know that an object has just been spawned. 
	 * Allows us to redisplay different underlying objects in the details panel.
	 */
	void HandleObjectSpawned(FGuid InObjectBinding, UObject* SpawnedObject, IMovieScenePlayer& Player);

	/** Re-bind to the current actor - used when sequence, selection etc. changes */
	void ReBindToActor();

	/** Bind us to an actor for editing */
	void HandleBindToActor(AActor* InActor, bool bFocus);

	/** Refresh our internal object list (they may have changed) */
	void RefreshObjects();

	DECLARE_EVENT_OneParam(FControlRigEditMode, FOnNodesSelected, const TArray<FName>& /*SelectedNodeNames*/);
	FControlRigEditMode::FOnNodesSelected& OnNodesSelected() { return OnNodesSelectedDelegate; }

	/** Refresh our trajectory cache */
	void RefreshTrajectoryCache();

	/** Set a key for a specific manipulator */
	void SetKeyForManipulator(UHierarchicalRig* HierarchicalRig, UControlManipulator* Manipulator);

	/** Get the settings we are using */
	const UControlRigEditModeSettings* GetSettings() { return Settings; }

private:
	/** Helper function: set ControlRigs array to the details panel */
	void SetObjects_Internal();

	/** Updates cached pivot transform */
	void RecalcPivotTransform();

	/** Helper function for box/frustum intersection */
	bool IntersectSelect(bool InSelect, const TFunctionRef<bool(UControlManipulator*, const FTransform&)>& Intersects);

	/** Handle selection internally */
	void HandleSelectionChanged(const TArray<FName>& InSelectedNodes);

	/** Set keys on all selected manipulators */
	void SetKeysForSelectedManipulators();

	/** Toggles visibility of manipulators in the viewport */
	void ToggleManipulators();

	/** Toggles visibility of trajectories in the viewport */
	void ToggleTrajectories();

	/** Bind our keyboard commands */
	void BindCommands();

	/** Render debug info for a limb control */
	void RenderLimb(const FLimbControl& Limb, UHumanRig* HumanRig, FPrimitiveDrawInterface* PDI);

private:
	/** Cache for rendering trajectories */
	FControlRigTrajectoryCache TrajectoryCache;

	/** Settings object used to insert controls into the details panel */
	UControlRigEditModeSettings* Settings;

	/** Currently selected nodes */
	TArray<FName> SelectedNodes;

	/** Indices of selected nodes */
	TArray<int32> SelectedIndices;

	/** Whether we are in the middle of a transaction */
	bool bIsTransacting;

	/** Whether a manipulator actually made a change when transacting */
	bool bManipulatorMadeChange;

	/** The ControlRigs we are animating */
	TArray<TWeakObjectPtr<UControlRig>> ControlRigs;

	/** The sequencer GUIDs of the objects we are animating */
	TArray<FGuid> ControlRigGuids;

	/** Sequencer we are currently bound to */
	TWeakPtr<ISequencer> WeakSequencer;

	/** As we cannot cycle widget mode during tracking, we defer cycling until after a click with this flag */
	bool bSelectedNode;

	/** Delegate fired when nodes are selected */
	FOnNodesSelected OnNodesSelectedDelegate;

	/** Guard value for selection */
	bool bSelecting;

	/** Guard value for selection by property path */
	bool bSelectingByPath;

	/** Cached transform of pivot point for selected nodes */
	FTransform PivotTransform;

	/** Command bindings for keyboard shortcuts */
	TSharedPtr<FUICommandList> CommandBindings;
};
