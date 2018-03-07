// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "DeprecatedGearVRControllerComponent.generated.h"

/** DEPRECATED in 4.17 : This component (the GearVRController) is superfluous. It will be removed in a subsequent version. To emulate it, use a UStaticMeshComponent attached to a UMotionControllerComponent. The mesh used by this component can be found here: /OculusVR/Meshes/GearVRController.*/
UCLASS(ClassGroup=(GearVR), deprecated)
class UDEPRECATED_DeprecatedGearVRControllerComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDEPRECATED_DeprecatedGearVRControllerComponent();

	DEPRECATED(4.17, "The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please use the mesh asset directly (located here: /OculusVR/Meshes/GearVRController).")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meshes", meta=(DeprecatedFunction, DeprecationMessage="The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please use the mesh asset directly (located here: /OculusVR/Meshes/GearVRController)."))
	UStaticMesh* ControllerMesh;
	
	DEPRECATED(4.17, "The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please manage your own MotionControllerComponent instead.")
	UFUNCTION(BlueprintCallable, Category = "GearVRController", meta = (Keywords = "Gear VR", DeprecatedFunction, DeprecationMessage = "The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please manage your own MotionControllerComponent instead."))
	UMotionControllerComponent* GetMotionController() const;

	DEPRECATED(4.17, "The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please manage your own StaticMeshComponent instead (the GearVR controller mesh can be found here: /OculusVR/Meshes/GearVRController).")
	UFUNCTION(BlueprintCallable, Category = "GearVRController", meta = (Keywords = "Gear VR", DeprecatedFunction, DeprecationMessage = "The GearVRController component is superfluous, and will be removed from the codebase in a future release. Please manage your own StaticMeshComponent instead (the GearVR controller mesh can be found here: /OculusVR/Meshes/GearVRController)."))
	UStaticMeshComponent* GetControllerMesh() const;

	virtual void OnRegister() override;

private:	

	UMotionControllerComponent* MotionControllerComponent;
	UStaticMeshComponent* ControllerMeshComponent;
};
