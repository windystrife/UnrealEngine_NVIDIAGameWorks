// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "PhysicsPublic.h"
#include "PhysXPublic.h"
#include "Modules/ModuleManager.h"
#include "IPhysXVehiclesPlugin.h"
#include "WheeledVehicleMovementComponent.h"
#include "PhysXVehicleManager.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"

class FPhysXVehiclesPlugin : public IPhysXVehiclesPlugin
{
private:

	void UpdatePhysXMaterial(UPhysicalMaterial* PhysicalMaterial)
	{
		FPhysXVehicleManager::UpdateTireFrictionTable();
	}

	void PhysicsAssetChanged(const UPhysicsAsset* InPhysAsset)
	{
		for (FObjectIterator Iter(UWheeledVehicleMovementComponent::StaticClass()); Iter; ++Iter)
		{
			UWheeledVehicleMovementComponent * WheeledVehicleMovementComponent = Cast<UWheeledVehicleMovementComponent>(*Iter);
			if (USkeletalMeshComponent * SkeltalMeshComponent = Cast<USkeletalMeshComponent>(WheeledVehicleMovementComponent->UpdatedComponent))
			{
				if (SkeltalMeshComponent->GetPhysicsAsset() == InPhysAsset)
				{
					//Need to recreate car data
					WheeledVehicleMovementComponent->RecreatePhysicsState();
				}

			}
		}
	}

	void PhysSceneInit(FPhysScene* PhysScene, EPhysicsSceneType SceneType)
	{
		// Only create PhysXVehicleManager in the sync scene
		if (SceneType == PST_Sync)
		{
			new FPhysXVehicleManager(PhysScene, SceneType);
		}
	}

	void PhysSceneTerm(FPhysScene* PhysScene, EPhysicsSceneType SceneType)
	{
		if (SceneType == PST_Sync)
		{
			FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene);
			if (VehicleManager != nullptr)
			{
				VehicleManager->DetachFromPhysScene(PhysScene);
				delete VehicleManager;
				VehicleManager = nullptr;
			}
		}
	}

	FDelegateHandle OnUpdatePhysXMaterialHandle;
	FDelegateHandle OnPhysicsAssetChangedHandle;
	FDelegateHandle OnPhysSceneInitHandle;
	FDelegateHandle OnPhysSceneTermHandle;

public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		PxInitVehicleSDK(*GPhysXSDK);

		OnUpdatePhysXMaterialHandle = FPhysicsDelegates::OnUpdatePhysXMaterial.AddRaw(this, &FPhysXVehiclesPlugin::UpdatePhysXMaterial);
		OnPhysicsAssetChangedHandle = FPhysicsDelegates::OnPhysicsAssetChanged.AddRaw(this, &FPhysXVehiclesPlugin::PhysicsAssetChanged);
		OnPhysSceneInitHandle = FPhysicsDelegates::OnPhysSceneInit.AddRaw(this, &FPhysXVehiclesPlugin::PhysSceneInit);
		OnPhysSceneTermHandle = FPhysicsDelegates::OnPhysSceneTerm.AddRaw(this, &FPhysXVehiclesPlugin::PhysSceneTerm);
	}

	virtual void ShutdownModule() override
	{
		FPhysicsDelegates::OnUpdatePhysXMaterial.Remove(OnUpdatePhysXMaterialHandle);
		FPhysicsDelegates::OnPhysicsAssetChanged.Remove(OnPhysicsAssetChangedHandle);
		FPhysicsDelegates::OnPhysSceneInit.Remove(OnPhysSceneInitHandle);
		FPhysicsDelegates::OnPhysSceneTerm.Remove(OnPhysSceneTermHandle);

		if (GPhysXSDK != NULL)
		{
			PxCloseVehicleSDK();
		}
	}
};

IMPLEMENT_MODULE(FPhysXVehiclesPlugin, PhysXVehicles )







