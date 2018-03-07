// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "LightmassPortalComponent.generated.h"

UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent, Activation, "Components|Activation", Mobility), MinimalAPI)
class ULightmassPortalComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UBoxComponent* PreviewBox;

public:
	void UpdatePreviewShape();

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface
};

