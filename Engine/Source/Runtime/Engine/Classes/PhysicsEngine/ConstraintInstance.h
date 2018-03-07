// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "EngineDefines.h"
#include "PhysxUserData.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"
#include "ConstraintInstance.generated.h"

class FMaterialRenderProxy;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class UMaterialInterface;
struct FBodyInstance;

#if WITH_PHYSX
namespace physx
{
	class PxD6Joint;
	class PxRigidActor;
	class PxScene;
}
#endif // WITH_PHYSX

class FMaterialRenderProxy;
class FPrimitiveDrawInterface;
class FMaterialRenderProxy;

/** Container for properties of a physics constraint that can be easily swapped at runtime. This is useful for switching different setups when going from ragdoll to standup for example */
USTRUCT()
struct ENGINE_API FConstraintProfileProperties
{
	GENERATED_USTRUCT_BODY()

	/** Linear tolerance value in world units. If the distance error exceeds this tolerence limit, the body will be projected. */
	UPROPERTY(EditAnywhere, Category = Projection, meta = (editcondition = "bEnableProjection", ClampMin = "0.0"))
	float ProjectionLinearTolerance;

	/** Angular tolerance value in world units. If the distance error exceeds this tolerence limit, the body will be projected. */
	UPROPERTY(EditAnywhere, Category = Projection, meta = (editcondition = "bEnableProjection", ClampMin = "0.0"))
	float ProjectionAngularTolerance;

	/** Force needed to break the distance constraint. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Linear, meta = (editcondition = "bLinearBreakable", ClampMin = "0.0"))
	float LinearBreakThreshold;

	/** Torque needed to break the joint. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Angular, meta = (editcondition = "bAngularBreakable", ClampMin = "0.0"))
	float AngularBreakThreshold;

	UPROPERTY(EditAnywhere, Category = Linear)
	FLinearConstraint LinearLimit;

	UPROPERTY(EditAnywhere, Category = Angular)
	FConeConstraint ConeLimit;

	UPROPERTY(EditAnywhere, Category = Angular)
	FTwistConstraint TwistLimit;

	UPROPERTY(EditAnywhere, Category = Linear)
	FLinearDriveConstraint LinearDrive;

	UPROPERTY(EditAnywhere, Category = Angular)
	FAngularDriveConstraint AngularDrive;

	// Disable collision between bodies joined by this constraint.
	UPROPERTY(EditAnywhere, Category = Constraint)
	uint8 bDisableCollision : 1;

	// When set, the parent body in a constraint will not be affected by the motion of the child
	UPROPERTY(EditAnywhere, Category = Constraint)
	uint8 bParentDominates : 1;

	/**
	* If distance error between bodies exceeds 0.1 units, or rotation error exceeds 10 degrees, body will be projected to fix this.
	* For example a chain spinning too fast will have its elements appear detached due to velocity, this will project all bodies so they still appear attached to each other.
	*/
	UPROPERTY(EditAnywhere, Category = Projection)
	uint8 bEnableProjection : 1;

	/** Whether it is possible to break the joint with angular force. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Angular)
	uint8 bAngularBreakable : 1;

	/** Whether it is possible to break the joint with linear force. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Linear)
	uint8 bLinearBreakable : 1;

	FConstraintProfileProperties();

#if WITH_PHYSX

	/** Updates physx joint properties from unreal properties (limits, drives, flags, etc...) */
	void UpdatePhysX_AssumesLocked(physx::PxD6Joint* Joint, float AverageMass, float UseScale) const;

	/** Updates physx drive target */
	void UpdatePhysXDriveTarget_AssumesLocked(physx::PxD6Joint* Joint) const;

	/** Updates physx joint breakable properties (threshold, etc...)*/
	void UpdatePhysXBreakable_AssumesLocked(physx::PxD6Joint* Joint) const;

	/** Updates physx joint flag based on profile properties */
	void UpdatePhysXConstraintFlags_AssumesLocked(physx::PxD6Joint* Joint) const;
#endif

#if WITH_EDITOR
	void SyncChangedConstraintProperties(struct FPropertyChangedChainEvent& PropertyChangedEvent);
#endif
};


/** Container for a physics representation of an object. */
USTRUCT()
struct ENGINE_API FConstraintInstance
{
	GENERATED_USTRUCT_BODY()

	/** Indicates position of this constraint within the array in SkeletalMeshComponent. */
	int32 ConstraintIndex;

#if WITH_PHYSX
	/** Internal use. Physics-engine representation of this constraint. */
	physx::PxD6Joint*		ConstraintData;
#endif	//WITH_PHYSX

	/** Physics scene index. */
	int32 SceneIndex;

	/** Name of bone that this joint is associated with. */
	UPROPERTY(VisibleAnywhere, Category=Constraint)
	FName JointName;

	///////////////////////////// CONSTRAINT GEOMETRY
	
	/** 
	 *	Name of first bone (body) that this constraint is connecting. 
	 *	This will be the 'child' bone in a PhysicsAsset.
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FName ConstraintBone1;

	/** 
	 *	Name of second bone (body) that this constraint is connecting. 
	 *	This will be the 'parent' bone in a PhysicsAset.
	 */
	UPROPERTY(EditAnywhere, Category=Constraint)
	FName ConstraintBone2;

	///////////////////////////// Body1 ref frame
	
	/** Location of constraint in Body1 reference frame. */
	UPROPERTY()
	FVector Pos1;

	/** Primary (twist) axis in Body1 reference frame. */
	UPROPERTY()
	FVector PriAxis1;

	/** Seconday axis in Body1 reference frame. Orthogonal to PriAxis1. */
	UPROPERTY()
	FVector SecAxis1;

	///////////////////////////// Body2 ref frame
	
	/** Location of constraint in Body2 reference frame. */
	UPROPERTY()
	FVector Pos2;

	/** Primary (twist) axis in Body2 reference frame. */
	UPROPERTY()
	FVector PriAxis2;

	/** Seconday axis in Body2 reference frame. Orthogonal to PriAxis2. */
	UPROPERTY()
	FVector SecAxis2;

	/** Specifies the angular offset between the two frames of reference. By default limit goes from (-Angle, +Angle)
	* This allows you to bias the limit for swing1 swing2 and twist. */
	UPROPERTY(EditAnywhere, Category = Angular)
	FRotator AngularRotationOffset;

	/** If true, linear limits scale using the absolute min of the 3d scale of the owning component */
	UPROPERTY(EditAnywhere, Category = Linear)
	uint32 bScaleLinearLimits : 1;

	float AverageMass;

	//Constraint Data (properties easily swapped at runtime based on different constraint profiles)
	UPROPERTY(EditAnywhere, Category = Constraint)
	FConstraintProfileProperties ProfileInstance;

public:
	/** Copies behavior properties from the given profile. Automatically updates the physx representation if it's been created */
	void CopyProfilePropertiesFrom(const FConstraintProfileProperties& FromProperties);

#if WITH_PHYSX
	FPhysxUserData PhysxUserData;
#endif

private:
	/** The component scale passed in during initialization*/
	float LastKnownScale;

public:

	/** Constructor **/
	FConstraintInstance();

	/** Gets the linear limit size */
	float GetLinearLimit() const
	{
		return ProfileInstance.LinearLimit.Limit;
	}

	/** Gets the motion type for the linear X-axis limit. */
	ELinearConstraintMotion GetLinearXMotion() const
	{
		return ProfileInstance.LinearLimit.XMotion;
	}

	/** Sets the Linear XMotion type */
	void SetLinearXMotion(ELinearConstraintMotion ConstraintType)
	{
		ProfileInstance.LinearLimit.XMotion = ConstraintType;
		UpdateLinearLimit();
	}

	/** Sets the LinearX Motion Type and the limit distance (Note distance is the same for all 3 axes) */
	void SetLinearXLimit(ELinearConstraintMotion ConstraintType, float InLinearLimitSize)
	{
		ProfileInstance.LinearLimit.XMotion = ConstraintType;
		ProfileInstance.LinearLimit.Limit = InLinearLimitSize;
		UpdateLinearLimit();
	}

	/** Gets the motion type for the linear Y-axis limit. */
	ELinearConstraintMotion GetLinearYMotion() const
	{
		return ProfileInstance.LinearLimit.YMotion;
	}

	/** Sets the Linear YMotion type */
	void SetLinearYMotion(ELinearConstraintMotion ConstraintType)
	{
		ProfileInstance.LinearLimit.YMotion = ConstraintType;
		UpdateLinearLimit();
	}

	/** Sets the LinearY Motion Type and the limit distance (Note distance is the same for all 3 axes) */
	void SetLinearYLimit(ELinearConstraintMotion ConstraintType, float InLinearLimitSize)
	{
		ProfileInstance.LinearLimit.YMotion = ConstraintType;
		ProfileInstance.LinearLimit.Limit = InLinearLimitSize;
		UpdateLinearLimit();
	}

	/** Gets the motion type for the linear Z-axis limit. */
	ELinearConstraintMotion GetLinearZMotion() const
	{
		return ProfileInstance.LinearLimit.ZMotion;
	}

	/** Sets the Linear ZMotion type */
	void SetLinearZMotion(ELinearConstraintMotion ConstraintType)
	{
		ProfileInstance.LinearLimit.ZMotion = ConstraintType;
		UpdateLinearLimit();
	}

	/** Sets the LinearZ Motion Type and the limit distance (Note distance is the same for all 3 axes) */
	void SetLinearZLimit(ELinearConstraintMotion ConstraintType, float InLinearLimitSize)
	{
		ProfileInstance.LinearLimit.ZMotion = ConstraintType;
		ProfileInstance.LinearLimit.Limit = InLinearLimitSize;
		UpdateLinearLimit();
	}

	/** Gets the motion type for the swing1 of the cone constraint */
	EAngularConstraintMotion GetAngularSwing1Motion() const
	{
		return ProfileInstance.ConeLimit.Swing1Motion;
	}

	/** Sets the cone limit's swing1 motion type */
	void SetAngularSwing1Motion(EAngularConstraintMotion MotionType)
	{
		ProfileInstance.ConeLimit.Swing1Motion = MotionType;
		UpdateAngularLimit();
	}

	// The current swing1 of the constraint
	float GetCurrentSwing1() const;

	/** Gets the cone limit swing1 angle in degrees */
	float GetAngularSwing1Limit() const
	{
		return ProfileInstance.ConeLimit.Swing1LimitDegrees;
	}

	/** Sets the Angular Swing1 Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularSwing1Limit(EAngularConstraintMotion MotionType, float InSwing1LimitAngle)
	{
		ProfileInstance.ConeLimit.Swing1Motion = MotionType;
		ProfileInstance.ConeLimit.Swing1LimitDegrees = InSwing1LimitAngle;
		UpdateAngularLimit();
	}
	
	/** Gets the motion type for the swing2 of the cone constraint */
	EAngularConstraintMotion GetAngularSwing2Motion() const
	{
		return ProfileInstance.ConeLimit.Swing2Motion;
	}

	/** Sets the cone limit's swing2 motion type */
	void SetAngularSwing2Motion(EAngularConstraintMotion MotionType)
	{
		ProfileInstance.ConeLimit.Swing2Motion = MotionType;
		UpdateAngularLimit();
	}

	// The current swing2 of the constraint
	float GetCurrentSwing2() const;

	/** Gets the cone limit swing2 angle in degrees */
	float GetAngularSwing2Limit() const
	{
		return ProfileInstance.ConeLimit.Swing2LimitDegrees;
	}

	/** Sets the Angular Swing2 Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularSwing2Limit(EAngularConstraintMotion MotionType, float InSwing2LimitAngle)
	{
		ProfileInstance.ConeLimit.Swing2Motion = MotionType;
		ProfileInstance.ConeLimit.Swing2LimitDegrees = InSwing2LimitAngle;
		UpdateAngularLimit();
	}

	/** Gets the motion type for the twist of the cone constraint */
	EAngularConstraintMotion GetAngularTwistMotion() const
	{
		return ProfileInstance.TwistLimit.TwistMotion;
	}

	/** Sets the twist limit's motion type */
	void SetAngularTwistMotion(EAngularConstraintMotion MotionType)
	{
		ProfileInstance.TwistLimit.TwistMotion = MotionType;
		UpdateAngularLimit();
	}

	// The current twist of the constraint
	float GetCurrentTwist() const;

	/** Gets the twist limit angle in degrees */
	float GetAngularTwistLimit() const
	{
		return ProfileInstance.TwistLimit.TwistLimitDegrees;
	}

	/** Sets the Angular Twist Motion Type
	*	@param MotionType	New Motion Type
	*/
	void SetAngularTwistLimit(EAngularConstraintMotion MotionType, float InTwistLimitAngle)
	{
		ProfileInstance.TwistLimit.TwistMotion = MotionType;
		ProfileInstance.TwistLimit.TwistLimitDegrees = InTwistLimitAngle;
		UpdateAngularLimit();
	}

	/** Sets the Linear Breakable properties
	*	@param bInLinearBreakable		Whether it is possible to break the joint with linear force
	*	@param InLinearBreakThreshold	Force needed to break the joint
	*/
	void SetLinearBreakable(bool bInLinearBreakable, float InLinearBreakThreshold)
	{
		ProfileInstance.bLinearBreakable = bInLinearBreakable;
		ProfileInstance.LinearBreakThreshold = InLinearBreakThreshold;
		UpdateBreakable();
	}

	/** Sets the Angular Breakable properties
	*	@param bInAngularBreakable		Whether it is possible to break the joint with angular force
	*	@param InAngularBreakThreshold	Torque needed to break the joint
	*/
	void SetAngularBreakable(bool bInAngularBreakable, float InAngularBreakThreshold)
	{
		ProfileInstance.bAngularBreakable = bInAngularBreakable;
		ProfileInstance.AngularBreakThreshold = InAngularBreakThreshold;
		UpdateBreakable();
	}

	// @todo document
	void CopyConstraintGeometryFrom(const FConstraintInstance* FromInstance);

	// @todo document
	void CopyConstraintParamsFrom(const FConstraintInstance* FromInstance);

	// Retrieve the constraint force most recently applied to maintain this constraint. Returns 0 forces if the constraint is not initialized or broken.
	void GetConstraintForce(FVector& OutLinearForce, FVector& OutAngularForce);

	// Retrieve the status of constraint being broken.
	bool IsBroken();

	/** Set which linear position drives are enabled */
	void SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);

	/** Whether the linear position drive is enabled */
	bool IsLinearPositionDriveEnabled() const
	{
		return ProfileInstance.LinearDrive.IsPositionDriveEnabled();
	}

	/** Set the linear drive's target position position */
	void SetLinearPositionTarget(const FVector& InPosTarget);

	/** Set which linear velocity drives are enabled */
	void SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);

	/** Whether the linear velocity drive is enabled */
	bool IsLinearVelocityDriveEnabled() const
	{
		return ProfileInstance.LinearDrive.IsVelocityDriveEnabled();
	}

	/** Set the linear drive's target velocity*/
	void SetLinearVelocityTarget(const FVector& InVelTarget);

	/** Set the linear drive's strength parameters */
	void SetLinearDriveParams(float InPositionStrength, float InVelocityStrength, float InForceLimit);

	DEPRECATED(4.15, "Please call SetOrientationDriveTwistAndSwing. Note the order of bools is reversed (Make sure to pass Twist and then Swing)")
	void SetAngularPositionDrive(bool bInEnableSwingDrive, bool bInEnableTwistDrive)
	{
		SetOrientationDriveTwistAndSwing(bInEnableTwistDrive, bInEnableSwingDrive);
	}

	/** Set which twist and swing orientation drives are enabled. Only applicable when Twist And Swing drive mode is used */
	void SetOrientationDriveTwistAndSwing(bool bInEnableTwistDrive, bool bInEnableSwingDrive);

	/** Set whether the SLERP angular position drive is enabled. Only applicable when SLERP drive mode is used */
	void SetOrientationDriveSLERP(bool bInEnableSLERP);

	/** Whether the angular orientation drive is enabled */
	bool IsAngularOrientationDriveEnabled() const
	{
		return ProfileInstance.AngularDrive.IsOrientationDriveEnabled();
	}
	
	/** Set the angular drive's orientation target*/
	void SetAngularOrientationTarget(const FQuat& InPosTarget);

	DEPRECATED(4.15, "Please call SetAngularVelocityDriveTwistAndSwing. Note the order of bools is reversed (Make sure to pass Twist and then Swing)")
	void SetAngularVelocityDrive(bool bInEnableSwingDrive, bool bInEnableTwistDrive)
	{
		SetAngularVelocityDriveTwistAndSwing(bInEnableTwistDrive, bInEnableSwingDrive);
	}

	/** Set which twist and swing angular velocity drives are enabled. Only applicable when Twist And Swing drive mode is used */
	void SetAngularVelocityDriveTwistAndSwing(bool bInEnableTwistDrive, bool bInEnableSwingDrive);

	/** Set whether the SLERP angular velocity drive is enabled. Only applicable when SLERP drive mode is used */
	void SetAngularVelocityDriveSLERP(bool bInEnableSLERP);

	/** Whether the angular velocity drive is enabled */
	bool IsAngularVelocityDriveEnabled() const
	{
		return ProfileInstance.AngularDrive.IsVelocityDriveEnabled();
	}
	
	/** Set the angular drive's angular velocity target*/
	void SetAngularVelocityTarget(const FVector& InVelTarget);

	/** Set the angular drive's strength parameters*/
	void SetAngularDriveParams(float InSpring, float InDamping, float InForceLimit);

	/** Set the angular drive mode */
	void SetAngularDriveMode(EAngularDriveMode::Type DriveMode);

	/** Refreshes the physics engine joint's linear limits. Only applicable if the joint has been created already.*/
	void UpdateLinearLimit();

	/** Refreshes the physics engine joint's angular limits. Only applicable if the joint has been created already.*/
	void UpdateAngularLimit();

	/** Scale Angular Limit Constraints (as defined in RB_ConstraintSetup). This only affects the physics engine and does not update the unreal side so you can do things like a LERP of the scale values. */
	void SetAngularDOFLimitScale(float InSwing1LimitScale, float InSwing2LimitScale, float InTwistLimitScale);

	/** Allows you to dynamically change the size of the linear limit 'sphere'. */
	void SetLinearLimitSize(float NewLimitSize);

	/** Create physics engine constraint. */
	void InitConstraint(FBodyInstance* Body1, FBodyInstance* Body2, float Scale, UObject* DebugOwner, FOnConstraintBroken InConstraintBrokenDelegate = FOnConstraintBroken());

#if WITH_PHYSX
	/** Create physics engine constraint using physx actors. */
	void InitConstraintPhysX_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene, float InScale, FOnConstraintBroken InConstraintBrokenDelegate = FOnConstraintBroken());
#endif

	/** Terminate physics engine constraint */
	void TermConstraint();

	/** Whether the physics engine constraint has been terminated */
	bool IsTerminated() const;

	/** See if this constraint is valid. */
	bool IsValidConstraintInstance() const;

	// Get component ref frame
	FTransform GetRefFrame(EConstraintFrame::Type Frame) const;

	// Pass in reference frame in. If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint. 
	void SetRefFrame(EConstraintFrame::Type Frame, const FTransform& RefFrame);

	/** Get the position of this constraint in world space. */
	FVector GetConstraintLocation();

	// Pass in reference position in (maintains reference orientation). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	void SetRefPosition(EConstraintFrame::Type Frame, const FVector& RefPosition);

	// Pass in reference orientation in (maintains reference position). If the constraint is currently active, this will set its active local pose. Otherwise the change will take affect in InitConstraint.
	void SetRefOrientation(EConstraintFrame::Type Frame, const FVector& PriAxis, const FVector& SecAxis);

	/** Whether collision is currently disabled */
	bool IsCollisionDisabled() const
	{
		return ProfileInstance.bDisableCollision;
	}

	/** Set whether jointed actors can collide with each other */
	void SetDisableCollision(bool InDisableCollision);

	// @todo document
	void DrawConstraint(int32 ViewIndex, class FMeshElementCollector& Collector,
		float Scale, float LimitDrawScale, bool bDrawLimits, bool bDrawSelected,
		const FTransform& Con1Frame, const FTransform& Con2Frame, bool bDrawAsPoint) const
	{
		DrawConstraintImp(FPDIOrCollector(ViewIndex, Collector), Scale, LimitDrawScale, bDrawLimits, bDrawSelected, Con1Frame, Con2Frame, bDrawAsPoint);
	}

	void DrawConstraint(FPrimitiveDrawInterface* PDI,
		float Scale, float LimitDrawScale, bool bDrawLimits, bool bDrawSelected,
		const FTransform& Con1Frame, const FTransform& Con2Frame, bool bDrawAsPoint) const
	{
		DrawConstraintImp(FPDIOrCollector(PDI), Scale, LimitDrawScale, bDrawLimits, bDrawSelected, Con1Frame, Con2Frame, bDrawAsPoint);
	}

	void GetUsedMaterials(TArray<UMaterialInterface*>& Materials);

	bool Serialize(FArchive& Ar);
	void PostSerialize(const FArchive& Ar);

	/** Turn on linear and angular projection */
	void EnableProjection();

	/** Turn off linear and angular projection */
	void DisableProjection();

	/** Enable/Disable parent dominates (meaning the parent body cannot be be affected at all by a child) */
	void EnableParentDominates();
	void DisableParentDominates();

	float GetLastKnownScale() const { return LastKnownScale; }

	//Hacks to easily get zeroed memory for special case when we don't use GC
	static void Free(FConstraintInstance * Ptr);
	static FConstraintInstance * Alloc();

private:
#if WITH_PHYSX 
	bool CreatePxJoint_AssumesLocked(physx::PxRigidActor* PActor1, physx::PxRigidActor* PActor2, physx::PxScene* PScene);
	void UpdateConstraintFlags_AssumesLocked();
	void UpdateAverageMass_AssumesLocked(const physx::PxRigidActor* PActor1, const physx::PxRigidActor* PActor2);
	physx::PxD6Joint* GetUnbrokenJoint_AssumesLocked() const;

	bool ExecuteOnUnbrokenJointReadOnly(TFunctionRef<void(const physx::PxD6Joint*)> Func) const;
	bool ExecuteOnUnbrokenJointReadWrite(TFunctionRef<void(physx::PxD6Joint*)> Func) const;
#endif

	struct FPDIOrCollector
	{
		FPrimitiveDrawInterface* PDI;
		FMeshElementCollector* Collector;
		int32 ViewIndex;

		FPDIOrCollector(FPrimitiveDrawInterface* InPDI)
			: PDI(InPDI)
			, Collector(nullptr)
			, ViewIndex(INDEX_NONE)
		{
		}

		FPDIOrCollector(int32 InViewIndex, FMeshElementCollector& InCollector)
			: PDI(nullptr)
			, Collector(&InCollector)
			, ViewIndex(InViewIndex)
		{
		}

		bool HasCollector() const
		{
			return Collector != nullptr;
		}

		FPrimitiveDrawInterface* GetPDI() const;

		void DrawCylinder(const FVector& Start, const FVector& End, float Thickness, FMaterialRenderProxy* MaterialProxy, ESceneDepthPriorityGroup DepthPriority) const;
	};

	void DrawConstraintImp(const FPDIOrCollector& PDIOrCollector,
		float Scale, float LimitDrawScale, bool bDrawLimits, bool bDrawSelected,
		const FTransform& Con1Frame, const FTransform& Con2Frame, bool bDrawAsPoint) const;

	void UpdateBreakable();
	void UpdateDriveTarget();

	FOnConstraintBroken OnConstraintBrokenDelegate;

	friend struct FConstraintBrokenDelegateData;

public:

	///////////////////////////// DEPRECATED
	// Most of these properties have moved inside the ProfileInstance member (FConstraintProfileProperties struct)
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 bDisableCollision_DEPRECATED : 1;
	UPROPERTY()
	uint32 bEnableProjection_DEPRECATED : 1;
	UPROPERTY()
	float ProjectionLinearTolerance_DEPRECATED;
	UPROPERTY()
	float ProjectionAngularTolerance_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearXMotion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearYMotion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum ELinearConstraintMotion> LinearZMotion_DEPRECATED;
	UPROPERTY()
	float LinearLimitSize_DEPRECATED;
	UPROPERTY()
	uint32 bLinearLimitSoft_DEPRECATED : 1;
	UPROPERTY()
	float LinearLimitStiffness_DEPRECATED;
	UPROPERTY()
	float LinearLimitDamping_DEPRECATED;
	UPROPERTY()
	uint32 bLinearBreakable_DEPRECATED : 1;
	UPROPERTY()
	float LinearBreakThreshold_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing1Motion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularTwistMotion_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<enum EAngularConstraintMotion> AngularSwing2Motion_DEPRECATED;
	UPROPERTY()
	uint32 bSwingLimitSoft_DEPRECATED : 1;
	UPROPERTY()
	uint32 bTwistLimitSoft_DEPRECATED : 1;
	UPROPERTY()
	float Swing1LimitAngle_DEPRECATED;
	UPROPERTY()
	float TwistLimitAngle_DEPRECATED;
	UPROPERTY()
	float Swing2LimitAngle_DEPRECATED;
	UPROPERTY()
	float SwingLimitStiffness_DEPRECATED;
	UPROPERTY()
	float SwingLimitDamping_DEPRECATED;
	UPROPERTY()
	float TwistLimitStiffness_DEPRECATED;
	UPROPERTY()
	float TwistLimitDamping_DEPRECATED;
	UPROPERTY()
	uint32 bAngularBreakable_DEPRECATED : 1;
	UPROPERTY()
	float AngularBreakThreshold_DEPRECATED;
private:
	UPROPERTY()
	uint32 bLinearXPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearXVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearYPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearYVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearZPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearZVelocityDrive_DEPRECATED : 1;
public:
	UPROPERTY()
	uint32 bLinearPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bLinearVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	FVector LinearPositionTarget_DEPRECATED;
	UPROPERTY()
	FVector LinearVelocityTarget_DEPRECATED;
	UPROPERTY()
	float LinearDriveSpring_DEPRECATED;
	UPROPERTY()
	float LinearDriveDamping_DEPRECATED;
	UPROPERTY()
	float LinearDriveForceLimit_DEPRECATED;
	UPROPERTY()
	uint32 bSwingPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bSwingVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bTwistPositionDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bTwistVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bAngularSlerpDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bAngularOrientationDrive_DEPRECATED : 1;
private:
	UPROPERTY()
	uint32 bEnableSwingDrive_DEPRECATED : 1;
	UPROPERTY()
	uint32 bEnableTwistDrive_DEPRECATED : 1;
public:
	UPROPERTY()
	uint32 bAngularVelocityDrive_DEPRECATED : 1;
	UPROPERTY()
	FQuat AngularPositionTarget_DEPRECATED;
	UPROPERTY()
	TEnumAsByte<EAngularDriveMode::Type> AngularDriveMode_DEPRECATED;
	UPROPERTY()
	FRotator AngularOrientationTarget_DEPRECATED;
	UPROPERTY()
	FVector AngularVelocityTarget_DEPRECATED;    // Revolutions per second
	UPROPERTY()
	float AngularDriveSpring_DEPRECATED;
	UPROPERTY()
	float AngularDriveDamping_DEPRECATED;
	UPROPERTY()
	float AngularDriveForceLimit_DEPRECATED;
#endif //EDITOR_ONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FConstraintInstance> : public TStructOpsTypeTraitsBase2<FConstraintInstance>
{
	enum 
	{
		WithSerializer = true,
		WithPostSerialize = true
	};
};