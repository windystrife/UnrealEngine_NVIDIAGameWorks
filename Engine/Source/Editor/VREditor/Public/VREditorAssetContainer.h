// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "VREditorAssetContainer.generated.h"


// Forward declarations
class USoundBase;
class USoundCue;
class UStaticMesh;
class UMaterial;
class UMaterialInterface;
class UMaterialInstance;
class UFont;

/**
 * Asset container for VREditor.
 */
UCLASS()
class VREDITOR_API UVREditorAssetContainer : public UDataAsset
{
	GENERATED_BODY()

public:
	
	//
	// Sounds
	//

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowCloseSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowOpenSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowDropSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DockableWindowDragSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* DropFromContentBrowserSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* RadialMenuOpenSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* RadialMenuCloseSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* TeleportSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundCue* ButtonPressSound;

	UPROPERTY(EditAnywhere, Category = Sound)
	USoundBase* AutoScaleSound;

	//
	// Meshes
	//

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* GenericHMDMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* PlaneMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* CylinderMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerStartMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerEndMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* LaserPointerHoverMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* VivePreControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* OculusControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* GenericControllerMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* TeleportRootMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* WindowMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* WindowSelectionBarMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* WindowCloseButtonMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* RadialMenuMainMesh;

	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* RadialMenuPointerMesh;
	
	UPROPERTY(EditAnywhere, Category = Mesh)
	UStaticMesh* PointerCursorMesh;

	//
	// Materials
	//

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* GridMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* LaserPointerMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* LaserPointerTranslucentMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterial* WorldMovementPostProcessMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* TextMaterial;
	
	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* VivePreControllerMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* OculusControllerMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* TeleportMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* WindowMaterial;

	UPROPERTY(EditAnywhere, Category = Material)
	UMaterialInterface* WindowTranslucentMaterial;

	//
	// Fonts
	//

	UPROPERTY(EditAnywhere, Category = Font)
	UFont* TextFont;
};
