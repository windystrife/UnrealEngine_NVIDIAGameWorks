// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXSupport.cpp: PhysX
=============================================================================*/

#include "PhysicsEngine/PhysXSupport.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CustomPhysXPayload.h"

#if WITH_PHYSX

#include "PhysXPublic.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodySetup.h"

PxFoundation*			GPhysXFoundation = NULL;
PxPvd*					GPhysXVisualDebugger = NULL;
PxPhysics*				GPhysXSDK = NULL;
FPhysXAllocator*		GPhysXAllocator = NULL;

#if WITH_APEX
ENGINE_API apex::ApexSDK*				GApexSDK = NULL;
ENGINE_API nvidia::apex::PhysX3Interface* GPhysX3Interface = nullptr;

#if WITH_APEX_LEGACY
ENGINE_API apex::Module*				GApexModuleLegacy = NULL;
#endif // WITH_APEX_LEGACY

#if WITH_APEX_CLOTHING
ENGINE_API apex::ModuleClothing*		GApexModuleClothing		= NULL;
#endif //WITH_APEX_CLOTHING

TMap<int16, apex::Scene*>				GPhysXSceneMap;

#if WITH_FLEX
ENGINE_API bool						GFlexIsInitialized = false;
ENGINE_API NvFlexLibrary*           GFlexLib = NULL;
#endif //WITH_FLEX

FApexNullRenderResourceManager		GApexNullRenderResourceManager;
FApexResourceCallback				GApexResourceCallback;
#else	// #if WITH_APEX
TMap<int16, PxScene*>		GPhysXSceneMap;
#endif	// #if WITH_APEX

int32						GNumPhysXConvexMeshes = 0;

TArray<PxConvexMesh*>	GPhysXPendingKillConvex;
TArray<PxTriangleMesh*>	GPhysXPendingKillTriMesh;
TArray<PxHeightField*>	GPhysXPendingKillHeightfield;
TArray<PxMaterial*>		GPhysXPendingKillMaterial;

///////////////////// Unreal to PhysX conversion /////////////////////

PxTransform UMatrix2PTransform(const FMatrix& UTM)
{
	PxQuat PQuat = U2PQuat(UTM.ToQuat());
	PxVec3 PPos = U2PVector(UTM.GetOrigin());

	PxTransform Result(PPos, PQuat);

	return Result;
}

PxTransform U2PTransform(const FTransform& UTransform)
{
	PxQuat PQuat = U2PQuat(UTransform.GetRotation());
	PxVec3 PPos = U2PVector(UTransform.GetLocation());

	PxTransform Result(PPos, PQuat);

	return Result;
}

PxMat44 U2PMatrix(const FMatrix& UTM)
{
	PxMat44 Result;

	const physx::PxMat44 *MatPtr = (const physx::PxMat44 *)(&UTM);
	Result = *MatPtr;

	return Result;
}

UCollision2PGeom::UCollision2PGeom(const FCollisionShape& CollisionShape)
{
	switch (CollisionShape.ShapeType)
	{
		case ECollisionShape::Box:
		{
			new (Storage)PxBoxGeometry(U2PVector(CollisionShape.GetBox()));
			break;
		}
		case ECollisionShape::Sphere:
		{
			new (Storage)PxSphereGeometry(CollisionShape.GetSphereRadius());
			break;
		}
		case ECollisionShape::Capsule:
		{
			new (Storage)PxCapsuleGeometry(CollisionShape.GetCapsuleRadius(), CollisionShape.GetCapsuleAxisHalfLength());
			break;
		}
		default:
			// invalid point
			ensure(false);
	}
}

///////////////////// PhysX to Unreal conversion /////////////////////

FMatrix P2UMatrix(const PxMat44& PMat)
{
	FMatrix Result;
	// we have to use Memcpy instead of typecasting, because PxMat44's are not aligned like FMatrix is
	FMemory::Memcpy(&Result, &PMat, sizeof(PMat));
	return Result;
}

FMatrix PTransform2UMatrix(const PxTransform& PTM)
{
	FQuat UQuat = P2UQuat(PTM.q);
	FVector UPos = P2UVector(PTM.p);

	FMatrix Result = FQuatRotationTranslationMatrix(UQuat, UPos);

	return Result;
}

FTransform P2UTransform(const PxTransform& PTM)
{
	FQuat UQuat = P2UQuat(PTM.q);
	FVector UPos = P2UVector(PTM.p);

	FTransform Result = FTransform(UQuat, UPos);

	return Result;
}

///////////////////// Utils /////////////////////


#if WITH_APEX

PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex)
{
	apex::Scene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return (*ScenePtr)->getPhysXScene();
	}

	return NULL;
}

apex::Scene* GetApexSceneFromIndex(int32 InSceneIndex)
{
	apex::Scene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return *ScenePtr;
	}

	return NULL;
}

#else	// #if WITH_APEX

PxScene* GetPhysXSceneFromIndex(int32 InSceneIndex)
{
	PxScene** ScenePtr = GPhysXSceneMap.Find(InSceneIndex);
	if(ScenePtr != NULL)
	{
		return *ScenePtr;
	}

	return NULL;
}

#endif	// #if WITH_APEX


void AddRadialImpulseToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
#if WITH_PHYSX
	if (!(PRigidBody.getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		float Mass = PRigidBody.getMass();
		PxTransform PCOMTransform = PRigidBody.getGlobalPose().transform(PRigidBody.getCMassLocalPose());
		PxVec3 PCOMPos = PCOMTransform.p; // center of mass in world space
		PxVec3 POrigin = U2PVector(Origin); // origin of radial impulse, in world space
		PxVec3 PDelta = PCOMPos - POrigin; // vector from origin to COM

		float Mag = PDelta.magnitude(); // Distance from COM to origin, in Unreal scale : @todo: do we still need conversion scale?

		// If COM is outside radius, do nothing.
		if (Mag > Radius)
		{
			return;
		}

		PDelta.normalize();

		// Scale by U2PScale here, because units are velocity * mass. 
		float ImpulseMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ImpulseMag *= (1.0f - (Mag / Radius));
		}

		PxVec3 PImpulse = PDelta * ImpulseMag;

		PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
		PRigidBody.addForce(PImpulse, Mode);
	}
#endif // WITH_PHYSX
}

void AddRadialForceToPxRigidBody_AssumesLocked(PxRigidBody& PRigidBody, const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange)
{
#if WITH_PHYSX
	if (!(PRigidBody.getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
	{
		float Mass = PRigidBody.getMass();
		PxTransform PCOMTransform = PRigidBody.getGlobalPose().transform(PRigidBody.getCMassLocalPose());
		PxVec3 PCOMPos = PCOMTransform.p; // center of mass in world space
		PxVec3 POrigin = U2PVector(Origin); // origin of radial impulse, in world space
		PxVec3 PDelta = PCOMPos - POrigin; // vector from

		float Mag = PDelta.magnitude(); // Distance from COM to origin, in Unreal scale : @todo: do we still need conversion scale?

		// If COM is outside radius, do nothing.
		if (Mag > Radius)
		{
			return;
		}

		PDelta.normalize();

		// If using linear falloff, scale with distance.
		float ForceMag = Strength;
		if (Falloff == RIF_Linear)
		{
			ForceMag *= (1.0f - (Mag / Radius));
		}

		// Apply force
		PxVec3 PImpulse = PDelta * ForceMag;
		PRigidBody.addForce(PImpulse, bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE);
	}
#endif // WITH_PHYSX
}

bool IsRigidBodyKinematic_AssumesLocked(const PxRigidBody* PRigidBody)
{
	if (PRigidBody)
	{
		//For some cases we only consider an actor kinematic if it's in the simulation scene. This is in cases where we set a kinematic target
		return (PRigidBody->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC);
	}

	return false;
}

bool IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(const PxRigidBody* PRigidBody)
{
	if (PRigidBody)
	{
		//For some cases we only consider an actor kinematic if it's in the simulation scene. This is in cases where we set a kinematic target
		return (PRigidBody->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) && !(PRigidBody->getActorFlags() & PxActorFlag::eDISABLE_SIMULATION);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// PHYSXSIMFILTERSHADER 

/** Util to return a string for the type of a query (for debugging) */
FString ObjTypeToString(PxFilterObjectAttributes PAtt)
{
	PxFilterObjectType::Enum Type = PxGetFilterObjectType(PAtt);

	if(Type == PxFilterObjectType::eRIGID_STATIC)
	{
		return TEXT("rigid static");
	}
	else if(Type == PxFilterObjectType::eRIGID_DYNAMIC)
	{
		return TEXT("rigid dynamic");
	}

	return TEXT("unknown");
}

PxFilterFlags PhysXSimFilterShader(	PxFilterObjectAttributes attributes0, PxFilterData filterData0, 
									PxFilterObjectAttributes attributes1, PxFilterData filterData1,
									PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize )
{
	//UE_LOG(LogPhysics, Log, TEXT("filterData0 (%s): %x %x %x %x"), *ObjTypeToString(attributes0), filterData0.word0, filterData0.word1, filterData0.word2, filterData0.word3);
	//UE_LOG(LogPhysics, Log, TEXT("filterData1 (%s): %x %x %x %x"), *ObjTypeToString(attributes1), filterData1.word0, filterData1.word1, filterData1.word2, filterData1.word3);

	bool k0 = PxFilterObjectIsKinematic(attributes0);
	bool k1 = PxFilterObjectIsKinematic(attributes1);

	PxU32 FilterFlags0 = (filterData0.word3 & 0xFFFFFF);
	PxU32 FilterFlags1 = (filterData1.word3 & 0xFFFFFF);

	if (k0 && k1)
	{
		//Ignore kinematic kinematic pairs unless they are explicitly requested
		if(!(FilterFlags0&EPDF_KinematicKinematicPairs) && !(FilterFlags1&EPDF_KinematicKinematicPairs))
		{
			return PxFilterFlag::eSUPPRESS;	//NOTE: Waiting on physx fix for refiltering on aggregates. For now use supress which automatically tests when changes to simulation happen
		}
	}
	
	bool s0 = PxGetFilterObjectType(attributes0) == PxFilterObjectType::eRIGID_STATIC;
	bool s1 = PxGetFilterObjectType(attributes1) == PxFilterObjectType::eRIGID_STATIC;

	//ignore static-kinematic (this assumes that statics can't be flagged as kinematics)
	// should return eSUPPRESS here instead eKILL so that kinematics vs statics will still be considered once kinematics become dynamic (dying ragdoll case)
	if((k0 || k1) && (s0 || s1))
	{
		return PxFilterFlag::eSUPPRESS;
	}
	
	// if these bodies are from the same component, use the disable table to see if we should disable collision. This case should only happen for things like skeletalmesh and destruction. The table is only created for skeletal mesh components at the moment
	if(filterData0.word2 == filterData1.word2)
	{
		check(constantBlockSize == sizeof(FPhysSceneShaderInfo));
		const FPhysSceneShaderInfo* PhysSceneShaderInfo = (const FPhysSceneShaderInfo*) constantBlock;
		check(PhysSceneShaderInfo);
		FPhysScene * PhysScene = PhysSceneShaderInfo->PhysScene;
		check(PhysScene);

		const TMap<uint32, TMap<FRigidBodyIndexPair, bool> *> & CollisionDisableTableLookup = PhysScene->GetCollisionDisableTableLookup();
		TMap<FRigidBodyIndexPair, bool>* const * DisableTablePtrPtr = CollisionDisableTableLookup.Find(filterData1.word2);
		if (DisableTablePtrPtr)		//Since collision table is deferred during sub-stepping it's possible that we won't get the collision disable table until the next frame
		{
			TMap<FRigidBodyIndexPair, bool>* DisableTablePtr = *DisableTablePtrPtr;
			FRigidBodyIndexPair BodyPair(filterData0.word0, filterData1.word0); // body indexes are stored in word 0
			if (DisableTablePtr->Find(BodyPair))
			{
				return PxFilterFlag::eKILL;
			}

		}
	}

	// Find out which channels the objects are in
	ECollisionChannel Channel0 = GetCollisionChannel(filterData0.word3);
	ECollisionChannel Channel1 = GetCollisionChannel(filterData1.word3);
	
	// see if 0/1 would like to block the other 
	PxU32 BlockFlagTo1 = (ECC_TO_BITFIELD(Channel1) & filterData0.word1);
	PxU32 BlockFlagTo0 = (ECC_TO_BITFIELD(Channel0) & filterData1.word1);

	bool bDoesWantToBlock = (BlockFlagTo1 && BlockFlagTo0);

	// if don't want to block, suppress
	if ( !bDoesWantToBlock )
	{
		return PxFilterFlag::eSUPPRESS;
	}

	

	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	//todo enabling CCD objects against everything else for now
	if(!(k0 && k1) && ((FilterFlags0&EPDF_CCD) || (FilterFlags1&EPDF_CCD)))
	{
		pairFlags |= PxPairFlag::eDETECT_CCD_CONTACT | PxPairFlag::eSOLVE_CONTACT;
	}


	if((FilterFlags0&EPDF_ContactNotify) || (FilterFlags1&EPDF_ContactNotify))
	{
		pairFlags |= (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS );
	}


	if ((FilterFlags0&EPDF_ModifyContacts) || (FilterFlags1&EPDF_ModifyContacts))
	{
		pairFlags |= (PxPairFlag::eMODIFY_CONTACTS);
	}

	return PxFilterFlags();
}

///////// FPhysXSimEventCallback //////////////////////////////////

void FPhysXSimEventCallback::onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs)
{
	// Check actors are not destroyed
	if( PairHeader.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1) )
	{
		UE_LOG(LogPhysics, Log, TEXT("%llu onContact(): Actors have been deleted!"), (uint64)GFrameCounter );
		return;
	}

	const PxActor* PActor0 = PairHeader.actors[0];
	const PxActor* PActor1 = PairHeader.actors[1];
	check(PActor0 && PActor1);

	const PxRigidBody* PRigidBody0 = PActor0->is<PxRigidBody>();
	const PxRigidBody* PRigidBody1 = PActor1->is<PxRigidBody>();

	const FBodyInstance* BodyInst0 = FPhysxUserData::Get<FBodyInstance>(PActor0->userData);
	const FBodyInstance* BodyInst1 = FPhysxUserData::Get<FBodyInstance>(PActor1->userData);
	
	bool bEitherCustomPayload = false;

	// check if it is a custom payload with special body instance conversion
	if (BodyInst0 == nullptr)
	{
		if (const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(PActor0->userData))
		{
			bEitherCustomPayload = true;
			BodyInst0 = CustomPayload->GetBodyInstance();
		}
	}

	if (BodyInst1 == nullptr)
	{
		if (const FCustomPhysXPayload* CustomPayload = FPhysxUserData::Get<FCustomPhysXPayload>(PActor1->userData))
		{
			bEitherCustomPayload = true;
			BodyInst1 = CustomPayload->GetBodyInstance();
		}
	}

	//if nothing valid just exit
	//if a custom payload (like apex destruction) generates collision between the same body instance we ignore it. This is potentially bad, but in general we have not had a need for this
	if(BodyInst0 == nullptr || BodyInst1 == nullptr || BodyInst0 == BodyInst1)
	{
		return;
	}

	//custom payloads may (hackily) rely on the onContact flag. Apex Destruction needs this for being able to apply damage as a result of collision.
	//Because of this we only want onContact events to happen if the user actually selected bNotifyRigidBodyCollision so we have to check if this is the case
	if (bEitherCustomPayload)
	{
		if (BodyInst0->bNotifyRigidBodyCollision == false && BodyInst1->bNotifyRigidBodyCollision == false)
		{
			return;
		}
	}

	TArray<FCollisionNotifyInfo>& PendingCollisionNotifies = OwningScene->GetPendingCollisionNotifies(SceneType);

	uint32 PreAddingCollisionNotify = PendingCollisionNotifies.Num() - 1;
	TArray<int32> PairNotifyMapping = FBodyInstance::AddCollisionNotifyInfo(BodyInst0, BodyInst1, Pairs, NumPairs, PendingCollisionNotifies);

	// Iterate through contact points
	for(uint32 PairIdx=0; PairIdx<NumPairs; PairIdx++)
	{
		int32 NotifyIdx = PairNotifyMapping[PairIdx];
		if (NotifyIdx == -1)	//the body instance this pair belongs to is not listening for events
		{
			continue;
		}

		FCollisionNotifyInfo * NotifyInfo = &PendingCollisionNotifies[NotifyIdx];
		FCollisionImpactData* ImpactInfo = &(NotifyInfo->RigidCollisionData);

		const PxContactPair* Pair = Pairs + PairIdx;

		// Get the two shapes that are involved in the collision
		const PxShape* Shape0 = Pair->shapes[0];
		check(Shape0);
		const PxShape* Shape1 = Pair->shapes[1];
		check(Shape1);

		// Get materials
		PxMaterial* Material0 = nullptr;
		UPhysicalMaterial* PhysMat0  = nullptr;
		if(Shape0->getNbMaterials() == 1)	//If we have simple geometry or only 1 material we set it here. Otherwise do it per face
		{
			Shape0->getMaterials(&Material0, 1);		
			PhysMat0 = Material0 ? FPhysxUserData::Get<UPhysicalMaterial>(Material0->userData) : nullptr;
		}

		PxMaterial* Material1 = nullptr;
		UPhysicalMaterial* PhysMat1  = nullptr;
		if (Shape1->getNbMaterials() == 1)	//If we have simple geometry or only 1 material we set it here. Otherwise do it per face
		{
			Shape1->getMaterials(&Material1, 1);
			PhysMat1 = Material1 ? FPhysxUserData::Get<UPhysicalMaterial>(Material1->userData) : nullptr;
		}

		// Iterate over contact points
		PxContactPairPoint ContactPointBuffer[16];
		int32 NumContactPoints = Pair->extractContacts(ContactPointBuffer, 16);
		for(int32 PointIdx=0; PointIdx<NumContactPoints; PointIdx++)
		{
			const PxContactPairPoint& Point = ContactPointBuffer[PointIdx];

			const PxVec3 NormalImpulse = Point.impulse.dot(Point.normal) * Point.normal; // project impulse along normal
			ImpactInfo->TotalNormalImpulse += P2UVector(NormalImpulse);
			ImpactInfo->TotalFrictionImpulse += P2UVector(Point.impulse - NormalImpulse); // friction is component not along contact normal

			// Get per face materials
			if(!Material0)	//there is complex geometry or multiple materials so resolve the physical material here
			{
				if(PxMaterial* Material0PerFace = Shape0->getMaterialFromInternalFaceIndex(Point.internalFaceIndex0))
				{
					PhysMat0 = FPhysxUserData::Get<UPhysicalMaterial>(Material0PerFace->userData);
				}
			}

			if (!Material1)	//there is complex geometry or multiple materials so resolve the physical material here
			{
				if(PxMaterial* Material1PerFace = Shape1->getMaterialFromInternalFaceIndex(Point.internalFaceIndex1))
				{
					PhysMat1 = FPhysxUserData::Get<UPhysicalMaterial>(Material1PerFace->userData);
				}
				
			}
			
			new(ImpactInfo->ContactInfos) FRigidBodyContactInfo(
				P2UVector(Point.position), 
				P2UVector(Point.normal), 
				-1.f * Point.separation, 
				PhysMat0, 
				PhysMat1);
		}	
	}

	for (int32 NotifyIdx = PreAddingCollisionNotify + 1; NotifyIdx < PendingCollisionNotifies.Num(); NotifyIdx++)
	{
		FCollisionNotifyInfo * NotifyInfo = &PendingCollisionNotifies[NotifyIdx];
		FCollisionImpactData* ImpactInfo = &(NotifyInfo->RigidCollisionData);
		// Discard pairs that don't generate any force (eg. have been rejected through a modify contact callback).
		if (ImpactInfo->TotalNormalImpulse.SizeSquared() < KINDA_SMALL_NUMBER)
		{
			PendingCollisionNotifies.RemoveAt(NotifyIdx);
			NotifyIdx--;
		}
	}
}

void FPhysXSimEventCallback::onConstraintBreak( PxConstraintInfo* constraints, PxU32 count )
{
	for (int32 i=0; i < (int32)count; ++i)
	{
		PxJoint* Joint = (PxJoint*)(constraints[i].externalReference);

		if (Joint && Joint->userData)
		{
			if (FConstraintInstance* Constraint = FPhysxUserData::Get<FConstraintInstance>(Joint->userData))
			{
				OwningScene->AddPendingOnConstraintBreak(Constraint, SceneType);
			}
		}
	}
}

void FPhysXSimEventCallback::onWake(PxActor** Actors, PxU32 Count)
{
	for(PxU32 ActorIdx = 0; ActorIdx < Count; ++ActorIdx)
	{
		OwningScene->AddPendingSleepingEvent(Actors[ActorIdx], SleepEvent::SET_Wakeup, SceneType);
	}
}

void FPhysXSimEventCallback::onSleep(PxActor** Actors, PxU32 Count)
{
	for (PxU32 ActorIdx = 0; ActorIdx < Count; ++ActorIdx)
	{
		OwningScene->AddPendingSleepingEvent(Actors[ActorIdx], SleepEvent::SET_Sleep, SceneType);
	}
}


//////////////////////////////////////////////////////////////////////////
// FPhysXCookingDataReader

FPhysXCookingDataReader::FPhysXCookingDataReader( FByteBulkData& InBulkData, FBodySetupUVInfo* UVInfo )
{
	// Read cooked physics data
	uint8* DataPtr = (uint8*)InBulkData.Lock( LOCK_READ_ONLY );
	FBufferReader Ar( DataPtr, InBulkData.GetBulkDataSize(), false );
	
	uint8 bLittleEndian = true;
	int32 NumConvexElementsCooked = 0;
	int32 NumMirroredElementsCooked = 0;
	int32 NumTriMeshesCooked = 0;

	Ar << bLittleEndian;
	Ar.SetByteSwapping( PLATFORM_LITTLE_ENDIAN ? !bLittleEndian : !!bLittleEndian );
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << NumTriMeshesCooked;
	
	ConvexMeshes.Empty(NumConvexElementsCooked);
	for( int32 ElementIndex = 0; ElementIndex < NumConvexElementsCooked; ElementIndex++ )
	{
		PxConvexMesh* ConvexMesh = ReadConvexMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		ConvexMeshes.Add( ConvexMesh );
	}

	ConvexMeshesNegX.Empty(NumMirroredElementsCooked);
	for( int32 ElementIndex = 0; ElementIndex < NumMirroredElementsCooked; ElementIndex++ )
	{
		PxConvexMesh* ConvexMeshNegX = ReadConvexMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		ConvexMeshesNegX.Add( ConvexMeshNegX );
	}

	TriMeshes.Empty(NumTriMeshesCooked);
	for(int32 ElementIndex = 0; ElementIndex < NumTriMeshesCooked; ++ElementIndex)
	{
		PxTriangleMesh* TriMesh = ReadTriMesh( Ar, DataPtr, InBulkData.GetBulkDataSize() );
		TriMeshes.Add(TriMesh);
	}

	// Init UVInfo pointer
	check(UVInfo);
	Ar << *UVInfo;

	InBulkData.Unlock();
}

PxConvexMesh* FPhysXCookingDataReader::ReadConvexMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize )
{
	LLM_SCOPE(ELLMTag::PhysXConvexMesh);

	PxConvexMesh* CookedMesh = NULL;
	uint8 IsMeshCooked = false;
	Ar << IsMeshCooked;
	if( IsMeshCooked )
	{
		FPhysXInputStream Buffer( InBulkDataPtr + Ar.Tell(), InBulkDataSize - Ar.Tell() );		
		CookedMesh = GPhysXSDK->createConvexMesh( Buffer );
		check( CookedMesh != NULL );
		Ar.Seek( Ar.Tell() + Buffer.ReadPos );
	}
	return CookedMesh;
}

PxTriangleMesh* FPhysXCookingDataReader::ReadTriMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize )
{
	LLM_SCOPE(ELLMTag::PhysXTriMesh);

	FPhysXInputStream Buffer( InBulkDataPtr + Ar.Tell(), InBulkDataSize - Ar.Tell() );
	PxTriangleMesh* CookedMesh = GPhysXSDK->createTriangleMesh(Buffer);
	check(CookedMesh);
	Ar.Seek( Ar.Tell() + Buffer.ReadPos );
	return CookedMesh;
}

SIZE_T GetPhysxObjectSize(PxBase* Obj, const PxCollection* SharedCollection)
{
	PxSerializationRegistry* Sr = PxSerialization::createSerializationRegistry(*GPhysXSDK);
	PxCollection* Collection = PxCreateCollection();

	Collection->add(*Obj);
	PxSerialization::complete(*Collection, *Sr, SharedCollection);	// chase all other stuff (shared shaps, materials, etc) needed to serialize this collection

	FPhysXCountMemoryStream Out;
	PxSerialization::serializeCollectionToBinary(Out, *Collection, *Sr, SharedCollection);

	Collection->release();
	Sr->release();

	return Out.UsedMemory;
}



FPhysxSharedData* FPhysxSharedData::Singleton = nullptr;

void FPhysxSharedData::Initialize()
{
	check(Singleton == nullptr);
	Singleton = new FPhysxSharedData();
}

void FPhysxSharedData::Terminate()
{
	if (Singleton)
	{
		delete Singleton;
		Singleton = nullptr;
	}
}

void FPhysxSharedData::Add( PxBase* Obj )
{
	if(Obj) 
	{ 
		SharedObjects->add(*Obj, (PxSerialObjectId)Obj);
	}
}

void FPhysxSharedData::DumpSharedMemoryUsage(FOutputDevice* Ar)
{
	struct FSharedResourceEntry
	{
		uint64 MemorySize;
		uint64 Count;
	};

	struct FSortBySize
	{
		FORCEINLINE bool operator()( const FSharedResourceEntry& A, const FSharedResourceEntry& B ) const 
		{ 
			// Sort descending
			return B.MemorySize < A.MemorySize;
		}
	};

	TMap<FString, FSharedResourceEntry> AllocationsByType;

	uint64 OverallSize = 0;
	int32 OverallCount = 0;

	TMap<FString, TArray<PxBase*> > ObjectsByType;

	for (int32 i=0; i < (int32)SharedObjects->getNbObjects(); ++i)
	{
		PxBase& Obj = SharedObjects->getObject(i);
		FString TypeName = ANSI_TO_TCHAR(Obj.getConcreteTypeName());

		TArray<PxBase*>* ObjectsArray = ObjectsByType.Find(TypeName);
		if (ObjectsArray == NULL)
		{
			ObjectsByType.Add(TypeName, TArray<PxBase*>());
			ObjectsArray = ObjectsByType.Find(TypeName);
		}

		check(ObjectsArray);
		ObjectsArray->Add(&Obj);
	}

	TArray<FString> TypeNames;
	ObjectsByType.GetKeys(TypeNames);

	for (int32 TypeIdx=0; TypeIdx < TypeNames.Num(); ++TypeIdx)
	{
		const FString& TypeName = TypeNames[TypeIdx];
		
		TArray<PxBase*>* ObjectsArray = ObjectsByType.Find(TypeName);
		check(ObjectsArray);

		PxSerializationRegistry* Sr = PxSerialization::createSerializationRegistry(*GPhysXSDK);
		PxCollection* Collection = PxCreateCollection();
		
		for (int32 i=0; i < ObjectsArray->Num(); ++i)
		{
			Collection->add(*((*ObjectsArray)[i]));;
		}

		PxSerialization::complete(*Collection, *Sr);	// chase all other stuff (shared shaps, materials, etc) needed to serialize this collection

		FPhysXCountMemoryStream Out;
		PxSerialization::serializeCollectionToBinary(Out, *Collection, *Sr);

		Collection->release();
		Sr->release();

		OverallSize += Out.UsedMemory;
		OverallCount += ObjectsArray->Num();

		FSharedResourceEntry NewEntry;
		NewEntry.Count = ObjectsArray->Num();
		NewEntry.MemorySize = Out.UsedMemory;

		AllocationsByType.Add(TypeName, NewEntry);
	}

	Ar->Logf(TEXT(""));
	Ar->Logf(TEXT("Shared Resources:"));
	Ar->Logf(TEXT(""));

	AllocationsByType.ValueSort(FSortBySize());
	
	Ar->Logf(TEXT("%-10d %s (%d)"), OverallSize, TEXT("Overall"), OverallCount );
	
	for( auto It=AllocationsByType.CreateConstIterator(); It; ++It )
	{
		Ar->Logf(TEXT("%-10d %s (%d)"), It.Value().MemorySize, *It.Key(), It.Value().Count );
	}
}

void AddToCollection(PxCollection* PCollection, PxBase* PBase)
{
	if (PBase)
	{
		PCollection->add(*PBase);
	}
}

PxCollection* MakePhysXCollection(const TArray<UPhysicalMaterial*>& PhysicalMaterials, const TArray<UBodySetup*>& BodySetups, uint64 BaseId)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_CreateSharedData);
	PxCollection* PCollection = PxCreateCollection();
	for (UPhysicalMaterial* PhysicalMaterial : PhysicalMaterials)
	{
		if (PhysicalMaterial)
		{
			PCollection->add(*PhysicalMaterial->GetPhysXMaterial());
		}
	}

	for (UBodySetup* BodySetup : BodySetups)
	{
		for(PxTriangleMesh* TriMesh : BodySetup->TriMeshes)
		{
			AddToCollection(PCollection, TriMesh);
		}

		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			AddToCollection(PCollection, ConvexElem.GetConvexMesh());
			AddToCollection(PCollection, ConvexElem.GetMirroredConvexMesh());
		}
	}

	PxSerialization::createSerialObjectIds(*PCollection, PxSerialObjectId(BaseId));

	return PCollection;
}

/** Util to convert PhysX error code to string */
FString ErrorCodeToString(PxErrorCode::Enum e)
{
	FString CodeString;

	switch (e)
	{
	case PxErrorCode::eNO_ERROR:
		CodeString = TEXT("eNO_ERROR");
		break;
	case PxErrorCode::eDEBUG_INFO:
		CodeString = TEXT("eDEBUG_INFO");
		break;
	case PxErrorCode::eDEBUG_WARNING:
		CodeString = TEXT("eDEBUG_WARNING");
		break;
	case PxErrorCode::eINVALID_PARAMETER:
		CodeString = TEXT("eINVALID_PARAMETER");
		break;
	case PxErrorCode::eINVALID_OPERATION:
		CodeString = TEXT("eINVALID_OPERATION");
		break;
	case PxErrorCode::eOUT_OF_MEMORY:
		CodeString = TEXT("eOUT_OF_MEMORY");
		break;
	case PxErrorCode::eINTERNAL_ERROR:
		CodeString = TEXT("eINTERNAL_ERROR");
		break;
	case PxErrorCode::eABORT:
		CodeString = TEXT("eABORT");
		break;
	case PxErrorCode::ePERF_WARNING:
		CodeString = TEXT("ePERF_WARNING");
		break;
	default:
		CodeString = TEXT("UNKONWN");		
	}

	return CodeString;
}

bool GHillClimbError = false;

void FPhysXErrorCallback::reportError(PxErrorCode::Enum e, const char* message, const char* file, int line)
{
	// if not in game, ignore Perf warnings - i.e. Moving Static actor in editor will produce this warning
	if (GIsEditor && e == PxErrorCode::ePERF_WARNING)
	{
		return;
	}


	if(e == PxErrorCode::eINTERNAL_ERROR)
	{
		const char* HillClimbError = "HillClimbing";
		const char* TestSATCapsulePoly = "testSATCapsulePoly";
		//HACK: We parse the message to see if it's hill climbing so that we can log some more useful information higher up in the callstack
		if(FPlatformString::Strstr(message, HillClimbError))
		{
			GHillClimbError = true;
		}
		
		if(FPlatformString::Strstr(message, TestSATCapsulePoly))
		{
			GHillClimbError = true;
		}
	}

	// Make string to print out, include physx file/line
	FString ErrorString = FString::Printf(TEXT("PHYSX: (%s %d) %s : %s"), ANSI_TO_TCHAR(file), line, *ErrorCodeToString(e), ANSI_TO_TCHAR(message));

	if (e == PxErrorCode::eOUT_OF_MEMORY || e == PxErrorCode::eABORT)
	{
		UE_LOG(LogPhysics, Error, TEXT("%s"), *ErrorString);
		//ensureMsgf(false, TEXT("%s"), *ErrorString);
	}
	else if (e == PxErrorCode::eINVALID_PARAMETER || e == PxErrorCode::eINVALID_OPERATION)
	{
		UE_LOG(LogPhysics, Error, TEXT("%s"), *ErrorString);
		//ensureMsgf(false, TEXT("%s"), *ErrorString);
	}
	else if (e == PxErrorCode::ePERF_WARNING || e == PxErrorCode::eINTERNAL_ERROR)
	{
		UE_LOG(LogPhysics, Warning, TEXT("%s"), *ErrorString);
	}
#if UE_BUILD_DEBUG
	else if (e == PxErrorCode::eDEBUG_WARNING)
	{
		UE_LOG(LogPhysics, Warning, TEXT("%s"), *ErrorString);
	}
#endif
	else
	{
		UE_LOG(LogPhysics, Log, TEXT("%s"), *ErrorString);
	}
}
#endif // WITH_PHYSX
