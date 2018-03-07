// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "AI/RVOAvoidanceInterface.h"
#include "GameFramework/PawnMovementComponent.h"
#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.generated.h"

class UCanvas;

namespace physx
{
	class PxVehicleDrive;
	class PxVehicleWheels;
	class PxVehicleWheelsSimData;
	class PxRigidBody;
}

struct FBodyInstance;

/**
 * Values passed from PhysX to generate tire forces
 */
struct FTireShaderInput
{
	// Friction value of the tire contact.
	float TireFriction;

	// Longitudinal slip of the tire
	float LongSlip;

	// Lateral slip of the tire.
	float LatSlip;

	// Rotational speed of the wheel, in radians.
	float WheelOmega;

	// The distance from the tire surface to the center of the wheel.
	float WheelRadius;

	// 1 / WheelRadius
	float RecipWheelRadius;

	// How much force (weight) is pushing on the tire when the vehicle is at rest.
	float RestTireLoad;

	// How much force (weight) is pushing on the tire right now.
	float TireLoad;

	// RestTireLoad / TireLoad
	float NormalizedTireLoad;

	// Acceleration due to gravity
	float Gravity;

	// 1 / Gravity
	float RecipGravity;
};

/**
 * Generated tire forces to pass back to PhysX
 */
struct FTireShaderOutput
{
	// The torque to be applied to the wheel around the wheel axle. Opposes the engine torque on the wheel
	float WheelTorque;

	// The magnitude of the longitudinal tire force to be applied to the vehicle's rigid body.
	float LongForce;

	// The magnitude of the lateral tire force to be applied to the vehicle's rigid body.
	float LatForce;

	FTireShaderOutput() {}

	FTireShaderOutput(float f)
		: WheelTorque(f)
		, LongForce(f)
		, LatForce(f)
	{
	}
};

/**
 * Vehicle-specific wheel setup
 */
USTRUCT()
struct PHYSXVEHICLES_API FWheelSetup
{
	GENERATED_USTRUCT_BODY()

	// The wheel class to use
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	TSubclassOf<UVehicleWheel> WheelClass;

	// Bone name on mesh to create wheel at
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	FName BoneName;

	// Additional offset to give the wheels for this axle.
	UPROPERTY(EditAnywhere, Category=WheelSetup)
	FVector AdditionalOffset;

	FWheelSetup();
};

USTRUCT()
struct PHYSXVEHICLES_API FReplicatedVehicleState
{
	GENERATED_USTRUCT_BODY()

	// input replication: steering
	UPROPERTY()
	float SteeringInput;

	// input replication: throttle
	UPROPERTY()
	float ThrottleInput;

	// input replication: brake
	UPROPERTY()
	float BrakeInput;

	// input replication: handbrake
	UPROPERTY()
	float HandbrakeInput;

	// state replication: current gear
	UPROPERTY()
	int32 CurrentGear;
};

USTRUCT()
struct PHYSXVEHICLES_API FVehicleInputRate
{
	GENERATED_USTRUCT_BODY()

	// Rate at which the input value rises
	UPROPERTY(EditAnywhere, Category=VehicleInputRate)
	float RiseRate;

	// Rate at which the input value falls
	UPROPERTY(EditAnywhere, Category=VehicleInputRate)
	float FallRate;

	FVehicleInputRate() : RiseRate(5.0f), FallRate(5.0f) { }

	/** Change an output value using max rise and fall rates */
	float InterpInputValue( float DeltaTime, float CurrentValue, float NewValue ) const
	{
		const float DeltaValue = NewValue - CurrentValue;

		// We are "rising" when DeltaValue has the same sign as CurrentValue (i.e. delta causes an absolute magnitude gain)
		// OR we were at 0 before, and our delta is no longer 0.
		const bool bRising = (( DeltaValue > 0.0f ) == ( CurrentValue > 0.0f )) ||
								(( DeltaValue != 0.f ) && ( CurrentValue == 0.f ));

		const float MaxDeltaValue = DeltaTime * ( bRising ? RiseRate : FallRate );
		const float ClampedDeltaValue = FMath::Clamp( DeltaValue, -MaxDeltaValue, MaxDeltaValue );
		return CurrentValue + ClampedDeltaValue;
	}
};

/**
 * Component to handle the vehicle simulation for an actor.
 */
UCLASS(Abstract, hidecategories=(PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class PHYSXVEHICLES_API UWheeledVehicleMovementComponent : public UPawnMovementComponent, public IRVOAvoidanceInterface
{
	GENERATED_UCLASS_BODY()

	// Supports the old (before 4.14) way of applying spring forces. We used to offset from the vehicle center of mass instead of the spring location center of mass. You should only use this for existing content that hasn't been re-tuned
	UPROPERTY(EditAnywhere, Category = VehicleSetup)
	uint8 bDeprecatedSpringOffsetMode : 1;

	/** Wheels to create */
	UPROPERTY(EditAnywhere, Category=VehicleSetup)
	TArray<FWheelSetup> WheelSetups;

	/** Mass to set the vehicle chassis to. It's much easier to tweak vehicle settings when
	 * the mass doesn't change due to tweaks with the physics asset. [kg] */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float Mass;

	/** DragCoefficient of the vehicle chassis. */
	UPROPERTY(EditAnywhere, Category = VehicleSetup)
	float DragCoefficient;

	/** Chassis width used for drag force computation (cm)*/
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ChassisWidth;

	/** Chassis height used for drag force computation (cm)*/
	UPROPERTY(EditAnywhere, Category = VehicleSetup, meta = (ClampMin = "0.01", UIMin = "0.01"))
	float ChassisHeight;

	/** If true, the brake and reverse controls will behave in a more arcade fashion where holding reverse also functions as brake. For a more realistic approach turn this off*/
	UPROPERTY(EditAnywhere, Category = VehicleSetup)
	bool bReverseAsBrake;

	// Drag area in cm^2
	UPROPERTY(transient)
	float DragArea;

	// Estimated mad speed for engine
	UPROPERTY(transient)
	float EstimatedMaxEngineSpeed;

	// Max RPM for engine
	UPROPERTY(transient)
	float MaxEngineRPM;

	// Debug drag magnitude last applied
	UPROPERTY(transient)
	float DebugDragMagnitude;

	// Used for backwards compat to fixup incorrect COM of vehicles

	/** When vehicle is created we want to compute some helper data like drag area, etc.... Derived classes should use this to properly compute things like engine RPM */
	virtual void ComputeConstants();

	/** Scales the vehicle's inertia in each direction (forward, right, up) */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	FVector InertiaTensorScale;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	float MinNormalizedTireLoad;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, AdvancedDisplay)
	float MinNormalizedTireLoadFiltered;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay)
	float MaxNormalizedTireLoad;

	/** Clamp normalized tire load to this value */
	UPROPERTY(EditAnywhere, Category = VehicleSetup, AdvancedDisplay)
	float MaxNormalizedTireLoadFiltered;

    /** PhysX sub-steps
     More sub-steps provides better stability but with greater computational cost.
     Typically, vehicles require more sub-steps at very low forward speeds.
     The threshold longitudinal speed has a default value of 5 metres per second. */
    UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay, meta = (ClampMin = "0.1", UIMin = "1.0", ClampMax = "1000.0", UIMax = "10.0"))
    float ThresholdLongitudinalSpeed;
    
    /** The sub-step count below the threshold longitudinal speed has a default of 3. */
    UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay, meta = (ClampMin = "1", UIMin = "1", ClampMax = "10", UIMax = "5"))
    int32 LowForwardSpeedSubStepCount;
    
    /** The sub-step count above the threshold longitudinal speed has a default of 1. */
    UPROPERTY(EditAnywhere, Category=VehicleSetup, AdvancedDisplay, meta = (ClampMin = "1", UIMin = "1", ClampMax = "10", UIMax = "5"))
    int32 HighForwardSpeedSubStepCount;
    
	// Our instanced wheels
	UPROPERTY(transient, duplicatetransient, BlueprintReadOnly, Category=Vehicle)
	TArray<class UVehicleWheel*> Wheels;

	// The value of PhysXVehicleManager::VehicleSetupTag when this vehicle created its physics state.
	// Used to recreate the physics if the blueprint changes.
	uint32 VehicleSetupTag;

	bool CheckSlipThreshold(float AbsLongSlipThreshold, float AbsLatSlipThreshold) const;
	float GetMaxSpringForce() const;

	/** UObject interface */
	virtual void Serialize(FArchive& Ar) override;
	/** End UObject interface*/


	// The instanced PhysX vehicle
	physx::PxVehicleWheels* PVehicle;
	physx::PxVehicleDrive* PVehicleDrive;

	/** Overridden to allow registration with components NOT owned by a Pawn. */
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	
	/** Compute the forces generates from a spinning tire */
	virtual void GenerateTireForces(class UVehicleWheel* Wheel, const FTireShaderInput& Input, FTireShaderOutput& Output);

	/** Return true if we are ready to create a vehicle */
	virtual bool CanCreateVehicle() const;

	/** Create and setup the physx vehicle */
	virtual void CreateVehicle();

	/** Tick this vehicle sim right before input is sent to the vehicle system  */
	virtual void TickVehicle( float DeltaTime );

	/** Updates the vehicle tuning and other state such as user input. */
	virtual void PreTick(float DeltaTime);

	/** Updates the forces of drag acting on the vehicle */
	virtual void UpdateDrag( float DeltaTime );

	/** Used to create any physics engine information for this component */
	virtual void OnCreatePhysicsState() override;

	/** Used to shut down and pysics engine structure for this component */
	virtual void OnDestroyPhysicsState() override;

	virtual bool ShouldCreatePhysicsState() const override;

	virtual bool HasValidPhysicsState() const override;

	/** Draw debug text for the wheels and suspension */
	virtual void DrawDebug(UCanvas* Canvas, float& YL, float& YPos);

	/** Draw debug lines for the wheels and suspension */
	virtual void DrawDebugLines();

	/** Skeletal mesh needs some special handling in the vehicle case */
	virtual void FixupSkeletalMesh();

#if WITH_EDITOR
	/** Respond to a property change in editor */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR

	virtual void StopMovementImmediately() override;

	/** Set the user input for the vehicle throttle */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetThrottleInput(float Throttle);

	/** Set the user input for the vehicle Brake */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|WheeledVehicleMovement")
	void SetBrakeInput(float Brake);
	
	/** Set the user input for the vehicle steering */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetSteeringInput(float Steering);

	/** Set the user input for handbrake */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetHandbrakeInput(bool bNewHandbrake);

	/** Set the user input for gear up */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetGearUp(bool bNewGearUp);

	/** Set the user input for gear down */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetGearDown(bool bNewGearDown);

	/** Set the user input for gear (-1 reverse, 0 neutral, 1+ forward)*/
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetTargetGear(int32 GearNum, bool bImmediate);

	/** Set the flag that will be used to select auto-gears */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	void SetUseAutoGears(bool bUseAuto);

	/** How fast the vehicle is moving forward */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetForwardSpeed() const;

	/** Get current engine's rotation speed */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetEngineRotationSpeed() const;

	/** Get current engine's max rotation speed */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	float GetEngineMaxRotationSpeed() const;

	/** Get current gear */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	int32 GetCurrentGear() const;

	/** Get target gear */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	int32 GetTargetGear() const;

	/** Are gears being changed automatically? */
	UFUNCTION(BlueprintCallable, Category="Game|Components|WheeledVehicleMovement")
	bool GetUseAutoGears() const;

	// RVO Avoidance

	/** If set, component will use RVO avoidance */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite)
	uint32 bUseRVOAvoidance : 1;
	
	/** Vehicle Radius to use for RVO avoidance (usually half of vehicle width) */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite)
	float RVOAvoidanceRadius;
	
	/** Vehicle Height to use for RVO avoidance (usually vehicle height) */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite)
	float RVOAvoidanceHeight;
	
	/** Area Radius to consider for RVO avoidance */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite)
	float AvoidanceConsiderationRadius;

	/** Value by which to alter steering per frame based on calculated avoidance */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float RVOSteeringStep;

	/** Value by which to alter throttle per frame based on calculated avoidance */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
	float RVOThrottleStep;
	
	/** calculate RVO avoidance and apply it to current velocity */
	virtual void CalculateAvoidanceVelocity(float DeltaTime);

	/** No default value, for now it's assumed to be valid if GetAvoidanceManager() returns non-NULL. */
	UPROPERTY(Category = "Avoidance", VisibleAnywhere, BlueprintReadOnly, AdvancedDisplay)
	int32 AvoidanceUID;

	/** Moving actor's group mask */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask AvoidanceGroup;

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement", meta = (DeprecatedFunction, DeprecationMessage = "Please use SetAvoidanceGroupMask function instead."))
	void SetAvoidanceGroup(int32 GroupFlags);

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement")
	void SetAvoidanceGroupMask(const FNavAvoidanceMask& GroupMask);

	/** Will avoid other agents if they are in one of specified groups */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask GroupsToAvoid;

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement", meta = (DeprecatedFunction, DeprecationMessage = "Please use SetGroupsToAvoidMask function instead."))
	void SetGroupsToAvoid(int32 GroupFlags);

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement")
	void SetGroupsToAvoidMask(const FNavAvoidanceMask& GroupMask);

	/** Will NOT avoid other agents if they are in one of specified groups, higher priority than GroupsToAvoid */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask GroupsToIgnore;

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement", meta = (DeprecatedFunction, DeprecationMessage = "Please use SetGroupsToIgnoreMask function instead."))
	void SetGroupsToIgnore(int32 GroupFlags);

	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement")
	void SetGroupsToIgnoreMask(const FNavAvoidanceMask& GroupMask);

	/** De facto default value 0.5 (due to that being the default in the avoidance registration function), indicates RVO behavior. */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly)
	float AvoidanceWeight;
	
	/** Temporarily holds launch velocity when pawn is to be launched so it happens at end of movement. */
	UPROPERTY()
	FVector PendingLaunchVelocity;
	
	/** Change avoidance state and register with RVO manager if necessary */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|WheeledVehicleMovement")
	void SetAvoidanceEnabled(bool bEnable);

protected:

	// replicated state of vehicle 
	UPROPERTY(Transient, Replicated)
	FReplicatedVehicleState ReplicatedState;

	// accumulator for RB replication errors 
	float AngErrorAccumulator;

	// What the player has the steering set to. Range -1...1
	UPROPERTY(Transient)
	float RawSteeringInput;

	// What the player has the accelerator set to. Range -1...1
	UPROPERTY(Transient)
	float RawThrottleInput;

	// What the player has the brake set to. Range -1...1
	UPROPERTY(Transient)
	float RawBrakeInput;

	// True if the player is holding the handbrake
	UPROPERTY(Transient)
	uint32 bRawHandbrakeInput : 1;

	// True if the player is holding gear up
	UPROPERTY(Transient)
	uint32 bRawGearUpInput : 1;

	// True if the player is holding gear down
	UPROPERTY(Transient)
	uint32 bRawGearDownInput : 1;

	// Steering output to physics system. Range -1...1
	UPROPERTY(Transient)
	float SteeringInput;

	// Accelerator output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ThrottleInput;

	// Brake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float BrakeInput;

	// Handbrake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float HandbrakeInput;

	// How much to press the brake when the player has release throttle
	UPROPERTY(EditAnywhere, Category=VehicleInput)
	float IdleBrakeInput;

	// Auto-brake when absolute vehicle forward speed is less than this (cm/s)
	UPROPERTY(EditAnywhere, Category=VehicleInput)
	float StopThreshold;

	// Auto-brake when vehicle forward speed is opposite of player input by at least this much (cm/s)
	UPROPERTY(EditAnywhere, Category = VehicleInput)
	float WrongDirectionThreshold;

	// Rate at which input throttle can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate ThrottleInputRate;

	// Rate at which input brake can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate BrakeInputRate;

	// Rate at which input handbrake can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate HandbrakeInputRate;

	// Rate at which input steering can rise and fall
	UPROPERTY(EditAnywhere, Category=VehicleInput, AdvancedDisplay)
	FVehicleInputRate SteeringInputRate;

	/** Compute steering input */
	float CalcSteeringInput();

	/** Compute brake input */
	float CalcBrakeInput();

	/** Compute handbrake input */
	float CalcHandbrakeInput();

	/** Compute throttle input */
	virtual float CalcThrottleInput();

	/**
	* Clear all interpolated inputs to default values.
	* Raw input won't be cleared, the vehicle may resume input based movement next frame.
	*/
	virtual void ClearInput();
	
	/**
	* Clear all raw inputs to default values.
	* Interpolated input won't be cleared, the vehicle will begin interpolating to no input.
	*/
	virtual void ClearRawInput();

	void ClearAllInput()
	{
		ClearRawInput();
		ClearInput();
	}

	/** Updates the COMOffset on the actual body instance */

	/** Read current state for simulation */
	void UpdateState(float DeltaTime);

	/** Pass current state to server */
	UFUNCTION(reliable, server, WithValidation)
	void ServerUpdateState(float InSteeringInput, float InThrottleInput, float InBrakeInput, float InHandbrakeInput, int32 CurrentGear);

	/** Update RVO Avoidance for simulation */
	void UpdateAvoidance(float DeltaTime);
		
	/** called in Tick to update data in RVO avoidance manager */
	void UpdateDefaultAvoidance();
	
	/** lock avoidance velocity */
	void SetAvoidanceVelocityLock(class UAvoidanceManager* Avoidance, float Duration);
	
	/** Was avoidance updated in this frame? */
	UPROPERTY(Transient)
	uint32 bWasAvoidanceUpdated : 1;
	
	/** Calculated avoidance velocity used to adjust steering and throttle */
	FVector AvoidanceVelocity;
	
	/** forced avoidance velocity, used when AvoidanceLockTimer is > 0 */
	FVector AvoidanceLockVelocity;
	
	/** remaining time of avoidance velocity lock */
	float AvoidanceLockTimer;

	/** Handle for delegate registered on mesh component */
	FDelegateHandle MeshOnPhysicsStateChangeHandle;
	
	/** BEGIN IRVOAvoidanceInterface */
	virtual void SetRVOAvoidanceUID(int32 UID) override;
	virtual int32 GetRVOAvoidanceUID() override;
	virtual void SetRVOAvoidanceWeight(float Weight) override;
	virtual float GetRVOAvoidanceWeight() override;
	virtual FVector GetRVOAvoidanceOrigin() override;
	virtual float GetRVOAvoidanceRadius() override;
	virtual float GetRVOAvoidanceHeight() override;
	virtual float GetRVOAvoidanceConsiderationRadius() override;
	virtual FVector GetVelocityForRVOConsideration() override;
	virtual int32 GetAvoidanceGroupMask() override;
	virtual int32 GetGroupsToAvoidMask() override;
	virtual int32 GetGroupsToIgnoreMask() override;
	/** END IRVOAvoidanceInterface */


	int32 GearToPhysXGear(const int32 Gear) const;

	int32 PhysXGearToGear(const int32 PhysXGear) const;

	/** Pass input values to vehicle simulation */
	virtual void UpdateSimulation( float DeltaTime );

	/** Allocate and setup the PhysX vehicle */
	virtual void SetupVehicle();

	/** Create the specific vehicle drive (4w drive vs tank etc...)*/
	virtual void SetupVehicleDrive(physx::PxVehicleWheelsSimData* PWheelsSimData);

	/** Do some final setup after the physx vehicle gets created */
	virtual void PostSetupVehicle();

	/** Set up the chassis and wheel shapes */
	virtual void SetupVehicleShapes();

	/** Adjust the PhysX actor's mass */
	virtual void SetupVehicleMass();

	virtual void SetupWheelMassProperties_AssumesLocked(const uint32 NumWheels, physx::PxVehicleWheelsSimData* PWheelsSimData, physx::PxRigidBody* PVehicleActor);

	/** Set up the wheel data */
	virtual void SetupWheels(physx::PxVehicleWheelsSimData* PWheelsSimData);

	/** Instantiate and setup our wheel objects */
	virtual void CreateWheels();

	/** Release our wheel objects */
	virtual void DestroyWheels();

	/** Get the local position of the wheel at rest */
	virtual FVector GetWheelRestingPosition(const FWheelSetup& WheelSetup);

	/** Get the local COM */
	virtual FVector GetLocalCOM() const;

	/** Get the mesh this vehicle is tied to */
	class USkinnedMeshComponent* GetMesh();

	void UpdateMassProperties(FBodyInstance* BI);
	

};

//some helper functions for converting units

//rev per minute to rad/s
inline float RPMToOmega(float RPM)
{
	return RPM * PI / 30.f;
}

//rad/s to rev per minute
inline float OmegaToRPM(float Omega)
{
	return Omega * 30.f / PI;
}

//km/h to cm/s
inline float KmHToCmS(float KmH)
{
	return KmH * 100000.f / 3600.f;
}

inline float CmSToKmH(float CmS)
{
	return CmS * 3600.f / 100000.f;
}

inline float M2ToCm2(float M2)
{
	return M2 * 100.f * 100.f;
}

inline float Cm2ToM2(float Cm2)
{
	return Cm2 / (100.f * 100.f);
}