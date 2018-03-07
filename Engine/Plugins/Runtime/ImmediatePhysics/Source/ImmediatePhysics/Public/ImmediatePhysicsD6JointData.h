// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

//TODO: this is just straight copied from physx
namespace ImmediatePhysics
{

#if WITH_PHYSX
struct D6JointData
{
	D6JointData(PxD6Joint* Joint);

	/** End solver API */


	PxConstraintInvMassScale	invMassScale;
	PxTransform					c2b[2];

	PxU32					locked;		// bitmap of locked DOFs
	PxU32					limited;	// bitmap of limited DOFs
	PxU32					driving;	// bitmap of active drives (implies driven DOFs not locked)

	
	PxD6Motion::Enum		motion[6];
	PxJointLinearLimit		linearLimit;
	PxJointAngularLimitPair	twistLimit;
	PxJointLimitCone		swingLimit;

	PxD6JointDrive			drive[PxD6Drive::eCOUNT];

	PxTransform				drivePosition;
	PxVec3					driveLinearVelocity;
	PxVec3					driveAngularVelocity;

	// derived quantities

	
										// tan-half and tan-quarter angles

	PxReal					thSwingY;
	PxReal					thSwingZ;
	PxReal					thSwingPad;

	PxReal					tqSwingY;
	PxReal					tqSwingZ;
	PxReal					tqSwingPad;

	PxReal					tqTwistLow;
	PxReal					tqTwistHigh;
	PxReal					tqTwistPad;

	PxReal					linearMinDist;	// linear limit minimum distance to get a good direction

											// projection quantities
	//PxReal					projectionLinearTolerance;
	//PxReal					projectionAngularTolerance;

	FTransform					ActorToBody[2];

	bool HasConstraints() const
	{
		return locked || limited || driving;
	}
};

void PrepareJointData(D6JointData& JointData);
PxU32 D6JointSolverPrep(Px1DConstraint* constraints, PxVec3& body0WorldOffset, PxU32 maxConstraints, PxConstraintInvMassScale& invMassScale, const void* constantBlock, const PxTransform& bA2w, const PxTransform& bB2w);

}
#endif