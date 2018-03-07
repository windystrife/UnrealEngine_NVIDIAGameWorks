// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApexDestructionModule.h"
#include "Modules/ModuleManager.h"
#include "ApexDestructionCustomPayload.h"
#include "Engine/World.h"
#include "DestructibleComponent.h"

#ifndef APEX_STATICALLY_LINKED
#define APEX_STATICALLY_LINKED	0
#endif

namespace nvidia
{
	namespace apex
	{
		class ModuleDestructible;
	}
}

FApexDestructionSyncActors* FApexDestructionCustomPayload::SingletonCustomSync = nullptr;
nvidia::apex::ModuleDestructible* GApexModuleDestructible = nullptr;
void* ApexDestructibleDLL = nullptr;

#if WITH_APEX
/**
APEX Destructible chunk report interface
This interface delivers summaries (which can be detailed to the single chunk level, depending on the settings)
of chunk fracture and destroy events.
*/
class FApexChunkReport : public nvidia::apex::UserChunkReport
{
public:
	// NxUserChunkReport interface.

	virtual void	onDamageNotify(const nvidia::apex::DamageEventReportData& damageEvent) override;
	

	virtual void	onStateChangeNotify(const nvidia::apex::ChunkStateEventData& visibilityEvent) override
	{
		UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(visibilityEvent.destructible->userData));
		check(DestructibleComponent);

		if (DestructibleComponent->IsPendingKill())	//don't notify if object is being destroyed
		{
			return;
		}

		DestructibleComponent->OnVisibilityEvent(visibilityEvent);
	}

	virtual bool	releaseOnNoChunksVisible(const nvidia::apex::DestructibleActor* destructible) override
	{
		return false;
	}

	virtual void	onDestructibleWake(nvidia::apex::DestructibleActor** destructibles, physx::PxU32 count) override
	{
	}

	virtual void	onDestructibleSleep(nvidia::apex::DestructibleActor** destructibles, physx::PxU32 count) override
	{
	}
}GApexChunkReport;


/**
APEX PhysX3 interface
This interface allows us to modify the PhysX simulation filter shader data with contact pair flags
*/
class FApexPhysX3Interface : public nvidia::apex::PhysX3Interface
{
public:
	// NxApexPhysX3Interface interface.

	virtual void setContactReportFlags(physx::PxShape* PShape, physx::PxPairFlags PFlags, nvidia::apex::DestructibleActor* actor, PxU16 actorChunkIndex) override
	{
		UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(PShape->userData));
		check(DestructibleComponent);

		DestructibleComponent->Pair(actorChunkIndex, PShape);
	}

	virtual physx::PxPairFlags getContactReportFlags(const physx::PxShape* PShape) const override
	{
		PxFilterData FilterData = PShape->getSimulationFilterData();
		return (physx::PxPairFlags)FilterData.word3;
	}
} GPhysX3Interface_ApexDestructionImp;

static TAutoConsoleVariable<int32> CVarAPEXMaxDestructibleDynamicChunkIslandCount(
	TEXT("p.APEXMaxDestructibleDynamicChunkIslandCount"),
	2000,
	TEXT("APEX Max Destructilbe Dynamic Chunk Island Count."),
	ECVF_Default);


static TAutoConsoleVariable<int32> CVarAPEXMaxDestructibleDynamicChunkCount(
	TEXT("p.APEXMaxDestructibleDynamicChunkCount"),
	2000,
	TEXT("APEX Max Destructible dynamic Chunk Count."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAPEXSortDynamicChunksByBenefit(
	TEXT("p.bAPEXSortDynamicChunksByBenefit"),
	1,
	TEXT("True if APEX should sort dynamic chunks by benefit."),
	ECVF_Default);

struct FPendingApexDamageEvent
{
	TWeakObjectPtr<UDestructibleComponent> DestructibleComponent;
	apex::DamageEventReportData DamageEvent;
	TArray<apex::ChunkData> ApexChunkData;

	FPendingApexDamageEvent(UDestructibleComponent* InDestructibleComponent, const apex::DamageEventReportData& InDamageEvent)
		: DestructibleComponent(InDestructibleComponent)
		, DamageEvent(InDamageEvent)
	{
		ApexChunkData.AddUninitialized(InDamageEvent.fractureEventListSize);
		for (uint32 ChunkIdx = 0; ChunkIdx < InDamageEvent.fractureEventListSize; ++ChunkIdx)
		{
			ApexChunkData[ChunkIdx] = InDamageEvent.fractureEventList[ChunkIdx];
		}

		DamageEvent.fractureEventList = ApexChunkData.GetData();
	}

};
#endif

class FApexDestructionModule : public IModuleInterface
{
private:
	FDelegateHandle OnPhysSceneInitHandle;
	FDelegateHandle OnPhysDispatchNotifications;

#if WITH_APEX
	TMap<FPhysScene*, TArray<FPendingApexDamageEvent>> PendingDamageEventsMap;
#endif

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{	
#if WITH_APEX
		ApexDestructibleDLL = PhysDLLHelper::LoadAPEXModule("Apex_Destructible");

#if APEX_STATICALLY_LINKED
		// We need to instantiate the module if we have statically linked them
		// Otherwise all createModule functions will fail
		instantiateModuleDestructible();
#endif

		// Load APEX Destruction module
		GApexModuleDestructible = static_cast<apex::ModuleDestructible*>(GApexSDK->createModule("Destructible"));
		check(GApexModuleDestructible);

		// Set Destructible module parameters
		NvParameterized::Interface* ModuleParams = GApexModuleDestructible->getDefaultModuleDesc();
		// ModuleParams contains the default module descriptor, which may be modified here before calling the module init function
		GApexModuleDestructible->init(*ModuleParams);
		// Set chunk report for fracture effect callbacks
		GApexModuleDestructible->setChunkReport(&GApexChunkReport);


		GApexModuleDestructible->setMaxDynamicChunkIslandCount((physx::PxU32)FMath::Max(CVarAPEXMaxDestructibleDynamicChunkIslandCount.GetValueOnGameThread(), 0));
		GApexModuleDestructible->setMaxChunkCount((physx::PxU32)FMath::Max(CVarAPEXMaxDestructibleDynamicChunkCount.GetValueOnGameThread(), 0));
		GApexModuleDestructible->setSortByBenefit(CVarAPEXSortDynamicChunksByBenefit.GetValueOnGameThread() != 0);

		GApexModuleDestructible->scheduleChunkStateEventCallback(apex::DestructibleCallbackSchedule::FetchResults);

		// APEX 1.3 to preserve 1.2 behavior
		GApexModuleDestructible->setUseLegacyDamageRadiusSpread(true);
		GApexModuleDestructible->setUseLegacyChunkBoundsTesting(true);

		FApexDestructionCustomPayload::SingletonCustomSync = new FApexDestructionSyncActors();

		GPhysX3Interface = &GPhysX3Interface_ApexDestructionImp;

#endif

		OnPhysSceneInitHandle = FPhysicsDelegates::OnPhysSceneInit.AddRaw(this, &FApexDestructionModule::OnInitPhys);
		OnPhysDispatchNotifications = FPhysicsDelegates::OnPhysDispatchNotifications.AddRaw(this, &FApexDestructionModule::OnDispatchNotifications);


		Singleton = this;

	}

	virtual void OnInitPhys(FPhysScene* PhysScene, EPhysicsSceneType SceneType)
	{
#if WITH_APEX
		//Can't set this flag after scene already created
		/*if(PxScene* PScene = PhysScene->GetPhysXScene(SceneType))
		{
			SCOPED_SCENE_WRITE_LOCK(PScene);
			PScene->setFlag(PxSceneFlag::eENABLE_KINEMATIC_PAIRS, true);
		}*/

		if((PhysScene->bAsyncSceneEnabled && SceneType == PST_Async) || (!PhysScene->bAsyncSceneEnabled && SceneType == PST_Sync))
		{
			// Make sure we use the sync scene for apex world support of destructibles in the async scene
			apex::Scene* ApexScene = PhysScene->GetApexScene(PhysScene->bAsyncSceneEnabled ? PST_Async : PST_Sync);
			check(ApexScene);
			PxScene* SyncPhysXScene = PhysScene->GetPhysXScene(PST_Sync);
			check(SyncPhysXScene);
			check(GApexModuleDestructible);
			GApexModuleDestructible->setWorldSupportPhysXScene(*ApexScene, SyncPhysXScene);
			GApexModuleDestructible->setDamageApplicationRaycastFlags(apex::DestructibleActorRaycastFlags::AllChunks, *ApexScene);
		}
		
		PendingDamageEventsMap.FindOrAdd(PhysScene);
#endif
	}

	virtual void OnDispatchNotifications(FPhysScene* PhysScene)
	{
#if WITH_APEX
		TArray<FPendingApexDamageEvent>& PendingDamageEvents = PendingDamageEventsMap.FindChecked(PhysScene);
		for (const FPendingApexDamageEvent& PendingDamageEvent : PendingDamageEvents)
		{
			if (UDestructibleComponent* DestructibleComponent = PendingDamageEvent.DestructibleComponent.Get())
			{
				const apex::DamageEventReportData & damageEvent = PendingDamageEvent.DamageEvent;
				check(DestructibleComponent == Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(damageEvent.destructible->userData)))	//we store this as a weak pointer above in case one of the callbacks decided to call DestroyComponent
					DestructibleComponent->OnDamageEvent(damageEvent);
			}
		}

		PendingDamageEvents.Empty();
#endif
	}

	void AddPendingDamageEvent(UDestructibleComponent* DestructibleComponent, const nvidia::apex::DamageEventReportData& DamageEvent)
	{
#if WITH_APEX
		FPhysScene* PhysScene = DestructibleComponent->GetWorld()->GetPhysicsScene();
		TArray<FPendingApexDamageEvent>& PendingDamageEvents = PendingDamageEventsMap.FindChecked(PhysScene);
		PendingDamageEvents.Emplace(DestructibleComponent, DamageEvent);
#endif
	}

	virtual void ShutdownModule() override
	{
		delete FApexDestructionCustomPayload::SingletonCustomSync;	//TODO: this should probably make sure all destructibles are removed from the physx sim

		FPhysicsDelegates::OnPhysSceneInit.Remove(OnPhysSceneInitHandle);

#if WITH_APEX
		if(ApexDestructibleDLL)
		{
			PhysDLLHelper::UnloadAPEXModule(ApexDestructibleDLL);
		}
#endif
	}

	static FApexDestructionModule& GetSingleton()
	{
		return *Singleton;
	}

	static FApexDestructionModule* Singleton;

};

FApexDestructionModule* FApexDestructionModule::Singleton = nullptr;

#if WITH_APEX
void FApexChunkReport::onDamageNotify(const nvidia::apex::DamageEventReportData& damageEvent)
{
	UDestructibleComponent* DestructibleComponent = Cast<UDestructibleComponent>(FPhysxUserData::Get<UPrimitiveComponent>(damageEvent.destructible->userData));
	check(DestructibleComponent);

	if (DestructibleComponent->IsPendingKill())	//don't notify if object is being destroyed
	{
		return;
	}


	FApexDestructionModule::GetSingleton().AddPendingDamageEvent(DestructibleComponent, damageEvent);
}
#endif

IMPLEMENT_MODULE(FApexDestructionModule, ApexDestruction)