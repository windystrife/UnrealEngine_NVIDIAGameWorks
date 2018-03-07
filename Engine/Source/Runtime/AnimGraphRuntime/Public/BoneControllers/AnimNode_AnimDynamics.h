// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "BoneContainer.h"
#include "Animation/AnimPhysicsSolver.h"
#include "BonePose.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_AnimDynamics.generated.h"

class UAnimInstance;
class USkeletalMeshComponent;

DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Dynamics Overall"), STAT_AnimDynamicsOverall, STATGROUP_Physics, ANIMGRAPHRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Dynamics Wind Data Update"), STAT_AnimDynamicsWindData, STATGROUP_Physics, ANIMGRAPHRUNTIME_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Dynamics Bone Evaluation"), STAT_AnimDynamicsBoneEval, STATGROUP_Physics, ANIMGRAPHRUNTIME_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Anim Dynamics Sub-Steps"), STAT_AnimDynamicsSubSteps, STATGROUP_Physics, ANIMGRAPHRUNTIME_API);

/** Supported angular constraint types */
UENUM()
enum class AnimPhysAngularConstraintType : uint8
{
	Angular,
	Cone
};

/** Supported linear axis constraints */
UENUM()
enum class AnimPhysLinearConstraintType : uint8
{
	Free,
	Limited,
};

UENUM(BlueprintType)
enum class AnimPhysSimSpaceType : uint8
{
	Component UMETA(ToolTip = "Sim origin is the location/orientation of the skeletal mesh component."),
	Actor UMETA(ToolTip = "Sim origin is the location/orientation of the actor containing the skeletal mesh component."),
	World UMETA(ToolTip = "Sim origin is the world origin. Teleporting characters is not recommended in this mode."),
	RootRelative UMETA(ToolTip = "Sim origin is the location/orientation of the root bone."),
	BoneRelative UMETA(ToolTip = "Sim origin is the location/orientation of the bone specified in RelativeSpaceBone"),
};

/** Helper mapping a rigid body to a bone reference */
struct FAnimPhysBoneRigidBody
{
	FAnimPhysBoneRigidBody(TArray<FAnimPhysShape>& Shapes, const FVector& Position, const FBoneReference& LinkedBone)
	: PhysBody(Shapes, Position)
	, BoundBone(LinkedBone)
	{}

	FAnimPhysRigidBody PhysBody;
	FBoneReference BoundBone;
};

/** Helper describing a body linked to an optional parent (can be nullptr) */
struct FAnimPhysLinkedBody
{
	FAnimPhysLinkedBody(TArray<FAnimPhysShape>& Shapes, const FVector& Position, const FBoneReference& LinkedBone)
	: RigidBody(Shapes, Position, LinkedBone)
	, ParentBody(nullptr)
	{}

	FAnimPhysBoneRigidBody RigidBody;
	FAnimPhysBoneRigidBody* ParentBody;
};

/** Constraint setup struct, holds data required to build a physics constraint */
USTRUCT()
struct FAnimPhysConstraintSetup
{
	GENERATED_BODY()

	FAnimPhysConstraintSetup()
	: LinearXLimitType(AnimPhysLinearConstraintType::Limited)
	, LinearYLimitType(AnimPhysLinearConstraintType::Limited)
	, LinearZLimitType(AnimPhysLinearConstraintType::Limited)
	, AngularConstraintType(AnimPhysAngularConstraintType::Angular)
	, TwistAxis(AnimPhysTwistAxis::AxisX)
	, ConeAngle(0.0f)
	, AngularXAngle_DEPRECATED(0.0f)
	, AngularYAngle_DEPRECATED(0.0f)
	, AngularZAngle_DEPRECATED(0.0f)
	{}

	/** Whether to limit the linear X axis */
	UPROPERTY(EditAnywhere, Category = Linear)
	AnimPhysLinearConstraintType LinearXLimitType;

	/** Whether to limit the linear Y axis */
	UPROPERTY(EditAnywhere, Category = Linear)
	AnimPhysLinearConstraintType LinearYLimitType;

	/** Whether to limit the linear Z axis */
	UPROPERTY(EditAnywhere, Category = Linear)
	AnimPhysLinearConstraintType LinearZLimitType;

	/** Minimum linear movement per-axis (Set zero here and in the max limit to lock) */
	UPROPERTY(EditAnywhere, Category = Linear, meta = (UIMax = "0", ClampMax = "0"))
	FVector LinearAxesMin;

	/** Maximum linear movement per-axis (Set zero here and in the min limit to lock) */
	UPROPERTY(EditAnywhere, Category = Linear, meta = (UIMin = "0", ClampMin = "0"))
	FVector LinearAxesMax;

	/** Method to use when constraining angular motion */
	UPROPERTY(EditAnywhere, Category = Angular)
	AnimPhysAngularConstraintType AngularConstraintType;

	/** Axis to consider for twist when constraining angular motion (forward axis) */
	UPROPERTY(EditAnywhere, Category = Angular)
	AnimPhysTwistAxis TwistAxis;

	/** Angle to use when constraining using a cone */
	UPROPERTY(EditAnywhere, Category = Angular, meta = (UIMin = "0", UIMax = "90", ClampMin = "0", ClampMax = "90"))
	float ConeAngle;

	/** X-axis limit for angular motion when using the "Angular" constraint type (Set to 0 to lock, or 180 to remain free) */
	UPROPERTY()
	float AngularXAngle_DEPRECATED;

	/** Y-axis limit for angular motion when using the "Angular" constraint type (Set to 0 to lock, or 180 to remain free) */
	UPROPERTY()
	float AngularYAngle_DEPRECATED;

	/** Z-axis limit for angular motion when using the "Angular" constraint type (Set to 0 to lock, or 180 to remain free) */
	UPROPERTY()
	float AngularZAngle_DEPRECATED;

	UPROPERTY(EditAnywhere, Category = Angular, meta = (UIMin = "-180", UIMax = "180", ClampMin = "-180", ClampMax = "180"))
	FVector AngularLimitsMin;

	UPROPERTY(EditAnywhere, Category = Angular, meta = (UIMin = "-180", UIMax = "180", ClampMin = "-180", ClampMax = "180"))
	FVector AngularLimitsMax;

	/** Axis on body1 to match to the angular target direction. */
	UPROPERTY(EditAnywhere, Category = Angular)
	AnimPhysTwistAxis AngularTargetAxis;

	/** Target direction to face for body1 (in body0 local space) */
	UPROPERTY(EditAnywhere, Category = Angular)
	FVector AngularTarget;

	/** The values below are calculated on initialisation and used when building the limits */

	/** If all axes are locked we can use 3 linear limits instead of the 6 needed for limited axes */
	UPROPERTY()
	bool bLinearFullyLocked;
};

USTRUCT()
struct FAnimPhysPlanarLimit
{
	GENERATED_BODY();

	/** When using a driving bone, the plane transform will be relative to the bone transform */
	UPROPERTY(EditAnywhere, Category=PlanarLimit)
	FBoneReference DrivingBone;

	/** Transform of the plane, this is either in component-space if no DrivinBone is specified
	 *  or in bone-space if a driving bone is present.
	 */
	UPROPERTY(EditAnywhere, Category=PlanarLimit)
	FTransform PlaneTransform;
};

/** Whether spheres keep bodies inside, or outside of their shape */
UENUM()
enum class ESphericalLimitType : uint8
{
	Inner,
	Outer
};

USTRUCT()
struct FAnimPhysSphericalLimit
{
	GENERATED_BODY();

	FAnimPhysSphericalLimit()
		: SphereLocalOffset(FVector::ZeroVector)
		, LimitRadius(0.0f)
		, LimitType(ESphericalLimitType::Outer)
	{}

	/** Bone to attach the sphere to */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	FBoneReference DrivingBone;

	/** Local offset for the sphere, if no driving bone is set this is in node space, otherwise bone space */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	FVector SphereLocalOffset;

	/** Radius of the sphere */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	float LimitRadius;

	/** Whether to lock bodies inside or outside of the sphere */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	ESphericalLimitType LimitType;
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_AnimDynamics : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY();

	FAnimNode_AnimDynamics();
	
	/** The space used to run the simulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	AnimPhysSimSpaceType SimulationSpace;

	/** When in BoneRelative sim space, the simulation will use this bone as the origin */
	UPROPERTY(EditAnywhere, Category = Setup)
	FBoneReference RelativeSpaceBone;

	/** Set to true to use the solver to simulate a connected chain */
	UPROPERTY(EditAnywhere, Category = Setup)
	bool bChain;

	/** The bone to attach the physics body to, if bChain is true this is the top of the chain */
	UPROPERTY(EditAnywhere, Category = Setup)
	FBoneReference BoundBone;
	
	/** If bChain is true this is the bottom of the chain, otherwise ignored */
	UPROPERTY(EditAnywhere, Category = Setup, meta=(EditCondition = bChain))
	FBoneReference ChainEnd;

	/** Extents of the box to use for simulation */
	UPROPERTY(EditAnywhere, Category = Setup)
	FVector BoxExtents;

	/** Vector relative to the body being simulated to attach the constraint to */
	UPROPERTY(EditAnywhere, Category = Setup)
	FVector LocalJointOffset;

	/** Scale for gravity, higher values increase forces due to gravity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float GravityScale;

	/** If true the body will attempt to spring back to its initial position */
	UPROPERTY(EditAnywhere, Category = Setup)
	bool bLinearSpring;

	/** If true the body will attempt to align itself with the specified angular target */
	UPROPERTY(EditAnywhere, Category = Setup)
	bool bAngularSpring;

	/** Spring constant to use when calculating linear springs, higher values mean a stronger spring.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float LinearSpringConstant;

	/** Spring constant to use when calculating angular springs, higher values mean a stronger spring */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float AngularSpringConstant;

	/** Whether or not wind is enabled for the bodies in this simulation */
	UPROPERTY(EditAnywhere, Category = Wind)
	bool bEnableWind;

	UPROPERTY(Transient)
	bool bWindWasEnabled;

	/** Scale to apply to calculated wind velocities in the solver */
	UPROPERTY(EditAnywhere, Category = Wind)
	float WindScale;

	/** If true, the override value will be used for linear damping */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	bool bOverrideLinearDamping;

	/** Overridden linear damping value */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float LinearDampingOverride;

	/** If true, the override value will be used for angular damping */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	bool bOverrideAngularDamping;

	/** Overridden angular damping value */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float AngularDampingOverride;

	/** If true, the override value will be used for the angular bias for bodies in this node. 
	 *  Angular bias is essentially a twist reduction for chain forces and defaults to a value to keep chains stability
	 *  in check. When using single-body systems sometimes angular forces will look like they are "catching-up" with
	 *  the mesh, if that's the case override this and push it towards 1.0f until it settles correctly
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	bool bOverrideAngularBias;

	/** Overridden angular bias value
	 *  Angular bias is essentially a twist reduction for chain forces and defaults to a value to keep chains stability
	 *  in check. When using single-body systems sometimes angular forces will look like they are "catching-up" with
	 *  the mesh, if that's the case override this and push it towards 1.0f until it settles correctly
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Setup, meta = (PinHiddenByDefault))
	float AngularBiasOverride;

	/** If true we will perform physics update, otherwise skip - allows visualisation of the initial state of the bodies */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	bool bDoUpdate;

	/** If true we will perform bone transform evaluation, otherwise skip - allows visualisation of the initial anim state compared to the physics sim */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	bool bDoEval;

	/** Number of update passes on the linear and angular limits before we solve the position of the bodies recommended to be four times the value of NumSolverIterationsPostUpdate */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	int32 NumSolverIterationsPreUpdate;

	/** Number of update passes on the linear and angular limits after we solve the position of the bodies, recommended to be around a quarter of NumSolverIterationsPreUpdate */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Setup)
	int32 NumSolverIterationsPostUpdate;

	/** Data describing the constraints we will apply to the body */
	UPROPERTY(EditAnywhere, Category = Constraint)
	FAnimPhysConstraintSetup ConstraintSetup;

	/** Whether to evaluate planar limits */
	UPROPERTY(EditAnywhere, Category=PlanarLimit)
	bool bUsePlanarLimit;

	/** List of available planar limits for this node */
	UPROPERTY(EditAnywhere, Category=PlanarLimit)
	TArray<FAnimPhysPlanarLimit> PlanarLimits;

	/** Whether to evaluate spherical limits */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	bool bUseSphericalLimits;

	/** List of available spherical limits for this node */
	UPROPERTY(EditAnywhere, Category = SphericalLimit)
	TArray<FAnimPhysSphericalLimit> SphericalLimits;

	/** Resolution method for planar limits */
	UPROPERTY(EditAnywhere, Category = Collision)
	AnimPhysCollisionType CollisionType;

	/** Radius to use if CollisionType is set to CustomSphere */
	UPROPERTY(EditAnywhere, Category = Collision, meta = (UIMin = "1", ClampMin = "1"))
	float SphereCollisionRadius;

	/** An external force to apply to all bodies in the simulation when ticked, specified in world space */
	UPROPERTY(EditAnywhere, Category = Forces, meta = (PinShownByDefault))
	FVector ExternalForce;

	// FAnimNode_SkeletalControlBase interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual bool NeedsDynamicReset() const { return true; }
	virtual void ResetDynamics() { RequestInitialise(); }
	// End of FAnimNode_SkeletalControlBase interface

	void RequestInitialise() { bRequiresInit = true; }
	void InitPhysics(FComponentSpacePoseContext& Output);
	void TermPhysics();

	void UpdateLimits(FComponentSpacePoseContext& Output);

	int32 GetNumBodies() const;
	const FAnimPhysRigidBody& GetPhysBody(int32 BodyIndex) const;

#if WITH_EDITOR

	// Accessors for editor code (mainly for visualization functions)
	FVector GetBodyLocalJointOffset(int32 BodyIndex) const;
	// Gets the number of currently bound bones (always 1 unless using a chain)
	int32 GetNumBoundBones() const;
	// Gets the specified bound bone reference
	const FBoneReference* GetBoundBoneReference(int32 Index) const;

#endif

protected:

	// FAnimNode_SkeletalControlBase protected interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones);
	// End of FAnimNode_SkeletalControlBase protected interface

private:

	// Given a bone index, get it's transform in the currently selected simulation space
	FTransform GetBoneTransformInSimSpace(FComponentSpacePoseContext& Output, const FCompactPoseBoneIndex& BoneIndex);

	// Given a transform in simulation space, convert it back to component space
	FTransform GetComponentSpaceTransformFromSimSpace(AnimPhysSimSpaceType SimSpace, FComponentSpacePoseContext& Output, const FTransform& InSimTransform);
	// Given a transform in component space, convert it to the current sim space
	FTransform GetSimSpaceTransformFromComponentSpace(AnimPhysSimSpaceType SimSpace, FComponentSpacePoseContext& Output, const FTransform& InComponentTransform);

	// Given a world-space vector, convert it into the current simulation space
	FVector TransformWorldVectorToSimSpace(FComponentSpacePoseContext& Output, const FVector& InVec);

	void ConvertSimulationSpace(FComponentSpacePoseContext& Output, AnimPhysSimSpaceType From, AnimPhysSimSpaceType To);

	// We can't get clean bone positions unless we are in the evaluate step.
	// Requesting an init or reinit sets this flag for us to pick up during evaluate
	bool bRequiresInit;

	// Maximum time to consider when accumulating time debt to avoid spiraling
	static const float MaxTimeDebt;

	// Cached timestep from the update phase (needed in evaluate phase)
	float NextTimeStep;

	// Current amount of time debt
	float TimeDebt;

	// Current time dilation
	float CurrentTimeDilation;

	// Cached physics settings. We cache these on initialise to avoid the cost of accessing UPhysicsSettings a lot each frame
	float MaxPhysicsDeltaTime;
	float MaxSubstepDeltaTime;
	int32 MaxSubsteps;
	//////////////////////////////////////////////////////////////////////////

	// Cached sim space that we last used
	AnimPhysSimSpaceType LastSimSpace;

	// Active body list
	TArray<FAnimPhysLinkedBody> Bodies;

	// Pointers to bodies that need to be reset to their bound bone.
	// This happens on LOD change so we don't make the simulation unstable
	TArray<FAnimPhysLinkedBody*> BodiesToReset;

	// Pointers back to the base bodies to pass to the simulation
	TArray<FAnimPhysRigidBody*> BaseBodyPtrs;

	// List of current linear limits built for the current frame
	TArray<FAnimPhysLinearLimit> LinearLimits;

	// List of current angular limits built for the current frame
	TArray<FAnimPhysAngularLimit> AngularLimits;

	// List of spring force generators created for this frame
	TArray<FAnimPhysSpring> Springs;

	// Local space offsets for each body
	TArray<FVector> JointOffsets;

	// List of bone references for all bodies in this node
	TArray<FBoneReference> BoundBoneReferences;

	// Depending on the LOD we might not be runnning all of the bound bodies (for chains)
	// this tracks the active bodies.
	TArray<int32> ActiveBoneIndices;

	// Gravity direction in sim space
	FVector SimSpaceGravityDirection;
};
