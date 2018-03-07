// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicleMovementComponent.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

class UTireConfig;

DECLARE_LOG_CATEGORY_EXTERN(LogVehicles, Log, All);

/**
 * Manages vehicles and tire surface data for all scenes
 */
class PHYSXVEHICLES_API FPhysXVehicleManager
{
public:

	// Updated when vehicles need to recreate their physics state.
	// Used when designer tweaks values while the game is running.
	static uint32											VehicleSetupTag;

	FPhysXVehicleManager(FPhysScene* PhysScene, uint32 SceneType);
	~FPhysXVehicleManager();

	/**
	 * Refresh the tire friction pairs
	 */
	static void UpdateTireFrictionTable();

	/**
	 * Register a PhysX vehicle for processing
	 */
	void AddVehicle( TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle );

	/**
	 * Unregister a PhysX vehicle from processing
	 */
	void RemoveVehicle( TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle );

	/**
	 * Set the vehicle that we want to record telemetry data for
	 */
	void SetRecordTelemetry( TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle, bool bRecord );

	/**
	 * Get the updated telemetry data
	 */
	PxVehicleTelemetryData* GetTelemetryData_AssumesLocked();
	
	/**
	 * Get a vehicle's wheels states, such as isInAir, suspJounce, contactPoints, etc
	 */
	PxWheelQueryResult* GetWheelsStates_AssumesLocked(TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle);

	/**
	 * Update vehicle data before the scene simulates
	 */
	void Update(FPhysScene* PhysScene, uint32 SceneType, float DeltaTime);
	
	/**
	 * Update vehicle tuning and other state such as input */
	void PreTick(FPhysScene* PhysScene, uint32 SceneType, float DeltaTime);

	/** Detach this vehicle manager from a FPhysScene (remove delegates, remove from map etc) */
	void DetachFromPhysScene(FPhysScene* PhysScene);

	PxScene* GetScene() const { return Scene; }



	/** Find a vehicle manager from an FPhysScene */
	static FPhysXVehicleManager* GetVehicleManagerFromScene(FPhysScene* PhysScene);

	/** Gets a transient default TireConfig object */
	static UTireConfig* GetDefaultTireConfig();

private:

	// True if the tire friction table needs to be updated
	static bool													bUpdateTireFrictionTable;

	// Friction from combinations of tire and surface types.
	static PxVehicleDrivableSurfaceToTireFrictionPairs*			SurfaceTirePairs;

	/** Map of physics scenes to corresponding vehicle manager */
	static TMap<FPhysScene*, FPhysXVehicleManager*>				SceneToVehicleManagerMap;


	// The scene we belong to
	PxScene*													Scene;

	// All instanced vehicles
	TArray<TWeakObjectPtr<class UWheeledVehicleMovementComponent>>			Vehicles;

	// All instanced PhysX vehicles
	TArray<PxVehicleWheels*>									PVehicles;

	// Store each vehicle's wheels' states like isInAir, suspJounce, contactPoints, etc
	TArray<PxVehicleWheelQueryResult>							PVehiclesWheelsStates;

	// Scene query results for each wheel for each vehicle
	TArray<PxRaycastQueryResult>								WheelQueryResults;

	// Scene raycast hits for each wheel for each vehicle
	TArray<PxRaycastHit>										WheelHitResults;

	// Batch query for the wheel suspension raycasts
	PxBatchQuery*												WheelRaycastBatchQuery;

	FDelegateHandle OnPhysScenePreTickHandle;
	FDelegateHandle OnPhysSceneStepHandle;


	/**
	 * Refresh the tire friction pairs
	 */
	void UpdateTireFrictionTableInternal();

	/**
	 * Reallocate the WheelRaycastBatchQuery if our number of wheels has increased
	 */
	void SetUpBatchedSceneQuery();

	/**
	 * Update all vehicles without telemetry
	 */
	void UpdateVehicles( float DeltaTime );

	/**
	 * Get the gravity for our phys scene
	 */
	PxVec3 GetSceneGravity_AssumesLocked();

#if PX_DEBUG_VEHICLE_ON

	PxVehicleTelemetryData*				TelemetryData4W;

	PxVehicleWheels*					TelemetryVehicle;

	/**
	 * Init telemetry data
	 */
	void SetupTelemetryData();

	/**
	 * Update the telemetry vehicle and then all other vehicles
	 */
	void UpdateVehiclesWithTelemetry( float DeltaTime );

#endif //PX_DEBUG_VEHICLE_ON
};
