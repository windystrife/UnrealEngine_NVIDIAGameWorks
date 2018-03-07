// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "VREditorAvatarActor.generated.h"

class UMaterialInstanceDynamic;

/**
 * Avatar Actor
 */
UCLASS()
class AVREditorAvatarActor : public AActor
{
	GENERATED_BODY()

public:

	/** Default constructor */
	AVREditorAvatarActor();

	/** Called by VREditorMode::Enter to initialize all post constructor components and to set the VRMode */
	void Init( class UVREditorMode* InVRMode );

	/** Called by VREditorMode to update us every frame */
	void TickManually( const float DeltaTime );

	// UObject overrides
	virtual bool IsEditorOnly() const final
	{
		return true;
	}

private:

	/** Our avatar's head mesh */
	UPROPERTY()
	class UStaticMeshComponent* HeadMeshComponent;

	/** The grid that appears while the user is dragging the world around */
	UPROPERTY()
	class UStaticMeshComponent* WorldMovementGridMeshComponent;

	//
	// World movement grid & FX
	//

	/** Grid mesh component dynamic material instance to set the opacity */
	UPROPERTY()
	class UMaterialInstanceDynamic* WorldMovementGridMID;

	/** Opacity of the movement grid and post process */
	UPROPERTY()
	float WorldMovementGridOpacity;

	/** True if we're currently drawing our world movement post process */
	UPROPERTY()
	bool bIsDrawingWorldMovementPostProcess;

	/** Post process material for "greying out" the world while in world movement mode */
	UPROPERTY()
	class UMaterialInstanceDynamic* WorldMovementPostProcessMaterial;

	//
	// World scaling progress bar
	//

	/** Background progressbar scaling mesh */
	UPROPERTY()
	class UStaticMeshComponent* ScaleProgressMeshComponent;

	/** Current scale progressbar mesh */
	UPROPERTY()
	class UStaticMeshComponent* CurrentScaleProgressMeshComponent;

	/** Current scale text */
	UPROPERTY()
	class UTextRenderComponent* UserScaleIndicatorText;

	/** Base dynamic material for the user scale fixed progressbar */
	UPROPERTY()
	UMaterialInstanceDynamic* FixedUserScaleMID;

	/** Translucent dynamic material for the user scale fixed progressbar */
	UPROPERTY()
	UMaterialInstanceDynamic* TranslucentFixedUserScaleMID;
	
	/** Base dynamic material for the current user scale progressbar */
	UPROPERTY()
	UMaterialInstanceDynamic* CurrentUserScaleMID;

	/** Translucent dynamic material for the current user scale progressbar */
	UPROPERTY()
	UMaterialInstanceDynamic* TranslucentCurrentUserScaleMID;

	//
	// Post process
	//

	/** Post process for drawing VR-specific post effects */
	UPROPERTY()
	class UPostProcessComponent* PostProcessComponent;

	/** Owning object */
	UPROPERTY()
	class UVREditorMode* VRMode;
};
