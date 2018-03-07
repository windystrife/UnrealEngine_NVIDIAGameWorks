// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/ConstraintInstance.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/AnimPhysObjectVersion.h"
#include "HAL/IConsoleManager.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsPublic.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/PhysXSupport.h"

#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

#define LOCTEXT_NAMESPACE "ConstraintInstance"

TAutoConsoleVariable<float> CVarConstraintLinearDampingScale(
	TEXT("p.ConstraintLinearDampingScale"),
	1.f,
	TEXT("The multiplier of constraint linear damping in simulation. Default: 1"),
	ECVF_ReadOnly);

TAutoConsoleVariable<float> CVarConstraintLinearStiffnessScale(
	TEXT("p.ConstraintLinearStiffnessScale"),
	1.f,
	TEXT("The multiplier of constraint linear stiffness in simulation. Default: 1"),
	ECVF_ReadOnly);

TAutoConsoleVariable<float> CVarConstraintAngularDampingScale(
	TEXT("p.ConstraintAngularDampingScale"),
	100000.f,
	TEXT("The multiplier of constraint angular damping in simulation. Default: 100000"),
	ECVF_ReadOnly);

TAutoConsoleVariable<float> CVarConstraintAngularStiffnessScale(
	TEXT("p.ConstraintAngularStiffnessScale"),
	100000.f,
	TEXT("The multiplier of constraint angular stiffness in simulation. Default: 100000"),
	ECVF_ReadOnly);

/** Handy macro for setting BIT of VAR based on the bool CONDITION */
#define SET_DRIVE_PARAM(VAR, CONDITION, BIT)   (VAR) = (CONDITION) ? ((VAR) | (BIT)) : ((VAR) & ~(BIT))

float RevolutionsToRads(const float Revolutions)
{
	return Revolutions * 2.f * PI;
}

FVector RevolutionsToRads(const FVector Revolutions)
{
	return Revolutions * 2.f * PI;
}

#if WITH_PHYSX

physx::PxD6Joint* FConstraintInstance::GetUnbrokenJoint_AssumesLocked() const
{
	return (ConstraintData && !(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN)) ? ConstraintData : nullptr;
}

#if WITH_EDITOR
void FConstraintProfileProperties::SyncChangedConstraintProperties(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	static const FName StiffnessProperty = GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness);
	static const FName MaxForceName = GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce);
	static const FName DampingName = GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping);

	if (TDoubleLinkedList<UProperty*>::TDoubleLinkedListNode* PropertyNode = PropertyChangedEvent.PropertyChain.GetTail())
	{
		if (TDoubleLinkedList<UProperty*>::TDoubleLinkedListNode* ParentProeprtyNode = PropertyNode->GetPrevNode())
		{
			if (UProperty* Property = PropertyNode->GetValue())
			{
				if (UProperty* ParentProperty = ParentProeprtyNode->GetValue())
				{
					const FName PropertyName = Property->GetFName();
					const FName ParentPropertyName = ParentProperty->GetFName();

					if (ParentPropertyName == GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, XDrive))
					{
						if (StiffnessProperty == PropertyName)
						{
							LinearDrive.YDrive.Stiffness = LinearDrive.XDrive.Stiffness;
							LinearDrive.ZDrive.Stiffness = LinearDrive.XDrive.Stiffness;
						}
						else if (MaxForceName == PropertyName)
						{
							LinearDrive.YDrive.MaxForce = LinearDrive.XDrive.MaxForce;
							LinearDrive.ZDrive.MaxForce = LinearDrive.XDrive.MaxForce;
						}
						else if (DampingName == PropertyName)
						{
							LinearDrive.YDrive.Damping = LinearDrive.XDrive.Damping;
							LinearDrive.ZDrive.Damping = LinearDrive.XDrive.Damping;
						}
					}
					else if (ParentPropertyName == GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, SlerpDrive))
					{
						if (StiffnessProperty == PropertyName)
						{
							AngularDrive.SwingDrive.Stiffness = AngularDrive.SlerpDrive.Stiffness;
							AngularDrive.TwistDrive.Stiffness = AngularDrive.SlerpDrive.Stiffness;
						}
						else if (MaxForceName == PropertyName)
						{
							AngularDrive.SwingDrive.MaxForce = AngularDrive.SlerpDrive.MaxForce;
							AngularDrive.TwistDrive.MaxForce = AngularDrive.SlerpDrive.MaxForce;
						}
						else if (DampingName == PropertyName)
						{
							AngularDrive.SwingDrive.Damping = AngularDrive.SlerpDrive.Damping;
							AngularDrive.TwistDrive.Damping = AngularDrive.SlerpDrive.Damping;
						}
					}
				}
			}
		}
	}
}
#endif

bool FConstraintInstance::ExecuteOnUnbrokenJointReadOnly(TFunctionRef<void(const physx::PxD6Joint*)> Func) const
{
	if(ConstraintData)
	{
		SCOPED_SCENE_READ_LOCK(ConstraintData->getScene());

		if(!(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
		{
			Func(ConstraintData);
			return true;
		}
	}

	return false;
}

bool FConstraintInstance::ExecuteOnUnbrokenJointReadWrite(TFunctionRef<void(physx::PxD6Joint*)> Func) const
{
	if (ConstraintData)
	{
		SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());

		if (!(ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN))
		{
			Func(ConstraintData);
			return true;
		}
	}

	return false;
}
#endif //WITH_PHYSX


FConstraintProfileProperties::FConstraintProfileProperties()
	: ProjectionLinearTolerance(5.f)
	, ProjectionAngularTolerance(180.f)
	, LinearBreakThreshold(300.f)
	, AngularBreakThreshold(500.f)
	, bDisableCollision(false)
	, bParentDominates(false)
	, bEnableProjection(true)
	, bAngularBreakable(false)
	, bLinearBreakable(false)
{
}

void FConstraintInstance::UpdateLinearLimit()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		ProfileInstance.LinearLimit.UpdatePhysXLinearLimit_AssumesLocked(Joint, AverageMass, bScaleLinearLimits ? LastKnownScale : 1.f);
	});
#endif
}

void FConstraintInstance::UpdateAngularLimit()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&](PxD6Joint* Joint)
	{
		ProfileInstance.ConeLimit.UpdatePhysXConeLimit_AssumesLocked(Joint, AverageMass);
		ProfileInstance.TwistLimit.UpdatePhysXTwistLimit_AssumesLocked(Joint, AverageMass);
	});
#endif
}

void FConstraintInstance::UpdateBreakable()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&](PxD6Joint* Joint)
	{
		ProfileInstance.UpdatePhysXBreakable_AssumesLocked(Joint);
	});
#endif
}

#if WITH_PHYSX
void FConstraintProfileProperties::UpdatePhysXBreakable_AssumesLocked(PxD6Joint* Joint) const
{
	const float LinearBreakForce = bLinearBreakable ? LinearBreakThreshold : PX_MAX_REAL;
	const float AngularBreakForce = bAngularBreakable ? AngularBreakThreshold : PX_MAX_REAL;

	Joint->setBreakForce(LinearBreakForce, AngularBreakForce);
}
#endif

void FConstraintInstance::UpdateDriveTarget()
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		ProfileInstance.UpdatePhysXDriveTarget_AssumesLocked(Joint);
	});
#endif
}

/** Constructor **/
FConstraintInstance::FConstraintInstance()
	: ConstraintIndex(0)
#if WITH_PHYSX
	, ConstraintData(NULL)
#endif	//WITH_PHYSX
	, SceneIndex(0)
	, bScaleLinearLimits(true)
	, AverageMass(0.f)
#if WITH_PHYSX
	, PhysxUserData(this)
#endif
	, LastKnownScale(1.f)
#if WITH_EDITORONLY_DATA
	, bDisableCollision_DEPRECATED(false)
	, bEnableProjection_DEPRECATED(true)
	, ProjectionLinearTolerance_DEPRECATED(5.f)
	, ProjectionAngularTolerance_DEPRECATED(180.f)
	, LinearXMotion_DEPRECATED(ACM_Locked)
	, LinearYMotion_DEPRECATED(ACM_Locked)
	, LinearZMotion_DEPRECATED(ACM_Locked)
	, LinearLimitSize_DEPRECATED(0.f)
	, bLinearLimitSoft_DEPRECATED(false)
	, LinearLimitStiffness_DEPRECATED(0.f)
	, LinearLimitDamping_DEPRECATED(0.f)
	, bLinearBreakable_DEPRECATED(false)
	, LinearBreakThreshold_DEPRECATED(300.f)
	, AngularSwing1Motion_DEPRECATED(ACM_Free)
	, AngularTwistMotion_DEPRECATED(ACM_Free)
	, AngularSwing2Motion_DEPRECATED(ACM_Free)
	, bSwingLimitSoft_DEPRECATED(true)
	, bTwistLimitSoft_DEPRECATED(true)
	, Swing1LimitAngle_DEPRECATED(45)
	, TwistLimitAngle_DEPRECATED(45)
	, Swing2LimitAngle_DEPRECATED(45)
	, SwingLimitStiffness_DEPRECATED(50)
	, SwingLimitDamping_DEPRECATED(5)
	, TwistLimitStiffness_DEPRECATED(50)
	, TwistLimitDamping_DEPRECATED(5)
	, bAngularBreakable_DEPRECATED(false)
	, AngularBreakThreshold_DEPRECATED(500.f)
	, bLinearXPositionDrive_DEPRECATED(false)
	, bLinearXVelocityDrive_DEPRECATED(false)
	, bLinearYPositionDrive_DEPRECATED(false)
	, bLinearYVelocityDrive_DEPRECATED(false)
	, bLinearZPositionDrive_DEPRECATED(false)
	, bLinearZVelocityDrive_DEPRECATED(false)
	, bLinearPositionDrive_DEPRECATED(false)
	, bLinearVelocityDrive_DEPRECATED(false)
	, LinearPositionTarget_DEPRECATED(ForceInit)
	, LinearVelocityTarget_DEPRECATED(ForceInit)
	, LinearDriveSpring_DEPRECATED(50.0f)
	, LinearDriveDamping_DEPRECATED(1.0f)
	, LinearDriveForceLimit_DEPRECATED(0)
	, bSwingPositionDrive_DEPRECATED(false)
	, bSwingVelocityDrive_DEPRECATED(false)
	, bTwistPositionDrive_DEPRECATED(false)
	, bTwistVelocityDrive_DEPRECATED(false)
	, bAngularOrientationDrive_DEPRECATED(false)
	, bEnableSwingDrive_DEPRECATED(true)
	, bEnableTwistDrive_DEPRECATED(true)
	, bAngularVelocityDrive_DEPRECATED(false)
	, AngularPositionTarget_DEPRECATED(ForceInit)
	, AngularOrientationTarget_DEPRECATED(ForceInit)
	, AngularVelocityTarget_DEPRECATED(ForceInit)
	, AngularDriveSpring_DEPRECATED(50.0f)
	, AngularDriveDamping_DEPRECATED(1.0f)
	, AngularDriveForceLimit_DEPRECATED(0)
#endif	//EDITOR_ONLY_DATA
{
	Pos1 = FVector(0.0f, 0.0f, 0.0f);
	PriAxis1 = FVector(1.0f, 0.0f, 0.0f);
	SecAxis1 = FVector(0.0f, 1.0f, 0.0f);

	Pos2 = FVector(0.0f, 0.0f, 0.0f);
	PriAxis2 = FVector(1.0f, 0.0f, 0.0f);
	SecAxis2 = FVector(0.0f, 1.0f, 0.0f);
}

void FConstraintInstance::SetDisableCollision(bool InDisableCollision)
{
	ProfileInstance.bDisableCollision = InDisableCollision;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxConstraintFlags Flags = Joint->getConstraintFlags();
		if (InDisableCollision)
		{
			Flags &= ~PxConstraintFlag::eCOLLISION_ENABLED;
		}
		else
		{
			Flags |= PxConstraintFlag::eCOLLISION_ENABLED;
		}

		Joint->setConstraintFlags(Flags);
	});
#endif
}

#if WITH_PHYSX
float ComputeAverageMass_AssumesLocked(const PxRigidActor* PActor1, const PxRigidActor* PActor2)
{
	float AverageMass = 0;
	{
		float TotalMass = 0;
		int NumDynamic = 0;

		if (PActor1 && PActor1->is<PxRigidBody>())
		{
			TotalMass += PActor1->is<PxRigidBody>()->getMass();
			++NumDynamic;
		}

		if (PActor2 && PActor2->is<PxRigidBody>())
		{
			TotalMass += PActor2->is<PxRigidBody>()->getMass();
			++NumDynamic;
		}

		check(NumDynamic);
		AverageMass = TotalMass / NumDynamic;
	}

	return AverageMass;
}

/** Finds the common scene and appropriate actors for the passed in body instances. Makes sure to do this without requiring a scene lock*/
bool GetPScene_LockFree(const FBodyInstance* Body1, const FBodyInstance* Body2, UObject* DebugOwner,PxScene*& OutScene)
{
	const int32 SceneIndex1 = Body1 ? Body1->GetSceneIndex() : -1;
	const int32 SceneIndex2 = Body2 ? Body2->GetSceneIndex() : -1;
	OutScene = nullptr;

	//ensure we constrain components from the same scene
	if(SceneIndex1 >= 0 && SceneIndex2 >= 0 && SceneIndex1 != SceneIndex2)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UPrimitiveComponent* PrimComp1 = Body1 ? Body1->OwnerComponent.Get() : nullptr;
		UPrimitiveComponent* PrimComp2 = Body2 ? Body2->OwnerComponent.Get() : nullptr;

		FMessageLog("PIE").Warning()
			->AddToken(FTextToken::Create(LOCTEXT("JointBetweenScenesStart", "Constraint")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("JointBetweenScenesOwner", "'{0}'"), FText::FromString(GetPathNameSafe(DebugOwner)))))
			->AddToken(FTextToken::Create(LOCTEXT("JointBetweenScenesMid", "attempting to create a joint between two actors in different scenes (")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("JointBetweenScenesArgs", "'{0}' and '{1}'"), FText::FromString(GetPathNameSafe(PrimComp1)), FText::FromString(GetPathNameSafe(PrimComp2)))))
			->AddToken(FTextToken::Create(LOCTEXT("JointBetweenScenesEnd", ").  No joint created.")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return false;
	}
	else if(SceneIndex1 >= 0 || SceneIndex2 >= 0)
	{
		OutScene = GetPhysXSceneFromIndex(SceneIndex1 >= 0 ? SceneIndex1 : SceneIndex2);
	}

	return true;	//we are simply using a nullscene which is valid in some cases
}

bool CanActorSimulate(const FBodyInstance* BI, const PxRigidActor* PActor, UObject* DebugOwner)
{
	if (PActor && (PActor->getActorFlags() & PxActorFlag::eDISABLE_SIMULATION))
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const UPrimitiveComponent* PrimComp = BI->OwnerComponent.Get();
		FMessageLog("PIE").Warning()
			->AddToken(FTextToken::Create(LOCTEXT("InvalidBodyStart", "Attempting to create a joint")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("InvalidBodyOwner", "'{0}'"), FText::FromString(GetPathNameSafe(DebugOwner)))))
			->AddToken(FTextToken::Create(LOCTEXT("InvalidBodyMid", "to body")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("InvalidBodyComponent", "'{0}'"), FText::FromString(GetPathNameSafe(PrimComp)))))
			->AddToken(FTextToken::Create(LOCTEXT("InvalidBodyEnd", "which is not eligible for simulation. Is it marked QueryOnly?")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		return false;
	}

	return true;
}

/*various logical checks to find the correct physx actor. Returns true if found valid actors that can be constrained*/
bool GetPActors_AssumesLocked(const FBodyInstance* Body1, const FBodyInstance* Body2, PxRigidActor** PActor1Out, PxRigidActor** PActor2Out, UObject* DebugOwner)
{
	PxRigidActor* PActor1 = Body1 ? Body1->GetPxRigidActor_AssumesLocked() : NULL;
	PxRigidActor* PActor2 = Body2 ? Body2->GetPxRigidActor_AssumesLocked() : NULL;

	// Do not create joint unless you have two actors
	// Do not create joint unless one of the actors is dynamic
	if ((!PActor1 || !PActor1->is<PxRigidBody>()) && (!PActor2 || !PActor2->is<PxRigidBody>()))
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FMessageLog("PIE").Warning()
			->AddToken(FTextToken::Create(LOCTEXT("TwoStaticBodiesWarningStart", "Constraint in")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("TwoStaticBodiesWarningOwner", "'{0}'"), FText::FromString(GetPathNameSafe(DebugOwner)))))
			->AddToken(FTextToken::Create(LOCTEXT("TwoStaticBodiesWarningEnd", "attempting to create a joint between objects that are both static.  No joint created.")));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return false;
	}

	if(PActor1 == PActor2)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const UPrimitiveComponent* PrimComp = Body1->OwnerComponent.Get();
		FMessageLog("PIE").Warning()
			->AddToken(FTextToken::Create(LOCTEXT("SameBodyWarningStart", "Constraint in")))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("SameBodyWarningOwner", "'{0}'"), FText::FromString(GetPathNameSafe(DebugOwner)))))
			->AddToken(FTextToken::Create(LOCTEXT("SameBodyWarningMid", "attempting to create a joint to the same body")))
			->AddToken(FUObjectToken::Create(PrimComp));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return false;
	}

	if(!CanActorSimulate(Body1, PActor1, DebugOwner) || !CanActorSimulate(Body2, PActor2, DebugOwner))
	{
		return false;
	}
	
	// Need to worry about the case where one is static and one is dynamic, and make sure the static scene is used which matches the dynamic scene
	if (PActor1 != NULL && PActor2 != NULL)
	{
		if (PActor1->is<PxRigidStatic>() && PActor2->is<PxRigidBody>())
		{
			const uint32 SceneType = Body2->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor1 = Body1->GetPxRigidActorFromScene_AssumesLocked(SceneType);
		}
		else
		if (PActor2->is<PxRigidStatic>() && PActor1->is<PxRigidBody>())
		{
			const uint32 SceneType = Body1->RigidActorSync != NULL ? PST_Sync : PST_Async;
			PActor2 = Body2->GetPxRigidActorFromScene_AssumesLocked(SceneType);
		}
	}

	*PActor1Out = PActor1;
	*PActor2Out = PActor2;
	return true;
}

bool FConstraintInstance::CreatePxJoint_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene)
{
	LLM_SCOPE(ELLMTag::PhysX);

	ConstraintData = nullptr;

	FTransform Local1 = GetRefFrame(EConstraintFrame::Frame1);
	if(PActor1)
	{
		Local1.ScaleTranslation(FVector(LastKnownScale));
	}

	checkf(Local1.IsValid() && !Local1.ContainsNaN(), TEXT("%s"), *Local1.ToString());

	FTransform Local2 = GetRefFrame(EConstraintFrame::Frame2);
	if(PActor2)
	{
		Local2.ScaleTranslation(FVector(LastKnownScale));
	}
	
	checkf(Local2.IsValid() && !Local2.ContainsNaN(), TEXT("%s"), *Local2.ToString());

	SCOPED_SCENE_WRITE_LOCK(PScene);

	// Because PhysX keeps limits/axes locked in the first body reference frame, whereas Unreal keeps them in the second body reference frame, we have to flip the bodies here.
	PxD6Joint* PD6Joint = PxD6JointCreate(*GPhysXSDK, PActor2, U2PTransform(Local2), PActor1, U2PTransform(Local1));

	if (PD6Joint == nullptr)
	{
		UE_LOG(LogPhysics, Log, TEXT("URB_ConstraintInstance::InitConstraint - Invalid 6DOF joint (%s)"), *JointName.ToString());
		return false;
	}

	///////// POINTERS
	PD6Joint->userData = &PhysxUserData;

	if(PScene)
	{
		// Remember reference to scene index.
		FPhysScene* RBScene = FPhysxUserData::Get<FPhysScene>(PScene->userData);
		if (RBScene->GetPhysXScene(PST_Sync) == PScene)
		{
			SceneIndex = RBScene->PhysXSceneIndex[PST_Sync];
		}
		else
			if (RBScene->GetPhysXScene(PST_Async) == PScene)
			{
				SceneIndex = RBScene->PhysXSceneIndex[PST_Async];
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("URB_ConstraintInstance::InitConstraint: PxScene has inconsistent FPhysScene userData.  No joint created."));
				return false;
			}
	}
	

	ConstraintData = PD6Joint;
	return true;
}

void FConstraintProfileProperties::UpdatePhysXConstraintFlags_AssumesLocked(PxD6Joint* Joint) const
{
	PxConstraintFlags Flags = PxConstraintFlags();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Flags |= PxConstraintFlag::eVISUALIZATION;
#endif

	if (!bDisableCollision)
	{
		Flags |= PxConstraintFlag::eCOLLISION_ENABLED;
	}

	if (bEnableProjection)
	{
		Flags |= PxConstraintFlag::ePROJECTION;

		Joint->setProjectionLinearTolerance(ProjectionLinearTolerance);
		Joint->setProjectionAngularTolerance(FMath::DegreesToRadians(ProjectionAngularTolerance));
	}

	if(bParentDominates)
	{
		Joint->setInvMassScale0(0.0f);
		Joint->setInvMassScale1(1.0f);

		Joint->setInvInertiaScale0(0.0f);
		Joint->setInvInertiaScale1(1.0f);
	}

	Joint->setConstraintFlags(Flags);
}


void FConstraintInstance::UpdateAverageMass_AssumesLocked(const PxRigidActor* PActor1, const PxRigidActor* PActor2)
{
	AverageMass = ComputeAverageMass_AssumesLocked(PActor1, PActor2);
}

void EnsureSleepingActorsStaySleeping_AssumesLocked(PxRigidActor* PActor1, PxRigidActor* PActor2)
{
	PxRigidDynamic* RigidDynamic1 = PActor1 ? PActor1->is<PxRigidDynamic>() : nullptr;
	PxRigidDynamic* RigidDynamic2 = PActor2 ? PActor2->is<PxRigidDynamic>() : nullptr;

	// record if actors are asleep before creating joint, so we can sleep them afterwards if so (creating joint wakes them)
	const bool bActor1Asleep = (RigidDynamic1 == nullptr) || (RigidDynamic1->getScene() != nullptr && RigidDynamic1->isSleeping());
	const bool bActor2Asleep = (RigidDynamic2 == nullptr) || (RigidDynamic2->getScene() != nullptr && RigidDynamic2->isSleeping());

	// creation of joints wakes up rigid bodies, so we put them to sleep again if both were initially asleep
	if (bActor1Asleep && bActor2Asleep)
	{
		if (PActor1 && !IsRigidBodyKinematic_AssumesLocked(RigidDynamic1))
		{
			if(RigidDynamic1)
			{
				RigidDynamic1->putToSleep();
			}
		}

		if (PActor2 && !IsRigidBodyKinematic_AssumesLocked(RigidDynamic2))
		{
			if(RigidDynamic2)
			{
				RigidDynamic2->putToSleep();
			}
			
		}
	}
}

#endif

/** 
 *	Create physics engine constraint.
 */
void FConstraintInstance::InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, float InScale, UObject* DebugOwner, FOnConstraintBroken InConstraintBrokenDelegate)
{
#if WITH_PHYSX
	PxRigidActor* PActor1 = nullptr;
	PxRigidActor* PActor2 = nullptr;
	PxScene* PScene;
	bool bValidScene = GetPScene_LockFree(Body1, Body2, DebugOwner, PScene);
	SCOPED_SCENE_WRITE_LOCK(PScene);
	{
		const bool bValidConstraintSetup = bValidScene && GetPActors_AssumesLocked(Body1, Body2, &PActor1, &PActor2, DebugOwner);
		if (!bValidConstraintSetup)
		{
			return;
		}

		InitConstraintPhysX_AssumesLocked(PActor1, PActor2, PScene, InScale, InConstraintBrokenDelegate);
	}
	
#endif // WITH_PHYSX

}

#if WITH_PHYSX
void FConstraintInstance::InitConstraintPhysX_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene, float InScale, FOnConstraintBroken InConstraintBrokenDelegate)
{
	OnConstraintBrokenDelegate = InConstraintBrokenDelegate;
	LastKnownScale = InScale;

	PhysxUserData = FPhysxUserData(this);

	// if there's already a constraint, get rid of it first
	if (ConstraintData)
	{
		TermConstraint();
	}

	if (!CreatePxJoint_AssumesLocked(PActor1, PActor2, PScene))
	{
		return;
	}
	
	// update mass
	UpdateAverageMass_AssumesLocked(PActor1, PActor2);

	ProfileInstance.UpdatePhysX_AssumesLocked(ConstraintData, AverageMass, bScaleLinearLimits ? LastKnownScale : 1.f);

	if(PScene)
	{
		EnsureSleepingActorsStaySleeping_AssumesLocked(PActor1, PActor2);
	}
}
#endif

#if WITH_PHYSX
void FConstraintProfileProperties::UpdatePhysX_AssumesLocked(PxD6Joint* Joint, float AverageMass, float UseScale) const
{
	// flags and projection settings
	UpdatePhysXConstraintFlags_AssumesLocked(Joint);

	//limits
	LinearLimit.UpdatePhysXLinearLimit_AssumesLocked(Joint, AverageMass, UseScale);
	ConeLimit.UpdatePhysXConeLimit_AssumesLocked(Joint, AverageMass);
	TwistLimit.UpdatePhysXTwistLimit_AssumesLocked(Joint, AverageMass);

	//breakable
	UpdatePhysXBreakable_AssumesLocked(Joint);

	//motors
	LinearDrive.UpdatePhysXLinearDrive_AssumesLocked(Joint);
	AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	UpdatePhysXDriveTarget_AssumesLocked(Joint);

}
#endif

#if WITH_PHYSX
void FConstraintProfileProperties::UpdatePhysXDriveTarget_AssumesLocked(PxD6Joint* Joint) const
{
	const FQuat OrientationTargetQuat(AngularDrive.OrientationTarget);

	Joint->setDrivePosition(PxTransform(U2PVector(LinearDrive.PositionTarget), U2PQuat(OrientationTargetQuat)));
	Joint->setDriveVelocity(U2PVector(LinearDrive.VelocityTarget), U2PVector(RevolutionsToRads(AngularDrive.AngularVelocityTarget)));

}
#endif

void FConstraintInstance::TermConstraint()
{
#if WITH_PHYSX
	if (!ConstraintData)
	{
		return;
	}

	// use correct scene
	PxScene* PScene = GetPhysXSceneFromIndex(SceneIndex);
	{
		SCOPED_SCENE_WRITE_LOCK(PScene);
		ConstraintData->release();
	}

	ConstraintData = nullptr;
#endif
}

bool FConstraintInstance::IsTerminated() const
{
#if WITH_PHYSX
	return (ConstraintData == nullptr);
#else 
	return true;
#endif //WITH_PHYSX
}

bool FConstraintInstance::IsValidConstraintInstance() const
{
#if WITH_PHYSX
	return ConstraintData != nullptr;
#else
	return false;
#endif // WITH_PHYSX
}

void FConstraintInstance::CopyProfilePropertiesFrom(const FConstraintProfileProperties& FromProperties)
{
	ProfileInstance = FromProperties;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&](PxD6Joint* Joint)
	{
		ProfileInstance.UpdatePhysX_AssumesLocked(Joint, AverageMass, bScaleLinearLimits ? LastKnownScale : 1.f);
	});
#endif
}

void FConstraintInstance::CopyConstraintGeometryFrom(const FConstraintInstance* FromInstance)
{
	Pos1 = FromInstance->Pos1;
	PriAxis1 = FromInstance->PriAxis1;
	SecAxis1 = FromInstance->SecAxis1;

	Pos2 = FromInstance->Pos2;
	PriAxis2 = FromInstance->PriAxis2;
	SecAxis2 = FromInstance->SecAxis2;
}

void FConstraintInstance::CopyConstraintParamsFrom(const FConstraintInstance* FromInstance)
{
#if WITH_PHYSX
	check(!FromInstance->ConstraintData);
	check(!ConstraintData);
#endif
	check(FromInstance->SceneIndex == 0);

	*this = *FromInstance;
}

FTransform FConstraintInstance::GetRefFrame(EConstraintFrame::Type Frame) const
{
	FTransform Result;

	if(Frame == EConstraintFrame::Frame1)
	{
		Result = FTransform(PriAxis1, SecAxis1, PriAxis1 ^ SecAxis1, Pos1);
	}
	else
	{
		Result = FTransform(PriAxis2, SecAxis2, PriAxis2 ^ SecAxis2, Pos2);
	}

	float Error = FMath::Abs( Result.GetDeterminant() - 1.0f );
	if(Error > 0.01f)
	{
		UE_LOG(LogPhysics, Warning,  TEXT("FConstraintInstance::GetRefFrame : Contained scale."));
	}

	return Result;
}

#if WITH_PHYSX
FORCEINLINE physx::PxJointActorIndex::Enum U2PConstraintFrame(EConstraintFrame::Type Frame)
{
	// Swap frame order, since Unreal reverses physx order
	return (Frame == EConstraintFrame::Frame1) ? physx::PxJointActorIndex::eACTOR1 : physx::PxJointActorIndex::eACTOR0;
}
#endif


void FConstraintInstance::SetRefFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
#endif
	if(Frame == EConstraintFrame::Frame1)
	{
		Pos1 = RefFrame.GetTranslation();
		PriAxis1 = RefFrame.GetUnitAxis( EAxis::X );
		SecAxis1 = RefFrame.GetUnitAxis( EAxis::Y );
	}
	else
	{
		Pos2 = RefFrame.GetTranslation();
		PriAxis2 = RefFrame.GetUnitAxis( EAxis::X );
		SecAxis2 = RefFrame.GetUnitAxis( EAxis::Y );
	}

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxTransform PxRefFrame = U2PTransform(RefFrame);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
#endif

}

void FConstraintInstance::SetRefPosition(EConstraintFrame::Type Frame, const FVector& RefPosition)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
#endif

	if (Frame == EConstraintFrame::Frame1)
	{
		Pos1 = RefPosition;
	}
	else
	{
		Pos2 = RefPosition;
	}

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxTransform PxRefFrame = ConstraintData->getLocalPose(PxFrame);
		PxRefFrame.p = U2PVector(RefPosition);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
#endif
}

void FConstraintInstance::SetRefOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis)
{
#if WITH_PHYSX
	PxJointActorIndex::Enum PxFrame = U2PConstraintFrame(Frame);
	FVector RefPos;
#endif
		
	if (Frame == EConstraintFrame::Frame1)
	{
		RefPos = Pos1;
		PriAxis1 = PriAxis;
		SecAxis1 = SecAxis;
	}
	else
	{
		RefPos = Pos2;
		PriAxis2 = PriAxis;
		SecAxis2 = SecAxis;
	}
	
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		FTransform URefTransform = FTransform(PriAxis, SecAxis, PriAxis ^ SecAxis, RefPos);
		PxTransform PxRefFrame = U2PTransform(URefTransform);
		Joint->setLocalPose(PxFrame, PxRefFrame);
	});
#endif
}

/** Get the position of this constraint in world space. */
FVector FConstraintInstance::GetConstraintLocation()
{
#if WITH_PHYSX
	PxD6Joint* Joint = (PxD6Joint*)	ConstraintData;
	if (!Joint)
	{
		return FVector::ZeroVector;
	}

	PxRigidActor* JointActor0, *JointActor1;
	Joint->getActors(JointActor0, JointActor1);

	PxVec3 JointPos(0);

	// get the first anchor point in global frame
	if(JointActor0)
	{
		JointPos = JointActor0->getGlobalPose().transform(Joint->getLocalPose(PxJointActorIndex::eACTOR0).p);
	}

	// get the second archor point in global frame
	if(JointActor1)
	{
		JointPos += JointActor1->getGlobalPose().transform(Joint->getLocalPose(PxJointActorIndex::eACTOR1).p);
	}

	JointPos *= 0.5f;
	
	return P2UVector(JointPos);

#else
	return FVector::ZeroVector;
#endif
}

void FConstraintInstance::GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce)
{
	OutLinearForce = FVector::ZeroVector;
	OutAngularForce = FVector::ZeroVector;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadOnly([&] (const PxD6Joint* Joint)
	{
		PxVec3 PxOutLinearForce;
		PxVec3 PxOutAngularForce;
		Joint->getConstraint()->getForce(PxOutLinearForce, PxOutAngularForce);

		OutLinearForce = P2UVector(PxOutLinearForce);
		OutAngularForce = P2UVector(PxOutAngularForce);
	});
#endif
}

bool FConstraintInstance::IsBroken()
{
#if WITH_PHYSX
	if (ConstraintData)
	{
		SCOPED_SCENE_READ_LOCK(ConstraintData->getScene());

		if (ConstraintData->getConstraintFlags()&PxConstraintFlag::eBROKEN)
		{
			return true;
		}
	}
#endif
	return false;
}

/** Function for turning linear position drive on and off. */
void FConstraintInstance::SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	ProfileInstance.LinearDrive.SetLinearPositionDrive(bEnableXDrive, bEnableYDrive, bEnableZDrive);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.LinearDrive.UpdatePhysXLinearDrive_AssumesLocked(Joint);
	});
#endif
}

/** Function for turning linear velocity drive on and off. */
void FConstraintInstance::SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	ProfileInstance.LinearDrive.SetLinearVelocityDrive(bEnableXDrive, bEnableYDrive, bEnableZDrive);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.LinearDrive.UpdatePhysXLinearDrive_AssumesLocked(Joint);
	});
#endif
}

void FConstraintInstance::SetOrientationDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive)
{
	ProfileInstance.AngularDrive.SetOrientationDriveTwistAndSwing(InEnableTwistDrive, InEnableSwingDrive);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}

void FConstraintInstance::SetOrientationDriveSLERP(bool InEnableSLERP)
{
	ProfileInstance.AngularDrive.SetOrientationDriveSLERP(InEnableSLERP);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}

/** Set which twist and swing angular velocity drives are enabled. Only applicable when Twist And Swing drive mode is used */
void FConstraintInstance::SetAngularVelocityDriveTwistAndSwing(bool bInEnableTwistDrive, bool bInEnableSwingDrive)
{
	ProfileInstance.AngularDrive.SetAngularVelocityDriveTwistAndSwing(bInEnableTwistDrive, bInEnableSwingDrive);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}

/** Set whether the SLERP angular velocity drive is enabled. Only applicable when SLERP drive mode is used */
void FConstraintInstance::SetAngularVelocityDriveSLERP(bool bInEnableSLERP)
{
	ProfileInstance.AngularDrive.SetAngularVelocityDriveSLERP(bInEnableSLERP);
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this](PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}

/** Set the angular drive mode */
void FConstraintInstance::SetAngularDriveMode(EAngularDriveMode::Type DriveMode)
{
	ProfileInstance.AngularDrive.SetAngularDriveMode(DriveMode);

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&](PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}

/** Function for setting linear position target. */
void FConstraintInstance::SetLinearPositionTarget(const FVector& InPosTarget)
{
	// If settings are the same, don't do anything.
	if( ProfileInstance.LinearDrive.PositionTarget == InPosTarget )
	{
		return;
	}

	ProfileInstance.LinearDrive.PositionTarget = InPosTarget;

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([InPosTarget](PxD6Joint* Joint)
	{
		PxVec3 Pos = U2PVector(InPosTarget);
		Joint->setDrivePosition(PxTransform(Pos, Joint->getDrivePosition().q));
	});
#endif
}

/** Function for setting linear velocity target. */
void FConstraintInstance::SetLinearVelocityTarget(const FVector& InVelTarget)
{
	// If settings are the same, don't do anything.
	if (ProfileInstance.LinearDrive.VelocityTarget == InVelTarget)
	{
		return;
	}

	ProfileInstance.LinearDrive.VelocityTarget = InVelTarget;

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([InVelTarget](PxD6Joint* Joint)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 NewLinearVel = U2PVector(InVelTarget);
		Joint->setDriveVelocity(NewLinearVel, CurrentAngVel);
	});
#endif
}

/** Function for setting linear motor parameters. */
void FConstraintInstance::SetLinearDriveParams(float InSpring, float InDamping, float InForceLimit)
{
	ProfileInstance.LinearDrive.SetDriveParams(InSpring, InDamping, InForceLimit);

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([this] (PxD6Joint* Joint)
	{
		ProfileInstance.LinearDrive.UpdatePhysXLinearDrive_AssumesLocked(Joint);
	});
#endif
}

/** Function for setting target angular position. */
void FConstraintInstance::SetAngularOrientationTarget(const FQuat& InOrientationTarget)
{
	FRotator OrientationTargetRot(InOrientationTarget);

	// If settings are the same, don't do anything.
	if( ProfileInstance.AngularDrive.OrientationTarget == OrientationTargetRot )
	{
		return;
	}

	ProfileInstance.AngularDrive.OrientationTarget = OrientationTargetRot;

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([InOrientationTarget](PxD6Joint* Joint)
	{
		PxQuat Quat = U2PQuat(InOrientationTarget);
		Joint->setDrivePosition(PxTransform(Joint->getDrivePosition().p, Quat));
	});
#endif
}

float FConstraintInstance::GetCurrentSwing1() const
{
	float Swing1 = 0.f;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadOnly([&Swing1](const PxD6Joint* Joint)
	{
		Swing1 = Joint->getSwingZAngle();
	});
#endif

	return Swing1;
}

float FConstraintInstance::GetCurrentSwing2() const
{
	float Swing2 = 0.f;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadOnly([&Swing2](const PxD6Joint* Joint)
	{
		Swing2 = Joint->getSwingYAngle();
	});
#endif

	return Swing2;
}

float FConstraintInstance::GetCurrentTwist() const
{
	float Twist = 0.f;
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadOnly([&Twist](const PxD6Joint* Joint)
	{
		Twist = Joint->getTwist();
	});
#endif

	return Twist;
}


/** Function for setting target angular velocity. */
void FConstraintInstance::SetAngularVelocityTarget(const FVector& InVelTarget)
{
	// If settings are the same, don't do anything.
	if( ProfileInstance.AngularDrive.AngularVelocityTarget == InVelTarget )
	{
		return;
	}

	ProfileInstance.AngularDrive.AngularVelocityTarget = InVelTarget;

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([InVelTarget](PxD6Joint* Joint)
	{
		PxVec3 CurrentLinearVel, CurrentAngVel;
		Joint->getDriveVelocity(CurrentLinearVel, CurrentAngVel);

		PxVec3 AngVel = U2PVector(RevolutionsToRads(InVelTarget));
		Joint->setDriveVelocity(CurrentLinearVel, AngVel);
	});
#endif
}

/** Function for setting angular motor parameters. */
void FConstraintInstance::SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit)
{
	ProfileInstance.AngularDrive.SetDriveParams(InSpring, InDamping, InForceLimit);

#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		ProfileInstance.AngularDrive.UpdatePhysXAngularDrive_AssumesLocked(Joint);
	});
#endif
}


/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup) */
void FConstraintInstance::SetAngularDOFLimitScale(float InSwing1LimitScale, float InSwing2LimitScale, float InTwistLimitScale)
{
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		if ( ProfileInstance.ConeLimit.Swing1Motion == ACM_Limited || ProfileInstance.ConeLimit.Swing2Motion == ACM_Limited )
		{
			// PhysX swing directions are different from Unreal's - so change here.
			if (ProfileInstance.ConeLimit.Swing1Motion == ACM_Limited)
			{
				Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
			}

			if (ProfileInstance.ConeLimit.Swing2Motion == ACM_Limited)
			{
				Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
			}
		
			//The limit values need to be clamped so it will be valid in PhysX
			PxReal ZLimitAngle = FMath::ClampAngle(ProfileInstance.ConeLimit.Swing1LimitDegrees * InSwing1LimitScale, KINDA_SMALL_NUMBER, 179.9999f) * (PI/180.0f);
			PxReal YLimitAngle = FMath::ClampAngle(ProfileInstance.ConeLimit.Swing2LimitDegrees * InSwing2LimitScale, KINDA_SMALL_NUMBER, 179.9999f) * (PI/180.0f);
			PxReal LimitContactDistance =  FMath::DegreesToRadians(FMath::Max(1.f, ProfileInstance.ConeLimit.ContactDistance * FMath::Min(InSwing1LimitScale, InSwing2LimitScale)));

			Joint->setSwingLimit(PxJointLimitCone(YLimitAngle, ZLimitAngle, LimitContactDistance));
		}

		if ( ProfileInstance.ConeLimit.Swing1Motion  == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLOCKED);
		}

		if ( ProfileInstance.ConeLimit.Swing2Motion  == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLOCKED);
		}

		if ( ProfileInstance.TwistLimit.TwistMotion == ACM_Limited )
		{
			Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
			const float TwistLimitRad	= ProfileInstance.TwistLimit.TwistLimitDegrees * InTwistLimitScale * (PI/180.0f);
			PxReal LimitContactDistance =  FMath::DegreesToRadians(FMath::Max(1.f, ProfileInstance.ConeLimit.ContactDistance * InTwistLimitScale));

			Joint->setTwistLimit(PxJointAngularLimitPair(-TwistLimitRad, TwistLimitRad, LimitContactDistance)); 
		}
		else if ( ProfileInstance.TwistLimit.TwistMotion == ACM_Locked )
		{
			Joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLOCKED);
		}
		
	});
#endif
}

/** Allows you to dynamically change the size of the linear limit 'sphere'. */
void FConstraintInstance::SetLinearLimitSize(float NewLimitSize)
{
	//TODO: Is this supposed to be scaling the linear limit? The code just sets it directly.
#if WITH_PHYSX
	ExecuteOnUnbrokenJointReadWrite([&] (PxD6Joint* Joint)
	{
		PxReal LimitContactDistance = 1.f * (PI / 180.0f);
		Joint->setLinearLimit(PxJointLinearLimit(GPhysXSDK->getTolerancesScale(), NewLimitSize, LimitContactDistance * GPhysXSDK->getTolerancesScale().length)); // LOC_MOD33 need to scale the contactDistance if not using its default value
	});
#endif
}

bool FConstraintInstance::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);
	return false;	//We only have this function to mark custom GUID. Still want serialize tagged properties
}

void FConstraintInstance::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_STIFFNESS_AND_DAMPING_SCALE)
	{
		LinearLimitStiffness_DEPRECATED		/= CVarConstraintAngularStiffnessScale.GetValueOnGameThread();
		SwingLimitStiffness_DEPRECATED		/= CVarConstraintAngularStiffnessScale.GetValueOnGameThread();
		TwistLimitStiffness_DEPRECATED		/= CVarConstraintAngularStiffnessScale.GetValueOnGameThread();
		LinearLimitDamping_DEPRECATED		/=  CVarConstraintAngularDampingScale.GetValueOnGameThread();
		SwingLimitDamping_DEPRECATED		/=  CVarConstraintAngularDampingScale.GetValueOnGameThread();
		TwistLimitDamping_DEPRECATED		/=  CVarConstraintAngularDampingScale.GetValueOnGameThread();
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_FIXUP_MOTOR_UNITS)
	{
		AngularVelocityTarget_DEPRECATED *= 1.f / (2.f * PI);	//we want to use revolutions per second - old system was using radians directly
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_CONSTRAINT_INSTANCE_MOTOR_FLAGS)
	{
		bLinearXVelocityDrive_DEPRECATED = LinearVelocityTarget_DEPRECATED.X != 0.f;
		bLinearYVelocityDrive_DEPRECATED = LinearVelocityTarget_DEPRECATED.Y != 0.f;
		bLinearZVelocityDrive_DEPRECATED = LinearVelocityTarget_DEPRECATED.Z != 0.f;
	}
	
	if (Ar.IsLoading() && Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::ConstraintInstanceBehaviorParameters)
	{
		//Need to move all the deprecated properties into the new profile struct
		ProfileInstance.bDisableCollision = bDisableCollision_DEPRECATED;
		ProfileInstance.bEnableProjection = bEnableProjection_DEPRECATED;
		ProfileInstance.ProjectionLinearTolerance = ProjectionLinearTolerance_DEPRECATED;
		ProfileInstance.ProjectionAngularTolerance = ProjectionAngularTolerance_DEPRECATED;
		ProfileInstance.LinearLimit.XMotion = LinearXMotion_DEPRECATED;
		ProfileInstance.LinearLimit.YMotion = LinearYMotion_DEPRECATED;
		ProfileInstance.LinearLimit.ZMotion = LinearZMotion_DEPRECATED;
		ProfileInstance.LinearLimit.Limit = LinearLimitSize_DEPRECATED;
		ProfileInstance.LinearLimit.bSoftConstraint = bLinearLimitSoft_DEPRECATED;
		ProfileInstance.LinearLimit.Stiffness = LinearLimitStiffness_DEPRECATED;
		ProfileInstance.LinearLimit.Damping = LinearLimitDamping_DEPRECATED;
		ProfileInstance.bLinearBreakable = bLinearBreakable_DEPRECATED;
		ProfileInstance.LinearBreakThreshold = LinearBreakThreshold_DEPRECATED;
		ProfileInstance.ConeLimit.Swing1Motion = AngularSwing1Motion_DEPRECATED;
		ProfileInstance.TwistLimit.TwistMotion = AngularTwistMotion_DEPRECATED;
		ProfileInstance.ConeLimit.Swing2Motion = AngularSwing2Motion_DEPRECATED;
		ProfileInstance.ConeLimit.bSoftConstraint = bSwingLimitSoft_DEPRECATED;
		ProfileInstance.TwistLimit.bSoftConstraint = bTwistLimitSoft_DEPRECATED;
		ProfileInstance.ConeLimit.Swing1LimitDegrees = Swing1LimitAngle_DEPRECATED;
		ProfileInstance.TwistLimit.TwistLimitDegrees = TwistLimitAngle_DEPRECATED;
		ProfileInstance.ConeLimit.Swing2LimitDegrees = Swing2LimitAngle_DEPRECATED;
		ProfileInstance.ConeLimit.Stiffness = SwingLimitStiffness_DEPRECATED;
		ProfileInstance.ConeLimit.Damping = SwingLimitDamping_DEPRECATED;
		ProfileInstance.TwistLimit.Stiffness = TwistLimitStiffness_DEPRECATED;
		ProfileInstance.TwistLimit.Damping = TwistLimitDamping_DEPRECATED;
		ProfileInstance.bAngularBreakable = bAngularBreakable_DEPRECATED;
		ProfileInstance.AngularBreakThreshold = AngularBreakThreshold_DEPRECATED;

		//we no longer have a single control for all linear axes. If it was off we ensure all individual drives are off. If it's on we just leave things alone.
		//This loses a bit of info, but the ability to toggle drives on and off at runtime was very obfuscated so hopefuly this doesn't hurt too many people. They can still toggle individual drives on and off
		ProfileInstance.LinearDrive.XDrive.bEnablePositionDrive = bLinearXPositionDrive_DEPRECATED && bLinearPositionDrive_DEPRECATED;
		ProfileInstance.LinearDrive.XDrive.bEnableVelocityDrive = bLinearXVelocityDrive_DEPRECATED && bLinearVelocityDrive_DEPRECATED;
		ProfileInstance.LinearDrive.YDrive.bEnablePositionDrive = bLinearYPositionDrive_DEPRECATED && bLinearPositionDrive_DEPRECATED;
		ProfileInstance.LinearDrive.YDrive.bEnableVelocityDrive = bLinearYVelocityDrive_DEPRECATED && bLinearVelocityDrive_DEPRECATED;
		ProfileInstance.LinearDrive.ZDrive.bEnablePositionDrive = bLinearZPositionDrive_DEPRECATED && bLinearPositionDrive_DEPRECATED;
		ProfileInstance.LinearDrive.ZDrive.bEnableVelocityDrive = bLinearZVelocityDrive_DEPRECATED && bLinearVelocityDrive_DEPRECATED;
		
		ProfileInstance.LinearDrive.PositionTarget = LinearPositionTarget_DEPRECATED;
		ProfileInstance.LinearDrive.VelocityTarget = LinearVelocityTarget_DEPRECATED;

		//Linear drives now set settings per axis so duplicate old data
		ProfileInstance.LinearDrive.XDrive.Stiffness = LinearDriveSpring_DEPRECATED;
		ProfileInstance.LinearDrive.YDrive.Stiffness = LinearDriveSpring_DEPRECATED;
		ProfileInstance.LinearDrive.ZDrive.Stiffness = LinearDriveSpring_DEPRECATED;
		ProfileInstance.LinearDrive.XDrive.Damping = LinearDriveDamping_DEPRECATED;
		ProfileInstance.LinearDrive.YDrive.Damping = LinearDriveDamping_DEPRECATED;
		ProfileInstance.LinearDrive.ZDrive.Damping = LinearDriveDamping_DEPRECATED;
		ProfileInstance.LinearDrive.XDrive.MaxForce = LinearDriveForceLimit_DEPRECATED;
		ProfileInstance.LinearDrive.YDrive.MaxForce = LinearDriveForceLimit_DEPRECATED;
		ProfileInstance.LinearDrive.ZDrive.MaxForce = LinearDriveForceLimit_DEPRECATED;

		//We now expose twist swing and slerp drive directly. In the old system you had a single switch, but then there was also special switches for disabling twist and swing
		//Technically someone COULD disable these, but they are not exposed in editor so it seems very unlikely. So if they are true and angular orientation is false we override it
		ProfileInstance.AngularDrive.SwingDrive.bEnablePositionDrive = bEnableSwingDrive_DEPRECATED && bAngularOrientationDrive_DEPRECATED;
		ProfileInstance.AngularDrive.SwingDrive.bEnableVelocityDrive = bEnableSwingDrive_DEPRECATED && bAngularVelocityDrive_DEPRECATED;
		ProfileInstance.AngularDrive.TwistDrive.bEnablePositionDrive = bEnableTwistDrive_DEPRECATED && bAngularOrientationDrive_DEPRECATED;
		ProfileInstance.AngularDrive.TwistDrive.bEnableVelocityDrive = bEnableTwistDrive_DEPRECATED && bAngularVelocityDrive_DEPRECATED;
		ProfileInstance.AngularDrive.SlerpDrive.bEnablePositionDrive = bAngularOrientationDrive_DEPRECATED;
		ProfileInstance.AngularDrive.SlerpDrive.bEnableVelocityDrive = bAngularVelocityDrive_DEPRECATED;

		ProfileInstance.AngularDrive.AngularDriveMode = AngularDriveMode_DEPRECATED;
		ProfileInstance.AngularDrive.OrientationTarget = AngularOrientationTarget_DEPRECATED;
		ProfileInstance.AngularDrive.AngularVelocityTarget = AngularVelocityTarget_DEPRECATED;

		//duplicate drive spring data into all 3 drives
		ProfileInstance.AngularDrive.SwingDrive.Stiffness = AngularDriveSpring_DEPRECATED;
		ProfileInstance.AngularDrive.TwistDrive.Stiffness = AngularDriveSpring_DEPRECATED;
		ProfileInstance.AngularDrive.SlerpDrive.Stiffness = AngularDriveSpring_DEPRECATED;
		ProfileInstance.AngularDrive.SwingDrive.Damping = AngularDriveDamping_DEPRECATED;
		ProfileInstance.AngularDrive.TwistDrive.Damping = AngularDriveDamping_DEPRECATED;
		ProfileInstance.AngularDrive.SlerpDrive.Damping = AngularDriveDamping_DEPRECATED;
		ProfileInstance.AngularDrive.SwingDrive.MaxForce = AngularDriveForceLimit_DEPRECATED;
		ProfileInstance.AngularDrive.TwistDrive.MaxForce = AngularDriveForceLimit_DEPRECATED;
		ProfileInstance.AngularDrive.SlerpDrive.MaxForce = AngularDriveForceLimit_DEPRECATED;

	}

	if (Ar.IsLoading() && Ar.CustomVer(FAnimPhysObjectVersion::GUID) < FAnimPhysObjectVersion::TuneSoftLimitStiffnessAndDamping)
	{
		//Handle the fact that 0,0 used to mean hard limit, but now means free
		if(ProfileInstance.LinearLimit.Stiffness == 0.f && ProfileInstance.LinearLimit.Damping == 0.f)
		{
			ProfileInstance.LinearLimit.bSoftConstraint = false;
		}

		if (ProfileInstance.ConeLimit.Stiffness == 0.f && ProfileInstance.ConeLimit.Damping == 0.f)
		{
			ProfileInstance.ConeLimit.bSoftConstraint = false;
		}

		if (ProfileInstance.TwistLimit.Stiffness == 0.f && ProfileInstance.TwistLimit.Damping == 0.f)
		{
			ProfileInstance.TwistLimit.bSoftConstraint = false;
		}

		//Now handle the new linear spring stiffness and damping coefficient
		if(CVarConstraintAngularStiffnessScale.GetValueOnGameThread() > 0.f)
		{
			ProfileInstance.LinearLimit.Stiffness *= CVarConstraintAngularStiffnessScale.GetValueOnGameThread() / CVarConstraintLinearStiffnessScale.GetValueOnGameThread();
		}

		if (CVarConstraintAngularDampingScale.GetValueOnGameThread() > 0.f)
		{
			ProfileInstance.LinearLimit.Damping *= CVarConstraintAngularDampingScale.GetValueOnGameThread() / CVarConstraintLinearDampingScale.GetValueOnGameThread();
		}
	}
#endif
}

//Hacks to easily get zeroed memory for special case when we don't use GC
void FConstraintInstance::Free(FConstraintInstance * Ptr)
{
	Ptr->~FConstraintInstance();
	FMemory::Free(Ptr);
}
FConstraintInstance * FConstraintInstance::Alloc()
{
	void* Memory = FMemory::Malloc(sizeof(FConstraintInstance));
	FMemory::Memzero(Memory, sizeof(FConstraintInstance));
	return new (Memory)FConstraintInstance();
}

void FConstraintInstance::EnableProjection()
{
	ProfileInstance.bEnableProjection = true;
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setProjectionLinearTolerance(ProfileInstance.ProjectionLinearTolerance);
	ConstraintData->setProjectionAngularTolerance(ProfileInstance.ProjectionAngularTolerance);
	ConstraintData->setConstraintFlag(PxConstraintFlag::ePROJECTION, true);
}

void FConstraintInstance::DisableProjection()
{
	ProfileInstance.bEnableProjection = false;
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setConstraintFlag(PxConstraintFlag::ePROJECTION, false);
}

void FConstraintInstance::EnableParentDominates()
{
	ProfileInstance.bParentDominates = true;
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setInvMassScale0(0.0f);
	ConstraintData->setInvMassScale1(1.0f);
	ConstraintData->setInvInertiaScale0(0.0f);
	ConstraintData->setInvInertiaScale1(1.0f);
}

void FConstraintInstance::DisableParentDominates()
{
	ProfileInstance.bParentDominates = false;
	SCOPED_SCENE_WRITE_LOCK(ConstraintData->getScene());
	ConstraintData->setInvMassScale0(1.0f);
	ConstraintData->setInvMassScale1(1.0f);
	ConstraintData->setInvInertiaScale0(1.0f);
	ConstraintData->setInvInertiaScale1(1.0f);
}

#undef LOCTEXT_NAMESPACE
