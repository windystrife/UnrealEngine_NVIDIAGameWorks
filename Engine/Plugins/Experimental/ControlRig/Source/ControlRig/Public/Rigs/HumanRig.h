// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "HierarchicalRig.h"
#include "AlphaBlend.h"
#include "Components/SplineComponent.h"
#include "HumanRig.generated.h"

#define MIN_SPINE_CHAIN 2
USTRUCT(BlueprintInternalUseOnly)
struct FLimbControl
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	// FK data
	FName				FKChainName[3];

	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	// IK data
	FName				IKChainName[3];

	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	FName				IKEffectorName;
	
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	FName				IKJointTargetName;

	// @todo: for no flip ik?
	// FName				IKJointTargetBase;

	// Output
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	FName				ResultChain[3];

	UPROPERTY(EditAnywhere, Category = FLimbControl, meta=(DisplayName="IK Weight"))
	float				IKBlendWeight;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="IK Space"))
	EIKSpaceMode		IKSpaceMode;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="FK Root"))
	FTransform			FKRoot;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="FK Joint"))
	FTransform			FKJoint;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="FK End"))
	FTransform			FKEnd;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="IK Joint"))
	FTransform			IKJoint;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="IK End"))
	FTransform			IKEnd;

	/** Position for IK/FK toggle button in picker */
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	FVector2D			PickerIKTogglePos;

	/** joint orientation axis - used for rotating joint to the right target */
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	TEnumAsByte<EAxisOption::Type>	JointAxis;

	/** joint orientation axis - used for rotating joint to the right target */
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	TEnumAsByte<EAxisOption::Type>	AxisToJointTarget;

	// save offset before switching to FK, and apply back when switching back to IK
	// this is @hack, but can save a lot more time to rig
	UPROPERTY(transient)
	FQuat			LastIKChainToIKEnd;

	// keep the initial length 
	UPROPERTY(transient)
	float				UpperLimbLength;

	UPROPERTY(transient)
	float				LowerLimbLength;

	/** 
	 * We flag first tick because if we switch to FK on the first frame of an animation
	 * we probably wont have valid animated transforms for the IK chain, so dont want to 
	 * copy over IK transforms to FK
	 */
	bool			bFirstTick;

	FLimbControl()
		: IKBlendWeight(1.0f)
		, IKSpaceMode(EIKSpaceMode::IKMode)
		, PickerIKTogglePos(FVector2D::ZeroVector)
		, JointAxis(EAxisOption::X)
		, AxisToJointTarget(EAxisOption::Y_Neg)
		, LastIKChainToIKEnd(FQuat::Identity)
		, bFirstTick(true)
	{}

	void Initialize(float InUpperLimbLen, float InLowerLimbLen);
};

USTRUCT(BlueprintInternalUseOnly)
struct FSpineControl
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	// FK data
	TArray<FName>			FKChains;

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	// IK data
	TArray<FName>			IKChains;

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	// IK data
	TArray<FName>			IKChainsResult;

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	FName					UpperControlIK;

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	FName					LowerControlIK;

	UPROPERTY(EditAnywhere, Category = FSpineControl)
	TArray<FName>			ResultChain;

	UPROPERTY(EditAnywhere, Interp, Category = FSpineControl, meta = (DisplayName = "Bottom Control"))
	FTransform				BottomControl;

	UPROPERTY(EditAnywhere, Interp, Category = FSpineControl, meta = (DisplayName = "Top Control"))
	FTransform				TopControl;

	UPROPERTY(EditAnywhere, Interp, Category = FSpineControl, meta = (DisplayName = "FK Controls"))
	TArray<FTransform>		FKControl;

	UPROPERTY(EditAnywhere, Category = Setup)
	TEnumAsByte<EAxis::Type> BoneAxis;

	UPROPERTY(/*EditAnywhere, Category = Setup*/)
	TEnumAsByte<EAxis::Type> ForwardAxis;

	UPROPERTY(/*EditAnywhere, Category = Setup*/)
	TEnumAsByte<EAxis::Type> UpAxis;

	/** The number of points in the spline if we are specifying it directly */
	UPROPERTY(EditAnywhere, Category = Setup)
	bool bAutoCalculateSpline;

	/** The number of points in the spline if we are not auto-calculating */
	UPROPERTY(EditAnywhere, Category = Setup, meta = (ClampMin = 2, UIMin = 2, EditCondition = "!bAutoCalculateSpline"))
	int32 PointCount;

	UPROPERTY(VisibleAnywhere, Category = FSpineControl)
	TArray<FName> ControlPointNodes; // just constraint them to cluster node

	UPROPERTY(VisibleAnywhere, Category = FSpineControl)
	FName ClusterRootNode;

	UPROPERTY(VisibleAnywhere, Category = FSpineControl)
	FName ClusterEndNode;

	/** Overall roll of the spline, applied on top of other rotations along the direction of the spline */
	UPROPERTY(transient)
	float Roll;

	/** The twist of the start bone. Twist is interpolated along the spline according to Twist Blend. */
	UPROPERTY(transient)
	float TwistStart;

	/** The twist of the end bone. Twist is interpolated along the spline according to Twist Blend. */
	UPROPERTY(transient)
	float TwistEnd;

	/** How to interpolate twist along the length of the spline */
	UPROPERTY(EditAnywhere, Category = Setup)
	struct FAlphaBlend TwistBlend;

	/**
	 * The maximum stretch allowed when fitting bones to the spline. 0.0 means bones do not stretch their length,
	 * 1.0 means bones stretch to the length of the spline
	 */
	 // I think these will be automatically tweaked depending on their control points
	UPROPERTY(EditAnywhere, Category = FSpineControl)
	float Stretch;

	/** The distance along the spline from the start from which bones are constrained */
	UPROPERTY(EditAnywhere, Category = FSpineControl)
	float Offset;

	/** Spline we maintain internally */
	UPROPERTY()
	FSplineCurves BoneSpline;

	/** Cached spline length from when the spline was originally applied to the skeleton */
	UPROPERTY()
	float OriginalSplineLength;

	/** Cached bone lengths. Same size as CachedBoneReferences */
	UPROPERTY(transient)
	TArray<float> CachedBoneLengths;

	/** Cached bone offset rotations. Same size as CachedBoneReferences */
	UPROPERTY(transient)
	TArray<FQuat> CachedOffsetRotations;

	/** Transformed spline */
	FSplineCurves TransformedSpline;

	/** Piecewise linear approximation of the spline, recalculated on creation and deformation */
	TArray<FSplinePositionLinearApproximation> LinearApproximation;

	UPROPERTY(EditAnywhere, Category = FLimbControl, meta=(DisplayName="IK Weight"))
	float				IKBlendWeight;

	UPROPERTY(EditAnywhere, Interp, Category = FLimbControl, meta=(DisplayName="IK Space"))
	EIKSpaceMode		IKSpaceMode;

	/** Position for IK/FK toggle button in picker */
	UPROPERTY(EditDefaultsOnly, Category = FLimbControl)
	FVector2D			PickerIKTogglePos;

	/** 
	 * We flag first tick because if we switch to FK on the first frame of an animation
	 * we probably wont have valid animated transforms for the IK chain, so dont want to 
	 * copy over IK transforms to FK
	 */
	bool			bFirstTick;

	FSpineControl()
		: BoneAxis(EAxis::X)
		, ForwardAxis(EAxis::Y)
		, UpAxis(EAxis::Z)
		, bAutoCalculateSpline(true)
		// allow stretch by default
		, Stretch(1.f)
		, IKBlendWeight(1.0f)
		, IKSpaceMode(EIKSpaceMode::IKMode)
		, PickerIKTogglePos(FVector2D::ZeroVector)
		, bFirstTick(true)
	{}

	bool IsValid() const 
	{
		// add more validation?
		return IKChains.Num() > MIN_SPINE_CHAIN;
	}

	void Initialize()
	{
		bFirstTick = true;
	}
};

USTRUCT()
struct FTwistControl
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FTwistControl)
	FName				BaseNode;

	UPROPERTY(EditDefaultsOnly, Category = FTwistControl)
	FName				TargetNode;

	UPROPERTY(EditDefaultsOnly, Category = FTwistControl)
	FName				TwistNode;

	UPROPERTY(EditDefaultsOnly, Category = FTwistControl)
	bool	bUpperTwist;

	UPROPERTY(EditDefaultsOnly, Category = FTwistControl)
	TEnumAsByte<EAxis::Type> TwistAxis;

	FTwistControl()
		: TwistAxis(EAxis::X)
	{
	}
};

//@todo: generalize with pose driver?
USTRUCT()
struct FTransformKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FTransformKey)
	float				Value;

	UPROPERTY(EditDefaultsOnly, Category = FTransformKey)
	FTransform			Transform;
};

USTRUCT()
struct FTransformKeys
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FTransformKeys)
	TArray<FTransformKey> Keys;
};

USTRUCT()
struct FPoseKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FPoseKey)
	TMap<FName, FTransformKeys>	TransformKeys;

	// this is temp utility function
	bool GetBlendedResult(FName InNodeName, float InKeyValue, FTransform& OutTransform) const;
};

USTRUCT(BlueprintInternalUseOnly)
struct FFingerDescription
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, Category = FFingerDescription)
	FName PoseName;

	UPROPERTY(EditDefaultsOnly, Category = FFingerDescription)
	FString NamePrefix;

	UPROPERTY(EditDefaultsOnly, Category = FFingerDescription)
	FString NameSuffix;

	UPROPERTY(EditAnywhere, interp, Category = FFingerDescription)
	float Weight;

	UPROPERTY(EditDefaultsOnly, Category = FFingerDescription)
	int32 ChainNum;

	FFingerDescription()
		:Weight(0.f)
		, ChainNum(0)
	{}

	FFingerDescription(const FName& InPoseName, const FString& InNamePrefix, const FString& InNameSuffix, float InWeight)
		: PoseName(InPoseName)
		, NamePrefix(InNamePrefix)
		, NameSuffix(InNameSuffix)
		, Weight(InWeight)
		, ChainNum(3)
	{
	}

	CONTROLRIG_API TArray<FName> GetNodeNames() const;
};

UCLASS()
class CONTROLRIG_API UHumanRig : public UHierarchicalRig
{
	GENERATED_BODY()

public:
	UHumanRig();

#if WITH_EDITOR
	// BEGIN: editor set up functions functions
	void SetupLimb(FLimbControl& LimbControl, FName UpperLegNode, FName LowerLegNode, FName AnkleLegNode);
	void SetupSpine(FName RootNode, FName EndNode);
	void AddTwoBoneIK(FName UpperNode, FName MiddleNode, FName EndNode, FName& OutJointTarget, FName& OutEffector);
	void AddCtrlGroupNode(FName& InOutNodeName, FName& OutCtrlNodeName, FName InParentNode, FTransform InTransform, FName LinkNode, FString Suffix = TEXT("_Ctrl"));
	void AddUniqueNode(FName& InOutNodeName, const FName& ParentName, const FTransform& Transform, const FName& LinkNode);
	void EnsureUniqueName(FName& InOutNodeName);
	// END: editor set up functions functions
#endif // WITH_EDITOR
private:
	// ControlRig interface start
	virtual void Evaluate() override;
	virtual void Initialize() override;
	// ControlRig interface end

	// UHierarchicalRig interface start
public:
	virtual bool IsManipulatorEnabled(UControlManipulator* InManipulator) const override;
private:
	virtual UControlManipulator* FindCounterpartManipulator(UControlManipulator* InManipulator) const override;
	virtual FName FindNodeDrivenByNode(FName InNodeName) const override;
	virtual void GetDependentArray(const FName& NodeName, TArray<FName>& OutList) const override;
	// UHierarchicalRig interface end

	// Helper functions
	FTransform Lerp(const FName& ANode, const FName& BNode, float Weight);

	/** Apply a function to each limb */
	void ForEachLimb(const TFunctionRef<void(FLimbControl&)>& InPredicate);

	/** 
	 * Apply a function to each limb
	 * @return true if any function succeeds
	 */
	bool ForEachLimb_EarlyOut(const TFunctionRef<bool(const FLimbControl&)>& InPredicate) const;

	/** 
	 * Apply a function to each limb
	 * @return a valid manipulator as soon as one is found if any function succeeds
	 */
	UControlManipulator* ForEachLimb_Manipulator(const TFunctionRef<UControlManipulator*(const FLimbControl&)>& InPredicate) const;

	/** 
	 * Apply a function to each limb
	 * @return a valid name as soon as one is found if any function succeeds
	 */
	FName ForEachLimb_Name(const TFunctionRef<FName(const FLimbControl&)>& InPredicate) const;

public:
	UPROPERTY(EditDefaultsOnly, Category = Limbs)
	FLimbControl LeftArm;

	UPROPERTY(EditDefaultsOnly, Category = Limbs)
	FLimbControl RightArm;

	UPROPERTY(EditDefaultsOnly, Category = Limbs)
	FLimbControl LeftLeg;

	UPROPERTY(EditDefaultsOnly, Category = Limbs)
	FLimbControl RightLeg;

	UPROPERTY(EditDefaultsOnly, Category = Twist)
	TArray<FTwistControl>	TwistControls;

	UPROPERTY(EditDefaultsOnly, Category = Spine)
	FSpineControl	Spine;

	// for fingers, we just use pose blending, 
	// in maya, simply it would be driven key
	UPROPERTY(EditDefaultsOnly, Category = Fingers)
	TMap<FName, FPoseKey>	KeyedPoses;

	UPROPERTY(EditAnywhere, Category = Fingers)
	TArray<FFingerDescription> FingerDescription;

public:
	/** Switch functions */
	template<typename T>
	void CorrectIKSpace(T& Control)
	{
		// if use weight, don't touch it
		EIKSpaceMode CurrentSpaceMode = Control.IKSpaceMode;
		if (CurrentSpaceMode != EIKSpaceMode::UseWeight)
		{
			float BlendWeight = Control.IKBlendWeight;

			if (CurrentSpaceMode == EIKSpaceMode::FKMode)
			{
				// if 0.f it is already in FK mode
				if (BlendWeight != 0.f)
				{
					// if this is the first tick, assume FK is animated so don't do the copy
					if (!Control.bFirstTick)
					{
						SwitchToFK(Control);
					}
					Control.bFirstTick = false;
					Control.IKBlendWeight = 0.0f;
				}

				// I can't change to useweight this value has to be keyable, you have to change to UseWeight if you want to animate again
			}
			else  if (CurrentSpaceMode == EIKSpaceMode::IKMode)
			{
				// if 1.f, it is already in IK mode
				if (BlendWeight != 1.f)
				{
					SwitchToIK(Control);
					Control.IKBlendWeight = 1.0f;
				}

				// I can't change to useweight this value has to be keyable, you have to change to UseWeight if you want to animate again
			}
		}
	}

	void SwitchToIK(FLimbControl& Control);
	void SwitchToFK(FLimbControl& Control);

	void SwitchToIK(FSpineControl& Control);
	void SwitchToFK(FSpineControl& Control);

	/** Get the space setting that the lib this node is in is using */
	bool GetIKSpaceForNode(FName Node, EIKSpaceMode& OutIKSpace) const;

	void EvaluateSpine();
	void EvaluateLimbs();
	void PostProcess();

	/** Find if there is a manipulator for a particular node (by name) */
	UControlManipulator* FindManipulatorForNode(FName Node) const;

	// transform spline from contrll point
	void TransformSpline();
	// cache spline parameter before it starts
	void CacheSpineParameter();

	// build spline from current node position, will be used when construct first time or when switching between IK/FK
	void BuildSpine(TArray<FTransform>& OutControlPoints);

	/** Get the current twist value at the specified spline alpha */
	float GetSpineTwist(float InAlpha, float TotalSplineAlpha);
	/** Use our linear approximation to determine the earliest intersection with a sphere */
	float FindParamAtFirstSphereIntersection(const FVector& InOrigin, float InRadius, int32& StartingLinearIndex);

private:
	void FaceJointTarget(const FLimbControl& LimbControl, FTransform&  InOutRootTransform, FTransform& InOutJointTransform, const FTransform& InEndTransform, const FVector& JointTargetPos);
};
