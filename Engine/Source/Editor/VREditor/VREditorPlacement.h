// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetData.h"
#include "VREditorPlacement.generated.h"

class UActorComponent;
class UViewportInteractor;
class UViewportWorldInteraction;
class UVREditorMode;
struct FViewportActionKeyInput;

/**
 * VR Editor interaction with the 3D world
 */
UCLASS()
class UVREditorPlacement : public UObject
{
	GENERATED_BODY()

public:

	/** Default constructor */
	UVREditorPlacement();

	/** Registers to events and sets initial values */
	void Init(UVREditorMode* InVRMode);
	
	/** Removes registered event */
	void Shutdown();

	/** Try spawn and start placing the specified objects */
	void StartPlacingObjects( const TArray<UObject*>& ObjectsToPlace, class UActorFactory* FactoryToUse, class UVREditorInteractor* PlacingWithInteractor, const bool bShouldInterpolateFromDragLocation );

protected:
	/** When an interactor stops dragging */
	void StopDragging( UViewportInteractor* Interactor );

	/** When the world scale changes, update the near clip plane */
	void UpdateNearClipPlaneOnScaleChange(const float NewWorldToMetersScale);

	/** Starts dragging a material, allowing the user to drop it on an object in the scene to place it */
	void StartDraggingMaterialOrTexture( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, const FVector HitLocation, UObject* MaterialOrTextureAsset );

	/** Tries to place whatever material or texture that's being dragged on the object under the hand's laser pointer */
	void PlaceDraggedMaterialOrTexture( UViewportInteractor* Interactor );

	/** Called when FEditorDelegates::OnAssetDragStarted is broadcast */
	void OnAssetDragStartedFromContentBrowser( const TArray<FAssetData>& DraggedAssets, class UActorFactory* FactoryToUse );

protected:

	/** Owning object */
	UPROPERTY()
	UVREditorMode* VRMode;

	/** The actual ViewportWorldInteraction */
	UPROPERTY()
	UViewportWorldInteraction* ViewportWorldInteraction;

	//
	// Dragging object from UI
	//

	/** The UI used to drag an asset into the level */
	UPROPERTY()
	class UWidgetComponent* FloatingUIAssetDraggedFrom;

	/** The material or texture asset we're dragging to place on an object */
	UPROPERTY()
	UObject* PlacingMaterialOrTextureAsset;
};

