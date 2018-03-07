// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Stats/Stats.h"
#include "UObject/UObjectBaseUtility.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "SkeletalMeshTypes.h"
#include "ClothSimData.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "SkeletalRender.h"
#include "SkeletalRenderPublic.h"
#include "ModuleManager.h"

#include "Logging/MessageLog.h"
#include "CollisionDebugDrawingPublic.h"

#if WITH_PHYSX
	#include "SceneManagement.h"
	#include "PhysXPublic.h"
	#include "PhysicsEngine/PhysXSupport.h"
#endif

#if WITH_APEX

#if WITH_APEX_CLOTHING
	// for cloth morph target	
	#include "Animation/MorphTarget.h"

#endif// #if WITH_APEX_CLOTHING

#endif//#if WITH_APEX
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

#if WITH_EDITOR
#include "ClothingSystemEditorInterfaceModule.h"
#include "SimulationEditorExtender.h"
#endif

#define LOCTEXT_NAMESPACE "SkeletalMeshComponentPhysics"

DECLARE_CYCLE_STAT(TEXT("CreateClothing"), STAT_CreateClothing, STATGROUP_Physics);


extern TAutoConsoleVariable<int32> CVarEnableClothPhysics;

//This is the total cloth time split up among multiple computation (updating gpu, updating sim, etc...)
DECLARE_CYCLE_STAT(TEXT("Cloth Total"), STAT_ClothTotalTime, STATGROUP_Physics);
DECLARE_CYCLE_STAT(TEXT("Cloth Writeback"), STAT_ClothWriteback, STATGROUP_Physics);

void FSkeletalMeshComponentClothTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FSkeletalMeshComponentClothTickFunction_ExecuteTick);
	FActorComponentTickFunction::ExecuteTickHelper(Target,/*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->TickClothing(DilatedTime, *this);
	});
}

FString FSkeletalMeshComponentClothTickFunction::DiagnosticMessage()
{
	return TEXT("FSkeletalMeshComponentClothTickFunction");
}

void FSkeletalMeshComponentEndPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FSkeletalMeshComponentEndPhysicsTickFunction_ExecuteTick);
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->EndPhysicsTickComponent(*this);
	});
}

FString FSkeletalMeshComponentEndPhysicsTickFunction::DiagnosticMessage()
{
	return TEXT("FSkeletalMeshComponentEndPhysicsTickFunction");
}

void USkeletalMeshComponent::CreateBodySetup()
{
	check(SkeletalMesh);

	if (BodySetup == NULL)
	{
		BodySetup = NewObject<UBodySetup>(this);
	}

	UBodySetup* OriginalBodySetup = SkeletalMesh->GetBodySetup();
	
	BodySetup->CopyBodyPropertiesFrom(OriginalBodySetup);
	BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;

	BodySetup->CookedFormatDataOverride = &OriginalBodySetup->CookedFormatData;

	//need to recreate meshes
	BodySetup->ClearPhysicsMeshes();
	BodySetup->CreatePhysicsMeshes();
}

//
//	USkeletalMeshComponent
//
UBodySetup* USkeletalMeshComponent::GetBodySetup()
{
	if (bEnablePerPolyCollision == false)
	{
		UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
		if (SkeletalMesh && PhysicsAsset)
		{
			for (int32 i = 0; i < SkeletalMesh->RefSkeleton.GetNum(); i++)
			{
				int32 BodyIndex = PhysicsAsset->FindBodyIndex(SkeletalMesh->RefSkeleton.GetBoneName(i));
				if (BodyIndex != INDEX_NONE)
				{
					return PhysicsAsset->SkeletalBodySetups[BodyIndex];
				}
			}
		}
	}
	else
	{
		if (BodySetup == NULL)
		{
			CreateBodySetup();
		}

		return BodySetup;
	}


	return NULL;
}

bool USkeletalMeshComponent::CanEditSimulatePhysics()
{
	return GetPhysicsAsset() != nullptr;
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bSimulate)
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	BodyInstance.bSimulatePhysics = bSimulate;

	// enable blending physics
	bBlendPhysics = bSimulate;

	//Go through body setups and see which bodies should be turned on and off
	if (UPhysicsAsset * PhysAsset = GetPhysicsAsset())
	{
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (FBodyInstance* BodyInst = Bodies[BodyIdx])
			{
				if (UBodySetup * PhysAssetBodySetup = PhysAsset->SkeletalBodySetups[BodyIdx])
				{
					if (PhysAssetBodySetup->PhysicsType == EPhysicsType::PhysType_Default)
					{
						BodyInst->SetInstanceSimulatePhysics(bSimulate);
					}
				}
			}
		}
	}

	if(IsSimulatingPhysics())
	{
		SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint
	}

 	UpdateEndPhysicsTickRegisteredState();
 	UpdateClothTickRegisteredState();
}

void USkeletalMeshComponent::OnComponentCollisionSettingsChanged()
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->UpdatePhysicsFilterData();
	}

	if (SceneProxy)
	{
		((FSkeletalMeshSceneProxy*)SceneProxy)->SetCollisionEnabled_GameThread(IsCollisionEnabled());
	}

	Super::OnComponentCollisionSettingsChanged();
}

void USkeletalMeshComponent::AddRadialImpulse(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange)
{
	if(bIgnoreRadialImpulse)
	{
		return;
	}

	PendingRadialForces.Emplace(Origin, Radius, Strength, Falloff, bVelChange, FPendingRadialForces::EType::AddImpulse);
	
	const float StrengthPerMass = Strength / FMath::Max(GetMass(), KINDA_SMALL_NUMBER);
	for(FBodyInstance* Body : Bodies)
	{
		const float StrengthPerBody = bVelChange ? Strength : (StrengthPerMass * Body->GetBodyMass());
		Body->AddRadialImpulseToBody(Origin, Radius, StrengthPerBody, Falloff, bVelChange);
	}
}



void USkeletalMeshComponent::AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bAccelChange)
{
	if(bIgnoreRadialForce)
	{
		return;
	}

	PendingRadialForces.Emplace(Origin, Radius, Strength, Falloff, bAccelChange, FPendingRadialForces::EType::AddForce);

	const float StrengthPerMass = Strength / FMath::Max(GetMass(), KINDA_SMALL_NUMBER);
	for (FBodyInstance* Body : Bodies)
	{
		const float StrengthPerBody = bAccelChange ? Strength : (StrengthPerMass * Body->GetBodyMass());
		Body->AddRadialForceToBody(Origin, Radius, StrengthPerBody, Falloff, bAccelChange);
	}
}

void USkeletalMeshComponent::WakeAllRigidBodies()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->WakeInstance();
	}
}

void USkeletalMeshComponent::PutAllRigidBodiesToSleep()
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->PutInstanceToSleep();
	}
}


bool USkeletalMeshComponent::IsAnyRigidBodyAwake()
{
	bool bAwake = false;

	// ..iterate over each body to find any that are awak
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		if(BI->IsInstanceAwake())
		{
			// Found an awake one - so mesh is considered 'awake'
			bAwake = true;
			continue;
		}
	}

	return bAwake;
}


void USkeletalMeshComponent::SetAllPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent)
{
	for (int32 i=0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BodyInst = Bodies[i];
		check(BodyInst);
		BodyInst->SetLinearVelocity(NewVel, bAddToCurrent);
	}
}

void USkeletalMeshComponent::SetAllPhysicsAngularVelocityInRadians(FVector const& NewAngVel, bool bAddToCurrent)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// Find the root actor. We use its location as the center of the rotation.
		FBodyInstance* RootBodyInst = Bodies[ RootBodyData.BodyIndex ];
		check(RootBodyInst);
		FTransform RootTM = RootBodyInst->GetUnrealWorldTransform();

		FVector RootPos = RootTM.GetLocation();

		// Iterate over each bone, updating its velocity
		for (int32 i = 0; i < Bodies.Num(); i++)
		{
			FBodyInstance* const BI = Bodies[i];
			check(BI);

			BI->SetAngularVelocityInRadians(NewAngVel, bAddToCurrent);
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsPosition(FVector NewPos)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewPos
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FVector DeltaLoc = NewPos - RootBodyTM.GetLocation();
			RootBodyTM.SetTranslation(NewPos);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

#if DO_CHECK
			FVector RelativeVector = (RootBI->GetUnrealWorldTransform().GetLocation() - NewPos);
			check(RelativeVector.SizeSquared() < 1.f);
#endif

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetTranslation(BodyTM.GetTranslation() + DeltaLoc);
					BI->SetBodyTransform(BodyTM, ETeleportType::TeleportPhysics);
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetAllPhysicsRotation(FRotator NewRot)
{
	SetAllPhysicsRotation(NewRot.Quaternion());
}

void USkeletalMeshComponent::SetAllPhysicsRotation(const FQuat& NewRot)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRot.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("USkeletalMeshComponent::SetAllPhysicsRotation found NaN in parameter NewRot: %s"), *NewRot.ToString());
	}
#endif
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewRot
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			FQuat DeltaQuat = RootBodyTM.GetRotation().Inverse() * NewRot;
			RootBodyTM.SetRotation(NewRot);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetRotation(BodyTM.GetRotation() * DeltaQuat);
					BI->SetBodyTransform( BodyTM, ETeleportType::TeleportPhysics );
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::ApplyDeltaToAllPhysicsTransforms(const FVector& DeltaLocation, const FQuat& DeltaRotation)
{
	if(RootBodyData.BodyIndex != INDEX_NONE && RootBodyData.BodyIndex < Bodies.Num())
	{
		// calculate the deltas to get the root body to NewRot
		FBodyInstance* RootBI = Bodies[RootBodyData.BodyIndex];
		check(RootBI);
		if(RootBI->IsValidBodyInstance())
		{
			// move the root body
			FTransform RootBodyTM = RootBI->GetUnrealWorldTransform();
			RootBodyTM.SetRotation(RootBodyTM.GetRotation() * DeltaRotation);
			RootBodyTM.SetTranslation(RootBodyTM.GetTranslation() + DeltaLocation);
			RootBI->SetBodyTransform(RootBodyTM, ETeleportType::TeleportPhysics);

			// apply the delta to all the other bodies
			for (int32 i = 0; i < Bodies.Num(); i++)
			{
				if (i != RootBodyData.BodyIndex)
				{
					FBodyInstance* BI = Bodies[i];
					check(BI);

					FTransform BodyTM = BI->GetUnrealWorldTransform();
					BodyTM.SetRotation(BodyTM.GetRotation() * DeltaRotation);
					BodyTM.SetTranslation(BodyTM.GetTranslation() + DeltaLocation);
					BI->SetBodyTransform( BodyTM, ETeleportType::TeleportPhysics );
				}
			}

			// Move component to new physics location
			SyncComponentToRBPhysics();
		}
	}
}

void USkeletalMeshComponent::SetPhysMaterialOverride(UPhysicalMaterial* NewPhysMaterial)
{
	// Single-body case - just use PrimComp code.
	UPrimitiveComponent::SetPhysMaterialOverride(NewPhysMaterial);

	// Now update any child bodies
	for( int32 i = 0; i < Bodies.Num(); i++ )
	{
		FBodyInstance* BI = Bodies[i];
		BI->UpdatePhysicalMaterials();
	}
}

void USkeletalMeshComponent::SetEnableGravity(bool bGravityEnabled)
{
	if (!bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer())
	{
		return;
	}

	BodyInstance.bEnableGravity = bGravityEnabled;

	if (UPhysicsAsset * PhysAsset = GetPhysicsAsset())
	{
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (FBodyInstance* BodyInst = Bodies[BodyIdx])
			{
				if (UBodySetup * PhysAssetBodySetup = PhysAsset->SkeletalBodySetups[BodyIdx])
				{
					bool bUseGravityEnabled = bGravityEnabled;
					
					//If the default body instance has gravity turned off then turning it ON for skeletal mesh component does not turn the instance on
					if(bUseGravityEnabled && !PhysAssetBodySetup->DefaultInstance.bEnableGravity)	
					{
						bUseGravityEnabled = false;
					}
				
					BodyInst->SetEnableGravity(bUseGravityEnabled);
				}
			}
		}
	}
}

bool USkeletalMeshComponent::IsGravityEnabled() const
{
	return BodyInstance.bEnableGravity;
}

void USkeletalMeshComponent::OnConstraintBrokenWrapper(int32 ConstraintIndex)
{
	OnConstraintBroken.Broadcast(ConstraintIndex);
}

DECLARE_CYCLE_STAT(TEXT("Init Articulated"), STAT_InitArticulated, STATGROUP_Physics);

int32 USkeletalMeshComponent::FindRootBodyIndex() const
{
	// Find root physics body
	int32 RootBodyIndex = RootBodyData.BodyIndex;
	if(RootBodyIndex == INDEX_NONE && SkeletalMesh)
	{
		if(const UPhysicsAsset* PhysicsAsset = GetPhysicsAsset())
		{
			for (int32 i = 0; i<SkeletalMesh->RefSkeleton.GetNum(); i++)
			{
				int32 BodyInstIndex = PhysicsAsset->FindBodyIndex(SkeletalMesh->RefSkeleton.GetBoneName(i));
				if (BodyInstIndex != INDEX_NONE)
				{
					RootBodyIndex = BodyInstIndex;
					break;
				}
			}
		}
	}

	return RootBodyIndex;
}

void USkeletalMeshComponent::InitArticulated(FPhysScene* PhysScene)
{
	SCOPE_CYCLE_COUNTER(STAT_InitArticulated);

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	if(PhysScene == nullptr || PhysicsAsset == nullptr || SkeletalMesh == nullptr)
	{
		return;
	}

	if(Bodies.Num() > 0)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Bodies already created (%s) - call TermArticulated first."), *GetPathName());
		return;
	}

	FVector Scale3D = GetComponentTransform().GetScale3D();

	// Find root physics body
	RootBodyData.BodyIndex = INDEX_NONE;	//Reset the root body index just in case we need to refind a new one
	const int32 RootBodyIndex = FindRootBodyIndex();

	if(RootBodyIndex == INDEX_NONE)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Could not find root physics body: '%s'"), *GetPathName() );
		return;
	}

	// Set up the map from skelmeshcomp ID to collision disable table
#if WITH_PHYSX
	uint32 SkelMeshCompID = GetUniqueID();
	PhysScene->DeferredAddCollisionDisableTable(SkelMeshCompID, &PhysicsAsset->CollisionDisableTable);

	int32 NumShapes = 0;
	const int32 NumBodies = PhysicsAsset->SkeletalBodySetups.Num();
	for(int32 BodyIndex = 0; BodyIndex < NumBodies; ++BodyIndex)
	{
		NumShapes += PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom.GetElementCount();
	}

	if(Aggregate == NULL && NumShapes > RagdollAggregateThreshold && NumShapes <= AggregateMaxSize)
	{
		Aggregate = GPhysXSDK->createAggregate(PhysicsAsset->SkeletalBodySetups.Num(), true);
	}
	else if(Aggregate && NumShapes > AggregateMaxSize)
	{
		UE_LOG(LogSkeletalMesh, Log, TEXT("USkeletalMeshComponent::InitArticulated : Too many shapes to create aggregate, Max: %u, This: %d"), AggregateMaxSize, NumShapes);
	}
#endif //WITH_PHYSX

	InstantiatePhysicsAsset(*PhysicsAsset, Scale3D, Bodies, Constraints, PhysScene, this, RootBodyIndex, Aggregate);

	// now update root body index because body has BodySetup now
	SetRootBodyIndex(RootBodyIndex);


	// Update Flag
#if WITH_APEX_CLOTHING
	PrevRootBoneMatrix = GetBoneMatrix(0); // save the root bone transform

	// pre-compute cloth teleport thresholds for performance
	ComputeTeleportDistanceThresholdInRadians();
	ComputeTeleportRotationThresholdInRadians();
#endif // #if WITH_APEX_CLOTHING
}

TAutoConsoleVariable<int32> CVarEnableRagdollPhysics(TEXT("p.RagdollPhysics"), 1, TEXT("If 1, ragdoll physics will be used. Otherwise just root body is simulated"));

void USkeletalMeshComponent::InstantiatePhysicsAsset(const UPhysicsAsset& PhysAsset, const FVector& Scale3D, TArray<FBodyInstance*>& OutBodies, TArray<FConstraintInstance*>& OutConstraints, FPhysScene* PhysScene, USkeletalMeshComponent* OwningComponent, int32 UseRootBodyIndex, physx::PxAggregate* UseAggregate) const
{
	const float ActualScale = Scale3D.GetAbsMin();
	const float Scale = ActualScale == 0.f ? KINDA_SMALL_NUMBER : ActualScale;

	const int32 NumOutBodies = PhysAsset.SkeletalBodySetups.Num();

	TMap<FName, FBodyInstance*> NameToBodyMap;

	// Create all the OutBodies.
	check(OutBodies.Num() == 0);
	OutBodies.AddZeroed(NumOutBodies);

	for (int32 BodyIdx = 0; BodyIdx<NumOutBodies; BodyIdx++)
	{
		UBodySetup* PhysicsAssetBodySetup = PhysAsset.SkeletalBodySetups[BodyIdx];
		OutBodies[BodyIdx] = new FBodyInstance;
		FBodyInstance* BodyInst = OutBodies[BodyIdx];
		check(BodyInst);

		// Get transform of bone by name.
		int32 BoneIndex = GetBoneIndex(PhysicsAssetBodySetup->BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			// Copy body setup default instance properties
			BodyInst->CopyBodyInstancePropertiesFrom(&PhysicsAssetBodySetup->DefaultInstance);
			// we don't allow them to use this in editor. For physics asset, this set up is overridden by Physics Type. 
			// but before we hide in the detail customization, we saved with this being true, causing the simulate always happens for some OutBodies
			// so adding initialization here to disable this. 
			// to check, please check BodySetupDetails.cpp, if (ChildProperty->GetProperty()->GetName() == TEXT("bSimulatePhysics"))
			// we hide this property, so it should always be false initially. 
			// this is not true for all other BodyInstance, but for physics assets it is true. 
			BodyInst->bSimulatePhysics = false;
			BodyInst->InstanceBodyIndex = BodyIdx; // Set body index 
			BodyInst->InstanceBoneIndex = BoneIndex; // Set bone index

			BodyInst->bStartAwake = UseRootBodyIndex >= 0 ? BodyInstance.bStartAwake : true;	//We don't allow customization here. Just use whatever the component is set to

			if (BodyIdx == UseRootBodyIndex)
			{
				BodyInst->DOFMode = BodyInstance.DOFMode;
				BodyInst->CustomDOFPlaneNormal = BodyInstance.CustomDOFPlaneNormal;
				BodyInst->bLockXTranslation = BodyInstance.bLockXTranslation;
				BodyInst->bLockYTranslation = BodyInstance.bLockYTranslation;
				BodyInst->bLockZTranslation = BodyInstance.bLockZTranslation;
				BodyInst->bLockXRotation = BodyInstance.bLockXRotation;
				BodyInst->bLockYRotation = BodyInstance.bLockYRotation;
				BodyInst->bLockZRotation = BodyInstance.bLockZRotation;
				BodyInst->bLockTranslation = BodyInstance.bLockTranslation;
				BodyInst->bLockRotation = BodyInstance.bLockRotation;

				BodyInst->COMNudge = BodyInstance.COMNudge;
			}
			else
			{
				BodyInst->DOFMode = EDOFMode::None;
				if (PhysScene != nullptr && !CVarEnableRagdollPhysics.GetValueOnGameThread())	//We only limit creation of the global physx scenes and not assets related to immedate mode
				{
					continue;
				}
			}

#if WITH_PHYSX
			// Create physics body instance.
			FTransform BoneTransform = GetBoneTransform(BoneIndex);
			FBodyInstance::FInitBodySpawnParams SpawnParams(OwningComponent);
			SpawnParams.DynamicActorScene = UseAsyncScene;

			if(OwningComponent == nullptr)
			{
				//special case where we don't use the skel mesh, but we still want to do certain logic like skeletal mesh
				SpawnParams.bStaticPhysics = false;
				SpawnParams.bPhysicsTypeDeterminesSimulation = true;
			}

			BodyInst->InitBody(PhysicsAssetBodySetup, BoneTransform, OwningComponent, PhysScene, SpawnParams, UseAggregate);

			NameToBodyMap.Add(PhysicsAssetBodySetup->BoneName, BodyInst);
#endif //WITH_PHYSX
		}
	}

#if WITH_PHYSX
	if (PhysScene && Aggregate)
	{
		// Get the scene type from the SkeletalMeshComponent's BodyInstance
		const uint32 SceneType = GetPhysicsSceneType(PhysAsset, *PhysScene, UseAsyncScene);
		PxScene* PScene = PhysScene->GetPhysXScene(SceneType);
		SCOPED_SCENE_WRITE_LOCK(PScene);
		// add Aggregate into the scene
		if (Aggregate && Aggregate->getNbActors() > 0)
		{
			PScene->addAggregate(*Aggregate);
		}
	}

#endif //WITH_PHYSX

	// Create all the OutConstraints.
	check(OutConstraints.Num() == 0);
	int32 NumOutConstraints = PhysAsset.ConstraintSetup.Num();
	OutConstraints.AddZeroed(NumOutConstraints);
	for (int32 ConstraintIdx = 0; ConstraintIdx<NumOutConstraints; ConstraintIdx++)
	{
		const UPhysicsConstraintTemplate* OutConstraintsetup = PhysAsset.ConstraintSetup[ConstraintIdx];
		OutConstraints[ConstraintIdx] = new FConstraintInstance;
		FConstraintInstance* ConInst = OutConstraints[ConstraintIdx];
		check(ConInst);
		ConInst->CopyConstraintParamsFrom(&OutConstraintsetup->DefaultInstance);
		ConInst->ConstraintIndex = ConstraintIdx; // Set the ConstraintIndex property in the ConstraintInstance.
#if WITH_EDITOR
		if (GetWorld()->IsGameWorld())
		{
			//In the editor we may be currently editing the physics asset, so make sure to use the default profile
			OutConstraintsetup->ApplyConstraintProfile(NAME_None, *ConInst, /*bDefaultIfNotFound=*/true);
		}
#endif

		// Get OutBodies we want to joint
		FBodyInstance* Body1 = NameToBodyMap.FindRef(ConInst->ConstraintBone1);
		FBodyInstance* Body2 = NameToBodyMap.FindRef(ConInst->ConstraintBone2);

		// If we have 2, joint 'em
		if (Body1 && Body2)
		{
			// Validates the body. OutBodies could be invalid due to outdated PhysAssets / bad constraint bone (or body) names.
			auto ValidateBody = [this](const FBodyInstance* InBody, const FName& InBoneName)
			{
				if (!InBody->IsValidBodyInstance())
				{
					// Disable log for now.
					// UE_LOG(LogSkeletalMesh, Warning, TEXT("USkeletalMeshComponent::InitArticulated : Unable to initialize constraint (%s) -  Body Invalid %s."), *(this->GetPathName()), *(InBoneName.ToString()));
					return false;
				}

				return true;
			};

			// Applies the adjusted / relative scale of the body instance.
			// Also, remove component scale as it will be reapplied in InitConstraint.
			// GetBoneTransform already accounts for component scale.
			auto ScalePosition = [](const FBodyInstance* InBody, const float InScale, FVector& OutPosition)
			{
				const FBodyInstance& DefaultBody = InBody->BodySetup.Get()->DefaultInstance;
				const FVector ScaledDefaultBodyScale = DefaultBody.Scale3D * InScale;
				const FVector AdjustedBodyScale = InBody->Scale3D * ScaledDefaultBodyScale.Reciprocal();
				OutPosition *= AdjustedBodyScale;
			};

			// Do this separately so both are logged if invalid.
			const bool Body1Valid = ValidateBody(Body1, ConInst->ConstraintBone1);
			const bool Body2Valid = ValidateBody(Body2, ConInst->ConstraintBone2);

			if (Body1Valid && Body2Valid)
			{
				ScalePosition(Body1, Scale, ConInst->Pos1);
				ScalePosition(Body2, Scale, ConInst->Pos2);
				ConInst->InitConstraint(Body1, Body2, Scale, OwningComponent, OwningComponent ? FOnConstraintBroken::CreateUObject(OwningComponent, &USkeletalMeshComponent::OnConstraintBrokenWrapper) : FOnConstraintBroken());
			}
		}
	}
}


void USkeletalMeshComponent::TermArticulated()
{
	ResetRootBodyIndex();

#if WITH_PHYSX
	uint32 SkelMeshCompID = GetUniqueID();
	UWorld* MyWorld = GetWorld();
	FPhysScene* PhysScene = (MyWorld ? MyWorld->GetPhysicsScene() : nullptr);
	if (PhysScene)
	{
		PhysScene->DeferredRemoveCollisionDisableTable(SkelMeshCompID);
		// Clear from deferred kinematic update set
		PhysScene->ClearPreSimKinematicUpdate(this);
	}

	// Get the scene type from the SkeletalMeshComponent's BodyInstance
	const uint32 SceneType = BodyInstance.UseAsyncScene(PhysScene) ? PST_Async : PST_Sync;
	PxScene* PScene = PhysScene ? PhysScene->GetPhysXScene(SceneType) : nullptr;
	SCOPED_SCENE_WRITE_LOCK(PScene);

#endif	//#if WITH_PHYSX

	// We shut down the physics for each body and constraint here. 
	// The actual UObjects will get GC'd

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		check( Constraints[i] );
		Constraints[i]->TermConstraint();
		delete Constraints[i];
	}

	Constraints.Empty();

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		check( Bodies[i] );
		Bodies[i]->TermBody();
		delete Bodies[i];
	}
	
	Bodies.Empty();

#if WITH_PHYSX
	// releasing Aggregate, it shouldn't contain any Bodies now, because they are released above
	if(Aggregate)
	{
		check(!Aggregate->getNbActors());
		Aggregate->release();
		Aggregate = NULL;
	}
#endif //WITH_PHYSX
}

uint32 USkeletalMeshComponent::GetPhysicsSceneType(const UPhysicsAsset& PhysAsset, const FPhysScene& PhysScene, EDynamicActorScene SimulationScene)
{
	bool bUseAsync = SimulationScene == EDynamicActorScene::Default ? PhysAsset.bUseAsyncScene : (SimulationScene == EDynamicActorScene::UseAsyncScene);
	return (bUseAsync && PhysScene.HasAsyncScene()) ? PST_Async : PST_Sync;
}

void USkeletalMeshComponent::TermBodiesBelow(FName ParentBoneName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && SkeletalMesh && Bodies.Num() > 0)
	{
		check(Bodies.Num() == PhysicsAsset->SkeletalBodySetups.Num());

		// Get index of parent bone
		int32 ParentBoneIndex = GetBoneIndex(ParentBoneName);
		if(ParentBoneIndex == INDEX_NONE)
		{
			UE_LOG(LogSkeletalMesh, Log, TEXT("TermBodiesBelow: ParentBoneName '%s' is invalid"), *ParentBoneName.ToString());
			return;
		}

		// First terminate any constraints at below this bone
		for(int32 i=0; i<Constraints.Num(); i++)
		{
			// Get bone index of constraint
			FName JointName = Constraints[i]->JointName;
			int32 JointBoneIndex = GetBoneIndex(JointName);

			// If constraint has bone in mesh, and is either the parent or child of it, term it
			if(	JointBoneIndex != INDEX_NONE && (JointName == ParentBoneName ||	SkeletalMesh->RefSkeleton.BoneIsChildOf(JointBoneIndex, ParentBoneIndex)) )
			{
				Constraints[i]->TermConstraint();
			}
		}

		// Then iterate over bodies looking for any which are children of supplied parent
		for(int32 i=0; i<Bodies.Num(); i++)
		{
			// Get bone index of body
			if (Bodies[i]->IsValidBodyInstance())
			{
				FName BodyName = Bodies[i]->BodySetup->BoneName;
				int32 BodyBoneIndex = GetBoneIndex(BodyName);

				// If body has bone in mesh, and is either the parent or child of it, term it
				if(	BodyBoneIndex != INDEX_NONE && (BodyName == ParentBoneName ||	SkeletalMesh->RefSkeleton.BoneIsChildOf(BodyBoneIndex, ParentBoneIndex)) )
				{
					Bodies[i]->TermBody();
				}
			}
		}
	}
}

float USkeletalMeshComponent::GetTotalMassBelowBone(FName InBoneName)
{
	float TotalMass = 0.f;

	ForEachBodyBelow(InBoneName, /*bIncludeSelf=*/true, /*bSkipCustomPhysics=*/false, [&TotalMass](FBodyInstance* BI)
	{
		TotalMass += BI->GetBodyMass();
	});

	return TotalMass;
}

void USkeletalMeshComponent::SetAllBodiesSimulatePhysics(bool bNewSimulate)
{
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceSimulatePhysics(bNewSimulate);
	}

	SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();
}

void USkeletalMeshComponent::SetCollisionObjectType(ECollisionChannel NewChannel)
{
	SetAllBodiesCollisionObjectType(NewChannel);
}

void USkeletalMeshComponent::SetAllBodiesCollisionObjectType(ECollisionChannel NewChannel)
{
	BodyInstance.SetObjectType(NewChannel);	//children bodies use the skeletal mesh override so make sure root is set properly

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetObjectType(NewChannel);
	}
}

void USkeletalMeshComponent::SetAllBodiesNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	BodyInstance.SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision); //children bodies use the skeletal mesh override so make sure root is set properly

	for(int32 i=0; i<Bodies.Num(); i++)
	{
		Bodies[i]->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	}
}


void USkeletalMeshComponent::SetAllBodiesBelowSimulatePhysics( const FName& InBoneName, bool bNewSimulate, bool bIncludeSelf )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, bIncludeSelf, /*bSkipCustomPhysicsType=*/ false, [bNewSimulate](FBodyInstance* BI)
	{
		BI->SetInstanceSimulatePhysics(bNewSimulate);
	});

	if (NumBodiesFound)
	{
		if (IsSimulatingPhysics())
		{
			SetRootBodyIndex(RootBodyData.BodyIndex);	//Update the root body data cache in case animation has moved root body relative to root joint
		}

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}


void USkeletalMeshComponent::SetAllMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default)
			{
				continue;
			}
		}

		Constraints[i]->SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularPositionDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->JointName) )
		{
			Constraints[i]->SetOrientationDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetOrientationDriveTwistAndSwing(!bEnableTwistDrive, !bEnableSwingDrive);
		}
	}
}

void USkeletalMeshComponent::SetNamedMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, const TArray<FName>& BoneNames, bool bSetOtherBodiesToComplement)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		FConstraintInstance* Instance = Constraints[i];
		if( BoneNames.Contains(Instance->JointName) )
		{
			Constraints[i]->SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
		}
		else if( bSetOtherBodiesToComplement )
		{
			Constraints[i]->SetAngularVelocityDriveTwistAndSwing(!bEnableTwistDrive, !bEnableSwingDrive);
		}
	}
}

void USkeletalMeshComponent::SetAllMotorsAngularVelocityDrive(bool bEnableSwingDrive, bool bEnableTwistDrive, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}

		Constraints[i]->SetAngularVelocityDriveTwistAndSwing(bEnableTwistDrive, bEnableSwingDrive);
	}
}

void USkeletalMeshComponent::SetConstraintProfile(FName JointName, FName ProfileName, bool bDefaultIfNotFound)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	for (int32 i = 0; i < Constraints.Num(); i++)
	{
		FConstraintInstance* ConstraintInstance = Constraints[i];
		if(ConstraintInstance->JointName == JointName)
		{
			PhysicsAsset->ConstraintSetup[i]->ApplyConstraintProfile(ProfileName, *ConstraintInstance, bDefaultIfNotFound);
		}
	}
}

void USkeletalMeshComponent::SetConstraintProfileForAll(FName ProfileName, bool bDefaultIfNotFound)
{
	if(UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
	{
		for (int32 i = 0; i < Constraints.Num(); i++)
		{
			if(FConstraintInstance* ConstraintInstance = Constraints[i])
			{
				PhysicsAsset->ConstraintSetup[i]->ApplyConstraintProfile(ProfileName, *ConstraintInstance, bDefaultIfNotFound);
			}
		}
	}
}

void USkeletalMeshComponent::SetAllMotorsAngularDriveParams(float InSpring, float InDamping, float InForceLimit, bool bSkipCustomPhysicsType)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	for(int32 i=0; i<Constraints.Num(); i++)
	{
		if( bSkipCustomPhysicsType )
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(Constraints[i]->JointName);
			if( BodyIndex != INDEX_NONE && PhysicsAsset->SkeletalBodySetups[BodyIndex]->PhysicsType != PhysType_Default )
			{
				continue;
			}
		}
		Constraints[i]->SetAngularDriveParams(InSpring, InDamping, InForceLimit);
	}
}

void USkeletalMeshComponent::ResetAllBodiesSimulatePhysics()
{
	if ( !bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer() )
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	if(CollisionEnabledHasPhysics(GetCollisionEnabled()))
	{
		// Fix / Unfix bones
		for (int32 i = 0; i<Bodies.Num(); i++)
		{
			FBodyInstance*	BodyInst = Bodies[i];
			UBodySetup*	BodyInstSetup = BodyInst->BodySetup.Get();

			// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
			if (BodyInstSetup && BodyInstSetup->PhysicsType != PhysType_Default)
			{
				if (BodyInstSetup->PhysicsType == PhysType_Simulated)
				{
					BodyInst->SetInstanceSimulatePhysics(true);
				}
				else
				{
					BodyInst->SetInstanceSimulatePhysics(false);
				}
			}
		}
	}
}

void USkeletalMeshComponent::SetEnablePhysicsBlending(bool bNewBlendPhysics)
{
	bBlendPhysics = bNewBlendPhysics;
}

void USkeletalMeshComponent::SetPhysicsBlendWeight(float PhysicsBlendWeight)
{
	bool bShouldSimulate = PhysicsBlendWeight > 0.f;
	if (bShouldSimulate != IsSimulatingPhysics())
	{
		SetSimulatePhysics(bShouldSimulate);
	}

	// if blend weight is not 1, set manual weight
	if ( PhysicsBlendWeight < 1.f )
	{
		bBlendPhysics = false;
		SetAllBodiesPhysicsBlendWeight (PhysicsBlendWeight, true);
	}
}

void USkeletalMeshComponent::SetAllBodiesPhysicsBlendWeight(float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if( !PhysicsAsset )
	{
		return;
	}

	// Fix / Unfix bones
	for(int32 i=0; i<Bodies.Num(); i++)
	{
		FBodyInstance*	BodyInst	= Bodies[i];
		UBodySetup*	BodyInstSetup	= BodyInst->BodySetup.Get();

		// Set fixed on any bodies with bAlwaysFullAnimWeight set to true
		if(BodyInstSetup && (!bSkipCustomPhysicsType || BodyInstSetup->PhysicsType == PhysType_Default) )
		{
			BodyInst->PhysicsBlendWeight = PhysicsBlendWeight;
		}
	}

	bBlendPhysics = false;

	UpdateEndPhysicsTickRegisteredState();
	UpdateClothTickRegisteredState();
}


void USkeletalMeshComponent::SetAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType, bool bIncludeSelf )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, bIncludeSelf, bSkipCustomPhysicsType, [PhysicsBlendWeight](FBodyInstance* BI)
	{
		BI->PhysicsBlendWeight = PhysicsBlendWeight;
	});

	if (NumBodiesFound)
	{
		bBlendPhysics = false;

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}


void USkeletalMeshComponent::AccumulateAllBodiesBelowPhysicsBlendWeight( const FName& InBoneName, float PhysicsBlendWeight, bool bSkipCustomPhysicsType )
{
	int32 NumBodiesFound = ForEachBodyBelow(InBoneName, /*bIncludeSelf=*/ true, /*bSkipCustomPhysicsType=*/ bSkipCustomPhysicsType, [PhysicsBlendWeight](FBodyInstance* BI)
	{
		BI->PhysicsBlendWeight = FMath::Min(BI->PhysicsBlendWeight + PhysicsBlendWeight, 1.f);
	});

	if (NumBodiesFound)
	{
		bBlendPhysics = false;

		UpdateEndPhysicsTickRegisteredState();
		UpdateClothTickRegisteredState();
	}
}

FConstraintInstance* USkeletalMeshComponent::FindConstraintInstance(FName ConName)
{
	UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
	if(PhysicsAsset && PhysicsAsset->ConstraintSetup.Num() == Constraints.Num())
	{
		int32 ConIndex = PhysicsAsset->FindConstraintIndex(ConName);
		if(ConIndex != INDEX_NONE)
		{
			return Constraints[ConIndex];
		}
	}

	return NULL;
}

void USkeletalMeshComponent::AddForceToAllBodiesBelow(FVector Force, FName BoneName, bool bAccelChange, bool bIncludeSelf)
{
	ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomPhysics=*/false, [Force, bAccelChange](FBodyInstance* BI)
	{
		BI->AddForce(Force, /*bAllowSubstepping=*/ true, bAccelChange);
	});
}

void USkeletalMeshComponent::AddImpulseToAllBodiesBelow(FVector Impulse, FName BoneName, bool bVelChange, bool bIncludeSelf)
{
	ForEachBodyBelow(BoneName, bIncludeSelf,/*bSkipCustomPhysics=*/false, [Impulse, bVelChange](FBodyInstance* BI)
	{
		BI->AddImpulse(Impulse, bVelChange);
	});
}

#ifndef OLD_FORCE_UPDATE_BEHAVIOR
#define OLD_FORCE_UPDATE_BEHAVIOR 0
#endif

void USkeletalMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	// We are handling the physics move below, so don't handle it at higher levels
	Super::OnUpdateTransform(UpdateTransformFlags | EUpdateTransformFlags::SkipPhysicsUpdate, Teleport);

	// Always send new transform to physics
	if(bPhysicsStateCreated && !(UpdateTransformFlags&EUpdateTransformFlags::SkipPhysicsUpdate))
	{
#if !OLD_FORCE_UPDATE_BEHAVIOR
		UpdateKinematicBonesToAnim(GetComponentSpaceTransforms(), Teleport, false);
#else
		UpdateKinematicBonesToAnim(GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, false);
#endif
	}

	if(ClothingSimulation && ClothingSimulation->ShouldSimulate())
	{
		UpdateClothTransform(Teleport);
	}
}

void USkeletalMeshComponent::UpdateOverlaps(TArray<FOverlapInfo> const* PendingOverlaps, bool bDoNotifies, const TArray<FOverlapInfo>* OverlapsAtEndLocation)
{
	// Parent class (USkinnedMeshComponent) routes only to children, but we really do want to test our own bodies for overlaps.
	UPrimitiveComponent::UpdateOverlaps(PendingOverlaps, bDoNotifies, OverlapsAtEndLocation);
}

bool USkeletalMeshComponent::ShouldCreatePhysicsState() const
{
	bool bShouldCreatePhysicsState = Super::ShouldCreatePhysicsState();
	bShouldCreatePhysicsState &= (MasterPoseComponent.IsValid() == false);
	
	return bShouldCreatePhysicsState;
}

void USkeletalMeshComponent::OnCreatePhysicsState()
{
	// Init physics
	if (bEnablePerPolyCollision == false)
	{
		InitArticulated(GetWorld()->GetPhysicsScene());
		USceneComponent::OnCreatePhysicsState(); // Need to route CreatePhysicsState, skip PrimitiveComponent
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SendRenderDebugPhysics();
#endif
	}
	else
	{
		CreateBodySetup();
		BodySetup->CreatePhysicsMeshes();
		Super::OnCreatePhysicsState();	//If we're doing per poly we'll use the body instance of the primitive component
	}

	// Notify physics created
	OnSkelMeshPhysicsCreated.Broadcast();
}


void USkeletalMeshComponent::OnDestroyPhysicsState()
{
	if (bEnablePerPolyCollision == false)
	{
		UnWeldFromParent();
		UnWeldChildren();
		TermArticulated();
	}

	Super::OnDestroyPhysicsState();
}



#if 0 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define DEBUGBROKENCONSTRAINTUPDATE(x) { ##x }
#else
#define DEBUGBROKENCONSTRAINTUPDATE(x)
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void USkeletalMeshComponent::SendRenderDebugPhysics(FPrimitiveSceneProxy* OverrideSceneProxy)
{
	FPrimitiveSceneProxy* UseSceneProxy = OverrideSceneProxy ? OverrideSceneProxy : SceneProxy;
	if (UseSceneProxy)
	{
		TArray<FPrimitiveSceneProxy::FDebugMassData> DebugMassData;
		DebugMassData.Reserve(Bodies.Num());
		
		for (FBodyInstance* BI : Bodies)
		{
			if (BI && BI->IsValidBodyInstance())
			{
				const int32 BoneIndex = BI->InstanceBoneIndex;
				DebugMassData.AddDefaulted();
				FPrimitiveSceneProxy::FDebugMassData& MassData = DebugMassData.Last();
				const FTransform MassToWorld = BI->GetMassSpaceToWorldSpace();
				const FTransform& BoneTM = GetComponentSpaceTransforms()[BoneIndex];
				const FTransform BoneToWorld = BoneTM * GetComponentTransform();

				MassData.LocalCenterOfMass = BoneToWorld.InverseTransformPosition(MassToWorld.GetLocation());
				MassData.LocalTensorOrientation = MassToWorld.GetRotation() * BoneToWorld.GetRotation().Inverse();
				MassData.MassSpaceInertiaTensor = BI->GetBodyInertiaTensor();
				MassData.BoneIndex = BoneIndex;
			}
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SkeletalMesh_SendRenderDebugPhysics, FPrimitiveSceneProxy*, PassedSceneProxy, UseSceneProxy, TArray<FPrimitiveSceneProxy::FDebugMassData>, UseDebugMassData, DebugMassData,
			{
				PassedSceneProxy->SetDebugMassData(UseDebugMassData);
			}
		);
		
	}
}
#endif

void USkeletalMeshComponent::UpdateMeshForBrokenConstraints()
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	// Needs to have a SkeletalMesh, and PhysicsAsset.
	if( !SkeletalMesh || !PhysicsAsset )
	{
		return;
	}

	DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("%3.3f UpdateMeshForBrokenConstraints"), GetWorld()->GetTimeSeconds());)

	// Iterate through list of constraints in the physics asset
	for(int32 ConstraintInstIndex = 0; ConstraintInstIndex < Constraints.Num(); ConstraintInstIndex++)
	{
		// See if we can find a constraint that has been terminated (broken)
		FConstraintInstance* ConstraintInst = Constraints[ConstraintInstIndex];
		if( ConstraintInst && ConstraintInst->IsTerminated() )
		{
			// Get the associated joint bone index.
			int32 JointBoneIndex = GetBoneIndex(ConstraintInst->JointName);
			if( JointBoneIndex == INDEX_NONE )
			{
				continue;
			}

			DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("  Found Broken Constraint: (%d) %s"), JointBoneIndex, *PhysicsAsset->ConstraintSetup(ConstraintInstIndex)->JointName.ToString());)

			// Get child bodies of this joint
			for(int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodySetupIndex++)
			{
				UBodySetup* PhysicsAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodySetupIndex];
				int32 BoneIndex = GetBoneIndex(PhysicsAssetBodySetup->BoneName);
				if( BoneIndex != INDEX_NONE && 
					(BoneIndex == JointBoneIndex || SkeletalMesh->RefSkeleton.BoneIsChildOf(BoneIndex, JointBoneIndex)) )
				{
					DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("    Found Child Bone: (%d) %s"), BoneIndex, *PhysicsAssetBodySetup->BoneName.ToString());)

					FBodyInstance* ChildBodyInst = Bodies[BodySetupIndex];
					if( ChildBodyInst )
					{
						// Unfix Body so, it is purely physical, not kinematic.
						if( !ChildBodyInst->IsInstanceSimulatingPhysics() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Unfixing body."));)
							ChildBodyInst->SetInstanceSimulatePhysics(true);
						}
					}

					FConstraintInstance* ChildConstraintInst = FindConstraintInstance(PhysicsAssetBodySetup->BoneName);
					if( ChildConstraintInst )
					{
						if( ChildConstraintInst->IsLinearPositionDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearPositionDrive."));)
							ChildConstraintInst->SetLinearPositionDrive(false, false, false);
						}
						if( ChildConstraintInst->IsLinearVelocityDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off LinearVelocityDrive."));)
							ChildConstraintInst->SetLinearVelocityDrive(false, false, false);
						}
						if( ChildConstraintInst->IsAngularOrientationDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularPositionDrive."));)
							ChildConstraintInst->SetOrientationDriveTwistAndSwing(false, false);
						}
						if( ChildConstraintInst->IsAngularVelocityDriveEnabled() )
						{
							DEBUGBROKENCONSTRAINTUPDATE(UE_LOG(LogSkeletalMesh, Log, TEXT("      Turning off AngularVelocityDrive."));)
							ChildConstraintInst->SetAngularVelocityDriveTwistAndSwing(false, false);
						}
					}
				}
			}
		}
	}
}


int32 USkeletalMeshComponent::FindConstraintIndex( FName ConstraintName )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintIndex(ConstraintName) : INDEX_NONE;
}


FName USkeletalMeshComponent::FindConstraintBoneName( int32 ConstraintIndex )
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	return PhysicsAsset ? PhysicsAsset->FindConstraintBoneName(ConstraintIndex) : NAME_None;
}


FBodyInstance* USkeletalMeshComponent::GetBodyInstance(FName BoneName, bool) const
{
	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();
	FBodyInstance* BodyInst = NULL;

	if(PhysicsAsset != NULL)
	{
		// A name of NAME_None indicates 'root body'
		if(BoneName == NAME_None)
		{
			if(Bodies.IsValidIndex(RootBodyData.BodyIndex))
			{
				BodyInst = Bodies[RootBodyData.BodyIndex];
			}
		}
		// otherwise, look for the body
		else
		{
			int32 BodyIndex = PhysicsAsset->FindBodyIndex(BoneName);
			if(Bodies.IsValidIndex(BodyIndex))
			{
				BodyInst = Bodies[BodyIndex];
			}
		}

	}

	return BodyInst;
}

void USkeletalMeshComponent::GetWeldedBodies(TArray<FBodyInstance*> & OutWeldedBodies, TArray<FName> & OutLabels, bool bIncludingAutoWeld)
{
	UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();

	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		FBodyInstance* BI = Bodies[BodyIdx];
		if (BI && (BI->WeldParent != nullptr || (bIncludingAutoWeld && BI->bAutoWeld)))
		{
			OutWeldedBodies.Add(BI);
			if (PhysicsAsset)
			{
				if (UBodySetup * PhysicsAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIdx])
				{
					OutLabels.Add(PhysicsAssetBodySetup->BoneName);
				}
				else
				{
					OutLabels.Add(NAME_None);
				}
			}
			else
			{
				OutLabels.Add(NAME_None);
			}

			for (USceneComponent * Child : GetAttachChildren())
			{
				if (UPrimitiveComponent * PrimChild = Cast<UPrimitiveComponent>(Child))
				{
					PrimChild->GetWeldedBodies(OutWeldedBodies, OutLabels, bIncludingAutoWeld);
				}
			}
		}
	}
}

int32 USkeletalMeshComponent::ForEachBodyBelow(FName BoneName, bool bIncludeSelf, bool bSkipCustomType, TFunctionRef<void(FBodyInstance*)> Func)
{
	if (BoneName == NAME_None && bIncludeSelf && !bSkipCustomType)
	{
		for (FBodyInstance* BI : Bodies)	//we want all bodies so just iterate the regular array
		{
			Func(BI);
		}

		return Bodies.Num();
	}
	else
	{
		UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();
		if (!PhysicsAsset || !SkeletalMesh)
		{
			return 0;
		}

		// if physics state is invalid - i.e. collision is disabled - or it does not have a valid bodies, this will crash right away
		if (!IsPhysicsStateCreated() || !bHasValidBodies)
		{
			FMessageLog("PIE").Warning(LOCTEXT("InvalidBodies", "Invalid Bodies : Make sure collision is enabled or root bone has body in PhysicsAsset."));
			return 0;
		}

		TArray<int32> BodyIndices;
		BodyIndices.Reserve(Bodies.Num());
		PhysicsAsset->GetBodyIndicesBelow(BodyIndices, BoneName, SkeletalMesh, bIncludeSelf);

		int32 NumBodiesFound = 0;
		for (int32 BodyIdx : BodyIndices)
		{
			FBodyInstance* BI = Bodies[BodyIdx];
			if (bSkipCustomType)
			{
				if (UBodySetup* PhysAssetBodySetup = PhysicsAsset->SkeletalBodySetups[BodyIdx])
				{
					if (PhysAssetBodySetup->PhysicsType != EPhysicsType::PhysType_Default)
					{
						continue;
					}
				}
			}

			++NumBodiesFound;
			Func(BI);
		}

		return NumBodiesFound;
	}

	return 0;
}

void USkeletalMeshComponent::SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision)
{
	for(FBodyInstance* BI : Bodies)
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	}

	if(Bodies.Num() > 0)
	{
		OnComponentCollisionSettingsChanged();
	}
}

void USkeletalMeshComponent::SetBodyNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision, FName BoneName /* = NAME_None */)
{
	if(FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);

		OnComponentCollisionSettingsChanged();
	}
}

void USkeletalMeshComponent::SetNotifyRigidBodyCollisionBelow(bool bNewNotifyRigidBodyCollision, FName BoneName, bool bIncludeSelf)
{
	const int32 NumBodiesFound = ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomType=*/false, [bNewNotifyRigidBodyCollision](FBodyInstance* BI)
	{
		BI->SetInstanceNotifyRBCollision(bNewNotifyRigidBodyCollision);
	});
	
	if(NumBodiesFound > 0)
	{
		OnComponentCollisionSettingsChanged();
	}
}

void USkeletalMeshComponent::BreakConstraint(FVector Impulse, FVector HitLocation, FName InBoneName)
{
	// you can enable/disable the instanced weights by calling
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if( ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num() )
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if( Constraint->IsTerminated() )
	{
		return;
	}

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// Figure out if Body is fixed or not
	FBodyInstance* Body = GetBodyInstance(Constraint->JointName);

	if( Body != NULL && Body->IsInstanceSimulatingPhysics() )
	{
		// Unfix body so it can be broken.
		Body->SetInstanceSimulatePhysics(true);
	}

	// Break Constraint
	Constraint->TermConstraint();
	// Make sure child bodies and constraints are released and turned to physics.
	UpdateMeshForBrokenConstraints();
	// Add impulse to broken limb
	AddImpulseAtLocation(Impulse, HitLocation, InBoneName);
}


void USkeletalMeshComponent::SetAngularLimits(FName InBoneName, float Swing1LimitAngle, float TwistLimitAngle, float Swing2LimitAngle)
{
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if (ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num())
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	// If already broken, our job has already been done. Bail!
	if (Constraint->IsTerminated())
	{
		return;
	}

	UPhysicsAsset * const PhysicsAsset = GetPhysicsAsset();

	// Figure out if Body is fixed or not
	FBodyInstance* Body = GetBodyInstance(Constraint->JointName);

	if (Body != NULL && Body->IsInstanceSimulatingPhysics())
	{
		// Unfix body so it can be broken.
		Body->SetInstanceSimulatePhysics(true);
	}

	// update limits
	Constraint->SetAngularSwing1Limit(Swing1LimitAngle == 0 ? ACM_Locked : (Swing1LimitAngle >= 180) ? ACM_Free : ACM_Limited, Swing1LimitAngle);
	Constraint->SetAngularTwistLimit(TwistLimitAngle == 0 ? ACM_Locked : (TwistLimitAngle >= 180) ? ACM_Free : ACM_Limited, TwistLimitAngle);
	Constraint->SetAngularSwing2Limit(Swing2LimitAngle == 0 ? ACM_Locked : (Swing2LimitAngle >= 180) ? ACM_Free : ACM_Limited, Swing2LimitAngle);
}


void USkeletalMeshComponent::GetCurrentJointAngles(FName InBoneName, float &Swing1Angle, float &TwistAngle, float &Swing2Angle)
{
	int32 ConstraintIndex = FindConstraintIndex(InBoneName);
	if (ConstraintIndex == INDEX_NONE || ConstraintIndex >= Constraints.Num())
	{
		return;
	}

	FConstraintInstance* Constraint = Constraints[ConstraintIndex];
	
	Swing1Angle = FMath::RadiansToDegrees(Constraint->GetCurrentSwing1());
	Swing2Angle = FMath::RadiansToDegrees(Constraint->GetCurrentSwing2());
	TwistAngle = FMath::RadiansToDegrees(Constraint->GetCurrentTwist());
}


void USkeletalMeshComponent::SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset, bool bForceReInit)
{
	// If this is different from what we have now, or we should have an instance but for whatever reason it failed last time, teardown/recreate now.
	if(bForceReInit || InPhysicsAsset != GetPhysicsAsset())
	{
		// SkelComp had a physics instance, then terminate it.
		TermArticulated();

		// Need to update scene proxy, because it keeps a ref to the PhysicsAsset.
		Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);
		MarkRenderStateDirty();

		// Update bHasValidBodies flag
		UpdateHasValidBodies();

		// Component should be re-attached here, so create physics.
		if( SkeletalMesh )
		{
			// Because we don't know what bones the new PhysicsAsset might want, we have to force an update to _all_ bones in the skeleton.
			RequiredBones.Reset(SkeletalMesh->RefSkeleton.GetNum());
			RequiredBones.AddUninitialized( SkeletalMesh->RefSkeleton.GetNum() );
			for(int32 i=0; i<SkeletalMesh->RefSkeleton.GetNum(); i++)
			{
				RequiredBones[i] = (FBoneIndexType)i;
			}
			RefreshBoneTransforms();

			// Initialize new Physics Asset
			UWorld* World = GetWorld();
			if(World->GetPhysicsScene() != nullptr && ShouldCreatePhysicsState())
			{
			//	UE_LOG(LogSkeletalMesh, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());			
				InitArticulated(World->GetPhysicsScene());
			}
		}
		else
		{
			// If PhysicsAsset hasn't been instanced yet, just update the template.
			Super::SetPhysicsAsset(InPhysicsAsset, bForceReInit);

			// Update bHasValidBodies flag
			UpdateHasValidBodies();
		}

		// Indicate that 'required bones' array will need to be recalculated.
		bRequiredBonesUpToDate = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SendRenderDebugPhysics();
#endif
	}
}


void USkeletalMeshComponent::UpdateHasValidBodies()
{
	// First clear out old data
	bHasValidBodies = false;

	const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset();

	// If we have a physics asset..
	if(PhysicsAsset != NULL)
	{
		// For each body in physics asset..
		for( int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++ )
		{
			// .. find the matching graphics bone index
			int32 BoneIndex = GetBoneIndex( PhysicsAsset->SkeletalBodySetups[ BodyIndex ]->BoneName );

			// If we found a valid graphics bone, set the 'valid' flag
			if(BoneIndex != INDEX_NONE)
			{
				bHasValidBodies = true;
				break;
			}
		}
	}
}

void USkeletalMeshComponent::UpdateBoneBodyMapping()
{
	if (Bodies.Num() > 0)	//If using per poly then there's no bodies to update indices on
	{
		// If we have a physics asset..
		if (const UPhysicsAsset* const PhysicsAsset = GetPhysicsAsset())
		{
			bool bNeedsReInit = false;

			// For each body in physics asset..
			for (int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); BodyIndex++)
			{
				// .. find the matching graphics bone index
				int32 BoneIndex = GetBoneIndex(PhysicsAsset->SkeletalBodySetups[BodyIndex]->BoneName);
				FBodyInstance* Inst = Bodies[BodyIndex];
				check(Inst);

				// Make sure physics state matches presence of bone
				bool bHasValidBone = (BoneIndex != INDEX_NONE);
				if (bHasValidBone != Inst->IsValidBodyInstance())
				{
					// If not, we need to recreate physics asset to clean up bodies or create new ones
					bNeedsReInit = true;
				}

				Inst->InstanceBoneIndex = BoneIndex;
			}

			// If the set of bodies needs to change, we recreate physics asset
			if (bNeedsReInit)
			{
				RecreatePhysicsState();
			}
		}
	}
}

void USkeletalMeshComponent::UpdatePhysicsToRBChannels()
{
	// Iterate over each bone/body.
	for (int32 i = 0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		check(BI);
		BI->UpdatePhysicsFilterData();
	}
}

FVector USkeletalMeshComponent::GetSkinnedVertexPosition(int32 VertexIndex) const
{
	// only if this component has clothing and is showing simulated results	
	if (SkeletalMesh &&
		SkeletalMesh->MeshClothingAssets.Num() > 0 &&
		MeshObject &&
		!bDisableClothSimulation &&
		ClothBlendWeight > 0.0f // if cloth blend weight is 0.0, only showing skinned vertices regardless of simulation positions
		)
	{
		FStaticLODModel& Model = MeshObject->GetSkeletalMeshResource().LODModels[0];

		// Find the chunk and vertex within that chunk, and skinning type, for this vertex.
		int32 SectionIndex;
		int32 VertIndexInChunk;
		bool bHasExtraBoneInfluences;
		Model.GetSectionFromVertexIndex(VertexIndex, SectionIndex, VertIndexInChunk, bHasExtraBoneInfluences);

		bool bClothVertex = false;
		int32 ClothAssetIndex = -1;
		FGuid ClothAssetGuid;

		// if this section corresponds to a cloth section, returns corresponding cloth section's info instead
		const FSkelMeshSection& Section = Model.Sections[SectionIndex];

		// if this chunk has cloth data
		if (Section.HasClothingData())
		{
			bClothVertex = true;
			ClothAssetIndex = Section.CorrespondClothAssetIndex;
			ClothAssetGuid = Section.ClothingData.AssetGuid;
		}
		else
		{
			// if current section is disabled and the corresponding cloth section is visible
			if (Section.bDisabled && Section.CorrespondClothSectionIndex >= 0)
			{
				bClothVertex = true;

				const FSkelMeshSection& ClothSection = Model.Sections[Section.CorrespondClothSectionIndex];
				ClothAssetIndex = ClothSection.CorrespondClothAssetIndex;
				ClothAssetGuid = ClothSection.ClothingData.AssetGuid;

				// the index can exceed the range because this vertex index is based on the corresponding original section
				// the number of cloth chunk's vertices is not always same as the corresponding one 
				// cloth chunk has only soft vertices
				if (VertIndexInChunk >= ClothSection.GetNumVertices())
				{
					// if the index exceeds, re-assign a random vertex index for this chunk
					VertIndexInChunk = FMath::TruncToInt(FMath::SRand() * (ClothSection.GetNumVertices() - 1));
				}
			}
		}

		if (bClothVertex)
		{
			FVector SimulatedPos;
			if (GetClothSimulatedPosition_GameThread(ClothAssetGuid, VertIndexInChunk, SimulatedPos))
			{
				// a simulated position is in world space and convert this to local space
				// because SkinnedMeshComponent::GetSkinnedVertexPosition() returns the position in local space
				SimulatedPos = GetComponentTransform().InverseTransformPosition(SimulatedPos);

				// if blend weight is 1.0, doesn't need to blend with a skinned position
				if (ClothBlendWeight < 1.0f) 
				{
					// blend with a skinned position
					FVector SkinnedPos = Super::GetSkinnedVertexPosition(VertexIndex);
					SimulatedPos = SimulatedPos*ClothBlendWeight + SkinnedPos*(1.0f - ClothBlendWeight);
				}
				return SimulatedPos;
			}
		}
	}

	return Super::GetSkinnedVertexPosition(VertexIndex);
}

void USkeletalMeshComponent::SetEnableBodyGravity(bool bEnableGravity, FName BoneName)
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		BI->SetEnableGravity(bEnableGravity);
	}
}

bool USkeletalMeshComponent::IsBodyGravityEnabled(FName BoneName)
{
	const FBodyInstance* BI = GetBodyInstance(BoneName);
	return BI && BI->bEnableGravity;
}

void USkeletalMeshComponent::SetEnableGravityOnAllBodiesBelow(bool bEnableGravity, FName BoneName, bool bIncludeSelf)
{
    ForEachBodyBelow(BoneName, bIncludeSelf, /*bSkipCustomPhysics=*/false, [bEnableGravity](FBodyInstance* BI)
	{
		BI->SetEnableGravity(bEnableGravity);
	});
}

//////////////////////////////////////////////////////////////////////////
// COLLISION

extern float DebugLineLifetime;

bool USkeletalMeshComponent::GetSquaredDistanceToCollision(const FVector& Point, float& OutSquaredDistance, FVector& OutClosestPointOnCollision) const
{
	OutClosestPointOnCollision = Point;
	bool bHasResult = false;

	for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		FBodyInstance* BodyInst = Bodies[BodyIdx];
		if (BodyInst && BodyInst->IsValidBodyInstance() && (BodyInst->GetCollisionEnabled() != ECollisionEnabled::NoCollision))
		{
			FVector ClosestPoint;
			float DistanceSqr = -1.f;

			if (!Bodies[BodyIdx]->GetSquaredDistanceToBody(Point, DistanceSqr, ClosestPoint))
			{
				// Invalid result, impossible to be better than ClosestPointDistance
				continue;
			}

			if (!bHasResult || (DistanceSqr < OutSquaredDistance))
			{
				bHasResult = true;
				OutSquaredDistance = DistanceSqr;
				OutClosestPointOnCollision = ClosestPoint;

				// If we're inside collision, we're not going to find anything better, so abort search we've got our best find.
				if (DistanceSqr <= KINDA_SMALL_NUMBER)
				{
					break;
				}
			}
		}
	}

	return bHasResult;
}

DECLARE_CYCLE_STAT(TEXT("GetClosestPointOnPhysicsAsset"), STAT_GetClosestPointOnPhysicsAsset, STATGROUP_Physics);

bool USkeletalMeshComponent::GetClosestPointOnPhysicsAsset(const FVector& WorldPosition, FClosestPointOnPhysicsAsset& ClosestPointOnPhysicsAsset, bool bApproximate) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetClosestPointOnPhysicsAsset);

	bool bSuccess = false;
	const UPhysicsAsset* PhysicsAsset = GetPhysicsAsset();
	const FReferenceSkeleton* RefSkeleton = SkeletalMesh ? &SkeletalMesh->RefSkeleton : nullptr;
	if(PhysicsAsset && RefSkeleton)
	{
		const TArray<FTransform>& BoneTransforms = GetComponentSpaceTransforms();
		const bool bHasMasterPoseComponent = MasterPoseComponent.IsValid();
		const FVector ComponentPosition = GetComponentTransform().InverseTransformPosition(WorldPosition);
	
		float CurrentClosestDistance = FLT_MAX;
		int32 CurrentClosestBoneIndex = INDEX_NONE;
		const UBodySetup* CurrentClosestBodySetup = nullptr;

		for(const UBodySetup* BodySetupInstance : PhysicsAsset->SkeletalBodySetups)
		{
			ClosestPointOnPhysicsAsset.Distance = FLT_MAX;
			const FName BoneName = BodySetupInstance->BoneName;
			const int32 BoneIndex = RefSkeleton->FindBoneIndex(BoneName);
			if(BoneIndex != INDEX_NONE)
			{
				const FTransform BoneTM = bHasMasterPoseComponent ? GetBoneTransform(BoneIndex) : BoneTransforms[BoneIndex];
				const float Dist = bApproximate ? (BoneTM.GetLocation() - ComponentPosition).SizeSquared() : BodySetupInstance->GetShortestDistanceToPoint(ComponentPosition, BoneTM);

				if (Dist < CurrentClosestDistance)
				{
					CurrentClosestDistance = Dist;
					CurrentClosestBoneIndex = BoneIndex;
					CurrentClosestBodySetup = BodySetupInstance;

					if(Dist <= 0.f) { break; }
				}
			}
		}

		if(CurrentClosestBoneIndex >= 0)
		{
			bSuccess = true;

			const FTransform BoneTM = bHasMasterPoseComponent ? GetBoneTransform(CurrentClosestBoneIndex) : (BoneTransforms[CurrentClosestBoneIndex] * GetComponentTransform());
			ClosestPointOnPhysicsAsset.Distance = CurrentClosestBodySetup->GetClosestPointAndNormal(WorldPosition, BoneTM, ClosestPointOnPhysicsAsset.ClosestWorldPosition, ClosestPointOnPhysicsAsset.Normal);
			ClosestPointOnPhysicsAsset.BoneName = CurrentClosestBodySetup->BoneName;
		}
	}

	return bSuccess;
}

bool USkeletalMeshComponent::K2_GetClosestPointOnPhysicsAsset(const FVector& WorldPosition, FVector& ClosestWorldPosition, FVector& Normal, FName& BoneName, float& Distance) const
{
	FClosestPointOnPhysicsAsset ClosestPointOnPhysicsAsset;
	bool bSuccess = GetClosestPointOnPhysicsAsset(WorldPosition, ClosestPointOnPhysicsAsset, /*bApproximate =*/ false);
	if(bSuccess)
	{
		ClosestWorldPosition = ClosestPointOnPhysicsAsset.ClosestWorldPosition;
		Normal = ClosestPointOnPhysicsAsset.Normal;
		BoneName = ClosestPointOnPhysicsAsset.BoneName;
		Distance = ClosestPointOnPhysicsAsset.Distance;
	}
	else
	{
		ClosestWorldPosition = FVector::ZeroVector;
		Normal = FVector::ZeroVector;
		BoneName = NAME_None;
		Distance = -1;
	}

	return bSuccess;
}

bool USkeletalMeshComponent::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params)
{
	UWorld* const World = GetWorld();
	bool bHaveHit = false;

	float MinTime = MAX_FLT;
	FHitResult Hit;
	for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		if (Bodies[BodyIdx]->LineTrace(Hit, Start, End, Params.bTraceComplex, Params.bReturnPhysicalMaterial))
		{
			bHaveHit = true;
			if(MinTime > Hit.Time)
			{
				MinTime = Hit.Time;
				OutHit = Hit;
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(World && World->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveHit)
		{
			Hits.Add(OutHit);
		}
		DrawLineTraces(GetWorld(), Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return bHaveHit;
}

bool USkeletalMeshComponent::SweepComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FCollisionShape& CollisionShape, bool bTraceComplex)
{
	bool bHaveHit = false;

	for (int32 BodyIdx=0; BodyIdx < Bodies.Num(); ++BodyIdx)
	{
		if (Bodies[BodyIdx]->Sweep(OutHit, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex))
		{
			bHaveHit = true;
			break;
		}
	}

	return bHaveHit;
}

bool USkeletalMeshComponent::ComponentOverlapComponentImpl(class UPrimitiveComponent* PrimComp,const FVector Pos,const FQuat& Quat,const struct FCollisionQueryParams& Params)
{
	//we do not support skeletal mesh vs skeletal mesh overlap test
	if (PrimComp->IsA<USkeletalMeshComponent>())
	{
		UE_LOG(LogCollision, Warning, TEXT("ComponentOverlapComponent : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	if (FBodyInstance* BI = PrimComp->GetBodyInstance())
	{
		return BI->OverlapTestForBodies(Pos, Quat, Bodies);
	}

	return false;
}

bool USkeletalMeshComponent::OverlapComponent(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape)
{
	for (FBodyInstance* Body : Bodies)
	{
		if (Body->OverlapTest(Pos, Rot, CollisionShape))
		{
			return true;
		}
	}

	return false;
}

bool USkeletalMeshComponent::ComponentOverlapMultiImpl(TArray<struct FOverlapResult>& OutOverlaps, const UWorld* World, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	OutOverlaps.Reset();

	if (!Bodies.IsValidIndex(RootBodyData.BodyIndex))
	{
		return false;
	}

	const FTransform WorldToComponent(GetComponentTransform().Inverse());
	const FCollisionResponseParams ResponseParams(GetCollisionResponseToChannels());

	FComponentQueryParams ParamsWithSelf = Params;
	ParamsWithSelf.AddIgnoredComponent(this);

	bool bHaveBlockingHit = false;
	for (const FBodyInstance* Body : Bodies)
	{
		checkSlow(Body);
		if (Body->OverlapMulti(OutOverlaps, World, &WorldToComponent, Pos, Quat, TestChannel, ParamsWithSelf, ResponseParams, ObjectQueryParams))
		{
			bHaveBlockingHit = true;
		}
	}

	return bHaveBlockingHit;
}

void USkeletalMeshComponent::AddClothingBounds(FBoxSphereBounds& InOutBounds, const FTransform& LocalToWorld) const
{
	if(ClothingSimulation && ClothingSimulation->ShouldSimulate())
	{
		InOutBounds = InOutBounds + ClothingSimulation->GetBounds(this).TransformBy(LocalToWorld);
	}
}

void USkeletalMeshComponent::RecreateClothingActors()
{
	ReleaseAllClothingResources();

	if(SkeletalMesh == nullptr)
	{
		return;
	}

	if (bDisableClothSimulation)
	{
		return;
	}

	if(ClothingSimulation)
	{
		TArray<UClothingAssetBase*> AssetsInUse;
		SkeletalMesh->GetClothingAssetsInUse(AssetsInUse);

		const int32 NumMeshAssets = SkeletalMesh->MeshClothingAssets.Num();
		for(int32 BaseAssetIndex = 0; BaseAssetIndex < NumMeshAssets; ++BaseAssetIndex)
		{
			UClothingAssetBase* Asset = SkeletalMesh->MeshClothingAssets[BaseAssetIndex];

			if(!AssetsInUse.Contains(Asset))
			{
				continue;
			}

			ClothingSimulation->CreateActor(this, Asset, BaseAssetIndex);
		}

		WritebackClothingSimulationData();
	}
}

void USkeletalMeshComponent::RemoveAllClothingActors()
{
	if(ClothingSimulation)
	{
		// Can't destroy our actors if we're still simulating
		HandleExistingParallelClothSimulation();

		ClothingSimulation->DestroyActors();
	}
}

void USkeletalMeshComponent::ReleaseAllClothingResources()
{
#if WITH_CLOTH_COLLISION_DETECTION
	if(ClothingSimulation)
	{
		// Ensure no running simulation first
		HandleExistingParallelClothSimulation();

		ClothingSimulation->ClearExternalCollisions();
	}
#endif // #if WITH_CLOTH_COLLISION_DETECTION

	RemoveAllClothingActors();
}

void USkeletalMeshComponent::GetWindForCloth_GameThread(FVector& WindDirection, float& WindAdaption) const
{
	check(IsInGameThread());

	WindDirection = FVector::ZeroVector;
	WindAdaption = 2.f;	//not sure where this const comes from, but that's what the old code did
	
	UWorld* World = GetWorld();
	if(World && World->Scene)
	{
		// set wind
		if(IsWindEnabled())
		{
			FVector Position = GetComponentTransform().GetTranslation();

			float WindSpeed;
			float WindMinGust;
			float WindMaxGust;
			World->Scene->GetWindParameters_GameThread(Position, WindDirection, WindSpeed, WindMinGust, WindMaxGust);

			WindDirection *= WindSpeed;
			WindAdaption = FMath::Rand() % 20 * 0.1f; // make range from 0 to 2
		}
	}
}

#if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::FindClothCollisions(FClothCollisionData& OutCollisions)
				{
	if(ClothingSimulation)
					{
		// Get collisions for this simulation, ignoring any externally added collisions
		// (i.e. on grab the asset collisions, not environment etc.)
		ClothingSimulation->GetCollisions(OutCollisions, false);
	}
}

void USkeletalMeshComponent::CopyClothCollisionsToChildren()
{
	// 3 steps
	// 1. release all previous parent collisions
	// 2. find new collisions from parent(this class)
	// 3. add new collisions to children

	TArray<USkeletalMeshComponent*> ClothChildren;

	for (USceneComponent* AttachedChild : GetAttachChildren())
	{
		USkeletalMeshComponent* pChild = Cast<USkeletalMeshComponent>(AttachedChild);
		if(pChild && pChild->ClothingSimulation->ShouldSimulate())
			{
				ClothChildren.Add(pChild);
		}
	}

	const int32 NumClothChildren = ClothChildren.Num();

	if(NumClothChildren == 0)
	{
		return;
	}

	FClothCollisionData NewCollisions;

	FindClothCollisions(NewCollisions);

	for(USkeletalMeshComponent* Child : ClothChildren)
	{
		Child->ClothingSimulation->AddExternalCollisions(NewCollisions);
	}
}

// children's collisions can affect to parent's cloth reversely
void USkeletalMeshComponent::CopyChildrenClothCollisionsToParent()
{
	// 3 steps
	// 1. release all previous children collisions
	// 2. find new collisions from children
	// 3. add new collisions to parent (this component)

	TArray<USkeletalMeshComponent*> ClothCollisionChildren;

	FClothCollisionData NewCollisions;

	for (USceneComponent* AttachedChild : GetAttachChildren())
	{
		USkeletalMeshComponent* pChild = Cast<USkeletalMeshComponent>(AttachedChild);
		if(pChild)
		{
			pChild->FindClothCollisions(NewCollisions);
		}
	}

	ClothingSimulation->AddExternalCollisions(NewCollisions);
}

void USkeletalMeshComponent::ProcessClothCollisionWithEnvironment()
{
	// Limiting the number of extracted shapes per component as these collisions are very expensive
	static const int32 MaxSyncShapesToConsider = 32;

	// don't handle collision detection if this component is in editor
	if(!GetWorld()->IsGameWorld() || !ClothingSimulation)
	{
		return;
	}

	FClothCollisionData NewCollisionData;

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;

	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	// to collide with other clothing objects
	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_PhysicsBody);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ClothOverlapComponents), false);

	GetWorld()->OverlapMultiByObjectType(Overlaps, Bounds.Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(Bounds.BoxExtent), Params);

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		const TWeakObjectPtr<UPrimitiveComponent>& Component = Overlaps[OverlapIdx].Component;
		if (Component.IsValid())
		{ 
			ECollisionChannel Channel = Component->GetCollisionObjectType();

			if(Channel == ECollisionChannel::ECC_WorldStatic)
			{
				// Static world geo
				if(!Component->BodyInstance.IsValidBodyInstance())
				{
					// Move to next component, this one has no valid physics
					continue;
				}

				bool bSuccessfulRead = false;
				Component->BodyInstance.ExecuteOnPhysicsReadOnly([&]
				{
					TArray<PxShape*> AllShapes;
					const int32 NumSyncShapes = Component->BodyInstance.GetAllShapes_AssumesLocked(AllShapes);

					if(NumSyncShapes == 0 || NumSyncShapes > MaxSyncShapesToConsider)
					{
						// Either no shapes or too complicated to consider
						return;
					}

					// Matrices required to transform shapes into sim space (component space)
					// Transform of external component and matrix describing external component -> this component
					FTransform Transform = Component->GetComponentTransform();
					FMatrix TransformMatrix = Transform.ToMatrixWithScale();
					FMatrix ComponentToClothMatrix = TransformMatrix * GetComponentTransform().ToMatrixWithScale().Inverse();

					for(PxShape* Shape : AllShapes)
					{
						PxGeometryType::Enum GeoType = Shape->getGeometryType();

						// Pose of the shape in actor space
						FMatrix ShapeLocalPose = P2UTransform(Shape->getLocalPose()).ToMatrixWithScale();

						switch(GeoType)
						{
							default: 
					break;

							case PxGeometryType::eSPHERE:
							{
								PxSphereGeometry SphereGeo;
								Shape->getSphereGeometry(SphereGeo);

								NewCollisionData.Spheres.AddDefaulted();
								FClothCollisionPrim_Sphere& NewSphere = NewCollisionData.Spheres.Last();

								NewSphere.BoneIndex = INDEX_NONE; // No bone, just local space
								NewSphere.LocalPosition = ComponentToClothMatrix.TransformPosition(ShapeLocalPose.GetOrigin());
								NewSphere.Radius = SphereGeo.radius;
							}
							break;

							case PxGeometryType::eCAPSULE:
							{
								PxCapsuleGeometry CapGeo;
								Shape->getCapsuleGeometry(CapGeo);

								const int32 BaseSphereIndex = NewCollisionData.Spheres.Num();

								NewCollisionData.Spheres.AddDefaulted(2);
								FClothCollisionPrim_Sphere& Sphere0 = NewCollisionData.Spheres.Last(1);
								FClothCollisionPrim_Sphere& Sphere1 = NewCollisionData.Spheres.Last(0);

								NewCollisionData.SphereConnections.AddDefaulted();
								FClothCollisionPrim_SphereConnection& Connection = NewCollisionData.SphereConnections.Last();

								FVector ZAxis = ComponentToClothMatrix.TransformVector(ShapeLocalPose.GetUnitAxis(EAxis::X));

								Sphere0.BoneIndex = INDEX_NONE;
								Sphere0.LocalPosition = ComponentToClothMatrix.TransformPosition(ShapeLocalPose.GetOrigin()) + CapGeo.halfHeight * ZAxis;
								Sphere0.Radius = CapGeo.radius;

								Sphere1.BoneIndex = INDEX_NONE;
								Sphere1.LocalPosition = ComponentToClothMatrix.TransformPosition(ShapeLocalPose.GetOrigin()) - CapGeo.halfHeight * ZAxis;
								Sphere1.Radius = CapGeo.radius;

								Connection.SphereIndices[0] = BaseSphereIndex;
								Connection.SphereIndices[1] = BaseSphereIndex + 1;
							}
							break;

							case PxGeometryType::eBOX:
							{
								PxBoxGeometry BoxGeo;
								Shape->getBoxGeometry(BoxGeo);

								// We're building the box in local space, so to get to the cloth transform
								// we need to go through local -> actor -> world -> cloth
								FMatrix FullTransformMatrix = ShapeLocalPose * ComponentToClothMatrix;

								NewCollisionData.Convexes.AddDefaulted();
								FClothCollisionPrim_Convex& Convex = NewCollisionData.Convexes.Last();
								Convex.Planes.Reset(6);

								// we need to inflate the hull to get nicer collisions (only particles collide)
								const static float Inflate = 2.0f;
								BoxGeo.halfExtents += PxVec3(Inflate);
				
								FPlane UPlane1(1, 0, 0, BoxGeo.halfExtents.x);
								UPlane1 = UPlane1.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane1);

								FPlane UPlane2(-1, 0, 0, BoxGeo.halfExtents.x);
								UPlane2 = UPlane2.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane2);

								FPlane UPlane3(0, 1, 0, BoxGeo.halfExtents.y);
								UPlane3 = UPlane3.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane3);

								FPlane UPlane4(0, -1, 0, BoxGeo.halfExtents.y);
								UPlane4 = UPlane4.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane4);

								FPlane UPlane5(0, 0, 1, BoxGeo.halfExtents.z);
								UPlane5 = UPlane5.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane5);

								FPlane UPlane6(0, 0, -1, BoxGeo.halfExtents.z);
								UPlane6 = UPlane6.TransformBy(FullTransformMatrix);
								Convex.Planes.Add(UPlane6);

								Convex.BoneIndex = INDEX_NONE;
							}
							break;

							case PxGeometryType::eCONVEXMESH:
							{
								PxConvexMeshGeometry MeshGeo;
								Shape->getConvexMeshGeometry(MeshGeo);

								// we need to inflate the hull to get nicer collisions (only particles collide)
								const static float Inflate = 2.0f;

								if(MeshGeo.convexMesh)
								{
									NewCollisionData.Convexes.AddDefaulted();
									FClothCollisionPrim_Convex& NewConvex = NewCollisionData.Convexes.Last();

									FMatrix FullTransformMatrix = ShapeLocalPose * ComponentToClothMatrix;

									uint32 NumPolys = MeshGeo.convexMesh->getNbPolygons();
									NewConvex.Planes.Empty(NumPolys);

									PxHullPolygon HullData;
									for(uint32 PolyIndex = 0; PolyIndex < NumPolys; ++PolyIndex)
									{
										MeshGeo.convexMesh->getPolygonData(PolyIndex, HullData);
										PxPlane PPlane(HullData.mPlane[0], HullData.mPlane[1], HullData.mPlane[2], HullData.mPlane[3]);
										FPlane UPlane = P2UPlane(PPlane);
										UPlane = UPlane.TransformBy(FullTransformMatrix);
										
										UPlane.W += Inflate;

										NewConvex.Planes.Add(UPlane);
									}
								}	
							}
							break;
						}
					}
					bSuccessfulRead = true;
				});
			}
			else if(Channel == ECollisionChannel::ECC_PhysicsBody)
			{
				// Possibly a skeletal mesh, extract it's clothing collisions if necessary
				USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Component.Get());

				if(SkelComp && SkelComp->SkeletalMesh)
				{
					if(SkelComp == this)
					{
						// Same mesh, move to next component
						continue;
					}

					if(SkelComp->ClothingSimulation)
					{
						SkelComp->ClothingSimulation->GetCollisions(NewCollisionData, false);
					}
				}
			}
		}
	}

	ClothingSimulation->AddExternalCollisions(NewCollisionData);
}

#endif// #if WITH_CLOTH_COLLISION_DETECTION

void USkeletalMeshComponent::EndPhysicsTickComponent(FSkeletalMeshComponentEndPhysicsTickFunction& ThisTickFunction)
{
	//IMPORTANT!
	//
	// The decision on whether to use EndPhysicsTickComponent or not is made by ShouldRunEndPhysicsTick()
	// Any changes that are made to EndPhysicsTickComponent that affect whether it should be run or not
	// have to be reflected in ShouldRunEndPhysicsTick() as well
	
	// if physics is disabled on dedicated server, no reason to be here. 
	if (!bEnablePhysicsOnDedicatedServer && IsRunningDedicatedServer())
	{
		FinalizeBoneTransform();
		return;
	}

	if (IsRegistered() && IsSimulatingPhysics())
	{
		SyncComponentToRBPhysics();
	}

	// this used to not run if not rendered, but that causes issues such as bounds not updated
	// causing it to not rendered, at the end, I think we should blend body positions
	// for example if you're only simulating, this has to happen all the time
	// whether looking at it or not, otherwise
	// @todo better solution is to check if it has moved by changing SyncComponentToRBPhysics to return true if anything modified
	// and run this if that is true or rendered
	// that will at least reduce the chance of mismatch
	// generally if you move your actor position, this has to happen to approximately match their bounds
	if (ShouldBlendPhysicsBones())
	{
		if (IsRegistered())
		{
			BlendInPhysics(ThisTickFunction);
		}
	}
}

void USkeletalMeshComponent::UpdateClothTransformImp()
{
	const bool bActiveClothing = ClothingSimulation && ClothingSimulation->ShouldSimulate();

#if WITH_CLOTH_COLLISION_DETECTION

	if(ClothingSimulation)
	{
		ClothingSimulation->ClearExternalCollisions();
	}

	if(bCollideWithAttachedChildren)
	{
		CopyClothCollisionsToChildren();
	}

	//check the environment when only transform is updated
	if(bCollideWithEnvironment && bActiveClothing)
	{
		ProcessClothCollisionWithEnvironment();
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

#if !(UE_BUILD_SHIPPING)
	FTransform ComponentTransform = GetComponentTransform();
	if (ComponentTransform.GetRotation().ContainsNaN())
	{
		logOrEnsureNanError(TEXT("SkeletalMeshComponent::UpdateClothTransform found NaN in GetComponentTransform().GetRotation()"));
		ComponentTransform.SetRotation(FQuat(0.0f, 0.0f, 0.0f, 1.0f));
		SetComponentToWorld(ComponentTransform);
	}
	if (ComponentTransform.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("SkeletalMeshComponent::UpdateClothTransform still found NaN in GetComponentTransform() (wasn't the rotation)"));
		SetComponentToWorld(FTransform::Identity);
	}
#endif
}

void USkeletalMeshComponent::UpdateClothTransform(ETeleportType TeleportType)
{
	//Note that it's not safe to run the update here. This is because cloth sim could still be running on another thread. We defer it
	bPendingClothTransformUpdate = true;
	PendingTeleportType = TeleportType;
}

void USkeletalMeshComponent::CheckClothTeleport()
{
	// Get the root bone transform
	FMatrix CurRootBoneMat = GetBoneMatrix(0);
	
	// distance check 
	// TeleportDistanceThreshold is greater than Zero and not teleported yet
	if(TeleportDistanceThreshold > 0 && ClothTeleportMode == EClothingTeleportMode::None)
	{
		float DistSquared = FVector::DistSquared(PrevRootBoneMatrix.GetOrigin(), CurRootBoneMat.GetOrigin());
		if ( DistSquared > ClothTeleportDistThresholdSquared ) // if it has traveled too far
		{
			ClothTeleportMode = bResetAfterTeleport ? EClothingTeleportMode::TeleportAndReset : EClothingTeleportMode::Teleport;
		}
	}

	// rotation check
	// if TeleportRotationThreshold is greater than Zero and the user didn't do force teleport
	if(TeleportRotationThreshold > 0 && ClothTeleportMode == EClothingTeleportMode::None)
	{
		// Detect whether teleportation is needed or not
		// Rotation matrix's transpose means an inverse but can't use a transpose because this matrix includes scales
		FMatrix AInvB = CurRootBoneMat * PrevRootBoneMatrix.InverseFast();
		float Trace = AInvB.M[0][0] + AInvB.M[1][1] + AInvB.M[2][2];
		float CosineTheta = (Trace - 1.0f) / 2.0f; // trace = 1+2cos(theta) for a 3x3 matrix

		if ( CosineTheta < ClothTeleportCosineThresholdInRad ) // has the root bone rotated too much
		{
			ClothTeleportMode = bResetAfterTeleport ? EClothingTeleportMode::TeleportAndReset : EClothingTeleportMode::Teleport;
		}
	}

	PrevRootBoneMatrix = CurRootBoneMat;
}

FAutoConsoleTaskPriority CPrio_FParallelClothTask(
	TEXT("TaskGraph.TaskPriorities.ParallelClothTask"),
	TEXT("Task and thread priority for parallel cloth."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

class FParallelClothTask
{
	const USkeletalMeshComponent& SkeletalMeshComponent;
	float DeltaTime;

public:
	FParallelClothTask(const USkeletalMeshComponent& InSkeletalMeshComponent, float InDeltaTime)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
		, DeltaTime(InDeltaTime)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelClothTask, STATGROUP_TaskGraphTasks);
	}
	static ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_FParallelClothTask.Get();
	}
	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FScopeCycleCounterUObject ContextScope(&SkeletalMeshComponent);
		SCOPE_CYCLE_COUNTER(STAT_ClothTotalTime);

		if(SkeletalMeshComponent.ClothingSimulation)
		{
			SkeletalMeshComponent.ClothingSimulation->Simulate(SkeletalMeshComponent.ClothingSimulationContext);
		}
	}
};

// This task runs after the clothing task to perform a writeback of data from the simulation to the
// component. This is done on the gamethread as that is where the data is to be valid (ensures no other
// component ticks will be accessing during the writeback)
class FParallelClothCompletionTask
{
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

public:
	FParallelClothCompletionTask(TWeakObjectPtr<USkeletalMeshComponent> InSkeletalMeshComponent)
		: SkeletalMeshComponent(InSkeletalMeshComponent)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParallelClothCompletionTask, STATGROUP_TaskGraphTasks);
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
		SCOPE_CYCLE_COUNTER(STAT_ClothWriteback)
		// Perform the data writeback
		if(USkeletalMeshComponent* MeshComp = SkeletalMeshComponent.Get())
		{
			MeshComp->CompleteParallelClothSimulation();
		}
	}
};

void USkeletalMeshComponent::UpdateClothStateAndSimulate(float DeltaTime, FTickFunction& ThisTickFunction)
{
	// If disabled or no simulation
	if (CVarEnableClothPhysics.GetValueOnGameThread() == 0 || !ClothingSimulation)
	{
		return;
	}

	check(IsInGameThread());

	// If we simulate a clothing actor at 0s it will fill simulated positions and normals with NaNs.
	// we can skip all the work it is still doing, and get the desired result (frozen sim) by not
	// updating and simulating.
	if(DeltaTime == 0.0f)
	{
		return;
	}

	// Make sure we aren't already in flight from previous frame
	HandleExistingParallelClothSimulation();

#if WITH_CLOTH_COLLISION_DETECTION
	if (bCollideWithAttachedChildren)
	{
		if(ClothingSimulation)
		{
			ClothingSimulation->ClearExternalCollisions();
		}

		CopyClothCollisionsToChildren();
		CopyChildrenClothCollisionsToParent();
	}
#endif // WITH_CLOTH_COLLISION_DETECTION

	UpdateClothSimulationContext(DeltaTime);

	if(ClothingSimulation)
	{
		ParallelClothTask = TGraphTask<FParallelClothTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this, DeltaTime);
			
		FGraphEventArray Prerequisites;
		Prerequisites.Add(ParallelClothTask);
		FGraphEventRef ClothCompletionEvent = TGraphTask<FParallelClothCompletionTask>::CreateTask(&Prerequisites, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this);

		ThisTickFunction.GetCompletionHandle()->DontCompleteUntil(ClothCompletionEvent);
	}
}

//This is the total cloth time split up among multiple computation (updating gpu, updating sim, etc...)
DECLARE_CYCLE_STAT(TEXT("Cloth Sim"), STAT_ClothSimTime, STATGROUP_Physics);

bool USkeletalMeshComponent::GetClothSimulatedPosition_GameThread(const FGuid& AssetGuid, int32 VertexIndex, FVector& OutSimulPos) const
{
	if(!SkeletalMesh)
	{
		// Can't proceed without a mesh
		return false;
	}
		
	bool bSucceed = false;

	int32 AssetIndex = SkeletalMesh->GetClothingAssetIndex(AssetGuid);

	if(AssetIndex != INDEX_NONE)
	{
		const FClothSimulData* ActorData = CurrentSimulationData_GameThread.Find(AssetIndex);

		if(ActorData && ActorData->Positions.IsValidIndex(VertexIndex))
		{
			OutSimulPos = ActorData->Positions[VertexIndex];

			bSucceed = true;
		}
	}
	return bSucceed;
}

void USkeletalMeshComponent::TickClothing(float DeltaTime, FTickFunction& ThisTickFunction)
{
	if (SkeletalMesh == nullptr || !ClothingSimulation || CVarEnableClothPhysics.GetValueOnGameThread() == 0)
	{
		return;
	}

	// Use the component update flag to gate simulation to respect the always tick options
	bool bShouldTick = ((MeshComponentUpdateFlag < EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered) || bRecentlyRendered);

	if (bShouldTick)
	{
		UpdateClothStateAndSimulate(DeltaTime, ThisTickFunction);
	}
	else
	{
		ForceClothNextUpdateTeleportAndReset();
	}
}

void USkeletalMeshComponent::GetUpdateClothSimulationData(TMap<int32, FClothSimulData>& OutClothSimData, USkeletalMeshComponent* OverrideLocalRootComponent)
{
	if(CVarEnableClothPhysics.GetValueOnAnyThread() == 0)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_ClothTotalTime);

	if(bDisableClothSimulation)
	{
		OutClothSimData.Reset();
		return;
	}

	if(ClothingSimulation)
	{
		ClothingSimulation->GetSimulationData(OutClothSimData, this, OverrideLocalRootComponent);
		return;
	}
}

void USkeletalMeshComponent::DebugDrawClothing(FPrimitiveDrawInterface* PDI)
{
#if WITH_EDITOR && ENABLE_DRAW_DEBUG

	if(ClothingSimulation)
	{
		FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

		if(ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(ClothingSimulationFactory->GetFName()))
		{
			Extender->DebugDrawSimulation(ClothingSimulation, this, PDI);
		}

		return;
	}

#endif
}

void USkeletalMeshComponent::SetAllMassScale(float InMassScale)
{
	// Apply mass scale to each child body
	for(FBodyInstance* BI : Bodies)
	{
		if (BI->IsValidBodyInstance())
		{
			BI->SetMassScale(InMassScale);
		}
	}
}


float USkeletalMeshComponent::GetMass() const
{
	float Mass = 0.0f;
	for (int32 i=0; i < Bodies.Num(); ++i)
	{
		FBodyInstance* BI = Bodies[i];

		if (BI->IsValidBodyInstance())
		{
			Mass += BI->GetBodyMass();
		}
	}
	return Mass;
}

float USkeletalMeshComponent::GetBoneMass(FName BoneName, bool bScaleMass) const
{
	if (FBodyInstance* BI = GetBodyInstance(BoneName))
	{
		float Scale = 1.0f;
		if (bScaleMass)
		{
			Scale = BI->MassScale;
		}
		return Scale*BI->GetBodyMass();
	}

	return 0.0f;
}

FVector USkeletalMeshComponent::GetSkeletalCenterOfMass() const
{
	FVector Location = FVector::ZeroVector;
	float TotalMass = 0.0f;
	for (int32 i = 0; i < Bodies.Num(); i++)
	{
		FBodyInstance* BI = Bodies[i];
		if (BI->IsValidBodyInstance())
		{
			float Mass = BI->MassScale*BI->GetBodyMass();
			Location += Mass*BI->GetCOMPosition();
			TotalMass += Mass;
		}
	}

	return Location / TotalMass;
}


// blueprint callable methods 
float USkeletalMeshComponent::GetClothMaxDistanceScale()
{
	return ClothMaxDistanceScale;
}

void USkeletalMeshComponent::SetClothMaxDistanceScale(float Scale)
{
	ClothMaxDistanceScale = Scale;
}

void USkeletalMeshComponent::ResetClothTeleportMode()
{
	ClothTeleportMode = EClothingTeleportMode::None;
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleport()
{
	ClothTeleportMode = EClothingTeleportMode::Teleport;
}

void USkeletalMeshComponent::ForceClothNextUpdateTeleportAndReset()
{
	ClothTeleportMode = EClothingTeleportMode::TeleportAndReset;
}

FTransform USkeletalMeshComponent::GetComponentTransformFromBodyInstance(FBodyInstance* UseBI)
{
	if (PhysicsTransformUpdateMode == EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform)
	{
		// undo root transform so that it only moves according to what actor itself suppose to move
		const FTransform& BodyTransform = UseBI->GetUnrealWorldTransform();
		return RootBodyData.TransformToRoot * BodyTransform;
	}
	else
	{
		return GetComponentTransform();
	}
}
#undef LOCTEXT_NAMESPACE
