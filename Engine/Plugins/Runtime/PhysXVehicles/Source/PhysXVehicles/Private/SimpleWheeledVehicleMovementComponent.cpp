// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SimpleWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#include "PhysXVehicleManager.h"

void USimpleWheeledVehicleMovementComponent::SetBrakeTorque(float BrakeTorque, int32 WheelIndex)
{
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [PVehicleLocal, WheelIndex, BrakeTorque](PxRigidDynamic* PRigidDynamic)
			{
				((PxVehicleNoDrive*)PVehicleLocal)->setBrakeTorque(WheelIndex, M2ToCm2(BrakeTorque));
			});
		}
	}
}

void USimpleWheeledVehicleMovementComponent::SetDriveTorque(float DriveTorque, int32 WheelIndex)
{
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [PVehicleLocal, WheelIndex, DriveTorque](PxRigidDynamic* PRigidDynamic)
			{
				((PxVehicleNoDrive*)PVehicleLocal)->setDriveTorque(WheelIndex, M2ToCm2(DriveTorque));
			});
		}
	}
}

void USimpleWheeledVehicleMovementComponent::SetSteerAngle(float SteerAngle, int32 WheelIndex)
{
	if (PVehicle && UpdatedPrimitive)
	{
		PxVehicleWheels* PVehicleLocal = PVehicle;
		if (WheelSetups.IsValidIndex(WheelIndex))
		{
			const float SteerRad = FMath::DegreesToRadians(SteerAngle);
			ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [PVehicleLocal, WheelIndex, SteerRad](PxRigidDynamic* PRigidDynamic)
			{
				((PxVehicleNoDrive*)PVehicleLocal)->setSteerAngle(WheelIndex, SteerRad);
			});
		}
	}
}


void USimpleWheeledVehicleMovementComponent::SetupVehicleDrive(PxVehicleWheelsSimData* PWheelsSimData)
{
	//Use a simple PxNoDrive which will give us suspension but no engine forces which we leave to the user

	// Create the vehicle
	PxVehicleNoDrive* PVehicleNoDrive = PxVehicleNoDrive::allocate(WheelSetups.Num());
	check(PVehicleNoDrive);

	ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [&](PxRigidDynamic* PRigidDynamic)
	{
		PVehicleNoDrive->setup(GPhysXSDK, PRigidDynamic, *PWheelsSimData);
		PVehicleNoDrive->setToRestState();

		// cleanup
		PWheelsSimData->free();
	});

	// cache values
	PVehicle = PVehicleNoDrive;
}

#endif
