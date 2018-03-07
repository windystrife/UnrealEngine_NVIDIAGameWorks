// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "HAL/ThreadSafeBool.h"
#if WITH_PHYSX
#include "PhysicsEngine/PhysXSupport.h"
#endif

void FinishSceneStat(uint32 Scene);

//This is only here for now while we transition into substepping
#if WITH_PHYSX
class PhysXCompletionTask : public PxLightCpuTask
{
	FGraphEventRef EventToFire;
	uint32 Scene;

	// Scratch memory for call to PxScene::simulate. This is owned by the FPhysScene that spawned this
	// task and is guaranteed to remain valid till FPhysScene destruction - can be nullptr if size
	// was set to zero or a buffer isn't required for this scene
	FSimulationScratchBuffer* ScratchBuffer;

public:
	PhysXCompletionTask()
		: Scene(0)
		, ScratchBuffer(nullptr)
	{}

	PhysXCompletionTask(FGraphEventRef& InEventToFire, uint32 InScene, PxTaskManager* InTaskManager, FSimulationScratchBuffer* InScratchBuffer = nullptr)
		: EventToFire(InEventToFire)
		, Scene(InScene)
		, ScratchBuffer(InScratchBuffer)
	{
		setContinuation(*InTaskManager, NULL);
	}
	virtual void run()
	{
	}
	virtual void release()
	{
		PxLightCpuTask::release();
		FinishSceneStat(Scene);
		if (EventToFire.GetReference())
		{
			TArray<FBaseGraphTask*> NewTasks;
			EventToFire->DispatchSubsequents(NewTasks);
		}
		delete this;
	}
	virtual const char *getName() const
	{
		return "CompleteSimulate";
	}

	uint8* GetScratchBufferData()
	{
		return ScratchBuffer ? ScratchBuffer->Buffer : nullptr;
	}

	int32 GetScratchBufferSize()
	{
		return ScratchBuffer ? ScratchBuffer->BufferSize : 0;
	}
};
#endif	//#if WITH_PHYSX


/** Hold information about kinematic target */
struct FKinematicTarget
{
	FKinematicTarget() : BodyInstance(0) {}
	FKinematicTarget(FBodyInstance* Body, const FTransform& TM) :
		BodyInstance(Body),
		TargetTM(TM),
		OriginalTM(Body->GetUnrealWorldTransform(true, true))
	{
	}

	/** Kinematic actor we are setting target for*/
	FBodyInstance* BodyInstance;

	/** Target transform for kinematic actor*/
	FTransform TargetTM;

	/** Start transform for kinematic actor*/
	FTransform OriginalTM;
};

/** Holds information about requested force */
struct FForceTarget
{
	FForceTarget(){}
	FForceTarget(const FVector& GivenForce) : Force(GivenForce), bPosition(false){}
	FForceTarget(const FVector& GivenForce, const FVector& GivenPosition) : Force(GivenForce), Position(GivenPosition), bPosition(true){}
	FVector Force;
	FVector Position;
	bool bPosition;
	bool bAccelChange;
	bool bIsLocalForce;
};

struct FTorqueTarget
{
	FTorqueTarget(){}
	FTorqueTarget(const FVector& GivenTorque) : Torque(GivenTorque){}
	FVector Torque;
	bool bAccelChange;
};

struct FRadialForceTarget
{
	FRadialForceTarget(){}
	FVector Origin;
	float Radius;
	float Strength;
	uint8 Falloff;
	bool bAccelChange;
};

struct FCustomTarget
{
	FCustomTarget(){ }
	FCustomTarget(const FCalculateCustomPhysics& GivenCalculateCustomPhysics) : CalculateCustomPhysics(&GivenCalculateCustomPhysics){}
	const FCalculateCustomPhysics* CalculateCustomPhysics;
};

/** Holds information on everything we need to fix for substepping of a single frame */
struct FPhysTarget
{
	FPhysTarget() : bKinematicTarget(false){}

	TArray<FForceTarget>  Forces;	//we can apply force at multiple places
	TArray<FTorqueTarget> Torques;
	TArray<FRadialForceTarget> RadialForces;
	TArray<FCustomTarget> CustomPhysics; //for calculating custom physics forces
	FKinematicTarget KinematicTarget;

	/** Tells us if the kinematic target has been set */
	bool bKinematicTarget;
	
	
};

#if WITH_PHYSX
#if WITH_APEX
typedef apex::Scene PxApexScene;	//helper typedef so we don't have to use as many ifdefs
#else
typedef PxScene PxApexScene;
#endif
#endif

/** Holds information used for substepping a scene */
class FPhysSubstepTask
{
public:
#if WITH_PHYSX
	FPhysSubstepTask(PxApexScene * GivenScene, FPhysScene* InPhysScene, uint32 InSceneType);
#endif

	void SetKinematicTarget_AssumesLocked(FBodyInstance* Body, const FTransform& TM);
	bool GetKinematicTarget_AssumesLocked(const FBodyInstance* Body, FTransform& OutTM) const;
	void AddCustomPhysics_AssumesLocked(FBodyInstance* Body, const FCalculateCustomPhysics& CalculateCustomPhysics);
	void AddForce_AssumesLocked(FBodyInstance* Body, const FVector& Force, bool bAccelChange);
	void AddForceAtPosition_AssumesLocked(FBodyInstance* Body, const FVector& Force, const FVector& Position, bool bIsLocalForce);
	void AddTorque_AssumesLocked(FBodyInstance* Body, const FVector& Torque, bool bAccelChange);
	void AddRadialForceToBody_AssumesLocked(FBodyInstance* Body, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, const bool bAccelChange);

	/** Removes a BodyInstance from doing substep work - should only be called when the FBodyInstance is getting destroyed */
	void RemoveBodyInstance_AssumesLocked(FBodyInstance* Body);

	
	void SwapBuffers();
	float UpdateTime(float UseDelta);

	void SubstepSimulationStart();
	void SubstepSimulationEnd(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
#if WITH_PHYSX
	void StepSimulation(PhysXCompletionTask * Task);
#endif
		
private:

	/** Applies interpolation and forces on all needed actors*/
	void SubstepInterpolation(float Scale, float DeltaTime);
	void ApplyCustomPhysics(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance, float DeltaTime);
	void ApplyForces_AssumesLocked(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance);
	void ApplyTorques_AssumesLocked(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance);
	void ApplyRadialForces_AssumesLocked(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance);
	void InterpolateKinematicActor_AssumesLocked(const FPhysTarget& PhysTarget, FBodyInstance* BodyInstance, float Alpha);

	typedef TMap<FBodyInstance*, FPhysTarget> PhysTargetMap;
	PhysTargetMap PhysTargetBuffers[2];	//need to double buffer between physics thread and game thread
	uint32 NumSubsteps;
	float SubTime;
	float DeltaSeconds;
	FThreadSafeBool External;
	class  PhysXCompletionTask * FullSimulationTask;
	float Alpha;
	float StepScale;
	float TotalSubTime;
	uint32 CurrentSubStep;
	FGraphEventRef CompletionEvent;

	FPhysScene* PhysScene;
	uint32 SceneType;

#if WITH_PHYSX
	PxApexScene * PAScene;
#endif

};
