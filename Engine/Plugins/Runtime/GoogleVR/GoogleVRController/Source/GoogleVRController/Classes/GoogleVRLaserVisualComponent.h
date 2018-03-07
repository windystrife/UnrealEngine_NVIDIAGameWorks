/* Copyright 2017 Google Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GoogleVRLaserVisual.h"
#include "Components/MaterialBillboardComponent.h"
#include "GoogleVRLaserPlaneComponent.h"
#include "GoogleVRLaserVisualComponent.generated.h"

UCLASS( ClassGroup=(GoogleVRController), meta=(BlueprintSpawnableComponent) )
class GOOGLEVRCONTROLLER_API UGoogleVRLaserVisualComponent : public UGoogleVRLaserVisual
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGoogleVRLaserVisualComponent();

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnRegister() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Static mesh used to represent the laser. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
	UStaticMesh* LaserPlaneMesh;

	/** Material used for the reticle billboard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	UMaterialInterface* ControllerReticleMaterial;

	/** TranslucentSortPriority to use when rendering.
	*  The reticle, the laser, and the controller mesh use TranslucentSortPriority.
	*  The controller touch point mesh uses TranslucentSortPriority+1, this makes sure that
	*  the touch point doesn't z-fight with the controller mesh.
	**/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	int TranslucentSortPriority;

	/** Maximum distance of the pointer (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
	float LaserDistanceMax;

	/** Minimum distance of the reticle (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleDistanceMin;

	/** Maximum distance of the reticle (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleDistanceMax;

	/** Size of the reticle (in meters) as seen from 1 meter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reticle")
	float ReticleSize;

	/** Get the MaterialBillboardComponent used to represent the reticle.
	*  Can be used if you desire to modify the reticle at runtime
	*  @return reticle billboard component.
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRMotionController", meta = (Keywords = "Cardboard AVR GVR"))
	UMaterialBillboardComponent* GetReticle() const;

	/** Get the StaticMeshComponent used to represent the laser.
	*  Can be used if you desire to modify the laser at runtime
	*  @return laser static mesh component.
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRMotionController", meta = (Keywords = "Cardboard AVR GVR"))
	UGoogleVRLaserPlaneComponent* GetLaser() const;

	/** Get the MaterialInstanceDynamic used to represent the laser material.
	*  Can be used if you desire to modify the laser at runtime
	*  (i.e. change laser color when pointing at object).
	*  @return laser dynamic material instance.
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRMotionController", meta = (Keywords = "Cardboard AVR GVR"))
	UMaterialInstanceDynamic* GetLaserMaterial() const;

	/** Set the distance of the pointer.
	*  This will update the distance of the laser and the reticle
	*  based upon the min and max distances.
	*  @param Distance - new distance
	*/
	UFUNCTION(BlueprintCallable, Category = "GoogleVRMotionController", meta = (Keywords = "Cardboard AVR GVR"))
	void SetPointerDistance(float Distance, float WorldToMetersScale, FVector CameraLocation);

	float GetMaxPointerDistance(float WorldToMetersScale) const;

	void SetDefaultLaserDistance(float WorldToMetersScale);
	void UpdateLaserDistance(float Distance);
	void UpdateLaserCorrection(FVector Correction);

	FMaterialSpriteElement* GetReticleSprite() const;
	float GetReticleSize();
	void SetDefaultReticleDistance(float WorldToMetersScale, FVector CameraLocation);
	void UpdateReticleLocation(FVector Location, FVector OriginLocation, float WorldToMetersScale, FVector CameraLocation);

	void SetSubComponentsEnabled(bool bNewEnabled);

private:

	UGoogleVRLaserPlaneComponent* LaserPlaneComponent;
	UMaterialBillboardComponent* ReticleBillboardComponent;

	void UpdateReticleDistance(float Distance, float WorldToMetersScale, FVector CameraLocation);
	void UpdateReticleSize(FVector CameraLocation);
};
