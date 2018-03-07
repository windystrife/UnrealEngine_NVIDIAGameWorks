// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/BodyInstance.h"
#include "EngineGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Volume.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "PhysicsPublic.h"
#include "Collision.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/ShapeElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "HAL/LowLevelMemTracker.h"

#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#if WITH_PHYSX
	#include "PhysXPublic.h"
	#include "Physics/PhysicsFiltering.h"
	#include "PhysicsEngine/PhysXSupport.h"
	#include "Collision/PhysXCollision.h"
	#include "Collision/CollisionConversions.h"
#endif // WITH_PHYSX

#include "PhysicsSerializer.h"

#define LOCTEXT_NAMESPACE "BodyInstance"

#include "Components/ModelComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"

DECLARE_CYCLE_STAT(TEXT("Init Body"), STAT_InitBody, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Debug"), STAT_InitBodyDebug, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Scene Interaction"), STAT_InitBodySceneInteraction, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Body Post Add to Scene"), STAT_InitBodyPostAdd, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Term Body"), STAT_TermBody, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Update Materials"), STAT_UpdatePhysMats, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Update Materials Scene Interaction"), STAT_UpdatePhysMatsSceneInteraction, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Filter Update"), STAT_UpdatePhysFilter, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Filter Update (PhysX Code)"), STAT_UpdatePhysFilterPhysX, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Init Bodies"), STAT_InitBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Bulk Body Scene Add"), STAT_BulkSceneAdd, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Static Init Bodies"), STAT_StaticInitBodies, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("UpdateBodyScale"), STAT_BodyInstanceUpdateBodyScale, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("CreatePhysicsShapesAndActors"), STAT_CreatePhysicsShapesAndActors, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("BodyInstance SetCollisionProfileName"), STAT_BodyInst_SetCollisionProfileName, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Phys SetBodyTransform"), STAT_SetBodyTransform, STATGROUP_Physics);

#if WITH_PHYSX
int32 FillInlinePxShapeArray_AssumesLocked(FInlinePxShapeArray& Array, const physx::PxRigidActor& RigidActor)
{
	const int32 NumShapes = RigidActor.getNbShapes();
	Array.AddUninitialized(NumShapes);
	RigidActor.getShapes(Array.GetData(), NumShapes);
	return NumShapes;
}

PxTransform GetPxTransform_AssumesLocked(const PxRigidActor* PRigidActor, bool bForceGlobalPose = false)
{
	if (!bForceGlobalPose)
	{
		if (const PxRigidDynamic* const PRigidDynamic = PRigidActor->is<PxRigidDynamic>())
		{
		    PxTransform Transform;
    
		    // getKinematicTarget will do checks to ensure the Body is both kinematic and
		    // that the kinematic target has been set.
		    if (PRigidDynamic->getKinematicTarget(Transform))
		    {
			    return Transform;
		    }
		}
	}

	return PRigidActor->getGlobalPose();
}

inline PxTransform GetPxTransform_AssumesLocked(const PxShape* PShape, const PxRigidActor* PRigidActor)
{
	return GetPxTransform_AssumesLocked(PRigidActor) * PShape->getLocalPose();
}
#endif

////////////////////////////////////////////////////////////////////////////
// FCollisionResponse
////////////////////////////////////////////////////////////////////////////

FCollisionResponse::FCollisionResponse()
{

}

FCollisionResponse::FCollisionResponse(ECollisionResponse DefaultResponse)
{
	SetAllChannels(DefaultResponse);
}

/** Set the response of a particular channel in the structure. */
void FCollisionResponse::SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(Channel);
	if (DefaultResponse == NewResponse)
	{
		RemoveReponseFromArray(Channel);
	}
	else
	{
		AddReponseToArray(Channel, NewResponse);
	}
#endif

	ResponseToChannels.SetResponse(Channel, NewResponse);
}

/** Set all channels to the specified response */
void FCollisionResponse::SetAllChannels(ECollisionResponse NewResponse)
{
	ResponseToChannels.SetAllChannels(NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	ResponseToChannels.ReplaceChannels(OldResponse, NewResponse);
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	UpdateArrayFromResponseContainer();
#endif
}

/** Set all channels from ChannelResponse Array **/
void FCollisionResponse::SetCollisionResponseContainer(const FCollisionResponseContainer& InResponseToChannels)
{
	ResponseToChannels = InResponseToChannels;
#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
	// this is only valid case that has to be updated
	UpdateArrayFromResponseContainer();
#endif
}

void FCollisionResponse::SetResponsesArray(const TArray<FResponseChannel>& InChannelResponses)
{
#if DO_GUARD_SLOW
	// verify if the name is overlapping, if so, ensure, do not remove in debug because it will cause inconsistent bug between debug/release
	int32 const ResponseNum = InChannelResponses.Num();
	for (int32 I=0; I<ResponseNum; ++I)
	{
		for (int32 J=I+1; J<ResponseNum; ++J)
		{
			if (InChannelResponses[I].Channel == InChannelResponses[J].Channel)
			{
				UE_LOG(LogCollision, Warning, TEXT("Collision Channel : Redundant name exists"));
			}
		}
	}
#endif

	ResponseArray = InChannelResponses;
	UpdateResponseContainerFromArray();
}

#if 1// @hack until PostLoad is disabled for CDO of BP WITH_EDITOR
bool FCollisionResponse::RemoveReponseFromArray(ECollisionChannel Channel)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	// We'd need to save name just in case that name is gone or not
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			ResponseArray.RemoveAt(Iter.GetIndex());
			return true;
		}
	}
	return false;
}

bool FCollisionResponse::AddReponseToArray(ECollisionChannel Channel, ECollisionResponse Response)
{
	// this is expensive operation, I'd love to remove names but this operation is supposed to do
	// so only allow it in editor
	// without editor, this does not have to match 
	FName ChannelName = UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(Channel);
	for (auto Iter=ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		if (ChannelName == (*Iter).Channel)
		{
			(*Iter).Response = Response;
			return true;
		}
	}

	// if not add to the list
	ResponseArray.Add(FResponseChannel(ChannelName, Response));
	return true;
}

void FCollisionResponse::UpdateArrayFromResponseContainer()
{
	ResponseArray.Empty(ARRAY_COUNT(ResponseToChannels.EnumArray));

	const FCollisionResponseContainer& DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer();
	const UCollisionProfile* CollisionProfile = UCollisionProfile::Get();

	for (int32 i = 0; i < ARRAY_COUNT(ResponseToChannels.EnumArray); i++)
	{
		// if not same as default
		if (ResponseToChannels.EnumArray[i] != DefaultResponse.EnumArray[i])
		{
			FName ChannelName = CollisionProfile->ReturnChannelNameFromContainerIndex(i);
			if (ChannelName != NAME_None)
			{
				FResponseChannel NewResponse;
				NewResponse.Channel = ChannelName;
				NewResponse.Response = (ECollisionResponse)ResponseToChannels.EnumArray[i];
				ResponseArray.Add(NewResponse);
			}
		}
	}
}

#endif // WITH_EDITOR

void FCollisionResponse::UpdateResponseContainerFromArray()
{
	ResponseToChannels = FCollisionResponseContainer::GetDefaultResponseContainer();

	for (auto Iter = ResponseArray.CreateIterator(); Iter; ++Iter)
	{
		FResponseChannel& Response = *Iter;

		int32 EnumIndex = UCollisionProfile::Get()->ReturnContainerIndexFromChannelName(Response.Channel);
		if ( EnumIndex != INDEX_NONE )
		{
			ResponseToChannels.SetResponse((ECollisionChannel)EnumIndex, Response.Response);
		}
		else
		{
			// otherwise remove
			ResponseArray.RemoveAt(Iter.GetIndex());
			--Iter;
		}
	}
}

bool FCollisionResponse::operator==(const FCollisionResponse& Other) const
{
	bool bCollisionResponseEqual = ResponseArray.Num() == Other.ResponseArray.Num();
	if(bCollisionResponseEqual)
	{
		for(int32 ResponseIdx = 0; ResponseIdx < ResponseArray.Num(); ++ResponseIdx)
		{
			for(int32 InternalIdx = 0; InternalIdx < ResponseArray.Num(); ++InternalIdx)
			{
				if(ResponseArray[ResponseIdx].Channel == Other.ResponseArray[InternalIdx].Channel)
				{
					bCollisionResponseEqual &= ResponseArray[ResponseIdx] == Other.ResponseArray[InternalIdx];
					break;
				}
			}
			
		}
	}

	return bCollisionResponseEqual;
}
////////////////////////////////////////////////////////////////////////////

#if WITH_PHYSX

template<typename AGGREGATE_FLAG_TYPE, typename FLAG_TYPE>
inline void ModifyFlag_Default(AGGREGATE_FLAG_TYPE& Flags, const FLAG_TYPE FlagToSet, const bool bValue)
{
	if (bValue)
	{
		Flags |= FlagToSet;
	}
	else
	{
		Flags.clear(FlagToSet);
	}
}

template<const PxActorFlag::Enum FlagToSet>
inline void ModifyActorFlag(physx::PxActorFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<const PxShapeFlag::Enum FlagToSet>
inline void ModifyShapeFlag(physx::PxShapeFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<const PxRigidBodyFlag::Enum FlagToSet>
inline void ModifyRigidBodyFlag(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	ModifyFlag_Default(Flags, FlagToSet, bValue);
}

template<>
inline void ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	// Objects can't be CCD and Kinematic at the same time.
	// If enabling Kinematic and CCD is on, disable it and turn on Speculative CCD instead.
	if (bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_CCD))
	{
		Flags |= PxRigidBodyFlag::eKINEMATIC;
		Flags |= PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD;
		Flags.clear(PxRigidBodyFlag::eENABLE_CCD);
	}

	// If disabling Kinematic and Speculative CCD is on, disable it and turn on CCD instead.
	else if (!bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD))
	{
		Flags |= PxRigidBodyFlag::eENABLE_CCD;
		Flags.clear(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD);
		Flags.clear(PxRigidBodyFlag::eKINEMATIC);
	}

	// No sanitization is needed.
	else
	{
		ModifyFlag_Default(Flags, PxRigidBodyFlag::eKINEMATIC, bValue);
	}
}

template<>
inline void ModifyRigidBodyFlag<PxRigidBodyFlag::eENABLE_CCD>(physx::PxRigidBodyFlags& Flags, const bool bValue)
{
	// Objects can't be CCD and Kinematic at the same time.
	// If disabling CCD and Speculative CCD is on, disable it too.
	if (!bValue && Flags.isSet(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD))
	{
		// CCD shouldn't be enabled at this point, but force disable just in case.
		Flags.clear(PxRigidBodyFlag::eENABLE_CCD);
		Flags.clear(PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD);
	}

	// If enabling CCD but Kinematic is on, enable Speculative CCD instead.
	else if (bValue && Flags.isSet(PxRigidBodyFlag::eKINEMATIC))
	{
		Flags |= PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD;
	}

	// No sanitization is needed.
	else
	{
		ModifyFlag_Default(Flags, PxRigidBodyFlag::eENABLE_CCD, bValue);
	}
}

template<const PxActorFlag::Enum FlagToSet>
inline void ModifyActorFlag_Isolated(PxActor* PActor, const bool bValue)
{
	PxActorFlags ActorFlags = PActor->getActorFlags();
	ModifyActorFlag<FlagToSet>(ActorFlags, bValue);
	PActor->setActorFlags(ActorFlags);
}

template<const PxRigidBodyFlag::Enum FlagToSet>
inline void ModifyRigidBodyFlag_Isolated(PxRigidBody* PRigidBody, const bool bValue)
{
	PxRigidBodyFlags RigidBodyFlags = PRigidBody->getRigidBodyFlags();
	ModifyRigidBodyFlag<FlagToSet>(RigidBodyFlags, bValue);
	PRigidBody->setRigidBodyFlags(RigidBodyFlags);
}

template<const PxShapeFlag::Enum FlagToSet>
inline void ModifyShapeFlag_Isolated(PxShape* PShape, const bool bValue)
{
	PxShapeFlags ShapeFlags = PShape->getFlags();
	ModifyShapeFlag<FlagToSet>(ShapeFlags, bValue);
	PShape->setFlags(ShapeFlags);
}

struct FSetShapeParams
{
	FSetShapeParams(TEnumAsByte<ECollisionEnabled::Type> InUseCollisionEnabled, const bool bInPhysicsStatic, const bool bInIsSync,
					const bool bInUseComplexAsSimple, const UPrimitiveComponent* InOwnerComponent, PxShape* InPShape, PxRigidActor* InPRigidActor):
		UseCollisionEnabled(InUseCollisionEnabled),
		bPhysicsStatic(bInPhysicsStatic),
		bIsSync(bInIsSync),
		bUseComplexAsSimple(bInUseComplexAsSimple),
		OwnerComponent(InOwnerComponent),
		PShape(InPShape),
		PRigidActor(InPRigidActor)
	{
	}

	const TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled;
	const bool bPhysicsStatic;
	const bool bIsSync;
	const bool bUseComplexAsSimple;

	const UPrimitiveComponent* OwnerComponent;
	PxShape* PShape;
	PxRigidActor* PRigidActor;

	const bool IsTriangleMesh() const
	{
		return PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH;
	}

	const bool IsOwnerAModelComponent() const
	{
		return OwnerComponent != nullptr && OwnerComponent->IsA(UModelComponent::StaticClass());
	}
};
#endif


FBodyInstance::FBodyInstance()
	: InstanceBodyIndex(INDEX_NONE)
	, InstanceBoneIndex(INDEX_NONE)
	, Scale3D(1.0f)
	, SceneIndexSync(0)
	, SceneIndexAsync(0)
#if WITH_PHYSX
	, CurrentSceneState(BodyInstanceSceneState::NotAdded)
#endif // WITH_PHYSX
	, SleepFamily(ESleepFamily::Normal)
	, DOFMode(0)
	, CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
	, CollisionProfileName(UCollisionProfile::CustomCollisionProfileName)
	, MaskFilter(0)
	, bUseCCD(false)
	, bNotifyRigidBodyCollision(false)
	, bSimulatePhysics(false)
	, bOverrideMass(false)
	, bEnableGravity(true)
	, bAutoWeld(false)
	, bStartAwake(true)
	, bGenerateWakeEvents(false)
	, bUpdateMassWhenScaleChanges(false)
	, bLockTranslation(true)
	, bLockRotation(true)
	, bLockXTranslation(false)
	, bLockYTranslation(false)
	, bLockZTranslation(false)
	, bLockXRotation(false)
	, bLockYRotation(false)
	, bLockZRotation(false)
#if WITH_PHYSX
	, bWokenExternally(false)
#endif // WITH_PHYSX
	, bUseAsyncScene(false)
	, bOverrideMaxDepenetrationVelocity(false)
	, bOverrideWalkableSlopeOnInstance(false)
	, MaxDepenetrationVelocity(0.f)
	, ExternalCollisionProfileBodySetup(nullptr)
	, MassInKgOverride(100.f)
	, LinearDamping(0.01)
	, AngularDamping(0.0)
	, CustomDOFPlaneNormal(FVector::ZeroVector)
	, COMNudge(ForceInit)
	, MassScale(1.f)
	, InertiaTensorScale(1.f)
	, ObjectType(ECC_WorldStatic)
	, DOFConstraint(nullptr)
	, WeldParent(nullptr)
	, PhysMaterialOverride(nullptr)
	, CustomSleepThresholdMultiplier(1.f)
	, StabilizationThresholdMultiplier(1.f)
	, PhysicsBlendWeight(0.f)
	, PositionSolverIterationCount(8)
#if WITH_PHYSX
	, RigidActorSync(nullptr)
	, RigidActorAsync(nullptr)
	, BodyAggregate(nullptr)
	, RigidActorSyncId(PX_SERIAL_OBJECT_ID_INVALID)
	, RigidActorAsyncId(PX_SERIAL_OBJECT_ID_INVALID)
#endif // WITH_PHYSX
	, VelocitySolverIterationCount(1)
#if WITH_PHYSX
	, InitialLinearVelocity(0.0f)
	, PhysxUserData(this)
#endif // WITH_PHYSX
// WaveWorks Start
	, ShapesVolume(0.0)
// WaveWorks End
{
	MaxAngularVelocity = UPhysicsSettings::Get()->MaxAngularVelocity;
}

FArchive& operator<<(FArchive& Ar,FBodyInstance& BodyInst)
{
	if (!Ar.IsLoading() && !Ar.IsSaving())
	{
		Ar << BodyInst.OwnerComponent;
		Ar << BodyInst.PhysMaterialOverride;
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_MAX_ANGULAR_VELOCITY_DEFAULT)
	{
		if(BodyInst.MaxAngularVelocity != 400.f)
		{
			BodyInst.bOverrideMaxAngularVelocity = true;
		}
	}

	return Ar;
}


/** Util for finding the parent bodyinstance of a specified body, using skeleton hierarchy */
FBodyInstance* FindParentBodyInstance(FName BodyName, USkeletalMeshComponent* SkelMeshComp)
{
	FName TestBoneName = BodyName;
	while(true)
	{
		TestBoneName = SkelMeshComp->GetParentBone(TestBoneName);
		// Bail out if parent bone not found
		if(TestBoneName == NAME_None)
		{
			return NULL;
		}

		// See if we have a body for the parent bone
		FBodyInstance* BI = SkelMeshComp->GetBodyInstance(TestBoneName);
		if(BI != NULL)
		{
			// We do - return it
			return BI;
		}

		// Don't repeat if we are already at the root!
		if(SkelMeshComp->GetBoneIndex(TestBoneName) == 0)
		{
			return NULL;
		}
	}

	return NULL;
}

FPhysScene* GetPhysicsScene(const FBodyInstance* BodyInstance)
{
	UPrimitiveComponent* OwnerComponentInst = BodyInstance->OwnerComponent.Get();
	return OwnerComponentInst ? OwnerComponentInst->GetWorld()->GetPhysicsScene() : nullptr;
}


#if WITH_PHYSX
//Determine that the shape is associated with this subbody (or root body)
bool FBodyInstance::IsShapeBoundToBody(const PxShape * PShape) const
{
	const FBodyInstance* BI = GetOriginalBodyInstance(PShape);
	return BI == this;
}

int32 FBodyInstance::GetAllShapes_AssumesLocked(TArray<PxShape*>& OutShapes) const
{
	int32 NumSyncShapes = 0;
	OutShapes.Empty();
	// grab shapes from sync actor
	if(RigidActorSync)
	{
		NumSyncShapes = RigidActorSync->getNbShapes();
		OutShapes.AddUninitialized(NumSyncShapes);
		RigidActorSync->getShapes(OutShapes.GetData(), NumSyncShapes);
	}

	// grab shapes from async actor
	if( RigidActorAsync != NULL && !HasSharedShapes())
	{
		const int32 NumAsyncShapes = RigidActorAsync->getNbShapes();
		OutShapes.AddUninitialized(NumAsyncShapes);
		RigidActorAsync->getShapes(OutShapes.GetData() + NumSyncShapes, NumAsyncShapes);
	}

	return NumSyncShapes;
}

#endif

void FBodyInstance::UpdateTriMeshVertices(const TArray<FVector> & NewPositions)
{
#if WITH_PHYSX
	if (BodySetup.IsValid())
	{
		ExecuteOnPhysicsReadWrite([&]
		{
			BodySetup->UpdateTriMeshVertices(NewPositions);

			//after updating the vertices we must call setGeometry again to update any shapes referencing the mesh

			TArray<PxShape *> PShapes;
			const int32 SyncShapeCount = GetAllShapes_AssumesLocked(PShapes);
			PxTriangleMeshGeometry PTriangleMeshGeometry;
			for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				if (PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
				{
					PShape->getTriangleMeshGeometry(PTriangleMeshGeometry);
					PShape->setGeometry(PTriangleMeshGeometry);
				}
			}
		});
	}
#endif
}

void FBodyInstance::UpdatePhysicalMaterials()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);
	UPhysicalMaterial* SimplePhysMat = GetSimplePhysicalMaterial();
	TArray<UPhysicalMaterial*> ComplexPhysMats = GetComplexPhysicalMaterials();

#if WITH_PHYSX
	PxMaterial* PSimpleMat = SimplePhysMat ? SimplePhysMat->GetPhysXMaterial() : nullptr;

	ExecuteOnPhysicsReadWrite([&]()
	{
		ApplyMaterialToInstanceShapes_AssumesLocked(PSimpleMat, ComplexPhysMats);
	});
#endif

}

void FBodyInstance::InvalidateCollisionProfileName()
{
	CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
	ExternalCollisionProfileBodySetup = nullptr;
}

void FBodyInstance::SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.SetResponse(Channel, NewResponse);
	CollisionResponses.SetResponse(Channel, NewResponse);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetResponseToAllChannels(ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.SetAllChannels(NewResponse);
	CollisionResponses.SetAllChannels(NewResponse);
	UpdatePhysicsFilterData();
}
	
void FBodyInstance::ReplaceResponseToChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	InvalidateCollisionProfileName();
	ResponseToChannels_DEPRECATED.ReplaceChannels(OldResponse, NewResponse);
	CollisionResponses.ReplaceChannels(OldResponse, NewResponse);
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetResponseToChannels(const FCollisionResponseContainer& NewReponses)
{
	InvalidateCollisionProfileName();
	CollisionResponses.SetCollisionResponseContainer(NewReponses);
	UpdatePhysicsFilterData();
}
	
void FBodyInstance::SetObjectType(ECollisionChannel Channel)
{
	InvalidateCollisionProfileName();
	ObjectType = Channel;
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetCollisionProfileName(FName InCollisionProfileName)
{
	SCOPE_CYCLE_COUNTER(STAT_BodyInst_SetCollisionProfileName);

	//Note that GetCollisionProfileName will use the external profile if one is set.
	//GetCollisionProfileName will be consistent with the values set by LoadProfileData.
	//This is why we can't use CollisionProfileName directly during the equality check
	if (GetCollisionProfileName() != InCollisionProfileName)
	{
		//LoadProfileData uses GetCollisionProfileName internally so we must now set the external collision data to null.
		ExternalCollisionProfileBodySetup = nullptr;
		CollisionProfileName = InCollisionProfileName;
		// now Load ProfileData
		LoadProfileData(false);
	}
	
	ExternalCollisionProfileBodySetup = nullptr;	//Even if incoming is the same as GetCollisionProfileName we turn it into "manual mode"
}

FName FBodyInstance::GetCollisionProfileName() const
{
	FName ReturnProfileName = CollisionProfileName;
	if (UBodySetup* BodySetupPtr = ExternalCollisionProfileBodySetup.Get(true))
	{
		ReturnProfileName = BodySetupPtr->DefaultInstance.CollisionProfileName;
	}
	
	return ReturnProfileName;
}


bool FBodyInstance::DoesUseCollisionProfile() const
{
	return IsValidCollisionProfileName(GetCollisionProfileName());
}

void FBodyInstance::SetMassScale(float InMassScale)
{
	MassScale = InMassScale;
	UpdateMassProperties();
}

void FBodyInstance::SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData)
{
	if (CollisionEnabled != NewType)
	{
		ECollisionEnabled::Type OldType = CollisionEnabled;
		InvalidateCollisionProfileName();
		CollisionEnabled = NewType;
		
		// Only update physics filter data if required
		if (bUpdatePhysicsFilterData)
		{
			UpdatePhysicsFilterData();
		}

		bool bWasPhysicsEnabled = CollisionEnabledHasPhysics(OldType);
		bool bIsPhysicsEnabled = CollisionEnabledHasPhysics(NewType);

		// Whenever we change physics state, call Recreate
		// This should also handle destroying the state (in case it's newly disabled).
		if (bWasPhysicsEnabled != bIsPhysicsEnabled)
		{
			if(UPrimitiveComponent* PrimComponent = OwnerComponent.Get())
			{
				PrimComponent->RecreatePhysicsState();
			}

		}

	}
}



EDOFMode::Type FBodyInstance::ResolveDOFMode(EDOFMode::Type DOFMode)
{
	EDOFMode::Type ResultDOF = DOFMode;
	if (DOFMode == EDOFMode::Default)
	{
		ESettingsDOF::Type SettingDefaultPlane = UPhysicsSettings::Get()->DefaultDegreesOfFreedom;
		if (SettingDefaultPlane == ESettingsDOF::XYPlane) ResultDOF = EDOFMode::XYPlane;
		if (SettingDefaultPlane == ESettingsDOF::XZPlane) ResultDOF = EDOFMode::XZPlane;
		if (SettingDefaultPlane == ESettingsDOF::YZPlane) ResultDOF = EDOFMode::YZPlane;
		if (SettingDefaultPlane == ESettingsDOF::Full3D) ResultDOF  = EDOFMode::SixDOF;
	}

	return ResultDOF;
}

FVector FBodyInstance::GetLockedAxis() const
{
	EDOFMode::Type MyDOFMode = ResolveDOFMode(DOFMode);

	switch (MyDOFMode)
	{
	case EDOFMode::None: return FVector::ZeroVector;
	case EDOFMode::YZPlane: return FVector(1, 0, 0);
	case EDOFMode::XZPlane: return FVector(0, 1, 0);
	case EDOFMode::XYPlane: return FVector(0, 0, 1);
	case EDOFMode::CustomPlane: return CustomDOFPlaneNormal;
	case EDOFMode::SixDOF: return FVector::ZeroVector;
	default:	check(0);	//unsupported locked axis type
	}

	return FVector::ZeroVector;
}

void FBodyInstance::UseExternalCollisionProfile(UBodySetup* InExternalCollisionProfileBodySetup)
{
	ensureAlways(InExternalCollisionProfileBodySetup);
	ExternalCollisionProfileBodySetup = InExternalCollisionProfileBodySetup;
	LoadProfileData(false);
}

void FBodyInstance::ClearExternalCollisionProfile()
{
	ExternalCollisionProfileBodySetup = nullptr;
	LoadProfileData(false);
}

void FBodyInstance::SetDOFLock(EDOFMode::Type NewAxisMode)
{
	DOFMode = NewAxisMode;

	CreateDOFLock();
}

void FBodyInstance::CreateDOFLock()
{
	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
		DOFConstraint = NULL;
	}

	const FVector LockedAxis = GetLockedAxis();
	const EDOFMode::Type DOF = ResolveDOFMode(DOFMode);

	if (IsDynamic() == false || (LockedAxis.IsNearlyZero() && DOF != EDOFMode::SixDOF))
	{
		return;
	}

	//if we're using SixDOF make sure we have at least one constraint
	if (DOF == EDOFMode::SixDOF && !bLockXTranslation && !bLockYTranslation && !bLockZTranslation && !bLockXRotation && !bLockYRotation && !bLockZRotation)
	{
		return;
	}

	DOFConstraint = FConstraintInstance::Alloc();
	{
		DOFConstraint->ProfileInstance.ConeLimit.bSoftConstraint = false;
		DOFConstraint->ProfileInstance.TwistLimit.bSoftConstraint  = false;
		DOFConstraint->ProfileInstance.LinearLimit.bSoftConstraint  = false;

		const FTransform TM = GetUnrealWorldTransform(false);
		FVector Normal = FVector(1, 0, 0);
		FVector Sec = FVector(0, 1, 0);


		if(DOF != EDOFMode::SixDOF)
		{
			DOFConstraint->SetAngularSwing1Motion((bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing2Motion((bLockRotation || DOFMode != EDOFMode::CustomPlane) ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularTwistMotion(EAngularConstraintMotion::ACM_Free);
			
			DOFConstraint->SetLinearXMotion((bLockTranslation || DOFMode != EDOFMode::CustomPlane) ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearYMotion(ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearZMotion(ELinearConstraintMotion::LCM_Free);

			Normal = LockedAxis.GetSafeNormal();
			FVector Garbage;
			Normal.FindBestAxisVectors(Garbage, Sec);
		}else
		{
			DOFConstraint->SetAngularTwistMotion(bLockXRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing2Motion(bLockYRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);
			DOFConstraint->SetAngularSwing1Motion(bLockZRotation ? EAngularConstraintMotion::ACM_Locked : EAngularConstraintMotion::ACM_Free);

			DOFConstraint->SetLinearXMotion(bLockXTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearYMotion(bLockYTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
			DOFConstraint->SetLinearZMotion(bLockZTranslation ? ELinearConstraintMotion::LCM_Locked : ELinearConstraintMotion::LCM_Free);
		}

		DOFConstraint->PriAxis1 = TM.InverseTransformVectorNoScale(Normal);
		DOFConstraint->SecAxis1 = TM.InverseTransformVectorNoScale(Sec);

		DOFConstraint->PriAxis2 = Normal;
		DOFConstraint->SecAxis2 = Sec;
		DOFConstraint->Pos2 = TM.GetLocation();

		// Create constraint instance based on DOF
		DOFConstraint->InitConstraint(this, nullptr, 1.f, OwnerComponent.Get());
	}
}

ECollisionEnabled::Type FBodyInstance::GetCollisionEnabled() const
{
	// Check actor override
	const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	if ((Owner != NULL) && !Owner->GetActorEnableCollision())
	{
		return ECollisionEnabled::NoCollision;
	}
	else
	{
		return CollisionEnabled;
	}
}

#if WITH_PHYSX
PxShape* ClonePhysXShape_AssumesLocked(PxShape* PShape)
{
	//NOTE: this code is copied directly from ExtSimpleFactory.cpp because physx does not currently have an API for copying a shape or incrementing a ref count.
	//This is hard to maintain so once they provide us with it we can get rid of this

	PxU16 PMaterialCount = PShape->getNbMaterials();

	TArray<PxMaterial*, TInlineAllocator<64>> PMaterials;

	PMaterials.AddZeroed(PMaterialCount);
	PShape->getMaterials(&PMaterials[0], PMaterialCount);

	PxShape* PNewShape = GPhysXSDK->createShape(PShape->getGeometry().any(), &PMaterials[0], PMaterialCount, false, PShape->getFlags());
	PNewShape->setLocalPose(PShape->getLocalPose());
	PNewShape->setContactOffset(PShape->getContactOffset());
	PNewShape->setRestOffset(PShape->getRestOffset());
	PNewShape->setSimulationFilterData(PShape->getSimulationFilterData());
	PNewShape->setQueryFilterData(PShape->getQueryFilterData());
	PNewShape->userData = PShape->userData;

	return PNewShape;
}


template<typename Lambda>
void ExecuteOnPxShapeWrite(FBodyInstance* BodyInstance, PxShape* PShape, Lambda Func)
{
	const bool bSharedShapes = BodyInstance->HasSharedShapes();
	if (bSharedShapes)
	{
		//we must now create a new shape because calling detach will lose our ref count (there's no way to increment it)
		//shape sharing is only done on static actors so this code should never execute outside the editor
		PxShape* PNewShape = ClonePhysXShape_AssumesLocked(PShape);
		BodyInstance->RigidActorSync->detachShape(*PShape, false);
		BodyInstance->RigidActorAsync->detachShape(*PShape, false);
		PShape = PNewShape;
	}

	Func(PShape);

	if (bSharedShapes)
	{
		BodyInstance->RigidActorSync->attachShape(*PShape);
		BodyInstance->RigidActorAsync->attachShape(*PShape);
		PShape->release();	//we must have created a new shape so release our reference to it (held by actors)
	}
}


void FBodyInstance::UpdatePhysicsShapeFilterData(uint32 ComponentID, bool bPhysicsStatic, const TEnumAsByte<ECollisionEnabled::Type> * CollisionEnabledOverride, FCollisionResponseContainer * ResponseOverride, bool * bNotifyOverride)
{
	ExecuteOnPhysicsReadWrite([&]
	{
		if (PxRigidActor* PActor = GetPxRigidActor_AssumesLocked())
		{

		// Iterate over all shapes and assign filterdata
		TArray<PxShape*> AllShapes;
		const int32 NumSyncShapes = GetAllShapes_AssumesLocked(AllShapes);

		bool bUpdateMassProperties = false;
		for (int32 ShapeIdx = 0; ShapeIdx < AllShapes.Num(); ShapeIdx++)
		{
			PxShape* PShape = AllShapes[ShapeIdx];
			const FBodyInstance* BI = GetOriginalBodyInstance(PShape);
			const bool bIsWelded = BI != this;

			const bool bUseComplexAsSimple = (BI->BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
			const bool bUseSimpleAsComplex = (BI->BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseSimpleAsComplex);

			const TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled = CollisionEnabledOverride && !bIsWelded ? *CollisionEnabledOverride : (TEnumAsByte<ECollisionEnabled::Type>)BI->GetCollisionEnabled();
			const FCollisionResponseContainer& UseResponse = ResponseOverride && !bIsWelded ? *ResponseOverride : BI->CollisionResponses.GetResponseContainer();
			bool bUseNotify = bNotifyOverride && !bIsWelded ? *bNotifyOverride : BI->bNotifyRigidBodyCollision;


			UPrimitiveComponent* OwnerPrimitiveComponent = BI->OwnerComponent.Get();
			AActor* OwnerActor = OwnerPrimitiveComponent ? OwnerPrimitiveComponent->GetOwner() : nullptr;
			int32 ActorID = (OwnerActor != nullptr) ? OwnerActor->GetUniqueID() : 0;

			// Create the filterdata structs
			PxFilterData PSimFilterData;
			PxFilterData PSimpleQueryData;
			PxFilterData PComplexQueryData;
			if (UseCollisionEnabled != ECollisionEnabled::NoCollision)
			{
				CreateShapeFilterData(BI->ObjectType, MaskFilter, ActorID, UseResponse, ComponentID, InstanceBodyIndex, PSimpleQueryData, PSimFilterData, bUseCCD && !bPhysicsStatic, bUseNotify, bPhysicsStatic);	//InstanceBodyIndex and CCD are determined by root body in case of welding
				PComplexQueryData = PSimpleQueryData;

				// Build filterdata variations for complex and simple
				PSimpleQueryData.word3 |= EPDF_SimpleCollision;
				if (bUseSimpleAsComplex)
				{
					PSimpleQueryData.word3 |= EPDF_ComplexCollision;
				}

				PComplexQueryData.word3 |= EPDF_ComplexCollision;
				if (bUseComplexAsSimple)
				{
					PComplexQueryData.word3 |= EPDF_SimpleCollision;
				}
			}

			ExecuteOnPxShapeWrite(this, PShape, [&](PxShape* PGivenShape)
			{
				FSetShapeParams ShapeData(
					/* UseCollisionEnabled */	UseCollisionEnabled,
					/* bPhysicsStatic */        bPhysicsStatic,
					/* bIsSync */               ShapeIdx < NumSyncShapes,
					/* bUseComplexAsSimple */   bUseComplexAsSimple,
					/* OwnerComponent */        OwnerPrimitiveComponent,
					/* PShape */                PGivenShape,
					/* PRigidActor */			PActor
				);

				SetShapeFlagsInternal_AssumesShapeLocked(ShapeData, bUpdateMassProperties);

				PGivenShape->setSimulationFilterData(PSimFilterData);

				// If query collision is enabled..
				if (ShapeData.UseCollisionEnabled != ECollisionEnabled::NoCollision)
				{
					// Triangle mesh is 'complex' geom
					if (ShapeData.IsTriangleMesh())
					{
						PGivenShape->setQueryFilterData(PComplexQueryData);
					}
					// Everything else is 'simple'
					else
					{
						PGivenShape->setQueryFilterData(PSimpleQueryData);
					}
				}
			});
		}

		if (bUpdateMassProperties)
		{
			UpdateMassProperties();
		}
	}
	});
}
#endif


void FBodyInstance::SetMaskFilter(FMaskFilter InMaskFilter)
{
	if (MaskFilter == InMaskFilter)
	{
		return;
	}

#if WITH_PHYSX

	ExecuteOnPhysicsReadWrite([&]
	{
		if (PxRigidActor* PActor = GetPxRigidActor_AssumesLocked())
		{

			// Iterate over all shapes and assign new mask filter
			TArray<PxShape*> AllShapes;
			const int32 NumSyncShapes = GetAllShapes_AssumesLocked(AllShapes);

			for (int32 ShapeIdx = 0; ShapeIdx < AllShapes.Num(); ShapeIdx++)
			{
				PxShape* PShape = AllShapes[ShapeIdx];
				const FBodyInstance* BI = GetOriginalBodyInstance(PShape);
				if (BI == this)	//only apply to shapes that came from our body
				{
					ExecuteOnPxShapeWrite(this, PShape, [&](PxShape* PGivenShape)
					{
						PxFilterData PQueryFilterData = PGivenShape->getQueryFilterData();
						UpdateMaskFilter(PQueryFilterData.word3, InMaskFilter);
						PGivenShape->setQueryFilterData(PQueryFilterData);

						PxFilterData PSimFilterData = PGivenShape->getSimulationFilterData();
						UpdateMaskFilter(PSimFilterData.word3, InMaskFilter);
						PGivenShape->setSimulationFilterData(PSimFilterData);

					});
				}
			}
		}
	});
#endif

	MaskFilter = InMaskFilter;

}

/** Update the filter data on the physics shapes, based on the owning component flags. */
void FBodyInstance::UpdatePhysicsFilterData()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePhysFilter);

	if(WeldParent)
	{
		WeldParent->UpdatePhysicsFilterData();
		return;
	}

	// Do nothing if no physics actor
	if (!IsValidBodyInstance())
	{
		return;
	}

	// this can happen in landscape height field collision component
	if (!BodySetup.IsValid())
	{
		return;
	}

	// Figure out if we are static
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

	// Grab collision setting from body instance
	TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled = GetCollisionEnabled(); // this checks actor override
	bool bUseNotifyRBCollision = bNotifyRigidBodyCollision;
	FCollisionResponseContainer UseResponse = CollisionResponses.GetResponseContainer();

	bool bUseCollisionEnabledOverride = false;
	bool bResponseOverride = false;
	bool bNotifyOverride = false;

	if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
	{
		// In skeletal case, collision enable/disable/movement should be overriden by mesh component
		// being in the physics asset, and not having collision is a waste and it can cause a bug where disconnected bodies
		if (Owner)
		{
			if (Owner->GetActorEnableCollision())	//we only override if actor has collision enabled
			{
				UseCollisionEnabled = SkelMeshComp->BodyInstance.CollisionEnabled;
			}
			else
			{	//actor has collision disabled so disable regardless of component setting
				UseCollisionEnabled = ECollisionEnabled::NoCollision;
			}
		}
		

		ObjectType = SkelMeshComp->GetCollisionObjectType();
		bUseCollisionEnabledOverride = true;

		if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled)
		{
			UseResponse.SetAllChannels(ECR_Block);
		}
		else if (BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
		}

		UseResponse = FCollisionResponseContainer::CreateMinContainer(UseResponse, SkelMeshComp->BodyInstance.CollisionResponses.GetResponseContainer());
		bUseNotifyRBCollision = bUseNotifyRBCollision && SkelMeshComp->BodyInstance.bNotifyRigidBodyCollision;
		bResponseOverride = true;
		bNotifyOverride = true;
	}

#if WITH_EDITOR
	// if no collision, but if world wants to enable trace collision for components, allow it
	if ((UseCollisionEnabled == ECollisionEnabled::NoCollision) && Owner && (Cast<AVolume>(Owner) == nullptr))
	{
		UWorld* World = Owner->GetWorld();
		UPrimitiveComponent* PrimComp = OwnerComponentInst;
		if (World && World->bEnableTraceCollision && 
			(Cast<UStaticMeshComponent>(PrimComp) || Cast<USkeletalMeshComponent>(PrimComp) || Cast<UBrushComponent>(PrimComp)))
		{
			//UE_LOG(LogPhysics, Warning, TEXT("Enabling collision %s : %s"), *GetNameSafe(Owner), *GetNameSafe(OwnerComponent.Get()));
			// clear all other channel just in case other people using those channels to do something
			UseResponse.SetAllChannels(ECR_Ignore);
			UseCollisionEnabled = ECollisionEnabled::QueryOnly;
			bResponseOverride = true;
			bUseCollisionEnabledOverride = true;
		}
	}
#endif

	// Get component ID
	uint32 ComponentID = OwnerComponentInst ? OwnerComponentInst->GetUniqueID() : INDEX_NONE;

#if WITH_PHYSX
	const TEnumAsByte<ECollisionEnabled::Type>* CollisionEnabledOverride = bUseCollisionEnabledOverride ? &UseCollisionEnabled : NULL;
	FCollisionResponseContainer * ResponseOverride = bResponseOverride ? &UseResponse : NULL;
	bool * bNotifyOverridePtr = bNotifyOverride ? &bUseNotifyRBCollision : NULL;
	UpdatePhysicsShapeFilterData(ComponentID, bPhysicsStatic, CollisionEnabledOverride, ResponseOverride, bNotifyOverridePtr);
#endif

}

TAutoConsoleVariable<int32> CDisableQueryOnlyActors(TEXT("p.DisableQueryOnlyActors"), 0, TEXT("If QueryOnly is used, actors are marked as simulation disabled. This is NOT compatible with origin shifting at the moment."));

#if UE_WITH_PHYSICS


template <bool bCompileStatic>
struct FInitBodiesHelper
{
	FInitBodiesHelper( TArray<FBodyInstance*>& InBodies, TArray<FTransform>& InTransforms, class UBodySetup* InBodySetup, class UPrimitiveComponent* InPrimitiveComp, class FPhysScene* InInRBScene, const FBodyInstance::FInitBodySpawnParams& InSpawnParams, FBodyInstance::PhysXAggregateType InInAggregate = NULL, UPhysicsSerializer* InPhysicsSerializer = nullptr)
	: Bodies(InBodies)
	, Transforms(InTransforms)
	, BodySetup(InBodySetup)
	, PrimitiveComp(InPrimitiveComp)
	, PhysScene(InInRBScene)
	, InAggregate(InInAggregate)
	, PhysicsSerializer(InPhysicsSerializer)
	, DebugName(TEXT(""))
	, InstanceBlendWeight(-1.f)
	, bInstanceSimulatePhysics(false)
	, bComponentAwake(false)
	, SkelMeshComp(nullptr)
	, InitialLinVel(EForceInit::ForceInitToZero)
	, SpawnParams(InSpawnParams)
	{
		//Compute all the needed constants

		PhysXName = GetDebugDebugName(PrimitiveComp, BodySetup, DebugName);

		bStatic = bCompileStatic || SpawnParams.bStaticPhysics;
		SkelMeshComp = bCompileStatic ? nullptr : Cast<USkeletalMeshComponent>(PrimitiveComp);
		if(SpawnParams.bPhysicsTypeDeterminesSimulation)
		{
			GetSimulatingAndBlendWeight(SkelMeshComp, BodySetup, InstanceBlendWeight, bInstanceSimulatePhysics);
		}
		

		const AActor* OwningActor = PrimitiveComp ? PrimitiveComp->GetOwner() : nullptr;
		InitialLinVel = GetInitialLinearVelocity(OwningActor, bComponentAwake);

#if WITH_PHYSX
		if(PhysScene)
		{
			PSyncScene = PhysScene->GetPhysXScene(PST_Sync);
			PAsyncScene = PhysScene->HasAsyncScene() ? PhysScene->GetPhysXScene(PST_Async) : nullptr;
		}
		else
		{
			PSyncScene = nullptr;
			PAsyncScene = nullptr;
		}
		
#endif

	}

	void InitBodies() 
	{
#if WITH_PHYSX
		InitBodies_PhysX();
#endif
	}

	FORCEINLINE bool IsStatic() const { return bCompileStatic || bStatic; }

	//The arguments passed into InitBodies
	TArray<FBodyInstance*>& Bodies;   
	TArray<FTransform>& Transforms;
	class UBodySetup* BodySetup;
	class UPrimitiveComponent* PrimitiveComp;
	class FPhysScene* PhysScene;
	FBodyInstance::PhysXAggregateType InAggregate;
	UPhysicsSerializer* PhysicsSerializer;

	FString DebugName;
	TSharedPtr<TArray<ANSICHAR>> PhysXName;

	//The constants shared between PhysX and Box2D
	bool bStatic;
	float InstanceBlendWeight;
	bool bInstanceSimulatePhysics;
	bool bComponentAwake;

	const USkeletalMeshComponent* SkelMeshComp;
	FVector InitialLinVel;

	const FBodyInstance::FInitBodySpawnParams& SpawnParams;

#if WITH_PHYSX

	PxScene* PSyncScene;
	PxScene* PAsyncScene;

	bool GetBinaryData_PhysX_AssumesLocked(FBodyInstance* Instance) const
	{
		Instance->RigidActorSync = PhysicsSerializer->GetRigidActor(Instance->RigidActorSyncId);
		Instance->RigidActorAsync = PhysicsSerializer->GetRigidActor(Instance->RigidActorAsyncId);

		return Instance->RigidActorSync || Instance->RigidActorAsync;
	}


	physx::PxRigidActor* CreateActor_PhysX_AssumesLocked(FBodyInstance* Instance, const PxTransform& PTransform) const
	{
		physx::PxRigidDynamic* PNewDynamic = nullptr;

		const ECollisionEnabled::Type CollisionType = Instance->GetCollisionEnabled();
		const bool bDisableSim = !CollisionEnabledHasPhysics(CollisionType) && CDisableQueryOnlyActors.GetValueOnGameThread();

		if (IsStatic())
		{
			Instance->RigidActorSync = GPhysXSDK->createRigidStatic(PTransform);

			if(bDisableSim)
			{
				ModifyActorFlag_Isolated<PxActorFlag::eDISABLE_SIMULATION>(Instance->RigidActorSync, true);
			}

			if (PAsyncScene)
			{
				Instance->RigidActorAsync = GPhysXSDK->createRigidStatic(PTransform);

				if (bDisableSim)
				{
					ModifyActorFlag_Isolated<PxActorFlag::eDISABLE_SIMULATION>(Instance->RigidActorAsync, true);
				}
			}
		}
		else
		{
			PNewDynamic = GPhysXSDK->createRigidDynamic(PTransform);
			bool bWantsAsyncScene = false;
			if(SpawnParams.DynamicActorScene == EDynamicActorScene::Default)
			{
				bWantsAsyncScene = Instance->bUseAsyncScene;
			}
			else
			{
				bWantsAsyncScene = SpawnParams.DynamicActorScene == EDynamicActorScene::UseAsyncScene;
			}
			
			if(bWantsAsyncScene && PhysScene && PhysScene->HasAsyncScene())
			{
				Instance->RigidActorAsync = PNewDynamic;
			}
			else
			{
				Instance->RigidActorSync = PNewDynamic;
			}

			if(!Instance->ShouldInstanceSimulatingPhysics())
			{
				ModifyRigidBodyFlag_Isolated<PxRigidBodyFlag::eKINEMATIC>(PNewDynamic, true);
			}

			PxActorFlags ActorFlags = PNewDynamic->getActorFlags();
			if(Instance->bGenerateWakeEvents)
			{
				ModifyActorFlag<PxActorFlag::eSEND_SLEEP_NOTIFIES>(ActorFlags, true);
			}

			if(bDisableSim)
			{
				ModifyActorFlag<PxActorFlag::eDISABLE_SIMULATION>(ActorFlags, true);
			}

			PNewDynamic->setActorFlags(ActorFlags);
		}

		return PNewDynamic;
	}

	bool CreateShapes_PhysX_AssumesLocked(FBodyInstance* Instance, physx::PxRigidActor* PNewDynamic, bool bKinematicTargetForSQ) const
	{
		UPhysicalMaterial* SimplePhysMat = Instance->GetSimplePhysicalMaterial();
		TArray<UPhysicalMaterial*> ComplexPhysMats = Instance->GetComplexPhysicalMaterials();

		PxMaterial* PSimpleMat = SimplePhysMat ? SimplePhysMat->GetPhysXMaterial() : nullptr;

		FShapeData ShapeData;
		Instance->GetFilterData_AssumesLocked(ShapeData);
		Instance->GetShapeFlags_AssumesLocked(ShapeData, ShapeData.CollisionEnabled, BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		if (!IsStatic() && PNewDynamic)
		{
			if (!Instance->ShouldInstanceSimulatingPhysics())
			{
				ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(ShapeData.SyncBodyFlags, true);
			}
			ModifyRigidBodyFlag<PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES>(ShapeData.SyncBodyFlags, bKinematicTargetForSQ);
		}

		bool bInitFail = false;

		const bool bShapeSharing = Instance->HasSharedShapes(); //If we have a static actor we can reuse the shapes between sync and async scene
		TArray<PxShape*> PSharedShapes;
		if (Instance->RigidActorSync)
		{
			BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, Instance->RigidActorSync, PST_Sync, Instance->Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, FTransform::Identity, bShapeSharing ? &PSharedShapes : nullptr, bShapeSharing);
			bInitFail |= Instance->RigidActorSync->getNbShapes() == 0;
			Instance->RigidActorSync->userData = &Instance->PhysxUserData;
			Instance->RigidActorSync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);

			check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData) == Instance);

		}

		if (Instance->RigidActorAsync)
		{
			check(PAsyncScene);
			if (bShapeSharing)
			{
				for (PxShape* PShape : PSharedShapes)
				{
					Instance->RigidActorAsync->attachShape(*PShape);
				}
			}
			else
			{
				BodySetup->AddShapesToRigidActor_AssumesLocked(Instance, Instance->RigidActorAsync, PST_Async, Instance->Scale3D, PSimpleMat, ComplexPhysMats, ShapeData);
			}

			bInitFail |= Instance->RigidActorAsync->getNbShapes() == 0;
			Instance->RigidActorAsync->userData = &Instance->PhysxUserData;
			Instance->RigidActorAsync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);

			check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData) == Instance);
		}

		return bInitFail;
	}

	bool CreateShapesAndActors_PhysX(TArray<PxActor*>& PSyncActors, TArray<PxActor*>& PAsyncActors, TArray<PxActor*>& PDynamicActors, const bool bCanDefer, bool& bDynamicsUseAsyncScene) 
	{
		SCOPE_CYCLE_COUNTER(STAT_CreatePhysicsShapesAndActors);

		const int32 NumBodies = Bodies.Num();
		PSyncActors.Reserve(NumBodies);

		if (PAsyncScene)
		{
			// Only allocate this if we can have async bodies
			PAsyncActors.Reserve(NumBodies);
		}

		// Ensure we have the AggGeom inside the body setup so we can calculate the number of shapes
		BodySetup->CreatePhysicsMeshes();

		for (int32 BodyIdx = NumBodies - 1; BodyIdx >= 0; BodyIdx--)   // iterate in reverse since list might shrink
		{
			FBodyInstance* Instance = Bodies[BodyIdx];
			const FTransform& Transform = Transforms[BodyIdx];

			FBodyInstance::ValidateTransform(Transform, DebugName, BodySetup);

			Instance->OwnerComponent = PrimitiveComp;
			Instance->BodySetup = BodySetup;
			Instance->Scale3D = Transform.GetScale3D();
			Instance->CharDebugName = PhysXName;
			Instance->bHasSharedShapes = IsStatic() && PhysScene && PhysScene->HasAsyncScene() && UPhysicsSettings::Get()->bEnableShapeSharing;
			Instance->bEnableGravity = Instance->bEnableGravity && (SkelMeshComp ? SkelMeshComp->BodyInstance.bEnableGravity : true);	//In the case of skeletal mesh component we AND bodies with the parent body

			// Handle autowelding here to avoid extra work
			if (!IsStatic() && Instance->bAutoWeld)
			{
				ECollisionEnabled::Type CollisionType = Instance->GetCollisionEnabled();
				if (CollisionType != ECollisionEnabled::QueryOnly)
				{
					if (UPrimitiveComponent * ParentPrimComponent = PrimitiveComp ? Cast<UPrimitiveComponent>(PrimitiveComp->GetAttachParent()) : NULL)
					{
						UWorld* World = PrimitiveComp->GetWorld();
						if (World && World->IsGameWorld())
						{
							//if we have a parent we will now do the weld and exit any further initialization
							if (PrimitiveComp->WeldToImplementation(ParentPrimComponent, PrimitiveComp->GetAttachSocketName(), false))	//welded new simulated body so initialization is done
							{
								return false;
							}
						}
					}
				}
			}

			// Don't process if we've already got a body
			if (Instance->GetPxRigidActor_AssumesLocked())
			{
				Instance->OwnerComponent = nullptr;
				Instance->BodySetup      = nullptr;
				Bodies.RemoveAt(BodyIdx);  // so we wont add it to the physx scene again later.
				Transforms.RemoveAt(BodyIdx);
				continue;
			}

			// Set sim parameters for bodies from skeletal mesh components
			if (!IsStatic() && SpawnParams.bPhysicsTypeDeterminesSimulation)
			{
				Instance->bSimulatePhysics = bInstanceSimulatePhysics;
				if (InstanceBlendWeight != -1.0f)
				{
					Instance->PhysicsBlendWeight = InstanceBlendWeight;
				}
			}

			Instance->PhysxUserData = FPhysxUserData(Instance);

			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());

			physx::PxRigidActor* PNewDynamic = nullptr;
			bool bFoundBinaryData = false;
			if (PhysicsSerializer)
			{
				bFoundBinaryData = GetBinaryData_PhysX_AssumesLocked(Instance);
			}

			if (!bFoundBinaryData)
			{
				PNewDynamic = CreateActor_PhysX_AssumesLocked(Instance, U2PTransform(Transform));
				const bool bInitFail = CreateShapes_PhysX_AssumesLocked(Instance, PNewDynamic, SpawnParams.bKinematicTargetsUpdateSQ);

				if (bInitFail)
				{
#if WITH_EDITOR
					//In the editor we may have ended up here because of world trace ignoring our EnableCollision. Since we can't get at the data in that function we check for it here
					if(!PrimitiveComp || PrimitiveComp->IsCollisionEnabled())
#endif
					{
						UE_LOG(LogPhysics, Log, TEXT("Init Instance %d of Primitive Component %s failed. Does it have collision data available?"), BodyIdx, *PrimitiveComp->GetReadableName());
					}

					if (Instance->RigidActorSync)
					{
						Instance->RigidActorSync->release();
						Instance->RigidActorSync = nullptr;
					}

					if (Instance->RigidActorAsync)
					{
						Instance->RigidActorAsync->release();
						Instance->RigidActorAsync = nullptr;
					}

					Instance->OwnerComponent = nullptr;
					Instance->BodySetup = nullptr;
					Instance->ExternalCollisionProfileBodySetup = nullptr;

					continue;
				}
			}

			if (Instance->RigidActorSync)
			{
				Instance->RigidActorSync->userData = &Instance->PhysxUserData;
				Instance->RigidActorSync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);
				check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorSync->userData) == Instance);
			}

			if (Instance->RigidActorAsync)
			{
				Instance->RigidActorAsync->userData = &Instance->PhysxUserData;
				Instance->RigidActorAsync->setName(Instance->CharDebugName.IsValid() ? Instance->CharDebugName->GetData() : nullptr);
				check(FPhysxUserData::Get<FBodyInstance>(Instance->RigidActorAsync->userData) == Instance);
			}

			if(PhysScene)
			{
				//handle special stuff only dynamic actors care about
				if (!IsStatic() && PNewDynamic)
				{
					// turn off gravity if desired
					if (!Instance->bEnableGravity)
					{
						ModifyActorFlag_Isolated<PxActorFlag::eDISABLE_GRAVITY>(PNewDynamic, true);
					}

					bDynamicsUseAsyncScene = Instance->UseAsyncScene(PhysScene);
				}

				if (bCanDefer)
				{
					if (!IsStatic() && PNewDynamic)
					{
						PhysScene->DeferAddActor(Instance, PNewDynamic, Instance->UseAsyncScene(PhysScene) ? PST_Async : PST_Sync);
					}
					else
					{
						if (Instance->RigidActorSync)
						{
							PhysScene->DeferAddActor(Instance, Instance->RigidActorSync, PST_Sync);
						}
						if (Instance->RigidActorAsync)
						{
							PhysScene->DeferAddActor(Instance, Instance->RigidActorAsync, PST_Async);
						}
					}
				}
				else
				{
					if (Instance->RigidActorSync)
					{
						PSyncActors.Add(Instance->RigidActorSync);
					}

					if (Instance->RigidActorAsync)
					{
						PAsyncActors.Add(Instance->RigidActorAsync);
					}

					if (PNewDynamic)
					{
						PDynamicActors.Add(PNewDynamic);
					}

					// Set the instance to added as we'll add it to the scene in a moment.
					// This makes sure if it ends up in one of the deferred queues later it 
					// functions correctly
					Instance->CurrentSceneState = BodyInstanceSceneState::Added;
				}

				Instance->SceneIndexSync = PhysScene->PhysXSceneIndex[PST_Sync];
				Instance->SceneIndexAsync = PAsyncScene ? PhysScene->PhysXSceneIndex[PST_Async] : 0;
			}
			
			Instance->InitialLinearVelocity = InitialLinVel;
			Instance->bWokenExternally = bComponentAwake;
		}

		return true;
	}

	void AddActorsToScene_PhysX_AssumesLocked(TArray<PxActor*>& PSyncActors, TArray<PxActor*>& PAsyncActors, TArray<PxActor*>& PDynamicActors, PxScene* PDyanmicScene) const
	{
		SCOPE_CYCLE_COUNTER(STAT_BulkSceneAdd);


		// Use the aggregate if it exists, has enough slots and is in the correct scene (or no scene)
		if (InAggregate && PDynamicActors.Num() > 0 && (InAggregate->getMaxNbActors() - InAggregate->getNbActors()) >= (uint32)PDynamicActors.Num())
		{
			if (InAggregate->getScene() == nullptr || InAggregate->getScene() == PDyanmicScene)
			{
				for (PxActor* Actor : PDynamicActors)
				{
					InAggregate->addActor(*Actor);
				}
			}
		}
		else
		{

			for (PxActor* Actor : PSyncActors)
			{
				PSyncScene->addActor(*Actor);
			}

			if (PAsyncScene)
			{
				for (PxActor* Actor : PAsyncActors)
				{
					PAsyncScene->addActor(*Actor);
				}
			}
		}
	}

	void InitBodies_PhysX() 
	{
		LLM_SCOPE(ELLMTag::PhysX);

		static TArray<PxActor*> PSyncActors;
		static TArray<PxActor*> PAsyncActors;
		static TArray<PxActor*> PDynamicActors;
		check(IsInGameThread());
		check(PSyncActors.Num() == 0);
		check(PAsyncActors.Num() == 0);
		check(PDynamicActors.Num() == 0);

		// Only static objects qualify for deferred addition
		const bool bCanDefer = IsStatic();
		bool bDynamicsUseAsync = false;
		if (CreateShapesAndActors_PhysX(PSyncActors, PAsyncActors, PDynamicActors, bCanDefer, bDynamicsUseAsync))
		{
			if (!bCanDefer)
			{
				const bool bAddingToSyncScene = (PSyncActors.Num() || (PDynamicActors.Num() && !bDynamicsUseAsync)) && PSyncScene;
				const bool bAddingToAsyncScene = (PAsyncActors.Num() || (PDynamicActors.Num() && bDynamicsUseAsync)) && PAsyncScene;

				SCOPED_SCENE_WRITE_LOCK(bAddingToSyncScene ? PSyncScene : nullptr);
				SCOPED_SCENE_WRITE_LOCK(bAddingToAsyncScene ? PAsyncScene : nullptr);

				if(PhysScene)
				{
					AddActorsToScene_PhysX_AssumesLocked(PSyncActors, PAsyncActors, PDynamicActors, bDynamicsUseAsync ? PAsyncScene : PSyncScene);
				}
				
				// Set up dynamic instance data
				if (!IsStatic())
				{
					SCOPE_CYCLE_COUNTER(STAT_InitBodyPostAdd);
					for (int32 BodyIdx = 0, NumBodies = Bodies.Num(); BodyIdx < NumBodies; ++BodyIdx)
					{
						FBodyInstance* Instance = Bodies[BodyIdx];
						Instance->InitDynamicProperties_AssumesLocked();
					}
				}
			}
			else if(PhysScene)
			{
				//For now we do not actually defer over multiple frames. To support this we need better automatic flushing when read locks are obtained
				PhysScene->FlushDeferredActors(PST_Sync);
				if (PhysScene->HasAsyncScene())
				{
					PhysScene->FlushDeferredActors(PST_Async);
				}
			}
		}

		PSyncActors.Reset();
		PAsyncActors.Reset();
		PDynamicActors.Reset();
	}
#endif

};

FBodyInstance::FInitBodySpawnParams::FInitBodySpawnParams(const UPrimitiveComponent* PrimComp)
{
	bStaticPhysics = PrimComp == nullptr || PrimComp->Mobility != EComponentMobility::Movable;
	DynamicActorScene = EDynamicActorScene::Default;

	if(const USkeletalMeshComponent* SKOwner = Cast<USkeletalMeshComponent>(PrimComp))
	{
		bPhysicsTypeDeterminesSimulation = true;
		bKinematicTargetsUpdateSQ = !SKOwner->bDeferMovementFromSceneQueries;
	}
	else
	{
		bPhysicsTypeDeterminesSimulation = false;
		bKinematicTargetsUpdateSQ = true;
	}
}

void FBodyInstance::InitBody(class UBodySetup* Setup, const FTransform& Transform, class UPrimitiveComponent* PrimComp, class FPhysScene* InRBScene, const FInitBodySpawnParams& SpawnParams, PhysXAggregateType InAggregate /*= NULL*/)
{
	SCOPE_CYCLE_COUNTER(STAT_InitBody);
	check(Setup);
	
	static TArray<FBodyInstance*> Bodies;
	static TArray<FTransform> Transforms;

	check(Bodies.Num() == 0);
	check(Transforms.Num() == 0);

	Bodies.Add(this);
	Transforms.Add(Transform);

	bool bIsStatic = SpawnParams.bStaticPhysics;
	if(bIsStatic)
	{
		FInitBodiesHelper<true> InitBodiesHelper(Bodies, Transforms, Setup, PrimComp, InRBScene, SpawnParams, InAggregate);
		InitBodiesHelper.InitBodies();
	}
	else
	{
		FInitBodiesHelper<false> InitBodiesHelper(Bodies, Transforms, Setup, PrimComp, InRBScene, SpawnParams, InAggregate);
		InitBodiesHelper.InitBodies();
	}

	Bodies.Reset();
	Transforms.Reset();
}

TSharedPtr<TArray<ANSICHAR>> GetDebugDebugName(const UPrimitiveComponent* PrimitiveComp, const UBodySetup* BodySetup, FString& DebugName)
{
	// Setup names
	// Make the debug name for this geometry...
	DebugName = (TEXT(""));
	TSharedPtr<TArray<ANSICHAR>> PhysXName;

#if (WITH_EDITORONLY_DATA || UE_BUILD_DEBUG || LOOKING_FOR_PERF_ISSUES) && !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && !NO_LOGGING
	if (PrimitiveComp)
	{
		DebugName += FString::Printf(TEXT("Component: '%s' "), *PrimitiveComp->GetPathName());
	}

	if (BodySetup->BoneName != NAME_None)
	{
		DebugName += FString::Printf(TEXT("Bone: '%s' "), *BodySetup->BoneName.ToString());
	}

	// Convert to char* for PhysX
	PhysXName = MakeShareable(new TArray<ANSICHAR>(StringToArray<ANSICHAR>(*DebugName, DebugName.Len() + 1)));
#endif

	return PhysXName;
}

void GetSimulatingAndBlendWeight(const USkeletalMeshComponent* SkelMeshComp, const UBodySetup* BodySetup, float& InstanceBlendWeight, bool& bInstanceSimulatePhysics)
{
	bool bEnableSim = false;
	if (SkelMeshComp)
	{
		if(CollisionEnabledHasPhysics(SkelMeshComp->BodyInstance.GetCollisionEnabled()))
		{
			if ((BodySetup->PhysicsType == PhysType_Simulated) || (BodySetup->PhysicsType == PhysType_Default))
			{
				bEnableSim = (SkelMeshComp && IsRunningDedicatedServer()) ? SkelMeshComp->bEnablePhysicsOnDedicatedServer : true;
				bEnableSim &= ((BodySetup->PhysicsType == PhysType_Simulated) || (SkelMeshComp->BodyInstance.bSimulatePhysics));	//if unfixed enable. If default look at parent
			}
		}
	}
	else
	{
		//not a skeletal mesh so don't bother with default and skeletal mesh component
		bEnableSim = BodySetup->PhysicsType == PhysType_Simulated;
	}

	if (bEnableSim)
	{
		// set simulate to true if using physics
		bInstanceSimulatePhysics = true;
		if (BodySetup->PhysicsType == PhysType_Simulated)
		{
			InstanceBlendWeight = 1.f;
		}
	}
	else
	{
		bInstanceSimulatePhysics = false;
		if (BodySetup->PhysicsType == PhysType_Simulated)
		{
			InstanceBlendWeight = 0.f;
		}
	}
}

FVector GetInitialLinearVelocity(const AActor* OwningActor, bool& bComponentAwake)
{
	FVector InitialLinVel(EForceInit::ForceInitToZero);
	if (OwningActor)
	{
		InitialLinVel = OwningActor->GetVelocity();

		if (InitialLinVel.SizeSquared() > FMath::Square(KINDA_SMALL_NUMBER))
		{
			bComponentAwake = true;
		}
	}

	return InitialLinVel;
}


#endif // UE_WITH_PHYSICS

#if WITH_PHYSX

const FBodyInstance* FBodyInstance::GetOriginalBodyInstance(const PxShape* PShape) const
{
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	const FWeldInfo* Result = BI->ShapeToBodiesMap.IsValid() ? BI->ShapeToBodiesMap->Find(PShape) : nullptr;
	return Result ? Result->ChildBI : BI;
}

const FTransform& FBodyInstance::GetRelativeBodyTransform(const physx::PxShape* PShape) const
{
	check(IsInGameThread());
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	const FWeldInfo* Result = BI->ShapeToBodiesMap.IsValid() ? BI->ShapeToBodiesMap->Find(PShape) : nullptr;
	return Result ? Result->RelativeTM : FTransform::Identity;
}

TArray<int32> FBodyInstance::AddCollisionNotifyInfo(const FBodyInstance* Body0, const FBodyInstance* Body1, const physx::PxContactPair * Pairs, uint32 NumPairs, TArray<FCollisionNotifyInfo> & PendingNotifyInfos)
{
	TArray<int32> PairNotifyMapping;
	PairNotifyMapping.Empty(NumPairs);

	TMap<const FBodyInstance *, TMap<const FBodyInstance *, int32> > BodyPairNotifyMap;
	for (uint32 PairIdx = 0; PairIdx < NumPairs; ++PairIdx)
	{
		const PxContactPair* Pair = Pairs + PairIdx;
		PairNotifyMapping.Add(-1);	//start as -1 because we can have collisions that we don't want to actually record collision
		
		// Check if either shape has been removed
		if ( !Pair->events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST) &&
			 !Pair->events.isSet(PxPairFlag::eNOTIFY_THRESHOLD_FORCE_LOST) &&
			 !Pair->flags.isSet(PxContactPairFlag::eREMOVED_SHAPE_0) &&
			 !Pair->flags.isSet(PxContactPairFlag::eREMOVED_SHAPE_1) )
		{
			// Get the two shapes that are involved in the collision
			const PxShape* Shape0 = Pair->shapes[0];
			check(Shape0);
			const PxShape* Shape1 = Pair->shapes[1];
			check(Shape1);
		
			PxU32 FilterFlags0 = Shape0->getSimulationFilterData().word3 & 0xFFFFFF;
			PxU32 FilterFlags1 = Shape1->getSimulationFilterData().word3 & 0xFFFFFF;

			const bool bBody0Notify = (FilterFlags0 & EPDF_ContactNotify) != 0;
			const bool bBody1Notify = (FilterFlags1 & EPDF_ContactNotify) != 0;
		
			if (bBody0Notify || bBody1Notify)
			{
				const FBodyInstance* SubBody0 = Body0->GetOriginalBodyInstance(Shape0);
				const FBodyInstance* SubBody1 = Body1->GetOriginalBodyInstance(Shape1);

				TMap<const FBodyInstance *, int32> & SubBodyNotifyMap = BodyPairNotifyMap.FindOrAdd(SubBody0);
				int32* NotifyInfoIndex = SubBodyNotifyMap.Find(SubBody1);

				if (NotifyInfoIndex == NULL)
				{
					FCollisionNotifyInfo * NotifyInfo = new (PendingNotifyInfos) FCollisionNotifyInfo;
					NotifyInfo->bCallEvent0 = bBody0Notify;
					NotifyInfo->Info0.SetFrom(SubBody0);
					NotifyInfo->bCallEvent1 = bBody1Notify;
					NotifyInfo->Info1.SetFrom(SubBody1);

					NotifyInfoIndex = &SubBodyNotifyMap.Add(SubBody0, PendingNotifyInfos.Num() - 1);
				}

				PairNotifyMapping[PairIdx] = *NotifyInfoIndex;
			}
		}
	}

	return PairNotifyMapping;
}

//helper function for TermBody to avoid code duplication between scenes
void TermBodyHelper(int16& SceneIndex, PxRigidActor*& PRigidActor, FBodyInstance* BodyInstance)
{
	if (SceneIndex)
	{
		PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);

		if (PScene)
		{
			// Enable scene lock
			SCOPED_SCENE_WRITE_LOCK(PScene);

			if (PRigidActor)
			{
				// Let FPhysScene know
				FPhysScene* PhysScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
				if (PhysScene)
				{
					PhysScene->TermBody_AssumesLocked(BodyInstance);
				}

				PRigidActor->release();
				PRigidActor = NULL;	//we must do this within the lock because we use it in the sub-stepping thread to determine that RigidActor is still valid
			}
		}

		SceneIndex = 0;
	}
#if WITH_APEX
	else
	{
		if (PRigidActor)
		{
			PRigidActor = NULL;
		}
	}
#endif

	checkSlow(PRigidActor == NULL);
	checkSlow(SceneIndex == 0);
}

#endif

/**
 *	Clean up the physics engine info for this instance.
 */
void FBodyInstance::TermBody()
{
	SCOPE_CYCLE_COUNTER(STAT_TermBody);

#if WITH_PHYSX
	// Release sync actor
	TermBodyHelper(SceneIndexSync, RigidActorSync, this);
	// Release async actor
	TermBodyHelper(SceneIndexAsync, RigidActorAsync, this);

	// releasing BodyAggregate, it shouldn't contain RigidActor now, because it's released above
	if(BodyAggregate)
	{
		check(!BodyAggregate->getNbActors());
		BodyAggregate->release();
		BodyAggregate = NULL;
	}
#endif

	// @TODO UE4: Release spring body here

	CurrentSceneState = BodyInstanceSceneState::NotAdded;
	BodySetup = NULL;
	OwnerComponent = NULL;
	ExternalCollisionProfileBodySetup = nullptr;

	if (DOFConstraint)
	{
		DOFConstraint->TermConstraint();
		FConstraintInstance::Free(DOFConstraint);
			DOFConstraint = NULL;
	}
	
}

bool FBodyInstance::Weld(FBodyInstance* TheirBody, const FTransform& TheirTM)
{
	check(IsInGameThread());
	check(TheirBody);
	if (TheirBody->BodySetup.IsValid() == false)	//attach actor can be called before body has been initialized. In this case just return false
	{
		return false;
	}

    if (TheirBody->WeldParent == this) // The body is already welded to this component. Do nothing.
    {
        return false;
    }

	//@TODO: BOX2D: Implement Weld

#if WITH_PHYSX
	TArray<PxShape *> PNewShapes;

	FTransform MyTM = GetUnrealWorldTransform(false);
	MyTM.SetScale3D(Scale3D);	//physx doesn't store 3d so set it here

	FTransform RelativeTM = TheirTM.GetRelativeTransform(MyTM);

	PxScene* PSyncScene = RigidActorSync ? RigidActorSync->getScene() : NULL;
	PxScene* PAsyncScene = RigidActorAsync ? RigidActorAsync->getScene() : NULL;

	ExecuteOnPhysicsReadWrite([&]
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdatePhysMats);

		TheirBody->WeldParent = this;

		UPhysicalMaterial* SimplePhysMat = TheirBody->GetSimplePhysicalMaterial();
		TArray<UPhysicalMaterial*> ComplexPhysMats = TheirBody->GetComplexPhysicalMaterials();
		PxMaterial* PSimpleMat =  SimplePhysMat ? SimplePhysMat->GetPhysXMaterial() : nullptr;

		FShapeData ShapeData;
		GetFilterData_AssumesLocked(ShapeData);
		GetShapeFlags_AssumesLocked(ShapeData, ShapeData.CollisionEnabled, BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);

		//child body gets placed into the same scenes as parent body
		if (PxRigidActor* MyBody = RigidActorSync)
		{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, MyBody, PST_Sync, Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, RelativeTM, &PNewShapes);
			if (TheirBody->bGenerateWakeEvents)
			{
				ModifyActorFlag_Isolated<PxActorFlag::eSEND_SLEEP_NOTIFIES>(MyBody, true);
			}
		}

		if (PxRigidActor* MyBody = RigidActorAsync)
		{
			TheirBody->BodySetup->AddShapesToRigidActor_AssumesLocked(this, MyBody, PST_Async, Scale3D, PSimpleMat, ComplexPhysMats, ShapeData, RelativeTM, &PNewShapes);
			if (TheirBody->bGenerateWakeEvents)
			{
				ModifyActorFlag_Isolated<PxActorFlag::eSEND_SLEEP_NOTIFIES>(MyBody, true);
			}
		}

		if(PNewShapes.Num())
		{
			if(!ShapeToBodiesMap.IsValid())
			{
				ShapeToBodiesMap = TSharedPtr<TMap<physx::PxShape*, FWeldInfo>> (new TMap<physx::PxShape*, FWeldInfo>());
			}

			for (int32 ShapeIdx = 0; ShapeIdx < PNewShapes.Num(); ++ShapeIdx)
			{
				PxShape* PShape = PNewShapes[ShapeIdx];
				ShapeToBodiesMap->Add(PShape, FWeldInfo(TheirBody, RelativeTM));
			}

			if(TheirBody->ShapeToBodiesMap.IsValid())
			{
				TSet<FBodyInstance*> Bodies;
				//If the body that is welding to us has things welded to it, make sure to weld those things to us as well
				TMap<physx::PxShape*, FWeldInfo>& TheirWeldInfo = *TheirBody->ShapeToBodiesMap.Get();
				for(auto Itr = TheirWeldInfo.CreateIterator(); Itr; ++Itr)
				{
					const FWeldInfo& WeldInfo = Itr->Value;
					if(!Bodies.Contains(WeldInfo.ChildBI))
					{
						Bodies.Add(WeldInfo.ChildBI);	//only want to weld once per body and can have multiple shapes
						const FTransform ChildWorldTM = WeldInfo.RelativeTM * TheirTM;
						Weld(WeldInfo.ChildBI, ChildWorldTM);
					}
				}

				TheirWeldInfo.Empty();	//They are no longer root so empty this
			}
		}

		PostShapeChange();

		//remove their body from scenes
		TermBodyHelper(TheirBody->SceneIndexSync, TheirBody->RigidActorSync, TheirBody);
		TermBodyHelper(TheirBody->SceneIndexAsync, TheirBody->RigidActorAsync, TheirBody);
	});
	
	TheirBody->UpdateDebugRendering();
	UpdateDebugRendering();

	return true;
#endif

	return false;
}

void FBodyInstance::UnWeld(FBodyInstance* TheirBI)
{
	check(IsInGameThread());

	//@TODO: BOX2D: Implement Weld

#if WITH_PHYSX

	bool bShapesChanged = false;

	ExecuteOnPhysicsReadWrite([&]
	{
		TArray<physx::PxShape *> PShapes;
		const int32 NumSyncShapes = GetAllShapes_AssumesLocked(PShapes);

		for (int32 ShapeIdx = 0; ShapeIdx < NumSyncShapes; ++ShapeIdx)
		{
			PxShape* PShape = PShapes[ShapeIdx];
				const FBodyInstance* BI = GetOriginalBodyInstance(PShape);
				if (TheirBI == BI)
				{
					ShapeToBodiesMap->Remove(PShape);
					RigidActorSync->detachShape(*PShape);
					bShapesChanged = true;
				}
		}

		for (int32 ShapeIdx = NumSyncShapes; ShapeIdx <PShapes.Num(); ++ShapeIdx)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			const FBodyInstance* BI = GetOriginalBodyInstance(PShape);
			if (TheirBI == BI)
			{
				ShapeToBodiesMap->Remove(PShape);
				RigidActorAsync->detachShape(*PShape);
				bShapesChanged = true;
			}
		}

	if (bShapesChanged)
	{
		PostShapeChange();
	}

		TheirBI->WeldParent = nullptr;
	});

	TheirBI->UpdateDebugRendering();
	UpdateDebugRendering();
#endif
}

void FBodyInstance::PostShapeChange()
{
	// Set the filter data on the shapes (call this after setting BodyData because it uses that pointer)
	UpdatePhysicsFilterData();

	UpdateMassProperties();
	// Update damping
	UpdateDampingProperties();
}

float AdjustForSmallThreshold(float NewVal, float OldVal)
{
	float Threshold = 0.1f;
	float Delta = NewVal - OldVal;
	if (Delta < 0 && FMath::Abs(NewVal) < Threshold)	//getting smaller and passed threshold so flip sign
	{
		return -Threshold;
	}
	else if (Delta > 0 && FMath::Abs(NewVal) < Threshold)	//getting bigger and passed small threshold so flip sign
	{
		return Threshold;
	}

	return NewVal;
}

//Non uniform scaling depends on the primitive that has the least non uniform scaling capability. So for example, a capsule's x and y axes scale are locked.
//So if a capsule exists in this body we must use locked x and y scaling for all shapes.
namespace EScaleMode
{
	enum Type
	{
		Free,
		LockedXY,
		LockedXYZ
	};
}

//computes the relative scaling vectors based on scale mode used
void ComputeScalingVectors(EScaleMode::Type ScaleMode, const FVector& InScale3D, FVector& OutScale3D, FVector& OutScale3DAbs)
{
	// Ensure no zeroes in any dimension
	FVector NewScale3D;
	NewScale3D.X = FMath::IsNearlyZero(InScale3D.X) ? KINDA_SMALL_NUMBER : InScale3D.X;
	NewScale3D.Y = FMath::IsNearlyZero(InScale3D.Y) ? KINDA_SMALL_NUMBER : InScale3D.Y;
	NewScale3D.Z = FMath::IsNearlyZero(InScale3D.Z) ? KINDA_SMALL_NUMBER : InScale3D.Z;

	const FVector NewScale3DAbs = NewScale3D.GetAbs();
	switch (ScaleMode)
	{
	case EScaleMode::Free:
	{
		OutScale3D = NewScale3D;
		break;
	}
	case EScaleMode::LockedXY:
	{
		float XYScaleAbs = FMath::Max(NewScale3DAbs.X, NewScale3DAbs.Y);
		float XYScale = FMath::Max(NewScale3D.X, NewScale3D.Y) < 0.f ? -XYScaleAbs : XYScaleAbs;	//if both xy are negative we should make the xy scale negative

		OutScale3D = NewScale3D;
		OutScale3D.X = OutScale3D.Y = XYScale;

		break;
	}
	case EScaleMode::LockedXYZ:
	{
		float UniformScaleAbs = NewScale3DAbs.GetMin();	//uniform scale uses the smallest magnitude
		float UniformScale = FMath::Max3(NewScale3D.X, NewScale3D.Y, NewScale3D.Z) < 0.f ? -UniformScaleAbs : UniformScaleAbs;	//if all three values are negative we should make uniform scale negative

		OutScale3D = FVector(UniformScale);
		break;
	}
	default:
	{
		check(false);	//invalid scale mode
	}
	}

	OutScale3DAbs = OutScale3D.GetAbs();
}

EScaleMode::Type ComputeScaleMode(const TArray<PxShape*>& PShapes)
{
	EScaleMode::Type ScaleMode = EScaleMode::Free;

	for (int32 ShapeIdx = 0; ShapeIdx < PShapes.Num(); ++ShapeIdx)
	{
		PxShape* PShape = PShapes[ShapeIdx];
		PxGeometryType::Enum GeomType = PShape->getGeometryType();

		if (GeomType == PxGeometryType::eSPHERE)
		{
			ScaleMode = EScaleMode::LockedXYZ;	//sphere is most restrictive so we can stop
			break;
		}
		else if (GeomType == PxGeometryType::eCAPSULE)
		{
			ScaleMode = EScaleMode::LockedXY;
		}
	}

	return ScaleMode;
}

void FBodyInstance::SetMassOverride(float MassInKG, bool bNewOverrideMass)
{
	bOverrideMass = bNewOverrideMass;
	MassInKgOverride = MassInKG;
}

bool FBodyInstance::UpdateBodyScale(const FVector& InScale3D, bool bForceUpdate)
{
	SCOPE_CYCLE_COUNTER(STAT_BodyInstanceUpdateBodyScale);

	if (!IsValidBodyInstance())
	{
		//UE_LOG(LogPhysics, Log, TEXT("Body hasn't been initialized. Call InitBody to initialize."));
		return false;
	}

	// if scale is already correct, and not forcing an update, do nothing
	if (Scale3D.Equals(InScale3D) && !bForceUpdate)
	{
		return false;
	}

	bool bSuccess = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ensureMsgf ( !Scale3D.ContainsNaN() && !InScale3D.ContainsNaN(), TEXT("Scale3D = (%f,%f,%f) InScale3D = (%f,%f,%f)"), Scale3D.X, Scale3D.Y, Scale3D.Z, InScale3D.X, InScale3D.Y, InScale3D.Z );
#endif

	FVector UpdatedScale3D;
#if WITH_PHYSX
	//Get all shapes
	EScaleMode::Type ScaleMode = EScaleMode::Free;
	ExecuteOnPhysicsReadWrite([&]
	{
		TArray<PxShape *> PShapes;
		GetAllShapes_AssumesLocked(PShapes);
		ScaleMode = ComputeScaleMode(PShapes);
#endif
		FVector AdjustedScale3D;
		FVector AdjustedScale3DAbs;
		ComputeScalingVectors(ScaleMode, InScale3D, AdjustedScale3D, AdjustedScale3DAbs);

		// Apply scaling
#if WITH_PHYSX
		//we need to allocate all of these here because PhysX insists on using the stack. This is wasteful, but reduces a lot of code duplication
		PxSphereGeometry PSphereGeom;
		PxBoxGeometry PBoxGeom;
		PxCapsuleGeometry PCapsuleGeom;
		PxConvexMeshGeometry PConvexGeom;
		PxTriangleMeshGeometry PTriMeshGeom;

		for (PxShape* PShape : PShapes)
		{
			bool bInvalid = false;	//we only mark invalid if actually found geom and it's invalid scale
			PxGeometry* UpdatedGeometry = NULL;
			PxTransform PLocalPose = PShape->getLocalPose();

			PxGeometryType::Enum GeomType = PShape->getGeometryType();
			FKShapeElem* ShapeElem = FPhysxUserData::Get<FKShapeElem>(PShape->userData);
			const FTransform& RelativeTM = GetRelativeBodyTransform(PShape);

			switch (GeomType)
			{
				case PxGeometryType::eSPHERE:
				{
					FKSphereElem* SphereElem = ShapeElem->GetShapeCheck<FKSphereElem>();
					ensure(ScaleMode == EScaleMode::LockedXYZ);

					PShape->getSphereGeometry(PSphereGeom);
					 
					PSphereGeom.radius = FMath::Max(SphereElem->Radius * AdjustedScale3DAbs.X, FCollisionShape::MinSphereRadius());
					PLocalPose.p = U2PVector(RelativeTM.TransformPosition(SphereElem->Center));
					PLocalPose.p *= AdjustedScale3D.X;

					if (PSphereGeom.isValid())
					{
						UpdatedGeometry = &PSphereGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case PxGeometryType::eBOX:
				{
					FKBoxElem* BoxElem = ShapeElem->GetShapeCheck<FKBoxElem>();
					PShape->getBoxGeometry(PBoxGeom);

					PBoxGeom.halfExtents.x = FMath::Max((0.5f * BoxElem->X * AdjustedScale3DAbs.X), FCollisionShape::MinBoxExtent());
					PBoxGeom.halfExtents.y = FMath::Max((0.5f * BoxElem->Y * AdjustedScale3DAbs.Y), FCollisionShape::MinBoxExtent());
					PBoxGeom.halfExtents.z = FMath::Max((0.5f * BoxElem->Z * AdjustedScale3DAbs.Z), FCollisionShape::MinBoxExtent());

					FTransform BoxTransform = BoxElem->GetTransform() * RelativeTM;
					PLocalPose = PxTransform(U2PTransform(BoxTransform));
					PLocalPose.p.x *= AdjustedScale3D.X;
					PLocalPose.p.y *= AdjustedScale3D.Y;
					PLocalPose.p.z *= AdjustedScale3D.Z;

					if (PBoxGeom.isValid())
					{
						UpdatedGeometry = &PBoxGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}
					break;
				}
				case PxGeometryType::eCAPSULE:
				{
					FKSphylElem* SphylElem = ShapeElem->GetShapeCheck<FKSphylElem>();
					ensure(ScaleMode == EScaleMode::LockedXY || ScaleMode == EScaleMode::LockedXYZ);

					float ScaleRadius = FMath::Max(AdjustedScale3DAbs.X, AdjustedScale3DAbs.Y);
					float ScaleLength = AdjustedScale3DAbs.Z;

					PShape->getCapsuleGeometry(PCapsuleGeom);

					// this is a bit confusing since radius and height is scaled
					// first apply the scale first 
					float Radius = FMath::Max(SphylElem->Radius * ScaleRadius, 0.1f);
					float Length = SphylElem->Length + SphylElem->Radius * 2.f;
					float HalfLength = Length * ScaleLength * 0.5f;
					Radius = FMath::Min(Radius, HalfLength);	//radius is capped by half length
					Radius = FMath::Max(Radius, FCollisionShape::MinCapsuleRadius()); // bounded by minimum limit.
					float HalfHeight = HalfLength - Radius;
					HalfHeight = FMath::Max(FCollisionShape::MinCapsuleAxisHalfHeight(), HalfHeight);

					PCapsuleGeom.halfHeight = HalfHeight;
					PCapsuleGeom.radius = Radius;

					PLocalPose = PxTransform(U2PVector(RelativeTM.TransformPosition(SphylElem->Center)), U2PQuat(SphylElem->Rotation.Quaternion()) * U2PSphylBasis);
					PLocalPose.p.x *= AdjustedScale3D.X;
					PLocalPose.p.y *= AdjustedScale3D.Y;
					PLocalPose.p.z *= AdjustedScale3D.Z;

					if (PCapsuleGeom.isValid())
					{
						UpdatedGeometry = &PCapsuleGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}

					break;
				}
				case PxGeometryType::eCONVEXMESH:
				{
					FKConvexElem* ConvexElem = ShapeElem->GetShapeCheck<FKConvexElem>();
					PShape->getConvexMeshGeometry(PConvexGeom);

					bool bUseNegX = CalcMeshNegScaleCompensation(AdjustedScale3D, PLocalPose);

					PConvexGeom.convexMesh = bUseNegX ? ConvexElem->GetMirroredConvexMesh() : ConvexElem->GetConvexMesh();
					PConvexGeom.scale.scale = U2PVector(AdjustedScale3DAbs);

					PxTransform PElementTransform = U2PTransform(RelativeTM);
					PLocalPose.q *= PElementTransform.q;
					PLocalPose.p = PElementTransform.p;
					PLocalPose.p.x *= AdjustedScale3D.X;
					PLocalPose.p.y *= AdjustedScale3D.Y;
					PLocalPose.p.z *= AdjustedScale3D.Z;

					if (PConvexGeom.isValid())
					{
						UpdatedGeometry = &PConvexGeom;
						bSuccess = true;
					}
					else
					{
						bInvalid = true;
					}

					break;
				}
				case PxGeometryType::eTRIANGLEMESH:
				{
					check(ShapeElem == nullptr);	//trimesh shape doesn't have userData
					PShape->getTriangleMeshGeometry(PTriMeshGeom);

					// find which trimesh elems it is
					// it would be nice to know if the order of PShapes array index is in the order of createShape
					if (BodySetup.IsValid())
					{
						for (PxTriangleMesh* TriMesh : BodySetup->TriMeshes)
						{
							// found it
							if (TriMesh == PTriMeshGeom.triangleMesh)
							{
								PTriMeshGeom.scale.scale = U2PVector(AdjustedScale3D);

								PLocalPose = U2PTransform(RelativeTM);
								PLocalPose.p.x *= AdjustedScale3D.X;
								PLocalPose.p.y *= AdjustedScale3D.Y;
								PLocalPose.p.z *= AdjustedScale3D.Z;

								if (PTriMeshGeom.isValid())
								{
									UpdatedGeometry = &PTriMeshGeom;
									bSuccess = true;
								}
								else
								{
									bInvalid = true;
								}
							}
						}
					}

					break;
				}
				case PxGeometryType::eHEIGHTFIELD:
				{
					// HeightField is only used by Landscape, which does different code path from other primitives
					break;
				}
				default:
				{
						   UE_LOG(LogPhysics, Error, TEXT("Unknown geom type."));
				}
			}// end switch

			if (UpdatedGeometry)
			{
				ExecuteOnPxShapeWrite(this, PShape, [&](PxShape* PGivenShape)
				{
					PGivenShape->setLocalPose(PLocalPose);
					PGivenShape->setGeometry(*UpdatedGeometry);
				});
				UpdatedScale3D = AdjustedScale3D;
			}
			else if (bInvalid)
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				FMessageLog("PIE").Warning()
					->AddToken(FTextToken::Create(LOCTEXT("InvalidScaleStart", "Applying invalid scale")))
					->AddToken(FTextToken::Create(AdjustedScale3D.ToCompactText()))
					->AddToken(FTextToken::Create(LOCTEXT("InvalidScaleMid", "to body")))
					->AddToken(FUObjectToken::Create(OwnerComponent.Get()));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			}
		}
	});
	
#endif

	// if success, overwrite old Scale3D, otherwise, just don't do it. It will have invalid scale next time
	if (bSuccess)
	{
		Scale3D = UpdatedScale3D;

		// update mass if required
		if (bUpdateMassWhenScaleChanges)
		{
			UpdateMassProperties();
		}
	}

	return bSuccess;
}

void FBodyInstance::UpdateInstanceSimulatePhysics()
{
	// In skeletal case, we need both our bone and skelcomponent flag to be true.
	// This might be 'and'ing us with ourself, but thats fine.
	const bool bUseSimulate = IsInstanceSimulatingPhysics();
	bool bInitialized = false;

#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		bInitialized = true;
		// If we want it fixed, and it is currently not kinematic
		const bool bNewKinematic = (bUseSimulate == false);
		{
			PxRigidBodyFlags RigidBodyFlags = PRigidDynamic->getRigidBodyFlags();
			ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(RigidBodyFlags, bNewKinematic);
			ModifyRigidBodyFlag<PxRigidBodyFlag::eENABLE_CCD>(RigidBodyFlags, bUseCCD);
			PRigidDynamic->setRigidBodyFlags(RigidBodyFlags);

			//if wake when level starts is true, calling this function automatically wakes body up
			if (bSimulatePhysics)
			{
				if(bStartAwake)
				{
					PRigidDynamic->wakeUp();
				}

				//Make sure to refresh filtering and generate contact points if we're already overlapping with an object
				//NOTE: waiting on fix from physx for refiltering on aggregates. For now we use supress to ensure this is not needed
				/*if(PxScene* PScene = PRigidDynamic->getScene())
				{
					PScene->resetFiltering(*PRigidDynamic);
				}*/
			}
		}
	});
#endif

	//In the original physx only implementation this was wrapped in a PRigidDynamic != NULL check.
	//We use bInitialized to check rigid actor has been created in either engine because if we haven't even initialized yet, we don't want to undo our settings
	if (bInitialized)
	{
		if (bUseSimulate)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}

		bSimulatePhysics = bUseSimulate;
	}
}

bool FBodyInstance::IsNonKinematic() const
{
	return bSimulatePhysics;
}

bool FBodyInstance::IsDynamic() const
{
	bool bIsDynamic = false;
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadOnly(this, [&](const PxRigidDynamic* PRigidDynamic)
	{
		bIsDynamic = true;
	});
#endif // WITH_PHYSX

	return bIsDynamic;
}

void FBodyInstance::ApplyWeldOnChildren()
{
	if(UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
	{
		TArray<FBodyInstance*> ChildrenBodies;
		TArray<FName> ChildrenLabels;
		OwnerComponentInst->GetWeldedBodies(ChildrenBodies, ChildrenLabels, /*bIncludingAutoWeld=*/true);

		for (int32 ChildIdx = 0; ChildIdx < ChildrenBodies.Num(); ++ChildIdx)
		{
			FBodyInstance* ChildBI = ChildrenBodies[ChildIdx];
			checkSlow(ChildBI);
			if (ChildBI != this)
			{
				const ECollisionEnabled::Type ChildCollision = ChildBI->GetCollisionEnabled();
				if(CollisionEnabledHasPhysics(ChildCollision))
				{
					if(UPrimitiveComponent* PrimOwnerComponent = ChildBI->OwnerComponent.Get())
					{
						Weld(ChildBI, PrimOwnerComponent->GetSocketTransform(ChildrenLabels[ChildIdx]));
					}
				}
			}
		}
	}
	
}

bool FBodyInstance::ShouldInstanceSimulatingPhysics() const
{
	return bSimulatePhysics && BodySetup.IsValid() && BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple;
}


void FBodyInstance::SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending)
{
	if (bSimulate)
	{
		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();

		// If we are enabling simulation, and we are the root body of our component (or we are welded), we detach the component 
		if (OwnerComponentInst && OwnerComponentInst->IsRegistered() && (OwnerComponentInst->GetBodyInstance() == this || OwnerComponentInst->IsWelded()))
		{
			if (OwnerComponentInst->GetAttachParent())
			{
				OwnerComponentInst->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			if (bSimulatePhysics == false)	//if we're switching from kinematic to simulated
			{
				ApplyWeldOnChildren();
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (OwnerComponentInst)
		{
			if (!IsValidBodyInstance())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysNoBody", "Trying to simulate physics on ''{0}'' but no physics body."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
			else if (!IsDynamic())
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimPhysStatic", "Trying to simulate physics on ''{0}'' but it is static."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}else if(BodySetup.IsValid() && BodySetup->GetCollisionTraceFlag() == ECollisionTraceFlag::CTF_UseComplexAsSimple)
			{
				FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimComplexAsSimple", "Trying to simulate physics on ''{0}'' but it has ComplexAsSimple collision."),
					FText::FromString(GetPathNameSafe(OwnerComponentInst))));
			}
		}
#endif
	}

	bSimulatePhysics = bSimulate;
	if ( !bMaintainPhysicsBlending )
	{
		if (bSimulatePhysics)
		{
			PhysicsBlendWeight = 1.f;
		}
		else
		{
			PhysicsBlendWeight = 0.f;
		}
	}

	UpdateInstanceSimulatePhysics();
}

bool FBodyInstance::IsValidBodyInstance() const
{
#if WITH_PHYSX
	if (PxRigidActor* PActor = GetPxRigidActor_AssumesLocked())
	{
		return true;
	}
#endif // WITH_PHYSX

	return false;
}

template <bool NeedsLock>
FTransform GetUnrealWorldTransformImp(const FBodyInstance* BodyInstance, bool bWithProjection, bool bGlobalPose)
{
	FTransform WorldTM = FTransform::Identity;
#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidActorReadOnly(BodyInstance, [&](const PxRigidActor* PActor)
	{
		PxTransform PTM = GetPxTransform_AssumesLocked(PActor, bGlobalPose);
		WorldTM = P2UTransform(PTM);

		if(bWithProjection)
		{
			BodyInstance->OnCalculateCustomProjection.ExecuteIfBound(BodyInstance, WorldTM);
		}
	});
#endif // WITH_PHYSX

	return WorldTM;
}

FTransform FBodyInstance::GetUnrealWorldTransform(bool bWithProjection /* = true*/, bool bForceGlobalPose /* = true*/) const
{
	return GetUnrealWorldTransformImp<true>(this, bWithProjection, bForceGlobalPose);
}


FTransform FBodyInstance::GetUnrealWorldTransform_AssumesLocked(bool bWithProjection /* = true*/, bool bForceGlobalPose /* = true*/) const
{
	return GetUnrealWorldTransformImp<false>(this, bWithProjection, bForceGlobalPose);
}

void FBodyInstance::SetBodyTransform(const FTransform& NewTransform, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_SetBodyTransform);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	extern bool GShouldLogOutAFrameOfSetBodyTransform;
	if (GShouldLogOutAFrameOfSetBodyTransform == true)
	{
		UE_LOG(LogPhysics, Log, TEXT("SetBodyTransform: %s"), *GetBodyDebugName());
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Catch NaNs and elegantly bail out.

	if( !ensureMsgf(!NewTransform.ContainsNaN(), TEXT("SetBodyTransform contains NaN (%s)\n%s"), (OwnerComponent.Get() ? *OwnerComponent->GetPathName() : TEXT("NONE")), *NewTransform.ToString()) )
	{
		return;
	}

#if WITH_PHYSX
	if (PxRigidActor* RigidActor = GetPxRigidActor_AssumesLocked())
	{
		const PxTransform PNewPose = U2PTransform(NewTransform);

		if (!PNewPose.isValid())
		{
			UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::SetBodyTransform: Trying to set new transform with bad data [p=(%f,%f,%f) q=(%f,%f,%f,%f)]"), PNewPose.p.x, PNewPose.p.y, PNewPose.p.z, PNewPose.q.x, PNewPose.q.y, PNewPose.q.z, PNewPose.q.w);
			return;
		}

		bool bEditorWorld = false;
#if WITH_EDITOR
		//If the body is moved in the editor we avoid setting the kinematic target. This is useful for tools that rely on the physx data being up to date in the editor (and velocities aren't important in this case)
		UPrimitiveComponent* OwnerComp = OwnerComponent.Get();
		UWorld* World = OwnerComp ? OwnerComp->GetWorld() : nullptr;
		bEditorWorld = World && World->WorldType == EWorldType::Editor;
#endif

		ExecuteOnPhysicsReadWrite([&]
		{
			// SIMULATED & KINEMATIC
			PxRigidDynamic* PRigidDynamic = GetPxRigidDynamic_AssumesLocked();
			if (!bEditorWorld && PRigidDynamic)
			{
				if (FPhysScene* PhysScene = GetPhysicsScene(this))
				{
					const bool bIsRigidBodyKinematic = IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(PRigidDynamic);
					// If kinematic and not teleporting, set kinematic target
					if (bIsRigidBodyKinematic && Teleport == ETeleportType::None)
					{
						PhysScene->SetKinematicTarget_AssumesLocked(this, NewTransform, true);
					}
					// Otherwise, set global pose
					else
					{
						if (bIsRigidBodyKinematic)  // check if kinematic  (checks the physx bit for this)
						{
							PRigidDynamic->setKinematicTarget(PNewPose);  // physx doesn't clear target on setGlobalPose, so overwrite any previous attempt to set this that wasn't yet resolved
						}

						PRigidDynamic->setGlobalPose(PNewPose);
					}
				}
			}
			// STATIC
			else
			{
				RigidActor->setGlobalPose(PNewPose);
			}
		});
	}
	else if(WeldParent)
	{
		WeldParent->SetWeldedBodyTransform(this, NewTransform);
	}
#endif  // WITH_PHYSX

}

void FBodyInstance::SetWeldedBodyTransform(FBodyInstance* TheirBody, const FTransform& NewTransform)
{
	UnWeld(TheirBody);
	Weld(TheirBody, NewTransform);
}

template <bool NeedsLock>
FVector GetUnrealWorldVelocityImp(const FBodyInstance* BodyInstance)
{
	FVector LinVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		LinVel = P2UVector(PRigidBody->getLinearVelocity());
	});
#endif // WITH_PHYSX

	return LinVel;
}

FVector FBodyInstance::GetUnrealWorldVelocity() const
{
	return GetUnrealWorldVelocityImp<true>(this);
}

FVector FBodyInstance::GetUnrealWorldVelocity_AssumesLocked() const
{
	return GetUnrealWorldVelocityImp<false>(this);
}

template <bool NeedsLock>
FVector GetUnrealWorldAngularVelocityInRadiansImp(const FBodyInstance* BodyInstance)
{
	FVector AngVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		AngVel = P2UVector(PRigidBody->getAngularVelocity());
	});
#endif // WITH_PHYSX

	return AngVel;
}

/** Note: returns angular velocity in degrees per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocityInRadians() const
{
	return GetUnrealWorldAngularVelocityInRadiansImp<true>(this);
}

/** Note: returns angular velocity in degrees per second. */
FVector FBodyInstance::GetUnrealWorldAngularVelocityInRadians_AssumesLocked() const
{
	return GetUnrealWorldAngularVelocityInRadiansImp<false>(this);
}

template <bool NeedsLock>
FVector GetUnrealWorldVelocityAtPointImp(const FBodyInstance* BodyInstance, const FVector& Point)
{
	FVector LinVel(EForceInit::ForceInitToZero);

#if WITH_PHYSX
	FPhysXSupport<NeedsLock>::ExecuteOnPxRigidBodyReadOnly(BodyInstance, [&](const PxRigidBody* PRigidBody)
	{
		PxVec3 PPoint = U2PVector(Point);
		LinVel = P2UVector(PxRigidBodyExt::getVelocityAtPos(*PRigidBody, PPoint));
	});
#endif // WITH_PHYSX

	return LinVel;
}


FVector FBodyInstance::GetUnrealWorldVelocityAtPoint(const FVector& Point) const
{
	return GetUnrealWorldVelocityAtPointImp<true>(this, Point);
}


FVector FBodyInstance::GetUnrealWorldVelocityAtPoint_AssumesLocked(const FVector& Point) const
{
	return GetUnrealWorldVelocityAtPointImp<false>(this, Point);
}

FTransform FBodyInstance::GetMassSpaceToWorldSpace() const
{
	FTransform MassSpaceToWorldSpace = FTransform::Identity;
#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(this, [this, &MassSpaceToWorldSpace](const PxRigidActor* PRigidActor)
	{
		if (const physx::PxRigidBody* PRigidBody = PRigidActor->is<PxRigidBody>())
		{
			PxTransform PLocalCOM = PRigidBody->getCMassLocalPose();
			PxTransform PMassSpaceToWorldSpace = GetPxTransform_AssumesLocked(PRigidBody) * PLocalCOM;
			MassSpaceToWorldSpace = P2UTransform(PMassSpaceToWorldSpace);
		}
		else
		{
			MassSpaceToWorldSpace = P2UTransform(GetPxTransform_AssumesLocked(PRigidActor));
		}
	});
#endif // WITH_PHYSX

	return MassSpaceToWorldSpace;
}

float FBodyInstance::GetBodyMass() const
{
	float Retval = 0.f;

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&] (const PxRigidBody* PRigidBody)
	{
		Retval = PRigidBody->getMass();
	});
#endif // WITH_PHYSX

	return Retval;
}


FVector FBodyInstance::GetBodyInertiaTensor() const
{
	FVector Retval = FVector::ZeroVector;

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&] (const PxRigidBody* PRigidBody)
	{
		Retval = P2UVector(PRigidBody->getMassSpaceInertiaTensor());
	});
#endif // WITH_PHYSX

	return Retval;
}

FBox FBodyInstance::GetBodyBounds() const
{
	FBox Bounds;

#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(this, [&](const PxRigidActor* PRigidActor)
	{
		PxBounds3 PBounds = PRigidActor->getWorldBounds();

		Bounds.Min = P2UVector(PBounds.minimum);
		Bounds.Max = P2UVector(PBounds.maximum);
	});
#endif // WITH_PHYSX

	//@TODO: BOX2D: Implement GetBodyBounds

	return Bounds;
}

void FBodyInstance::DrawCOMPosition(FPrimitiveDrawInterface* PDI, float COMRenderSize, const FColor& COMRenderColor)
{
	if (IsValidBodyInstance())
	{
		DrawWireStar(PDI, GetCOMPosition(), COMRenderSize, COMRenderColor, SDPG_World);
	}
}

/** Utility for copying properties from one BodyInstance to another. */
void FBodyInstance::CopyBodyInstancePropertiesFrom(const FBodyInstance* FromInst)
{
	// No copying of runtime instances (strictly defaults off BodySetup)
	check(FromInst);
	check(FromInst->OwnerComponent.Get() == NULL);
	check(FromInst->BodySetup.Get() == NULL);
#if WITH_PHYSX
	check(!FromInst->RigidActorSync);
	check(!FromInst->RigidActorAsync);
	check(!FromInst->BodyAggregate);
#endif //WITH_PHYSX
	check(FromInst->SceneIndexSync == 0);
	check(FromInst->SceneIndexAsync == 0);

	//check(!OwnerComponent);
#if WITH_PHYSX
	check(!RigidActorSync);
	check(!RigidActorAsync);
	check(!BodyAggregate);
#endif //WITH_PHYSX
	//check(SceneIndex == 0);

	*this = *FromInst;
}

void FBodyInstance::ExecuteOnPhysicsReadOnly(TFunctionRef<void()> Func) const
{
	//If we are doing a read operation on a static actor there is really no reason to lock both scenes since the data should be the same (as far as queries etc... go)
	//Because of this our read operations are typically on a dynamic or the sync actor

#if WITH_PHYSX
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	const int32 SceneIndex = BI->RigidActorSync ? BI->SceneIndexSync : BI->SceneIndexAsync;
	SCOPED_SCENE_READ_LOCK(GetPhysXSceneFromIndex(SceneIndex));
	Func();
#endif
}

void FBodyInstance::ExecuteOnPhysicsReadWrite(TFunctionRef<void()> Func) const
{
	//If we are doing a write operation we will need to modify both actors to keep them in sync (or it's dynamic).
	//Because of that write operations on static actors are more expensive and require both locks.

#if WITH_PHYSX
	const FBodyInstance* BI = WeldParent ? WeldParent : this;
	if(BI->RigidActorSync)
	{
		SCENE_LOCK_WRITE(GetPhysXSceneFromIndex(BI->SceneIndexSync));
	}
	
	if (BI->RigidActorAsync)
	{
		SCENE_LOCK_WRITE(GetPhysXSceneFromIndex(BI->SceneIndexAsync));
	}

	Func();

	if (BI->RigidActorSync)
	{
		SCENE_UNLOCK_WRITE(GetPhysXSceneFromIndex(BI->SceneIndexSync));
	}

	if (BI->RigidActorAsync)
	{
		SCENE_UNLOCK_WRITE(GetPhysXSceneFromIndex(BI->SceneIndexAsync));
	}
#endif
}


#if WITH_PHYSX
int32 FBodyInstance::GetSceneIndex(int32 SceneType /* = -1 */) const
{
	if(SceneType < 0)
	{
		return RigidActorSync ? SceneIndexSync : SceneIndexAsync;
	}
	else if(SceneType < PST_MAX)
	{
		return SceneType == PST_Sync ? SceneIndexSync : SceneIndexAsync;
	}

	return -1;
}

PxRigidActor* FBodyInstance::GetPxRigidActorFromScene_AssumesLocked(int32 SceneType) const
{
	// Negative scene type means to return whichever is not NULL, preferring the sync scene.
	if( SceneType < 0 )
	{
		return RigidActorSync != NULL ? RigidActorSync : RigidActorAsync;
	}
	else
	// Otherwise return the specified actor
	if( SceneType < PST_MAX )
	{
		return SceneType == PST_Sync ? RigidActorSync : RigidActorAsync;
	}
	return NULL;
}

PxRigidDynamic* FBodyInstance::GetPxRigidDynamic_AssumesLocked() const
{
	// The logic below works because dynamic actors are non-NULL in only one scene.
	// If this assumption changes, the logic needs to change.
	PxRigidActor* RigidActor = RigidActorSync ? RigidActorSync : RigidActorAsync;
	return RigidActor ? RigidActor->is<PxRigidDynamic>() : nullptr;
}

PxRigidBody* FBodyInstance::GetPxRigidBody_AssumesLocked() const
{
	// The logic below works because dynamic actors are non-NULL in only one scene.
	// If this assumption changes, the logic needs to change.
	PxRigidActor* RigidActor = RigidActorSync ? RigidActorSync : RigidActorAsync;
	return RigidActor ? RigidActor->is<PxRigidBody>() : nullptr;
}
#endif // WITH_PHYSX


const FWalkableSlopeOverride& FBodyInstance::GetWalkableSlopeOverride() const
{
	if (bOverrideWalkableSlopeOnInstance || !BodySetup.IsValid())
	{
		return WalkableSlopeOverride;
	}
	else
	{
		return BodySetup->WalkableSlopeOverride;
	}
}

void FBodyInstance::SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride)
{
	bOverrideWalkableSlopeOnInstance = true;
	WalkableSlopeOverride = NewOverride;
}


/** 
*	Changes the current PhysMaterialOverride for this body. 
*	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its 
*	surface properties like friction and the damping.
*/
void FBodyInstance::SetPhysMaterialOverride( UPhysicalMaterial* NewPhysMaterial )
{
	// Save ref to PhysicalMaterial
	PhysMaterialOverride = NewPhysMaterial;

	// Go through the chain of physical materials and update the shapes 
	UpdatePhysicalMaterials();

	// Because physical material has changed, we need to update the mass
	UpdateMassProperties();
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial() const
{
	return GetSimplePhysicalMaterial(this, OwnerComponent, BodySetup);
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TWeakObjectPtr<UBodySetup> BodySetupPtr)
{
	if(!GEngine || !GEngine->DefaultPhysMaterial)
	{
		UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::GetSimplePhysicalMaterial : GEngine not initialized! Cannot call this during native CDO construction, wrap with if(!HasAnyFlags(RF_ClassDefaultObject)) or move out of constructor, material parameters will not be correct."));

		return nullptr;
	}

	// Find the PhysicalMaterial we need to apply to the physics bodies.
	// (LOW priority) Engine Mat, Material PhysMat, BodySetup Mat, Component Override, Body Override (HIGH priority)
	
	UPhysicalMaterial* ReturnPhysMaterial = NULL;

	// BodyInstance override
	if (BodyInstance->PhysMaterialOverride != NULL)
	{
		ReturnPhysMaterial = BodyInstance->PhysMaterialOverride;
		check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
	}
	else
	{
		// Component override
		UPrimitiveComponent* OwnerPrimComponent = OwnerComp.Get();
		if (OwnerPrimComponent && OwnerPrimComponent->BodyInstance.PhysMaterialOverride != NULL)
		{
			ReturnPhysMaterial = OwnerComp->BodyInstance.PhysMaterialOverride;
			check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
		}
		else
		{
			// BodySetup
			UBodySetup* BodySetupRawPtr = BodySetupPtr.Get();
			if (BodySetupRawPtr && BodySetupRawPtr->PhysMaterial != NULL)
			{
				ReturnPhysMaterial = BodySetupPtr->PhysMaterial;
				check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
			}
			else
			{
				// See if the Material has a PhysicalMaterial
				UMeshComponent* MeshComp = Cast<UMeshComponent>(OwnerPrimComponent);
				UPhysicalMaterial* PhysMatFromMaterial = NULL;
				if (MeshComp != NULL)
				{
					UMaterialInterface* Material = MeshComp->GetMaterial(0);
					if (Material != NULL)
					{
						PhysMatFromMaterial = Material->GetPhysicalMaterial();
					}
				}

				if (PhysMatFromMaterial != NULL)
				{
					ReturnPhysMaterial = PhysMatFromMaterial;
					check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
				}
				// fallback is default physical material
				else
				{
					ReturnPhysMaterial = GEngine->DefaultPhysMaterial;
					check(!ReturnPhysMaterial || ReturnPhysMaterial->IsValidLowLevel());
				}
			}
		}
	}
	
	return ReturnPhysMaterial;
}

TArray<UPhysicalMaterial*> FBodyInstance::GetComplexPhysicalMaterials() const
{
	TArray<UPhysicalMaterial*> PhysMaterials;
	GetComplexPhysicalMaterials(PhysMaterials);
	return PhysMaterials;
}

void FBodyInstance::GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*>& PhysMaterials) const
{
	GetComplexPhysicalMaterials(this, OwnerComponent, PhysMaterials);
}


void FBodyInstance::GetComplexPhysicalMaterials(const FBodyInstance*, TWeakObjectPtr<UPrimitiveComponent> OwnerComp, TArray<UPhysicalMaterial*>& OutPhysicalMaterials)
{
	//NOTE: Binary serialization of physics objects depends on this logic being the same between runs.
	//      If you make any changes to the returned object update the GUID of the physx binary serializer (FDerivedDataPhysXBinarySerializer)

	check(GEngine->DefaultPhysMaterial != NULL);
	// See if the Material has a PhysicalMaterial
	UPrimitiveComponent* PrimComp = OwnerComp.Get();
	if (PrimComp)
	{
		const int32 NumMaterials = PrimComp->GetNumMaterials();
		OutPhysicalMaterials.SetNum(NumMaterials);

		for (int32 MatIdx = 0; MatIdx < NumMaterials; MatIdx++)
		{
			UPhysicalMaterial* PhysMat = GEngine->DefaultPhysMaterial;
			UMaterialInterface* Material = PrimComp->GetMaterial(MatIdx);
			if (Material)
			{
				PhysMat = Material->GetPhysicalMaterial();
			}

			check(PhysMat != NULL);
			OutPhysicalMaterials[MatIdx] = PhysMat;
		}
	}
}

#if WITH_PHYSX
/** Util for finding the number of 'collision sim' shapes on this PxRigidActor */
int32 GetNumSimShapes_AssumesLocked(const PxRigidBody* PRigidBody)
{
	FInlinePxShapeArray PShapes;
	const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *PRigidBody);

	int32 NumSimShapes = 0;

	for(int32 ShapeIdx=0; ShapeIdx<NumShapes; ShapeIdx++)
	{
		PxShape* PShape = PShapes[ShapeIdx];
		if(PShape->getFlags() & PxShapeFlag::eSIMULATION_SHAPE)
		{
			NumSimShapes++;
		}
	}

	return NumSimShapes;
}
#endif // WITH_PHYSX

float KgPerM3ToKgPerCm3(float KgPerM3)
{
	//1m = 100cm => 1m^3 = (100cm)^3 = 1000000cm^3
	//kg/m^3 = kg/1000000cm^3
	const float M3ToCm3Inv = 1.f / (100.f * 100.f * 100.f);
	return KgPerM3 * M3ToCm3Inv;
}

float gPerCm3ToKgPerCm3(float gPerCm3)
{
	//1000g = 1kg
	//kg/cm^3 = 1000g/cm^3 => g/cm^3 = kg/1000 cm^3
	const float gToKG = 1.f / 1000.f;
	return gPerCm3 * gToKG;
}

#if WITH_PHYSX
/** Computes and adds the mass properties (inertia, com, etc...) based on the mass settings of the body instance. */
PxMassProperties ComputeMassProperties(const FBodyInstance* OwningBodyInstance, TArray<PxShape*> Shapes, const FTransform& MassModifierTransform, float& ShapesVolume)
{
	// physical material - nothing can weigh less than hydrogen (0.09 kg/m^3)
	float DensityKGPerCubicUU = 1.0f;
	float RaiseMassToPower = 0.75f;
	if (UPhysicalMaterial* PhysMat = OwningBodyInstance->GetSimplePhysicalMaterial())
	{
		DensityKGPerCubicUU = FMath::Max(KgPerM3ToKgPerCm3(0.09f), gPerCm3ToKgPerCm3(PhysMat->Density));
		RaiseMassToPower = PhysMat->RaiseMassToPower;
	}

	
	PxMassProperties MassProps = PxRigidBodyExt::computeMassPropertiesFromShapes(Shapes.GetData(), Shapes.Num());
// WaveWorks Start
	ShapesVolume = MassProps.mass;
// WaveWorks End
	MassProps = MassProps * DensityKGPerCubicUU;

	float OldMass = MassProps.mass;
	float NewMass = 0.f;

	if (OwningBodyInstance->bOverrideMass == false)
	{
		float UsePow = FMath::Clamp<float>(RaiseMassToPower, KINDA_SMALL_NUMBER, 1.f);
		NewMass = FMath::Pow(OldMass, UsePow);

		// Apply user-defined mass scaling.
		NewMass *= FMath::Clamp<float>(OwningBodyInstance->MassScale, 0.01f, 100.0f);
	}
	else
	{
		NewMass = FMath::Max(OwningBodyInstance->GetMassOverride(), 0.001f);	//min weight of 1g
	}

	check(NewMass > 0.f);

	float MassRatio = NewMass / OldMass;
	
	PxMassProperties FinalMassProps = MassProps * MassRatio;

	FinalMassProps.centerOfMass += U2PVector(MassModifierTransform.TransformVector(OwningBodyInstance->COMNudge));
	FinalMassProps.inertiaTensor = PxMassProperties::scaleInertia(FinalMassProps.inertiaTensor, PxQuat(PxIdentity), U2PVector(OwningBodyInstance->InertiaTensorScale));

	return FinalMassProps;
}
#endif

void FBodyInstance::UpdateMassProperties()
{
	UPhysicalMaterial* PhysMat = GetSimplePhysicalMaterial();

#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&] (PxRigidBody* PRigidBody)
	{
		if(GetNumSimShapes_AssumesLocked(PRigidBody) > 0)
		{
			TArray<PxShape*> Shapes;
			const uint32 NumShapes = PRigidBody->getNbShapes();
			Shapes.AddUninitialized(NumShapes);
			PRigidBody->getShapes(Shapes.GetData(), NumShapes);

			//Ignore trimeshes
			for(int32 ShapeIdx = Shapes.Num() - 1; ShapeIdx >= 0; --ShapeIdx)
			{
				if(Shapes[ShapeIdx]->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
				{
					Shapes.RemoveAtSwap(ShapeIdx);
				}
			}

// WaveWorks Start
			float TotalVolume = 0;
// WaveWorks End
			PxMassProperties TotalMassProperties;
			if(ShapeToBodiesMap.IsValid() && ShapeToBodiesMap->Num() > 0)
			{
				struct FWeldedBatch
				{
					TArray<PxShape*> Shapes;
					FTransform RelTM;
				};

				//If we have welded children we must compute the mass properties of each individual body first and then combine them all together
				TMap<FBodyInstance*, FWeldedBatch> BodyToShapes;

				for (PxShape* Shape : Shapes)	//sort all welded children by their original bodies
				{
					if (FWeldInfo* WeldInfo = ShapeToBodiesMap->Find(Shape))
					{
						FWeldedBatch* WeldedBatch = BodyToShapes.Find(WeldInfo->ChildBI);
						if(!WeldedBatch)
						{
							WeldedBatch = &BodyToShapes.Add(WeldInfo->ChildBI);
							WeldedBatch->RelTM = WeldInfo->RelativeTM;
						}

						WeldedBatch->Shapes.Add(Shape);
					}
					else
					{
						//no weld info so shape really belongs to this body
						FWeldedBatch* WeldedBatch = BodyToShapes.Find(this);
						if (!WeldedBatch)
						{
							WeldedBatch = &BodyToShapes.Add(this);
							WeldedBatch->RelTM = FTransform::Identity;
						}

						WeldedBatch->Shapes.Add(Shape);
					}
				}

				TArray<PxMassProperties> SubMassProperties;
				TArray<PxTransform> MassTMs;
				for(auto BodyShapesItr : BodyToShapes)
				{
					const FBodyInstance* OwningBI = BodyShapesItr.Key;
					const FWeldedBatch& WeldedBatch = BodyShapesItr.Value;
					FTransform MassModifierTransform = WeldedBatch.RelTM;
					MassModifierTransform.SetScale3D(MassModifierTransform.GetScale3D() * Scale3D);	//Ensure that any scaling that is done on the component is passed into the mass frame modifiers

					PxMassProperties BodyMassProperties = ComputeMassProperties(OwningBI, WeldedBatch.Shapes, MassModifierTransform, TotalVolume);
					SubMassProperties.Add(BodyMassProperties);
					MassTMs.Add(PxTransform(PxIdentity));
				}

				TotalMassProperties = PxMassProperties::sum(SubMassProperties.GetData(), MassTMs.GetData(), SubMassProperties.Num());
			}
			else
			{
				//No children welded so just get this body's mass properties
				FTransform MassModifierTransform(FQuat::Identity, FVector(0.f, 0.f, 0.f), Scale3D);	//Ensure that any scaling that is done on the component is passed into the mass frame modifiers
				TotalMassProperties = ComputeMassProperties(this, Shapes, MassModifierTransform, TotalVolume);
			}
			
			PxQuat MassOrientation;
			const PxVec3 MassSpaceInertiaTensor = PxMassProperties::getMassSpaceInertia(TotalMassProperties.inertiaTensor, MassOrientation);

			PRigidBody->setMass(TotalMassProperties.mass);
			PRigidBody->setMassSpaceInertiaTensor(MassSpaceInertiaTensor);
// WaveWorks Start
			this->SetBodyVolume(TotalVolume);
// WaveWorks End

			PxTransform COM(TotalMassProperties.centerOfMass, MassOrientation);
			PRigidBody->setCMassLocalPose(COM);
		}
	});
#endif

	//Let anyone who cares about mass properties know they've been updated
	OnRecalculatedMassProperties.Broadcast(this);
}

void FBodyInstance::UpdateDebugRendering()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//After we update the mass properties, we should update any debug rendering
	if (UPrimitiveComponent* OwnerPrim = OwnerComponent.Get())
	{
		OwnerPrim->SendRenderDebugPhysics();
	}
#endif
}

void FBodyInstance::UpdateDampingProperties()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		PRigidDynamic->setLinearDamping(LinearDamping);
		PRigidDynamic->setAngularDamping(AngularDamping);
	});
#endif
}

bool FBodyInstance::IsInstanceAwake() const
{
	bool bIsSleeping = false;
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadOnly(this, [&](const PxRigidDynamic* PRigidDynamic)
	{
		bIsSleeping = PRigidDynamic->isSleeping();
	});
#endif

	return !bIsSleeping;
}

void FBodyInstance::WakeInstance()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		// Only do the wake if we are not kinematic and we have a scene. If a body is not in a
		// scene yet (could have been defered) then it will wake when it is added to the scene
		if (PRigidDynamic->getScene() && !IsRigidBodyKinematic_AssumesLocked(PRigidDynamic))
		{
			PRigidDynamic->wakeUp();
		}
	});
#endif
}

void FBodyInstance::PutInstanceToSleep()
{
#if WITH_PHYSX
	ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidDynamic))
		{
			PRigidDynamic->putToSleep();
		}
	});
#endif //WITH_PHYSX
}

float FBodyInstance::GetSleepThresholdMultiplier() const
{
	if (SleepFamily == ESleepFamily::Sensitive)
	{
		return 1 / 20.0f;
	}
	else if (SleepFamily == ESleepFamily::Custom)
	{
		return CustomSleepThresholdMultiplier;
	}

	return 1.f;
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PxVec3 PNewVel = U2PVector(NewVel);

		if (bAddToCurrent)
		{
			const PxVec3 POldVel = PRigidBody->getLinearVelocity();
			PNewVel += POldVel;
		}

		PRigidBody->setLinearVelocity(PNewVel);
	});
#endif // WITH_PHYSX
}

/** Note NewAngVel is in degrees per second */
void FBodyInstance::SetAngularVelocityInRadians(const FVector& NewAngVel, bool bAddToCurrent)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PxVec3 PNewAngVel = U2PVector(NewAngVel);

		if (bAddToCurrent)
		{
			const PxVec3 POldAngVel = PRigidBody->getAngularVelocity();
			PNewAngVel += POldAngVel;
		}

		PRigidBody->setAngularVelocity(PNewAngVel);
	});
#endif //WITH_PHYSX
}

float FBodyInstance::GetMaxAngularVelocityInRadians() const
{
	return bOverrideMaxAngularVelocity ? FMath::DegreesToRadians(MaxAngularVelocity) : FMath::DegreesToRadians(UPhysicsSettings::Get()->MaxAngularVelocity);
}

void FBodyInstance::SetMaxAngularVelocityInRadians(float NewMaxAngVel, bool bAddToCurrent, bool bUpdateOverrideMaxAngularVelocity)
{
#if WITH_PHYSX
	float DegNewMaxAngVel = FMath::RadiansToDegrees(NewMaxAngVel);

	const bool bIsDynamic = ExecuteOnPxRigidDynamicReadWrite(this, [&](PxRigidDynamic* PRigidDynamic)
	{
		if (bAddToCurrent)
		{
			float OldMaxAngVel = PRigidDynamic->getMaxAngularVelocity();
			NewMaxAngVel += OldMaxAngVel;

			//doing this part so our UI stays in degrees and not lose precision from the conversion
			float DegOldMaxAngVel = FMath::RadiansToDegrees(OldMaxAngVel);
			DegNewMaxAngVel += DegOldMaxAngVel;
		}

		PRigidDynamic->setMaxAngularVelocity(NewMaxAngVel);

		MaxAngularVelocity = DegNewMaxAngVel;
	});

	if(!bIsDynamic)
	{
		// Doesn't really matter since we are not dynamic, but makes sense that we update this anyway
		MaxAngularVelocity = DegNewMaxAngVel;
	}

	if(bUpdateOverrideMaxAngularVelocity)
	{
		bOverrideMaxAngularVelocity = true;
	}
	
#endif

	//@TODO: BOX2D: Implement SetMaxAngularVelocity
}

void FBodyInstance::SetMaxDepenetrationVelocity(float MaxVelocity)
{
	MaxDepenetrationVelocity = MaxVelocity;
#if WITH_PHYSX
	//0 from both instance and project settings means 
	float UseMaxVelocity = MaxVelocity == 0.f ? PX_MAX_F32 : MaxVelocity;
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		PRigidBody->setMaxDepenetrationVelocity(UseMaxVelocity);
	});
#endif

}


void FBodyInstance::AddCustomPhysics(FCalculateCustomPhysics& CalculateCustomPhysics)
{
	
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadOnly(this, [&](const PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddCustomPhysics_AssumesLocked(this, CalculateCustomPhysics);
			}
		}
	});
	
#endif // WITH_PHYSX

}

void FBodyInstance::AddForce(const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddForce_AssumesLocked(this, Force, bAllowSubstepping, bAccelChange);
			}

		}
	});
#endif // WITH_PHYSX
}

void FBodyInstance::AddForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddForceAtPosition_AssumesLocked(this, Force, Position, bAllowSubstepping, bIsLocalForce);
			}
		}
	});
	
#endif // WITH_PHYSX
}

void FBodyInstance::AddTorqueInRadians(const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddTorque_AssumesLocked(this, Torque, bAllowSubstepping, bAccelChange);
			}
		}
	});
#endif // WITH_PHYSX
}

void FBodyInstance::AddAngularImpulseInRadians(const FVector& AngularImpulse, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		// If we don't have a PxScene yet do not continue, this can happen with deferred bodies such as
		// destructible chunks that are not immediately placed in a scene
		if(PRigidBody->getScene() && !IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
			PRigidBody->addTorque(U2PVector(AngularImpulse), Mode, true);
		}
	});
	
#endif // WITH_PHYSX
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;
			PRigidBody->addForce(U2PVector(Impulse), Mode, true);
		}
	});
	
#endif // WITH_PHYSX
}

void FBodyInstance::AddImpulseAtPosition(const FVector& Impulse, const FVector& Position)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			PxForceMode::Enum Mode = PxForceMode::eIMPULSE; // does not support eVELOCITY_CHANGE
			PxRigidBodyExt::addForceAtPos(*PRigidBody, U2PVector(Impulse), U2PVector(Position), Mode, true);
		}
	});
	
#endif // WITH_PHYSX
}

void FBodyInstance::SetInstanceNotifyRBCollision(bool bNewNotifyCollision)
{
	bNotifyRigidBodyCollision = bNewNotifyCollision;
	UpdatePhysicsFilterData();
}

void FBodyInstance::SetEnableGravity(bool bInGravityEnabled)
{
	if (bEnableGravity != bInGravityEnabled)
	{
		bEnableGravity = bInGravityEnabled;

#if WITH_PHYSX
		ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
		{
			ModifyActorFlag_Isolated<PxActorFlag::eDISABLE_GRAVITY>(PRigidBody, !bEnableGravity);
		});
#endif // WITH_PHYSX

		if (bEnableGravity)
		{
			WakeInstance();
		}
	}
}


void FBodyInstance::AddRadialImpulseToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			AddRadialImpulseToPxRigidBody_AssumesLocked(*PRigidBody, Origin, Radius, Strength, Falloff, bVelChange);
		}
	});
	
#endif // WITH_PHYSX
}

void FBodyInstance::AddRadialForceToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
#if WITH_PHYSX
	ExecuteOnPxRigidBodyReadWrite(this, [&](PxRigidBody* PRigidBody)
	{
		if (!IsRigidBodyKinematic_AssumesLocked(PRigidBody))
		{
			if(FPhysScene* PhysScene = GetPhysicsScene(this))
			{
				PhysScene->AddRadialForceToBody_AssumesLocked(this, Origin, Radius, Strength, Falloff, bAccelChange, bAllowSubstepping);
			}

		}
	});
#endif // WITH_PHYSX
}

FString FBodyInstance::GetBodyDebugName() const
{
	FString DebugName;

	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	if (OwnerComponentInst != NULL)
	{
		DebugName = OwnerComponentInst->GetPathName();
		if (const UObject* StatObject = OwnerComponentInst->AdditionalStatObject())
		{
			DebugName += TEXT(" ");
			StatObject->AppendName(DebugName);
		}
	}

	if ((BodySetup != NULL) && (BodySetup->BoneName != NAME_None))
	{
		DebugName += FString(TEXT(" Bone: ")) + BodySetup->BoneName.ToString();
	}

	return DebugName;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COLLISION

bool FBodyInstance::LineTrace(struct FHitResult& OutHit, const FVector& Start, const FVector& End, bool bTraceComplex, bool bReturnPhysicalMaterial) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_LineTrace);

	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;

	bool bHitSomething = false;

	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if (DeltaMag > KINDA_SMALL_NUMBER)
	{
#if WITH_PHYSX
		ExecuteOnPhysicsReadOnly([&]
		{
			const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();
		if ((RigidBody != NULL) && (RigidBody->getNbShapes() != 0))
		{
			// Create filter data used to filter collisions, should always return eTOUCH for LineTraceComponent		
			PxHitFlags PHitFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eFACE_INDEX;

			PxRaycastHit BestHit;
			float BestHitDistance = BIG_NUMBER;

			// Get all the shapes from the actor
			FInlinePxShapeArray PShapes;
			const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *RigidBody);

			// Iterate over each shape
			for (int32 ShapeIdx = 0; ShapeIdx < NumShapes; ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				check(PShape);

				if (IsShapeBoundToBody(PShape) == false) { continue;  }

				const PxU32 HitBufferSize = 1;
				PxRaycastHit PHits[HitBufferSize];

				// Filter so we trace against the right kind of collision
				PxFilterData ShapeFilter = PShape->getQueryFilterData();
				const bool bShapeIsComplex = (ShapeFilter.word3 & EPDF_ComplexCollision) != 0;
				const bool bShapeIsSimple = (ShapeFilter.word3 & EPDF_SimpleCollision) != 0;
				if ((bTraceComplex && bShapeIsComplex) || (!bTraceComplex && bShapeIsSimple))
				{
					const int32 ArraySize = ARRAY_COUNT(PHits);		
					const PxI32 NumHits = PxGeometryQuery::raycast(U2PVector(Start), U2PVector(Delta / DeltaMag), PShape->getGeometry().any(), GetPxTransform_AssumesLocked(PShape, RigidBody), DeltaMag, PHitFlags, ArraySize, PHits);

					if (ensure(NumHits <= ArraySize))
					{
						for (int HitIndex = 0; HitIndex < NumHits; HitIndex++)
						{
							PxRaycastHit& Hit = PHits[HitIndex];
							if (Hit.distance < BestHitDistance)
							{
								BestHitDistance = PHits[HitIndex].distance;
								BestHit = PHits[HitIndex];

								// we don't get Shape information when we access via PShape, so I filled it up
								BestHit.shape = PShape;
								BestHit.actor = HasSharedShapes() ? RigidActorSync : PShape->getActor();	//for shared shapes there is no actor, but since it's shared just return the sync actor
							}
						}
					}
				}
			}

			if (BestHitDistance < BIG_NUMBER)
			{
				// we just like to make sure if the hit is made, set to test touch
				PxFilterData QueryFilter;
				QueryFilter.word2 = 0xFFFFF;

				PxTransform PStartTM(U2PVector(Start));
				const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
				ConvertQueryImpactHit(OwnerComponentInst ? OwnerComponentInst->GetWorld() : nullptr, BestHit, OutHit, DeltaMag, QueryFilter, Start, End, NULL, PStartTM, true, bReturnPhysicalMaterial);
				bHitSomething = true;
			}
		}
		});
		
#endif //WITH_PHYSX
	}

	return bHitSomething;
}

bool FBodyInstance::Sweep(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& ShapeWorldRotation, const FCollisionShape& CollisionShape, bool bTraceComplex) const
{
	if (CollisionShape.IsNearlyZero())
	{
		return LineTrace(OutHit, Start, End, bTraceComplex);
	}
	else
	{
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;

		bool bSweepHit = false;
#if WITH_PHYSX
		ExecuteOnPhysicsReadOnly([&]
		{
			const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();

            if ((RigidBody != NULL) && (RigidBody->getNbShapes() != 0) && (OwnerComponent != NULL))
            {
                FPhysXShapeAdaptor ShapeAdaptor(ShapeWorldRotation, CollisionShape);
                bSweepHit = InternalSweepPhysX(OutHit, Start, End, ShapeAdaptor, bTraceComplex, RigidBody);
            }
		});
		
#endif //WITH_PHYSX

		return bSweepHit;
	}
}

#if WITH_PHYSX
bool FBodyInstance::InternalSweepPhysX(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FPhysXShapeAdaptor& ShapeAdaptor, bool bTraceComplex, const PxRigidActor* RigidBody) const
{
	const FVector Delta = End - Start;
	const float DeltaMag = Delta.Size();
	if(DeltaMag > KINDA_SMALL_NUMBER)
	{
		PxHitFlags POutputFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE | PxHitFlag::eFACE_INDEX | PxHitFlag::eMTD;

		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
		PxTransform PStartTM(U2PVector(Start), ShapeAdaptor.GetGeomOrientation());
		PxTransform PCompTM(U2PTransform(OwnerComponentInst->GetComponentTransform()));

		PxVec3 PDir = U2PVector(Delta/DeltaMag);

		PxSweepHit PHit;

		// Get all the shapes from the actor
		FInlinePxShapeArray PShapes;
		const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *RigidBody);

		// Iterate over each shape
		for(int32 ShapeIdx=0; ShapeIdx<NumShapes; ShapeIdx++)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			check(PShape);

			if (IsShapeBoundToBody(PShape) == false){ continue; }

			// Filter so we trace against the right kind of collision
			PxFilterData ShapeFilter = PShape->getQueryFilterData();
			const bool bShapeIsComplex = (ShapeFilter.word3 & EPDF_ComplexCollision) != 0;
			const bool bShapeIsSimple = (ShapeFilter.word3 & EPDF_SimpleCollision) != 0;
			if((bTraceComplex && bShapeIsComplex) || (!bTraceComplex && bShapeIsSimple))
			{
				PxTransform PGlobalPose = PCompTM.transform(PShape->getLocalPose());
                const PxGeometry& Geometry = ShapeAdaptor.GetGeometry();
				if(PxGeometryQuery::sweep(PDir, DeltaMag, Geometry, PStartTM, PShape->getGeometry().any(), PGlobalPose, PHit, POutputFlags))
				{
					// we just like to make sure if the hit is made
					PxFilterData QueryFilter;
					QueryFilter.word2 = 0xFFFFF;

					// we don't get Shape information when we access via PShape, so I filled it up
					PHit.shape = PShape;
					PHit.actor = HasSharedShapes() ? RigidActorSync : PShape->getActor();	//in the case of shared shapes getActor will return null. Since the shape is shared we just return the sync actor
					PxTransform PStartTransform(U2PVector(Start));
					PHit.faceIndex = FindFaceIndex(PHit, PDir);
					ConvertQueryImpactHit(OwnerComponentInst->GetWorld(), PHit, OutHit, DeltaMag, QueryFilter, Start, End, NULL, PStartTransform, false, false);
					return true;
				}
			}
		}
	}
	return false;
}
#endif //WITH_PHYSX

bool FBodyInstance::GetSquaredDistanceToBody(const FVector& Point, float& OutDistanceSquared, FVector& OutPointOnBody) const
{
	OutPointOnBody = Point;
	float ReturnDistance = -1.f;
	float MinDistanceSqr = BIG_NUMBER;
	bool bFoundValidBody = false;
	bool bEarlyOut = true;

	const FBodyInstance* UseBI = WeldParent ? WeldParent : this;

#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(UseBI, [&](const PxRigidActor* RigidActor)
	{
		if (RigidActor->getNbShapes() == 0 || UseBI->OwnerComponent == NULL)
		{
			return;
		}

		bEarlyOut = false;

		// Get all the shapes from the actor
		FInlinePxShapeArray PShapes;
		const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *RigidActor);

		const PxVec3 PPoint = U2PVector(Point);

		// Iterate over each shape
		for (int32 ShapeIdx = 0; ShapeIdx < NumShapes; ShapeIdx++)
		{
			PxShape* PShape = PShapes[ShapeIdx];
			check(PShape);

			if (IsShapeBoundToBody(PShape) == false)	//skip welded shapes that do not belong to us
			{
				continue;
			}

			PxGeometryHolder Holder = PShape->getGeometry();	// getGeometry() result is stored on the stack, if we don't hold on to it it may be gone next statement.
			PxGeometry& PGeom = Holder.any();

			PxTransform PGlobalPose = GetPxTransform_AssumesLocked(PShape, RigidActor);
			PxGeometryType::Enum GeomType = PShape->getGeometryType();

			if (GeomType == PxGeometryType::eTRIANGLEMESH)
			{
				// Type unsupported for this function, but some other shapes will probably work. 
				continue;
			}
			bFoundValidBody = true;

			PxVec3 PClosestPoint;
			float SqrDistance = PxGeometryQuery::pointDistance(PPoint, PGeom, PGlobalPose, &PClosestPoint);
			// distance has valid data and smaller than mindistance
				if (SqrDistance > 0.f && MinDistanceSqr > SqrDistance)
			{
				MinDistanceSqr = SqrDistance;
				OutPointOnBody = P2UVector(PClosestPoint);
			}
				else if (SqrDistance == 0.f)
			{
				MinDistanceSqr = 0.f;
				break;
			}
		}	
	});
#endif //WITH_PHYSX

	if (!bFoundValidBody && !bEarlyOut)
	{
		UE_LOG(LogPhysics, Warning, TEXT("GetDistanceToBody: Component (%s) has no simple collision and cannot be queried for closest point."), OwnerComponent.Get() ? *OwnerComponent->GetPathName() : TEXT("NONE"));
	}

	if (bFoundValidBody)
	{
		OutDistanceSquared = MinDistanceSqr;
	}
	return bFoundValidBody;

	//@TODO: BOX2D: Implement DistanceToBody
}

float FBodyInstance::GetDistanceToBody(const FVector& Point, FVector& OutPointOnBody) const
{
	float DistanceSqr = -1.f;
	return (GetSquaredDistanceToBody(Point, DistanceSqr, OutPointOnBody) ? FMath::Sqrt(DistanceSqr) : -1.f);
}

template <typename AllocatorType>
bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*, AllocatorType>& Bodies) const
{
	bool bHaveOverlap = false;
#if WITH_PHYSX

	ExecuteOnPxRigidActorReadOnly(this, [&] (const PxRigidActor* TargetRigidActor)
	{
		// calculate the test global pose of the rigid body
		PxTransform PTestGlobalPose = U2PTransform(FTransform(Rot, Pos));

		// Get all the shapes from the actor
		FInlinePxShapeArray PTargetShapes;
		const int32 NumTargetShapes = FillInlinePxShapeArray_AssumesLocked(PTargetShapes, *TargetRigidActor);

		for (int32 TargetShapeIdx = 0; TargetShapeIdx < NumTargetShapes; ++TargetShapeIdx)
		{
			const PxShape * PTargetShape = PTargetShapes[TargetShapeIdx];
			const PxGeometryType::Enum ShapeType = PTargetShape->getGeometryType();
			if(ShapeType == PxGeometryType::eHEIGHTFIELD || ShapeType == PxGeometryType::eTRIANGLEMESH)
			{
				continue;	//we skip complex shapes - should this respect ComplexAsSimple?
			}

			// Calc shape global pose
			PxTransform PShapeGlobalPose = PTestGlobalPose.transform(PTargetShape->getLocalPose());
			for (const FBodyInstance* BodyInstance : Bodies)
			{
				bHaveOverlap = BodyInstance->OverlapPhysX_AssumesLocked(PTargetShape->getGeometry().any(), PShapeGlobalPose);
				if (bHaveOverlap)
				{
					return;
				}
			}
		}
	});
#endif //WITH_PHYSX
	return bHaveOverlap;
}

// Explicit template instantiation for the above.
template bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*>& Bodies) const;
template bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Pos, const FQuat& Rot, const TArray<FBodyInstance*, TInlineAllocator<1>>& Bodies) const;


bool FBodyInstance::OverlapTest(const FVector& Position, const FQuat& Rotation, const struct FCollisionShape& CollisionShape, FMTDResult* OutMTD) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_OverlapTest);

	bool bHasOverlap = false;

#if WITH_PHYSX
	ExecuteOnPxRigidActorReadOnly(this, [&] (const PxRigidActor* )
	{
	FPhysXShapeAdaptor ShapeAdaptor(Rotation, CollisionShape);
		bHasOverlap = OverlapPhysX_AssumesLocked(ShapeAdaptor.GetGeometry(), ShapeAdaptor.GetGeomPose(Position), OutMTD);
	});
#endif

	return bHasOverlap;
}

FTransform RootSpaceToWeldedSpace(const FBodyInstance* BI, const FTransform& RootTM)
{
	if (BI->WeldParent)
	{
		UPrimitiveComponent* BIOwnerComponentInst = BI->OwnerComponent.Get();
		if (BIOwnerComponentInst)
		{
			FTransform RootToWelded = BIOwnerComponentInst->GetRelativeTransform().Inverse();
			RootToWelded.ScaleTranslation(BI->Scale3D);

			return RootToWelded * RootTM;
		}
	}

	return RootTM;
}

bool FBodyInstance::OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams) const
{
	SCOPE_CYCLE_COUNTER(STAT_Collision_SceneQueryTotal);
	SCOPE_CYCLE_COUNTER(STAT_Collision_FBodyInstance_OverlapMulti);

	if ( !IsValidBodyInstance()  && (!WeldParent || !WeldParent->IsValidBodyInstance()))
	{
		UE_LOG(LogCollision, Log, TEXT("FBodyInstance::OverlapMulti : (%s) No physics data"), *GetBodyDebugName());
		return false;
	}

	bool bHaveBlockingHit = false;

	// Determine how to convert the local space of this body instance to the test space
	const FTransform ComponentSpaceToTestSpace(Quat, Pos);

	FTransform BodyInstanceSpaceToTestSpace(NoInit);
	if (pWorldToComponent)
	{
		const FTransform RootTM = WeldParent ? WeldParent->GetUnrealWorldTransform() : GetUnrealWorldTransform();
		const FTransform LocalOffset = (*pWorldToComponent) * RootTM;
		BodyInstanceSpaceToTestSpace = ComponentSpaceToTestSpace * LocalOffset;
	}
	else
	{
		BodyInstanceSpaceToTestSpace = ComponentSpaceToTestSpace;
	}

	//We want to test using global position. However, the global position of the body will be in terms of the root body which we are welded to. So we must undo the relative transform so that our shapes are centered
	//Global = Parent * Relative => Global * RelativeInverse = Parent
	if (WeldParent)
	{
		BodyInstanceSpaceToTestSpace = RootSpaceToWeldedSpace(this, BodyInstanceSpaceToTestSpace);
	}

#if WITH_PHYSX
	PxRigidActor* PRigidActor = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();
	if (PRigidActor)
	{
		const PxTransform PBodyInstanceSpaceToTestSpace = U2PTransform(BodyInstanceSpaceToTestSpace);

		ExecuteOnPhysicsReadOnly([&]
		{
		// Get all the shapes from the actor
			FInlinePxShapeArray PShapes;
			const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *PRigidActor);

			// Iterate over each shape
			TArray<struct FOverlapResult> TempOverlaps;
			for (int32 ShapeIdx = 0; ShapeIdx < NumShapes; ShapeIdx++)
			{
				PxShape* PShape = PShapes[ShapeIdx];
				check(PShape);

				if (IsShapeBoundToBody(PShape) == false)
				{
					continue;
				}

				const PxGeometryType::Enum ShapeType = PShape->getGeometryType();
				if (ShapeType == PxGeometryType::eHEIGHTFIELD || ShapeType == PxGeometryType::eTRIANGLEMESH)
				{
					continue;	//we skip complex shapes - should this respect ComplexAsSimple?
				}

				// Calc shape global pose
				const PxTransform PLocalPose = PShape->getLocalPose();
				const PxTransform PShapeGlobalPose = PBodyInstanceSpaceToTestSpace.transform(PLocalPose);
			
				TempOverlaps.Reset();
				if (GeomOverlapMulti_PhysX(World, PShape->getGeometry().any(), PShapeGlobalPose, TempOverlaps, TestChannel, Params, ResponseParams, ObjectQueryParams))
				{
					bHaveBlockingHit = true;
				}
				InOutOverlaps.Append(TempOverlaps);
			}
		});
	}
#endif

	return bHaveBlockingHit;
}

extern bool GHillClimbError;

void LogHillClimbError(const FBodyInstance* BI, const PxGeometry& PGeom, const PxTransform& ShapePose)
{
	FString DebugName = BI->OwnerComponent.Get() ? BI->OwnerComponent->GetReadableName() : FString("None");
	FString TransformString = P2UTransform(ShapePose).ToString();
	if(PGeom.getType() == PxGeometryType::eCAPSULE)
	{
		const PxCapsuleGeometry& CapsuleGeom = static_cast<const PxCapsuleGeometry&>(PGeom);
		ensureAlwaysMsgf(false, TEXT("HillClimbing stuck in infinite loop for component:%s with Capsule half-height:%f, radius:%f, at world transform:%s"), *DebugName, CapsuleGeom.halfHeight, CapsuleGeom.radius, *TransformString);
	}
	else
	{
		const uint32 GeomType = PGeom.getType();
		ensureAlwaysMsgf(false, TEXT("HillClimbing stuck in infinite loop for component:%s with geometry type:%d, at world transform:%s"), *DebugName, GeomType, *TransformString);
	}


	
	GHillClimbError = false;
}

#if WITH_PHYSX
bool FBodyInstance::OverlapPhysX_AssumesLocked(const PxGeometry& PGeom, const PxTransform& ShapePose, FMTDResult* OutMTD) const
{
	const PxRigidActor* RigidBody = WeldParent ? WeldParent->GetPxRigidActor_AssumesLocked() : GetPxRigidActor_AssumesLocked();

	if (RigidBody==NULL || RigidBody->getNbShapes()==0)
	{
		return false;
	}

	// Get all the shapes from the actor
	FInlinePxShapeArray PShapes;
	const int32 NumShapes = FillInlinePxShapeArray_AssumesLocked(PShapes, *RigidBody);

	// Iterate over each shape
	for(int32 ShapeIdx=0; ShapeIdx<NumShapes; ++ShapeIdx)
	{
		const PxShape* PShape = PShapes[ShapeIdx];
		check(PShape);

		if (IsShapeBoundToBody(PShape) == true)
		{
			PxVec3 POutDirection;
			float OutDistance;

			if(OutMTD)
			{
				if (PxGeometryQuery::computePenetration(POutDirection, OutDistance, PGeom, ShapePose, PShape->getGeometry().any(), GetPxTransform_AssumesLocked(PShape, RigidBody)))
				{
					//TODO: there are some edge cases that give us nan results. In these cases we skip
					if (!POutDirection.isFinite())
					{
#if !UE_BUILD_SHIPPING
						//UE_LOG(LogPhysics, Warning, TEXT("Warning: OverlapPhysX_AssumesLocked: MTD returned NaN :( normal: (X:%f, Y:%f, Z:%f)"), POutDirection.x, POutDirection.y, POutDirection.z);
#endif
						POutDirection.x = 0.f;
						POutDirection.y = 0.f;
						POutDirection.z = 0.f;
					}

					OutMTD->Direction = P2UVector(POutDirection);
					OutMTD->Distance = FMath::Abs(OutDistance);

					if (GHillClimbError)
					{
						LogHillClimbError(this, PGeom, ShapePose);
					}

					return true;
				}
			}
			else
			{
				if(PxGeometryQuery::overlap(PGeom, ShapePose, PShape->getGeometry().any(), GetPxTransform_AssumesLocked(PShape, RigidBody)))
				{
					return true;
				}
			}
					
		}
	}

	if (GHillClimbError)
	{
		LogHillClimbError(this, PGeom, ShapePose);
	}
	return false;
}
#endif //WITH_PHYSX


bool FBodyInstance::IsValidCollisionProfileName(FName InCollisionProfileName)
{
	return (InCollisionProfileName != NAME_None) && (InCollisionProfileName != UCollisionProfile::CustomCollisionProfileName);
}

void FBodyInstance::LoadProfileData(bool bVerifyProfile)
{
	FName UseCollisionProfileName = GetCollisionProfileName();
	if ( bVerifyProfile )
	{
		// if collision profile name exists, 
		// check with current settings
		// if same, then keep the profile name
		// if not same, that means it has been modified from default
		// leave it as it is, and clear profile name
		if ( IsValidCollisionProfileName(UseCollisionProfileName) )
		{
			FCollisionResponseTemplate Template;
			if ( UCollisionProfile::Get()->GetProfileTemplate(UseCollisionProfileName, Template) ) 
			{
				// this function is only used for old code that did require verification of using profile or not
				// so that means it will have valid ResponsetoChannels value, so this is okay to access. 
				if (Template.IsEqual(CollisionEnabled, ObjectType, CollisionResponses.GetResponseContainer()) == false)
				{
					InvalidateCollisionProfileName(); 
				}
			}
			else
			{
				UE_LOG(LogPhysics, Warning, TEXT("COLLISION PROFILE [%s] is not found"), *UseCollisionProfileName.ToString());
				// if not nothing to do
				InvalidateCollisionProfileName(); 
			}
		}

	}
	else
	{
		if ( IsValidCollisionProfileName(UseCollisionProfileName) )
		{
			if ( UCollisionProfile::Get()->ReadConfig(UseCollisionProfileName, *this) == false)
			{
				// clear the name
				InvalidateCollisionProfileName();
			}
		}

		// no profile, so it just needs to update container from array data
		if ( DoesUseCollisionProfile() == false )
		{
			// if external profile copy the data over
			if (ExternalCollisionProfileBodySetup.IsValid(true))
			{
				UBodySetup* BodySetupInstance = ExternalCollisionProfileBodySetup.Get(true);
				const FBodyInstance& ExternalBodyInstance = BodySetupInstance->DefaultInstance;
				CollisionProfileName = ExternalBodyInstance.CollisionProfileName;
				ObjectType = ExternalBodyInstance.ObjectType;
				CollisionEnabled = ExternalBodyInstance.CollisionEnabled;
				CollisionResponses.SetCollisionResponseContainer(ExternalBodyInstance.CollisionResponses.ResponseToChannels);
			}
			else
			{
				CollisionResponses.UpdateResponseContainerFromArray();
			}
		}
	}
}

SIZE_T FBodyInstance::GetBodyInstanceResourceSize(EResourceSizeMode::Type Mode) const
{
	FResourceSizeEx ResSize = FResourceSizeEx(Mode);
	GetBodyInstanceResourceSizeEx(ResSize);
	return ResSize.GetTotalMemoryBytes();
}

void FBodyInstance::GetBodyInstanceResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
{
#if WITH_PHYSX
	// Get size of PhysX data, skipping contents of 'shared data'
	if (RigidActorSync != NULL)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetPhysxObjectSize(RigidActorSync, FPhysxSharedData::Get().GetCollection()));
	}

	if (RigidActorAsync != NULL)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetPhysxObjectSize(RigidActorAsync, FPhysxSharedData::Get().GetCollection()));
	}
#endif
}

void FBodyInstance::FixupData(class UObject* Loader)
{
	check (Loader);

	int32 const UE4Version = Loader->GetLinkerUE4Version();

	if (UE4Version < VER_UE4_ADD_CUSTOMPROFILENAME_CHANGE)
	{
		if (CollisionProfileName == NAME_None)
		{
			CollisionProfileName = UCollisionProfile::CustomCollisionProfileName;
		}
	}

	if (UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
	{
		CollisionResponses.SetCollisionResponseContainer(ResponseToChannels_DEPRECATED);
	}

	// Load profile. If older version, please verify profile name first
	bool bNeedToVerifyProfile = (UE4Version < VER_UE4_COLLISION_PROFILE_SETTING) || 
		// or shape component needs to convert since we added profile
		(UE4Version < VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL && Loader->IsA(UShapeComponent::StaticClass()));
	LoadProfileData(bNeedToVerifyProfile);

	// if profile isn't set, then fix up channel responses
	if( CollisionProfileName == UCollisionProfile::CustomCollisionProfileName ) 
	{
		if (UE4Version >= VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL)
		{
			CollisionResponses.UpdateResponseContainerFromArray();
		}
	}
}

bool FBodyInstance::UseAsyncScene(const FPhysScene* PhysScene) const
{
	bool bHasAsyncScene = true;
	if (PhysScene)
	{
		bHasAsyncScene = PhysScene->HasAsyncScene();
	}

	return bUseAsyncScene && bHasAsyncScene;
}


void FBodyInstance::SetUseAsyncScene(bool bNewUseAsyncScene)
{
	// Set flag
	bUseAsyncScene = bNewUseAsyncScene;
}

void FBodyInstance::ApplyMaterialToShape_AssumesLocked(PxShape* PShape, PxMaterial* PSimpleMat, const TArray<UPhysicalMaterial*>& ComplexPhysMats, const bool bSharedShape)
{
	if(!bSharedShape && !PShape->isExclusive())	//user says the shape is exclusive, but physx says it's shared
	{
		UE_LOG(LogPhysics, Warning, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : Trying to change the physical material of a shared shape. If this is your intention pass bSharedShape = true"));
	}

	// If a triangle mesh, need to get array of materials...
	if(PShape->getGeometryType() == PxGeometryType::eTRIANGLEMESH)
	{
		TArray<PxMaterial*, TInlineAllocator<16>> PComplexMats;
		PComplexMats.AddUninitialized(ComplexPhysMats.Num());
		for(int MatIdx = 0; MatIdx < ComplexPhysMats.Num(); MatIdx++)
		{
			check(ComplexPhysMats[MatIdx] != NULL);
			PComplexMats[MatIdx] = ComplexPhysMats[MatIdx]->GetPhysXMaterial();
			check(PComplexMats[MatIdx] != NULL);
		}

		if(PComplexMats.Num())
		{
			PShape->setMaterials(PComplexMats.GetData(), PComplexMats.Num());
		}
		else
		{
			if(PSimpleMat)
			{
				UE_LOG(LogPhysics, Verbose, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : PComplexMats is empty - falling back on simple physical material."));
				PShape->setMaterials(&PSimpleMat, 1);
			}
			else
			{
				UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : PComplexMats is empty, and we do not have a valid simple material."));
			}
		}

	}
	// Simple shape, 
	else if(PSimpleMat)
	{
		PShape->setMaterials(&PSimpleMat, 1);
	}
	else
	{
		UE_LOG(LogPhysics, Error, TEXT("FBodyInstance::ApplyMaterialToShape_AssumesLocked : No valid simple physics material found."));
	}
}

void FBodyInstance::ApplyMaterialToInstanceShapes_AssumesLocked(PxMaterial* PSimpleMat, TArray<UPhysicalMaterial*>& ComplexPhysMats)
{
	FBodyInstance* TheirBI = this;
	FBodyInstance* BIWithActor = TheirBI->WeldParent ? TheirBI->WeldParent : TheirBI;

	TArray<PxShape*> AllShapes;
	BIWithActor->GetAllShapes_AssumesLocked(AllShapes);

	for(int32 ShapeIdx = 0; ShapeIdx < AllShapes.Num(); ShapeIdx++)
	{
		PxShape* PShape = AllShapes[ShapeIdx];
		if (TheirBI->IsShapeBoundToBody(PShape))
		{
			ExecuteOnPxShapeWrite(BIWithActor, PShape, [&](PxShape* PNewShape)
		{
				ApplyMaterialToShape_AssumesLocked(PNewShape, PSimpleMat, ComplexPhysMats, TheirBI->HasSharedShapes());
		});		
	}
}
}

bool FBodyInstance::ValidateTransform(const FTransform &Transform, const FString& DebugName, const UBodySetup* Setup)
{
	if(Transform.GetScale3D().IsNearlyZero())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Scale3D is (nearly) zero: %s"), *DebugName);
		return false;
	}

	// Check we support mirroring/non-mirroring
	const float TransformDet = Transform.GetDeterminant();
	if(TransformDet < 0.f && !Setup->bGenerateMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is mirrored but bGenerateMirroredCollision == false: %s"), *DebugName);
		return false;
	}

	if(TransformDet > 0.f && !Setup->bGenerateNonMirroredCollision)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Body is not mirrored but bGenerateNonMirroredCollision == false: %s"), *DebugName);
		return false;
	}

#if !(UE_BUILD_SHIPPING)
	if(Transform.ContainsNaN())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Initialising Body : Bad transform - %s %s\n%s"), *DebugName, *Setup->BoneName.ToString(), *Transform.ToString());
		return false;
	}
#endif

	return true;
}

#if WITH_PHYSX
void FBodyInstance::InitDynamicProperties_AssumesLocked()
{
	if (!BodySetup.IsValid())
	{
		// This may be invalid following an undo if the BodySetup was a transient object (e.g. in Mesh Paint mode)
		// Just exit gracefully if so.
		return;
	}

	//QueryOnly bodies cannot become simulated at runtime. To do this they must change their CollisionEnabled which recreates the physics state
	//So early out to save a lot of useless work
	if (GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
	{
		return;
	}
	
	if(PxRigidDynamic* RigidActor = GetPxRigidDynamic_AssumesLocked())
	{
		//A non simulated body may become simulated at runtime, so we need to compute its mass.
		//However, this is not supported for complexAsSimple since a trimesh cannot itself be simulated, it can only be used for collision of other simple shapes.
		if (BodySetup->GetCollisionTraceFlag() != ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			UpdateMassProperties();
			UpdateDampingProperties();
			SetMaxAngularVelocityInRadians(GetMaxAngularVelocityInRadians(), false, false);
			SetMaxDepenetrationVelocity(bOverrideMaxDepenetrationVelocity ? MaxDepenetrationVelocity : UPhysicsSettings::Get()->MaxDepenetrationVelocity);
		}else
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bSimulatePhysics)
			{
				if(UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get())
				{
					FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SimComplexAsSimple", "Trying to simulate physics on ''{0}'' but it has ComplexAsSimple collision."),
						FText::FromString(GetPathNameSafe(OwnerComponentInst))));
				}
				
			}
#endif
		}

		if (ShouldInstanceSimulatingPhysics())
		{
			RigidActor->setLinearVelocity(U2PVector(InitialLinearVelocity));
		}

		float SleepEnergyThresh = RigidActor->getSleepThreshold();
		SleepEnergyThresh *= GetSleepThresholdMultiplier();
		RigidActor->setSleepThreshold(SleepEnergyThresh);
		float StabilizationThreshold = RigidActor->getStabilizationThreshold();
		StabilizationThreshold *= StabilizationThresholdMultiplier;
		RigidActor->setStabilizationThreshold(StabilizationThreshold);
		// set solver iteration count 
		int32 PositionIterCount = FMath::Clamp(PositionSolverIterationCount, 1, 255);
		int32 VelocityIterCount = FMath::Clamp(VelocitySolverIterationCount, 1, 255);
		RigidActor->setSolverIterationCounts(PositionIterCount, VelocityIterCount);

		CreateDOFLock();
		if(!IsRigidBodyKinematic_AssumesLocked(RigidActor))
		{
			if(!bStartAwake && !bWokenExternally)
			{
				RigidActor->setWakeCounter(0.f);
				RigidActor->putToSleep();
			}
		}
	}
}

void FBodyInstance::GetFilterData_AssumesLocked(FShapeData& ShapeData, bool bForceSimpleAsComplex)//PxFilterData& SimData, PxFilterData& QueryDataSimple, PxFilterData& QueryDataComplex, TEnumAsByte<ECollisionEnabled::Type>& UseCollisionEnabled, bool bForceSimpleAsComplex)
{
	// Do nothing if no physics actor
	if(!IsValidBodyInstance())
	{
		return;
	}

	// this can happen in landscape height field collision component
	if(!BodySetup.IsValid())
	{
		return;
	}

	// Figure out if we are static
	UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
	AActor* Owner = OwnerComponentInst ? OwnerComponentInst->GetOwner() : NULL;
	const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

	// Grab collision setting from body instance
	ShapeData.CollisionEnabled = GetCollisionEnabled(); // this checks actor override
	bool bUseNotifyRBCollision = bNotifyRigidBodyCollision;
	FCollisionResponseContainer UseResponse = CollisionResponses.GetResponseContainer();

	bool bUseCollisionEnabledOverride = false;
	bool bResponseOverride = false;
	bool bNotifyOverride = false;

	if(USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(OwnerComponentInst))
	{
		// In skeletal case, collision enable/disable/movement should be overriden by mesh component
		// being in the physics asset, and not having collision is a waste and it can cause a bug where disconnected bodies
		if(Owner)
		{
			if(Owner->GetActorEnableCollision())	//we only override if actor has collision enabled
			{
				ShapeData.CollisionEnabled = SkelMeshComp->BodyInstance.CollisionEnabled;
			}
			else
			{	//actor has collision disabled so disable regardless of component setting
				ShapeData.CollisionEnabled = ECollisionEnabled::NoCollision;
			}
		}


		ObjectType = SkelMeshComp->GetCollisionObjectType();
		bUseCollisionEnabledOverride = true;

		if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Enabled)
		{
			UseResponse.SetAllChannels(ECR_Block);
		}
		else if(BodySetup->CollisionReponse == EBodyCollisionResponse::BodyCollision_Disabled)
		{
			UseResponse.SetAllChannels(ECR_Ignore);
		}

		UseResponse = FCollisionResponseContainer::CreateMinContainer(UseResponse, SkelMeshComp->BodyInstance.CollisionResponses.GetResponseContainer());
		bUseNotifyRBCollision = bUseNotifyRBCollision && SkelMeshComp->BodyInstance.bNotifyRigidBodyCollision;
		bResponseOverride = true;
		bNotifyOverride = true;
	}

#if WITH_EDITOR
	// if no collision, but if world wants to enable trace collision for components, allow it
	if((ShapeData.CollisionEnabled == ECollisionEnabled::NoCollision) && Owner && (Owner->IsA(AVolume::StaticClass()) == false))
	{
		UWorld* World = Owner->GetWorld();
		UPrimitiveComponent* PrimComp = OwnerComponentInst;
		if(World && World->bEnableTraceCollision &&
		   (PrimComp->IsA(UStaticMeshComponent::StaticClass()) || PrimComp->IsA(USkeletalMeshComponent::StaticClass()) || PrimComp->IsA(UBrushComponent::StaticClass())))
		{
			//UE_LOG(LogPhysics, Warning, TEXT("Enabling collision %s : %s"), *GetNameSafe(Owner), *GetNameSafe(OwnerComponent.Get()));
			// clear all other channel just in case other people using those channels to do something
			UseResponse.SetAllChannels(ECR_Ignore);
			ShapeData.CollisionEnabled = ECollisionEnabled::QueryOnly;
			bResponseOverride = true;
			bUseCollisionEnabledOverride = true;
		}
	}
#endif

	const bool bUseComplexAsSimple = !bForceSimpleAsComplex && (BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
	const bool bUseSimpleAsComplex = bForceSimpleAsComplex || (BodySetup.Get()->GetCollisionTraceFlag() == CTF_UseSimpleAsComplex);

	if(GetPxRigidActor_AssumesLocked())
	{
		if(ShapeData.CollisionEnabled != ECollisionEnabled::NoCollision)
		{
			bool * bNotifyOverridePtr = bNotifyOverride ? &bUseNotifyRBCollision : NULL;

			PxFilterData PSimFilterData;
			PxFilterData PSimpleQueryData;
			PxFilterData PComplexQueryData;
			uint32 ActorID = Owner ? Owner->GetUniqueID() : 0;
			uint32 CompID = (OwnerComponentInst != nullptr) ? OwnerComponentInst->GetUniqueID() : 0;
			CreateShapeFilterData(ObjectType, MaskFilter, ActorID, UseResponse, CompID, InstanceBodyIndex, PSimpleQueryData, PSimFilterData, bUseCCD && !bPhysicsStatic, bUseNotifyRBCollision, bPhysicsStatic);	//CCD is determined by root body in case of welding
			PComplexQueryData = PSimpleQueryData;
			
			// Set output sim data
			ShapeData.FilterData.SimFilter = PSimFilterData;

			// Build filterdata variations for complex and simple
			PSimpleQueryData.word3 |= EPDF_SimpleCollision;
			if(bUseSimpleAsComplex)
			{
				PSimpleQueryData.word3 |= EPDF_ComplexCollision;
			}

			PComplexQueryData.word3 |= EPDF_ComplexCollision;
			if(bUseComplexAsSimple)
			{
				PComplexQueryData.word3 |= EPDF_SimpleCollision;
			}
			
			ShapeData.FilterData.QuerySimpleFilter = PSimpleQueryData;
			ShapeData.FilterData.QueryComplexFilter = PComplexQueryData;
		}
	}
}

void FBodyInstance::InitStaticBodies(const TArray<FBodyInstance*>& Bodies, const TArray<FTransform>& Transforms, class UBodySetup* BodySetup, class UPrimitiveComponent* PrimitiveComp, class FPhysScene* InRBScene, UPhysicsSerializer* PhysicsSerializer)
{
	SCOPE_CYCLE_COUNTER(STAT_StaticInitBodies);

	check(BodySetup);
	check(InRBScene);
	check(Bodies.Num() > 0);

	static TArray<FBodyInstance*> BodiesStatic;
	static TArray<FTransform> TransformsStatic;

	check(BodiesStatic.Num() == 0);
	check(TransformsStatic.Num() == 0);

	BodiesStatic = Bodies;
	TransformsStatic = Transforms;

	FInitBodiesHelper<true> InitBodiesHelper(BodiesStatic, TransformsStatic, BodySetup, PrimitiveComp, InRBScene, FInitBodySpawnParams(PrimitiveComp), nullptr, PhysicsSerializer);
	InitBodiesHelper.InitBodies();

	BodiesStatic.Reset();
	TransformsStatic.Reset();
}

void FBodyInstance::SetShapeFlagsInternal_AssumesShapeLocked(FSetShapeParams& Params, bool& bUpdateMassProperties)
{
	PxShapeFlags ShapeFlags = Params.PShape->getFlags();

	// If query collision is enabled.
	if (Params.UseCollisionEnabled != ECollisionEnabled::NoCollision)
	{
		// Determine whether or not Queries are enabled.
		const bool bQueryEnabled = CollisionEnabledHasQuery(Params.UseCollisionEnabled);

		// Determines whether or not physics collision is enabled.
		const bool bSimCollision = CollisionEnabledHasPhysics(Params.UseCollisionEnabled);

		// Only perform scene queries when requested, and the shape is non-static
		// or is in the sync scene.
		ModifyShapeFlag<PxShapeFlag::eSCENE_QUERY_SHAPE>(ShapeFlags, bQueryEnabled && (!Params.bPhysicsStatic || Params.bIsSync));

		// Triangle mesh is 'complex' geom
		if (Params.IsTriangleMesh())
		{
			// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
			ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeFlags, bSimCollision && Params.bUseComplexAsSimple);

			if (!Params.IsOwnerAModelComponent())
			{
				ModifyShapeFlag<PxShapeFlag::eVISUALIZATION>(ShapeFlags, false);
			}
		}
		// Everything else is 'simple'
		else
		{
			// See if we currently have sim collision
			const bool bCurrentSimCollision = (ShapeFlags & PxShapeFlag::eSIMULATION_SHAPE);

			if (bCurrentSimCollision != bSimCollision)
			{
				bUpdateMassProperties = true;
				ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeFlags, bSimCollision);
			}

			// enable swept bounds for CCD for this shape
			if (PxRigidBody* PBody = Params.PRigidActor->is<PxRigidBody>())
			{
				const bool bWantsCcd = bSimCollision && !Params.bPhysicsStatic && bUseCCD;
				ModifyRigidBodyFlag_Isolated<PxRigidBodyFlag::eENABLE_CCD>(PBody, bWantsCcd);
			}
		}
	}
	// No collision enabled
	else
	{
		ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeFlags, false);
		ModifyShapeFlag<PxShapeFlag::eSCENE_QUERY_SHAPE>(ShapeFlags, false);
	}

	Params.PShape->setFlags(ShapeFlags);
}

void FBodyInstance::SetShapeFlags_AssumesLocked(TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, PxShape* PInShape, EPhysicsSceneType SceneType, const bool bUseComplexAsSimple)
{
	ExecuteOnPxShapeWrite(this, PInShape, [&](PxShape* PShape)
	{
		UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();

		// If query collision is enabled.
		bool bUpdateMassProperties = false;

		FSetShapeParams Params(
			/* UseCollisionEnabled */	UseCollisionEnabled,
			/* bPhysicsStatic */        !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry(),
			/* bIsSync */               SceneType == PST_Sync,
			/* bUseComplexAsSimple */   bUseComplexAsSimple,
			/* OwnerComponent */        OwnerComponentInst,
			/* PShape */                PShape,
			/* PRigidActor */           GetPxRigidActor_AssumesLocked()
		);

		SetShapeFlagsInternal_AssumesShapeLocked(Params, bUpdateMassProperties);

		if (bUpdateMassProperties)
		{
			UpdateMassProperties();
		}
	});
}

void FBodyInstance::GetShapeFlags_AssumesLocked(FShapeData& ShapeData, TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, const bool bUseComplexAsSimple /*= false*/)
{
	ShapeData.CollisionEnabled = UseCollisionEnabled;
	ShapeData.SyncShapeFlags = PxShapeFlags(0);
	ShapeData.AsyncShapeFlags = PxShapeFlags(0);
	ShapeData.SimpleShapeFlags = PxShapeFlags(0);
	ShapeData.ComplexShapeFlags = PxShapeFlags(0);
	ShapeData.SyncBodyFlags = PxRigidBodyFlags(0);

	// Default flags
	ShapeData.SimpleShapeFlags |= PxShapeFlag::eVISUALIZATION;
	ShapeData.ComplexShapeFlags |= PxShapeFlag::eVISUALIZATION;

	if(UseCollisionEnabled != ECollisionEnabled::NoCollision)
	{
		const bool bQueryEnabled = CollisionEnabledHasQuery(UseCollisionEnabled);

		// See if we want physics collision
		const bool bSimCollision = CollisionEnabledHasPhysics(UseCollisionEnabled);

		const UPrimitiveComponent* OwnerComponentInst = OwnerComponent.Get();
		const bool bPhysicsStatic = !OwnerComponentInst || OwnerComponentInst->IsWorldGeometry();

		if (bQueryEnabled)
		{
			// Only perform scene queries if enabled and the shape is non-static or the shape is sync.
			ModifyShapeFlag<PxShapeFlag::eSCENE_QUERY_SHAPE>(ShapeData.SyncShapeFlags, true);

			if (!bPhysicsStatic)
			{
				ModifyShapeFlag<PxShapeFlag::eSCENE_QUERY_SHAPE>(ShapeData.AsyncShapeFlags, true);
			}
		}

		// Enable sim collision
		if(bSimCollision)
		{
			ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeData.SimpleShapeFlags, true);
			
			// on dynamic objects and objects which don't use complex as simple, tri mesh not used for sim
			if(bUseComplexAsSimple)
			{
				ModifyShapeFlag<PxShapeFlag::eSIMULATION_SHAPE>(ShapeData.ComplexShapeFlags, true);
			}
		}
		
		if(OwnerComponentInst == NULL || !OwnerComponentInst->IsA(UModelComponent::StaticClass()))
		{
			ModifyShapeFlag<PxShapeFlag::eVISUALIZATION>(ShapeData.ComplexShapeFlags, false);
		}

		// enable swept bounds for CCD for this shape
		if(bSimCollision && !bPhysicsStatic)
		{
			if (GetPxRigidActor_AssumesLocked()->is<PxRigidBody>())
			{
				const bool bKinematic = !ShouldInstanceSimulatingPhysics();
				ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(ShapeData.SyncBodyFlags, bKinematic);
				ModifyRigidBodyFlag<PxRigidBodyFlag::eKINEMATIC>(ShapeData.AsyncBodyFlags, bKinematic);

				ModifyRigidBodyFlag<PxRigidBodyFlag::eENABLE_CCD>(ShapeData.SyncBodyFlags, bUseCCD);
				ModifyRigidBodyFlag<PxRigidBodyFlag::eENABLE_CCD>(ShapeData.AsyncBodyFlags, bUseCCD);
			}
		}
	}
}
#endif // WITH_PHYSX

////////////////////////////////////////////////////////////////////////////
// FBodyInstanceEditorHelpers

#if WITH_EDITOR

void FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(UPrimitiveComponent* Component, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Automatically change collision profile based on mobility and physics settings (if it is currently one of the default profiles)
		const bool bMobilityChanged = PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, Mobility);
		const bool bSimulatePhysicsChanged = PropertyName == GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics);

		if (bMobilityChanged || bSimulatePhysicsChanged)
		{
			// If we enabled physics simulation, but we are not marked movable, do that for them
			if (bSimulatePhysicsChanged && Component->BodyInstance.bSimulatePhysics && (Component->Mobility != EComponentMobility::Movable))
			{
				Component->SetMobility(EComponentMobility::Movable);
			}
			// If we made the component no longer movable, but simulation was enabled, disable that for them
			else if (bMobilityChanged && (Component->Mobility != EComponentMobility::Movable) && Component->BodyInstance.bSimulatePhysics)
			{
				Component->BodyInstance.bSimulatePhysics = false;
			}

			// If the collision profile is one of the 'default' ones for a StaticMeshActor, make sure it is the correct one
			// If user has changed it to something else, don't touch it
			const FName CurrentProfileName = Component->BodyInstance.GetCollisionProfileName();
			if ((CurrentProfileName == UCollisionProfile::BlockAll_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::BlockAllDynamic_ProfileName) ||
				(CurrentProfileName == UCollisionProfile::PhysicsActor_ProfileName))
			{
				if (Component->Mobility == EComponentMobility::Movable)
				{
					if (Component->BodyInstance.bSimulatePhysics)
					{
						Component->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
					}
					else
					{
						Component->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
					}
				}
				else
				{
					Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
				}
			}
		}
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
