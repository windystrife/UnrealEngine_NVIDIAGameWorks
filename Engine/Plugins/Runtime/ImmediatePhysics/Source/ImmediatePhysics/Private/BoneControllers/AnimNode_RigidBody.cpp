// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_RigidBody.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#include "ImmediatePhysicsSimulation.h"
#include "ImmediatePhysicsActorHandle.h"

#include "Logging/MessageLog.h"

using namespace ImmediatePhysics;

//#pragma optimize("", off)

/////////////////////////////////////////////////////
// FAnimNode_Ragdoll

#define LOCTEXT_NAMESPACE "ImmediatePhysics"

FAnimNode_RigidBody::FAnimNode_RigidBody():
	QueryParams(NAME_None, FCollisionQueryParams::GetUnknownStatId())
{
	bResetSimulated = false;
	PhysicsSimulation = nullptr;
	OverridePhysicsAsset = nullptr;
	bOverrideWorldGravity = false;
	CachedBoundsScale = 1.2f;
	SimulationSpace = ESimulationSpace::ComponentSpace;
	ExternalForce = FVector::ZeroVector;
	bComponentSpaceSimulation_DEPRECATED = true;
	OverrideWorldGravity = FVector::ZeroVector;
	TotalMass = 0.f;
	CachedBounds.W = 0;
	UnsafeWorld = nullptr;
}

FAnimNode_RigidBody::~FAnimNode_RigidBody()
{
	delete PhysicsSimulation;
}

void FAnimNode_RigidBody::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += ")";

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

FVector WorldVectorToSpaceNoScale(ESimulationSpace Space, const FVector& WorldDir, const FTransform& ComponentToWorld, const FTransform& RootBoneTM)
{
	switch(Space)
	{
		case ESimulationSpace::ComponentSpace: return ComponentToWorld.InverseTransformVectorNoScale(WorldDir);
		case ESimulationSpace::WorldSpace: return WorldDir;
		case ESimulationSpace::RootBoneSpace: return RootBoneTM.InverseTransformVectorNoScale(ComponentToWorld.InverseTransformVectorNoScale(WorldDir));
		default: return FVector::ZeroVector;
	}
}

FVector WorldPositionToSpace(ESimulationSpace Space, const FVector& WorldPoint, const FTransform& ComponentToWorld, const FTransform& RootBoneTM)
{
	switch (Space)
	{
		case ESimulationSpace::ComponentSpace: return ComponentToWorld.InverseTransformPosition(WorldPoint);
		case ESimulationSpace::WorldSpace: return WorldPoint;
		case ESimulationSpace::RootBoneSpace: return RootBoneTM.InverseTransformPosition(ComponentToWorld.InverseTransformPosition(WorldPoint));
		default: return FVector::ZeroVector;
	}
}


DECLARE_CYCLE_STAT(TEXT("FAnimNode_Ragdoll::EvaluateSkeletalControl_AnyThread"), STAT_ImmediateEvaluateSkeletalControl, STATGROUP_ImmediatePhysics);

void FAnimNode_RigidBody::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_ImmediateEvaluateSkeletalControl);
	//FPlatformMisc::BeginNamedEvent(FColor::Magenta, "FAnimNode_Ragdoll::EvaluateSkeletalControl_AnyThread");

	if(PhysicsSimulation && DeltaSeconds > 0)
	{
		const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

		const FTransform CompWorldSpaceTM = Output.AnimInstanceProxy->GetComponentTransform();
		const FTransform RootBoneTM = Output.Pose.GetComponentSpaceTransform(RootBoneRef.GetCompactPoseIndex(BoneContainer));
		//Update physics with transforms
		
		for (const FOutputBoneData& OutputData : OutputBoneData)
		{
			int32 BodyIndex = OutputData.BodyIndex;
			if(BodyIndex != INDEX_NONE && (bResetSimulated || !IsSimulated[BodyIndex]))
			{
				FCompactPoseBoneIndex SimBoneIndex = OutputData.BoneReference.GetCompactPoseIndex(BoneContainer);
				const FTransform& ComponentSpaceTM = Output.Pose.GetComponentSpaceTransform(SimBoneIndex);
				
				FTransform BodyTM;
				switch(SimulationSpace)
				{
					case ESimulationSpace::ComponentSpace : BodyTM = ComponentSpaceTM; break;
					case ESimulationSpace::WorldSpace : BodyTM = ComponentSpaceTM * CompWorldSpaceTM; break;
					case ESimulationSpace::RootBoneSpace: BodyTM = ComponentSpaceTM.GetRelativeTransform(RootBoneTM); break;
					default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); BodyTM = ComponentSpaceTM;
				}

				if(bResetSimulated)
				{
					Bodies[BodyIndex]->SetWorldTransform(BodyTM);
				}
				else
				{
					Bodies[BodyIndex]->SetKinematicTarget(BodyTM);
				}
				
			}
		}

		bResetSimulated = false;

		UpdateWorldForces(CompWorldSpaceTM, RootBoneTM);

		//simulate
		const FVector SimSpaceGravity = WorldVectorToSpaceNoScale(SimulationSpace, WorldSpaceGravity, CompWorldSpaceTM, RootBoneTM);
		PhysicsSimulation->Simulate(DeltaSeconds, SimSpaceGravity);

		
		//write back to animation system
		for (const FOutputBoneData& OutputData : OutputBoneData)
		{
			int32 BodyIndex = OutputData.BodyIndex;
			if(BodyIndex != INDEX_NONE)
			{
				/*if(true || IsSimulated[BodyIndex])*/	//todo: only output kinematic bones that have simulated ancestors
				{
					FCompactPoseBoneIndex SimBoneIndex = OutputData.BoneReference.GetCompactPoseIndex(BoneContainer);
					const FTransform BodyTM = Bodies[BodyIndex]->GetWorldTransform();
					FTransform ComponentSpaceTM;

					switch(SimulationSpace)
					{
						case ESimulationSpace::ComponentSpace: ComponentSpaceTM = BodyTM; break;
						case ESimulationSpace::WorldSpace: ComponentSpaceTM = BodyTM.GetRelativeTransform(CompWorldSpaceTM); break;
						case ESimulationSpace::RootBoneSpace: ComponentSpaceTM = BodyTM * RootBoneTM; break;
						default: ensureMsgf(false, TEXT("Unsupported Simulation Space")); ComponentSpaceTM = BodyTM;
					}
					
					OutBoneTransforms.Add(FBoneTransform(SimBoneIndex, ComponentSpaceTM));
				}
			}
			else
			{
				//we have no body, but our ancestors are simulated so update our component space transform
			}
		}
	}
	//FPlatformMisc::EndNamedEvent();
}

void ComputeBodyInsertionOrder(TArray<FBoneIndexType>& InsertionOrder, const USkeletalMeshComponent& SKC)
{
	//We want to ensure simulated bodies are sorted by LOD so that the first simulated bodies are at the highest LOD.
	//Since LOD2 is a subset of LOD1 which is a subset of LOD0 we can change the number of simulated bodies without any reordering
	//For this to work we must first insert all simulated bodies in the right order. We then insert all the kinematic bodies in the right order

	InsertionOrder.Reset();
	if(FSkeletalMeshResource* SkelMeshResource = SKC.GetSkeletalMeshResource())
	{
		const int32 NumLODs = SkelMeshResource->LODModels.Num();

		TArray<bool> InSortedOrder;

		// total number of mesh bones
		const int32 TotalNumBones = SKC.SkeletalMesh->RefSkeleton.GetNum();
		InSortedOrder.AddZeroed(TotalNumBones);

		auto MergeIndices = [&InsertionOrder, &InSortedOrder](const TArray<FBoneIndexType>& RequiredBones) -> void
		{
			for (FBoneIndexType BoneIdx : RequiredBones)
			{
				if (!InSortedOrder[BoneIdx])
				{
					InsertionOrder.Add(BoneIdx);
				}

				InSortedOrder[BoneIdx] = true;
			}
		};


		for(int32 LodIdx = NumLODs - 1; LodIdx >= 0; --LodIdx)
		{
			TArray<FBoneIndexType> RequiredBones;
			TArray<FBoneIndexType> ComponentSpaceTMs;
			SKC.ComputeRequiredBones(RequiredBones, ComponentSpaceTMs, LodIdx, /*bIgnorePhysicsAsset=*/ true);
			MergeIndices(RequiredBones);
		}
	}
}

void FAnimNode_RigidBody::InitPhysics(const UAnimInstance* InAnimInstance)
{
	const USkeletalMeshComponent* SkeletalMeshComp = InAnimInstance->GetSkelMeshComponent();
	const FReferenceSkeleton& RefSkel = SkeletalMeshComp->SkeletalMesh->RefSkeleton;
	UPhysicsAsset* UsePhysicsAsset = OverridePhysicsAsset ? OverridePhysicsAsset : InAnimInstance->GetSkelMeshComponent()->GetPhysicsAsset();
	if(UsePhysicsAsset)
	{
		delete PhysicsSimulation;
		PhysicsSimulation = new FSimulation();
		const int32 NumBodies = UsePhysicsAsset->SkeletalBodySetups.Num();
		Bodies.Empty(NumBodies);
		BodyBoneIndices.Empty(NumBodies);
		ComponentsInSim.Reset();
		IsSimulated.Reset();
		TotalMass = 0.f;
		

		TArray<FBodyInstance*> HighLevelBodyInstances;
		TArray<FConstraintInstance*> HighLevelConstraintInstances;
		SkeletalMeshComp->InstantiatePhysicsAsset(*UsePhysicsAsset, SimulationSpace == ESimulationSpace::WorldSpace ? SkeletalMeshComp->GetComponentToWorld().GetScale3D() : FVector(1.f), HighLevelBodyInstances, HighLevelConstraintInstances);

		TMap<FName, FActorHandle*> NamesToHandles;
		TArray<FActorHandle*> IgnoreCollisionActors;

		TArray<FBoneIndexType> InsertionOrder;
		ComputeBodyInsertionOrder(InsertionOrder, *SkeletalMeshComp);
		
		const int32 NumMeshBones = RefSkel.GetNum();
		// body index is based on mesh bones, 
		// so we use mesh bones as buffer size when bone index is referenced
		TArray<FActorHandle*> BodyIndexToActorHandle;
		BodyIndexToActorHandle.AddZeroed(NumMeshBones);

		TArray<FBodyInstance*> BodiesSorted;
		BodiesSorted.AddZeroed(NumMeshBones);

		TArray<FBoneIndexType> RequiredBones0;
		TArray<FBoneIndexType> ComponentSpaceTMs0;
		SkeletalMeshComp->ComputeRequiredBones(RequiredBones0, ComponentSpaceTMs0, 0, /*bIgnorePhysicsAsset=*/ true);

		for (FBodyInstance* BI : HighLevelBodyInstances)
		{
			if(BI->IsValidBodyInstance())
			{
				// verify if the LOD 0 required bones contains the index
				if (RequiredBones0.Contains(BI->InstanceBoneIndex))
				{
					BodiesSorted[BI->InstanceBoneIndex] = BI;
				}
				else
				{
					UE_LOG(LogAnimation, Warning, TEXT("AnimNode_RigidBody : missing body to simulate for bone %s"), *BI->BodySetup->BoneName.ToString());
				}
			}
		}

		auto InsertBodiesHelper = [&](bool bSimulatedBodies)
		{
			for (FBoneIndexType InsertBone : InsertionOrder)
			{
				if (FBodyInstance* BodyInstance = BodiesSorted[InsertBone])
				{
					UBodySetup* BodySetup = UsePhysicsAsset->SkeletalBodySetups[BodyInstance->InstanceBodyIndex];
					const bool bKinematic = BodySetup->PhysicsType != EPhysicsType::PhysType_Simulated;
					const FTransform& LastTransform = SkeletalMeshComp->GetComponentSpaceTransforms()[InsertBone];	//This is out of date, but will still give our bodies an initial setup that matches the constraints (TODO: use refpose)

					FActorHandle* NewBodyHandle = nullptr;
					if (bSimulatedBodies && !bKinematic)
					{
#if WITH_PHYSX
						NewBodyHandle = PhysicsSimulation->CreateDynamicActor(BodyInstance->GetPxRigidDynamic_AssumesLocked(), LastTransform);
						checkSlow(NewBodyHandle);
						const float InvMass = NewBodyHandle->GetInverseMass();
						TotalMass += InvMass > 0.f ? 1.f / InvMass : 0.f;
#endif
					}
					else if (bSimulatedBodies && bKinematic)
					{
#if WITH_PHYSX
						NewBodyHandle = PhysicsSimulation->CreateKinematicActor(BodyInstance->GetPxRigidBody_AssumesLocked(), LastTransform);
#endif
					}

					if (NewBodyHandle)
					{
						Bodies.Add(NewBodyHandle);
						BodyBoneIndices.Add(InsertBone);
						NamesToHandles.Add(BodySetup->BoneName, NewBodyHandle);
						IsSimulated.Add(!bKinematic);
						BodyIndexToActorHandle[BodyInstance->InstanceBodyIndex] = NewBodyHandle;

						if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
						{
							IgnoreCollisionActors.Add(NewBodyHandle);
						}

						BodyInstance->TermBody();
						delete BodyInstance;
						BodiesSorted[InsertBone] = nullptr;
					}
				}
			}
		};

		//Insert simulated bodies first to avoid any re-ordering
		InsertBodiesHelper(/*bSimulated=*/true);
		InsertBodiesHelper(/*bSimulated=*/false);
		HighLevelBodyInstances.Empty();
		BodiesSorted.Empty();

		//Insert joints so that they coincide body order. That is, if we stop simulating all bodies past some index, we can simply ignore joints past a corresponding index without any re-order
		//For this to work we consider the most last inserted bone in each joint
		TArray<int32> InsertionOrderPerBone;
		InsertionOrderPerBone.AddUninitialized(NumMeshBones);

		for(int32 Position = 0; Position < InsertionOrder.Num(); ++Position)
		{
			InsertionOrderPerBone[InsertionOrder[Position]] = Position;
		}

		HighLevelConstraintInstances.Sort([&InsertionOrderPerBone, &RefSkel](const FConstraintInstance& LHS, const FConstraintInstance& RHS)
		{
			if(LHS.IsValidConstraintInstance() && RHS.IsValidConstraintInstance())
			{
				const int32 BoneIdxLHS1 = RefSkel.FindBoneIndex(LHS.ConstraintBone1);
				const int32 BoneIdxLHS2 = RefSkel.FindBoneIndex(LHS.ConstraintBone2);

				const int32 BoneIdxRHS1 = RefSkel.FindBoneIndex(RHS.ConstraintBone1);
				const int32 BoneIdxRHS2 = RefSkel.FindBoneIndex(RHS.ConstraintBone2);

				const int32 MaxPositionLHS = FMath::Max(InsertionOrderPerBone[BoneIdxLHS1], InsertionOrderPerBone[BoneIdxLHS2]);
				const int32 MaxPositionRHS = FMath::Max(InsertionOrderPerBone[BoneIdxRHS1], InsertionOrderPerBone[BoneIdxRHS2]);

				return MaxPositionLHS < MaxPositionRHS;
			}
			
			return false;
		});

#if WITH_PHYSX
		if(NamesToHandles.Num() > 0)
		{
			//constraints
			for(int32 ConstraintIdx = 0; ConstraintIdx < HighLevelConstraintInstances.Num(); ++ConstraintIdx)
			{
				FConstraintInstance* CI = HighLevelConstraintInstances[ConstraintIdx];
				FActorHandle* Body1Handle = NamesToHandles.FindRef(CI->ConstraintBone1);
				FActorHandle* Body2Handle = NamesToHandles.FindRef(CI->ConstraintBone2);

				if(Body1Handle && Body2Handle)
				{
					if (Body1Handle->IsSimulated() || Body2Handle->IsSimulated())
					{
						PhysicsSimulation->CreateJoint(CI->ConstraintData, Body1Handle, Body2Handle);
					}
				}

				CI->TermConstraint();
				delete CI;
			}

			bResetSimulated = true;
		}

		TArray<FSimulation::FIgnorePair> IgnorePairs;
		const TMap<FRigidBodyIndexPair, bool>& DisableTable = UsePhysicsAsset->CollisionDisableTable;
		for(auto ConstItr = DisableTable.CreateConstIterator(); ConstItr; ++ConstItr)
		{
			FSimulation::FIgnorePair Pair;
			Pair.A = BodyIndexToActorHandle[ConstItr.Key().Indices[0]];
			Pair.B = BodyIndexToActorHandle[ConstItr.Key().Indices[1]];
			IgnorePairs.Add(Pair);
		}

		PhysicsSimulation->SetIgnoreCollisionPairTable(IgnorePairs);
		PhysicsSimulation->SetIgnoreCollisionActors(IgnoreCollisionActors);
	}
#endif
}

DECLARE_CYCLE_STAT(TEXT("FAnimNode_Ragdoll::UpdateWorldGeometry"), STAT_ImmediateUpdateWorldGeometry, STATGROUP_ImmediatePhysics);

void FAnimNode_RigidBody::UpdateWorldGeometry(const UWorld& World, const USkeletalMeshComponent& SKC)
{
	SCOPE_CYCLE_COUNTER(STAT_ImmediateUpdateWorldGeometry);
	QueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(RagdollNodeFindGeometry), /*bTraceComplex=*/false);
#if WITH_EDITOR
	if(!World.IsGameWorld())
	{
		QueryParams.MobilityType = EQueryMobilityType::Any;	//If we're in some preview world trace against everything because things like the preview floor are not static
		QueryParams.AddIgnoredComponent(&SKC);
	}
	else
#endif
	{
		QueryParams.MobilityType = EQueryMobilityType::Static;	//We only want static actors
	}

	Bounds = SKC.CalcBounds(SKC.GetComponentToWorld()).GetSphere();

	if (!Bounds.IsInside(CachedBounds))
	{
		// Since the cached bounds are no longer valid, update them.
		
		CachedBounds = Bounds;
		CachedBounds.W *= CachedBoundsScale;

		// Cache the PhysScene and World for use in UpdateWorldForces.
		PhysScene = World.GetPhysicsScene();
		UnsafeWorld = &World;
	}
}

DECLARE_CYCLE_STAT(TEXT("FAnimNode_Ragdoll::UpdateWorldForces"), STAT_ImmediateUpdateWorldForces, STATGROUP_ImmediatePhysics);

void FAnimNode_RigidBody::UpdateWorldForces(const FTransform& ComponentToWorld, const FTransform& RootBoneTM)
{
	SCOPE_CYCLE_COUNTER(STAT_ImmediateUpdateWorldForces);

	if(TotalMass > 0.f)
	{
		for (const USkeletalMeshComponent::FPendingRadialForces& PendingRadialForce : PendingRadialForces)
		{
			const FVector RadialForceOrigin = WorldPositionToSpace(SimulationSpace, PendingRadialForce.Origin, ComponentToWorld, RootBoneTM);
			for(FActorHandle* Body : Bodies)
			{
				const float InvMass = Body->GetInverseMass();
				if(InvMass > 0.f)
				{
					const float StrengthPerBody = PendingRadialForce.bIgnoreMass ? PendingRadialForce.Strength : PendingRadialForce.Strength / (TotalMass * InvMass);
					FSimulation::EForceType ForceType;
					if (PendingRadialForce.Type == USkeletalMeshComponent::FPendingRadialForces::AddImpulse)
					{
						ForceType = PendingRadialForce.bIgnoreMass ? FSimulation::EForceType::AddVelocity : FSimulation::EForceType::AddImpulse;
					}
					else
					{
						ForceType = PendingRadialForce.bIgnoreMass ? FSimulation::EForceType::AddAcceleration : FSimulation::EForceType::AddForce;
					}
					
					Body->AddRadialForce(RadialForceOrigin, StrengthPerBody, PendingRadialForce.Radius, PendingRadialForce.Falloff, ForceType);
				}
			}
		}

		if(!ExternalForce.IsNearlyZero())
		{
			const FVector ExternalForceInSimSpace = WorldVectorToSpaceNoScale(SimulationSpace, ExternalForce, ComponentToWorld, RootBoneTM);
			for (FActorHandle* Body : Bodies)
			{
				const float InvMass = Body->GetInverseMass();
				if (InvMass > 0.f)
				{
					Body->AddForce(ExternalForceInSimSpace);
				}
			}
		}
	}
}

void FAnimNode_RigidBody::PreUpdate(const UAnimInstance* InAnimInstance)
{
	UWorld* World = InAnimInstance->GetWorld();
	USkeletalMeshComponent* SKC = InAnimInstance->GetSkelMeshComponent();

#if WITH_EDITOR
	if (bEnableWorldGeometry && SimulationSpace != ESimulationSpace::WorldSpace)
	{
		FMessageLog("PIE").Warning(FText::Format(LOCTEXT("WorldCollisionComponentSpace", "Trying to use world collision without world space simulation for ''{0}''. This is not supported, please change SimulationSpace to WorldSpace"),
			FText::FromString(GetPathNameSafe(SKC))));
	}
#endif

	DeltaSeconds = World->GetDeltaSeconds();
	WorldSpaceGravity = bOverrideWorldGravity ? OverrideWorldGravity : FVector(0.f, 0.f, World->GetGravityZ());

	if(SKC)
	{
		if (PhysicsSimulation && bEnableWorldGeometry && SimulationSpace == ESimulationSpace::WorldSpace)
		{
			UpdateWorldGeometry(*World, *SKC);
		}

		PendingRadialForces = SKC->GetPendingRadialForces();
	}
	
}

void FAnimNode_RigidBody::UpdateInternal(const FAnimationUpdateContext& Context)
{
	if (UnsafeWorld != nullptr)
	{

#if WITH_PHYSX

		TArray<FOverlapResult> Overlaps;
		UnsafeWorld->OverlapMultiByChannel(Overlaps, Bounds.Center, FQuat::Identity, OverlapChannel, FCollisionShape::MakeSphere(Bounds.W), QueryParams, FCollisionResponseParams(ECR_Overlap));
		SCOPED_SCENE_READ_LOCK(PhysScene ? PhysScene->GetPhysXScene(PST_Sync) : nullptr); //TODO: expose this part to the anim node
	
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (UPrimitiveComponent* OverlapComp = Overlap.GetComponent())
			{
				if (ComponentsInSim.Contains(OverlapComp) == false)
				{
					ComponentsInSim.Add(OverlapComp);
					if (PxRigidActor* RigidActor = OverlapComp->BodyInstance.GetPxRigidActor_AssumesLocked())
					{
						PhysicsSimulation->CreateStaticActor(RigidActor, P2UTransform(RigidActor->getGlobalPose()));
					}
				}
			}
		}
#endif

		UnsafeWorld = nullptr;
		PhysScene = nullptr;
	}
}

void FAnimNode_RigidBody::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	/** We only need to update simulated bones and children of simulated bones*/
	const int32 NumBodies = Bodies.Num();
	const TArray<FBoneIndexType>& RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	const int32 NumRequiredBoneIndices = RequiredBoneIndices.Num();
	const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();
	
	OutputBoneData.Empty(NumRequiredBoneIndices);

	TArray<bool> OutputBonesCache;	//easy lookup into whether parent is going to be evaluated
	OutputBonesCache.AddZeroed(RefSkeleton.GetNum());

	int32 NumSimulatedBodies = 0;

	//we always cache root bone
	RootBoneRef.BoneName = RefSkeleton.GetBoneName(0);
	RootBoneRef.Initialize(RequiredBones);

	for(int32 RequiredBoneIndexIndex = 0; RequiredBoneIndexIndex < NumRequiredBoneIndices; ++RequiredBoneIndexIndex)
	{
		const FBoneIndexType RequiredBoneIndex = RequiredBoneIndices[RequiredBoneIndexIndex];
		int32 FoundBodyIndex = INDEX_NONE;

		for(int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)	//TODO: we could sort this and avoid n^2
		{
			if(BodyBoneIndices[BodyIndex] == RequiredBoneIndex)	//If we have a body we need to save it for later
			{
				FOutputBoneData* OutputData = new (OutputBoneData) FOutputBoneData();
				OutputData->BodyIndex = BodyIndex;
				OutputData->BoneReference.BoneName = RefSkeleton.GetBoneName(RequiredBoneIndex);
				OutputData->BoneReference.Initialize(RequiredBones);

				if(IsSimulated[BodyIndex])
				{
					OutputBonesCache[RequiredBoneIndex] = true;	//children of simulated bodies need to update the component space transform
					++NumSimulatedBodies;
				}
		
				//++BodyIndex;	//Move on to next body
				FoundBodyIndex = BodyIndex;
				break;
			}
		}

		if(RequiredBoneIndex > 0 && FoundBodyIndex == INDEX_NONE)
		{
			//If we don't have a body, but our ancestors are simulated we will need to update the component space transform
			FBoneIndexType ParentBoneIndex = RequiredBones.GetParentBoneIndex(RequiredBoneIndex);
			if (OutputBonesCache[ParentBoneIndex])
			{
				OutputBonesCache[RequiredBoneIndex] = true;

				FOutputBoneData* OutputData = new (OutputBoneData) FOutputBoneData();
				OutputData->BodyIndex = INDEX_NONE;
				OutputData->BoneReference.BoneName = RefSkeleton.GetBoneName(RequiredBoneIndex);
				OutputData->BoneReference.Initialize(RequiredBones);
			}
		}
	}

	if(PhysicsSimulation)
	{
		PhysicsSimulation->SetNumActiveBodies(NumSimulatedBodies);
	}
}

void FAnimNode_RigidBody::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	InitPhysics(InAnimInstance);
}

void FAnimNode_RigidBody::PostSerialize(const FArchive& Ar)
{
	if(bComponentSpaceSimulation_DEPRECATED == false)
	{
		//If this is not the default value it means we have old content where we were simulating in world space
		SimulationSpace = ESimulationSpace::WorldSpace;
		bComponentSpaceSimulation_DEPRECATED = true;
	}
}

#undef LOCTEXT_NAMESPACE