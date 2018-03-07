// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImmediatePhysicsD6JointData.h"

/****** 
 ************ :(
///CODE COPIED DIRECTLY OUT OF PHYSX (until refactor)
 ************
 ******/

#if WITH_PHYSX

namespace ImmediatePhysics
{

D6JointData::D6JointData(PxD6Joint* Joint)
	: linearLimit(GPhysXSDK->getTolerancesScale(), 0.f)
	, twistLimit(0.f, 0.f)
	, swingLimit(0.f, 0.f)
{
	PxRigidActor* Actor0 = nullptr;
	PxRigidActor* Actor1 = nullptr;
	Joint->getActors(Actor0, Actor1);

	auto GetCOM = [](PxRigidActor* Actor) -> PxTransform
	{
		if (Actor)
		{
			if(Actor->getType() == PxActorType::eRIGID_DYNAMIC || Actor->getType() == PxActorType::eARTICULATION_LINK)
			{
				return static_cast<PxRigidBody*>(Actor)->getCMassLocalPose();
			}
			else
			{
				return static_cast<PxRigidStatic*>(Actor)->getGlobalPose().getInverse();
			}
		}

		return PxTransform(PxIdentity);
	};

	c2b[0] = GetCOM(Actor0).transformInv(Joint->getLocalPose(PxJointActorIndex::eACTOR0));
	c2b[1] = GetCOM(Actor1).transformInv(Joint->getLocalPose(PxJointActorIndex::eACTOR1));

	linearLimit = Joint->getLinearLimit();
	swingLimit = Joint->getSwingLimit();
	twistLimit = Joint->getTwistLimit();

	for(uint32 DriveIdx = 0; DriveIdx < PxD6Drive::eCOUNT; ++DriveIdx)
	{
		drive[DriveIdx] = Joint->getDrive((PxD6Drive::Enum)DriveIdx);	
	}

	Joint->getDriveVelocity(driveLinearVelocity, driveAngularVelocity);
	drivePosition = Joint->getDrivePosition();

	for(uint32 MotionIdx = 0; MotionIdx < PxD6Axis::eCOUNT; ++MotionIdx)
	{
		motion[MotionIdx] = Joint->getMotion((PxD6Axis::Enum)MotionIdx);
	}

	invMassScale.linear0 = Joint->getInvMassScale0();
	invMassScale.linear1 = Joint->getInvMassScale1();
	invMassScale.angular0 = Joint->getInvInertiaScale0();
	invMassScale.angular1 = Joint->getInvInertiaScale1();
}

bool IsActive(const D6JointData& JointData, const PxD6Drive::Enum Index)
{
	const PxD6JointDrive& DriveData = JointData.drive[Index];
	return DriveData.stiffness != 0 || DriveData.damping != 0;
}

void PrepareJointData(D6JointData& JointData)
{
	//angular
	JointData.thSwingY = PxTan(JointData.swingLimit.yAngle / 2);
	JointData.thSwingZ = PxTan(JointData.swingLimit.zAngle / 2);
	JointData.thSwingPad = PxTan(JointData.swingLimit.contactDistance / 2);

	JointData.tqSwingY = PxTan(JointData.swingLimit.yAngle / 4);
	JointData.tqSwingZ = PxTan(JointData.swingLimit.zAngle / 4);
	JointData.tqSwingPad = PxTan(JointData.swingLimit.contactDistance / 4);

	JointData.tqTwistLow = PxTan(JointData.twistLimit.lower / 4);
	JointData.tqTwistHigh = PxTan(JointData.twistLimit.upper / 4);
	JointData.tqTwistPad = PxTan(JointData.twistLimit.contactDistance / 4);

	//linear
	JointData.driving = 0;
	JointData.limited = 0;
	JointData.locked = 0;

	for (PxU32 i = 0; i<PxD6Axis::eCOUNT; i++)
	{
		if (JointData.motion[i] == PxD6Motion::eLIMITED)
			JointData.limited |= 1 << i;
		else if (JointData.motion[i] == PxD6Motion::eLOCKED)
			JointData.locked |= 1 << i;
	}

	// a linear direction isn't driven if it's locked
	if (IsActive(JointData, PxD6Drive::eX) && JointData.motion[PxD6Axis::eX] != PxD6Motion::eLOCKED) JointData.driving |= 1 << PxD6Drive::eX;
	if (IsActive(JointData, PxD6Drive::eY) && JointData.motion[PxD6Axis::eY] != PxD6Motion::eLOCKED) JointData.driving |= 1 << PxD6Drive::eY;
	if (IsActive(JointData, PxD6Drive::eZ) && JointData.motion[PxD6Axis::eZ] != PxD6Motion::eLOCKED) JointData.driving |= 1 << PxD6Drive::eZ;

	// SLERP drive requires all angular dofs unlocked, and inhibits swing/twist

	bool swing1Locked = JointData.motion[PxD6Axis::eSWING1] == PxD6Motion::eLOCKED;
	bool swing2Locked = JointData.motion[PxD6Axis::eSWING2] == PxD6Motion::eLOCKED;
	bool twistLocked = JointData.motion[PxD6Axis::eTWIST] == PxD6Motion::eLOCKED;

	if (IsActive(JointData, PxD6Drive::eSLERP) && !swing1Locked && !swing2Locked && !twistLocked)
		JointData.driving |= 1 << PxD6Drive::eSLERP;
	else
	{
		if (IsActive(JointData, PxD6Drive::eTWIST) && !twistLocked)
			JointData.driving |= 1 << PxD6Drive::eTWIST;
		if (IsActive(JointData, PxD6Drive::eSWING) && (!swing1Locked || !swing2Locked))
			JointData.driving |= 1 << PxD6Drive::eSWING;
	}
}

void computeJacobianAxes(PxVec3 row[3], const PxQuat& qa, const PxQuat& qb)
{
	// Compute jacobian matrix for (qa* qb)  [[* means conjugate in this expr]]
	// d/dt (qa* qb) = 1/2 L(qa*) R(qb) (omega_b - omega_a)
	// result is L(qa*) R(qb), where L(q) and R(q) are left/right q multiply matrix

	PxReal wa = qa.w, wb = qb.w;
	const PxVec3 va(qa.x, qa.y, qa.z), vb(qb.x, qb.y, qb.z);

	const PxVec3 c = vb*wa + va*wb;
	const PxReal d0 = wa*wb;
	const PxReal d1 = va.dot(vb);
	const PxReal d = d0 - d1;

	row[0] = (va * vb.x + vb * va.x + PxVec3(d, c.z, -c.y)) * 0.5f;
	row[1] = (va * vb.y + vb * va.y + PxVec3(-c.z, d, c.x)) * 0.5f;
	row[2] = (va * vb.z + vb * va.z + PxVec3(c.y, -c.x, d)) * 0.5f;

	if ((d0 + d1) != 0.0f)  // check if relative rotation is 180 degrees which can lead to singular matrix
		return;
	else
	{
		row[0].x += PX_EPS_F32;
		row[1].y += PX_EPS_F32;
		row[2].z += PX_EPS_F32;
	}
}


PxReal tanAdd(PxReal tan1, PxReal tan2)
{
	PX_ASSERT(PxAbs(1 - tan1*tan2)>1e-6f);
	return (tan1 + tan2) / (1 - tan1*tan2);
}

PX_CUDA_CALLABLE PxF32 sqr(const PxF32 a)
{
	return a * a;
}

PX_CUDA_CALLABLE PxReal tanHalf(PxReal sin, PxReal cos)
{
	return sin / (1 + cos);
}


class ConstraintHelper
{
	Px1DConstraint* mConstraints;
	Px1DConstraint* mCurrent;
	PxVec3 mRa, mRb;

public:
	ConstraintHelper(Px1DConstraint* c, const PxVec3& ra, const PxVec3& rb)
		: mConstraints(c), mCurrent(c), mRa(ra), mRb(rb) {}


	// hard linear & angular
	void linearHard(const PxVec3& axis, PxReal posErr)
	{
		Px1DConstraint *c = linear(axis, posErr, PxConstraintSolveHint::eEQUALITY);
		c->flags |= Px1DConstraintFlag::eOUTPUT_FORCE;
	}

	void angularHard(const PxVec3& axis, PxReal posErr)
	{
		Px1DConstraint *c = angular(axis, posErr, PxConstraintSolveHint::eEQUALITY);
		c->flags |= Px1DConstraintFlag::eOUTPUT_FORCE;
	}

	// limited linear & angular
	void linearLimit(const PxVec3& axis, PxReal ordinate, PxReal limitValue, const PxJointLimitParameters& limit)
	{
		PxReal pad = limit.isSoft() ? 0 : limit.contactDistance;

		if (ordinate + pad > limitValue)
			addLimit(linear(axis, limitValue - ordinate, PxConstraintSolveHint::eNONE), limit);
	}

	void angularLimit(const PxVec3& axis, PxReal ordinate, PxReal limitValue, PxReal pad, const PxJointLimitParameters& limit)
	{
		if (limit.isSoft())
			pad = 0;

		if (ordinate + pad > limitValue)
			addLimit(angular(axis, limitValue - ordinate, PxConstraintSolveHint::eNONE), limit);
	}


	void angularLimit(const PxVec3& axis, PxReal error, const PxJointLimitParameters& limit)
	{
		addLimit(angular(axis, error, PxConstraintSolveHint::eNONE), limit);
	}

	void halfAnglePair(PxReal halfAngle, PxReal lower, PxReal upper, PxReal pad, const PxVec3& axis, const PxJointLimitParameters& limit)
	{
		PX_ASSERT(lower<upper);
		if (limit.isSoft())
			pad = 0;

		if (halfAngle < lower + pad)
			angularLimit(-axis, -(lower - halfAngle) * 2, limit);
		if (halfAngle > upper - pad)
			angularLimit(axis, (upper - halfAngle) * 2, limit);
	}

	void quarterAnglePair(PxReal quarterAngle, PxReal lower, PxReal upper, PxReal pad, const PxVec3& axis, const PxJointLimitParameters& limit)
	{
		if (limit.isSoft())
			pad = 0;

		PX_ASSERT(lower<upper);
		if (quarterAngle < lower + pad)
			angularLimit(-axis, -(lower - quarterAngle) * 4, limit);
		if (quarterAngle > upper - pad)
			angularLimit(axis, (upper - quarterAngle) * 4, limit);
	}

	// driven linear & angular

	void linear(const PxVec3& axis, PxReal velTarget, PxReal error, const PxD6JointDrive& drive)
	{
		addDrive(linear(axis, error, PxConstraintSolveHint::eNONE), velTarget, drive);
	}

	void angular(const PxVec3& axis, PxReal velTarget, PxReal error, const PxD6JointDrive& drive, PxConstraintSolveHint::Enum hint = PxConstraintSolveHint::eNONE)
	{
		addDrive(angular(axis, error, hint), velTarget, drive);
	}


	PxU32 getCount() { return PxU32(mCurrent - mConstraints); }

	void prepareLockedAxes(const PxQuat& qA, const PxQuat& qB, const PxVec3& cB2cAp, PxU32 lin, PxU32 ang)
	{
		Px1DConstraint* current = mCurrent;
		if (ang)
		{
			PxQuat qB2qA = qA.getConjugate() * qB;

			PxVec3 row[3];
			computeJacobianAxes(row, qA, qB);
			PxVec3 imp = qB2qA.getImaginaryPart();
			if (ang & 1) angular(row[0], -imp.x, PxConstraintSolveHint::eEQUALITY, current++);
			if (ang & 2) angular(row[1], -imp.y, PxConstraintSolveHint::eEQUALITY, current++);
			if (ang & 4) angular(row[2], -imp.z, PxConstraintSolveHint::eEQUALITY, current++);
		}

		if (lin)
		{
			PxMat33 axes(qA);
			if (lin & 1) linear(axes[0], -cB2cAp[0], PxConstraintSolveHint::eEQUALITY, current++);
			if (lin & 2) linear(axes[1], -cB2cAp[1], PxConstraintSolveHint::eEQUALITY, current++);
			if (lin & 4) linear(axes[2], -cB2cAp[2], PxConstraintSolveHint::eEQUALITY, current++);
		}

		for (Px1DConstraint* front = mCurrent; front < current; front++)
			front->flags = Px1DConstraintFlag::eOUTPUT_FORCE;

		mCurrent = current;
	}

	Px1DConstraint *getConstraintRow()
	{
		return mCurrent++;
	}

private:
	Px1DConstraint* linear(const PxVec3& axis, PxReal posErr, PxConstraintSolveHint::Enum hint)
	{
		Px1DConstraint* c = mCurrent++;

		c->solveHint = PxU16(hint);
		c->linear0 = axis;					c->angular0 = mRa.cross(axis);
		c->linear1 = axis;					c->angular1 = mRb.cross(axis);
		PX_ASSERT(c->linear0.isFinite());
		PX_ASSERT(c->linear1.isFinite());
		PX_ASSERT(c->angular0.isFinite());
		PX_ASSERT(c->angular1.isFinite());

		c->geometricError = posErr;
		
		return c;
	}

	Px1DConstraint* angular(const PxVec3& axis, PxReal posErr, PxConstraintSolveHint::Enum hint)
	{
		Px1DConstraint* c = mCurrent++;

		c->solveHint = PxU16(hint);
		c->linear0 = PxVec3(0);		c->angular0 = axis;
		c->linear1 = PxVec3(0);		c->angular1 = axis;

		c->geometricError = posErr;
		return c;
	}

	Px1DConstraint* linear(const PxVec3& axis, PxReal posErr, PxConstraintSolveHint::Enum hint, Px1DConstraint* c)
	{
		c->solveHint = PxU16(hint);
		c->linear0 = axis;					c->angular0 = mRa.cross(axis);
		c->linear1 = axis;					c->angular1 = mRb.cross(axis);
		PX_ASSERT(c->linear0.isFinite());
		PX_ASSERT(c->linear1.isFinite());
		PX_ASSERT(c->angular0.isFinite());
		PX_ASSERT(c->angular1.isFinite());

		c->geometricError = posErr;

		return c;
	}

	Px1DConstraint* angular(const PxVec3& axis, PxReal posErr, PxConstraintSolveHint::Enum hint, Px1DConstraint* c)
	{
		c->solveHint = PxU16(hint);
		c->linear0 = PxVec3(0.f);		c->angular0 = axis;
		c->linear1 = PxVec3(0.f);		c->angular1 = axis;

		c->geometricError = posErr;

		return c;
	}

	void addLimit(Px1DConstraint* c, const PxJointLimitParameters& limit)
	{
		PxU16 flags = PxU16(c->flags | Px1DConstraintFlag::eOUTPUT_FORCE);

		if (limit.isSoft())
		{
			flags |= Px1DConstraintFlag::eSPRING;
			c->mods.spring.stiffness = limit.stiffness;
			c->mods.spring.damping = limit.damping;
		}
		else
		{
			c->solveHint = PxConstraintSolveHint::eINEQUALITY;
			c->mods.bounce.restitution = limit.restitution;
			c->mods.bounce.velocityThreshold = limit.bounceThreshold;
			if (c->geometricError>0)
				flags |= Px1DConstraintFlag::eKEEPBIAS;
			if (limit.restitution>0)
				flags |= Px1DConstraintFlag::eRESTITUTION;
		}

		c->flags = flags;
		c->minImpulse = 0;
	}

	void addDrive(Px1DConstraint* c, PxReal velTarget, const PxD6JointDrive& drive)
	{
		c->velocityTarget = velTarget;

		PxU16 flags = PxU16(c->flags | Px1DConstraintFlag::eSPRING | Px1DConstraintFlag::eHAS_DRIVE_LIMIT);
		if (drive.flags & PxD6JointDriveFlag::eACCELERATION)
			flags |= Px1DConstraintFlag::eACCELERATION_SPRING;
		c->flags = flags;
		c->mods.spring.stiffness = drive.stiffness;
		c->mods.spring.damping = drive.damping;

		c->minImpulse = -drive.forceLimit;
		c->maxImpulse = drive.forceLimit;

		PX_ASSERT(c->linear0.isFinite());
		PX_ASSERT(c->angular0.isFinite());
	}
};

void separateSwingTwist(const PxQuat& q, PxQuat& swing, PxQuat& twist)
{
	twist = q.x != 0.0f ? PxQuat(q.x, 0, 0, q.w).getNormalized() : PxQuat(PxIdentity);
	swing = q * twist.getConjugate();
}

PX_CUDA_CALLABLE PxVec3 ellipseClamp(const PxVec3& point, const PxVec3& radii)
{
	// This function need to be implemented in the header file because
	// it is included in a spu shader program.

	// finds the closest point on the ellipse to a given point

	// (p.y, p.z) is the input point
	// (e.y, e.z) are the radii of the ellipse

	// lagrange multiplier method with Newton/Halley hybrid root-finder.
	// see http://www.geometrictools.com/Documentation/DistancePointToEllipse2.pdf
	// for proof of Newton step robustness and initial estimate.
	// Halley converges much faster but sometimes overshoots - when that happens we take
	// a newton step instead

	// converges in 1-2 iterations where D&C works well, and it's good with 4 iterations
	// with any ellipse that isn't completely crazy

	const PxU32 MAX_ITERATIONS = 20;
	const PxReal convergenceThreshold = 1e-4f;

	// iteration requires first quadrant but we recover generality later

	PxVec3 q(0, PxAbs(point.y), PxAbs(point.z));
	const PxReal tinyEps = 1e-6f; // very close to minor axis is numerically problematic but trivial
	if (radii.y >= radii.z)
	{
		if (q.z < tinyEps)
			return PxVec3(0, point.y > 0 ? radii.y : -radii.y, 0);
	}
	else
	{
		if (q.y < tinyEps)
			return PxVec3(0, 0, point.z > 0 ? radii.z : -radii.z);
	}

	PxVec3 denom, e2 = radii.multiply(radii), eq = radii.multiply(q);

	// we can use any initial guess which is > maximum(-e.y^2,-e.z^2) and for which f(t) is > 0.
	// this guess works well near the axes, but is weak along the diagonals.

	PxReal t = PxMax(eq.y - e2.y, eq.z - e2.z);

	for (PxU32 i = 0; i < MAX_ITERATIONS; i++)
	{
		denom = PxVec3(0, 1 / (t + e2.y), 1 / (t + e2.z));
		PxVec3 denom2 = eq.multiply(denom);

		PxVec3 fv = denom2.multiply(denom2);
		PxReal f = fv.y + fv.z - 1;

		// although in exact arithmetic we are guaranteed f>0, we can get here
		// on the first iteration via catastrophic cancellation if the point is
		// very close to the origin. In that case we just behave as if f=0

		if (f < convergenceThreshold)
			return e2.multiply(point).multiply(denom);

		PxReal df = fv.dot(denom) * -2.0f;
		t = t - f / df;
	}

	// we didn't converge, so clamp what we have
	PxVec3 r = e2.multiply(point).multiply(denom);
	return r * PxRecipSqrt(sqr(r.y / radii.y) + sqr(r.z / radii.z));
}


class ConeLimitHelper
{
public:
	ConeLimitHelper(PxReal tanQSwingY, PxReal tanQSwingZ, PxReal tanQPadding)
		: mTanQYMax(tanQSwingY), mTanQZMax(tanQSwingZ), mTanQPadding(tanQPadding) {}

	// whether the point is inside the (inwardly) padded cone - if it is, there's no limit
	// constraint

	bool contains(const PxVec3& tanQSwing)
	{
		PxReal tanQSwingYPadded = tanAdd(PxAbs(tanQSwing.y), mTanQPadding);
		PxReal tanQSwingZPadded = tanAdd(PxAbs(tanQSwing.z), mTanQPadding);
		return sqr(tanQSwingYPadded / mTanQYMax) + sqr(tanQSwingZPadded / mTanQZMax) <= 1;
	}

	PxVec3 clamp(const PxVec3& tanQSwing,
		PxVec3& normal)
	{
		PxVec3 p = ellipseClamp(tanQSwing, PxVec3(0, mTanQYMax, mTanQZMax));
		normal = PxVec3(0, p.y / sqr(mTanQYMax), p.z / sqr(mTanQZMax));
#ifdef PX_PARANOIA_ELLIPSE_CHECK
		PxReal err = PxAbs(sqr(p.y / mTanQYMax) + sqr(p.z / mTanQZMax) - 1);
		PX_ASSERT(err<1e-3);
#endif

		return p;
	}


	// input is a swing quat, such that swing.x = twist.y = twist.z = 0, q = swing * twist
	// The routine is agnostic to the sign of q.w (i.e. we don't need the minimal-rotation swing)

	// output is an axis such that positive rotation increases the angle outward from the
	// limit (i.e. the image of the x axis), the error is the sine of the angular difference,
	// positive if the twist axis is inside the cone 

	bool getLimit(const PxQuat& swing, PxVec3& axis, PxReal& error)
	{
		PX_ASSERT(swing.w>0);
		PxVec3 twistAxis = swing.getBasisVector0();
		PxVec3 tanQSwing = PxVec3(0, tanHalf(swing.z, swing.w), -tanHalf(swing.y, swing.w));
		if (contains(tanQSwing))
			return false;

		PxVec3 normal, clamped = clamp(tanQSwing, normal);

		// rotation vector and ellipse normal
		PxVec3 r(0, -clamped.z, clamped.y), d(0, -normal.z, normal.y);

		// the point on the cone defined by the tanQ swing vector r
		PxVec3 p(1.f, 0, 0);
		PxReal r2 = r.dot(r), a = 1 - r2, b = 1 / (1 + r2), b2 = b*b;
		PxReal v1 = 2 * a*b2;
		PxVec3 v2(a, 2 * r.z, -2 * r.y);		// a*p + 2*r.cross(p);
		PxVec3 coneLine = v1 * v2 - p;		// already normalized

											// the derivative of coneLine in the direction d	
		PxReal rd = r.dot(d);
		PxReal dv1 = -4 * rd*(3 - r2)*b2*b;
		PxVec3 dv2(-2 * rd, 2 * d.z, -2 * d.y);

		PxVec3 coneNormal = v1 * dv2 + dv1 * v2;

		axis = coneLine.cross(coneNormal) / coneNormal.magnitude();
		error = coneLine.cross(axis).dot(twistAxis);

		PX_ASSERT(PxAbs(axis.magnitude() - 1)<1e-5f);

#ifdef PX_PARANOIA_ELLIPSE_CHECK
		bool inside = sqr(tanQSwing.y / mTanQYMax) + sqr(tanQSwing.z / mTanQZMax) <= 1;
		PX_ASSERT(inside && error>-1e-4f || !inside && error<1e-4f);
#endif

		return true;
	}

private:


	PxReal mTanQYMax, mTanQZMax, mTanQPadding;
};


PxU32 D6JointSolverPrep(Px1DConstraint* constraints,
	PxVec3& body0WorldOffset,
	PxU32 maxConstraints,
	PxConstraintInvMassScale& invMassScale,
	const void* constantBlock,
	const PxTransform& bA2w,
	const PxTransform& bB2w)
{
	PX_UNUSED(maxConstraints);

	PX_UNUSED(maxConstraints);
	const D6JointData& data =
		*reinterpret_cast<const D6JointData*>(constantBlock);

	invMassScale = data.invMassScale;

	const PxU32 SWING1_FLAG = 1 << PxD6Axis::eSWING1,
		SWING2_FLAG = 1 << PxD6Axis::eSWING2,
		TWIST_FLAG = 1 << PxD6Axis::eTWIST;

	const PxU32 ANGULAR_MASK = SWING1_FLAG | SWING2_FLAG | TWIST_FLAG;
	const PxU32 LINEAR_MASK = 1 << PxD6Axis::eX | 1 << PxD6Axis::eY | 1 << PxD6Axis::eZ;

	const PxD6JointDrive* drives = data.drive;
	PxU32 locked = data.locked, limited = data.limited, driving = data.driving;

	PxTransform cA2w = bA2w.transform(data.c2b[0]);
	PxTransform cB2w = bB2w.transform(data.c2b[1]);

	body0WorldOffset = cB2w.p - bA2w.p;
	ConstraintHelper g(constraints, cB2w.p - bA2w.p, cB2w.p - bB2w.p);

	if (cA2w.q.dot(cB2w.q)<0)	// minimum dist quat (equiv to flipping cB2bB.q, which we don't use anywhere)
		cB2w.q = -cB2w.q;

	PxTransform cB2cA = cA2w.transformInv(cB2w);

	PX_ASSERT(data.c2b[0].isValid());
	PX_ASSERT(data.c2b[1].isValid());
	PX_ASSERT(cA2w.isValid());
	PX_ASSERT(cB2w.isValid());
	PX_ASSERT(cB2cA.isValid());

	PxMat33 cA2w_m(cA2w.q), cB2w_m(cB2w.q);

	// handy for swing computation
	PxVec3 bX = cB2w_m[0], aY = cA2w_m[1], aZ = cA2w_m[2];

	if (driving & ((1 << PxD6Drive::eX) | (1 << PxD6Drive::eY) | (1 << PxD6Drive::eZ)))
	{
		// TODO: make drive unilateral if we are outside the limit
		PxVec3 posErr = data.drivePosition.p - cB2cA.p;
		for (PxU32 i = 0; i < 3; i++)
		{
			// -driveVelocity because velTarget is child (body1) - parent (body0) and Jacobian is 1 for body0 and -1 for parent
			if (driving & (1 << (PxD6Drive::eX + i)))
				g.linear(cA2w_m[i], -data.driveLinearVelocity[i], posErr[i], drives[PxD6Drive::eX + i]);
		}
	}

	if (driving & ((1 << PxD6Drive::eSLERP) | (1 << PxD6Drive::eSWING) | (1 << PxD6Drive::eTWIST)))
	{
		PxQuat d2cA_q = cB2cA.q.dot(data.drivePosition.q)>0 ? data.drivePosition.q : -data.drivePosition.q;

		const PxVec3& v = data.driveAngularVelocity;
		PxQuat delta = d2cA_q.getConjugate() * cB2cA.q;

		if (driving & (1 << PxD6Drive::eSLERP))
		{
			PxVec3 velTarget = -cA2w.rotate(data.driveAngularVelocity);

			PxVec3 axis[3] = { PxVec3(1.f,0,0), PxVec3(0,1.f,0), PxVec3(0,0,1.f) };

			if (drives[PxD6Drive::eSLERP].stiffness != 0)
				computeJacobianAxes(axis, cA2w.q * d2cA_q, cB2w.q);	// converges faster if there is only velocity drive

			for (PxU32 i = 0; i < 3; i++)
				g.angular(axis[i], axis[i].dot(velTarget), -delta.getImaginaryPart()[i], drives[PxD6Drive::eSLERP], PxConstraintSolveHint::eSLERP_SPRING);
		}
		else
		{
			if (driving & (1 << PxD6Drive::eTWIST))
				g.angular(bX, v.x, -2.0f * delta.x, drives[PxD6Drive::eTWIST]);

			if (driving & (1 << PxD6Drive::eSWING))
			{
				PxVec3 err = delta.rotate(PxVec3(1.f, 0, 0));

				if (!(locked & SWING1_FLAG))
					g.angular(cB2w_m[1], v.y, err.z, drives[PxD6Drive::eSWING]);

				if (!(locked & SWING2_FLAG))
					g.angular(cB2w_m[2], v.z, -err.y, drives[PxD6Drive::eSWING]);
			}
		}
	}

	if (limited & ANGULAR_MASK)
	{
		PxQuat swing, twist;
		separateSwingTwist(cB2cA.q, swing, twist);

		// swing limits: if just one is limited: if the other is free, we support 
		// (-pi/2, +pi/2) limit, using tan of the half-angle as the error measure parameter. 
		// If the other is locked, we support (-pi, +pi) limits using the tan of the quarter-angle
		// Notation: th == Ps::tanHalf, tq = tanQuarter

		if (limited & SWING1_FLAG && limited & SWING2_FLAG)
		{
			ConeLimitHelper coneHelper(data.tqSwingZ, data.tqSwingY, data.tqSwingPad);

			PxVec3 axis;
			PxReal error;
			if (coneHelper.getLimit(swing, axis, error))
				g.angularLimit(cA2w.rotate(axis), error, data.swingLimit);
		}
		else
		{
			const PxJointLimitCone &limit = data.swingLimit;

			PxReal tqPad = data.tqSwingPad, thPad = data.thSwingPad;

			if (limited & SWING1_FLAG)
			{
				if (locked & SWING2_FLAG)
				{
					g.quarterAnglePair(tanHalf(swing.y, swing.w), -data.tqSwingY, data.tqSwingY, tqPad, aY, limit);
				}
				else
				{
					PxReal dot = -aZ.dot(bX);
					g.halfAnglePair(tanHalf(dot, 1 - dot*dot), -data.thSwingY, data.thSwingY, thPad, aZ.cross(bX), limit);
				}
			}
			if (limited & SWING2_FLAG)
			{
				if (locked & SWING1_FLAG)
				{
					g.quarterAnglePair(tanHalf(swing.z, swing.w), -data.tqSwingZ, data.tqSwingZ, tqPad, aZ, limit);
				}
				else
				{
					PxReal dot = aY.dot(bX);
					g.halfAnglePair(tanHalf(dot, 1 - dot*dot), -data.thSwingZ, data.thSwingZ, thPad, -aY.cross(bX), limit);
				}
			}
		}

		if (limited & TWIST_FLAG)
		{
			g.quarterAnglePair(tanHalf(twist.x, twist.w), data.tqTwistLow, data.tqTwistHigh, data.tqTwistPad,
				cB2w_m[0], data.twistLimit);
		}
	}

	if (limited & LINEAR_MASK)
	{
		PxVec3 limitDir = PxVec3(0);

		for (PxU32 i = 0; i < 3; i++)
		{
			if (limited & (1 << (PxD6Axis::eX + i)))
				limitDir += cA2w_m[i] * cB2cA.p[i];
		}

		PxReal distance = limitDir.magnitude();
		if (distance > data.linearMinDist)
			g.linearLimit(limitDir * (1.0f / distance), distance, data.linearLimit.value, data.linearLimit);
	}

	// we handle specially the case of just one swing dof locked

	PxU32 angularLocked = locked & ANGULAR_MASK;

	if (angularLocked == SWING1_FLAG)
	{
		g.angularHard(bX.cross(aZ), -bX.dot(aZ));
		locked &= ~SWING1_FLAG;
	}
	else if (angularLocked == SWING2_FLAG)
	{
		locked &= ~SWING2_FLAG;
		g.angularHard(bX.cross(aY), -bX.dot(aY));
	}

	g.prepareLockedAxes(cA2w.q, cB2w.q, cB2cA.p, locked & 7, locked >> 3);

	return g.getCount();
}

/*PxMassProperties FLowLevelPhysicsEntity::ComputeMassProperties() const
{
	//return PxRigidBodyExt::computeMassPropertiesFromGeometries(Geometries.GetData(), LocalTMs.GetData(), Geometries.Num());
}*/

}
#endif