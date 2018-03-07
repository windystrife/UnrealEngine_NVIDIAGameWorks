// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UnrealWidget.h"
#include "EditorViewportClient.h"

class FBlueprintEditor;
class FCanvas;
class FPreviewScene;
class FScopedTransaction;
class SSCSEditorViewport;
class UStaticMeshComponent;

/**
 * An editor viewport client subclass for the SCS editor viewport.
 */
class FSCSEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FSCSEditorViewportClient>
{
public:
	/**
	 * Constructor.
	 *
	 * @param InBlueprintEditorPtr A weak reference to the Blueprint Editor context.
	 * @param InPreviewScene The preview scene to use.
	 */
	FSCSEditorViewportClient(TWeakPtr<class FBlueprintEditor>& InBlueprintEditorPtr, FPreviewScene* InPreviewScene, const TSharedRef<SSCSEditorViewport>& InSCSEditorViewport);

	/**
	 * Destructor.
	 */
	virtual ~FSCSEditorViewportClient();

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) override;
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) override;
	virtual void TrackingStopped() override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual void SetWidgetMode( FWidget::EWidgetMode NewMode ) override;
	virtual void SetWidgetCoordSystemSpace( ECoordSystem NewCoordSystem ) override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override { return WidgetCoordSystem; }
	virtual int32 GetCameraSpeedSetting() const override;
	virtual void SetCameraSpeedSetting(int32 SpeedSetting) override;


	/** 
	 * Recreates the preview scene and invalidates the owning viewport.
	 *
	 * @param bResetCamera Whether or not to reset the camera after recreating the preview scene.
	 */
	void InvalidatePreview(bool bResetCamera = true);

	/**
	 * Resets the camera position
	 */
	void ResetCamera();

	/**
	 * Determines whether or not realtime preview is enabled.
	 * 
	 * @return true if realtime preview is enabled, false otherwise.
	 */
	bool GetRealtimePreview() const
	{
		return IsRealtime();
	}

	/**
	 * Toggles realtime preview on/off.
	 */
	void ToggleRealtimePreview();

	/**
	 * Focuses the viewport on the selected components
	 */
	void FocusViewportToSelection();

	/**
	 * Returns true if simulate is enabled in the viewport
	 */
	bool GetIsSimulateEnabled();

	/**
	 * Will toggle the simulation mode of the viewport
	 */
	void ToggleIsSimulateEnabled();

	/**
	 * Returns true if the floor is currently visible in the viewport
	 */
	bool GetShowFloor();

	/**
	 * Will toggle the floor's visibility in the viewport
	 */
	void ToggleShowFloor();

	/**
	 * Returns true if the grid is currently visible in the viewport
	 */
	bool GetShowGrid();

	/**
	 * Will toggle the grid's visibility in the viewport
	 */
	void ToggleShowGrid();

	/**
	 * Gets the current preview actor instance.
	 */
	AActor* GetPreviewActor() const;

protected:
	/**
	 * Initiates a transaction.
	 */
	void BeginTransaction(const FText& Description);

	/**
	 * Ends the current transaction, if one exists.
	 */
	void EndTransaction();

	/**
	 * Updates preview bounds and floor positioning
	 */
	void RefreshPreviewBounds();

private:
	FWidget::EWidgetMode WidgetMode;
	ECoordSystem WidgetCoordSystem;

	/** Weak reference to the editor hosting the viewport */
	TWeakPtr<class FBlueprintEditor> BlueprintEditorPtr;

	/** The full bounds of the preview scene (encompasses all visible components) */
	FBoxSphereBounds PreviewActorBounds;

	/** If true then we are manipulating a specific property or component */
	bool bIsManipulating;

	/** The current transaction for undo/redo */
	FScopedTransaction* ScopedTransaction;

	/** Floor static mesh component */
	UStaticMeshComponent* EditorFloorComp;

	/** If true, the physics simulation gets ticked */
	bool bIsSimulateEnabled;
};
