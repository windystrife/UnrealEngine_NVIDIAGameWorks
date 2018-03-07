// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*
 * Base VehicleSim for the 4W PhysX vehicle class
 */
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "WheeledVehicleMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "WheeledVehicleMovementComponent4W.generated.h"

UENUM()
namespace EVehicleDifferential4W
{
	enum Type
	{
		LimitedSlip_4W,
		LimitedSlip_FrontDrive,
		LimitedSlip_RearDrive,
		Open_4W,
		Open_FrontDrive,
		Open_RearDrive,
	};
}

USTRUCT()
struct FVehicleDifferential4WData
{
	GENERATED_USTRUCT_BODY()

	/** Type of differential */
	UPROPERTY(EditAnywhere, Category=Setup)
	TEnumAsByte<EVehicleDifferential4W::Type> DifferentialType;
	
	/** Ratio of torque split between front and rear (>0.5 means more to front, <0.5 means more to rear, works only with 4W type) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float FrontRearSplit;

	/** Ratio of torque split between front-left and front-right (>0.5 means more to front-left, <0.5 means more to front-right, works only with 4W and LimitedSlip_FrontDrive) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float FrontLeftRightSplit;

	/** Ratio of torque split between rear-left and rear-right (>0.5 means more to rear-left, <0.5 means more to rear-right, works only with 4W and LimitedSlip_RearDrive) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float RearLeftRightSplit;
	
	/** Maximum allowed ratio of average front wheel rotation speed and rear wheel rotation speeds (range: 1..inf, works only with LimitedSlip_4W) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CentreBias;

	/** Maximum allowed ratio of front-left and front-right wheel rotation speeds (range: 1..inf, works only with LimitedSlip_4W, LimitedSlip_FrontDrive) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float FrontBias;

	/** Maximum allowed ratio of rear-left and rear-right wheel rotation speeds (range: 1..inf, works only with LimitedSlip_4W, LimitedSlip_FrontDrive) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "1.0", UIMin = "1.0"))
	float RearBias;
};

USTRUCT()
struct FVehicleEngineData
{
	GENERATED_USTRUCT_BODY()

	/** Torque (Nm) at a given RPM*/
	UPROPERTY(EditAnywhere, Category = Setup)
	FRuntimeFloatCurve TorqueCurve;

	/** Maximum revolutions per minute of the engine */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MaxRPM;

	/** Moment of inertia of the engine around the axis of rotation (Kgm^2). */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float MOI;

	/** Damping rate of engine when full throttle is applied (Kgm^2/s) */
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DampingRateFullThrottle;

	/** Damping rate of engine in at zero throttle when the clutch is engaged (Kgm^2/s)*/
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DampingRateZeroThrottleClutchEngaged;

	/** Damping rate of engine in at zero throttle when the clutch is disengaged (in neutral gear) (Kgm^2/s)*/
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DampingRateZeroThrottleClutchDisengaged;

	/** Find the peak torque produced by the TorqueCurve */
	float FindPeakTorque() const;
};


USTRUCT()
struct FVehicleGearData
{
	GENERATED_USTRUCT_BODY()

	/** Determines the amount of torque multiplication*/
	UPROPERTY(EditAnywhere, Category = Setup)
	float Ratio;

	/** Value of engineRevs/maxEngineRevs that is low enough to gear down*/
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Setup)
	float DownRatio;

	/** Value of engineRevs/maxEngineRevs that is high enough to gear up*/
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Setup)
	float UpRatio;
};

USTRUCT()
struct FVehicleTransmissionData
{
	GENERATED_USTRUCT_BODY()
	/** Whether to use automatic transmission */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta=(DisplayName = "Automatic Transmission"))
	bool bUseGearAutoBox;

	/** Time it takes to switch gears (seconds) */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GearSwitchTime;
	
	/** Minimum time it takes the automatic transmission to initiate a gear change (seconds)*/
	UPROPERTY(EditAnywhere, Category = Setup, meta = (editcondition = "bUseGearAutoBox", ClampMin = "0.0", UIMin="0.0"))
	float GearAutoBoxLatency;

	/** The final gear ratio multiplies the transmission gear ratios.*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	float FinalRatio;

	/** Forward gear ratios (up to 30) */
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay)
	TArray<FVehicleGearData> ForwardGears;

	/** Reverse gear ratio */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	float ReverseGearRatio;

	/** Value of engineRevs/maxEngineRevs that is high enough to increment gear*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float NeutralGearUpRatio;

	/** Strength of clutch (Kgm^2/s)*/
	UPROPERTY(EditAnywhere, Category = Setup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ClutchStrength;
};

UCLASS(meta = (BlueprintSpawnableComponent), hidecategories = (PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class PHYSXVEHICLES_API UWheeledVehicleMovementComponent4W : public UWheeledVehicleMovementComponent
{
	GENERATED_UCLASS_BODY()

	/** Engine */
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
	FVehicleEngineData EngineSetup;

	/** Differential */
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
	FVehicleDifferential4WData DifferentialSetup;

	/** Transmission data */
	UPROPERTY(EditAnywhere, Category = MechanicalSetup)
	FVehicleTransmissionData TransmissionSetup;

	/** Maximum steering versus forward speed (km/h) */
	UPROPERTY(EditAnywhere, Category = SteeringSetup)
	FRuntimeFloatCurve SteeringCurve;

	/** Accuracy of Ackermann steer calculation (range: 0..1) */
	UPROPERTY(EditAnywhere, Category = SteeringSetup, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float AckermannAccuracy;


	virtual void Serialize(FArchive & Ar) override;
	virtual void ComputeConstants() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:


	/** Allocate and setup the PhysX vehicle */
	virtual void SetupVehicleDrive(physx::PxVehicleWheelsSimData* PWheelsSimData) override;

	virtual void UpdateSimulation(float DeltaTime) override;


	/** update simulation data: engine */
	void UpdateEngineSetup(const FVehicleEngineData& NewEngineSetup);

	/** update simulation data: differential */
	void UpdateDifferentialSetup(const FVehicleDifferential4WData& NewDifferentialSetup);

	/** update simulation data: transmission */
	void UpdateTransmissionSetup(const FVehicleTransmissionData& NewGearSetup);
};

