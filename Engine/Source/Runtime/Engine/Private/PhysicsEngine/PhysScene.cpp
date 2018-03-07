// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "PhysxUserData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicsPublic.h"
#include "CustomPhysXPayload.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_PHYSX
	#include "PhysXPublic.h"
	#include "PhysicsEngine/PhysXSupport.h"
#endif

#include "PhysicsEngine/PhysSubstepTasks.h"
#if WITH_FLEX
	#include "PhysicsEngine/FlexContainer.h"
	#include "FlexContainerInstance.h"	
	#include "DrawDebugHelpers.h" // FlushPersistentDebugLines
#endif

#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Components/LineBatchComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"

/** Physics stats **/

DEFINE_STAT(STAT_TotalPhysicsTime);
DEFINE_STAT(STAT_NumCloths);
DEFINE_STAT(STAT_NumClothVerts);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (sync)"), STAT_PhysicsKickOffDynamicsTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (sync)"), STAT_PhysicsFetchDynamicsTime, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (cloth)"), STAT_PhysicsKickOffDynamicsTime_Cloth, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (cloth)"), STAT_PhysicsFetchDynamicsTime_Cloth, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Start Physics Time (async)"), STAT_PhysicsKickOffDynamicsTime_Async, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Fetch Results Time (async)"), STAT_PhysicsFetchDynamicsTime_Async, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Update Kinematics On Deferred SkelMeshes"), STAT_UpdateKinematicsOnDeferredSkelMeshes, STATGROUP_Physics);

DECLARE_CYCLE_STAT(TEXT("Phys Events Time"), STAT_PhysicsEventTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (sync)"), STAT_SyncComponentsToBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (cloth)"), STAT_SyncComponentsToBodies_Cloth, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("SyncComponentsToBodies (async)"), STAT_SyncComponentsToBodies_Async, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Adds"), STAT_NumBroadphaseAdds, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Broadphase Removes"), STAT_NumBroadphaseRemoves, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Constraints"), STAT_NumActiveConstraints, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Simulated Bodies"), STAT_NumActiveSimulatedBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Kinematic Bodies"), STAT_NumActiveKinematicBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mobile Bodies"), STAT_NumMobileBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Static Bodies"), STAT_NumStaticBodies, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Shapes"), STAT_NumShapes, STATGROUP_Physics);

DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Adds"), STAT_NumBroadphaseAddsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Broadphase Removes"), STAT_NumBroadphaseRemovesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Constraints"), STAT_NumActiveConstraintsAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Simulated Bodies"), STAT_NumActiveSimulatedBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Active Kinematic Bodies"), STAT_NumActiveKinematicBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Mobile Bodies"), STAT_NumMobileBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Static Bodies"), STAT_NumStaticBodiesAsync, STATGROUP_Physics);
DECLARE_DWORD_COUNTER_STAT(TEXT("(ASync) Shapes"), STAT_NumShapesAsync, STATGROUP_Physics);

static int16 PhysXSceneCount = 1;

EPhysicsSceneType FPhysScene::SceneType_AssumesLocked(const FBodyInstance* BodyInstance) const
{
#if WITH_PHYSX
	//This is a helper function for dynamic actors - static actors are in both scenes
	return HasAsyncScene() && BodyInstance->bUseAsyncScene ? PST_Async : PST_Sync;
#endif

	return PST_Sync;
}

/**
* Return true if we should be running in single threaded mode, ala dedicated server
**/

/**
* Return true if we should lag the async scene a frame
**/
FORCEINLINE static bool FrameLagAsync()
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}
	return true;
}

#if WITH_PHYSX

FAutoConsoleTaskPriority CPrio_FPhysXTask(
	TEXT("TaskGraph.TaskPriorities.PhysXTask"),
	TEXT("Task and thread priority for FPhysXTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

FAutoConsoleTaskPriority CPrio_FPhysXTask_Cloth(
	TEXT("TaskGraph.TaskPriorities.PhysXTask.Cloth"),
	TEXT("Task and thread priority for FPhysXTask (cloth)."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

DECLARE_STATS_GROUP(TEXT("PhysXTasks"), STATGROUP_PhysXTasks, STATCAT_Advanced);

struct FPhysXRingBuffer
{
	static const int32 Size = 16;

	PxBaseTask* Buffer[Size];
	int32 Start;
	int32 End;
	int32 Num;
};

int32 GBatchPhysXTasksSize = 3;	//NOTE: FPhysXRingBuffer::Size should be twice as big as this value
TAutoConsoleVariable<int32> CVarBatchPhysXTasksSize(TEXT("p.BatchPhysXTasksSize"), GBatchPhysXTasksSize, TEXT("Number of tasks to batch together (max 8). 1 will go as wide as possible, but more overhead on small tasks"), ECVF_Default);

struct FBatchPhysXTasks
{
	static void SetPhysXTasksSinkFunc()
	{
		GBatchPhysXTasksSize = FMath::Max(1, FMath::Min(FPhysXRingBuffer::Size / 2, CVarBatchPhysXTasksSize.GetValueOnGameThread()));
	}
};

static FAutoConsoleVariableSink CVarBatchPhysXTasks(FConsoleCommandDelegate::CreateStatic(&FBatchPhysXTasks::SetPhysXTasksSinkFunc));

namespace DynamicStatsHelper
{
	struct FStatLookup
	{
		const char* StatName;
		TStatId Stat;
	} Stats[100];

	int NumStats = 0;
	FCriticalSection CS;

	TStatId FindOrCreateStatId(const char* StatName)
	{
#if STATS
		for (int StatIdx = 0; StatIdx < NumStats; ++StatIdx)
		{
			FStatLookup& Lookup = Stats[StatIdx];
			if (Lookup.StatName == StatName)
			{
				return Lookup.Stat;
			}
		}

		if (ensureMsgf(NumStats < sizeof(Stats) / sizeof(Stats[0]), TEXT("Too many different physx task stats. This will make the stat search slow")))
		{
			FScopeLock ScopeLock(&CS);

			//Do the search again in case another thread added
			for (int StatIdx = 0; StatIdx < NumStats; ++StatIdx)
			{
				FStatLookup& Lookup = Stats[StatIdx];
				if (Lookup.StatName == StatName)
				{
					return Lookup.Stat;
				}
			}

			FStatLookup& NewStat = Stats[NumStats];
			NewStat.StatName = StatName;
			NewStat.Stat = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_PhysXTasks>(FName(StatName));
			FPlatformMisc::MemoryBarrier();
			++NumStats;	//make sure to do this at the end in case another thread is currently iterating
			return NewStat.Stat;
		}
#endif // STATS
		return TStatId();
	}
}

template <bool IsCloth>
struct FPhysXCPUDispatcher;

template <bool IsCloth>
class FPhysXTask
{
public:
	FPhysXTask(PxBaseTask& InTask, FPhysXCPUDispatcher<IsCloth>& InDispatcher);
	FPhysXTask(FPhysXRingBuffer& RingBuffer, FPhysXCPUDispatcher<IsCloth>& InDispatcher);
	~FPhysXTask();

	static FORCEINLINE TStatId GetStatId()
	{
		if (!IsCloth)
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXTask, STATGROUP_Physics);
		}
		else
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysXClothTask, STATGROUP_Physics);
		}

	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		if (!IsCloth)
		{
			return CPrio_FPhysXTask.Get();
		}
		else
		{
			return CPrio_FPhysXTask_Cloth.Get();
		}
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);

	FPhysXRingBuffer RingBuffer;
	FPhysXCPUDispatcher<IsCloth>& Dispatcher;
};

/** Used to dispatch physx tasks to task graph */
template <bool IsClothScene>
struct FPhysXCPUDispatcher : public PxCpuDispatcher
{
	FPhysXCPUDispatcher()
	{
		check(IsInGameThread());
		TLSKey = FPlatformTLS::AllocTlsSlot();
	}

	~FPhysXCPUDispatcher()
	{
		check(IsInGameThread());
		FPlatformTLS::FreeTlsSlot(TLSKey);
	}

	virtual void submitTask(PxBaseTask& Task) override
	{
		if (IsInGameThread())
		{
			//Game thread enqueues on task graph
			TGraphTask<FPhysXTask<IsClothScene>>::CreateTask(NULL).ConstructAndDispatchWhenReady(Task, *this);
		}
		else
		{
			//See if we can use local queue
			FPhysXRingBuffer& RingBuffer = *(FPhysXRingBuffer*)FPlatformTLS::GetTlsValue(TLSKey);
			RingBuffer.Buffer[RingBuffer.End] = &Task;
			RingBuffer.End = (RingBuffer.End + 1) % FPhysXRingBuffer::Size;
			RingBuffer.Num++;

			if (RingBuffer.Num >= GBatchPhysXTasksSize * 2)
			{
				TGraphTask<FPhysXTask<IsClothScene>>::CreateTask(NULL).ConstructAndDispatchWhenReady(RingBuffer, *this);
			}
		}
	}

	virtual PxU32 getWorkerCount() const override
	{
		return FTaskGraphInterface::Get().GetNumWorkerThreads();
	}

	uint32 TLSKey;
};

template <bool IsCloth>
FPhysXTask<IsCloth>::FPhysXTask(PxBaseTask& Task, FPhysXCPUDispatcher<IsCloth>& InDispatcher)
	: Dispatcher(InDispatcher)
{
	RingBuffer.Buffer[0] = &Task;
	RingBuffer.Start = 0;
	RingBuffer.End = 1;
	RingBuffer.Num = 1;
}

template <bool IsCloth>
FPhysXTask<IsCloth>::FPhysXTask(FPhysXRingBuffer& InRingBuffer, FPhysXCPUDispatcher<IsCloth>& InDispatcher)
	: Dispatcher(InDispatcher)
{
	int32 NumToSteal = InRingBuffer.Num / 2;
	ensureMsgf(NumToSteal > 0, TEXT("Trying to steal 0 items"));

	const int32 StartPos = (InRingBuffer.Start + NumToSteal);
	for (int32 Count = 0; Count < NumToSteal; ++Count)
	{
		RingBuffer.Buffer[Count] = InRingBuffer.Buffer[(StartPos + Count) % FPhysXRingBuffer::Size];
	}

	RingBuffer.Start = 0;
	RingBuffer.End = NumToSteal;
	RingBuffer.Num = NumToSteal;


	InRingBuffer.Num -= NumToSteal;
	InRingBuffer.End = (StartPos) % FPhysXRingBuffer::Size;
}

template <bool IsCloth>
FPhysXTask<IsCloth>::~FPhysXTask()
{
	FPlatformTLS::SetTlsValue(Dispatcher.TLSKey, nullptr);
}

template <bool IsCloth>
void FPhysXTask<IsCloth>::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FPlatformTLS::SetTlsValue(Dispatcher.TLSKey, &RingBuffer);

	while (RingBuffer.Num)
	{
		PxBaseTask* Task = RingBuffer.Buffer[RingBuffer.Start];

#if STATS
		const char* StatName = Task->getName();
		FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(StatName));
#endif
		Task->run();
		Task->release();

		RingBuffer.Start = (RingBuffer.Start + 1) % RingBuffer.Size;
		--RingBuffer.Num;

	}
}


DECLARE_CYCLE_STAT(TEXT("PhysX Single Thread Task"), STAT_PhysXSingleThread, STATGROUP_Physics);

/** Used to dispatch physx tasks to the game thread */
template <bool IsClothDispatcher>
class FPhysXCPUDispatcherSingleThread : public PxCpuDispatcher
{
	TArray<PxBaseTask*> TaskStack;

	virtual void submitTask(PxBaseTask& Task) override
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysXSingleThread);

		if(!IsClothDispatcher)
		{
			// Clothing will always be running from a worker, and the tasks
			// are safe to run off the game thread.
			check(IsInGameThread());
		}

		TaskStack.Push(&Task);
		if (TaskStack.Num() > 1)
		{
			return;
		}

		{
#if STATS
			const char* StatName = Task.getName();
			FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(StatName));
#endif
			Task.run();
			Task.release();
		}

		while (TaskStack.Num() > 1)
		{
			PxBaseTask& ChildTask = *TaskStack.Pop();
			{
#if STATS
				const char* StatName = ChildTask.getName();
				FScopeCycleCounter CycleCounter(DynamicStatsHelper::FindOrCreateStatId(StatName));
#endif
				ChildTask.run();
				ChildTask.release();
			}
		}
		verify(&Task == TaskStack.Pop() && !TaskStack.Num());
	}

	virtual PxU32 getWorkerCount() const override
	{
		return 1;
	}
};

TSharedPtr<ISimEventCallbackFactory> FPhysScene::SimEventCallbackFactory;

#endif // WITH_PHYSX

static void StaticSetPhysXTreeRebuildRate(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() > 0)
	{
		const int32 NewRate = FCString::Atoi(*Args[0]);
		if(World && World->GetPhysicsScene())
		{
			World->GetPhysicsScene()->SetPhysXTreeRebuildRate(NewRate);
		}
	}
	else
	{
		UE_LOG(LogPhysics, Warning, TEXT("Usage: p.PhysXTreeRebuildRate <num_frames>"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GSetPhysXTreeRebuildRate(TEXT("p.PhysXTreeRebuildRate"), TEXT("Utility function to change PhysXTreeRebuildRate, useful when profiling fetchResults vs scene queries."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StaticSetPhysXTreeRebuildRate)
);


/** Exposes creation of physics-engine scene outside Engine (for use with Physics Asset Editor for example). */
FPhysScene::FPhysScene()
{
	LineBatcher = NULL;
	OwningWorld = NULL;
#if WITH_PHYSX
	PhysxUserData = FPhysxUserData(this);
#endif	//#if WITH_PHYSX

	UPhysicsSettings * PhysSetting = UPhysicsSettings::Get();
	FMemory::Memzero(FrameTimeSmoothingFactor);
	FrameTimeSmoothingFactor[PST_Sync] = PhysSetting->SyncSceneSmoothingFactor;
	FrameTimeSmoothingFactor[PST_Async] = PhysSetting->AsyncSceneSmoothingFactor;

	bSubstepping = PhysSetting->bSubstepping;
	bSubsteppingAsync = PhysSetting->bSubsteppingAsync;
	bAsyncSceneEnabled = PhysSetting->bEnableAsyncScene;
	NumPhysScenes = bAsyncSceneEnabled ? PST_Async + 1 : PST_Cloth + 1;

	PhysXTreeRebuildRate = PhysSetting->PhysXTreeRebuildRate;

	// Create scenes of all scene types
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		// Create the physics scene
		InitPhysScene(SceneType);

		// Also initialize scene data
		bPhysXSceneExecuting[SceneType] = false;

		// Initialize to a value which would be acceptable if FrameTimeSmoothingFactor[i] = 1.0f, i.e. constant simulation substeps
		AveragedFrameTime[SceneType] = PhysSetting->InitialAverageFrameRate;

		// gets from console variable, and clamp to [0, 1] - 1 should be fixed time as 30 fps
		FrameTimeSmoothingFactor[SceneType] = FMath::Clamp<float>(FrameTimeSmoothingFactor[SceneType], 0.f, 1.f);
	}

	if (!bAsyncSceneEnabled)
	{
		PhysXSceneIndex[PST_Async] = 0;
	}
	
	PreGarbageCollectDelegateHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FPhysScene::WaitPhysScenes);

#if WITH_PHYSX
	// Initialise PhysX scratch buffers (only if size > 0)
	int32 SceneScratchBufferSize = PhysSetting->SimulateScratchMemorySize;
	if(SceneScratchBufferSize > 0)
	{
		// Make sure that SceneScratchBufferSize is a multiple of 16K as requested by PhysX.
		SceneScratchBufferSize = FMath::DivideAndRoundUp<int32>(SceneScratchBufferSize, SimScratchBufferBoundary) * SimScratchBufferBoundary;

		for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
		{
			if(SceneType < NumPhysScenes)
			{
				// Only allocate a scratch buffer if we have a scene and we are not using that cloth scene.
				// Clothing actors are not simulated with this scene but simulated per-actor
				PxScene* Scene = GetPhysXScene(SceneType);
				if(SceneType != PST_Cloth && Scene)
				{
					// We have a valid scene, so allocate the buffer for it
					SimScratchBuffers[SceneType].Buffer = (uint8*)FMemory::Malloc(SceneScratchBufferSize, 16);
					SimScratchBuffers[SceneType].BufferSize = SceneScratchBufferSize;
				}
			}
		}
	}
#endif
}

void FPhysScene::SetOwningWorld(UWorld* InOwningWorld)
{
	OwningWorld = InOwningWorld;
}

/** Exposes destruction of physics-engine scene outside Engine. */
FPhysScene::~FPhysScene()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGarbageCollectDelegateHandle);

#if WITH_FLEX
	// Clean up Flex scenes
	if (GFlexIsInitialized && FlexContainerMap.Num())
	{
		for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
		{
			UFlexContainer* FlexContainerCopy = It->Value->Template;

			delete It->Value;
			It.RemoveCurrent();

			// Destroy the UFlexContainer copy that was created by GetFlexContainer()
			if (FlexContainerCopy != nullptr && FlexContainerCopy->IsValidLowLevel())
			{
				FlexContainerCopy->ConditionalBeginDestroy();
			}
		}
	}
#endif

	// Make sure no scenes are left simulating (no-ops if not simulating)
	WaitPhysScenes();
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{

		// Destroy the physics scene
		TermPhysScene(SceneType);

#if WITH_PHYSX
		GPhysCommandHandler->DeferredDeleteCPUDispathcer(CPUDispatcher[SceneType]);
#endif	//#if WITH_PHYSX
	}

#if WITH_PHYSX
	// Free the scratch buffers
 	for(uint32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		if(SimScratchBuffers[SceneType].Buffer != nullptr)
		{
			FMemory::Free(SimScratchBuffers[SceneType].Buffer);
			SimScratchBuffers[SceneType].Buffer = nullptr;
			SimScratchBuffers[SceneType].BufferSize = 0;
		}
	}
#endif
}

namespace
{

bool UseSyncTime(uint32 SceneType)
{
	return (FrameLagAsync() && SceneType == PST_Async);
}

}

bool FPhysScene::GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			return PhysSubStepper->GetKinematicTarget_AssumesLocked(BodyInstance, OutTM);
		}
		else
		{
			PxTransform POutTM;
			bool validTM = PRigidDynamic->getKinematicTarget(POutTM);
			if (validTM)
			{
				OutTM = P2UTransform(POutTM);
				return true;
			}
		}
	}
#endif

	return false;
}

void FPhysScene::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTransform, bool bAllowSubstepping)
{
	TargetTransform.DiagnosticCheck_IsValid();

#if WITH_PHYSX
	if (PxRigidDynamic * PRigidDynamic = BodyInstance->GetPxRigidDynamic_AssumesLocked())
	{
		const bool bIsKinematicTarget = IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(PRigidDynamic);
		if(bIsKinematicTarget)
		{
			uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
			if (bAllowSubstepping && IsSubstepping(BodySceneType))
			{
				FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
				PhysSubStepper->SetKinematicTarget_AssumesLocked(BodyInstance, TargetTransform);
			}

			const PxTransform PNewPose = U2PTransform(TargetTransform);
			PRigidDynamic->setKinematicTarget(PNewPose);	//If we interpolate, we will end up setting the kinematic target once per sub-step. However, for the sake of scene queries we should do this right away
		}
		else
		{
			const PxTransform PNewPose = U2PTransform(TargetTransform);
			PRigidDynamic->setGlobalPose(PNewPose);
		}
	}
#endif
}

void FPhysScene::AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
{
#if WITH_PHYSX
	uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
	if (IsSubstepping(BodySceneType))
	{
		FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[SceneType_AssumesLocked(BodyInstance)];
		PhysSubStepper->AddCustomPhysics_AssumesLocked(BodyInstance, CalculateCustomPhysics);
	}
	else
	{
		// Since physics frame is set up before "pre-physics" tick group is called, can just fetch delta time from there
		CalculateCustomPhysics.ExecuteIfBound(this->DeltaSeconds, BodyInstance);
	}
#endif
}

void FPhysScene::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForce_AssumesLocked(BodyInstance, Force, bAccelChange);
		}
		else
		{
			PRigidBody->addForce(U2PVector(Force), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddForceAtPosition_AssumesLocked(BodyInstance, Force, Position, bIsLocalForce);
		}
		else if (!bIsLocalForce)
		{
			PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
		else
		{
			PxRigidBodyExt::addLocalForceAtLocalPos(*PRigidBody, U2PVector(Force), U2PVector(Position), PxForceMode::eFORCE, true);
		}
	}
#endif
}

void FPhysScene::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddRadialForceToBody_AssumesLocked(BodyInstance, Origin, Radius, Strength, Falloff, bAccelChange);
		}
		else
		{
			AddRadialForceToPxRigidBody_AssumesLocked(*PRigidBody, Origin, Radius, Strength, Falloff, bAccelChange);
		}
	}
#endif
}

void FPhysScene::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX

	if (PxRigidBody * PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		uint32 BodySceneType = SceneType_AssumesLocked(BodyInstance);
		if (bAllowSubstepping && IsSubstepping(BodySceneType))
		{
			FPhysSubstepTask * PhysSubStepper = PhysSubSteppers[BodySceneType];
			PhysSubStepper->AddTorque_AssumesLocked(BodyInstance, Torque, bAccelChange);
		}
		else
		{
			PRigidBody->addTorque(U2PVector(Torque), bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE, true);
		}
	}
#endif
}

#if WITH_PHYSX
void FPhysScene::RemoveActiveBody_AssumesLocked(FBodyInstance* BodyInstance, uint32 SceneType)
{
	if(PxRigidActor* RigidActor = BodyInstance->GetPxRigidActorFromScene_AssumesLocked(SceneType))
	{
		RemoveActiveRigidActor(SceneType, RigidActor);
	}


	PendingSleepEvents[SceneType].Remove(BodyInstance->GetPxRigidActorFromScene_AssumesLocked(SceneType));
}
#endif
void FPhysScene::TermBody_AssumesLocked(FBodyInstance* BodyInstance)
{
	if (PxRigidBody* PRigidBody = BodyInstance->GetPxRigidBody_AssumesLocked())
	{
		FPhysSubstepTask* PhysSubStepper = PhysSubSteppers[SceneType_AssumesLocked(BodyInstance)];
		PhysSubStepper->RemoveBodyInstance_AssumesLocked(BodyInstance);
	}

	// Remove body from any pending deferred addition / removal
	for(FDeferredSceneData& Deferred : DeferredSceneData)
	{
		int32 FoundIdx = INDEX_NONE;
		if(Deferred.AddInstances.Find(BodyInstance, FoundIdx))
		{
			Deferred.AddActors.RemoveAtSwap(FoundIdx);
			Deferred.AddInstances.RemoveAtSwap(FoundIdx);
		}
	}

#if WITH_PHYSX
	RemoveActiveBody_AssumesLocked(BodyInstance, PST_Sync);
	RemoveActiveBody_AssumesLocked(BodyInstance, PST_Async);
#endif
}

FAutoConsoleTaskPriority CPrio_PhysXStepSimulation(
	TEXT("TaskGraph.TaskPriorities.PhysXStepSimulation"),
	TEXT("Task and thread priority for FPhysSubstepTask::StepSimulation."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

bool FPhysScene::SubstepSimulation(uint32 SceneType, FGraphEventRef &InOutCompletionEvent)
{
#if WITH_PHYSX
	check(SceneType != PST_Cloth); //we don't bother sub-stepping cloth
	float UseDelta = UseSyncTime(SceneType)? SyncDeltaSeconds : DeltaSeconds;
	float SubTime = PhysSubSteppers[SceneType]->UpdateTime(UseDelta);
	PxScene* PScene = GetPhysXScene(SceneType);
	if(SubTime <= 0.f)
	{
		return false;
	}else
	{
		//we have valid scene and subtime so enqueue task
		PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager(), &SimScratchBuffers[SceneType]);
		ENamedThreads::Type NamedThread = PhysSingleThreadedMode() ? ENamedThreads::GameThread : ENamedThreads::SetTaskPriority(ENamedThreads::GameThread, ENamedThreads::HighTaskPriority);

		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SubstepSimulationImp"),
			STAT_FSimpleDelegateGraphTask_SubstepSimulationImp,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(PhysSubSteppers[SceneType], &FPhysSubstepTask::StepSimulation, Task),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SubstepSimulationImp), NULL, NamedThread
		);
		return true;
	}
#endif

}

/** Adds to queue of skelmesh we want to add to collision disable table */
void FPhysScene::DeferredAddCollisionDisableTable(uint32 SkelMeshCompID, TMap<struct FRigidBodyIndexPair, bool> * CollisionDisableTable)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingCollisionDisableTable;
	PendingCollisionDisableTable.SkelMeshCompID = SkelMeshCompID;
	PendingCollisionDisableTable.CollisionDisableTable = CollisionDisableTable;

	DeferredCollisionDisableTableQueue.Add(PendingCollisionDisableTable);
}

/** Adds to queue of skelmesh we want to remove from collision disable table */
void FPhysScene::DeferredRemoveCollisionDisableTable(uint32 SkelMeshCompID)
{
	check(IsInGameThread());

	FPendingCollisionDisableTable PendingDisableCollisionTable;
	PendingDisableCollisionTable.SkelMeshCompID = SkelMeshCompID;
	PendingDisableCollisionTable.CollisionDisableTable = NULL;

	DeferredCollisionDisableTableQueue.Add(PendingDisableCollisionTable);
}

void FPhysScene::FlushDeferredCollisionDisableTableQueue()
{
	check(IsInGameThread());
	for (int32 i = 0; i < DeferredCollisionDisableTableQueue.Num(); ++i)
	{
		FPendingCollisionDisableTable & PendingCollisionDisableTable = DeferredCollisionDisableTableQueue[i];

		if (PendingCollisionDisableTable.CollisionDisableTable)
		{
			CollisionDisableTableLookup.Add(PendingCollisionDisableTable.SkelMeshCompID, PendingCollisionDisableTable.CollisionDisableTable);
		}
		else
		{
			CollisionDisableTableLookup.Remove(PendingCollisionDisableTable.SkelMeshCompID);
		}
	}

	DeferredCollisionDisableTableQueue.Empty();
}

void GatherPhysXStats_AssumesLocked(PxScene* PSyncScene, PxScene* PAsyncScene)
{
	/** Gather PhysX stats */
	if (PSyncScene)
	{
		PxSimulationStatistics SimStats;
		PSyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraints, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodies, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodies, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodies, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodies, SimStats.nbDynamicBodies);

		//SET_DWORD_STAT(STAT_NumBroadphaseAdds, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY));	//TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemoves, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapes, NumShapes);

	}

	if(PAsyncScene)
	{
		//Having to duplicate because of macros. In theory we can fix this but need to get this quickly
		PxSimulationStatistics SimStats;
		PAsyncScene->getSimulationStatistics(SimStats);

		SET_DWORD_STAT(STAT_NumActiveConstraintsAsync, SimStats.nbActiveConstraints);
		SET_DWORD_STAT(STAT_NumActiveSimulatedBodiesAsync, SimStats.nbActiveDynamicBodies);
		SET_DWORD_STAT(STAT_NumActiveKinematicBodiesAsync, SimStats.nbActiveKinematicBodies);
		SET_DWORD_STAT(STAT_NumStaticBodiesAsync, SimStats.nbStaticBodies);
		SET_DWORD_STAT(STAT_NumMobileBodiesAsync, SimStats.nbDynamicBodies);

		//SET_DWORD_STAT(STAT_NumBroadphaseAddsAsync, SimStats.getNbBroadPhaseAdds(PxSimulationStatistics::VolumeType::eRIGID_BODY)); //TODO: These do not seem to work
		//SET_DWORD_STAT(STAT_NumBroadphaseRemovesAsync, SimStats.getNbBroadPhaseRemoves(PxSimulationStatistics::VolumeType::eRIGID_BODY));

		uint32 NumShapes = 0;
		for (int32 GeomType = 0; GeomType < PxGeometryType::eGEOMETRY_COUNT; ++GeomType)
		{
			NumShapes += SimStats.nbShapes[GeomType];
		}

		SET_DWORD_STAT(STAT_NumShapesAsync, NumShapes);
	}
}

DECLARE_FLOAT_COUNTER_STAT(TEXT("Sync Sim Time (ms)"), STAT_PhysSyncSim, STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Async Sim Time (ms)"), STAT_PhysAsyncSim, STATGROUP_Physics);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Cloth Sim Time (ms)"), STAT_PhysClothSim, STATGROUP_Physics);

double GSimStartTime[PST_MAX] = {0.f, 0.f, 0.f};

void FinishSceneStat(uint32 Scene)
{
	if (Scene < PST_MAX)	//PST_MAX used when we don't care
	{
		float SceneTime = float(FPlatformTime::Seconds() - GSimStartTime[Scene]) * 1000.0f;
		switch(Scene)
		{
			case PST_Sync:
			INC_FLOAT_STAT_BY(STAT_PhysSyncSim, SceneTime); break;
			case PST_Async:
			INC_FLOAT_STAT_BY(STAT_PhysAsyncSim, SceneTime); break;
			case PST_Cloth:
			INC_FLOAT_STAT_BY(STAT_PhysClothSim, SceneTime); break;
		}
	}
}

void GatherClothingStats(const UWorld* World)
{
#if STATS
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GatherApexStats);

	SET_DWORD_STAT(STAT_NumCloths, 0);
	SET_DWORD_STAT(STAT_NumClothVerts, 0);

	if ( FThreadStats::IsCollectingData(GET_STATID(STAT_NumCloths)) ||  FThreadStats::IsCollectingData(GET_STATID(STAT_NumClothVerts)) )
	{
		for (TObjectIterator<USkeletalMeshComponent> Itr; Itr; ++Itr)
		{
			if (Itr->GetWorld() != World) { continue; }

			if(const IClothingSimulation* Simulation = Itr->GetClothingSimulation())
			{
				Simulation->GatherStats();
			}
		}
	}
#endif
}

void FPhysScene::MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning)
{
	// If null, or pending kill, do nothing
	if (InSkelComp != nullptr && !InSkelComp->IsPendingKill())
	{
		// If we are already flagged, just need to update info
		if(InSkelComp->bDeferredKinematicUpdate)
		{
			FDeferredKinematicUpdateInfo* Info = DeferredKinematicUpdateSkelMeshes.Find(InSkelComp);
			check(Info != nullptr); // If the bool was set, we must be in the map!
			// If we are currently not going to teleport physics, but this update wants to, we 'upgrade' it
			if (Info->TeleportType == ETeleportType::None && InTeleport == ETeleportType::TeleportPhysics)
			{
				Info->TeleportType = ETeleportType::TeleportPhysics;
			}

			// If we need skinning, remember that
			if (bNeedsSkinning)
			{
				Info->bNeedsSkinning = true;
			}
		}
		// We are not flagged yet..
		else
		{
			// Set info and add to map
			FDeferredKinematicUpdateInfo Info;
			Info.TeleportType = InTeleport;
			Info.bNeedsSkinning = bNeedsSkinning;
			DeferredKinematicUpdateSkelMeshes.Add(InSkelComp, Info);

			// Set flag on component
			InSkelComp->bDeferredKinematicUpdate = true;
		}
	}
}

void FPhysScene::ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp)
{
	// If non-null, and flagged for deferred update..
	if(InSkelComp != nullptr && InSkelComp->bDeferredKinematicUpdate)
	{
		// Remove from map
		int32 NumRemoved = DeferredKinematicUpdateSkelMeshes.Remove(InSkelComp);

		check(NumRemoved == 1); // Should be in map if flag was set!

		// Clear flag
		InSkelComp->bDeferredKinematicUpdate = false;
	}
}


void FPhysScene::UpdateKinematicsOnDeferredSkelMeshes()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateKinematicsOnDeferredSkelMeshes);

	for (TMap< USkeletalMeshComponent*, FDeferredKinematicUpdateInfo >::TIterator It(DeferredKinematicUpdateSkelMeshes); It; ++It)
	{
		USkeletalMeshComponent* SkelComp = (*It).Key;
		FDeferredKinematicUpdateInfo Info = (*It).Value;

		check(SkelComp->bDeferredKinematicUpdate); // Should be true if in map!

		// Perform kinematic updates
		SkelComp->UpdateKinematicBonesToAnim(SkelComp->GetComponentSpaceTransforms(), Info.TeleportType, Info.bNeedsSkinning, EAllowKinematicDeferral::DisallowDeferral);

		// Clear deferred flag
		SkelComp->bDeferredKinematicUpdate = false;
	}

	// Empty map now all is done
	DeferredKinematicUpdateSkelMeshes.Reset();
}


/** Exposes ticking of physics-engine scene outside Engine. */
void FPhysScene::TickPhysScene(uint32 SceneType, FGraphEventRef& InOutCompletionEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime_Async, SceneType == PST_Async);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsKickOffDynamicsTime_Cloth, SceneType == PST_Cloth);

	check(SceneType < NumPhysScenes);

	GSimStartTime[SceneType] = FPlatformTime::Seconds();

	if (bPhysXSceneExecuting[SceneType] != 0)
	{
		// Already executing this scene, must call WaitPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("TickPhysScene: Already executing scene (%d) - aborting."), SceneType);
		return;
	}

	if (IsSubstepping(SceneType))	//we don't bother sub-stepping cloth
	{
		//We're about to start stepping so swap buffers. Might want to find a better place for this?
		PhysSubSteppers[SceneType]->SwapBuffers();
	}

	/**
	* clamp down... if this happens we are simming physics slower than real-time, so be careful with it.
	* it can improve framerate dramatically (really, it is the same as scaling all velocities down and
	* enlarging all timesteps) but at the same time, it will screw with networking (client and server will
	* diverge a lot more.)
	*/

	float UseDelta = FMath::Min(UseSyncTime(SceneType) ? SyncDeltaSeconds : DeltaSeconds, MaxPhysicsDeltaTime);

	// Only simulate a positive time step.
	if (UseDelta <= 0.f)
	{
		if (UseDelta < 0.f)
		{
			// only do this if negative. Otherwise, whenever we pause, this will come up
			UE_LOG(LogPhysics, Warning, TEXT("TickPhysScene: Negative timestep (%f) - aborting."), UseDelta);
		}
		return;
	}

	/**
	* Weight frame time according to PhysScene settings.
	*/
	AveragedFrameTime[SceneType] *= FrameTimeSmoothingFactor[SceneType];
	AveragedFrameTime[SceneType] += (1.0f - FrameTimeSmoothingFactor[SceneType])*UseDelta;

	// Set execution flag
	bPhysXSceneExecuting[SceneType] = true;

	check(!InOutCompletionEvent.GetReference()); // these should be gone because nothing is outstanding
	InOutCompletionEvent = FGraphEvent::CreateGraphEvent();
	bool bTaskOutstanding = false;

	// Update any skeletal meshes that need their bone transforms sent to physics sim
	UpdateKinematicsOnDeferredSkelMeshes();


	float PreTickTime = IsSubstepping(SceneType) ? UseDelta : AveragedFrameTime[SceneType];

	// Broadcast 'pre tick' delegate
	OnPhysScenePreTick.Broadcast(this, SceneType, PreTickTime);

	// If not substepping, call this delegate here. Otherwise we call it in FPhysSubstepTask::SubstepSimulationStart
		if (IsSubstepping(SceneType) == false)
		{
		OnPhysSceneStep.Broadcast(this, SceneType, PreTickTime);
		}


#if WITH_PHYSX

	FlushDeferredActors((EPhysicsSceneType)SceneType);
	DeferredSceneData[SceneType].bIsSimulating = true;

#if !WITH_APEX
	PxScene* PScene = GetPhysXScene(SceneType);
	if (PScene && (UseDelta > 0.f))
#else
	apex::Scene* ApexScene = GetApexScene(SceneType);
	if (ApexScene && UseDelta > 0.f)
#endif
	{
		if(IsSubstepping(SceneType)) //we don't bother sub-stepping cloth
		{
			bTaskOutstanding = SubstepSimulation(SceneType, InOutCompletionEvent);
		}
		else
		{
#if !WITH_APEX
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, PScene->getTaskManager());
			PScene->lockWrite();
			PScene->simulate(AveragedFrameTime[SceneType], Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			PScene->unlockWrite();
			Task->removeReference();
			bTaskOutstanding = true;
#else
			PhysXCompletionTask* Task = new PhysXCompletionTask(InOutCompletionEvent, SceneType, ApexScene->getTaskManager());
			ApexScene->simulate(AveragedFrameTime[SceneType], true, Task, SimScratchBuffers[SceneType].Buffer, SimScratchBuffers[SceneType].BufferSize);
			Task->removeReference();
			bTaskOutstanding = true;
#endif
		}
	}

#endif // WITH_PHYSX

	if (!bTaskOutstanding)
	{
		TArray<FBaseGraphTask*> NewTasks;
		InOutCompletionEvent->DispatchSubsequents(NewTasks, ENamedThreads::AnyThread); // nothing to do, so nothing to wait for
	}
	bSubstepping = UPhysicsSettings::Get()->bSubstepping;
	bSubsteppingAsync = UPhysicsSettings::Get()->bSubsteppingAsync;
}

void FPhysScene::KillVisualDebugger()
{
	if (GPhysXVisualDebugger)
	{
		GPhysXVisualDebugger->disconnect();
	}
}

#if WITH_FLEX

void FPhysScene::WaitFlexScenes()
{
	if (GFlexIsInitialized && FlexContainerMap.Num())
	{
		if (FlexSimulateTaskRef.IsValid())
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(FlexSimulateTaskRef);

		// if debug draw enabled on any containers then ensure any persistent lines are flushed
		bool NeedsFlushDebugLines = false;

		for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
		{
			// The container instances can be removed, so we need to check and handle that case
			if (!It->Value->TemplateRef.IsValid())
			{
				delete It->Value;
				It.RemoveCurrent();
			}
			else if (It->Value->Template->DebugDraw)
			{
				NeedsFlushDebugLines = true;
				break;
			}
		}

		if (FFlexContainerInstance::sGlobalDebugDraw || NeedsFlushDebugLines)
			FlushPersistentDebugLines(OwningWorld);

		// synchronize flex components with results
		for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
			It->Value->Synchronize();
	}

}


void FPhysScene::TickFlexScenesTask(float dt)
{
	// ensure we have the correct CUDA context set for Flex
	// this would be done automatically when making a Flex API call
	// but by acquiring explicitly in advance we save some unnecessary
	// CUDA calls to repeatedly set/unset the context
	NvFlexAcquireContext(GFlexLib);

	for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
	{
		// if template has been garbage collected then remove container (need to use the thread safe IsValid() flag)
		if (!It->Value->TemplateRef.IsValid(false, true))
		{
			delete It->Value;
			It.RemoveCurrent();
		}
		else
		{
			It->Value->Simulate(dt);
		}
	}

	NvFlexRestoreContext(GFlexLib);
}

void FPhysScene::TickFlexScenes(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, float dt)
{
    if (GPhysXSDK)
    if (GFlexIsInitialized)
    {
		// when true the Flex CPU update will be run as a task async to the game thread
		// note that this is different from the async tick in LevelTick.cpp
		const bool bFlexAsync = true;

		if (bFlexAsync)
		{
			FlexSimulateTaskRef = FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::TickFlexScenesTask, dt),
				GET_STATID(STAT_TotalPhysicsTime));
		}
		else
		{
			TickFlexScenesTask(dt);
		}
    }
}
#endif // WITH_FLEX


void FPhysScene::WaitPhysScenes()
{
	check(IsInGameThread());

	FGraphEventArray ThingsToComplete;
	if (PhysicsSceneCompletion.GetReference())
	{
		ThingsToComplete.Add(PhysicsSceneCompletion);
	}
	// Loop through scene types to get all scenes
	// we just wait on everything, though some of these are redundant
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		if (PhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(PhysicsSubsceneCompletion[SceneType]);
		}
		if (FrameLaggedPhysicsSubsceneCompletion[SceneType].GetReference())
		{
			ThingsToComplete.Add(FrameLaggedPhysicsSubsceneCompletion[SceneType]);
		}
	}
	if (ThingsToComplete.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FPhysScene_WaitPhysScenes);
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ThingsToComplete, ENamedThreads::GameThread);
	}
}

void FPhysScene::SceneCompletionTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, EPhysicsSceneType SceneType)
{
	ProcessPhysScene(SceneType);
}

void FPhysScene::ProcessPhysScene(uint32 SceneType)
{
	LLM_SCOPE(ELLMTag::PhysX);
	
	SCOPED_NAMED_EVENT(FPhysScene_ProcessPhysScene, FColor::Orange);
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime_Cloth, SceneType == PST_Cloth);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_PhysicsFetchDynamicsTime_Async, SceneType == PST_Async);

	check(SceneType < NumPhysScenes);
	if (bPhysXSceneExecuting[SceneType] == 0)
	{
		// Not executing this scene, must call TickPhysScene before calling this function again.
		UE_LOG(LogPhysics, Log, TEXT("WaitPhysScene`: Not executing this scene (%d) - aborting."), SceneType);
		return;
	}

	if (FrameLagAsync())
	{
		static_assert(PST_MAX == 3, "Physics scene static test failed."); // Here we assume the PST_Sync is the master and never fame lagged
		if (SceneType == PST_Sync)
		{
			// the one frame lagged one should be done by now.
			check(!FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() || FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete());
		}
		else if (SceneType == PST_Async)
		{
			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = NULL;
		}
	}


	// Reset execution flag

	bool bSuccess = false;
	IgnoreActiveActors[SceneType].Empty();

//This fetches and gets active transforms. It's important that the function that calls this locks because getting the transforms and using the data must be an atomic operation
#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);
	PxU32 OutErrorCode = 0;

#if !WITH_APEX
	PScene->lockWrite();
	bSuccess = PScene->fetchResults(true, &OutErrorCode);
	PScene->unlockWrite();
#else	//	#if !WITH_APEX
	// The APEX scene calls the fetchResults function for the PhysX scene, so we only call ApexScene->fetchResults().
	apex::Scene* ApexScene = GetApexScene(SceneType);
	check(ApexScene);
	bSuccess = ApexScene->fetchResults(true, &OutErrorCode);
#endif	//	#if !WITH_APEX

	if (OutErrorCode != 0)
	{
		UE_LOG(LogPhysics, Log, TEXT("PHYSX FETCHRESULTS ERROR: %d"), OutErrorCode);
	}
#endif // WITH_PHYSX

	PhysicsSubsceneCompletion[SceneType] = NULL;
	bPhysXSceneExecuting[SceneType] = false;

#if WITH_PHYSX
	DeferredSceneData[SceneType].bIsSimulating = false;
	FlushDeferredActors((EPhysicsSceneType)SceneType);
#endif
}

/** Struct to remember a pending component transform change */
struct FPhysScenePendingComponentTransform
{
	/** Component to move */
	TWeakObjectPtr<UPrimitiveComponent> OwningComp;
	/** New transform from physics engine */
	FTransform NewTransform;

	FPhysScenePendingComponentTransform(UPrimitiveComponent* InOwningComp, const FTransform& InNewTransform)
		: OwningComp(InOwningComp)
		, NewTransform(InNewTransform)
	{}
};

void FPhysScene::SyncComponentsToBodies_AssumesLocked(uint32 SceneType)
{
	checkSlow(SceneType < PST_MAX);

	SCOPE_CYCLE_COUNTER(STAT_TotalPhysicsTime);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies, SceneType == PST_Sync);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies_Cloth, SceneType == PST_Cloth);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_SyncComponentsToBodies_Async, SceneType == PST_Async);

#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	check(PScene);

	/** Array of custom sync handlers (plugins) */
	TArray<FCustomPhysXSyncActors*> CustomPhysXSyncActors;

	PxU32 NumActors = 0;
	PxActor** PActiveActors = PScene->getActiveActors(NumActors);

	TArray<FPhysScenePendingComponentTransform> PendingTransforms;

	for (PxU32 TransformIdx = 0; TransformIdx < NumActors; ++TransformIdx)
	{
		PxActor* PActiveActor = PActiveActors[TransformIdx];
#ifdef __EMSCRIPTEN__
		// emscripten doesn't seem to know how to look at <PxRigidActor> from the PxActor class...
		PxRigidActor* XRigidActor = static_cast<PxRigidActor*>(PActiveActor); // is()
		PxRigidActor* RigidActor = XRigidActor->PxRigidActor::isKindOf(PxTypeInfo<PxRigidActor>::name()) ? XRigidActor : NULL; // typeMatch<T>()
#else
		PxRigidActor* RigidActor = PActiveActor->is<PxRigidActor>();
#endif

		if (IgnoreActiveActors[SceneType].Find(RigidActor) != INDEX_NONE)
		{
			continue;
		}

		ensure(!RigidActor->userData || !FPhysxUserData::IsGarbage(RigidActor->userData));

		if (FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(RigidActor->userData))
		{
			if (BodyInstance->InstanceBodyIndex == INDEX_NONE && BodyInstance->OwnerComponent.IsValid())
			{
				check(BodyInstance->OwnerComponent->IsRegistered()); // shouldn't have a physics body for a non-registered component!

				const FTransform NewTransform = BodyInstance->GetUnrealWorldTransform_AssumesLocked();

				// Add to set of transforms to process
				// We can't actually move the component now (or check for out of world), because that could destroy a body
				// elsewhere in the PActiveActors array, resulting in a bad pointer
				FPhysScenePendingComponentTransform NewEntry(BodyInstance->OwnerComponent.Get(), NewTransform);
				PendingTransforms.Add(NewEntry);
			}
		}
		else if (const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(RigidActor->userData))
		{
			if(CustomPayload->CustomSyncActors)
			{
				CustomPhysXSyncActors.AddUnique(CustomPayload->CustomSyncActors);	//NOTE: AddUnique because the assumed number of plugins that rely on this is very small
				CustomPayload->CustomSyncActors->Actors.Add(RigidActor);
			}
		}
	}

	/// Now actually move components
	for (FPhysScenePendingComponentTransform& Entry : PendingTransforms)
	{
		// Check if still valid (ie not destroyed)
		UPrimitiveComponent* OwnerComponent = Entry.OwningComp.Get();
		if (OwnerComponent != nullptr)
		{
			AActor* Owner = OwnerComponent->GetOwner();

			// See if the transform is actually different, and if so, move the component to match physics
			if (!Entry.NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
			{
				const FVector MoveBy = Entry.NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
				const FQuat NewRotation = Entry.NewTransform.GetRotation();

				//@warning: do not reference BodyInstance again after calling MoveComponent() - events from the move could have made it unusable (destroying the actor, SetPhysics(), etc)
				OwnerComponent->MoveComponent(MoveBy, NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
			}

			// Check if we didn't fall out of the world
			if (Owner != NULL && !Owner->IsPendingKill())
			{
				Owner->CheckStillInWorld();
			}
		}
	}


	for(FCustomPhysXSyncActors* CustomSync : CustomPhysXSyncActors)
	{
		CustomSync->SyncToActors_AssumesLocked(SceneType, CustomSync->Actors);
		CustomSync->Actors.Empty(CustomSync->Actors.Num());
	}

	IgnoreActiveActors[SceneType].Empty();
#endif
}

void FPhysScene::DispatchPhysNotifications_AssumesLocked()
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsEventTime);

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		TArray<FCollisionNotifyInfo>& PendingCollisionNotifies = GetPendingCollisionNotifies(SceneType);

		// Let the game-specific PhysicsCollisionHandler process any physics collisions that took place
		if (OwningWorld != NULL && OwningWorld->PhysicsCollisionHandler != NULL)
		{
			OwningWorld->PhysicsCollisionHandler->HandlePhysicsCollisions_AssumesLocked(PendingCollisionNotifies);
		}

		// Fire any collision notifies in the queue.
		for (int32 i = 0; i<PendingCollisionNotifies.Num(); i++)
		{
			FCollisionNotifyInfo& NotifyInfo = PendingCollisionNotifies[i];
			if (NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0)
			{
				if (NotifyInfo.bCallEvent0 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info0.Actor.IsValid())
				{
					NotifyInfo.Info0.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
				}

				// Need to check IsValidForNotify again in case first call broke something.
				if (NotifyInfo.bCallEvent1 && NotifyInfo.IsValidForNotify() && NotifyInfo.Info1.Actor.IsValid())
				{
					NotifyInfo.RigidCollisionData.SwapContactOrders();
					NotifyInfo.Info1.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info1, NotifyInfo.Info0, NotifyInfo.RigidCollisionData);
				}
			}
		}
		PendingCollisionNotifies.Reset();
	}

#if WITH_PHYSX
	for (int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		for (auto MapItr = PendingSleepEvents[SceneType].CreateIterator(); MapItr; ++MapItr)
		{
			PxActor* Actor = MapItr.Key();
			if(FBodyInstance* BodyInstance = FPhysxUserData::Get<FBodyInstance>(Actor->userData))
			{
				if(UPrimitiveComponent* PrimitiveComponent = BodyInstance->OwnerComponent.Get())
				{
					PrimitiveComponent->DispatchWakeEvents(MapItr.Value(), BodyInstance->BodySetup->BoneName);
				}
			}
		}

		PendingSleepEvents[SceneType].Empty();
	}
#endif

	for(int32 SceneType = 0; SceneType < PST_MAX; ++SceneType)
	{
		FPendingConstraintData& ConstraintData = PendingConstraintData[SceneType];
		for(FConstraintBrokenDelegateData& ConstraintBrokenData : ConstraintData.PendingConstraintBroken)
		{
			ConstraintBrokenData.DispatchOnBroken();
		}

		ConstraintData.PendingConstraintBroken.Empty();
	}


	FPhysicsDelegates::OnPhysDispatchNotifications.Broadcast(this);
}

void FPhysScene::SetUpForFrame(const FVector* NewGrav, float InDeltaSeconds, float InMaxPhysicsDeltaTime)
{
	DeltaSeconds = InDeltaSeconds;
	MaxPhysicsDeltaTime = InMaxPhysicsDeltaTime;
#if WITH_PHYSX
	if (NewGrav)
	{
		// Loop through scene types to get all scenes
		for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
		{
			PxScene* PScene = GetPhysXScene(SceneType);
			if (PScene != NULL)
			{
				//@todo phys_thread don't do this if gravity changes

				//@todo, to me it looks like we should avoid this if the gravity has not changed, the lock is probably expensive
				// Lock scene lock, in case it is required
				SCENE_LOCK_WRITE(PScene);

				PScene->setGravity(U2PVector(*NewGrav));

#if WITH_APEX_CLOTHING
				apex::Scene* ApexScene = GetApexScene(SceneType);
				if(SceneType == PST_Cloth && ApexScene)
				{
					ApexScene->updateGravity();
				}
#endif

				// Unlock scene lock, in case it is required
				SCENE_UNLOCK_WRITE(PScene);
			}
		}
	}
#endif
}

FAutoConsoleTaskPriority CPrio_PhyXSceneCompletion(
	TEXT("TaskGraph.TaskPriorities.PhyXSceneCompletion"),
	TEXT("Task and thread priority for PhysicsSceneCompletion."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::HighTaskPriority, // .. at high task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);



void FPhysScene::StartFrame()
{
	FGraphEventArray FinishPrerequisites;

	//Update the collision disable table before ticking
	FlushDeferredCollisionDisableTableQueue();

	// Run the sync scene
	TickPhysScene(PST_Sync, PhysicsSubsceneCompletion[PST_Sync]);
	{
		FGraphEventArray MainScenePrerequisites;
		if (FrameLagAsync() && bAsyncSceneEnabled)
		{
			if (FrameLaggedPhysicsSubsceneCompletion[PST_Async].GetReference() && !FrameLaggedPhysicsSubsceneCompletion[PST_Async]->IsComplete())
			{
				MainScenePrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
				FinishPrerequisites.Add(FrameLaggedPhysicsSubsceneCompletion[PST_Async]);
			}
		}
		if (PhysicsSubsceneCompletion[PST_Sync].GetReference())
		{
			MainScenePrerequisites.Add(PhysicsSubsceneCompletion[PST_Sync]);

			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Sync"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Sync,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Sync),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Sync), &MainScenePrerequisites,
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	if (!FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			new (FinishPrerequisites)FGraphEventRef(
				FDelegateGraphTask::CreateAndDispatchWhenReady(
					FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Async),
					GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
					ENamedThreads::GameThread, ENamedThreads::GameThread
				)
			);
		}
	}

	check(!PhysicsSceneCompletion.GetReference()); // this should have been cleared
	if (FinishPrerequisites.Num())
	{
		if (FinishPrerequisites.Num() > 1)  // we don't need to create a new task if we only have one prerequisite
		{
			DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.ProcessPhysScene_Join"),
				STAT_FNullGraphTask_ProcessPhysScene_Join,
				STATGROUP_TaskGraphTasks);

			PhysicsSceneCompletion = TGraphTask<FNullGraphTask>::CreateTask(&FinishPrerequisites, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
				GET_STATID(STAT_FNullGraphTask_ProcessPhysScene_Join), PhysSingleThreadedMode() ? ENamedThreads::GameThread : CPrio_PhyXSceneCompletion.Get());
		}
		else
		{
			PhysicsSceneCompletion = FinishPrerequisites[0]; // we don't need a join
		}
	}


	// Query clothing stats from skel mesh components in this world
	// This is done outside TickPhysScene because clothing is
	// not related to a scene.
	GatherClothingStats(this->OwningWorld);

	// Record the sync tick time for use with the async tick
	SyncDeltaSeconds = DeltaSeconds;
}

TAutoConsoleVariable<int32> CVarEnableClothPhysics(TEXT("p.ClothPhysics"), 1, TEXT("If 1, physics cloth will be used for simulation."));

void FPhysScene::StartAsync()
{
	FGraphEventArray FinishPrerequisites;

	//If the async scene is lagged we start it here to make sure any cloth in the async scene is using the results of the previous simulation.
	if (FrameLagAsync() && bAsyncSceneEnabled)
	{
		TickPhysScene(PST_Async, PhysicsSubsceneCompletion[PST_Async]);
		if (PhysicsSubsceneCompletion[PST_Async].GetReference())
		{
			DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ProcessPhysScene_Async"),
				STAT_FDelegateGraphTask_ProcessPhysScene_Async,
				STATGROUP_TaskGraphTasks);

			FrameLaggedPhysicsSubsceneCompletion[PST_Async] = FDelegateGraphTask::CreateAndDispatchWhenReady(
				FDelegateGraphTask::FDelegate::CreateRaw(this, &FPhysScene::SceneCompletionTask, PST_Async),
				GET_STATID(STAT_FDelegateGraphTask_ProcessPhysScene_Async), PhysicsSubsceneCompletion[PST_Async],
				ENamedThreads::GameThread, ENamedThreads::GameThread
			);
		}
	}
}

void FPhysScene::EndFrame(ULineBatchComponent* InLineBatcher)
{
	check(IsInGameThread());

	PhysicsSceneCompletion = NULL;

	/**
	* At this point physics simulation has finished. We obtain both scene locks so that the various read/write operations needed can be done quickly.
	* This means that anyone attempting to write on other threads will be blocked. This is OK because accessing any of these game objects from another thread is probably a bad idea!
	*/

	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(PST_Sync));
	SCOPED_SCENE_WRITE_LOCK(bAsyncSceneEnabled ? GetPhysXScene(PST_Async) : nullptr);

#if ( WITH_PHYSX  && !(UE_BUILD_SHIPPING || WITH_PHYSX_RELEASE))
	GatherPhysXStats_AssumesLocked(GetPhysXScene(PST_Sync), HasAsyncScene() ? GetPhysXScene(PST_Async) : nullptr);
#endif

	if (bAsyncSceneEnabled)
	{
		SyncComponentsToBodies_AssumesLocked(PST_Async);
	}

	SyncComponentsToBodies_AssumesLocked(PST_Sync);

	// Perform any collision notification events
	DispatchPhysNotifications_AssumesLocked();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST || WITH_PHYSX_RELEASE)
	// Handle debug rendering
	if (InLineBatcher)
	{
		AddDebugLines(PST_Sync, InLineBatcher);

		if (bAsyncSceneEnabled)
		{
			AddDebugLines(PST_Async, InLineBatcher);
		}

	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST || WITH_PHYSX_RELEASE)
}

#if WITH_PHYSX
/** Helper struct that puts all awake actors to sleep and then later wakes them back up */
struct FHelpEnsureCollisionTreeIsBuilt
{
	FHelpEnsureCollisionTreeIsBuilt(PxScene* InPScene)
	: PScene(InPScene)
	{
		if(PScene)
		{
			SCOPED_SCENE_WRITE_LOCK(PScene);
			const int32 NumActors = PScene->getNbActors(PxActorTypeFlag::eRIGID_DYNAMIC);

			if (NumActors)
			{
				ActorBuffer.AddUninitialized(NumActors);
				PScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, &ActorBuffer[0], NumActors);

				for (PxActor*& PActor : ActorBuffer)
				{
					if (PActor)
					{
						if (PxRigidDynamic* PDynamic = PActor->is<PxRigidDynamic>())
						{
							if (PDynamic->isSleeping() == false)
							{
								PDynamic->putToSleep();
							}
							else
							{
								PActor = nullptr;
							}
						}
					}
				}
			}
		}
	}

	~FHelpEnsureCollisionTreeIsBuilt()
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		for (PxActor* PActor : ActorBuffer)
		{
			if (PActor)
			{
				if (PxRigidDynamic* PDynamic = PActor->is<PxRigidDynamic>())
				{
					PDynamic->wakeUp();
				}
			}
		}
	}

private:

	TArray<PxActor*> ActorBuffer;
	PxScene* PScene;
};

#endif


DECLARE_CYCLE_STAT(TEXT("EnsureCollisionTreeIsBuilt"), STAT_PhysicsEnsureCollisionTreeIsBuilt, STATGROUP_Physics);

void FPhysScene::EnsureCollisionTreeIsBuilt(UWorld* World)
{
	check(IsInGameThread());


	SCOPE_CYCLE_COUNTER(STAT_PhysicsEnsureCollisionTreeIsBuilt);
	//We have to call fetchResults several times to update the internal data structures. PhysX doesn't have an API for this so we have to make all actors sleep before doing this

	SetIsStaticLoading(true);

#if WITH_PHYSX
	FHelpEnsureCollisionTreeIsBuilt SyncSceneHelper(GetPhysXScene(PST_Sync));
	FHelpEnsureCollisionTreeIsBuilt AsyncSceneHelper(HasAsyncScene() ? GetPhysXScene(PST_Async) : nullptr);
#endif

	for (int Iteration = 0; Iteration < 6; ++Iteration)
	{
		World->SetupPhysicsTickFunctions(0.1f);
		StartFrame();
		WaitPhysScenes();
		EndFrame(nullptr);
	}

	SetIsStaticLoading(false);
}

void FPhysScene::SetIsStaticLoading(bool bStaticLoading)
{
	SetPhysXTreeRebuildRateImp(bStaticLoading ? 5 : PhysXTreeRebuildRate);
}

void FPhysScene::SetPhysXTreeRebuildRate(int32 RebuildRate)
{
	PhysXTreeRebuildRate = FMath::Max(4, RebuildRate);
	SetPhysXTreeRebuildRateImp(RebuildRate);
}

void FPhysScene::SetPhysXTreeRebuildRateImp(int32 RebuildRate)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPhysXScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			// Sets the rebuild rate hint, to 1 frame if static loading
			PScene->setDynamicTreeRebuildRateHint(PhysXTreeRebuildRate);

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

#if WITH_PHYSX
/** Utility for looking up the PxScene associated with this FPhysScene. */
PxScene* FPhysScene::GetPhysXScene(uint32 SceneType) const
{
	if(SceneType < NumPhysScenes)
	{
		return GetPhysXSceneFromIndex(PhysXSceneIndex[SceneType]);
	}

	return nullptr;
}

#if WITH_APEX
apex::Scene* FPhysScene::GetApexScene(uint32 SceneType) const
{
	if(SceneType < NumPhysScenes)
	{
		return GetApexSceneFromIndex(PhysXSceneIndex[SceneType]);
	}

	return nullptr;

}
#endif // WITH_APEX

#if WITH_FLEX
FFlexContainerInstance*	FPhysScene::GetFlexContainer(UFlexContainer* Template)
{
	if (GFlexIsInitialized == false)
		return NULL;

	FFlexContainerInstance** Instance = FlexContainerMap.Find(Template);
	if (Instance)
	{
		return *Instance;
	}
	else
	{
		// Make a copy of the UFlexContainer so that modifying it in blueprint doesn't change the asset
		// The owning object will be the Transient Pacakge
		auto ContainerCopy = DuplicateObject<UFlexContainer>(Template, GetTransientPackage()); 
		
		// no Garbage Collection please, we need this object to last as long as the FFlexContainerInstance
		ContainerCopy->AddToRoot();
		FFlexContainerInstance* NewInst = new FFlexContainerInstance(ContainerCopy, this);
		FlexContainerMap.Add(Template, NewInst);

		return NewInst;
	}
}

void FPhysScene::StartFlexRecord()
{
	/*
	for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
	{
		FFlexContainerInstance* Container = It->Value;
		FString Name = Container->Template->GetName();

		flexStartRecord(Container->Solver, StringCast<ANSICHAR>(*(FString("flexCapture_") + Name + FString(".flx"))).Get());
	}
	*/
}

void FPhysScene::StopFlexRecord()
{
	/*
	for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
	{
		FFlexContainerInstance* Container = It->Value;
		
		flexStopRecord(Container->Solver);
	}
	*/
}


void FPhysScene::AddRadialForceToFlex(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff)
{
	for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
	{
		FFlexContainerInstance* Container = It->Value;
		Container->AddRadialForce(Origin, Radius, Strength, Falloff);
	}
}

void FPhysScene::AddRadialImpulseToFlex(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	for (auto It = FlexContainerMap.CreateIterator(); It; ++It)
	{
		FFlexContainerInstance* Container = It->Value;
		Container->AddRadialImpulse(Origin, Radius, Strength, Falloff, bVelChange);
	}
}
#endif // WITH_FLEX


static void BatchPxRenderBufferLines(class ULineBatchComponent& LineBatcherToUse, const PxRenderBuffer& DebugData)
{
	int32 NumPoints = DebugData.getNbPoints();
	if (NumPoints > 0)
	{
		const PxDebugPoint* Points = DebugData.getPoints();
		for (int32 i = 0; i<NumPoints; i++)
		{
			LineBatcherToUse.DrawPoint(P2UVector(Points->pos), FColor((uint32)Points->color), 2, SDPG_World);

			Points++;
		}
	}

	// Build a list of all the lines we want to draw
	TArray<FBatchedLine> DebugLines;

	// Add all the 'lines' from PhysX
	int32 NumLines = DebugData.getNbLines();
	if (NumLines > 0)
	{
		const PxDebugLine* Lines = DebugData.getLines();
		for (int32 i = 0; i<NumLines; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Lines->pos0), P2UVector(Lines->pos1), FColor((uint32)Lines->color0), 0.f, 0.0f, SDPG_World);
			Lines++;
		}
	}

	// Add all the 'triangles' from PhysX
	int32 NumTris = DebugData.getNbTriangles();
	if (NumTris > 0)
	{
		const PxDebugTriangle* Triangles = DebugData.getTriangles();
		for (int32 i = 0; i<NumTris; i++)
		{
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos0), P2UVector(Triangles->pos1), FColor((uint32)Triangles->color0), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos1), P2UVector(Triangles->pos2), FColor((uint32)Triangles->color1), 0.f, 0.0f, SDPG_World);
			new(DebugLines)FBatchedLine(P2UVector(Triangles->pos2), P2UVector(Triangles->pos0), FColor((uint32)Triangles->color2), 0.f, 0.0f, SDPG_World);
			Triangles++;
		}
	}

	// Draw them all in one call.
	if (DebugLines.Num() > 0)
	{
		LineBatcherToUse.DrawLines(DebugLines);
	}
}
#endif // WITH_PHYSX


/** Add any debug lines from the physics scene to the supplied line batcher. */
void FPhysScene::AddDebugLines(uint32 SceneType, class ULineBatchComponent* LineBatcherToUse)
{
	check(SceneType < NumPhysScenes);

	if (LineBatcherToUse)
	{
#if WITH_PHYSX
		// Render PhysX debug data
		PxScene* PScene = GetPhysXScene(SceneType);
		const PxRenderBuffer& DebugData = PScene->getRenderBuffer();
		BatchPxRenderBufferLines(*LineBatcherToUse, DebugData);
#if WITH_APEX
		// Render APEX debug data
		apex::Scene* ApexScene = GetApexScene(SceneType);
		const PxRenderBuffer* RenderBuffer = ApexScene->getRenderBuffer();
		if (RenderBuffer != NULL)
		{
			BatchPxRenderBufferLines(*LineBatcherToUse, *RenderBuffer);
			ApexScene->updateRenderResources();
		}
#endif	// WITH_APEX
#endif	// WITH_PHYSX
	}
}

bool FPhysScene::IsSubstepping(uint32 SceneType) const
{
	// Substepping relies on interpolating transforms over frames, but only game worlds will be ticked,
	// so we disallow this feature in non-game worlds.
	if (!OwningWorld || !OwningWorld->IsGameWorld())
	{
		return false;
	}

	if (SceneType == PST_Sync)
	{
		return bSubstepping;
	}

	if (SceneType == PST_Async)
	{
		return bSubsteppingAsync;
	}

	return false;
}


void FPhysScene::ApplyWorldOffset(FVector InOffset)
{
#if WITH_PHYSX
	// Loop through scene types to get all scenes
	for (uint32 SceneType = 0; SceneType < NumPhysScenes; ++SceneType)
	{
		PxScene* PScene = GetPhysXScene(SceneType);
		if (PScene != NULL)
		{
			// Lock scene lock, in case it is required
			SCENE_LOCK_WRITE(PScene);

			PScene->shiftOrigin(U2PVector(-InOffset));

			// Unlock scene lock, in case it is required
			SCENE_UNLOCK_WRITE(PScene);
		}
	}
#endif
}

void FPhysScene::InitPhysScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX

	int64 NumPhysxDispatcher = 0;
	FParse::Value(FCommandLine::Get(), TEXT("physxDispatcher="), NumPhysxDispatcher);
	if (NumPhysxDispatcher == 0 && FParse::Param(FCommandLine::Get(), TEXT("physxDispatcher")))
	{
		NumPhysxDispatcher = 4;	//by default give physx 4 threads
	}

	// Create dispatcher for tasks
	if (PhysSingleThreadedMode())
	{
		if(SceneType == PST_Cloth)
		{
			CPUDispatcher[SceneType] = new FPhysXCPUDispatcherSingleThread<true>();
		}
		else
		{
			CPUDispatcher[SceneType] = new FPhysXCPUDispatcherSingleThread<false>();
		}
	}
	else
	{
		if (NumPhysxDispatcher)
		{
			CPUDispatcher[SceneType] = PxDefaultCpuDispatcherCreate(NumPhysxDispatcher);
		}
		else
		{
			if (SceneType == PST_Cloth)
			{
				CPUDispatcher[SceneType] = new FPhysXCPUDispatcher<true>();
			}
			else
			{
				CPUDispatcher[SceneType] = new FPhysXCPUDispatcher<false>();
			}

		}

	}


	PhysxUserData = FPhysxUserData(this);

	// Create sim event callback
	SimEventCallback[SceneType] = SimEventCallbackFactory.IsValid() ? SimEventCallbackFactory->Create(this, SceneType) : new FPhysXSimEventCallback(this, SceneType);



	// Include scene descriptor in loop, so that we might vary it with scene type
	PxSceneDesc PSceneDesc(GPhysXSDK->getTolerancesScale());
	PSceneDesc.cpuDispatcher = CPUDispatcher[SceneType];

	FPhysSceneShaderInfo PhysSceneShaderInfo;
	PhysSceneShaderInfo.PhysScene = this;
	PSceneDesc.filterShaderData = &PhysSceneShaderInfo;
	PSceneDesc.filterShaderDataSize = sizeof(PhysSceneShaderInfo);

	PSceneDesc.filterShader = GSimulationFilterShader ? GSimulationFilterShader : PhysXSimFilterShader;
	PSceneDesc.simulationEventCallback = SimEventCallback[SceneType];

	if(UPhysicsSettings::Get()->bEnablePCM)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_PCM;
	}
	else
	{
		PSceneDesc.flags &= ~PxSceneFlag::eENABLE_PCM;
	}

	if (UPhysicsSettings::Get()->bEnableStabilization)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_STABILIZATION;
	}
	else
	{
		PSceneDesc.flags &= ~PxSceneFlag::eENABLE_STABILIZATION;
	}

	// Set bounce threshold
	PSceneDesc.bounceThresholdVelocity = UPhysicsSettings::Get()->BounceThresholdVelocity;

	// If we're frame lagging the async scene (truly running it async) then use the scene lock
#if USE_SCENE_LOCK
	if(UPhysicsSettings::Get()->bWarnMissingLocks)
	{
		PSceneDesc.flags |= PxSceneFlag::eREQUIRE_RW_LOCK;
	}

#endif

	if(!UPhysicsSettings::Get()->bDisableActiveActors)
	{
		// We want to use 'active actors'
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ACTIVE_ACTORS;
		PSceneDesc.flags |= PxSceneFlag::eEXCLUDE_KINEMATICS_FROM_ACTIVE_ACTORS;
	}

	// enable CCD at scene level
	if (UPhysicsSettings::Get()->bDisableCCD == false)
	{
		PSceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
	}

	// Need to turn this on to consider kinematics turning into dynamic. Otherwise, you'll need to call resetFiltering to do the expensive broadphase reinserting
	PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_STATIC_PAIRS;
	PSceneDesc.flags |= PxSceneFlag::eENABLE_KINEMATIC_PAIRS;	//this is only needed for destruction, but unfortunately this flag cannot be modified after creation and the plugin has no hook (yet)

	// @TODO Should we set up PSceneDesc.limits? How?

	// Do this to improve loading times, esp. for streaming in sublevels
	PSceneDesc.staticStructure = PxPruningStructureType::eDYNAMIC_AABB_TREE;
	// Default to rebuilding tree slowly
	PSceneDesc.dynamicTreeRebuildRateHint = PhysXTreeRebuildRate;

	if (UPhysicsSettings::Get()->bEnableEnhancedDeterminism) {
		PSceneDesc.flags |= PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;
	}

	bool bIsValid = PSceneDesc.isValid();
	if (!bIsValid)
	{
		UE_LOG(LogPhysics, Log, TEXT("Invalid PSceneDesc"));
	}

	// Create scene, and add to map
	PxScene* PScene = GPhysXSDK->createScene(PSceneDesc);

#if WITH_APEX
	// Build the APEX scene descriptor for the PhysX scene
	apex::SceneDesc ApexSceneDesc;
	ApexSceneDesc.scene = PScene;
	// This interface allows us to modify the PhysX simulation filter shader data with contact pair flags
	ApexSceneDesc.physX3Interface = GPhysX3Interface;

	// Create the APEX scene from our descriptor
	apex::Scene* ApexScene = GApexSDK->createScene(ApexSceneDesc);

	// This enables debug rendering using the "legacy" method, not using the APEX render API
	ApexScene->setUseDebugRenderable(true);

	// Allocate a view matrix for APEX scene LOD
	ApexScene->allocViewMatrix(apex::ViewMatrixType::LOOK_AT_RH);

	// Add the APEX scene to the map instead of the PhysX scene, since we can access the latter through the former
	GPhysXSceneMap.Add(PhysXSceneCount, ApexScene);
#else	// #if WITH_APEX
	GPhysXSceneMap.Add(PhysXSceneCount, PScene);
#endif	// #if WITH_APEX

	// Save pointer to FPhysScene in userdata
	PScene->userData = &PhysxUserData;
#if WITH_APEX
	ApexScene->userData = &PhysxUserData;
#endif

	// Store index of PhysX Scene in this FPhysScene
	this->PhysXSceneIndex[SceneType] = PhysXSceneCount;

	// Increment scene count
	PhysXSceneCount++;

	//Initialize substeppers
	//we don't bother sub-stepping cloth
#if WITH_PHYSX
#if WITH_APEX
	PhysSubSteppers[SceneType] = SceneType == PST_Cloth ? NULL : new FPhysSubstepTask(ApexScene, this, SceneType);
#else
	PhysSubSteppers[SceneType] = SceneType == PST_Cloth ? NULL : new FPhysSubstepTask(PScene, this, SceneType);
#endif

	if (PxPvdSceneClient* PVDSceneClient = PScene->getScenePvdClient())
	{
		PVDSceneClient->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONTACTS | PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES | PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS);
	}
#endif

	FPhysicsDelegates::OnPhysSceneInit.Broadcast(this, (EPhysicsSceneType)SceneType);

#endif // WITH_PHYSX
}

void FPhysScene::TermPhysScene(uint32 SceneType)
{
	check(SceneType < NumPhysScenes);

#if WITH_PHYSX
	PxScene* PScene = GetPhysXScene(SceneType);
	if (PScene != NULL)
	{
#if WITH_APEX
		apex::Scene* ApexScene = GetApexScene(SceneType);
		if (ApexScene != NULL)
		{
			GPhysCommandHandler->DeferredRelease(ApexScene);
		}
#endif // #if WITH_APEX

		FPhysicsDelegates::OnPhysSceneTerm.Broadcast(this, (EPhysicsSceneType)SceneType);

		delete PhysSubSteppers[SceneType];
		PhysSubSteppers[SceneType] = NULL;

		// @todo block on any running scene before calling this
		GPhysCommandHandler->DeferredRelease(PScene);
		GPhysCommandHandler->DeferredDeleteSimEventCallback(SimEventCallback[SceneType]);

		// Commands may have accumulated as the scene is terminated - flush any commands for this scene.
		GPhysCommandHandler->Flush();

		// Remove from the map
		GPhysXSceneMap.Remove(PhysXSceneIndex[SceneType]);
	}
#endif
}

void FPhysScene::AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType)
{
	PendingConstraintData[SceneType].PendingConstraintBroken.Add( FConstraintBrokenDelegateData(ConstraintInstance) );
}

FConstraintBrokenDelegateData::FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance)
	: OnConstraintBrokenDelegate(ConstraintInstance->OnConstraintBrokenDelegate)
	, ConstraintIndex(ConstraintInstance->ConstraintIndex)
{

}

#if WITH_PHYSX

void FPhysScene::AddPendingSleepingEvent(PxActor* Actor, SleepEvent::Type SleepEventType, int32 SceneType)
{
	PendingSleepEvents[SceneType].FindOrAdd(Actor) = SleepEventType;
}

FPhysScene::FDeferredSceneData::FDeferredSceneData()
{
	bIsSimulating = false;
}

void FPhysScene::FDeferredSceneData::FlushDeferredActors_AssumesLocked(PxScene* Scene)
{
	check(AddInstances.Num() == AddActors.Num());

	if (AddInstances.Num() > 0)
	{
		if(!bIsSimulating)
		{
			//This is the fast path, but it's only allowed when the physx simulation is not currently running
			Scene->addActors(AddActors.GetData(), AddActors.Num());
		}
		else
		{
			for(PxActor* Actor : AddActors)
			{
				Scene->addActor(*Actor);
			}
		}


		int32 Idx = -1;
		for (FBodyInstance* Instance : AddInstances)
		{
			++Idx;
			Instance->CurrentSceneState = BodyInstanceSceneState::Added;

			if(Instance->GetPxRigidDynamic_AssumesLocked())
			{
				// Extra setup necessary for dynamic objects.
				Instance->InitDynamicProperties_AssumesLocked();
			}
		}

		AddInstances.Empty();
		AddActors.Empty();
	}

	check(RemoveInstances.Num() == RemoveActors.Num());

	if (RemoveInstances.Num() > 0)
	{
		Scene->removeActors(RemoveActors.GetData(), RemoveActors.Num());

		for (FBodyInstance* Instance : AddInstances)
		{
			Instance->CurrentSceneState = BodyInstanceSceneState::Removed;
		}

		RemoveInstances.Empty();
		RemoveActors.Empty();
	}
}

void FPhysScene::FDeferredSceneData::DeferAddActor_AssumesLocked(FBodyInstance* OwningInstance, PxActor* Actor)
{
	// Allowed to be unadded or awaiting add here (objects can be in more than one scene
	if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::NotAdded ||
		OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingAdd)
	{
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::AwaitingAdd;
		AddInstances.Add(OwningInstance);
		AddActors.Add(Actor);
	}
	else if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingRemove)
	{
		// We were waiting to be removed, but we're canceling that

		OwningInstance->CurrentSceneState = BodyInstanceSceneState::Added;
		RemoveInstances.RemoveSingle(OwningInstance);
		RemoveActors.RemoveSingle(Actor);
	}
}

void FPhysScene::DeferAddActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType)
{
	check(OwningInstance && Actor);
	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(SceneType));

	DeferredSceneData[SceneType].DeferAddActor_AssumesLocked(OwningInstance, Actor);
}


void FPhysScene::FDeferredSceneData::DeferAddActors_AssumesLocked(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors)
{
	int32 Num = OwningInstances.Num();
	AddInstances.Reserve(AddInstances.Num() + Num);
	AddActors.Reserve(AddActors.Num() + Num);

	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		DeferAddActor_AssumesLocked(OwningInstances[Idx], Actors[Idx]);
	}
}

void FPhysScene::DeferAddActors(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors, EPhysicsSceneType SceneType)
{
	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(SceneType));
	DeferredSceneData[SceneType].DeferAddActors_AssumesLocked(OwningInstances, Actors);
}

void FPhysScene::FDeferredSceneData::DeferRemoveActor_AssumesLocked(FBodyInstance* OwningInstance, PxActor* Actor)
{
	if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::Added ||
		OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingRemove)
	{
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::AwaitingRemove;
		RemoveInstances.Add(OwningInstance);
		RemoveActors.Add(Actor);
	}
	else if (OwningInstance->CurrentSceneState == BodyInstanceSceneState::AwaitingAdd)
	{
		// We were waiting to add but now we're canceling it
		OwningInstance->CurrentSceneState = BodyInstanceSceneState::Removed;
		AddInstances.RemoveSingle(OwningInstance);
		AddActors.RemoveSingle(Actor);
	}
}

void FPhysScene::FDeferredSceneData::DeferRemoveActors_AssumesLocked(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors)
{
	check(OwningInstances.Num() == Actors.Num());
	const int32 Num = OwningInstances.Num();

	for(int32 Idx = 0; Idx < Num; ++Idx)
	{
		DeferRemoveActor_AssumesLocked(OwningInstances[Idx], Actors[Idx]);
	}
}


void FPhysScene::DeferRemoveActor(FBodyInstance* OwningInstance, PxActor* Actor, EPhysicsSceneType SceneType)
{
	check(OwningInstance && Actor);
	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(SceneType));

	DeferredSceneData[SceneType].DeferRemoveActor_AssumesLocked(OwningInstance, Actor);
}

void FPhysScene::DeferRemoveActors(const TArray<FBodyInstance*>& OwningInstances, const TArray<PxActor*>& Actors, EPhysicsSceneType SceneType)
{
	SCOPED_SCENE_WRITE_LOCK(GetPhysXScene(SceneType));
	DeferredSceneData[SceneType].DeferRemoveActors_AssumesLocked(OwningInstances, Actors);
}

void FPhysScene::FlushDeferredActors(EPhysicsSceneType SceneType)
{
	PxScene* Scene = GetPhysXScene(SceneType);
	SCOPED_SCENE_WRITE_LOCK(Scene);
	DeferredSceneData[SceneType].FlushDeferredActors_AssumesLocked(Scene);
}

#endif
