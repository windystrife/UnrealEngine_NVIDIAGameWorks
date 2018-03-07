// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImmediatePhysicsSimulation.h"
#include "ImmediatePhysicsActorHandle.h"
#include "ImmediatePhysicsJointHandle.h"
#include "ImmediatePhysicsContactPointRecorder.h"

namespace ImmediatePhysics
{

FSimulation::FSimulation()
{
	NumActiveSimulatedBodies = 0;
	NumSimulatedBodies = 0;
	NumKinematicBodies = 0;
	NumSimulatedShapesWithCollision = 0;
	NumContactHeaders = 0;
	NumJointHeaders = 0;
	NumActiveJoints = 0;
	NumPositionIterations = 1;
	NumVelocityIterations = 1;
	bDirtyJointData = false;
	bRecreateIterationCache = false;
	SimCount = 0;
}

FSimulation::~FSimulation()
{
	for(FActorHandle* ActorHandle : ActorHandles)
	{
		delete ActorHandle;
	}

	ActorHandles.Empty();
}

void FSimulation::SetNumActiveBodies(uint32 InNumActiveBodies)
{
	NumActiveSimulatedBodies = InNumActiveBodies;
	bDirtyJointData = true;
}

#if WITH_PHYSX
FJointHandle* FSimulation::CreateJoint(PxD6Joint* Joint, FActorHandle* Actor1, FActorHandle* Actor2)
{
	//TODO: make this robust
	if (Actor1 == nullptr) return nullptr;

	check(&Actor1->OwningSimulation == this);
	check(!Actor2 || &Actor2->OwningSimulation == this);	//Actor2 can be null

	FJointHandle* NewHandle = nullptr;

	int32 JointDataIdx = JointData.Emplace(Joint);
	PrepareJointData(JointData[JointDataIdx]);

	if(!JointData[JointDataIdx].HasConstraints())
	{
		JointData.RemoveAt(JointDataIdx, /*Count=*/1, /*bAllowShrinking=*/false);
	}
	else
	{
		Joints.Emplace();
		FJoint& NewJoint = Joints[JointDataIdx];
		//TODO: make dynamic vs static API more robust
		NewJoint.DynamicActor = Actor2;
		NewJoint.OtherActor = Actor1;

		NewHandle = new FJointHandle(*this, Joints.Num() - 1);
		JointHandles.Add(NewHandle);
		bDirtyJointData = true;
	}

	ValidateArrays();

	return NewHandle;
}

void FSimulation::SetIgnoreCollisionPairTable(const TArray<FIgnorePair>& InIgnoreTable)
{
	IgnoreCollisionPairTable.Reset();
	for(const FIgnorePair& IgnorePair : InIgnoreTable)
	{
		TSet<FActorHandle*>& IgnoreActorsA = IgnoreCollisionPairTable.FindOrAdd(IgnorePair.A);
		IgnoreActorsA.Add(IgnorePair.B);

		TSet<FActorHandle*>& IgnoreActorsB = IgnoreCollisionPairTable.FindOrAdd(IgnorePair.B);
		IgnoreActorsB.Add(IgnorePair.A);
	}

	bRecreateIterationCache = true;
}

void FSimulation::SetIgnoreCollisionActors(const TArray<FActorHandle*>& InIgnoreCollisionActors)
{
	IgnoreCollisionActors.Reset();
	IgnoreCollisionActors.Append(InIgnoreCollisionActors);
	bRecreateIterationCache = true;
}

template <FSimulation::ECreateActorType ActorType>
uint32 FSimulation::CreateActor(PxRigidActor* RigidActor, const FTransform& WorldTM)
{
	bDirtyJointData = true;	//new entity potentially re-orders bodies so joint data becomes stale. TODO: can this be optimized for adding static cases that don't re-order?
	bRecreateIterationCache = true;	//new entity potentially re-orders bodies so our iteration cache becomes stale. TODO: can this be optimized for adding static cases that don't re-order?

	FActor* NewEntity = new (Actors) FActor();
	const uint32 ActorDataIndex = Actors.Num() - 1;

	FActorHandle* NewActorHandle = new FActorHandle(*this, ActorDataIndex);
	ActorHandles.Add(NewActorHandle);

	PxSolverBodyData* NewSolverBodyData = new (SolverBodiesData) PxSolverBodyData();
	immediate::PxRigidBodyData* NewRigidBodyData = new (RigidBodiesData) immediate::PxRigidBodyData();

	//Fill rigid body data based on the actor we've been given
	PxRigidBody* const RigidBody = ActorType == ECreateActorType::StaticActor ? nullptr : static_cast<PxRigidBody*>(RigidActor);	//Note the const pointer - this is for templated code
	PxRigidDynamic* const RigidDynamic = ActorType == ECreateActorType::DynamicActor ? static_cast<PxRigidDynamic*>(RigidActor) : nullptr;	//Note the const pointer - this is for templated code

	NewRigidBodyData->invMass = RigidDynamic ? RigidDynamic->getInvMass() : 0.f;
	NewRigidBodyData->angularVelocity = PxVec3(PxZero);
	NewRigidBodyData->maxDepenetrationVelocity = RigidDynamic ? RigidDynamic->getMaxDepenetrationVelocity() : PX_MAX_F32;
	NewRigidBodyData->invInertia = RigidDynamic ? RigidDynamic->getMassSpaceInvInertiaTensor() : PxVec3(PxZero);
	NewRigidBodyData->maxContactImpulse = RigidBody ? RigidBody->getMaxContactImpulse() : PX_MAX_F32;
	
	NewActorHandle->ActorToBody = RigidBody ? P2UTransform(RigidBody->getCMassLocalPose()) : FTransform::Identity;	//TODO: Why is this an unreal transform?
	NewRigidBodyData->body2World = U2PTransform(NewActorHandle->ActorToBody * WorldTM);

	NewRigidBodyData->linearDamping = RigidDynamic ? RigidDynamic->getLinearDamping() : 0.f;
	NewRigidBodyData->angularDamping = RigidDynamic ? RigidDynamic->getAngularDamping() : 0.f;
	NewRigidBodyData->maxLinearVelocitySq = PX_MAX_F32;	//NOTE: rigid dynamics don't seem to have this max property.

	PendingAcceleration.AddZeroed();
	KinematicTargets.AddZeroed();

	if(RigidDynamic)
	{
		NewRigidBodyData->maxAngularVelocitySq = RigidDynamic->getMaxAngularVelocity();
		NewRigidBodyData->maxAngularVelocitySq *= NewRigidBodyData->maxAngularVelocitySq;

		uint32 PositionIterations, VelocityIterations;
		RigidDynamic->getSolverIterationCounts(PositionIterations, VelocityIterations);

		NumPositionIterations = FMath::Max(PositionIterations, NumPositionIterations);
		NumVelocityIterations = FMath::Max(VelocityIterations, NumVelocityIterations);

	}else
	{
		NewRigidBodyData->maxAngularVelocitySq = PX_MAX_F32;
	}

	if (ActorType == ECreateActorType::StaticActor)
	{
		//If static we fill this struct here, for dynamic we fill during every call to simulate
		immediate::PxConstructStaticSolverBody(NewRigidBodyData->body2World, *NewSolverBodyData);
	}

	NewEntity->CreateGeometry(RigidActor, U2PTransform(NewActorHandle->ActorToBody));

	ValidateArrays();

	//We sort entities so that dynamics are first, kinematics second, and static third
	if (ActorType == ECreateActorType::DynamicActor)	//dynamic first
	{
		if (NumSimulatedBodies < ActorDataIndex)	//There's at least one kinematic/static entity so swap
		{
			const int32 FirstNonSimulatedIndex = NumSimulatedBodies;
			SwapActorData(FirstNonSimulatedIndex, ActorDataIndex);
		}

		++NumSimulatedBodies;
	}
	else if(ActorType == ECreateActorType::KinematicActor)  //then kinematics
	{
		const uint32 NumMobileBodies = NumKinematicBodies + NumSimulatedBodies;
		if (NumMobileBodies < ActorDataIndex)	//There's at least one static entity so swap
		{
			const uint32 FirstStaticIndex = NumMobileBodies;
			SwapActorData(FirstStaticIndex, ActorDataIndex);
		}

		++NumKinematicBodies;
	}

	return NewActorHandle->ActorDataIndex;
}

FActorHandle* FSimulation::CreateDynamicActor(PxRigidDynamic* RigidDynamic, const FTransform& TM)
{
	const uint32 ActorDataIndex = CreateActor<ECreateActorType::DynamicActor>(RigidDynamic, TM);
	NumActiveSimulatedBodies = NumSimulatedBodies;
	bDirtyJointData = true;

	return ActorHandles[ActorDataIndex];
}

FActorHandle* FSimulation::CreateKinematicActor(PxRigidBody* RigidBody, const FTransform& TM)
{
	const uint32 ActorDataIndex = CreateActor<ECreateActorType::KinematicActor>(RigidBody, TM);
	return ActorHandles[ActorDataIndex];
}

FActorHandle* FSimulation::CreateStaticActor(PxRigidActor* RigidActor, const FTransform& TM)
{
	const uint32 ActorDataIndex = CreateActor<ECreateActorType::StaticActor>(RigidActor, TM);
	return ActorHandles[ActorDataIndex];
}
#endif


void FSimulation::SwapActorData(uint32 Actor1DataIdx, uint32 Actor2DataIdx)
{
	check(Actors.IsValidIndex(Actor1DataIdx));
	check(Actors.IsValidIndex(Actor2DataIdx));

	Swap(Actors[Actor1DataIdx], Actors[Actor2DataIdx]);
	Swap(ActorHandles[Actor1DataIdx], ActorHandles[Actor2DataIdx]);
	Swap(RigidBodiesData[Actor1DataIdx], RigidBodiesData[Actor2DataIdx]);
	Swap(SolverBodiesData[Actor1DataIdx], SolverBodiesData[Actor2DataIdx]);
	Swap(PendingAcceleration[Actor1DataIdx], PendingAcceleration[Actor2DataIdx]);
	Swap(KinematicTargets[Actor1DataIdx], KinematicTargets[Actor2DataIdx]);

	//Update entity index on the handle
	ActorHandles[Actor1DataIdx]->ActorDataIndex = Actor1DataIdx;
	ActorHandles[Actor2DataIdx]->ActorDataIndex = Actor2DataIdx;
	bDirtyJointData = true;	//reordering of bodies could lead to stale joint data
	bRecreateIterationCache = true;	//reordering of bodies so we need to change iteration order potentially
}

void FSimulation::SwapJointData(uint32 Joint1Idx, uint32 Joint2Idx)
{
	check(JointData.IsValidIndex(Joint1Idx));
	check(JointData.IsValidIndex(Joint2Idx));

	Swap(JointData[Joint1Idx], JointData[Joint2Idx]);
	Swap(Joints[Joint1Idx], Joints[Joint2Idx]);
	Swap(JointHandles[Joint1Idx], JointHandles[Joint2Idx]);

	//Update joint index on the handle
	JointHandles[Joint1Idx]->JointDataIndex = Joint1Idx;
	JointHandles[Joint2Idx]->JointDataIndex = Joint2Idx;
	bDirtyJointData = true;	//reordering of joints could lead to stale joint data
}

void FSimulation::ValidateArrays() const
{
	check(Actors.Num() == ActorHandles.Num());
#if WITH_PHYSX
	check(Actors.Num() == RigidBodiesData.Num());
	check(Actors.Num() == SolverBodiesData.Num());
	check(Actors.Num() == PendingAcceleration.Num());
	check(Actors.Num() == KinematicTargets.Num());
	check(Joints.Num() == JointData.Num());
	check(Joints.Num() == JointHandles.Num());
	
#endif
}

/*static char*  CacheHack = nullptr;
int32 GCacheSumHack = 0;

TAutoConsoleVariable<int32> GHackEvictCache(TEXT("p.ForceCacheEvict"), 1, TEXT("blah"));

void FSimulation::EvictCache()
{
	if(GHackEvictCache.GetValueOnAnyThread() == 0)
	{
		return;
	}

	if(CacheHack == nullptr)
	{
		CacheHack = (char*) FMemory::Malloc(1024*1024*2);
	}

	const int32 NumCacheLines = 1024*1024*2/64;
	for(int i=0; i<NumCacheLines; ++i)
	{
		GCacheSumHack += CacheHack[i*64];
	}
	
}*/

DECLARE_DWORD_COUNTER_STAT(TEXT("Simulated Bodies"), STAT_IPNumSimulatedBodies, STATGROUP_ImmediatePhysics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Simulated Bodies"), STAT_IPNumActiveSimulatedBodies, STATGROUP_ImmediatePhysics);
DECLARE_DWORD_COUNTER_STAT(TEXT("Kinematic Bodies"), STAT_IPNumKinematicBodies, STATGROUP_ImmediatePhysics);

DECLARE_CYCLE_STAT(TEXT("FSimulation::Simulate"), STAT_ImmediateSimulate, STATGROUP_ImmediatePhysics);

void FSimulation::Simulate(float DeltaTime, const FVector& Gravity)
{
	SET_DWORD_STAT(STAT_IPNumSimulatedBodies, NumSimulatedBodies);
	SET_DWORD_STAT(STAT_IPNumActiveSimulatedBodies, NumActiveSimulatedBodies);
	SET_DWORD_STAT(STAT_IPNumKinematicBodies, NumKinematicBodies);

	DeltaTime = FMath::Min(DeltaTime, 0.033f);
	//Create dynamic bodies and integrate their unconstrained velocities
	if(DeltaTime > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ImmediateSimulate);

		++SimCount;

		ConstructSolverBodies(DeltaTime, Gravity);
	
		if(bRecreateIterationCache)
		{
			PrepareIterationCache();
		}
		
		GenerateContacts();
		
		BatchConstraints();

		PrepareConstraints(DeltaTime);

		SolveAndIntegrate(DeltaTime);

		Workspace.Reset();
	}

	//EvictCache();
}

DECLARE_CYCLE_STAT(TEXT("ConstructSolverBodies"), STAT_ImmediateConstructSolverBodies, STATGROUP_ImmediatePhysics);

void FSimulation::ConstructSolverBodies(float DeltaTime, const FVector& Gravity)
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediateConstructSolverBodies);
	const int32 NumBytes = Actors.Num() * sizeof(PxSolverBody);
	SolverBodies = (PxSolverBody*)Workspace.Alloc(NumBytes);

	for(uint32 BodyIdx = 0; BodyIdx < NumActiveSimulatedBodies; ++BodyIdx)
	{
		RigidBodiesData[BodyIdx].linearVelocity += PendingAcceleration[BodyIdx] * DeltaTime;
	}

	FMemory::Memzero(PendingAcceleration.GetData(), sizeof(PendingAcceleration[0]) * NumSimulatedBodies);

	FMemory::Memzero(SolverBodies, NumBytes);
	immediate::PxConstructSolverBodies(RigidBodiesData.GetData(), SolverBodiesData.GetData(), NumActiveSimulatedBodies, U2PVector(Gravity), DeltaTime);
	
	const float DeltaTimeInv = 1.f / DeltaTime;

	//compute velocity for kinematic targets
	const uint32 NumActors = Actors.Num();
	for (uint32 KinematicBodyIdx = NumSimulatedBodies; KinematicBodyIdx < NumActors; ++KinematicBodyIdx)
	{
		const FKinematicTarget& KinematicTarget = KinematicTargets[KinematicBodyIdx];
		if(KinematicTarget.bTargetSet)
		{
			immediate::PxRigidBodyData& RigidBodyData = RigidBodiesData[KinematicBodyIdx];
			const PxTransform& CurrentTM = RigidBodyData.body2World;
			PxVec3 LinearDelta = KinematicTarget.BodyToWorld.p - CurrentTM.p;
			PxQuat AngularDelta = KinematicTarget.BodyToWorld.q * CurrentTM.q.getConjugate();

			if (AngularDelta.w < 0)	//shortest angle.
				AngularDelta = -AngularDelta;

			PxReal Angle;
			PxVec3 Axis;
			AngularDelta.toRadiansAndUnitAxis(Angle, Axis);
			
			RigidBodyData.linearVelocity = LinearDelta * DeltaTimeInv;
			RigidBodyData.angularVelocity = Axis * Angle * DeltaTimeInv;
		}
	}

	immediate::PxConstructSolverBodies(RigidBodiesData.GetData() + NumActiveSimulatedBodies, SolverBodiesData.GetData() + NumActiveSimulatedBodies, NumKinematicBodies + (NumSimulatedBodies - NumActiveSimulatedBodies), PxVec3(PxZero), DeltaTime);
#endif
}

DECLARE_CYCLE_STAT(TEXT("PrepareIterationCache"), STAT_ImmediatePrepareIterationCache, STATGROUP_ImmediatePhysics);

void FSimulation::PrepareIterationCache()
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediatePrepareIterationCache);
	// Iteration cache is not yet available, so we need to iterate over bodies and prepare the data as needed
	const int32 NumActors = Actors.Num();
	ShapeSOA.LocalTMs.Empty(NumActors);
	ShapeSOA.Materials.Empty(NumActors);
	ShapeSOA.Geometries.Empty(NumActors);
	ShapeSOA.Bounds.Empty(NumActors);
	ShapeSOA.OwningActors.Empty(NumActors);
	ShapeSOA.BoundsOffsets.Empty(NumActors);

	int32 NumShapes = 0;
	NumSimulatedShapesWithCollision = 0;

	//flatten out all shapes of actors that have collision
	for(int32 ActorIdx = 0; ActorIdx < NumActors; ++ActorIdx)
	{
		const FActor& Actor = Actors[ActorIdx];
		if(IgnoreCollisionActors.Contains(ActorHandles[ActorIdx]))
		{
			if (ActorIdx+1 == NumSimulatedBodies)
			{
				NumSimulatedShapesWithCollision = NumShapes;
			}

			continue;
		}

		for(const FShape& Shape : Actor.Shapes)
		{
			ShapeSOA.LocalTMs.Add(Shape.LocalTM);
			ShapeSOA.Materials.Add(Shape.Material);
			ShapeSOA.Geometries.Add(Shape.Geometry);
			ShapeSOA.Bounds.Add(Shape.BoundsMagnitude);
			ShapeSOA.BoundsOffsets.Add(Shape.BoundsOffset);
			ShapeSOA.OwningActors.Add(ActorIdx);
			++NumShapes;
		}

		if (ActorIdx+1 == NumSimulatedBodies)
		{
			NumSimulatedShapesWithCollision = NumShapes;
		}
	}

	//build cache of which shapes to skip
	int32 IterationCount = 0;
	SkipCollisionCache.Empty(NumShapes);

	for(uint32 SimShapeIdx = 0; SimShapeIdx < NumSimulatedShapesWithCollision; ++SimShapeIdx)
	{
		const int32 SimActorIdx = ShapeSOA.OwningActors[SimShapeIdx];
		const TSet<FActorHandle*>* IgnoreActorsForSimulated = IgnoreCollisionPairTable.Find(ActorHandles[SimActorIdx]);

		for(int32 OtherShapeIdx = SimShapeIdx + 1; OtherShapeIdx < NumShapes; ++OtherShapeIdx)
		{
			const int32 OtherActorIdx = ShapeSOA.OwningActors[OtherShapeIdx];

			if(SimActorIdx == OtherActorIdx || (IgnoreActorsForSimulated && IgnoreActorsForSimulated->Find(ActorHandles[OtherActorIdx])))
			{
				SkipCollisionCache.Add(IterationCount++);
			}
			else
			{
				++IterationCount;
			}
		}
	}

#if PERSISTENT_CONTACT_PAIRS
	const int32 NumPairs = (NumShapes) * (NumShapes - 1) / 2;
	ShapeSOA.ContactPairData.Empty(NumPairs);
	ShapeSOA.ContactPairData.AddZeroed(NumPairs);
#endif

	bRecreateIterationCache = false;
#endif
}

DECLARE_CYCLE_STAT(TEXT("GenerateContacts"), STAT_ImmediateGenerateContacts, STATGROUP_ImmediatePhysics);

void FSimulation::GenerateContacts()
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediateGenerateContacts);

	//narrow phase, find actual contact points
	CacheAllocator.Reset();

	ContactPairs.Reset();
	ContactPoints.Reset();
	
	const int32 NumShapes = ShapeSOA.LocalTMs.Num();
	PxTransform* ShapeWorldTMs = (PxTransform*)Workspace.Alloc(sizeof(PxTransform) * NumShapes);
	for(int32 ShapeIdx = 0; ShapeIdx < NumShapes; ++ShapeIdx)
	{
		const immediate::PxRigidBodyData& Body = RigidBodiesData[ShapeSOA.OwningActors[ShapeIdx]];
		ShapeWorldTMs[ShapeIdx] = Body.body2World * ShapeSOA.LocalTMs[ShapeIdx];
	}

	int32 IterationCount = 0;
	int32 PotentialPairCount = 0;
	const int32 IterationCacheSize = SkipCollisionCache.Num();
	int32 CurrentIterationCacheIdx = 0;
	const int32* SkipCollisionCacheRaw = SkipCollisionCache.GetData();

	for(uint32 SimShapeIdx = 0; SimShapeIdx < NumSimulatedShapesWithCollision; ++SimShapeIdx)
	{
		const float SimulatedRadius = ShapeSOA.Bounds[SimShapeIdx];
		const PxTransform& SimulatedShapeTM = ShapeWorldTMs[SimShapeIdx];
		const PxVec3 SimulatedBoundsOffset = ShapeSOA.BoundsOffsets[SimShapeIdx];
		const PxVec3 SimulatedShapeBoundsOrigin = SimulatedShapeTM.transform(SimulatedBoundsOffset);
		
		const uint32 SimulatedActorIdx = ShapeSOA.OwningActors[SimShapeIdx];
		const FMaterial& SimulatedShapeMaterial = ShapeSOA.Materials[SimShapeIdx];
		const PxGeometry* SimulatedGeometry = ShapeSOA.Geometries[SimShapeIdx];

		if(SimulatedActorIdx >= NumActiveSimulatedBodies)
		{
			break;
		}

		for(int32 OtherShapeIdx = SimShapeIdx + 1; OtherShapeIdx < NumShapes; ++OtherShapeIdx)
		{
			if(CurrentIterationCacheIdx < IterationCacheSize && SkipCollisionCache[CurrentIterationCacheIdx] == IterationCount++)
			{
				++CurrentIterationCacheIdx;
				continue;
			}

			const uint32 OtherActorIdx = ShapeSOA.OwningActors[OtherShapeIdx];
			if(OtherActorIdx >= NumActiveSimulatedBodies && OtherActorIdx < NumSimulatedBodies)
			{
				continue;
			}

			PxCache* InCache;
			const int32 PotentialPairIdx = PotentialPairCount++;
#if PERSISTENT_CONTACT_PAIRS
			FPersistentContactPairData& PersistentPairData = ShapeSOA.ContactPairData[PotentialPairIdx];
			InCache = &PersistentPairData.Cache;

			if(PersistentPairData.SimCount + 1 != SimCount)	//Note: in the case where we roll over to 0 does this matter? Could use % but seems unneeded for now
			{
				PersistentPairData.Clear();
			}
#else
			PxCache Cache;
			InCache = &Cache;
#endif
			const FMaterial& OtherShapeMaterial = ShapeSOA.Materials[OtherShapeIdx];
			const float OtherRadius = ShapeSOA.Bounds[OtherShapeIdx];
			const float TotalRadius = (SimulatedRadius + OtherRadius);
			const float TotalRadius2 = TotalRadius * TotalRadius;
			const PxTransform& OtherShapeTM = ShapeWorldTMs[OtherShapeIdx];
			const PxVec3 OtherShapeBoundsOrigin = OtherShapeTM.transform(ShapeSOA.BoundsOffsets[OtherShapeIdx]);

			const float Distance2 = (SimulatedShapeBoundsOrigin  - OtherShapeBoundsOrigin).magnitudeSquared();

			if ( Distance2 > TotalRadius2)
			{
#if PERSISTENT_CONTACT_PAIRS
				PersistentPairData.Clear();
#endif
				continue;	//no intersection so skip narrow phase
			}

			FContactPointRecorder ContactRecorder(*this, SimulatedActorIdx, OtherActorIdx, PotentialPairIdx, SimulatedShapeMaterial, OtherShapeMaterial);
			if (!immediate::PxGenerateContacts(&SimulatedGeometry, &ShapeSOA.Geometries[OtherShapeIdx], &SimulatedShapeTM, &OtherShapeTM, InCache, 1, ContactRecorder, 4.f, 1.f, 100.f, CacheAllocator))
			{
#if PERSISTENT_CONTACT_PAIRS
				//no contacts found so clear friction
				PersistentPairData.Frictions = nullptr;
				PersistentPairData.NumFrictions = 0;
#endif
			}
		}
	}
#endif
}

bool GBatchJoints = false;	//TODO: temp hack to fix crash in immediate mode due to joint batching

DECLARE_CYCLE_STAT(TEXT("BatchConstraints"), STAT_ImmediateBatchConstraints, STATGROUP_ImmediatePhysics);

void FSimulation::BatchConstraints()
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediateBatchConstraints);
	
	const int32 NumContactPairs = ContactPairs.Num();
	const int32 NumJoints = JointData.Num();
	const int32 NumConstraints = NumJoints + NumContactPairs;
	OrderedDescriptors.SetNum(NumConstraints); //Ordered descriptors are semi-persistent. We only reorder joint constraints when needed, but we always update contact constraints which come at the end of the array.
	BatchHeaders.SetNum(NumConstraints);	   //batch headers follow the same approach
	
	// Joint constraints if needed
	if (bDirtyJointData)
	{
		NumActiveJoints = 0;
		if(NumJoints > 0)
		{
			PxSolverConstraintDesc* JointDescriptors = (PxSolverConstraintDesc*)Workspace.Alloc(sizeof(PxSolverConstraintDesc) * NumJoints);

			for (int32 JointIdx = 0; JointIdx < NumJoints; ++JointIdx)
			{
				PxSolverConstraintDesc& JointDescriptor = JointDescriptors[NumActiveJoints];

				const FJoint& Joint = Joints[JointIdx];
				const PxU32 DynamicActorIdx = Joint.DynamicActor->ActorDataIndex;
				const PxU32 OtherActorIdx = Joint.OtherActor != nullptr ? Joint.OtherActor->ActorDataIndex : INDEX_NONE;

				if( (DynamicActorIdx >= NumActiveSimulatedBodies && DynamicActorIdx < NumSimulatedBodies) || (OtherActorIdx >= NumActiveSimulatedBodies && OtherActorIdx < NumSimulatedBodies) )
				{
					continue;
				}

				JointDescriptor.bodyA = &SolverBodies[DynamicActorIdx];
				JointDescriptor.bodyB = OtherActorIdx != INDEX_NONE ? &SolverBodies[OtherActorIdx] : nullptr;

				JointDescriptor.bodyADataIndex = PxU16(DynamicActorIdx);
				JointDescriptor.bodyBDataIndex = PxU16(OtherActorIdx);
				JointDescriptor.linkIndexA = PxSolverConstraintDesc::NO_LINK;
				JointDescriptor.linkIndexB = PxSolverConstraintDesc::NO_LINK;
				JointDescriptor.writeBack = nullptr;

				JointDescriptor.constraint = (PxU8*)JointHandles[JointIdx];
				JointDescriptor.constraintLengthOver16 = PxSolverConstraintDesc::eJOINT_CONSTRAINT;

				++NumActiveJoints;
			}

			if(GBatchJoints)
			{
				NumJointHeaders = physx::immediate::PxBatchConstraints(JointDescriptors, NumActiveJoints, SolverBodies, Actors.Num(), BatchHeaders.GetData(), OrderedDescriptors.GetData());
			}
			else
			{
				NumJointHeaders = NumActiveJoints;
				OrderedDescriptors.AddUninitialized(NumJointHeaders);
				FMemory::Memcpy(OrderedDescriptors.GetData(), JointDescriptors, sizeof(OrderedDescriptors[0]) * NumJointHeaders);

				for(uint32 Offset = 0; Offset < NumActiveJoints; ++Offset)
				{
					PxConstraintBatchHeader& BatchHeader = BatchHeaders[Offset];
					BatchHeader.mStartIndex = Offset;
					BatchHeader.mStride = 1;
				}
			}
			

			//ensure D6 joint data and low level joint data are matching the Ordered descriptors
			for(uint32 DescriptorIdx = 0; DescriptorIdx < NumActiveJoints; ++DescriptorIdx)
			{
				FJointHandle* JointHandle = (FJointHandle*) OrderedDescriptors[DescriptorIdx].constraint;
				if(JointHandle->JointDataIndex != DescriptorIdx)
				{
					//The joint or the descriptor have moved - either way fix it
					SwapJointData(DescriptorIdx, JointHandle->JointDataIndex);
				}
			}
		}
		else
		{
			NumJointHeaders = 0;
		}
	}

	if(NumContactPairs > 0)
	{
		//Contact constraints
		PxSolverConstraintDesc* ContactDescriptors = (PxSolverConstraintDesc*)Workspace.Alloc(sizeof(PxSolverConstraintDesc) * NumContactPairs);
		for (int32 ContactPairIdx = 0; ContactPairIdx < NumContactPairs; ++ContactPairIdx)
		{
			const FContactPair& ContactPair = ContactPairs[ContactPairIdx];
			PxSolverConstraintDesc& ContactDescriptor = ContactDescriptors[ContactPairIdx];

			//Set body pointers and bodyData idxs
			ContactDescriptor.bodyA = &SolverBodies[ContactPair.DynamicActorDataIndex];
			ContactDescriptor.bodyB = &SolverBodies[ContactPair.OtherActorDataIndex];

			ContactDescriptor.bodyADataIndex = PxU16(ContactPair.DynamicActorDataIndex);
			ContactDescriptor.bodyBDataIndex = PxU16(ContactPair.OtherActorDataIndex);
			ContactDescriptor.linkIndexA = PxSolverConstraintDesc::NO_LINK;
			ContactDescriptor.linkIndexB = PxSolverConstraintDesc::NO_LINK;
			ContactDescriptor.writeBack = nullptr;

			ContactDescriptor.constraint = (PxU8*)(&ContactPair);	//Need to save this off for later when the constraint descriptors get re-ordered
			ContactDescriptor.constraintLengthOver16 = PxSolverConstraintDesc::eCONTACT_CONSTRAINT;
		}

		NumContactHeaders = physx::immediate::PxBatchConstraints(ContactDescriptors, NumContactPairs, SolverBodies, Actors.Num(), BatchHeaders.GetData() + NumJointHeaders, OrderedDescriptors.GetData() + NumActiveJoints);
	}
	else
	{
		NumContactHeaders = 0;
	}
	
#endif
}

//float GReadCacheHack = 0.f;

DECLARE_CYCLE_STAT(TEXT("PrepareConstraints"), STAT_ImmediatePrepareConstraints, STATGROUP_ImmediatePhysics);

void FSimulation::PrepareConstraints(float DeltaTime)
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediatePrepareConstraints);

	//Prepares constraint data based on latest position data and contact pairs
	const int32 NumBodies = Actors.Num();
	const float InvDeltaTime = 1.f / DeltaTime;
	ConstraintAllocator.Reset();

	PxSolverConstraintPrepDesc JointConstraints[4];
	Px1DConstraint Rows[24];	//24 rows because we have at most 6 rows per joint and we could be solving at most 4 joints in one batch

	/*FPlatformMisc::Prefetch(OrderedDescriptors.GetData());
	FPlatformMisc::Prefetch(BatchHeaders.GetData());
	FPlatformMisc::Prefetch(SolverBodiesData.GetData());
	FPlatformMisc::Prefetch(JointData.GetData());*/

	const int32 NumMobileBodies = NumSimulatedBodies + NumKinematicBodies;
	
	//Joint constraints
	for (PxU32 HeaderIdx = 0; HeaderIdx < NumJointHeaders; ++HeaderIdx)
	{
		PxConstraintBatchHeader& Header = BatchHeaders[HeaderIdx];
		Header.mConstraintType = PxSolverConstraintDesc::eJOINT_CONSTRAINT;	//Since joint headers are cached and solver re-uses this we have to reset it every time

		FMemory::Memzero(Rows, sizeof(Rows));	//TODO: do we actually need to do this?
		for (Px1DConstraint& Constraint : Rows)
		{
			Constraint.minImpulse = -FLT_MAX;
			Constraint.maxImpulse = FLT_MAX;
		}

		int32 CurRows = 0;

		for (PxU32 BatchInnerIdx = 0; BatchInnerIdx < Header.mStride; ++BatchInnerIdx)
		{
			const PxU32 DescriptorIdx = Header.mStartIndex + BatchInnerIdx;
			//FPlatformMisc::PrefetchBlock(&JointData[DescriptorIdx], 128);
			//GReadCacheHack = JointData[DescriptorIdx].invMassScale.angular0;

			PxSolverConstraintDesc& OrderedDescriptor = OrderedDescriptors[DescriptorIdx];
			PxSolverConstraintPrepDesc& JointDescriptor = JointConstraints[BatchInnerIdx];

			//TODO: is it worth caching some of this? Most of the data doesn't change each frame
			JointDescriptor.body0 = OrderedDescriptor.bodyA;
			JointDescriptor.body1 = OrderedDescriptor.bodyB;
			JointDescriptor.data0 = &SolverBodiesData[OrderedDescriptor.bodyADataIndex];
			JointDescriptor.data1 = &SolverBodiesData[OrderedDescriptor.bodyBDataIndex];

			JointDescriptor.bodyFrame0 = JointDescriptor.data0->body2World;
			JointDescriptor.bodyFrame1 = JointDescriptor.data1->body2World;

			JointDescriptor.bodyState0 = PxSolverConstraintPrepDescBase::eDYNAMIC_BODY;
			JointDescriptor.bodyState1 = JointDescriptor.body1 == nullptr ? PxSolverConstraintPrepDescBase::eSTATIC_BODY : (OrderedDescriptor.bodyBDataIndex < NumSimulatedBodies ? PxSolverConstraintPrepDescBase::eDYNAMIC_BODY : PxSolverConstraintPrepDescBase::eSTATIC_BODY);
			JointDescriptor.desc = &OrderedDescriptor;
			JointDescriptor.mInvMassScales.angular0 = JointDescriptor.mInvMassScales.angular1 = JointDescriptor.mInvMassScales.linear0 = JointDescriptor.mInvMassScales.linear1 = 1.f;
			JointDescriptor.writeback = NULL;
			//Constraint.getBreakForce(JointDescriptor.linBreakForce, JointDescriptor.angBreakForce);
			JointDescriptor.linBreakForce = FLT_MAX;
			JointDescriptor.angBreakForce = FLT_MAX;

			JointDescriptor.minResponseThreshold = 0;//Constraint.getMinResponseThreshold();
			JointDescriptor.disablePreprocessing = false;//!!(Constraint.getFlags() & PxConstraintFlag::eDISABLE_PREPROCESSING);
			JointDescriptor.improvedSlerp = true;//!!(Constraint.getFlags() & PxConstraintFlag::eIMPROVED_SLERP);
			JointDescriptor.driveLimitsAreForces = false;//!!(Constraint.getFlags() & PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES);

			PxVec3 Body0WorldOffset;
			PxConstraintInvMassScale InvMassScale;
			const D6JointData& Joint = JointData[DescriptorIdx];	
			uint32 NumRows = D6JointSolverPrep(&Rows[CurRows], Body0WorldOffset, 0, InvMassScale, (const void*)&Joint, JointDescriptor.bodyFrame0, JointDescriptor.bodyFrame1);

			JointDescriptor.mInvMassScales = InvMassScale;

			JointDescriptor.numRows = NumRows;
			JointDescriptor.rows = &Rows[CurRows];
			CurRows += NumRows;
		}

		immediate::PxCreateJointConstraints(&Header, 1, JointConstraints, ConstraintAllocator, DeltaTime, InvDeltaTime);
	}

	bDirtyJointData = false;

	//Contact constraints
	const PxU32 NumTotalHeaders = NumContactHeaders + NumJointHeaders;
	PxSolverContactDesc ContactDescriptors[4];
	const FContactPair* InnerContactPairs[4];
	const PxU32 NumJoints = JointData.Num();

	for (PxU32 HeaderIdx = NumJointHeaders; HeaderIdx < NumTotalHeaders; ++HeaderIdx)
	{
		PxConstraintBatchHeader& Header = BatchHeaders[HeaderIdx];
		check(Header.mConstraintType == PxSolverConstraintDesc::eCONTACT_CONSTRAINT);

		Header.mStartIndex += NumActiveJoints;
		
		for (PxU32 BatchInnerIdx = 0; BatchInnerIdx < Header.mStride; ++BatchInnerIdx)
		{
			PxSolverConstraintDesc& OrderedDescriptor = OrderedDescriptors[Header.mStartIndex + BatchInnerIdx];
			PxSolverContactDesc& ContactDescriptor = ContactDescriptors[BatchInnerIdx];
			//Extract the contact pair that we saved in this structure earlier.
			const FContactPair& ContactPair = *reinterpret_cast<const FContactPair*>(OrderedDescriptor.constraint);
			InnerContactPairs[BatchInnerIdx] = &ContactPair;

			ContactDescriptor.body0 = OrderedDescriptor.bodyA;
			ContactDescriptor.body1 = OrderedDescriptor.bodyB;
			ContactDescriptor.data0 = &SolverBodiesData[OrderedDescriptor.bodyADataIndex];
			ContactDescriptor.data1 = &SolverBodiesData[OrderedDescriptor.bodyBDataIndex];

			ContactDescriptor.bodyFrame0 = ContactDescriptor.data0->body2World;
			ContactDescriptor.bodyFrame1 = ContactDescriptor.data1->body2World;

			ContactDescriptor.contactForces = nullptr;//&ContactForces[ContactPair.StartContactIndex];
			ContactDescriptor.contacts = &ContactPoints[ContactPair.StartContactIndex];
			ContactDescriptor.numContacts = ContactPair.NumContacts;

#if  PERSISTENT_CONTACT_PAIRS
			FPersistentContactPairData& PersistentPairData = ShapeSOA.ContactPairData[ContactPair.PairIdx];

			ContactDescriptor.frictionPtr = PersistentPairData.Frictions;
			ContactDescriptor.frictionCount = PersistentPairData.NumFrictions;
#else
			ContactDescriptor.frictionPtr = nullptr;
			ContactDescriptor.frictionCount = 0;
#endif

			ContactDescriptor.disableStrongFriction = false;
			ContactDescriptor.hasMaxImpulse = false;
			ContactDescriptor.hasForceThresholds = false;
			ContactDescriptor.shapeInteraction = NULL;
			ContactDescriptor.restDistance = 0.f;
			ContactDescriptor.maxCCDSeparation = PX_MAX_F32;

			ContactDescriptor.bodyState0 = PxSolverConstraintPrepDescBase::eDYNAMIC_BODY;
			ContactDescriptor.bodyState1 = ContactPair.OtherActorDataIndex < NumSimulatedBodies ? PxSolverConstraintPrepDescBase::eDYNAMIC_BODY : PxSolverConstraintPrepDescBase::eSTATIC_BODY;
			ContactDescriptor.desc = &OrderedDescriptor;
			ContactDescriptor.mInvMassScales.angular0 = ContactDescriptor.mInvMassScales.angular1 = ContactDescriptor.mInvMassScales.linear0 = ContactDescriptor.mInvMassScales.linear1 = 1.f;
		}

		immediate::PxCreateContactConstraints(&Header, 1, &ContactDescriptors[0], ConstraintAllocator, InvDeltaTime, -200.f, 4.f, 1.f);

#if PERSISTENT_CONTACT_PAIRS
		// Now cache friction results
		for (PxU32 BatchInnerIdx = 0; BatchInnerIdx < Header.mStride; ++BatchInnerIdx)
		{
			PxSolverContactDesc& ContactDescriptor = ContactDescriptors[BatchInnerIdx];

			//Extract the contact pair that we saved in this structure earlier.
			const FContactPair& ContactPair = *InnerContactPairs[BatchInnerIdx];
			FPersistentContactPairData& PersistentPairData = ShapeSOA.ContactPairData[ContactPair.PairIdx];

			PersistentPairData.Frictions = ContactDescriptor.frictionPtr;
			PersistentPairData.NumFrictions = ContactDescriptor.frictionCount;
		}
#endif
	}

#endif
}

DECLARE_CYCLE_STAT(TEXT("SolveAndIntegrate"), STAT_ImmediateSolveAndIntegrate, STATGROUP_ImmediatePhysics);


void FSimulation::SolveAndIntegrate(float DeltaTime)
{
#if WITH_PHYSX
	SCOPE_CYCLE_COUNTER(STAT_ImmediateSolveAndIntegrate);

	PxVec3* LinearMotionVelocity = (PxVec3*) Workspace.Alloc(sizeof(PxVec3) * NumActiveSimulatedBodies * 2);
	PxVec3* AngularMotionVelocity = &LinearMotionVelocity[NumActiveSimulatedBodies];

	//Solve all constraints
	immediate::PxSolveConstraints(BatchHeaders.GetData(), NumContactHeaders + NumJointHeaders, OrderedDescriptors.GetData(), SolverBodies, LinearMotionVelocity, AngularMotionVelocity, NumActiveSimulatedBodies, NumPositionIterations, NumVelocityIterations);

	//Integrate velocities
	immediate::PxIntegrateSolverBodies(SolverBodiesData.GetData(), SolverBodies, LinearMotionVelocity, AngularMotionVelocity, NumActiveSimulatedBodies, DeltaTime);

	//Copy positions of dynamic bodies back into rigid body
	for (uint32 DynamicsBodyIdx = 0; DynamicsBodyIdx < NumActiveSimulatedBodies; ++DynamicsBodyIdx)
	{
		RigidBodiesData[DynamicsBodyIdx].linearVelocity = SolverBodiesData[DynamicsBodyIdx].linearVelocity;
		RigidBodiesData[DynamicsBodyIdx].angularVelocity = SolverBodiesData[DynamicsBodyIdx].angularVelocity;
		RigidBodiesData[DynamicsBodyIdx].body2World = SolverBodiesData[DynamicsBodyIdx].body2World;
	}

	//Copy kinematic targets
	const uint32 NumActors = Actors.Num();
	for(uint32 KinematicBodyIdx = NumSimulatedBodies; KinematicBodyIdx < NumActors; ++KinematicBodyIdx)
	{
		if(KinematicTargets[KinematicBodyIdx].bTargetSet)
		{
			RigidBodiesData[KinematicBodyIdx].body2World = KinematicTargets[KinematicBodyIdx].BodyToWorld;
			RigidBodiesData[KinematicBodyIdx].linearVelocity = PxVec3(PxZero);
			RigidBodiesData[KinematicBodyIdx].angularVelocity = PxVec3(PxZero);
		}
	}

	FMemory::Memzero(KinematicTargets.GetData(), NumActors * sizeof(KinematicTargets[0]));
#endif
}

void FSimulation::AddRadialForce(int32 ActorDataIndex, const FVector& InOrigin, float Strength, float Radius, ERadialImpulseFalloff Falloff, EForceType ForceType)
{
#if WITH_PHYSX
	if(IsSimulated(ActorDataIndex))
	{
		PxVec3 Origin = U2PVector(InOrigin);
		immediate::PxRigidBodyData& LowLevelBody = GetLowLevelBody(ActorDataIndex);
		const PxTransform& PCOMTransform = LowLevelBody.body2World;
		PxVec3 Delta = LowLevelBody.body2World.p - Origin;

		float Mag = Delta.magnitude(); // Distance from COM to origin
		if (Mag > Radius)
		{
			return;
		}

		Delta.normalize();

		// Scale by U2PScale here, because units are velocity * mass. 
		float ImpulseMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ImpulseMag *= (1.0f - (Mag / Radius));
		}

		const PxVec3 PImpulse = Delta * ImpulseMag;
		const PxVec3 ApplyDelta = (ForceType == EForceType::AddAcceleration || ForceType == EForceType::AddVelocity) ? PImpulse : PImpulse * LowLevelBody.invMass;
		
		if(ForceType == EForceType::AddImpulse || ForceType == EForceType::AddVelocity)
		{
			LowLevelBody.linearVelocity += ApplyDelta;
		}
		else
		{
			PendingAcceleration[ActorDataIndex] += ApplyDelta;
		}
	}
#endif
}

void FSimulation::AddForce(int32 ActorDataIndex, const FVector& Force)
{
#if WITH_PHYSX
	if (IsSimulated(ActorDataIndex))
	{
		immediate::PxRigidBodyData& LowLevelBody = GetLowLevelBody(ActorDataIndex);
		PendingAcceleration[ActorDataIndex] += U2PVector(Force) * LowLevelBody.invMass;
	}
#endif
}


} // namespace ImmediatePhysics