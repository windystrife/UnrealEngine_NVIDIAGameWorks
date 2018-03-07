// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "SceneManagement.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysXPublic.h"

const FConstraintProfileProperties UPhysicalAnimationComponent::PhysicalAnimationProfile = []()
{
	//Setup the default constraint profile for all joints created by physical animation system
	FConstraintProfileProperties RetProfile;
	RetProfile.LinearLimit.XMotion = LCM_Free;
	RetProfile.LinearLimit.YMotion = LCM_Free;
	RetProfile.LinearLimit.ZMotion = LCM_Free;

	RetProfile.ConeLimit.Swing1Motion = ACM_Free;
	RetProfile.ConeLimit.Swing2Motion = ACM_Free;
	RetProfile.TwistLimit.TwistMotion = ACM_Free;

	RetProfile.LinearDrive.XDrive.bEnablePositionDrive = true;
	RetProfile.LinearDrive.XDrive.bEnableVelocityDrive = true;
	RetProfile.LinearDrive.YDrive.bEnablePositionDrive = true;
	RetProfile.LinearDrive.YDrive.bEnableVelocityDrive = true;
	RetProfile.LinearDrive.ZDrive.bEnablePositionDrive = true;
	RetProfile.LinearDrive.ZDrive.bEnableVelocityDrive = true;

	RetProfile.AngularDrive.SlerpDrive.bEnablePositionDrive = true;
	RetProfile.AngularDrive.SlerpDrive.bEnableVelocityDrive = true;
	RetProfile.AngularDrive.AngularDriveMode = EAngularDriveMode::SLERP;

	return RetProfile;
}();

UPhysicalAnimationComponent::UPhysicalAnimationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	SceneIndex = INDEX_NONE;
	StrengthMultiplyer = 1.f;
}

void UPhysicalAnimationComponent::InitializeComponent()
{
	Super::InitializeComponent();
	InitComponent();
}

void UPhysicalAnimationComponent::InitComponent()
{
	if (SkeletalMeshComponent)
	{
		OnTeleportDelegateHandle = SkeletalMeshComponent->RegisterOnTeleportDelegate(FOnSkelMeshTeleported::CreateUObject(this, &UPhysicalAnimationComponent::OnTeleport));
		PrimaryComponentTick.AddPrerequisite(SkeletalMeshComponent, SkeletalMeshComponent->PrimaryComponentTick);
		UpdatePhysicsEngine();
	}
}

void UPhysicalAnimationComponent::BeginDestroy()
{
	if(SkeletalMeshComponent && OnTeleportDelegateHandle.IsValid())
	{
		SkeletalMeshComponent->UnregisterOnTeleportDelegate(OnTeleportDelegateHandle);
	}

	ReleasePhysicsEngine();
	Super::BeginDestroy();
}

void UPhysicalAnimationComponent::SetSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if(SkeletalMeshComponent && OnTeleportDelegateHandle.IsValid())
	{
		SkeletalMeshComponent->UnregisterOnTeleportDelegate(OnTeleportDelegateHandle);
	}

	SkeletalMeshComponent = InSkeletalMeshComponent;
	DriveData.Empty();
	ReleasePhysicsEngine();
	InitComponent();

}

bool UpdatePhysicalAnimationSettings(FName BodyName, const FPhysicalAnimationData& InPhysicalAnimationData, TArray<FPhysicalAnimationData>& DriveData, const UPhysicsAsset& PhysAsset)
{
	int32 BodyIdx = PhysAsset.FindBodyIndex(BodyName);
	if (BodyIdx != INDEX_NONE)
	{
		//This code does a linear search in the insertion. This is by choice so that during tick we get nice tight cache for iteration. We could keep a map of sorts, but expected number of bodies is small
		FPhysicalAnimationData* UpdateAnimationData = DriveData.FindByPredicate([BodyName](const FPhysicalAnimationData& Elem) { return Elem.BodyName == BodyName; });
		if (UpdateAnimationData == nullptr)
		{
			UpdateAnimationData = &DriveData[DriveData.AddUninitialized()];
		}
		*UpdateAnimationData = InPhysicalAnimationData;
		UpdateAnimationData->BodyName = BodyName;

		return true;
	}

	return false;
}

void UPhysicalAnimationComponent::ApplyPhysicalAnimationSettings(FName BodyName, const FPhysicalAnimationData& PhysicalAnimationData)
{
	UPhysicsAsset* PhysAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (PhysAsset)
	{
		if(UpdatePhysicalAnimationSettings(BodyName, PhysicalAnimationData, DriveData, *PhysAsset))
		{
			UpdatePhysicsEngine();
		}
	}
}

void UPhysicalAnimationComponent::ApplyPhysicalAnimationSettingsBelow(FName BodyName, const FPhysicalAnimationData& PhysicalAnimationData, bool bIncludeSelf)
{
	UPhysicsAsset* PhysAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (PhysAsset && SkeletalMeshComponent)
	{
		TArray<FPhysicalAnimationData>& NewDriveData = DriveData;
		bool bNeedsUpdating = false;
		SkeletalMeshComponent->ForEachBodyBelow(BodyName, bIncludeSelf, /*bSkipCustomType=*/false, [PhysAsset, &NewDriveData, PhysicalAnimationData, &bNeedsUpdating](const FBodyInstance* BI)
		{
			const FName IterBodyName = PhysAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
			bNeedsUpdating |= UpdatePhysicalAnimationSettings(IterBodyName, PhysicalAnimationData, NewDriveData, *PhysAsset);
		});

		if(bNeedsUpdating)
		{
			UpdatePhysicsEngine();
		}
	}
}


void UPhysicalAnimationComponent::ApplyPhysicalAnimationProfileBelow(FName BodyName, FName ProfileName, bool bIncludeSelf, bool bClearNotFound)
{
	UPhysicsAsset* PhysAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (PhysAsset && SkeletalMeshComponent)
	{
		TArray<FPhysicalAnimationData>& NewDriveData = DriveData;
		bool bNeedsUpdating = false;
		SkeletalMeshComponent->ForEachBodyBelow(BodyName, bIncludeSelf, /*bSkipCustomType=*/false, [bClearNotFound, ProfileName, PhysAsset, &NewDriveData, &bNeedsUpdating](const FBodyInstance* BI)
		{
			if(USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName IterBodyName = PhysAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
				if(FPhysicalAnimationProfile* Profile = BodySetup->FindPhysicalAnimationProfile(ProfileName))
				{
					bNeedsUpdating |= UpdatePhysicalAnimationSettings(IterBodyName, Profile->PhysicalAnimationData, NewDriveData, *PhysAsset);
				}
				else if(bClearNotFound)
				{
					bNeedsUpdating |= UpdatePhysicalAnimationSettings(IterBodyName, FPhysicalAnimationData(), NewDriveData, *PhysAsset);
				}
			}
			
		});

		if (bNeedsUpdating)
		{
			UpdatePhysicsEngine();
		}
	}
}

FTransform UPhysicalAnimationComponent::GetBodyTargetTransform(FName BodyName) const
{
	if (SkeletalMeshComponent)
	{
#if WITH_PHYSX
		for (int32 DataIdx = 0; DataIdx < DriveData.Num(); ++DataIdx)
		{
			const FPhysicalAnimationData& PhysAnimData = DriveData[DataIdx];
			const FPhysicalAnimationInstanceData& InstanceData = RuntimeInstanceData[DataIdx];
			if (BodyName == PhysAnimData.BodyName)
			{
				if (PxRigidDynamic* TargetActor = InstanceData.TargetActor)
				{
					PxTransform PKinematicTarget;
					if (TargetActor->getKinematicTarget(PKinematicTarget))
					{
						return P2UTransform(PKinematicTarget);
					}
					else
					{
						return P2UTransform(TargetActor->getGlobalPose());
					}
				}

				break;
			}
		}
#endif

		// if body isn't controlled by physical animation, just return the body position
		const TArray<FTransform>& ComponentSpaceTransforms = SkeletalMeshComponent->GetComponentSpaceTransforms();
		const int32 BoneIndex = SkeletalMeshComponent->GetBoneIndex(BodyName);
		if (ComponentSpaceTransforms.IsValidIndex(BoneIndex))
		{
			return ComponentSpaceTransforms[BoneIndex] * SkeletalMeshComponent->GetComponentToWorld();
		}
	}

	return FTransform::Identity;
}

FTransform ComputeWorldSpaceTargetTM(const USkeletalMeshComponent& SkeletalMeshComponent, const TArray<FTransform>& SpaceBases, int32 BoneIndex)
{
	return SpaceBases[BoneIndex] * SkeletalMeshComponent.GetComponentToWorld();
}

FTransform ComputeLocalSpaceTargetTM(const USkeletalMeshComponent& SkeletalMeshComponent, const UPhysicsAsset& PhysAsset, int32 BoneIndex)
{
	const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent.SkeletalMesh->RefSkeleton;
	FTransform AccumulatedDelta = SkeletalMeshComponent.BoneSpaceTransforms[BoneIndex];
	int32 CurBoneIdx = BoneIndex;
	while ((CurBoneIdx = RefSkeleton.GetParentIndex(CurBoneIdx)) != INDEX_NONE)
	{
		FName BoneName = RefSkeleton.GetBoneName(CurBoneIdx);
		int32 BodyIndex = PhysAsset.FindBodyIndex(BoneName);

		if (CurBoneIdx == BoneIndex)	//some kind of loop so just stop TODO: warn?
		{
			break;
		}

		if (SkeletalMeshComponent.Bodies.IsValidIndex(BodyIndex))
		{

			FBodyInstance* ParentBody = SkeletalMeshComponent.Bodies[BodyIndex];
			const FTransform NewWorldTM = AccumulatedDelta * ParentBody->GetUnrealWorldTransform_AssumesLocked();
			return NewWorldTM;
		}

		AccumulatedDelta = AccumulatedDelta * SkeletalMeshComponent.BoneSpaceTransforms[CurBoneIdx];
	}

	return FTransform::Identity;
}

FTransform ComputeTargetTM(const FPhysicalAnimationData& PhysAnimData, const USkeletalMeshComponent& SkeletalMeshComponent, const UPhysicsAsset& PhysAsset, const TArray<FTransform>& SpaceBases, int32 BoneIndex)
{
	return PhysAnimData.bIsLocalSimulation ? ComputeLocalSpaceTargetTM(SkeletalMeshComponent, PhysAsset, BoneIndex) : ComputeWorldSpaceTargetTM(SkeletalMeshComponent, SpaceBases, BoneIndex);
}

void UPhysicalAnimationComponent::UpdateTargetActors(ETeleportType TeleportType)
{
	UPhysicsAsset* PhysAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (PhysAsset && SkeletalMeshComponent->SkeletalMesh)
	{
		const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent->SkeletalMesh->RefSkeleton;
		const TArray<FTransform>& SpaceBases = SkeletalMeshComponent->GetEditableComponentSpaceTransforms();

#if WITH_PHYSX
		if (PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex))
		{
			SCOPED_SCENE_WRITE_LOCK(Scene);
			for (int32 DataIdx = 0; DataIdx < DriveData.Num(); ++DataIdx)
			{
				const FPhysicalAnimationData& PhysAnimData = DriveData[DataIdx];
				FPhysicalAnimationInstanceData& InstanceData = RuntimeInstanceData[DataIdx];
				if (PxRigidDynamic* TargetActor = InstanceData.TargetActor)
				{
					const int32 BoneIdx = RefSkeleton.FindBoneIndex(PhysAnimData.BodyName);
					if (BoneIdx != INDEX_NONE)	//It's possible the skeletal mesh has changed out from under us. In that case we should probably reset, but at the very least don't do work on non-existent bones
					{
						const FTransform TargetTM = ComputeTargetTM(PhysAnimData, *SkeletalMeshComponent, *PhysAsset, SpaceBases, BoneIdx);
						TargetActor->setKinematicTarget(U2PTransform(TargetTM));	//TODO: this doesn't work with sub-stepping!
						
						if(TeleportType == ETeleportType::TeleportPhysics)
						{
							TargetActor->setGlobalPose(U2PTransform(TargetTM));	//Note that we still set the kinematic target because physx doesn't clear this
						}
						
					}
				}
			}
		}
#endif
	}
}

void UPhysicalAnimationComponent::OnTeleport()
{
	UpdateTargetActors(ETeleportType::TeleportPhysics);
}

void UPhysicalAnimationComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	UpdateTargetActors(ETeleportType::None);
}

//NOTE: Technically skeletal mesh component could have bodies in multiple scenes. This doesn't seem like a legit setup though and we should probably enforce that it's not supported.
int32 FindSceneIndexForSkeletalMeshComponent(const USkeletalMeshComponent* SkeletalMeshComp)
{
	for(FBodyInstance* BI : SkeletalMeshComp->Bodies)
	{
		return BI->GetSceneIndex();
	}

	return INDEX_NONE;
}

void SetMotorStrength(FConstraintInstance& ConstraintInstance, const FPhysicalAnimationData& PhysAnimData, float StrengthMultiplyer)
{
	ConstraintInstance.SetAngularDriveParams(PhysAnimData.OrientationStrength * StrengthMultiplyer, PhysAnimData.AngularVelocityStrength * StrengthMultiplyer, PhysAnimData.MaxAngularForce * StrengthMultiplyer);
	if (PhysAnimData.bIsLocalSimulation)	//linear only works for world space simulation
	{
		ConstraintInstance.SetLinearDriveParams(0.f, 0.f, 0.f);
	}
	else
	{
		ConstraintInstance.SetLinearDriveParams(PhysAnimData.PositionStrength * StrengthMultiplyer, PhysAnimData.VelocityStrength * StrengthMultiplyer, PhysAnimData.MaxLinearForce * StrengthMultiplyer);
	}
}

void UPhysicalAnimationComponent::UpdatePhysicsEngine()
{
	UPhysicsAsset* PhysAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if(PhysAsset && SkeletalMeshComponent->SkeletalMesh)
	{
		//TODO: This is hacky and assumes constraints can only be added and not removed. True for now, but bad in general!
		const int32 NumData = DriveData.Num();
		const int32 NumInstances = RuntimeInstanceData.Num();
		
		RuntimeInstanceData.AddZeroed(NumData - NumInstances);
		SceneIndex = FindSceneIndexForSkeletalMeshComponent(SkeletalMeshComponent);

		const TArray<FTransform>& SpaceBases = SkeletalMeshComponent->GetEditableComponentSpaceTransforms();
		const FReferenceSkeleton& RefSkeleton = SkeletalMeshComponent->SkeletalMesh->RefSkeleton;
#if WITH_PHYSX
		if(PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex))
		{
			SCOPED_SCENE_WRITE_LOCK(Scene);
			for (int32 DataIdx = 0; DataIdx < DriveData.Num(); ++DataIdx)
			{
				bool bNewConstraint = false;
				const FPhysicalAnimationData& PhysAnimData = DriveData[DataIdx];
				FPhysicalAnimationInstanceData& InstanceData = RuntimeInstanceData[DataIdx];
				FConstraintInstance*& ConstraintInstance = InstanceData.ConstraintInstance;
				if(ConstraintInstance == nullptr)
				{
					bNewConstraint = true;
					ConstraintInstance = new FConstraintInstance();
					ConstraintInstance->ProfileInstance = PhysicalAnimationProfile;
				}

				//Apply drive forces
				SetMotorStrength(*ConstraintInstance, PhysAnimData, StrengthMultiplyer);
				
				if(bNewConstraint)
				{
					//Find body instances
					int32 ChildBodyIdx = PhysAsset->FindBodyIndex(PhysAnimData.BodyName);
					if (FBodyInstance* ChildBody = (ChildBodyIdx == INDEX_NONE ? nullptr : SkeletalMeshComponent->Bodies[ChildBodyIdx]))
					{
						if (PxRigidActor* PRigidActor = ChildBody->GetPxRigidActor_AssumesLocked())
						{
							ConstraintInstance->SetRefFrame(EConstraintFrame::Frame1, FTransform::Identity);
							ConstraintInstance->SetRefFrame(EConstraintFrame::Frame2, FTransform::Identity);

							const FTransform TargetTM = ComputeTargetTM(PhysAnimData, *SkeletalMeshComponent, *PhysAsset, SpaceBases, ChildBody->InstanceBoneIndex);

							// Create kinematic actor we are going to create joint with. This will be moved around with calls to SetLocation/SetRotation.
							PxRigidDynamic* KineActor = Scene->getPhysics().createRigidDynamic(U2PTransform(TargetTM));
							KineActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
							KineActor->setMass(1.0f);
							KineActor->setMassSpaceInertiaTensor(PxVec3(1.0f, 1.0f, 1.0f));

							// No bodyinstance
							KineActor->userData = NULL;

							// Add to Scene
							Scene->addActor(*KineActor);

							// Save reference to the kinematic actor.
							InstanceData.TargetActor = KineActor;

							ConstraintInstance->InitConstraintPhysX_AssumesLocked(PRigidActor, InstanceData.TargetActor, Scene, 1.f);
						}
					}
				}
			}
		}
#endif
	}
}

void UPhysicalAnimationComponent::SetStrengthMultiplyer(float InStrengthMultiplyer)
{
	if(InStrengthMultiplyer >= 0.f)
	{
		StrengthMultiplyer = InStrengthMultiplyer;

#if WITH_PHYSX
		if (PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex))
		{
			SCOPED_SCENE_WRITE_LOCK(Scene);
			for (int32 DataIdx = 0; DataIdx < DriveData.Num(); ++DataIdx)
			{
				bool bNewConstraint = false;
				const FPhysicalAnimationData& PhysAnimData = DriveData[DataIdx];
				FPhysicalAnimationInstanceData& InstanceData = RuntimeInstanceData[DataIdx];
				if(FConstraintInstance* ConstraintInstance = InstanceData.ConstraintInstance)
				{
					//Apply drive forces
					SetMotorStrength(*ConstraintInstance, PhysAnimData, StrengthMultiplyer);
				}
			}
		}
#endif
	}
}

void UPhysicalAnimationComponent::ReleasePhysicsEngine()
{
#if WITH_PHYSX
	PxScene* Scene = GetPhysXSceneFromIndex(SceneIndex);
	SCOPED_SCENE_WRITE_LOCK(Scene);

	for (FPhysicalAnimationInstanceData& Instance : RuntimeInstanceData)
	{
		if(Instance.ConstraintInstance)
		{
			Instance.ConstraintInstance->TermConstraint();
			delete Instance.ConstraintInstance;
			Instance.ConstraintInstance = nullptr;
		}
		
		if(Instance.TargetActor)
		{
			Scene->removeActor(*Instance.TargetActor);
			Instance.TargetActor->release();

			Instance.TargetActor = nullptr;
		}
	}

	RuntimeInstanceData.Reset();
#endif
}

#if WITH_EDITOR
static const FColor	TargetActorColor(255, 0, 0);

void UPhysicalAnimationComponent::DebugDraw(FPrimitiveDrawInterface* PDI) const
{
	for (const FPhysicalAnimationInstanceData& PhysAnimData : RuntimeInstanceData)
	{
#if WITH_PHYSX
		if (PhysAnimData.TargetActor)
		{
			PDI->DrawPoint(P2UVector(PhysAnimData.TargetActor->getGlobalPose().p), TargetActorColor, 3.f, SDPG_World);
		}
#endif

	}
}
#endif
