// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*
* Simple n-wheeled vehicle with suspension and tire friction. If you need a motor sim see UWheeledVehicleMovementComponent4W
*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "WheeledVehicleMovementComponent.h"
#include "SimpleWheeledVehicleMovementComponent.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent), hidecategories = (PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class PHYSXVEHICLES_API USimpleWheeledVehicleMovementComponent : public UWheeledVehicleMovementComponent
{
	GENERATED_BODY()
public:
	/**Set the brake torque to be applied to a specific wheel*/
	UFUNCTION(BlueprintCallable, Category = Vehicle)
	void SetBrakeTorque(float BrakeTorque, int32 WheelIndex);

	/**Set the drive torque to be applied to a specific wheel*/
	UFUNCTION(BlueprintCallable, Category = Vehicle)
	void SetDriveTorque(float DriveTorque, int32 WheelIndex);

	/**Set the steer angle (in degrees) to be applied to a specific wheel*/
	UFUNCTION(BlueprintCallable, Category = Vehicle)
	void SetSteerAngle(float SteerAngle, int32 WheelIndex);

protected:

	/** Allocate and setup the PhysX vehicle */
	virtual void SetupVehicleDrive(physx::PxVehicleWheelsSimData* PWheelsSimData) override;
};
