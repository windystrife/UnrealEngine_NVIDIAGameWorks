// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HumanRig.h"
#include "TwoBoneIK.h"
#include "SplineIK.h"
#include "AnimationRuntime.h"
#include "AnimationCoreLibrary.h"

#define LOCTEXT_NAMESPACE "HumanRig"

/////////////////////////////////////////////////////
// UHumanRig

UHumanRig::UHumanRig()
{
	// add default finger descriptions
	FingerDescription.Add(FFingerDescription(TEXT("index_l"), TEXT("index"), TEXT("_l"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("middle_l"), TEXT("middle"), TEXT("_l"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("pinky_l"), TEXT("pinky"), TEXT("_l"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("ring_l"), TEXT("ring"), TEXT("_l"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("thumb_l"), TEXT("thumb"), TEXT("_l"), 0.f));

	FingerDescription.Add(FFingerDescription(TEXT("index_r"), TEXT("index"), TEXT("_r"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("middle_r"), TEXT("middle"), TEXT("_r"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("pinky_r"), TEXT("pinky"), TEXT("_r"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("ring_r"), TEXT("ring"), TEXT("_r"), 0.f));
	FingerDescription.Add(FFingerDescription(TEXT("thumb_r"), TEXT("thumb"), TEXT("_r"), 0.f));
}

void UHumanRig::Evaluate()
{
	Super::Evaluate();

	// evaluate spine first -
	EvaluateSpine();

	// evaluate limbs
	EvaluateLimbs();

	// do post process
	PostProcess();
}

void UHumanRig::EvaluateLimbs()
{
	// update control node position
	auto EvaluateLimb = [this](FLimbControl& LimbControl)
	{
		// make sure we're in the correct space first
		CorrectIKSpace(LimbControl);

		// IK solver
		FTransform RootTransform = GetMappedGlobalTransform(LimbControl.IKChainName[0]);
		FTransform JointTransform = GetMappedGlobalTransform(LimbControl.IKChainName[1]);
		FTransform EndTransform = GetMappedGlobalTransform(LimbControl.IKChainName[2]);
		FTransform InitEndTransform = EndTransform;

		FVector JointTargetPos, DesiredPos;
		JointTargetPos = GetMappedGlobalTransform(LimbControl.IKJointTargetName).GetLocation();
		DesiredPos = GetMappedGlobalTransform(LimbControl.IKEffectorName).GetLocation();

		AnimationCore::SolveTwoBoneIK(RootTransform, JointTransform, EndTransform, JointTargetPos, DesiredPos, LimbControl.UpperLimbLength, LimbControl.LowerLimbLength, false, 1.f, 1.f);

		FaceJointTarget(LimbControl, RootTransform, JointTransform, EndTransform, JointTargetPos);

		SetMappedGlobalTransform(LimbControl.IKChainName[0], RootTransform);
		SetMappedGlobalTransform(LimbControl.IKChainName[1], JointTransform);
		SetMappedGlobalTransform(LimbControl.IKChainName[2], EndTransform);

		// now blend between by weight
		float BlendWeight = LimbControl.IKBlendWeight;
		SetGlobalTransform(LimbControl.ResultChain[0], Lerp(LimbControl.FKChainName[0], LimbControl.IKChainName[0], BlendWeight));
		SetGlobalTransform(LimbControl.ResultChain[1], Lerp(LimbControl.FKChainName[1], LimbControl.IKChainName[1], BlendWeight));
		SetGlobalTransform(LimbControl.ResultChain[2], Lerp(LimbControl.FKChainName[2], LimbControl.IKChainName[2], BlendWeight));
	};

	ForEachLimb(EvaluateLimb);
}

FVector GetAlignVector(const FTransform& Transform, EAxisOption::Type AxisOption)
{
	switch (AxisOption)
	{
	case EAxisOption::X:
		return Transform.GetUnitAxis(EAxis::X);
	case EAxisOption::X_Neg:
		return -Transform.GetUnitAxis(EAxis::X);
	case EAxisOption::Y:
		return Transform.GetUnitAxis(EAxis::Y);
	case EAxisOption::Y_Neg:
		return -Transform.GetUnitAxis(EAxis::Y);
	case EAxisOption::Z:
		return Transform.GetUnitAxis(EAxis::Z);
	case EAxisOption::Z_Neg:
		return -Transform.GetUnitAxis(EAxis::Z);
	default:
		break;
	}

	return FVector(1.f, 0.f, 0.f);
}

void UHumanRig::PostProcess()
{
	// do blending of poses
#if 0  // comment out finger things - doesn't do anything as it's same as refpose, but as for safely comment it out. 
	for (int32 FingerIndex = 0; FingerIndex < FingerDescription.Num(); ++FingerIndex)
	{
		// now blend based on weight
		const FFingerDescription& Finger = FingerDescription[FingerIndex];
		const FPoseKey* CurrentKey = KeyedPoses.Find(Finger.PoseName);
		if (CurrentKey)
		{
			// @todo: do cache even for demo?
			TArray<FName> NodeNames = Finger.GetNodeNames();
			// now get the weighted result
			for (int32 NodeIndex = 0; NodeIndex < NodeNames.Num(); ++NodeIndex)
			{
				FTransform FingerTransform; 
				if (CurrentKey->GetBlendedResult(NodeNames[NodeIndex], Finger.Weight, FingerTransform))
				{
					SetLocalTransform(NodeNames[NodeIndex], FingerTransform);
				}
			}
		}
	}
#endif // 

	// do twist update. Twist has to do after all transform update, and 
	// on the mesh space, not in rig space
	for (int32 TwistIndex = 0; TwistIndex < TwistControls.Num(); ++TwistIndex)
	{
		const FTwistControl& TwistCtrl = TwistControls[TwistIndex];

		FTransform BaseTransform = GetMappedGlobalTransform(TwistCtrl.BaseNode);
		FTransform TargetTransform = GetMappedGlobalTransform(TwistCtrl.TargetNode);
		FTransform BaseToTarget = TargetTransform.GetRelativeTransform(BaseTransform);

		FVector TwistVector = FMatrix::Identity.GetUnitAxis(TwistCtrl.TwistAxis);

		FTransform TwistLocalTransform = GetMappedLocalTransform(TwistCtrl.TwistNode);

		FQuat BaseToTargetRot = BaseToTarget.GetRotation();
		FQuat TwistLocalRot = TwistLocalTransform.GetRotation();

		FQuat TargetSwingQuat, TargetTwistQuat;
		BaseToTargetRot.ToSwingTwist(TwistVector, TargetSwingQuat, TargetTwistQuat);

		FQuat TwistSwingQuat, TwistTwistQuat;
		TwistLocalRot.ToSwingTwist(TwistVector, TwistSwingQuat, TwistTwistQuat);

		if (TwistCtrl.bUpperTwist)
		{
			// inverse of target
			TwistTwistQuat = FQuat::FastLerp(FQuat::Identity, TargetTwistQuat.Inverse(), 0.8f);
		}
		else
		{
			TwistTwistQuat = FQuat::FastLerp(FQuat::Identity, TargetTwistQuat, 0.5f);
		}

		TwistLocalTransform.SetRotation(TwistSwingQuat * TwistTwistQuat);
		TwistLocalTransform.NormalizeRotation();
		SetMappedLocalTransform(TwistCtrl.TwistNode, TwistLocalTransform);
	}
}

// make sure the legs are facing the target correctly
void UHumanRig::FaceJointTarget(const FLimbControl& LimbControl, FTransform&  InOutRootTransform, FTransform& InOutJointTransform, const FTransform& InEndTransform, const FVector& JointTargetPos)
{

	auto FaceTarget = [this](const FTransform& ParentTransform, FTransform& InOutChildTransform, const FVector& NewDir, const FVector& LookTarget, EAxisOption::Type AlignAxis, EAxisOption::Type LookAtAxis)
	{
		// do this in local space
		FVector BoneDir = ParentTransform.InverseTransformVector(NewDir);
		FVector AlignDir = GetAlignVector(FTransform::Identity, AlignAxis);
		FQuat BoneRotation = FQuat::FindBetweenNormals(AlignDir, BoneDir);

		// get twist from aim
		// because just facing isn't going to give you twist value, we'll get twist from facing to joint target pos
		FVector LookDir = GetAlignVector(FTransform::Identity, LookAtAxis);
		FTransform LocalTransform = InOutChildTransform.GetRelativeTransform(ParentTransform);
		// I think this has to be parent? @Todo:
		FVector LocalLookTarget = ParentTransform.InverseTransformPosition(LookTarget);
		// ignore up vector
		FQuat DeltaRotation = AnimationCore::SolveAim(LocalTransform, LocalLookTarget, LookDir, false, FVector::ForwardVector, 0.f);

		FQuat Twist, Swing;
		DeltaRotation.ToSwingTwist(AlignDir, Swing, Twist);

		// compose rotation again using twist and swing
		FQuat WorldRotation = ParentTransform.GetRotation() * BoneRotation * Twist;
		InOutChildTransform.SetRotation(WorldRotation);
	};

		// get parent transform
	FName ParentName = GetHierarchy().GetParentName(LimbControl.IKChainName[0]);
	const FTransform RootParentTransform = GetMappedGlobalTransform(ParentName);

	// get local transform of root
	FVector NewDir = (InOutJointTransform.GetLocation() - InOutRootTransform.GetLocation()).GetSafeNormal();
	FaceTarget(RootParentTransform, InOutRootTransform, NewDir, JointTargetPos, LimbControl.JointAxis, LimbControl.AxisToJointTarget);

	NewDir = (InEndTransform.GetLocation() - InOutJointTransform.GetLocation()).GetSafeNormal();
	FaceTarget(InOutRootTransform, InOutJointTransform, NewDir, JointTargetPos, LimbControl.JointAxis, LimbControl.AxisToJointTarget);
}

void UHumanRig::Initialize()
{
	// These calls will add all limb manipulators. We don't do this each time for now to allow for extensibility.
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftLegFKRoot", "Left Leg FK Root"), TEXT("thigh_l_FK_Ctrl"), TEXT("LeftLeg.FKRoot"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftLegFKJoint", "Left Leg FK Joint"), TEXT("calf_l_FK_Ctrl"), TEXT("LeftLeg.FKJoint"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftLegFKEnd", "Left Leg FK End"), TEXT("foot_l_FK_Ctrl"), TEXT("LeftLeg.FKEnd"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("LeftLegIKJoint", "Left Leg IK Joint"), TEXT("foot_l_IK_JointTarget"), TEXT("LeftLeg.IKJoint"), EIKSpaceMode::IKMode, true, false, false, false);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("LeftLegIKEnd", "Left Leg IK End"), TEXT("foot_l_IK_Effector"), TEXT("LeftLeg.IKEnd"), EIKSpaceMode::IKMode, true, false, false, false);
// 
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightLegFKRoot", "Right Leg FK Root"), TEXT("thigh_r_FK_Ctrl"), TEXT("RightLeg.FKRoot"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightLegFKJoint", "Right Leg FK Joint"), TEXT("calf_r_FK_Ctrl"), TEXT("RightLeg.FKJoint"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightLegFKEnd", "Right Leg FK End"), TEXT("foot_r_FK_Ctrl"), TEXT("RightLeg.FKEnd"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("RightLegIKJoint", "Right Leg IK Joint"), TEXT("foot_r_IK_JointTarget"), TEXT("RightLeg.IKJoint"), EIKSpaceMode::IKMode, true, false, false, false);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("RightLegIKEnd", "Right Leg IK End"), TEXT("foot_r_IK_Effector"), TEXT("RightLeg.IKEnd"), EIKSpaceMode::IKMode, true, false, false, false);
// 
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftArmFKRoot", "Left Arm FK Root"), TEXT("upperarm_l_FK_Ctrl"), TEXT("LeftArm.FKRoot"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftArmFKJoint", "Left Arm FK Joint"), TEXT("lowerarm_l_FK_Ctrl"), TEXT("LeftArm.FKJoint"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("LeftArmFKEnd", "Left Arm FK End"), TEXT("hand_l_FK_Ctrl"), TEXT("LeftArm.FKEnd"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("LeftArmIKJoint", "Left Arm IK Joint"), TEXT("hand_l_IK_JointTarget"), TEXT("LeftArm.IKJoint"), EIKSpaceMode::IKMode, true, false, false, false);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("LeftArmIKEnd", "Left Arm IK End"), TEXT("hand_l_IK_Effector"), TEXT("LeftArm.IKEnd"), EIKSpaceMode::IKMode, true, false, false, false);
// 
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightArmFKRoot", "Right Arm FK Root"), TEXT("upperarm_r_FK_Ctrl"), TEXT("RightArm.FKRoot"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightArmFKJoint", "Right Arm FK Joint"), TEXT("lowerarm_r_FK_Ctrl"), TEXT("RightArm.FKJoint"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(UBoxManipulator::StaticClass(), LOCTEXT("RightArmFKEnd", "Right Arm FK End"), TEXT("hand_r_FK_Ctrl"), TEXT("RightArm.FKEnd"), EIKSpaceMode::FKMode, false, true, false, true);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("RightArmIKJoint", "Right Arm IK Joint"), TEXT("hand_r_IK_JointTarget"), TEXT("RightArm.IKJoint"), EIKSpaceMode::IKMode, true, false, false, false);
// 	AddManipulator(USphereManipulator::StaticClass(), LOCTEXT("RightArmIKEnd", "Right Arm IK End"), TEXT("hand_r_IK_Effector"), TEXT("RightArm.IKEnd"), EIKSpaceMode::IKMode, true, false, false, false);

	Super::Initialize();

	auto InitializeLimb = [this](FLimbControl& LimbControl)
	{
		// update control node position
		FTransform Upper = GetMappedGlobalTransform(LimbControl.IKChainName[0]);
		FTransform Middle = GetMappedGlobalTransform(LimbControl.IKChainName[1]);
		FTransform Lower = GetMappedGlobalTransform(LimbControl.IKChainName[2]);
		float UpperLimbLength = (Upper.GetLocation() - Middle.GetLocation()).Size();
		float LowerLimbLength = (Middle.GetLocation() - Lower.GetLocation()).Size();
		LimbControl.Initialize(UpperLimbLength, LowerLimbLength);
	};

	ForEachLimb(InitializeLimb);

	Spine.Initialize();

	if (Spine.IsValid())
	{
		CacheSpineParameter();
	}
}

#if WITH_EDITOR
void UHumanRig::SetupLimb(FLimbControl& LimbControl, FName UpperLimbNode, FName LowerLimbNode, FName AnkleLimbNode)
{
	FTransform UpperLimbNodeTransform = GetGlobalTransform(UpperLimbNode);
	FTransform LowerLimbNodeTransform = GetGlobalTransform(LowerLimbNode);
	FTransform AnkleLimbNodeTransform = GetGlobalTransform(AnkleLimbNode);

	LimbControl.ResultChain[0] = UpperLimbNode;
	LimbControl.ResultChain[1] = LowerLimbNode;
	LimbControl.ResultChain[2] = AnkleLimbNode;

	FName NewNodeName = FName(*FString(UpperLimbNode.ToString() + TEXT("_FK")));
	AddCtrlGroupNode(NewNodeName, LimbControl.FKChainName[0], NAME_None, UpperLimbNodeTransform, UpperLimbNode);

	NewNodeName = FName(*FString(LowerLimbNode.ToString() + TEXT("_FK")));
	AddCtrlGroupNode(NewNodeName, LimbControl.FKChainName[1], LimbControl.FKChainName[0], LowerLimbNodeTransform, LowerLimbNode);

	NewNodeName = FName(*FString(AnkleLimbNode.ToString() + TEXT("_FK")));
	AddCtrlGroupNode(NewNodeName, LimbControl.FKChainName[2], LimbControl.FKChainName[1], AnkleLimbNodeTransform, AnkleLimbNode);

	// Add IK nodes
	NewNodeName = FName(*FString(UpperLimbNode.ToString() + TEXT("_IK")));
	AddUniqueNode(NewNodeName, NAME_None, UpperLimbNodeTransform, UpperLimbNode);
	LimbControl.IKChainName[0] = NewNodeName;

	FName ParentNodeName = NewNodeName;
	NewNodeName = FName(*FString(LowerLimbNode.ToString() + TEXT("_IK")));
	AddUniqueNode(NewNodeName, ParentNodeName, LowerLimbNodeTransform, LowerLimbNode);
	LimbControl.IKChainName[1] = NewNodeName;

	ParentNodeName = NewNodeName;
	NewNodeName = FName(*FString(AnkleLimbNode.ToString() + TEXT("_IK")));
	AddUniqueNode(NewNodeName, ParentNodeName, AnkleLimbNodeTransform, AnkleLimbNode);
	LimbControl.IKChainName[2] = NewNodeName;

	// add IK chains
	AddTwoBoneIK(UpperLimbNode, LowerLimbNode, AnkleLimbNode, LimbControl.IKJointTargetName, LimbControl.IKEffectorName);
}

void UHumanRig::SetupSpine(FName RootNode, FName EndNode)
{
	// fill up data for SpineControl
	// make sure have enough chain to create spine
	TArray<FName> SpineChain;
	FName CurrentNode = EndNode;

	const FAnimationHierarchy& MyHierarchy = GetHierarchy();
	while (MyHierarchy.Contains(CurrentNode))
	{
		SpineChain.Insert(CurrentNode, 0);

		// if it hits top node, break
		if (CurrentNode == RootNode)
		{
			break;
		}

		// look for parent
		CurrentNode = MyHierarchy.GetParentName(CurrentNode);
	}

	// failed, need more chain
	if (SpineChain.Num() < MIN_SPINE_CHAIN)
	{
		return;
	}

	// add FK chains
	// add IK chains
	// 
	int32 NumChain = SpineChain.Num();
	Spine.FKChains.Reset(NumChain);
	Spine.IKChains.Reset(NumChain);
	Spine.IKChainsResult.Reset(NumChain);
	Spine.FKChains.AddDefaulted(NumChain);
	Spine.IKChains.AddDefaulted(NumChain);
	Spine.IKChainsResult.AddDefaulted(NumChain);
	Spine.ResultChain = SpineChain;

	TArray<FTransform> ChainTransformArray;
	ChainTransformArray.Reset(NumChain);
	ChainTransformArray.AddDefaulted(NumChain);

	// add nodes
	for (int32 ChainIndex = 0; ChainIndex < SpineChain.Num(); ++ChainIndex)
	{
		// add FK node
		FName ChainName = SpineChain[ChainIndex];
		FTransform ChainTransform = GetGlobalTransform(ChainName);
		FName NewNodeName = FName(*FString(ChainName.ToString() + TEXT("_FK")));
		FName ParentChain = (ChainIndex > 0) ? Spine.FKChains[ChainIndex - 1]: NAME_None;
		AddCtrlGroupNode(NewNodeName, Spine.FKChains[ChainIndex], ParentChain, ChainTransform, ChainName);

		// add IK node
		Spine.IKChains[ChainIndex] = FName(*FString(ChainName.ToString() + TEXT("_IK")));
		ParentChain = (ChainIndex > 0) ? Spine.IKChains[ChainIndex - 1] : NAME_None;
		AddCtrlGroupNode(Spine.IKChains[ChainIndex], Spine.IKChainsResult[ChainIndex], ParentChain, ChainTransform, ChainName, TEXT("_Result"));
		ChainTransformArray[ChainIndex] = ChainTransform;
	}

	// add IK twist control, roll control
	// twist goes to the most children
	Spine.UpperControlIK = FName(TEXT("Spine_UpperControl"));
	// need a way to attach to parent
	AddUniqueNode(Spine.UpperControlIK, NAME_None, ChainTransformArray[NumChain - 1], EndNode);

	Spine.LowerControlIK = FName(TEXT("Spine_LowerControl"));
	AddUniqueNode(Spine.LowerControlIK, NAME_None, ChainTransformArray[0], RootNode);

	// build spline
	TArray<FTransform> ControlPoints;
	BuildSpine(ControlPoints);

	auto CalculateClusterTransform = [](int32 StartIndex, int32 EndIndex, TArray<FTransform>& InControlPoints) -> FTransform
	{
		FVector MidPoint = FVector::ZeroVector;
		for (int32 ControlPointIndex = StartIndex; ControlPointIndex < EndIndex; ++ControlPointIndex)
		{
			MidPoint += InControlPoints[ControlPointIndex].GetTranslation();
		}

		MidPoint /= (EndIndex-StartIndex);

		FTransform MidTransform = FTransform::Identity;
		MidTransform.SetLocation(MidPoint);

		return MidTransform;
	};

	// add cluster node, we'll need to hook up top 2 and bottom 2 separate
	const int32 ControlPointCount = ControlPoints.Num();
	const int32 HalfControlPointCount = FMath::FloorToInt(ControlPointCount/ 2.f);
	FTransform ClusterTransform = CalculateClusterTransform(0, HalfControlPointCount, ControlPoints);
	FName ClusterRootNode = FName(TEXT("Spine_Root_Cluster"));
	AddUniqueNode(ClusterRootNode, Spine.LowerControlIK, ClusterTransform, NAME_None);
	Spine.ClusterRootNode = ClusterRootNode;

	ClusterTransform = CalculateClusterTransform(HalfControlPointCount, ControlPoints.Num(), ControlPoints);
	FName ClusterEndNode = FName(TEXT("Spine_End_Cluster"));
	AddUniqueNode(ClusterEndNode, Spine.UpperControlIK, ClusterTransform, NAME_None);
	Spine.ClusterEndNode = ClusterEndNode;

	Spine.ControlPointNodes.Reset(ControlPointCount);
	Spine.ControlPointNodes.AddDefaulted(ControlPointCount);

	auto CreateControlPointNodes = [this](int32 StartIndex, int32 EndIndex, FName InTargetNode, FName InSecondaryTargetNode, float StartWeight, float EndWeight, TArray<FTransform>& OutControlPoints)
	{
		// add control point nodes
		FConstraintDescription ControlPointOperator;
		ControlPointOperator.bTranslation = true;
		ControlPointOperator.bRotation = true;

		FTransformConstraint ControlPointConstraint;
		ControlPointConstraint.bMaintainOffset = true;
		ControlPointConstraint.Operator = ControlPointOperator;

		float Decrement = (EndIndex - StartIndex - 1) > 0? (EndWeight - StartWeight) / (EndIndex - StartIndex - 1) : 0.f;

		for (int32 ControlPointIndex = StartIndex; ControlPointIndex < EndIndex; ++ControlPointIndex)
		{
			FString ControlPointString = FString(TEXT("ControlPointNode_")) + FString::FromInt(ControlPointIndex);
			FName ControlPointNode = FName(*ControlPointString);
			AddCtrlGroupNode(ControlPointNode, Spine.ControlPointNodes[ControlPointIndex], NAME_None, OutControlPoints[ControlPointIndex], NAME_None);
			ControlPointConstraint.SourceNode = ControlPointNode;
			ControlPointConstraint.TargetNode = InTargetNode;
			ControlPointConstraint.Weight = StartWeight + Decrement * (ControlPointIndex - StartIndex);
			AddConstraint(ControlPointConstraint);

			ControlPointConstraint.SourceNode = ControlPointNode;
			ControlPointConstraint.TargetNode = InSecondaryTargetNode;
			ControlPointConstraint.Weight = 1 - ControlPointConstraint.Weight;
			if (ControlPointConstraint.Weight > ZERO_ANIMWEIGHT_THRESH)
			{
				AddConstraint(ControlPointConstraint);
			}
		}
	};

	CreateControlPointNodes(0, HalfControlPointCount, ClusterRootNode, ClusterEndNode, 1.f, 0.75f, ControlPoints);
	CreateControlPointNodes(HalfControlPointCount, ControlPointCount, ClusterEndNode, ClusterRootNode, 0.75f, 1.f, ControlPoints);

	// assign properties
	Spine.FKControl.SetNum(Spine.ResultChain.Num(), true);
}

void UHumanRig::AddUniqueNode(FName& InOutNodeName, const FName& ParentName, const FTransform& Transform, const FName& LinkNode)
{
	EnsureUniqueName(InOutNodeName);
	AddNode(InOutNodeName, ParentName, Transform, LinkNode);
}

void UHumanRig::EnsureUniqueName(FName& InOutNodeName)
{
	FName NewNodeName = InOutNodeName;
	int32 SuffixIndex = 1;
	const FAnimationHierarchy& MyHierarchy = GetHierarchy();
	while (MyHierarchy.Contains(NewNodeName))
	{
		TArray<FStringFormatArg> Args;
		Args.Add(InOutNodeName.ToString());
		Args.Add(FString::FormatAsNumber(SuffixIndex));
		NewNodeName = FName(*FString::Format(TEXT("{0}_{1}"), Args));
		++SuffixIndex;
	}

	InOutNodeName = NewNodeName;
}

void UHumanRig::AddCtrlGroupNode(FName& InOutNodeName, FName& OutCtrlNodeName, FName InParentNode, FTransform InTransform, FName LinkNode, FString Suffix)
{
	// should verify name
	EnsureUniqueName(InOutNodeName);
	AddNode(InOutNodeName, InParentNode, InTransform, LinkNode);

	// add control name
	OutCtrlNodeName = FName(*FString(InOutNodeName.ToString() + Suffix));
	EnsureUniqueName(OutCtrlNodeName);
	AddNode(OutCtrlNodeName, InOutNodeName, InTransform, LinkNode);
}

void UHumanRig::AddTwoBoneIK(FName UpperNode, FName MiddleNode, FName EndNode, FName& OutJointTarget, FName& OutEffector)
{
	FTransform JointTarget = GetGlobalTransform(MiddleNode);
	JointTarget.SetRotation(FQuat::Identity);
	// this doens't work anymore because it's using property for manipulator
	OutJointTarget = FName(*FString(EndNode.ToString() + TEXT("_IK_JointTarget")));
	EnsureUniqueName(OutJointTarget);
	AddNode(OutJointTarget, NAME_None, JointTarget);

	FTransform EndEffector = GetGlobalTransform(EndNode);
	EndEffector.SetRotation(FQuat::Identity);
	// this doens't work anymore because it's using property for manipulator
	OutEffector = FName(*FString(EndNode.ToString() + TEXT("_IK_Effector")));
	EnsureUniqueName(OutEffector);
	AddNode(OutEffector, NAME_None, EndEffector);
}
#endif // WITH_EDITOR

FTransform UHumanRig::Lerp(const FName& ANode, const FName& BNode, float Weight)
{
	FTransform ATransform = GetGlobalTransform(ANode);
	FTransform BTransform = GetGlobalTransform(BNode);

	if (Weight < ZERO_ANIMWEIGHT_THRESH)
	{
		return ATransform;
	}
	if (Weight > 1 - ZERO_ANIMWEIGHT_THRESH)
	{
		return BTransform;
	}

	BTransform.BlendWith(ATransform, Weight);
	BTransform.NormalizeRotation();
	return BTransform;
}

void UHumanRig::SwitchToIK(FLimbControl& Control)
{
	// switch to IK mode
	//we only focus on 
	FTransform EndTransform = GetGlobalTransform(Control.ResultChain[2]);
	FTransform MidTransform = GetGlobalTransform(Control.ResultChain[1]);
	FTransform RootTransform = GetGlobalTransform(Control.ResultChain[0]);

	// now get the plain of 3 points
	// get joint target
	// first get normal dir to mid joint 
	FVector BaseVector = (EndTransform.GetLocation() - RootTransform.GetLocation()).GetSafeNormal();
	FVector DirToMid = (MidTransform.GetLocation() - RootTransform.GetLocation()).GetSafeNormal();

	// if it's not parallel
	if (FMath::Abs(FVector::DotProduct(BaseVector, DirToMid)) < 0.999f)
	{
		FVector UpVector = FVector::CrossProduct(BaseVector, DirToMid).GetSafeNormal();
		FVector NewDir = FVector::CrossProduct(BaseVector, UpVector);

		// make sure new dir is aligning with dirtomid
		if (FVector::DotProduct(NewDir, DirToMid) < 0.f)
		{
			NewDir *= -1;
		}

		FTransform JointTransform = MidTransform;
		JointTransform.SetLocation(MidTransform.GetLocation() + NewDir * 100.f);

		SetGlobalTransform(Control.IKJointTargetName, JointTransform);
	}
	else
	{
		// @todo fixme:
		SetGlobalTransform(Control.IKJointTargetName, MidTransform);
	}

	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTransform FKTransform = GetGlobalTransform(Control.ResultChain[Index]);
		SetGlobalTransform(Control.IKChainName[Index], FKTransform);
	}

	// effector transform is simple. See if rotation works
	FTransform EndEffector = EndTransform;
	// this is a bit difficult to get, but since IKChainName[2] is consstraint to IKEffector rotation
	// we can't modify its rotation
	// so I'll just grab transform from Resultchain[2] since that's where we get transform above.
	FQuat LastChainRotation = GetGlobalTransform(Control.ResultChain[2]).GetRotation();
	// override rotation using the offset
	EndEffector.SetRotation(LastChainRotation * Control.LastIKChainToIKEnd);
	//EndEffector.SetLocation(EndTransform.GetLocation());
	SetGlobalTransform(Control.IKEffectorName, EndEffector);
}

void UHumanRig::SwitchToFK(FLimbControl& Control)
{
	// switch to FK mode
	// copy all result node transform to FK
	// I think I only have to copy result
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTransform FKTransform = GetGlobalTransform(Control.ResultChain[Index]);
		SetGlobalTransform(Control.FKChainName[Index], FKTransform);
	}

	// save offset
	// get IK last chain and calculate relative transform 
	FQuat LastIKRotation = GetGlobalTransform(Control.IKChainName[2]).GetRotation();
	FQuat EndEffectorRotation = GetGlobalTransform(Control.IKEffectorName).GetRotation();
	Control.LastIKChainToIKEnd = LastIKRotation.Inverse()*EndEffectorRotation;
}

void UHumanRig::SwitchToIK(FSpineControl& Control)
{
	// switch to FK mode
	// copy all result node transform to FK
	// I think I only have to copy result
	for (int32 Index = 0; Index < Control.ResultChain.Num(); ++Index)
	{
		FTransform IKTransform = GetGlobalTransform(Control.ResultChain[Index]);
		SetGlobalTransform(Control.IKChains[Index], IKTransform);
	}

	// build spline
	TArray<FTransform> ControlPoints;
	BuildSpine(ControlPoints);

	auto CalculateClusterTransform = [](int32 StartIndex, int32 EndIndex, TArray<FTransform>& InControlPoints) -> FTransform
	{
		int32 TotalNum = (EndIndex - StartIndex);
		float Weight = 1.f / TotalNum;
		TArray<float> PointWeights;
		TArray<FTransform> ControlPointsTransform;

		for (int32 ControlPointIndex = StartIndex + 1; ControlPointIndex < EndIndex; ++ControlPointIndex)
		{
			ControlPointsTransform.Add(InControlPoints[ControlPointIndex]);
			PointWeights.Add(Weight);
		}

		FTransform MidTransform;
		FAnimationRuntime::BlendTransformsByWeight(MidTransform, ControlPointsTransform, PointWeights);
		return MidTransform;
	};

	// add cluster node, we'll need to hook up top 2 and bottom 2 separate
	const int32 ControlPointCount = ControlPoints.Num();
	const int32 HalfControlPointCount = FMath::FloorToInt(ControlPointCount / 2.f);
	FTransform ClusterTransform = CalculateClusterTransform(0, HalfControlPointCount, ControlPoints);
	SetGlobalTransform(Spine.ClusterRootNode, ClusterTransform);

	ClusterTransform = CalculateClusterTransform(HalfControlPointCount, ControlPoints.Num(), ControlPoints);
	SetGlobalTransform(Spine.ClusterEndNode, ClusterTransform);

	auto SetControlPointNodes = [this](int32 StartIndex, int32 EndIndex, TArray<FTransform>& InControlPoints)
	{
		for (int32 ControlPointIndex = StartIndex; ControlPointIndex < EndIndex; ++ControlPointIndex)
		{
			SetGlobalTransform(Spine.ControlPointNodes[ControlPointIndex], InControlPoints[ControlPointIndex]);
		}
	};

	SetControlPointNodes(0, HalfControlPointCount, ControlPoints);
	SetControlPointNodes(HalfControlPointCount, ControlPointCount, ControlPoints);
}

void UHumanRig::SwitchToFK(FSpineControl& Control)
{
	// switch to FK mode
	// copy all result node transform to FK
	// I think I only have to copy result
	for (int32 Index = 0; Index < Control.ResultChain.Num() ; ++Index)
	{
		FTransform FKTransform = GetGlobalTransform(Control.ResultChain[Index]);
		SetGlobalTransform(Control.FKChains[Index], FKTransform);
	}
}

/** Util to try and get space for a node by name within the Spine. Returns false if failed to find Node in Spine. */
bool GetIKSpaceForNodeInSpine(const FSpineControl& SpineControl, FName Node, EIKSpaceMode& OutIKSpace)
{
	if (SpineControl.UpperControlIK == Node || SpineControl.LowerControlIK == Node)
	{
		OutIKSpace = SpineControl.IKSpaceMode;
		return true;
	}

	for (const FName& Name : SpineControl.FKChains)
	{
		if (Name == Node)
		{
			OutIKSpace = SpineControl.IKSpaceMode;
			return true;
		}
	}

	for (const FName& Name : SpineControl.IKChains)
	{
		if (Name == Node)
		{
			OutIKSpace = SpineControl.IKSpaceMode;
			return true;
		}
	}

	return false;
}

bool UHumanRig::GetIKSpaceForNode(FName Node, EIKSpaceMode& OutIKSpace) const
{
	// Check for node in spine
	if (GetIKSpaceForNodeInSpine(Spine, Node, OutIKSpace))
	{
		return true;
	}

	// Check limbs
	auto GetMatchingIKSpace = [Node, &OutIKSpace](const FLimbControl& LimbControl)
	{
		if (LimbControl.IKEffectorName == Node || LimbControl.IKJointTargetName == Node)
		{
			OutIKSpace = LimbControl.IKSpaceMode;
			return true;
		}

		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (LimbControl.FKChainName[Index] == Node)
			{
				OutIKSpace = LimbControl.IKSpaceMode;
				return true;
			}
			if (LimbControl.IKChainName[Index] == Node)
			{
				OutIKSpace = LimbControl.IKSpaceMode;
				return true;
			}
		}

		return false;
	};

	return ForEachLimb_EarlyOut(GetMatchingIKSpace);
}

bool UHumanRig::IsManipulatorEnabled(UControlManipulator* InManipulator) const
{
	EIKSpaceMode IKSpaceForManipulator;
	if (GetIKSpaceForNode(InManipulator->Name, IKSpaceForManipulator))
	{
		return IKSpaceForManipulator == EIKSpaceMode::UseWeight || IKSpaceForManipulator == InManipulator->KinematicSpace;
	}
	
	return true;
}

void UHumanRig::CacheSpineParameter()
{
	// build spline control point using ik chain data
	// Build cached bone info
	Spine.CachedBoneLengths.Reset();
	Spine.CachedOffsetRotations.Reset();

	if (Spine.IsValid())
	{
		const FAnimationHierarchy& MyHierarchy = GetHierarchy();
		TArray<FTransform> ChainTransform;
		for (int32 ChainIndex = 0; ChainIndex < Spine.IKChains.Num(); ChainIndex++)
		{
			float BoneLength = 0.0f;
			FQuat BoneOffsetRotation = FQuat::Identity;

			const FTransform Transform = GetGlobalTransform(Spine.IKChains[ChainIndex]);
			ChainTransform.Add(Transform);

			if (ChainIndex > 0)
			{
				// previous chain is parent here
				const FTransform ParentTransform = ChainTransform[ChainIndex - 1];
				const FVector BoneDir = Transform.GetLocation() - ParentTransform.GetLocation();
				BoneLength = BoneDir.Size();

				// Calculate a quaternion that gets us from our current rotation to the desired one.
				FVector TransformedAxis = Transform.GetRotation().RotateVector(FMatrix::Identity.GetUnitAxis(Spine.BoneAxis)).GetSafeNormal();
				BoneOffsetRotation = FQuat::FindBetweenNormals(BoneDir.GetSafeNormal(), TransformedAxis);
			}

			Spine.CachedBoneLengths.Add(BoneLength);
			Spine.CachedOffsetRotations.Add(BoneOffsetRotation);
		}
	}
}

void UHumanRig::BuildSpine(TArray<FTransform>& OutControlPoints)
{
	// build spline control point using ik chain data
	if (Spine.IsValid())
	{
		const FAnimationHierarchy& MyHierarchy = GetHierarchy();
		TArray<FTransform> ChainTransform;
		for (int32 ChainIndex = 0; ChainIndex < Spine.IKChains.Num(); ChainIndex++)
		{
			const FTransform Transform = GetGlobalTransform(Spine.IKChains[ChainIndex]);
			ChainTransform.Add(Transform);
		}

		// Setup curve params in component space
		FSplineCurves& BoneSpline = Spine.BoneSpline;
		BoneSpline.Position.Reset();
		BoneSpline.Rotation.Reset();
		BoneSpline.Scale.Reset();

		const int32 ClampedPointCount = FMath::Max(2, Spine.PointCount);
		if (Spine.bAutoCalculateSpline || ClampedPointCount == Spine.IKChains.Num())
		{
			// We are auto-calculating, so use each bone as a control point
			int32 TotalChainCount = Spine.IKChains.Num();
			OutControlPoints.Reset(TotalChainCount);
			OutControlPoints.AddZeroed(TotalChainCount);
			for (int32 ChainIndex = 0; ChainIndex < TotalChainCount; ChainIndex++)
			{
				// @note : this should be correct?
				const float CurveAlpha = (float)(ChainIndex);

				const FTransform& Transform = ChainTransform[ChainIndex];
				OutControlPoints[ChainIndex] = Transform;
				BoneSpline.Position.Points.Emplace(CurveAlpha, Transform.GetLocation(), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
				BoneSpline.Rotation.Points.Emplace(CurveAlpha, Transform.GetRotation(), FQuat::Identity, FQuat::Identity, CIM_Linear);
				BoneSpline.Scale.Points.Emplace(CurveAlpha, Transform.GetScale3D(), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
			}
		}
		else
		{
			// We are not auto-calculating, so we need to build an approximation to the curve. First we build a curve using our transformed curve
			// as a temp storage area, then we evaluate the curve at appropriate points to approximate the bone chain with a new cubic.
			FSplineCurves Spline;
			
			// Build the linear spline
			float TotalChainLinks = (float)(Spine.IKChains.Num() - 1);
			for (int32 ChainIndex = 0; ChainIndex < Spine.IKChains.Num(); ChainIndex++)
			{
				const float CurveAlpha = (float)ChainIndex / TotalChainLinks;

				const FTransform& Transform = ChainTransform[ChainIndex];
				Spline.Position.Points.Emplace(CurveAlpha, Transform.GetLocation(), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
				Spline.Rotation.Points.Emplace(CurveAlpha, Transform.GetRotation(), FQuat::Identity, FQuat::Identity, CIM_Linear);
				Spline.Scale.Points.Emplace(CurveAlpha, Transform.GetScale3D(), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
			}

			Spline.UpdateSpline();

			// now build the approximation
			OutControlPoints.Reset(ClampedPointCount);
			OutControlPoints.AddZeroed(ClampedPointCount);
			float TotalPointLinks = (float)(ClampedPointCount - 1);
			for (int32 PointIndex = 0; PointIndex < ClampedPointCount; ++PointIndex)
			{
				const float CurveAlpha = (float)PointIndex / TotalPointLinks;
				FVector EvalPosition = Spline.Position.Eval(CurveAlpha);
				FQuat	EvalQuat = Spline.Rotation.Eval(CurveAlpha);
				FVector	EvalScale = Spline.Scale.Eval(CurveAlpha);

				OutControlPoints[PointIndex] = FTransform(EvalQuat, EvalPosition, EvalScale);
				BoneSpline.Position.Points.Emplace(CurveAlpha, EvalPosition, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
				BoneSpline.Rotation.Points.Emplace(CurveAlpha, EvalQuat, FQuat::Identity, FQuat::Identity, CIM_Linear);
				BoneSpline.Scale.Points.Emplace(CurveAlpha, EvalScale, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
			}
		}

		BoneSpline.UpdateSpline();

		Spine.OriginalSplineLength = BoneSpline.GetSplineLength();

		FSplinePositionLinearApproximation::Build(BoneSpline, Spine.LinearApproximation);
	}
}

float UHumanRig::GetSpineTwist(float InAlpha, float TotalSplineAlpha)
{
	Spine.TwistBlend.SetAlpha(InAlpha / TotalSplineAlpha);
	return Spine.TwistBlend.GetBlendedValue();
}

void UHumanRig::EvaluateSpine()
{
	if (Spine.IsValid())
	{
		CorrectIKSpace(Spine);

		if (Spine.IKBlendWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			TransformSpline();

			const float TotalSplineLength = Spine.TransformedSpline.GetSplineLength();
			const float TotalSplineAlpha = Spine.TransformedSpline.ReparamTable.Points.Last().OutVal;
			Spine.TwistBlend.SetValueRange(Spine.TwistStart, Spine.TwistEnd);

			TArray<FTransform> Transforms;
			TArray<FTransform> OutTransforms;

			for (int32 ChainIndex = 0; ChainIndex < Spine.IKChains.Num(); ChainIndex++)
			{
				// get IK transform
				Transforms.Add(GetGlobalTransform(Spine.IKChains[ChainIndex]));
			}

			OutTransforms.AddZeroed(Transforms.Num());

			AnimationCore::SolveSplineIK(Transforms, Spine.TransformedSpline.Position, Spine.TransformedSpline.Rotation, Spine.TransformedSpline.Scale, TotalSplineAlpha, TotalSplineLength,
				FFloatMapping::CreateUObject(this, &UHumanRig::GetSpineTwist, TotalSplineAlpha), Spine.Roll, Spine.Stretch, Spine.Offset, Spine.BoneAxis,
				FFindParamAtFirstSphereIntersection::CreateUObject(this, &UHumanRig::FindParamAtFirstSphereIntersection), Spine.CachedOffsetRotations, Spine.CachedBoneLengths, Spine.OriginalSplineLength, OutTransforms);

			for (int32 ChainIndex = 0; ChainIndex < Spine.IKChains.Num(); ChainIndex++)
			{
				// get IK transform
				SetGlobalTransform(Spine.IKChainsResult[ChainIndex], OutTransforms[ChainIndex]);
			}

			// now blend between IKChainResult and FKResult;
			float BlendWeight = Spine.IKBlendWeight;
			for (int32 Index = 0; Index < Spine.ResultChain.Num(); ++Index)
			{
				SetGlobalTransform(Spine.ResultChain[Index], Lerp(Spine.FKChains[Index], Spine.IKChainsResult[Index], BlendWeight));
			}
		}
		else
		{
			for (int32 Index = 0; Index < Spine.ResultChain.Num(); ++Index)
			{
				FTransform FKTransform = GetGlobalTransform(Spine.FKChains[Index]);
				SetGlobalTransform(Spine.ResultChain[Index], FKTransform);
			}
		}
	}
}

UControlManipulator* UHumanRig::FindManipulatorForNode(FName Node) const
{
	UControlManipulator* Result = nullptr;
	for (UControlManipulator* Manipulator : Manipulators)
	{
		if (Node == Manipulator->Name)
		{
			Result = Manipulator;
			break;
		}
	}
	return Result;
}

void UHumanRig::TransformSpline()
{
	Spine.TransformedSpline.Position.Reset();
	Spine.TransformedSpline.Rotation.Reset();
	Spine.TransformedSpline.Scale.Reset();

	FSplineCurves& BoneSpline = Spine.BoneSpline;
	for (int32 PointIndex = 0; PointIndex < BoneSpline.Position.Points.Num(); PointIndex++)
	{
		FTransform PointTransform = GetGlobalTransform(Spine.ControlPointNodes[PointIndex]);

		Spine.TransformedSpline.Position.Points.Emplace(BoneSpline.Position.Points[PointIndex]);
		Spine.TransformedSpline.Rotation.Points.Emplace(BoneSpline.Rotation.Points[PointIndex]);
		Spine.TransformedSpline.Scale.Points.Emplace(BoneSpline.Scale.Points[PointIndex]);

		Spine.TransformedSpline.Position.Points[PointIndex].OutVal = PointTransform.GetLocation();
		Spine.TransformedSpline.Rotation.Points[PointIndex].OutVal = PointTransform.GetRotation();
		Spine.TransformedSpline.Scale.Points[PointIndex].OutVal = PointTransform.GetScale3D();
	}

	Spine.TransformedSpline.UpdateSpline();

	FSplinePositionLinearApproximation::Build(Spine.TransformedSpline, Spine.LinearApproximation);
}

float UHumanRig::FindParamAtFirstSphereIntersection(const FVector& InOrigin, float InRadius, int32& StartingLinearIndex)
{
	const float RadiusSquared = InRadius * InRadius;
	const int32 LinearCount = Spine.LinearApproximation.Num() - 1;
	for (int32 LinearIndex = StartingLinearIndex; LinearIndex < LinearCount; ++LinearIndex)
	{
		const FSplinePositionLinearApproximation& LinearPoint = Spine.LinearApproximation[LinearIndex];
		const FSplinePositionLinearApproximation& NextLinearPoint = Spine.LinearApproximation[LinearIndex + 1];

		const float InnerDistanceSquared = (InOrigin - LinearPoint.Position).SizeSquared();
		const float OuterDistanceSquared = (InOrigin - NextLinearPoint.Position).SizeSquared();
		if (InnerDistanceSquared <= RadiusSquared && OuterDistanceSquared >= RadiusSquared)
		{
			StartingLinearIndex = LinearIndex;

			const float InnerDistance = FMath::Sqrt(InnerDistanceSquared);
			const float OuterDistance = FMath::Sqrt(OuterDistanceSquared);
			const float InterpParam = FMath::Clamp((InRadius - InnerDistance) / (OuterDistance - InnerDistance), 0.0f, 1.0f);

			return FMath::Lerp(LinearPoint.SplineParam, NextLinearPoint.SplineParam, InterpParam);
		}
	}

	StartingLinearIndex = 0;
	return Spine.TransformedSpline.ReparamTable.Points.Last().OutVal;
}

UControlManipulator* UHumanRig::FindCounterpartManipulator(UControlManipulator* InManipulator) const
{
	auto FindManipulatorForFKChain = [this](const FLimbControl& LimbControl, bool bEnd) -> UControlManipulator*
	{
		for (UControlManipulator* Manipulator : Manipulators)
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (bEnd && Index == 2 && LimbControl.FKChainName[Index] == Manipulator->Name)
				{
					return Manipulator;
				}

				if (!bEnd && Index == 1 && LimbControl.FKChainName[Index] == Manipulator->Name)
				{
					return Manipulator;
				}
			}
		}

		return nullptr;
	};

	auto FindManipulatorForIKChain = [this](const FLimbControl& LimbControl, bool bEnd) -> UControlManipulator*
	{
		for (UControlManipulator* Manipulator : Manipulators)
		{
			if (bEnd && LimbControl.IKEffectorName == Manipulator->Name)
			{
				return Manipulator;
			}

			if (!bEnd && LimbControl.IKJointTargetName == Manipulator->Name)
			{
				return Manipulator;
			}
		}

		return nullptr;
	};

	auto GetMatchingManipulator = [InManipulator, &FindManipulatorForFKChain, &FindManipulatorForIKChain](const FLimbControl& LimbControl) -> UControlManipulator*
	{
		if (LimbControl.IKEffectorName == InManipulator->Name)
		{
			return FindManipulatorForFKChain(LimbControl, true);
		}

		if (LimbControl.IKJointTargetName == InManipulator->Name)
		{
			return FindManipulatorForFKChain(LimbControl, false);
		}

		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (LimbControl.FKChainName[Index] == InManipulator->Name)
			{
				return FindManipulatorForIKChain(LimbControl, Index == 2);
			}
			if (LimbControl.IKChainName[Index] == InManipulator->Name)
			{
				return FindManipulatorForIKChain(LimbControl, Index == 2);
			}
		}

		return nullptr;
	};

	return ForEachLimb_Manipulator(GetMatchingManipulator);
}

FName UHumanRig::FindNodeDrivenByNode(FName InNodeName) const
{
	// helper function to find a driven node in a limb
	auto GetMatchingManipulator = [InNodeName](const FLimbControl& LimbControl) -> FName
	{
		if (LimbControl.IKEffectorName == InNodeName || LimbControl.FKChainName[2] == InNodeName)
		{
			return LimbControl.ResultChain[2];
		}

		if (LimbControl.IKJointTargetName == InNodeName || LimbControl.FKChainName[1] == InNodeName)
		{
			return LimbControl.ResultChain[1];
		}

		if (LimbControl.FKChainName[0] == InNodeName)
		{
			return LimbControl.ResultChain[0];
		}

		return NAME_None;
	};

	// First try limbs
	FName NodeName = ForEachLimb_Name(GetMatchingManipulator);
	if (NodeName != NAME_None)
	{
		return NodeName;
	}

	int32 NumLinks = FMath::Min(FMath::Min(Spine.FKChains.Num(), Spine.IKChains.Num()), Spine.ResultChain.Num());

	// try spine
	for (int32 ChainIndex = 0; ChainIndex < NumLinks; ChainIndex++)
	{
		if (Spine.FKChains[ChainIndex] == InNodeName)
		{
			return Spine.ResultChain[ChainIndex];
		}

		if (Spine.IKChains[ChainIndex] == InNodeName)
		{
			return Spine.ResultChain[ChainIndex];
		}
	}

	if (Spine.UpperControlIK == InNodeName && Spine.ResultChain.Num() > 0)
	{
		return Spine.ResultChain.Last();
	}

	if (Spine.LowerControlIK == InNodeName && Spine.ResultChain.Num() > 0)
	{
		return Spine.ResultChain[0];
	}

	return NAME_None;
}

void UHumanRig::ForEachLimb(const TFunctionRef<void(FLimbControl&)>& InPredicate)
{
	InPredicate(LeftArm);
	InPredicate(RightArm);
	InPredicate(LeftLeg);
	InPredicate(RightLeg);
}

bool UHumanRig::ForEachLimb_EarlyOut(const TFunctionRef<bool(const FLimbControl&)>& InPredicate) const
{
	if (InPredicate(LeftArm) || InPredicate(RightArm) || InPredicate(LeftLeg) || InPredicate(RightLeg))
	{
		return true;
	}

	return false;
}

UControlManipulator* UHumanRig::ForEachLimb_Manipulator(const TFunctionRef<UControlManipulator*(const FLimbControl&)>& InPredicate) const
{
	UControlManipulator* Manipulator = InPredicate(LeftArm);
	if (Manipulator)
	{
		return Manipulator;
	}
	
	Manipulator = InPredicate(RightArm);
	if (Manipulator)
	{
		return Manipulator;
	}
	
	Manipulator = InPredicate(LeftLeg);
	if (Manipulator)
	{
		return Manipulator;
	}
	
	Manipulator = InPredicate(RightLeg);
	if (Manipulator)
	{
		return Manipulator;
	}

	return nullptr;
}

FName UHumanRig::ForEachLimb_Name(const TFunctionRef<FName(const FLimbControl&)>& InPredicate) const
{
	FName Name = InPredicate(LeftArm);
	if (Name != NAME_None)
	{
		return Name;
	}

	Name = InPredicate(RightArm);
	if (Name != NAME_None)
	{
		return Name;
	}

	Name = InPredicate(LeftLeg);
	if (Name != NAME_None)
	{
		return Name;
	}

	Name = InPredicate(RightLeg);
	if (Name != NAME_None)
	{
		return Name;
	}

	return NAME_None;
}

void UHumanRig::GetDependentArray(const FName& NodeName, TArray<FName>& OutList) const
{
	// make sure to add IKs chains 
	Super::GetDependentArray(NodeName, OutList);

	// IK chains are dependent on effector/joint targets
	auto AddToDependentArray = [this, NodeName, &OutList](const FLimbControl& LimbControl)
	{
		for (int32 IKChainIdx = 0; IKChainIdx < 3; ++IKChainIdx)
		{
			if (LimbControl.IKChainName[IKChainIdx] == NodeName)
			{
				OutList.Add(LimbControl.IKEffectorName);
				OutList.Add(LimbControl.IKJointTargetName);
			}
		}
	};

	AddToDependentArray(LeftArm);
	AddToDependentArray(RightArm);
	AddToDependentArray(LeftLeg);
	AddToDependentArray(RightLeg);

	// check spine
	for (int32 IKChainIdx = 0; IKChainIdx < Spine.IKChains.Num(); ++IKChainIdx)
	{
		if (NodeName == Spine.IKChains[IKChainIdx])
		{
			OutList.Add(Spine.LowerControlIK);
			OutList.Add(Spine.UpperControlIK);
		}
	}
}

///////////////////////////////////////////////////////////////////
// FLimbControl
/////////////////////////////////////////////////////////////
void FLimbControl::Initialize(float InUpperLimbLen, float InLowerLimbLen)
{
	UpperLimbLength = InUpperLimbLen;
	LowerLimbLength = InLowerLimbLen;
	bFirstTick = true;
}

///////////////////////////////////////////////////////////////////
// FPoseKey
/////////////////////////////////////////////////////////////

bool FPoseKey::GetBlendedResult(FName InNodeName, float InKeyValue, FTransform& OutTransform) const
{
	const FTransformKeys* FoundKeys = TransformKeys.Find(InNodeName);

	if (FoundKeys && FoundKeys->Keys.Num() > 0)
	{
		const TArray<FTransformKey>& Keys = FoundKeys->Keys;

		// this is very simple version of curve, so just iterate through to find InKeyValue and blend between
		// we assume this key is in the order
		FTransform TransformA = Keys[0].Transform, TransformB = Keys.Last().Transform;
		float TransformAValue = Keys[0].Value, TransformBValue = Keys.Last().Value;

		for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
		{
			const FTransformKey& Key = Keys[KeyIndex];
			if (InKeyValue > Key.Value)
			{
				TransformAValue = Key.Value;
				TransformA = Key.Transform;
			}
			else
			{
				TransformBValue = Key.Value;
				// if less than current value, just use it
				TransformB = Key.Transform;
				break;
			}
		}

		float Range = TransformBValue - TransformAValue;
		if (Range > ZERO_ANIMWEIGHT_THRESH)
		{
			float BlendWeight = FMath::Clamp<float>((InKeyValue - TransformAValue) / Range, 0.f, 1.f);
			TransformA.BlendWith(TransformB, BlendWeight);
			OutTransform = TransformA;
		}
		else
		{
			// ensure - just in case users didn't mean this
			ensure(TransformA.Equals(TransformB));
			// otherwise, just send any transform, they're assume to be same vlaue
			OutTransform = TransformA;
			OutTransform.NormalizeRotation();
		}

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////
// FFingerDescription
/////////////////////////////////////////////////////////////
TArray<FName> FFingerDescription::GetNodeNames() const
{
	TArray<FName> OutNodeNames;
	for (int32 Index = 0; Index < ChainNum; ++Index)
	{
		TArray<FStringFormatArg> Args;
		Args.Add(NamePrefix);
		Args.Add(LexicalConversion::ToString(Index+1));
		Args.Add(NameSuffix);

		FString NodeString = FString::Format(TEXT("{0}_0{1}{2}"), Args);
		OutNodeNames.Add(FName(*NodeString));
	}

	return OutNodeNames;
}
#undef LOCTEXT_NAMESPACE