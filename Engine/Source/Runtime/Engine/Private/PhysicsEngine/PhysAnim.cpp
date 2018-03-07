// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysAnim.cpp: Code for supporting animation/physics blending
=============================================================================*/ 

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimStats.h"
#include "SkeletalRenderPublic.h"
#include "SkeletalMeshTypes.h"
#include "Components/LineBatchComponent.h"
#if WITH_PHYSX
	#include "PhysXPublic.h"
#endif // WITH_PHYSX
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"


/** Used for drawing pre-phys skeleton if bShowPrePhysBones is true */
static const FColor AnimSkelDrawColor(255, 64, 64);

// Temporary workspace for caching world-space matrices.
struct FAssetWorldBoneTM
{
	FTransform	TM;			// Should never contain scaling.
	bool bUpToDate;			// If this equals PhysAssetUpdateNum, then the matrix is up to date.
};


FAutoConsoleTaskPriority CPrio_FParallelBlendPhysicsTask(
	TEXT("TaskGraph.TaskPriorities.ParallelBlendPhysicsTask"),
	TEXT("Task and thread priority for FParallelBlendPhysicsTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FParallelBlendPhysicsTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelBlendPhysicsTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelBlendPhysicsTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FParallelBlendPhysicsTask.Get();
	}
	static FORCEINLINE ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (USkeletalMeshComponent* Comp = SkeletalMeshComponent.Get())
		{
			SCOPED_NAMED_EVENT(FParallelBlendPhysicsTask_DoTask, FColor::Yellow);
			Comp->ParallelBlendPhysics();
		}
	}
};

class FParallelBlendPhysicsCompletionTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelBlendPhysicsCompletionTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelBlendPhysicsCompletionTask, STATGROUP_TaskGraphTasks);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPED_NAMED_EVENT(FParallelBlendPhysicsCompletionTask_DoTask, FColor::Yellow);
		SCOPE_CYCLE_COUNTER(STAT_AnimGameThreadTime);
		if (USkeletalMeshComponent* Comp = SkeletalMeshComponent.Get())
		{
			Comp->CompleteParallelBlendPhysics();
		}
	}
};

typedef TArray<FAssetWorldBoneTM, TMemStackAllocator<alignof(FAssetWorldBoneTM)>> TAssetWorldBoneTMArray;
// Use current pose to calculate world-space position of this bone without physics now.
void UpdateWorldBoneTM(TAssetWorldBoneTMArray& WorldBoneTMs, const TArray<FTransform>& InBoneSpaceTransforms, int32 BoneIndex, USkeletalMeshComponent* SkelComp, const FTransform &LocalToWorldTM, const FVector& Scale3D)
{
	// If its already up to date - do nothing
	if(	WorldBoneTMs[BoneIndex].bUpToDate )
	{
		return;
	}

	FTransform ParentTM, RelTM;
	if(BoneIndex == 0)
	{
		// If this is the root bone, we use the mesh component LocalToWorld as the parent transform.
		ParentTM = LocalToWorldTM;
	}
	else
	{
		// If not root, use our cached world-space bone transforms.
		int32 ParentIndex = SkelComp->SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
		UpdateWorldBoneTM(WorldBoneTMs, InBoneSpaceTransforms, ParentIndex, SkelComp, LocalToWorldTM, Scale3D);
		ParentTM = WorldBoneTMs[ParentIndex].TM;
	}

	RelTM = InBoneSpaceTransforms[BoneIndex];
	RelTM.ScaleTranslation( Scale3D );

	WorldBoneTMs[BoneIndex].TM = RelTM * ParentTM;
	WorldBoneTMs[BoneIndex].bUpToDate = true;
}
TAutoConsoleVariable<int32> CVarPhysicsAnimBlendUpdatesPhysX(TEXT("p.PhysicsAnimBlendUpdatesPhysX"), 1, TEXT("Whether to update the physx simulation with the results of physics animation blending"));

void USkeletalMeshComponent::PerformBlendPhysicsBones(const TArray<FBoneIndexType>& InRequiredBones, TArray<FTransform>& InBoneSpaceTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_BlendInPhysics);
	// Get drawscale from Owner (if there is one)
	FVector TotalScale3D = GetComponentTransform().GetScale3D();
	FVector RecipScale3D = TotalScale3D.Reciprocal();

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	check( PhysicsAsset );

	if (GetNumComponentSpaceTransforms() == 0)
	{
		return;
	}

	// Get the scene, and do nothing if we can't get one.
	FPhysScene* PhysScene = nullptr;
	if (GetWorld() != nullptr)
	{
		PhysScene = GetWorld()->GetPhysicsScene();
	}

	if (PhysScene == nullptr)
	{
		return;
	}

	FMemMark Mark(FMemStack::Get());
	// Make sure scratch space is big enough.
	TAssetWorldBoneTMArray WorldBoneTMs;
	WorldBoneTMs.AddZeroed(GetNumComponentSpaceTransforms());
	
	FTransform LocalToWorldTM = GetComponentTransform();
	LocalToWorldTM.RemoveScaling();

	TArray<FTransform>& EditableComponentSpaceTransforms = GetEditableComponentSpaceTransforms();

	struct FBodyTMPair
	{
		FBodyInstance* BI;
		FTransform TM;
	};

#define DEPRECATED_PHYSBLEND_UPDATES_PHYSX 0	//If you really need the old behavior of physics blending set this to 1. Note this is inefficient and doesn't lead to good results. Please use UPhysicalAnimationComponent
#if DEPRECATED_PHYSBLEND_UPDATES_PHYSX
	TArray<FBodyTMPair, TMemStackAllocator<alignof(FBodyTMPair)>> PendingBodyTMs;
#endif

#if WITH_PHYSX
	{
		const uint32 SceneType = GetPhysicsSceneType(*PhysicsAsset, *PhysScene, UseAsyncScene);
		SCOPED_SCENE_READ_LOCK(PhysScene->GetPhysXScene(SceneType));
#endif

		bool bSetParentScale = false;
		const bool bSimulatedRootBody = Bodies.IsValidIndex(RootBodyData.BodyIndex) && Bodies[RootBodyData.BodyIndex]->IsInstanceSimulatingPhysics();
		const FTransform NewComponentToWorld = bSimulatedRootBody ? GetComponentTransformFromBodyInstance(Bodies[RootBodyData.BodyIndex]) : FTransform::Identity;

		// For each bone - see if we need to provide some data for it.
		for(int32 i=0; i<InRequiredBones.Num(); i++)
		{
			int32 BoneIndex = InRequiredBones[i];

			// See if this is a physics bone..
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(SkeletalMesh->RefSkeleton.GetBoneName(BoneIndex));
			// need to update back to physX so that physX knows where it was after blending
#if DEPRECATED_PHYSBLEND_UPDATES_PHYSX
			bool bUpdatePhysics = false;
#endif
			FBodyInstance* PhysicsAssetBodyInstance = nullptr;

			// If so - get its world space matrix and its parents world space matrix and calc relative atom.
			if(BodyIndex != INDEX_NONE )
			{	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// tracking down TTP 280421. Remove this if this doesn't happen. 
				if ( !ensure(Bodies.IsValidIndex(BodyIndex)) )
				{
					UE_LOG(LogPhysics, Warning, TEXT("%s(Mesh %s, PhysicsAsset %s)"), 
						*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset));
					UE_LOG(LogPhysics, Warning, TEXT(" - # of BodySetup (%d), # of Bodies (%d), Invalid BodyIndex(%d)"), 
						PhysicsAsset->SkeletalBodySetups.Num(), Bodies.Num(), BodyIndex);
					continue;
				}
#endif
				PhysicsAssetBodyInstance = Bodies[BodyIndex];

				//if simulated body copy back and blend with animation
				if(PhysicsAssetBodyInstance->IsInstanceSimulatingPhysics())
				{
					FTransform PhysTM = PhysicsAssetBodyInstance->GetUnrealWorldTransform_AssumesLocked();

					// Store this world-space transform in cache.
					WorldBoneTMs[BoneIndex].TM = PhysTM;
					WorldBoneTMs[BoneIndex].bUpToDate = true;

					float UsePhysWeight = (bBlendPhysics)? 1.f : PhysicsAssetBodyInstance->PhysicsBlendWeight;

					// Find this bones parent matrix.
					FTransform ParentWorldTM;

					// if we wan't 'full weight' we just find 
					if(UsePhysWeight > 0.f)
					{
						if(BoneIndex == 0)
						{
							ParentWorldTM = LocalToWorldTM;
						}
						else
						{
							// If not root, get parent TM from cache (making sure its up-to-date).
							int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
							UpdateWorldBoneTM(WorldBoneTMs, InBoneSpaceTransforms, ParentIndex, this, LocalToWorldTM, TotalScale3D);
							ParentWorldTM = WorldBoneTMs[ParentIndex].TM;
						}


						// Then calc rel TM and convert to atom.
						FTransform RelTM = PhysTM.GetRelativeTransform(ParentWorldTM);
						RelTM.RemoveScaling();
						FQuat RelRot(RelTM.GetRotation());
						FVector RelPos =  RecipScale3D * RelTM.GetLocation();
						FTransform PhysAtom = FTransform(RelRot, RelPos, InBoneSpaceTransforms[BoneIndex].GetScale3D());

						// Now blend in this atom. See if we are forcing this bone to always be blended in
						InBoneSpaceTransforms[BoneIndex].Blend( InBoneSpaceTransforms[BoneIndex], PhysAtom, UsePhysWeight );

						if (!bSetParentScale)
						{
							//We must update RecipScale3D based on the atom scale of the root
							TotalScale3D *= InBoneSpaceTransforms[0].GetScale3D();
							RecipScale3D = TotalScale3D.Reciprocal();
							bSetParentScale = true;
						}

						#if DEPRECATED_PHYSBLEND_UPDATES_PHYSX
						if (UsePhysWeight < 1.f)
						{
							bUpdatePhysics = true;
						}
						#endif
					}
				}
			}

			// Update SpaceBases entry for this bone now
			if( BoneIndex == 0 )
			{
				EditableComponentSpaceTransforms[0] = InBoneSpaceTransforms[0];
			}
			else
			{
				if(bLocalSpaceKinematics || BodyIndex == INDEX_NONE || Bodies[BodyIndex]->IsInstanceSimulatingPhysics())
				{
					const int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
					EditableComponentSpaceTransforms[BoneIndex] = InBoneSpaceTransforms[BoneIndex] * EditableComponentSpaceTransforms[ParentIndex];

					/**
					* Normalize rotations.
					* We want to remove any loss of precision due to accumulation of error.
					* i.e. A componentSpace transform is the accumulation of all of its local space parents. The further down the chain, the greater the error.
					* SpaceBases are used by external systems, we feed this to PhysX, send this to gameplay through bone and socket queries, etc.
					* So this is a good place to make sure all transforms are normalized.
					*/
					EditableComponentSpaceTransforms[BoneIndex].NormalizeRotation();
				}
				else if(bSimulatedRootBody)
				{
					EditableComponentSpaceTransforms[BoneIndex] = Bodies[BodyIndex]->GetUnrealWorldTransform_AssumesLocked().GetRelativeTransform(NewComponentToWorld);
				}
			}
#if DEPRECATED_PHYSBLEND_UPDATES_PHYSX
			if (bUpdatePhysics && PhysicsAssetBodyInstance)
			{
				//This is extremely inefficient. We need to obtain a write lock which will block other threads from blending
				//For now I'm juts deferring it to the end of this loop, but in general we need to move it all out of here and do it when the blend task is done
				FBodyTMPair* BodyTMPair = new (PendingBodyTMs) FBodyTMPair;
				BodyTMPair->BI = PhysicsAssetBodyInstance;
				BodyTMPair->TM = EditableComponentSpaceTransforms[BoneIndex] * GetComponentTransform();
			}
#endif
		}

#if WITH_PHYSX
	}	//end scope for read lock
#if DEPRECATED_PHYSBLEND_UPDATES_PHYSX
	if(PendingBodyTMs.Num() && CVarPhysicsAnimBlendUpdatesPhysX.GetValueOnAnyThread())
	{
		//This is extremely inefficient. We need to obtain a write lock which will block other threads from blending
		//For now I'm juts deferring it to the end of this loop, but in general we need to move it all out of here and do it when the blend task is done
		SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPhysXScene(SceneType));

		for (const FBodyTMPair& BodyTMPair : PendingBodyTMs)
		{
			BodyTMPair.BI->SetBodyTransform(BodyTMPair.TM, ETeleportType::TeleportPhysics);
		}
    }
#endif
#endif
	
}



bool USkeletalMeshComponent::ShouldBlendPhysicsBones() const
{
	return Bodies.Num() > 0 && CollisionEnabledHasPhysics(GetCollisionEnabled()) && (DoAnyPhysicsBodiesHaveWeight() || bBlendPhysics);
}

bool USkeletalMeshComponent::DoAnyPhysicsBodiesHaveWeight() const
{
	for (const FBodyInstance* Body : Bodies)
	{
		if (Body->PhysicsBlendWeight > 0.f)
		{
			return true;
		}
	}

	return false;
}

TAutoConsoleVariable<int32> CVarUseParallelBlendPhysics(TEXT("a.ParallelBlendPhysics"), 1, TEXT("If 1, physics blending will be run across the task graph system. If 0, blending will run purely on the game thread"));

void USkeletalMeshComponent::BlendInPhysics(FTickFunction& ThisTickFunction)
{
	check(IsInGameThread());

	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	// We now have all the animations blended together and final relative transforms for each bone.
	// If we don't have or want any physics, we do nothing.
	if( Bodies.Num() > 0 && CollisionEnabledHasPhysics(GetCollisionEnabled()) )
	{
		HandleExistingParallelEvaluationTask(/*bBlockOnTask = */ true, /*bPerformPostAnimEvaluation =*/ true);
		// start parallel work
		check(!IsValidRef(ParallelAnimationEvaluationTask));

		const bool bParallelBlend = !!CVarUseParallelBlendPhysics.GetValueOnGameThread() && FApp::ShouldUseThreadingForPerformance();
		if(bParallelBlend)
		{
			if (SkeletalMesh->RefSkeleton.GetNum() != AnimEvaluationContext.BoneSpaceTransforms.Num())
			{
				// Initialize Parallel Task arrays
				AnimEvaluationContext.ComponentSpaceTransforms = GetComponentSpaceTransforms();
			}

			AnimEvaluationContext.BoneSpaceTransforms.Reset(BoneSpaceTransforms.Num());
			AnimEvaluationContext.BoneSpaceTransforms.Append(BoneSpaceTransforms);

			ParallelAnimationEvaluationTask = TGraphTask<FParallelBlendPhysicsTask>::CreateTask().ConstructAndDispatchWhenReady(this);

			// set up a task to run on the game thread to accept the results
			FGraphEventArray Prerequistes;
			Prerequistes.Add(ParallelAnimationEvaluationTask);

			check(!IsValidRef(ParallelBlendPhysicsCompletionTask));
			ParallelBlendPhysicsCompletionTask = TGraphTask<FParallelBlendPhysicsCompletionTask>::CreateTask(&Prerequistes).ConstructAndDispatchWhenReady(this);

			ThisTickFunction.GetCompletionHandle()->DontCompleteUntil(ParallelBlendPhysicsCompletionTask);
		}
		else
		{
			PerformBlendPhysicsBones(RequiredBones, BoneSpaceTransforms);
			PostBlendPhysics();
		}
	}
}

void USkeletalMeshComponent::PostBlendPhysics()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateLocalToWorldAndOverlaps);
	
	// Flip bone buffer and send 'post anim' notification
	FinalizeBoneTransform();

	// Update Child Transform - The above function changes bone transform, so will need to update child transform
	UpdateChildTransforms();

	// animation often change overlap. 
	UpdateOverlaps();

	// Cached local bounds are now out of date
	InvalidateCachedBounds();

	// update bounds
	UpdateBounds();

	// Need to send new bounds to 
	MarkRenderTransformDirty();

	// New bone positions need to be sent to render thread
	MarkRenderDynamicDataDirty();
}

void USkeletalMeshComponent::CompleteParallelBlendPhysics()
{
	Exchange(AnimEvaluationContext.BoneSpaceTransforms, AnimEvaluationContext.bDoInterpolation ? CachedBoneSpaceTransforms : BoneSpaceTransforms);
		
	PostBlendPhysics();

	ParallelAnimationEvaluationTask.SafeRelease();
	ParallelBlendPhysicsCompletionTask.SafeRelease();
}

void USkeletalMeshComponent::UpdateKinematicBonesToAnim(const TArray<FTransform>& InSpaceBases, ETeleportType Teleport, bool bNeedsSkinning, EAllowKinematicDeferral DeferralAllowed)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBBones);

	// Double check that the physics state has been created.
	// If there's no physics state, we can't do anything.
	if (!IsPhysicsStateCreated())
	{
		return;
	}

	// This below code produces some interesting result here
	// - below codes update physics data, so if you don't update pose, the physics won't have the right result
	// - but if we just update physics bone without update current pose, it will have stale data
	// If desired, pass the animation data to the physics joints so they can be used by motors.
	// See if we are going to need to update kinematics
	const bool bUpdateKinematics = (KinematicBonesUpdateType != EKinematicBonesUpdateToPhysics::SkipAllBones);
	const bool bTeleport = Teleport == ETeleportType::TeleportPhysics;
	// If desired, update physics bodies associated with skeletal mesh component to match.
	if(!bUpdateKinematics && !(bTeleport && IsAnySimulatingPhysics()))
	{
		// nothing to do 
		return;
	}

	// Get the scene, and do nothing if we can't get one.
	FPhysScene* PhysScene = nullptr;
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		PhysScene = GetWorld()->GetPhysicsScene();
	}

	if(PhysScene == nullptr)
	{
		return;
	}

	const FTransform& CurrentLocalToWorld = GetComponentTransform();

#if !(UE_BUILD_SHIPPING)
	// Gracefully handle NaN
	if(CurrentLocalToWorld.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("USkeletalMeshComponent::UpdateKinematicBonesToAnim: CurrentLocalToWorld contains NaN, aborting."));
		return;
	}
#endif

	// If we are only using bodies for physics, don't need to move them right away, can defer until simulation (unless told not to)
	if(DeferralAllowed == EAllowKinematicDeferral::AllowDeferral && (bDeferMovementFromSceneQueries || BodyInstance.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly))
	{
		PhysScene->MarkForPreSimKinematicUpdate(this, Teleport, bNeedsSkinning);
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// If desired, draw the skeleton at the point where we pass it to the physics.
	if (bShowPrePhysBones && SkeletalMesh && InSpaceBases.Num() == SkeletalMesh->RefSkeleton.GetNum())
	{
		for (int32 i = 1; i<InSpaceBases.Num(); i++)
		{
			FVector ThisPos = CurrentLocalToWorld.TransformPosition(InSpaceBases[i].GetLocation());

			int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(i);
			FVector ParentPos = CurrentLocalToWorld.TransformPosition(InSpaceBases[ParentIndex].GetLocation());

			World->LineBatcher->DrawLine(ThisPos, ParentPos, AnimSkelDrawColor, SDPG_Foreground);
		}
	}
#endif

	// warn if it has non-uniform scale
	const FVector& MeshScale3D = CurrentLocalToWorld.GetScale3D();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if( !MeshScale3D.IsUniform() )
	{
		UE_LOG(LogPhysics, Log, TEXT("USkeletalMeshComponent::UpdateKinematicBonesToAnim : Non-uniform scale factor (%s) can cause physics to mismatch for %s  SkelMesh: %s"), *MeshScale3D.ToString(), *GetFullName(), SkeletalMesh ? *SkeletalMesh->GetFullName() : TEXT("NULL"));
	}
#endif


	if (bEnablePerPolyCollision == false)
	{
		const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
		if (PhysicsAsset && SkeletalMesh && Bodies.Num() > 0)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!ensureMsgf(PhysicsAsset->SkeletalBodySetups.Num() == Bodies.Num(), TEXT("Mesh (%s) has PhysicsAsset(%s), and BodySetup(%d) and Bodies(%d) don't match"),
						*SkeletalMesh->GetName(), *PhysicsAsset->GetName(), PhysicsAsset->SkeletalBodySetups.Num(), Bodies.Num()))
			{
				return;
			}
#endif
			const int32 NumComponentSpaceTransforms = GetNumComponentSpaceTransforms();
			const int32 NumBodies = Bodies.Num();
#if WITH_PHYSX

			const uint32 SceneType = GetPhysicsSceneType(*PhysicsAsset, *PhysScene, UseAsyncScene);
			// Lock the scenes we need (flags set in InitArticulated)
			SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPhysXScene(SceneType));
#endif

			// Iterate over each body
			for (int32 i = 0; i < NumBodies; i++)
			{
				FBodyInstance* BodyInst = Bodies[i];
				PxRigidActor* RigidActor = BodyInst->GetPxRigidActor_AssumesLocked();

				if (RigidActor && (bTeleport || !BodyInst->IsInstanceSimulatingPhysics()))	//If we have a body and it's kinematic, or we are teleporting a simulated body
				{
					const int32 BoneIndex = BodyInst->InstanceBoneIndex;

					// If we could not find it - warn.
					if (BoneIndex == INDEX_NONE || BoneIndex >= NumComponentSpaceTransforms)
					{
						const FName BodyName = PhysicsAsset->SkeletalBodySetups[i]->BoneName;
						UE_LOG(LogPhysics, Log, TEXT("UpdateRBBones: WARNING: Failed to find bone '%s' need by PhysicsAsset '%s' in SkeletalMesh '%s'."), *BodyName.ToString(), *PhysicsAsset->GetName(), *SkeletalMesh->GetName());
					}
					else
					{
#if WITH_PHYSX
						// update bone transform to world
						const FTransform BoneTransform = InSpaceBases[BoneIndex] * CurrentLocalToWorld;
						if(!BoneTransform.IsValid())
						{
							const FName BodyName = PhysicsAsset->SkeletalBodySetups[i]->BoneName;
							UE_LOG(LogPhysics, Warning, TEXT("UpdateKinematicBonesToAnim: Trying to set transform with bad data %s on PhysicsAsset '%s' in SkeletalMesh '%s' for bone '%s'"), *BoneTransform.ToHumanReadableString(), *PhysicsAsset->GetName(), *SkeletalMesh->GetName(), *BodyName.ToString());
							BoneTransform.DiagnosticCheck_IsValid();	//In special nan mode we want to actually ensure

							continue;
						}

						// If not teleporting (must be kinematic) set kinematic target
						if (!bTeleport)
						{
							PhysScene->SetKinematicTarget_AssumesLocked(BodyInst, BoneTransform, true);
						}
						// Otherwise, set global pose
						else
						{
							const PxTransform PNewPose = U2PTransform(BoneTransform);
							ensure(PNewPose.isValid());
							RigidActor->setGlobalPose(PNewPose);
						}
#endif


						// now update scale
						// if uniform, we'll use BoneTranform
						if (MeshScale3D.IsUniform())
						{
							// @todo UE4 should we update scale when it's simulated?
							BodyInst->UpdateBodyScale(BoneTransform.GetScale3D());
						}
						else
						{
							// @note When you have non-uniform scale on mesh base,
							// hierarchical bone transform can update scale too often causing performance issue
							// So we just use mesh scale for all bodies when non-uniform
							// This means physics representation won't be accurate, but
							// it is performance friendly by preventing too frequent physics update
							BodyInst->UpdateBodyScale(MeshScale3D);
						}
					}
				}
				else
				{
					//make sure you have physics weight or blendphysics on, otherwise, you'll have inconsistent representation of bodies
					// @todo make this to be kismet log? But can be too intrusive
					if (!bBlendPhysics && BodyInst->PhysicsBlendWeight <= 0.f && BodyInst->BodySetup.IsValid())
					{
						//It's not clear whether this should be a warning. There are certainly cases where you interpolate the blend weight towards 0. The blend feature needs some work which will probably change this in the future.
						//Making it Verbose for now
						UE_LOG(LogPhysics, Verbose, TEXT("%s(Mesh %s, PhysicsAsset %s, Bone %s) is simulating, but no blending. "),
							*GetName(), *GetNameSafe(SkeletalMesh), *GetNameSafe(PhysicsAsset), *BodyInst->BodySetup.Get()->BoneName.ToString());
					}
				}
			}
		}
	}
	else
	{
		//per poly update requires us to update all vertex positions
		if (MeshObject)
		{
			if (bNeedsSkinning)
			{
				const FStaticLODModel& Model = MeshObject->GetSkeletalMeshResource().LODModels[0];
				TArray<FVector> NewPositions;
				if (true)
				{
					SCOPE_CYCLE_COUNTER(STAT_SkinPerPolyVertices);
					ComputeSkinnedPositions(NewPositions);
				}
				else	//keep old way around for now - useful for comparing performance
				{
					NewPositions.AddUninitialized(Model.NumVertices);
					{
						SCOPE_CYCLE_COUNTER(STAT_SkinPerPolyVertices);
						for (uint32 VertIndex = 0; VertIndex < Model.NumVertices; ++VertIndex)
						{
							NewPositions[VertIndex] = GetSkinnedVertexPosition(VertIndex);
						}
					}
				}
				BodyInstance.UpdateTriMeshVertices(NewPositions);
			}
			
			BodyInstance.SetBodyTransform(CurrentLocalToWorld, Teleport);
		}
	}


}



void USkeletalMeshComponent::UpdateRBJointMotors()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRBJoints);

	// moved this flag to here, so that
	// you can call it but still respect the flag
	if( !bUpdateJointsFromAnimation )
	{
		return;
	}

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && Constraints.Num() > 0 && SkeletalMesh)
	{
		check( PhysicsAsset->ConstraintSetup.Num() == Constraints.Num() );


		// Iterate over the constraints.
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			UPhysicsConstraintTemplate* CS = PhysicsAsset->ConstraintSetup[i];
			FConstraintInstance* CI = Constraints[i];

			FName JointName = CS->DefaultInstance.JointName;
			int32 BoneIndex = SkeletalMesh->RefSkeleton.FindBoneIndex(JointName);

			// If we found this bone, and a visible bone that is not the root, and its joint is motorised in some way..
			if( (BoneIndex != INDEX_NONE) && (BoneIndex != 0) &&
				(BoneVisibilityStates[BoneIndex] == BVS_Visible) &&
				(CI->IsAngularOrientationDriveEnabled()) )
			{
				check(BoneIndex < BoneSpaceTransforms.Num());

				// If we find the joint - get the local-space animation between this bone and its parent.
				FQuat LocalQuat = BoneSpaceTransforms[BoneIndex].GetRotation();
				FQuatRotationTranslationMatrix LocalRot(LocalQuat, FVector::ZeroVector);

				// We loop from the graphics parent bone up to the bone that has the body which the joint is attached to, to calculate the relative transform.
				// We need this to compensate for welding, where graphics and physics parents may not be the same.
				FMatrix ControlBodyToParentBoneTM = FMatrix::Identity;

				int32 TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex); // This give the 'graphics' parent of this bone
				bool bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2); // ConstraintBone2 is the 'physics' parent of this joint.

				while(!bFoundControlBody)
				{
					// Abort if we find a bone scaled to zero.
					const FVector Scale3D = BoneSpaceTransforms[TestBoneIndex].GetScale3D();
					const float ScaleSum = Scale3D.X + Scale3D.Y + Scale3D.Z;
					if(ScaleSum < KINDA_SMALL_NUMBER)
					{
						break;
					}

					// Add the current animated local transform into the overall controlling body->parent bone TM
					FMatrix RelTM = BoneSpaceTransforms[TestBoneIndex].ToMatrixNoScale();
					RelTM.SetOrigin(FVector::ZeroVector);
					ControlBodyToParentBoneTM = ControlBodyToParentBoneTM * RelTM;

					// Move on to parent
					TestBoneIndex = SkeletalMesh->RefSkeleton.GetParentIndex(TestBoneIndex);

					// If we are at the root - bail out.
					if(TestBoneIndex == 0)
					{
						break;
					}

					// See if this is the controlling body
					bFoundControlBody = (SkeletalMesh->RefSkeleton.GetBoneName(TestBoneIndex) == CS->DefaultInstance.ConstraintBone2);
				}

				// If after that we didn't find a parent body, we can' do this, so skip.
				if(bFoundControlBody)
				{
					// The animation rotation is between the two bodies. We need to supply the joint with the relative orientation between
					// the constraint ref frames. So we work out each body->joint transform

					FMatrix Body1TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame1).ToMatrixNoScale();
					Body1TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body1TMInv = Body1TM.InverseFast();

					FMatrix Body2TM = CS->DefaultInstance.GetRefFrame(EConstraintFrame::Frame2).ToMatrixNoScale();
					Body2TM.SetOrigin(FVector::ZeroVector);
					FMatrix Body2TMInv = Body2TM.InverseFast();

					FMatrix JointRot = Body1TM * (LocalRot * ControlBodyToParentBoneTM) * Body2TMInv;
					FQuat JointQuat(JointRot);

					// Then pass new quaternion to the joint!
					CI->SetAngularOrientationTarget(JointQuat);
				}
			}
		}
	}
}

