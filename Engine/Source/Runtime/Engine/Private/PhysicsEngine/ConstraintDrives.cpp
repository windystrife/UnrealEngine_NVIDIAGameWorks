// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/ConstraintDrives.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

const bool bIsAccelerationDrive = true;

FConstraintDrive::FConstraintDrive()
	: Stiffness(50.f)
	, Damping(1.f)
	, MaxForce(0.f)
	, bEnablePositionDrive(false)
	, bEnableVelocityDrive(false)
{
}

FLinearDriveConstraint::FLinearDriveConstraint()
	: PositionTarget(ForceInit)
	, VelocityTarget(ForceInit)
{
}

FAngularDriveConstraint::FAngularDriveConstraint()
	: OrientationTarget(ForceInit)
	, AngularVelocityTarget(ForceInit)
	, AngularDriveMode(EAngularDriveMode::SLERP)
{
}

#if WITH_PHYSX
void WakeupJointedActors_AssumesLocked(PxD6Joint* Joint)
{
	PxRigidActor* Actor0 = nullptr;
	PxRigidActor* Actor1 = nullptr;
	Joint->getActors(Actor0, Actor1);

	auto WakeupActorHelper = [](PxRigidActor* Actor)
	{
		if (PxRigidDynamic* DynamicActor = (Actor ? Actor->is<PxRigidDynamic>() : nullptr))
		{
			if (DynamicActor->getScene() && !(DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC))
			{
				DynamicActor->wakeUp();
			}

		}
	};

	WakeupActorHelper(Actor0);
	WakeupActorHelper(Actor1);
}

void FLinearDriveConstraint::UpdatePhysXLinearDrive_AssumesLocked(PxD6Joint* Joint) const
{
	XDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eX, true);
	YDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eY, true);
	ZDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eZ, true);
	WakeupJointedActors_AssumesLocked(Joint);
}

void FAngularDriveConstraint::UpdatePhysXAngularDrive_AssumesLocked(PxD6Joint* Joint) const
{
	const bool bUseSlerpDrive = AngularDriveMode == EAngularDriveMode::SLERP;
	SlerpDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eSLERP, bUseSlerpDrive);
	SwingDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eSWING, !bUseSlerpDrive);
	TwistDrive.UpdatePhysXDrive_AssumesLocked(Joint, PxD6Drive::eTWIST, !bUseSlerpDrive);
	WakeupJointedActors_AssumesLocked(Joint);
}

void FConstraintDrive::UpdatePhysXDrive_AssumesLocked(PxD6Joint* Joint, int32 DriveType, bool bDriveEnabled) const
{
	check(DriveType >= PxD6Drive::eX && DriveType <= PxD6Drive::eCOUNT);
	PxD6Drive::Enum PDriveType = (PxD6Drive::Enum) DriveType;

	if(bDriveEnabled)
	{
		const float UseStiffness = bEnablePositionDrive ? Stiffness : 0.f;
		const float UseDamping = bEnableVelocityDrive ? Damping : 0.f;
		const float UseMaxForce = MaxForce > 0.f ? MaxForce : PX_MAX_F32;
		Joint->setDrive(PDriveType, PxD6JointDrive(UseStiffness, UseDamping, UseMaxForce, bIsAccelerationDrive));
	}
	else
	{
		Joint->setDrive(PDriveType, PxD6JointDrive());
	}
}

void FLinearDriveConstraint::SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	XDrive.bEnablePositionDrive = bEnableXDrive;
	YDrive.bEnablePositionDrive = bEnableYDrive;
	ZDrive.bEnablePositionDrive = bEnableZDrive;
}

void FLinearDriveConstraint::SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive)
{
	XDrive.bEnableVelocityDrive = bEnableXDrive;
	YDrive.bEnableVelocityDrive = bEnableYDrive;
	ZDrive.bEnableVelocityDrive = bEnableZDrive;
}

void FAngularDriveConstraint::SetOrientationDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive)
{
	SwingDrive.bEnablePositionDrive = InEnableSwingDrive;
	TwistDrive.bEnablePositionDrive = InEnableTwistDrive;
}

void FAngularDriveConstraint::SetOrientationDriveSLERP(bool InEnableSLERP)
{
	SlerpDrive.bEnablePositionDrive = InEnableSLERP;
}

void FAngularDriveConstraint::SetAngularVelocityDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive)
{
	SwingDrive.bEnableVelocityDrive = InEnableSwingDrive;
	TwistDrive.bEnableVelocityDrive = InEnableTwistDrive;
}

void FAngularDriveConstraint::SetAngularVelocityDriveSLERP(bool InEnableSLERP)
{
	SlerpDrive.bEnableVelocityDrive = InEnableSLERP;
}

void FConstraintDrive::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	Stiffness = InStiffness;
	Damping = InDamping;
	MaxForce = InForceLimit;
}

void FLinearDriveConstraint::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	XDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	YDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	ZDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
}

void FAngularDriveConstraint::SetAngularDriveMode(EAngularDriveMode::Type DriveMode)
{
	AngularDriveMode = DriveMode;
}

void FAngularDriveConstraint::SetDriveParams(float InStiffness, float InDamping, float InForceLimit)
{
	SwingDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	TwistDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
	SlerpDrive.SetDriveParams(InStiffness, InDamping, InForceLimit);
}
#endif
