// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysXVehicleManager.h"
#include "UObject/UObjectIterator.h"
#include "TireConfig.h"

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Physics/PhysicsFiltering.h"
#include "PhysXPublic.h"

DEFINE_LOG_CATEGORY(LogVehicles);

DECLARE_STATS_GROUP(TEXT("PhysXVehicleManager"), STATGROUP_PhysXVehicleManager, STATGROUP_Advanced);
DECLARE_CYCLE_STAT(TEXT("PxVehicleSuspensionRaycasts"), STAT_PhysXVehicleManager_PxVehicleSuspensionRaycasts, STATGROUP_PhysXVehicleManager);
DECLARE_CYCLE_STAT(TEXT("PxUpdateVehicles"), STAT_PhysXVehicleManager_PxUpdateVehicles, STATGROUP_PhysXVehicleManager);
DECLARE_CYCLE_STAT(TEXT("UpdateTireFrictionTable"), STAT_PhysXVehicleManager_UpdateTireFrictionTable, STATGROUP_PhysXVehicleManager);
DECLARE_CYCLE_STAT(TEXT("TickVehicles"), STAT_PhysXVehicleManager_TickVehicles, STATGROUP_PhysXVehicleManager);
DECLARE_CYCLE_STAT(TEXT("VehicleManager Update"), STAT_PhysXVehicleManager_Update, STATGROUP_PhysXVehicleManager);
DECLARE_CYCLE_STAT(TEXT("Pretick Vehicles"), STAT_PhysXVehicleManager_PretickVehicles, STATGROUP_Physics);

bool FPhysXVehicleManager::bUpdateTireFrictionTable = false;
PxVehicleDrivableSurfaceToTireFrictionPairs* FPhysXVehicleManager::SurfaceTirePairs = NULL;
TMap<FPhysScene*, FPhysXVehicleManager*> FPhysXVehicleManager::SceneToVehicleManagerMap;
uint32 FPhysXVehicleManager::VehicleSetupTag = 0;

/**
 * prefilter shader for suspension raycasts
 */
static PxQueryHitType::Enum WheelRaycastPreFilter(	
	PxFilterData SuspensionData, 
	PxFilterData HitData,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& filterFlags)
{
	// SuspensionData is the vehicle suspension raycast.
	// HitData is the shape potentially hit by the raycast.

	// don't collide with owner chassis
	if ( SuspensionData.word0 == HitData.word0 )
	{
		return PxQueryHitType::eNONE;
	}
	
	PxU32 ShapeFlags = SuspensionData.word3 & 0xFFFFFF;
	PxU32 QuerierFlags = HitData.word3 & 0xFFFFFF;
	PxU32 CommonFlags = ShapeFlags & QuerierFlags;

	// Check complexity matches
	if (!(CommonFlags & EPDF_SimpleCollision) && !(CommonFlags & EPDF_ComplexCollision))
	{
		return PxQueryHitType::eNONE;
	}

	// collision channels filter
	ECollisionChannel SuspensionChannel = GetCollisionChannel(SuspensionData.word3);

	if ( ECC_TO_BITFIELD(SuspensionChannel) & HitData.word1)
	{
		// debug what object we hit
		if ( false )
		{
			for ( FObjectIterator It; It; ++It )
			{
				if ( It->GetUniqueID() == HitData.word0 )
				{
					UObject* HitObj = *It;
					FString HitObjName = HitObj->GetName();
					break;
				}
			}
		}

		return PxQueryHitType::eBLOCK;
	}

	return PxQueryHitType::eNONE;
}

FPhysXVehicleManager::FPhysXVehicleManager(FPhysScene* PhysScene, uint32 SceneType)
	: WheelRaycastBatchQuery(NULL)

#if PX_DEBUG_VEHICLE_ON
	, TelemetryData4W(NULL)
	, TelemetryVehicle(NULL)
#endif
{
	// Save pointer to PhysX scene
	Scene = PhysScene->GetPhysXScene(SceneType);

	// Set up delegates
	OnPhysScenePreTickHandle = PhysScene->OnPhysScenePreTick.AddRaw(this, &FPhysXVehicleManager::PreTick);
	OnPhysSceneStepHandle = PhysScene->OnPhysSceneStep.AddRaw(this, &FPhysXVehicleManager::Update);

	// Add to map
	FPhysXVehicleManager::SceneToVehicleManagerMap.Add(PhysScene, this);


	// Set the correct basis vectors with Z up, X forward. It's very IMPORTANT to set the Ackermann axle separation and frontWidth, rearWidth accordingly
	PxVehicleSetBasisVectors( PxVec3(0,0,1), PxVec3(1,0,0) );
}

void FPhysXVehicleManager::DetachFromPhysScene(FPhysScene* PhysScene)
{
	PhysScene->OnPhysScenePreTick.Remove(OnPhysScenePreTickHandle);
	PhysScene->OnPhysSceneStep.Remove(OnPhysSceneStepHandle);

	FPhysXVehicleManager::SceneToVehicleManagerMap.Remove(PhysScene);
}

FPhysXVehicleManager::~FPhysXVehicleManager()
{
#if PX_DEBUG_VEHICLE_ON
	if(TelemetryData4W)
	{
		TelemetryData4W->free();
		TelemetryData4W = NULL;
	}

	TelemetryVehicle = NULL;
#endif

	// Remove the N-wheeled vehicles.
	while( Vehicles.Num() > 0 )
	{
		RemoveVehicle( Vehicles.Last() );
	}

	// Release batch query data
	if ( WheelRaycastBatchQuery )
	{
		WheelRaycastBatchQuery->release();
		WheelRaycastBatchQuery = NULL;
	}

	// Release the  friction values used for combinations of tire type and surface type.
	//if ( SurfaceTirePairs )
	//{
	//	SurfaceTirePairs->release();
	//	SurfaceTirePairs = NULL;
	//}
}

FPhysXVehicleManager* FPhysXVehicleManager::GetVehicleManagerFromScene(FPhysScene* PhysScene)
{
	FPhysXVehicleManager* Manager = nullptr;
	FPhysXVehicleManager** ManagerPtr = SceneToVehicleManagerMap.Find(PhysScene);
	if (ManagerPtr != nullptr)
	{
		Manager = *ManagerPtr;
	}
	return Manager;
}

static UTireConfig* DefaultTireConfig = nullptr;

UTireConfig* FPhysXVehicleManager::GetDefaultTireConfig()
{
	if (DefaultTireConfig == nullptr)
	{
		DefaultTireConfig = NewObject<UTireConfig>();
		DefaultTireConfig->AddToRoot(); // prevent GC
	}

	return DefaultTireConfig;
}

void FPhysXVehicleManager::UpdateTireFrictionTable()
{
	bUpdateTireFrictionTable = true;
}

void FPhysXVehicleManager::UpdateTireFrictionTableInternal()
{
	const PxU32 MAX_NUM_MATERIALS = 128;

	// There are tire types and then there are drivable surface types.
	// PhysX supports physical materials that share a drivable surface type,
	// but we just create a drivable surface type for every type of physical material
	PxMaterial*							AllPhysicsMaterials[MAX_NUM_MATERIALS];
	PxVehicleDrivableSurfaceType		DrivableSurfaceTypes[MAX_NUM_MATERIALS];

	// Gather all the physical materials
	uint32 NumMaterials = GPhysXSDK->getMaterials(AllPhysicsMaterials, MAX_NUM_MATERIALS);

	uint32 NumTireConfigs = UTireConfig::AllTireConfigs.Num();

	for ( uint32 m = 0; m < NumMaterials; ++m )
	{
		// Set up the drivable surface type that will be used for the new material.
		DrivableSurfaceTypes[m].mType = m;
	}

	// Release the previous SurfaceTirePairs, if any
	if ( SurfaceTirePairs )
	{
		SurfaceTirePairs->release();
		SurfaceTirePairs = NULL;
	}

	// Set up the friction values arising from combinations of tire type and surface type.
	SurfaceTirePairs = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate( NumTireConfigs, NumMaterials );
	SurfaceTirePairs->setup(NumTireConfigs, NumMaterials, (const PxMaterial**)AllPhysicsMaterials, DrivableSurfaceTypes );

	// Iterate over each physical material
	for ( uint32 m = 0; m < NumMaterials; ++m )
	{
		UPhysicalMaterial* PhysMat = FPhysxUserData::Get<UPhysicalMaterial>(AllPhysicsMaterials[m]->userData);
		if (PhysMat != nullptr)
		{
			// Iterate over each tire config
			for (uint32 t = 0; t < NumTireConfigs; ++t)
		{
				UTireConfig* TireConfig = UTireConfig::AllTireConfigs[t].Get();
				if (TireConfig != nullptr)
			{
					float TireFriction = TireConfig->GetTireFriction(PhysMat);
					SurfaceTirePairs->setTypePairFriction(m, t, TireFriction);
			}
			}
		}
	}
}

void FPhysXVehicleManager::SetUpBatchedSceneQuery()
{
	int32 NumWheels = 0;

	for ( int32 v = PVehicles.Num() - 1; v >= 0; --v )
	{
		NumWheels += PVehicles[v]->mWheelsSimData.getNbWheels();
	}

	if ( NumWheels > WheelQueryResults.Num() )
	{
		WheelQueryResults.AddZeroed( NumWheels - WheelQueryResults.Num() );
		WheelHitResults.AddZeroed( NumWheels - WheelHitResults.Num() );

		check( WheelHitResults.Num() == WheelQueryResults.Num() );

		if ( WheelRaycastBatchQuery )
		{
			WheelRaycastBatchQuery->release();
			WheelRaycastBatchQuery = NULL;
		}
		
		PxBatchQueryDesc SqDesc(NumWheels, 0, 0);
		SqDesc.queryMemory.userRaycastResultBuffer = WheelQueryResults.GetData();
		SqDesc.queryMemory.userRaycastTouchBuffer = WheelHitResults.GetData();
		SqDesc.queryMemory.raycastTouchBufferSize = WheelHitResults.Num();
		SqDesc.preFilterShader = WheelRaycastPreFilter;

		WheelRaycastBatchQuery = Scene->createBatchQuery( SqDesc );
	}
}

void FPhysXVehicleManager::AddVehicle( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle )
{
	check(Vehicle != NULL);
	check(Vehicle->PVehicle);

	Vehicles.Add( Vehicle );
	PVehicles.Add( Vehicle->PVehicle );

	// init wheels' states
	int32 NewIndex = PVehiclesWheelsStates.AddZeroed();
	PxU32 NumWheels = Vehicle->PVehicle->mWheelsSimData.getNbWheels();
	PVehiclesWheelsStates[NewIndex].nbWheelQueryResults = NumWheels;
	PVehiclesWheelsStates[NewIndex].wheelQueryResults = new PxWheelQueryResult[NumWheels];  

	SetUpBatchedSceneQuery();
}

void FPhysXVehicleManager::RemoveVehicle( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle )
{
	check(Vehicle != NULL);
	check(Vehicle->PVehicle);

	PxVehicleWheels* PVehicle = Vehicle->PVehicle;

	int32 RemovedIndex = Vehicles.Find(Vehicle);

	Vehicles.Remove( Vehicle );
	PVehicles.Remove( PVehicle );

	delete[] PVehiclesWheelsStates[RemovedIndex].wheelQueryResults;
	PVehiclesWheelsStates.RemoveAt(RemovedIndex); // LOC_MOD double check this
	//PVehiclesWheelsStates.Remove(PVehiclesWheelsStates[RemovedIndex]);

	if ( PVehicle == TelemetryVehicle )
	{
		TelemetryVehicle = NULL;
	}

	switch( PVehicle->getVehicleType() )
	{
	case PxVehicleTypes::eDRIVE4W:
		((PxVehicleDrive4W*)PVehicle)->free();
		break;
	case PxVehicleTypes::eDRIVETANK:
		((PxVehicleDriveTank*)PVehicle)->free();
		break;
	case PxVehicleTypes::eDRIVENW:
		((PxVehicleDriveNW*)PVehicle)->free();
		break;
	case PxVehicleTypes::eNODRIVE:
		((PxVehicleNoDrive*)PVehicle)->free();
		break;
	default:
		checkf( 0, TEXT("Unsupported vehicle type"));
		break;
	}
}

void FPhysXVehicleManager::Update(FPhysScene* PhysScene, uint32 SceneType, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_Update);

	// Only support vehicles in sync scene
	if (SceneType != PST_Sync || Vehicles.Num() == 0 )
	{
		return;
	}

	if ( bUpdateTireFrictionTable )
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_UpdateTireFrictionTable);
		bUpdateTireFrictionTable = false;
		UpdateTireFrictionTableInternal();
	}

	// Suspension raycasts
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_PxVehicleSuspensionRaycasts);
		SCOPED_SCENE_READ_LOCK(Scene);
		PxVehicleSuspensionRaycasts( WheelRaycastBatchQuery, PVehicles.Num(), PVehicles.GetData(), WheelQueryResults.Num(), WheelQueryResults.GetData() );
	}
	

	// Tick vehicles
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_TickVehicles);
		for (int32 i = Vehicles.Num() - 1; i >= 0; --i)
		{
			Vehicles[i]->TickVehicle(DeltaTime);
		}
	}

#if PX_DEBUG_VEHICLE_ON

	if ( TelemetryVehicle != NULL )
	{
		UpdateVehiclesWithTelemetry( DeltaTime );
	}
	else
	{
		UpdateVehicles( DeltaTime );
	}

#else

	UpdateVehicles( DeltaTime );

#endif //PX_DEBUG_VEHICLE_ON
}

void FPhysXVehicleManager::PreTick(FPhysScene* PhysScene, uint32 SceneType, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_PretickVehicles);

	// Only support vehicles in sync scene
	if (SceneType == PST_Sync)
{
	for (int32 i = 0; i < Vehicles.Num(); ++i)
	{
		Vehicles[i]->PreTick(DeltaTime);
	}
}
}


void FPhysXVehicleManager::UpdateVehicles( float DeltaTime )
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXVehicleManager_PxUpdateVehicles);
	SCOPED_SCENE_WRITE_LOCK(Scene);
	PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num(), PVehicles.GetData(), PVehiclesWheelsStates.GetData());
}

PxVec3 FPhysXVehicleManager::GetSceneGravity_AssumesLocked()
{
	return Scene->getGravity();
}

void FPhysXVehicleManager::SetRecordTelemetry( TWeakObjectPtr<UWheeledVehicleMovementComponent> Vehicle, bool bRecord )
{
#if PX_DEBUG_VEHICLE_ON

	if ( Vehicle != NULL && Vehicle->PVehicle != NULL )
	{
		PxVehicleWheels* PVehicle = Vehicle->PVehicle;

		if ( bRecord )
		{
			int32 VehicleIndex = Vehicles.Find( Vehicle );

			if ( VehicleIndex != INDEX_NONE )
			{
				// Make sure telemetry is setup
				SetupTelemetryData();

				TelemetryVehicle = PVehicle;

				if ( VehicleIndex != 0 )
				{
					Vehicles.Swap( 0, VehicleIndex );
					PVehicles.Swap( 0, VehicleIndex );
					PVehiclesWheelsStates.Swap( 0, VehicleIndex );
				}
			}
		}
		else
		{
			if ( PVehicle == TelemetryVehicle )
			{
				TelemetryVehicle = NULL;
			}
		}
	}

#endif
}

#if PX_DEBUG_VEHICLE_ON

void FPhysXVehicleManager::SetupTelemetryData()
{
	// set up telemetry for 4 wheels
	if(TelemetryData4W == NULL)
	{
		SCOPED_SCENE_WRITE_LOCK(Scene);

		float Empty[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

		TelemetryData4W = PxVehicleTelemetryData::allocate(4);
		TelemetryData4W->setup(1.0f, 1.0f, 0.0f, 0.0f, Empty, Empty, PxVec3(0,0,0), PxVec3(0,0,0), PxVec3(0,0,0));
	}
}

void FPhysXVehicleManager::UpdateVehiclesWithTelemetry( float DeltaTime )
{
	check(TelemetryVehicle);
	check(PVehicles.Find(TelemetryVehicle) == 0);

	SCOPED_SCENE_WRITE_LOCK(Scene);
	if ( PxVehicleTelemetryData* TelemetryData = GetTelemetryData_AssumesLocked() )
	{
		PxVehicleUpdateSingleVehicleAndStoreTelemetryData( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, TelemetryVehicle, PVehiclesWheelsStates.GetData(), *TelemetryData );

		if ( PVehicles.Num() > 1 )
		{
			PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num() - 1, &PVehicles[1], &PVehiclesWheelsStates[1] );
		}
	}
	else
	{
		UE_LOG( LogPhysics, Warning, TEXT("Cannot record telemetry for vehicle, it does not have 4 wheels") );

		PxVehicleUpdates( DeltaTime, GetSceneGravity_AssumesLocked(), *SurfaceTirePairs, PVehicles.Num(), PVehicles.GetData(), PVehiclesWheelsStates.GetData() );
	}
}

PxVehicleTelemetryData* FPhysXVehicleManager::GetTelemetryData_AssumesLocked()
{
	if ( TelemetryVehicle )
	{
		if ( TelemetryVehicle->mWheelsSimData.getNbWheels() == 4 )
		{
			return TelemetryData4W;
		}
	}

	return NULL;
}

#endif //PX_DEBUG_VEHICLE_ON

PxWheelQueryResult* FPhysXVehicleManager::GetWheelsStates_AssumesLocked(TWeakObjectPtr<class UWheeledVehicleMovementComponent> Vehicle)
{
	int32 Index = Vehicles.Find(Vehicle);

	if(Index != INDEX_NONE)
	{
		return PVehiclesWheelsStates[Index].wheelQueryResults;
	}
	else
	{
		return NULL;
	}
}
