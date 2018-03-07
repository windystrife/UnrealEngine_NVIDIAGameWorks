// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Public/ShowFlags.h"
#include "ComposurePlayerCompositingTarget.generated.h"


class APlayerCameraManager;
class UTextureRenderTarget2D;


/**
 * Object to bind to a APlayerCameraManager with a UTextureRenderTarget2D to be used as a player's render target.
 */
UCLASS(BlueprintType)
class COMPOSURE_API UComposurePlayerCompositingTarget : public UObject
{
	GENERATED_UCLASS_BODY()

	~UComposurePlayerCompositingTarget();

public:

	// Current player camera manager the target is bind on.
	UFUNCTION(BlueprintCallable, Category = "Player Compositing target")
	APlayerCameraManager* GetPlayerCameraManager() const
	{
		return PlayerCameraManager;
	}

	// Set player camera manager to bind the render target to.
	UFUNCTION(BlueprintCallable, Category = "Player Compositing target")
	APlayerCameraManager* SetPlayerCameraManager(APlayerCameraManager* PlayerCameraManager);

	// Set the render target of the player.
	UFUNCTION(BlueprintCallable, Category = "Player Compositing target")
	void SetRenderTarget(UTextureRenderTarget2D* RenderTarget);


	// Begins UObject
	virtual void FinishDestroy() override;
	// Ends UObject


private:
	// Current player camera manager the target is bind on.
	UPROPERTY(Transient)
	APlayerCameraManager* PlayerCameraManager;

	// Underlying player camera modifier
	UPROPERTY(Transient)
	class UComposurePlayerCompositingCameraModifier* PlayerCameraModifier;
	
	// Post process material that replaces the tonemapper to dump the player's render target.
	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* ReplaceTonemapperMID;

	// Backup of the engine showflags to restore when unbinding the compositing target from the camera manager.
	FEngineShowFlags EngineShowFlagsBackup;


	// Entries called by PlayerCameraModifier.
	void OverrideBlendableSettings(class FSceneView& View, float Weight) const;


	friend class UComposurePlayerCompositingCameraModifier;
};
