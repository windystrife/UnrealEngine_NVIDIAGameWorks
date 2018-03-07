// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 * 
 *
 */

#pragma once

#include "SceneCapture.h"
#include "WaveWorksShorelineCapture.generated.h"

UCLASS(hidecategories=(LOD, Collision, Material, Attachment, Actor), MinimalAPI)
class AWaveWorksShorelineCapture : public ASceneCapture
{
	GENERATED_UCLASS_BODY()

public:
	AWaveWorksShorelineCapture() {}

	//~ Begin AActor Interface
	ENGINE_API virtual void PostActorCreated() override;
	//~ End AActor Interface.

	UFUNCTION(BlueprintCallable, Category="Rendering")
	void OnInterpToggle(bool bEnable);

	/** Returns CaptureComponent2D subobject **/
	ENGINE_API class UWaveWorksShorelineCaptureComponent* GetWaveWorksShorelineCaptureComponent() const;

private:
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWaveWorksShorelineCaptureComponent* WaveWorksShorelineCaptureComponent;
};