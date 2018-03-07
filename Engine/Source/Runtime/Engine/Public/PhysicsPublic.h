// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsPublic.h
	Rigid Body Physics Public Types
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Engine/EngineTypes.h"
#include "Misc/CoreMisc.h"
#include "EngineDefines.h"
#include "RenderResource.h"
#include "PhysicsEngine/BodyInstance.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"

class AActor;
class ULineBatchComponent;
class UPhysicalMaterial;
class UPhysicsAsset;
class UPrimitiveComponent;
class USkeletalMeshComponent;
struct FConstraintInstance;

/**
 * Physics stats
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("FetchAndStart Time (all)"), STAT_TotalPhysicsTime, STATGROUP_Physics, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Cloth Actor Count"), STAT_NumCloths, STATGROUP_Physics, ENGINE_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Simulated Cloth Verts"), STAT_NumClothVerts, STATGROUP_Physics, ENGINE_API);

#if WITH_PHYSX

namespace physx
{
	class PxScene;
	class PxConvexMesh;
	class PxTriangleMesh;
	class PxCooking;
	class PxPhysics;
	class PxVec3;
	class PxJoint;
	class PxMat44;
	class PxCpuDispatcher;
	class PxSimulationEventCallback;
	struct PxActiveTransform;
	class PxActor;
	class PxRigidActor;
}

#if WITH_APEX
namespace nvidia
{
	namespace apex
	{
		class DestructibleAsset;
		class Scene;
		struct DamageEventReportData;
		class ApexSDK;
		class ModuleDestructible;
		class DestructibleActor;
		class ModuleClothing;
		class Module;
		class ClothingActor;
		class ClothingAsset;
		class ApexInterface;
	}
}
#endif // WITH_APEX

struct FConstraintInstance;
class UPhysicsAsset;


struct FConstraintBrokenDelegateData
{
	FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance);

	void DispatchOnBroken()
	{
		OnConstraintBrokenDelegate.ExecuteIfBound(ConstraintIndex);
	}

	FOnConstraintBroken OnConstraintBrokenDelegate;
	int32 ConstraintIndex;
};

using namespace physx;
#if WITH_APEX
using namespace nvidia;
#endif	//WITH_APEX

/** Pointer to PhysX SDK object */
extern ENGINE_API PxPhysics*			GPhysXSDK;
/** Pointer to PhysX cooking object */
extern ENGINE_API PxCooking*			GPhysXCooking;
/** Pointer to PhysX allocator */
extern ENGINE_API class FPhysXAllocator* GPhysXAllocator;

/** Pointer to PhysX Command Handler */
extern ENGINE_API class FPhysCommandHandler* GPhysCommandHandler;

namespace NvParameterized
{
	class Interface;
}

#if WITH_APEX

/** Pointer to APEX SDK object */
extern ENGINE_API apex::ApexSDK*			GApexSDK;
/** Pointer to APEX legacy module object */
extern ENGINE_API apex::Module* 			GApexModuleLegacy;
#if WITH_APEX_CLOTHING
/** Pointer to APEX Clothing module object */
extern ENGINE_API apex::ModuleClothing*		GApexModuleClothing;
#endif //WITH_APEX_CLOTHING

#endif // #if WITH_APEX

#endif // WITH_PHYSX

#if WITH_FLEX
class UFlexContainer;
struct FFlexContainerInstance;
extern ENGINE_API bool GFlexIsInitialized;
#endif

/** Information about a specific object involved in a rigid body collision */
struct ENGINE_API FRigidBodyCollisionInfo
{
	/** Actor involved in the collision */
	TWeakObjectPtr<AActor>					Actor;

	/** Component of Actor involved in the collision. */
	TWeakObjectPtr<UPrimitiveComponent>		Component;

	/** Index of body if this is in a PhysicsAsset. INDEX_NONE otherwise. */
	int32									BodyIndex;

	/** Name of bone if a PhysicsAsset */
	FName									BoneName;

	FRigidBodyCollisionInfo() :
		BodyIndex(INDEX_NONE),
		BoneName(NAME_None)
	{}

	/** Utility to set up the body collision info from an FBodyInstance */
	void SetFrom(const FBodyInstance* BodyInst);
	/** Get body instance */
	FBodyInstance* GetBodyInstance() const;
};

/** One entry in the array of collision notifications pending execution at the end of the physics engine run. */
struct ENGINE_API FCollisionNotifyInfo
{
	/** If this notification should be called for the Actor in Info0. */
	bool							bCallEvent0;

	/** If this notification should be called for the Actor in Info1. */
	bool							bCallEvent1;

	/** Information about the first object involved in the collision. */
	FRigidBodyCollisionInfo			Info0;

	/** Information about the second object involved in the collision. */
	FRigidBodyCollisionInfo			Info1;

	/** Information about the collision itself */
	FCollisionImpactData			RigidCollisionData;

	FCollisionNotifyInfo() :
		bCallEvent0(false),
		bCallEvent1(false)
	{}

	/** Check that is is valid to call a notification for this entry. Looks at the IsPendingKill() flags on both Actors. */
	bool IsValidForNotify() const;
};

namespace PhysCommand
{
	enum Type
	{
		Release,
		ReleasePScene,
		DeleteCPUDispatcher,
		DeleteSimEventCallback,
		Max
	};
}


/** Container used for physics tasks that need to be deferred from GameThread. This is not safe for general purpose multi-therading*/
class FPhysCommandHandler
{
public:

	~FPhysCommandHandler();
	
	/** Executes pending commands and clears buffer **/
	void ENGINE_API Flush();
	bool ENGINE_API HasPendingCommands();

#if WITH_APEX
	/** enqueues a command to release destructible actor once apex has finished simulating */
	void ENGINE_API DeferredRelease(apex::ApexInterface* ApexInterface);
#endif

#if WITH_PHYSX
	void ENGINE_API DeferredRelease(physx::PxScene * PScene);
	void ENGINE_API DeferredDeleteSimEventCallback(physx::PxSimulationEventCallback * SimEventCallback);
	void ENGINE_API DeferredDeleteCPUDispathcer(physx::PxCpuDispatcher * CPUDispatcher);
#endif

private:

	/** Command to execute when physics simulation is done */
	struct FPhysPendingCommand
	{
		union
		{
#if WITH_APEX
			apex::ApexInterface * ApexInterface;
			apex::DestructibleActor * DestructibleActor;
#endif
#if WITH_PHYSX
			physx::PxScene * PScene;
			physx::PxCpuDispatcher * CPUDispatcher;
			physx::PxSimulationEventCallback * SimEventCallback;
#endif
		} Pointer;

		PhysCommand::Type CommandType;
	};

	/** Execute all enqueued commands */
	void ExecuteCommands();

	/** Enqueue a command to the double buffer */
	void EnqueueCommand(const FPhysPendingCommand& Command);

	/** Array of commands waiting to execute once simulation is done */
	TArray<FPhysPendingCommand> PendingCommands;
};

namespace SleepEvent
{
	enum Type
	{
		SET_Wakeup,
		SET_Sleep
	};

}

/** Buffers used as scratch space for PhysX to avoid allocations during simulation */
struct FSimulationScratchBuffer
{
	FSimulationScratchBuffer()
		: Buffer(nullptr)
		, BufferSize(0)
	{}

	// The scratch buffer
	uint8* Buffer;

	// Allocated size of the buffer
	int32 BufferSize;
};

#if WITH_PHYSX
/** Interface for the creation of customized simulation event callbacks. */
class ISimEventCallbackFactory
{
public:
	virtual physx::PxSimulationEventCallback* Create(class FPhysScene* PhysScene, int32 SceneType) = 0;
	virtual void Destroy(physx::PxSimulationEventCallback* Callback) = 0;
};
#endif // WITH PHYSX


/** Container object for a physics engine 'scene'. */

class FPhysScene
{
public:
	/** Indicates whether the async scene is enabled or not. */
	bool							bAsyncSceneEnabled;

	/** Indicates whether the scene is using substepping */
	bool							bSubstepping;

	/** Indicates whether the scene is using substepping */
	bool							bSubsteppingAsync;

	/** Stores the number of valid scenes we are working with. This will be PST_MAX or PST_Async, 
		depending on whether the async scene is enabled or not*/
	uint32							NumPhysScenes;
	
	/** Gets the array of collision notifications, pending execution at the end of the physics engine run. */
	TArray<FCollisionNotifyInfo>& GetPendingCollisionNotifies(int32 SceneType){ return PendingCollisionData[SceneType].PendingCollisionNotifies; }


	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysScenePreTick, FPhysScene*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysScenePreTick OnPhysScenePreTick;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPhysSceneStep, FPhysScene*, uint32 /*SceneType*/, float /*DeltaSeconds*/);
	FOnPhysSceneStep OnPhysSceneStep;



private:
	/** World that owns this physics scene */
	UWorld*							OwningWorld;

public:
	//Owning world is made private so that any code which depends on setting an owning world can update
	void SetOwningWorld(UWorld* InOwningWorld);
	UWorld* GetOwningWorld(){ return OwningWorld; }
	const UWorld* GetOwningWorld() const { return OwningWorld; }

	/** These indices are used to get the actual PxScene or ApexScene from the GPhysXSceneMap. */
	int16								PhysXSceneIndex[PST_MAX];

	/** Whether or not the given scene is between its execute and sync point. */
	bool							bPhysXSceneExecuting[PST_MAX];

	/** Frame time, weighted with current frame time. */
	float							AveragedFrameTime[PST_MAX];

	/**
	 * Weight for averaged frame time.  Value should be in the range [0.0f, 1.0f].
	 * Weight = 0.0f => no averaging; current frame time always used.
	 * Weight = 1.0f => current frame time ignored; initial value of AveragedFrameTime[i] is always used.
	 */
	float							FrameTimeSmoothingFactor[PST_MAX];

#if WITH_PHYSX
	bool IsFlushNeededForDeferredActors_AssumesLocked(EPhysicsSceneType SceneType) const
	{
		return DeferredSceneData[SceneType].IsFlushNeeded_AssumesLocked();
	}

	/** Defer the addition of an actor to a scene, this will actually be performed before the *next*
	 *  Physics tick
	 *	@param OwningInstance - The FBodyInstance that owns the actor
	 *	@param Actor - The actual PhysX actor to add
	 *	@param SceneType - The scene type to add the actor to
	 */
	void DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType);

	/** Defer the addition of a group of actors to a scene, this will actually be performed before the *next*
	 *  Physics tick. 
	 *
	 *	@param OwningInstances - The FBodyInstance that owns the actor
	 *	@param Actors - The actual PhysX actor to add
	 *	@param SceneType - The scene type to add the actor to
	 */
	void DeferAddActors(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors, EPhysicsSceneType SceneType);

	/** Defer the removal of an actor to a scene, this will actually be performed before the *next*
	 *  Physics tick
	 *	@param OwningInstance - The FBodyInstance that owns the actor
	 *	@param Actor - The actual PhysX actor to remove
	 *	@param SceneType - The scene type to remove the actor from
	 */
	void DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType);

	/** Defer the removal of actors from a scene, this will actually be performed before the *next*
	*  Physics tick
	*	@param OwningInstances - The FBodyInstance that owns the actor
	*	@param Actors - The actual PhysX actor to remove
	*	@param SceneType - The scene type to remove the actor from
	*/
	void DeferRemoveActors(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors, EPhysicsSceneType SceneType);

	/** Flushes deferred actors to ensure they are in the physics scene. Note if this is called while the simulation is still running we use a slower insertion/removal path */
	void FlushDeferredActors(EPhysicsSceneType SceneType);

	void AddPendingSleepingEvent(PxActor* Actor, SleepEvent::Type SleepEventType, int32 SceneType);

	/** Pending constraint break events */
	void AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType);
#endif

private:
	/** DeltaSeconds from UWorld. */
	float										DeltaSeconds;
	/** DeltaSeconds from the WorldSettings. */
	float										MaxPhysicsDeltaTime;
	/** DeltaSeconds used by the last synchronous scene tick.  This may be used for the async scene tick. */
	float										SyncDeltaSeconds;
	/** LineBatcher from UWorld. */
	ULineBatchComponent*						LineBatcher;

	/** Completion event (not tasks) for the physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef PhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (not tasks) for the frame lagged physics scenes these are fired by the physics system when it is done; prerequisites for the below */
	FGraphEventRef FrameLaggedPhysicsSubsceneCompletion[PST_MAX];
	/** Completion events (task) for the physics scenes	(both apex and non-apex). This is a "join" of the above. */
	FGraphEventRef PhysicsSceneCompletion;

	// Data for scene scratch buffers, these will be allocated once on FPhysScene construction and used
	// for the calls to PxScene::simulate to save it calling into the OS to allocate during simulation
	FSimulationScratchBuffer SimScratchBuffers[PST_MAX];

	// Boundary value for PhysX scratch buffers (currently PhysX requires the buffer length be a multiple of 16K)
	static const int32 SimScratchBufferBoundary = 16 * 1024;

#if WITH_PHYSX

	struct FDeferredSceneData
	{
		FDeferredSceneData();

		/** Whether the physx scene is currently simulating. */
		bool bIsSimulating;

		/** Body instances awaiting scene add */
		TArray<FBodyInstance*> AddInstances;
		/** PhysX Actors awaiting scene add */
		TArray<PxActor*> AddActors;

		/** Body instances awaiting scene remove */
		TArray<FBodyInstance*> RemoveInstances;
		/** PhysX Actors awaiting scene remove */
		TArray<PxActor*> RemoveActors;

		void FlushDeferredActors_AssumesLocked(PxScene* Scene);
		void DeferAddActor_AssumesLocked(FBodyInstance* OwningInstance, PxActor* Actor);
		void DeferAddActors_AssumesLocked(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors);
		void DeferRemoveActor_AssumesLocked(FBodyInstance* OwningInstance, PxActor* Actor);
		void DeferRemoveActors_AssumesLocked(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors);

		bool IsFlushNeeded_AssumesLocked() const
		{
			return AddInstances.Num() > 0 || RemoveInstances.Num() > 0;
		}
	};

	FDeferredSceneData DeferredSceneData[PST_MAX];

	/** Dispatcher for CPU tasks */
	class PxCpuDispatcher*			CPUDispatcher[PST_MAX];
	/** Simulation event callback object */
	physx::PxSimulationEventCallback*			SimEventCallback[PST_MAX];
#endif	//

	struct FPendingCollisionData
	{
		/** Array of collision notifications, pending execution at the end of the physics engine run. */
		TArray<FCollisionNotifyInfo>	PendingCollisionNotifies;
	};

	FPendingCollisionData PendingCollisionData[PST_MAX];

	struct FPendingConstraintData
	{
		/** Array of constraint broken notifications, pending execution at the end of the physics engine run. */
		TArray<FConstraintBrokenDelegateData> PendingConstraintBroken;
	};

	FPendingConstraintData PendingConstraintData[PST_MAX];
	
#if WITH_FLEX
    /** Map from Flex container template to instances belonging to this physscene */
    TMap<UFlexContainer*, FFlexContainerInstance*>    FlexContainerMap;
	FGraphEventRef FlexSimulateTaskRef;
#endif

public:
#if WITH_PHYSX
	/** Static factory used to override the simulation event callback from other modules.
	If not set it defaults to using FPhysXSimEventCallback. */
	ENGINE_API static TSharedPtr<ISimEventCallbackFactory> SimEventCallbackFactory;


	/** Utility for looking up the PxScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	ENGINE_API physx::PxScene*					GetPhysXScene(uint32 SceneType) const;

#endif

#if WITH_APEX
	/** Utility for looking up the ApexScene of the given EPhysicsSceneType associated with this FPhysScene.  SceneType must be in the range [0,PST_MAX). */
	ENGINE_API nvidia::apex::Scene*				GetApexScene(uint32 SceneType) const;
#endif

#if WITH_FLEX
	/** Retrive the container instance for a template, will create the instance if it doesn't already exist */
	FFlexContainerInstance*	GetFlexContainer(UFlexContainer* Template);
	void StartFlexRecord();
	void StopFlexRecord();

	/** Adds a radial force to all flex container instances */
	void AddRadialForceToFlex(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff);

	/** Adds a radial force to all flex container instances */
	void AddRadialImpulseToFlex(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange);
#endif

	ENGINE_API FPhysScene();
	ENGINE_API ~FPhysScene();

	/** Start simulation on the physics scene of the given type */
	ENGINE_API void TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

#if WITH_FLEX
	ENGINE_API void WaitFlexScenes();
	ENGINE_API void TickFlexScenes(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, float dt);
	ENGINE_API void TickFlexScenesTask(float dt);
#endif

	/** Set the gravity and timing of all physics scenes */
	ENGINE_API void SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds = 0.0f, float InMaxPhysicsDeltaTime = 0.0f);

	/** Starts a frame */
	ENGINE_API void StartFrame();

	/** Ends a frame */
	ENGINE_API void EndFrame(ULineBatchComponent* InLineBatcher);

	/** Starts cloth Simulation*/
	ENGINE_API void StartAsync();

	/** returns the completion event for a frame */
	FGraphEventRef GetCompletionEvent()
	{
		return PhysicsSceneCompletion;
	}

	/** Ensures that the collision tree is built. */
	ENGINE_API void EnsureCollisionTreeIsBuilt(UWorld* World);

	/** Waits for all physics scenes to complete */
	ENGINE_API void WaitPhysScenes();

	/** Kill the visual debugger */
	ENGINE_API void KillVisualDebugger();

	/** Fetches results, fires events, and adds debug lines */
	void ProcessPhysScene(uint32 SceneType);

	/** Sync components in the scene to physics bodies that changed */
	void SyncComponentsToBodies_AssumesLocked(uint32 SceneType);

	/** Call after WaitPhysScene on the synchronous scene to make deferred OnRigidBodyCollision calls.  */
	ENGINE_API void DispatchPhysNotifications_AssumesLocked();

	/** Add any debug lines from the physics scene of the given type to the supplied line batcher. */
	ENGINE_API void AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse);

	/** @return Whether physics scene supports scene origin shifting */
	static bool SupportsOriginShifting() { return true; }

	/** @return Whether physics scene is using substepping */
	bool IsSubstepping(uint32 SceneType) const;
	
	/** Shifts physics scene origin by specified offset */
	void ApplyWorldOffset(FVector InOffset);

	/** Returns whether an async scene is setup and can be used. This depends on the console variable "p.EnableAsyncScene". */
	ENGINE_API bool HasAsyncScene() const { return bAsyncSceneEnabled; }

	/** Lets the scene update anything related to this BodyInstance as it's now being terminated */
	void TermBody_AssumesLocked(FBodyInstance* BodyInstance);

	/** Add a custom callback for next step that will be called on every substep */
	void AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics);

	/** Adds a force to a body - We need to go through scene to support substepping */
	void AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange);

	/** Adds a force to a body at a specific position - We need to go through scene to support substepping */
	void AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce=false);

	/** Adds a radial force to a body - We need to go through scene to support substepping */
	void AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping);

	/** Adds torque to a body - We need to go through scene to support substepping */
	void AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange);

	/** Sets a Kinematic actor's target position - We need to do this here to support substepping*/
	void SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping);

	/** Gets a Kinematic actor's target position - We need to do this here to support substepping
	  * Returns true if kinematic target has been set. If false the OutTM is invalid */
	bool GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const;

	/** Gets the collision disable table */
	const TMap<uint32, TMap<struct FRigidBodyIndexPair, bool> *> & GetCollisionDisableTableLookup()
	{
		return CollisionDisableTableLookup;
	}

	/** Adds to queue of skelmesh we want to add to collision disable table */
	ENGINE_API void DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable);

	/** Adds to queue of skelmesh we want to remove from collision disable table */
	ENGINE_API void DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID);

	/** Marks actor as being deleted to ensure it is not updated as an actor actor. This should only be called by very advanced code that is using physx actors directly (not recommended!) */
	void RemoveActiveRigidActor(uint32 SceneType, physx::PxRigidActor* ActiveRigidActor)
	{
		IgnoreActiveActors[SceneType].Add(ActiveRigidActor);
	}

	/** Add this SkeletalMeshComponent to the list needing kinematic bodies updated before simulating physics */
	void MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning);

	/** Remove this SkeletalMeshComponent from set needing kinematic update before simulating physics*/
	void ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp);

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	void SetPhysXTreeRebuildRate(int32 RebuildRate);
	
private:
	/** Initialize a scene of the given type.  Must only be called once for each scene type. */
	void InitPhysScene(uint32 SceneType);

	/** Terminate a scene of the given type.  Must only be called once for each scene type. */
	void TermPhysScene(uint32 SceneType);

	/** Called when all subscenes of a given scene are complete, calls  ProcessPhysScene*/
	void SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType);

	/** Helper function for determining which scene a dyanmic body is in*/
	EPhysicsSceneType SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const;

	/** Process kinematic updates on any deferred skeletal meshes */
	void UpdateKinematicsOnDeferredSkelMeshes();

	/** Task created from TickPhysScene so we can substep without blocking */
	bool SubstepSimulation(uint32 SceneType, FGraphEventRef& InOutCompletionEvent);

	/** Set whether we're doing a static load and want to stall, or are during gameplay and want to distribute over many frames */
	void SetIsStaticLoading(bool bStaticLoading);

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	void SetPhysXTreeRebuildRateImp(int32 RebuildRate);

#if WITH_PHYSX
	/** User data wrapper passed to physx */
	struct FPhysxUserData PhysxUserData;

	TArray<physx::PxRigidActor*> IgnoreActiveActors[PST_MAX];	//Active actors that have been deleted after fetchResults but before EndFrame and must be ignored
	void RemoveActiveBody_AssumesLocked(FBodyInstance* BodyInstance, uint32 SceneType);
#endif

	class FPhysSubstepTask * PhysSubSteppers[PST_MAX];

	struct FPendingCollisionDisableTable
	{
		uint32 SkelMeshCompID;
		TMap<struct FRigidBodyIndexPair, bool>* CollisionDisableTable;
	};

	/** Updates CollisionDisableTableLookup with the deferred insertion and deletion */
	void FlushDeferredCollisionDisableTableQueue();

	/** Queue of deferred collision table insertion and deletion */
	TArray<FPendingCollisionDisableTable> DeferredCollisionDisableTableQueue;

	/** Map from SkeletalMeshComponent UniqueID to a pointer to the collision disable table inside its PhysicsAsset */
	TMap< uint32, TMap<struct FRigidBodyIndexPair, bool>* >		CollisionDisableTableLookup;

#if WITH_PHYSX
	TMap<PxActor*, SleepEvent::Type> PendingSleepEvents[PST_MAX];
#endif

	/** Information about how to perform kinematic update before physics */
	struct FDeferredKinematicUpdateInfo
	{
		/** Whether to teleport physics bodies or not */
		ETeleportType	TeleportType;
		/** Whether to update skinning info */
		bool			bNeedsSkinning;
	};

	/** Map of SkeletalMeshComponents that need their bone transforms sent to the physics engine before simulation. */
	TMap<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>	DeferredKinematicUpdateSkelMeshes;

	FDelegateHandle PreGarbageCollectDelegateHandle;

	int32 PhysXTreeRebuildRate;
};

/**
* Return true if we should be running in single threaded mode, ala dedicated server
**/
FORCEINLINE bool PhysSingleThreadedMode()
{
	if (IsRunningDedicatedServer() || FPlatformMisc::NumberOfCores() < 3 || !FPlatformProcess::SupportsMultithreading())
	{
		return true;
	}
	return false;
}

#if WITH_PHYSX
/** Struct used for passing info to the PhysX shader */

struct FPhysSceneShaderInfo
{
	FPhysScene * PhysScene;
};

#endif

/** Enum to indicate types of simple shapes */
enum DEPRECATED(4.17, "Please use EAggCollisionShape::Type") EKCollisionPrimitiveType
{
	KPT_Sphere = 0,
	KPT_Box,
	KPT_Sphyl,
	KPT_Convex,
	KPT_Unknown
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexDataElement
{
public:
	TArray<uint8>	ConvexElementData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexDataElement& S )
	{
		S.ConvexElementData.BulkSerialize(Ar);
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_REMOVE_PHYS_SCALED_GEOM_CACHES)
class FKCachedConvexData
{
public:
	TArray<FKCachedConvexDataElement>	CachedConvexElements;

	friend FArchive& operator<<( FArchive& Ar, FKCachedConvexData& S )
	{
		Ar << S.CachedConvexElements;
		return Ar;
	}
};

// Only used for legacy serialization (ver < VER_UE4_ADD_BODYSETUP_GUID)
struct FKCachedPerTriData
{
	TArray<uint8> CachedPerTriData;

	friend FArchive& operator<<( FArchive& Ar, FKCachedPerTriData& S )
	{
		S.CachedPerTriData.BulkSerialize(Ar);
		return Ar;
	}
};



class FConvexCollisionVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override;
};

class FConvexCollisionIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override;
};

class FConvexCollisionVertexFactory : public FLocalVertexFactory
{
public:

	FConvexCollisionVertexFactory()
	{}

	/** Initialization constructor. */
	FConvexCollisionVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
	{
		InitConvexVertexFactory(VertexBuffer);
	}


	void InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer);
};

class FKConvexGeomRenderInfo
{
public:
	FConvexCollisionVertexBuffer* VertexBuffer;
	FConvexCollisionIndexBuffer* IndexBuffer;
	FConvexCollisionVertexFactory* CollisionVertexFactory;

	FKConvexGeomRenderInfo()
	: VertexBuffer(NULL)
	, IndexBuffer(NULL)
	, CollisionVertexFactory(NULL)
	{}

	/** Util to see if this render info has some valid geometry to render. */
	bool HasValidGeometry()
	{
		return 
			(VertexBuffer != NULL) && 
			(VertexBuffer->Vertices.Num() > 0) && 
			(IndexBuffer != NULL) &&
			(IndexBuffer->Indices.Num() > 0);
	}
};

namespace PhysDLLHelper
{
/**
 *	Load the required modules for PhysX
 */
ENGINE_API void LoadPhysXModules(bool bLoadCooking);


#if WITH_APEX
	ENGINE_API void* LoadAPEXModule(const FString& Path);
	ENGINE_API void UnloadAPEXModule(void* Handle);
#endif

/** 
 *	Unload the required modules for PhysX
 */
void UnloadPhysXModules();
}

ENGINE_API void	InitGamePhys();
ENGINE_API void	InitGamePhysPostRHI();
ENGINE_API void	TermGamePhys();


bool	ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld);

/** Util to list to log all currently awake rigid bodies */
void	ListAwakeRigidBodies(bool bIncludeKinematic, UWorld* world);


FTransform FindBodyTransform(AActor* Actor, FName BoneName);
FBox	FindBodyBox(AActor* Actor, FName BoneName);

/** Set of delegates to allowing hooking different parts of the physics engine */
class ENGINE_API FPhysicsDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdatePhysXMaterial, UPhysicalMaterial*);
	static FOnUpdatePhysXMaterial OnUpdatePhysXMaterial;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhysicsAssetChanged, const UPhysicsAsset*);
	static FOnPhysicsAssetChanged OnPhysicsAssetChanged;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysSceneInit, FPhysScene*, EPhysicsSceneType);
	static FOnPhysSceneInit OnPhysSceneInit;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPhysSceneTerm, FPhysScene*, EPhysicsSceneType);
	static FOnPhysSceneTerm OnPhysSceneTerm;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPhysDispatchNotifications, FPhysScene*);
	static FOnPhysDispatchNotifications OnPhysDispatchNotifications;
};

extern ENGINE_API class IPhysXCookingModule* GetPhysXCookingModule(bool bForceLoad = true);
