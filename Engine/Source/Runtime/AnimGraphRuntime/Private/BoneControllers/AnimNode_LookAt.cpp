// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_LookAt.h"
#include "SceneManagement.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationCoreLibrary.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"

static const FVector DefaultLookAtAxis(0.f, 1.f, 0.f);
static const FVector DefaultLookUpAxis(1.f, 0.f, 0.f);

/////////////////////////////////////////////////////
// FAnimNode_LookAt

FAnimNode_LookAt::FAnimNode_LookAt()
	: LookAtLocation(FVector(100.f, 0.f, 0.f))
	, LookAtAxis_DEPRECATED(EAxisOption::Y)
	, CustomLookAtAxis_DEPRECATED(FVector(0.f, 1.f, 0.f))
	, LookAt_Axis(DefaultLookAtAxis)
	, LookUpAxis_DEPRECATED(EAxisOption::X)
	, CustomLookUpAxis_DEPRECATED(FVector(1.f, 0.f, 0.f))
	, LookUp_Axis(DefaultLookUpAxis)
	, LookAtClamp(0.f)
	, InterpolationTime(0.f)
	, InterpolationTriggerThreashold(0.f)
	, CurrentLookAtLocation(FVector::ZeroVector)
	, AccumulatedInterpoolationTime(0.f)
{
}

void FAnimNode_LookAt::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	if (LookAtTarget.HasValidSetup())
	{
		DebugLine += FString::Printf(TEXT(" Bone: %s, Look At Target: %s, Look At Location: %s, Target Location : %s)"), *BoneToModify.BoneName.ToString(), *LookAtTarget.GetTargetSetup().ToString(), *LookAtLocation.ToString(), *CachedCurrentTargetLocation.ToString());
	}
	else
	{
		DebugLine += FString::Printf(TEXT(" Bone: %s, Look At Location : %s, Target Location : %s)"), *BoneToModify.BoneName.ToString(), *LookAtLocation.ToString(), *CachedCurrentTargetLocation.ToString());
	}

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

float FAnimNode_LookAt::AlphaToBlendType(float InAlpha, EInterpolationBlend::Type BlendType)
{
	switch (BlendType)
	{
	case EInterpolationBlend::Sinusoidal: return FMath::Clamp<float>((FMath::Sin(InAlpha * PI - HALF_PI) + 1.f) / 2.f, 0.f, 1.f);
	case EInterpolationBlend::Cubic: return FMath::Clamp<float>(FMath::CubicInterp<float>(0.f, 0.f, 1.f, 0.f, InAlpha), 0.f, 1.f);
	case EInterpolationBlend::EaseInOutExponent2: return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 2), 0.f, 1.f);
	case EInterpolationBlend::EaseInOutExponent3: return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 3), 0.f, 1.f);
	case EInterpolationBlend::EaseInOutExponent4: return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 4), 0.f, 1.f);
	case EInterpolationBlend::EaseInOutExponent5: return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 5), 0.f, 1.f);
	}

	return InAlpha;
}

void FAnimNode_LookAt::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FCompactPoseBoneIndex ModifyBoneIndex = BoneToModify.GetCompactPoseIndex(BoneContainer);
	FTransform ComponentBoneTransform = Output.Pose.GetComponentSpaceTransform(ModifyBoneIndex);

	// get target location
	FTransform TargetTransform = LookAtTarget.GetTargetTransform(LookAtLocation, Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());
	FVector TargetLocationInComponentSpace = TargetTransform.GetLocation();
	
	FVector OldCurrentTargetLocation = CurrentTargetLocation;
	FVector NewCurrentTargetLocation = TargetLocationInComponentSpace;

	if ((NewCurrentTargetLocation - OldCurrentTargetLocation).SizeSquared() > InterpolationTriggerThreashold*InterpolationTriggerThreashold)
	{
		if (AccumulatedInterpoolationTime >= InterpolationTime)
		{
			// reset current Alpha, we're starting to move
			AccumulatedInterpoolationTime = 0.f;
		}

		PreviousTargetLocation = OldCurrentTargetLocation;
		CurrentTargetLocation = NewCurrentTargetLocation;
	}
	else if (InterpolationTriggerThreashold == 0.f)
	{
		CurrentTargetLocation = NewCurrentTargetLocation;
	}

	if (InterpolationTime > 0.f)
	{
		float CurrentAlpha = AccumulatedInterpoolationTime/InterpolationTime;

		if (CurrentAlpha < 1.f)
		{
			float BlendAlpha = AlphaToBlendType(CurrentAlpha, InterpolationType);

			CurrentLookAtLocation = FMath::Lerp(PreviousTargetLocation, CurrentTargetLocation, BlendAlpha);
		}
	}
	else
	{
		CurrentLookAtLocation = CurrentTargetLocation;
	}

#if !UE_BUILD_SHIPPING
	CachedOriginalTransform = ComponentBoneTransform;
	CachedTargetCoordinate = LookAtTarget.GetTargetTransform(FVector::ZeroVector, Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());
	CachedPreviousTargetLocation = PreviousTargetLocation;
	CachedCurrentLookAtLocation = CurrentLookAtLocation;
#endif
	CachedCurrentTargetLocation = CurrentTargetLocation;

	// lookat vector
	FVector LookAtVector = LookAt_Axis.GetTransformedAxis(ComponentBoneTransform);
	// find look up vector in local space
	FVector LookUpVector = LookUp_Axis.GetTransformedAxis(ComponentBoneTransform);
	// Find new transform from look at info
	FQuat DeltaRotation = AnimationCore::SolveAim(ComponentBoneTransform, CurrentLookAtLocation, LookAtVector, bUseLookUpAxis, LookUpVector, LookAtClamp);
	ComponentBoneTransform.SetRotation(DeltaRotation * ComponentBoneTransform.GetRotation());
	// Set New Transform 
	OutBoneTransforms.Add(FBoneTransform(ModifyBoneIndex, ComponentBoneTransform));

#if !UE_BUILD_SHIPPING
	CachedLookAtTransform = ComponentBoneTransform;
#endif
}

void FAnimNode_LookAt::EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context)
{
	Super::EvaluateComponentSpaceInternal(Context);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/*if (bEnableDebug)
	{
		const FTransform LocalToWorld = Context.AnimInstanceProxy->GetSkelMeshCompLocalToWorld();
		FVector TargetWorldLoc = LocalToWorld.TransformPosition(CachedCurrentTargetLocation);
		FVector SourceWorldLoc = LocalToWorld.TransformPosition(CachedComponentBoneLocation);

		Context.AnimInstanceProxy->AnimDrawDebugLine(SourceWorldLoc, TargetWorldLoc, FColor::Green);
	}*/

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool FAnimNode_LookAt::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	return (BoneToModify.IsValidToEvaluate(RequiredBones) &&
		// or if name isn't set (use Look At Location) or Look at bone is valid 
		// do not call isValid since that means if look at bone isn't in LOD, we won't evaluate
		// we still should evaluate as long as the BoneToModify is valid even LookAtBone isn't included in required bones
		(!LookAtTarget.HasTargetSetup() || LookAtTarget.IsValidToEvaluate(RequiredBones)) );
}

#if WITH_EDITOR
// can't use World Draw functions because this is called from Render of viewport, AFTER ticking component, 
// which means LineBatcher already has ticked, so it won't render anymore
// to use World Draw functions, we have to call this from tick of actor
void FAnimNode_LookAt::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp) const
{
	auto CalculateLookAtMatrixFromTransform = [this](const FTransform& BaseTransform) -> FMatrix
	{
		FVector TransformedLookAtAxis = BaseTransform.TransformVector(LookAt_Axis.Axis);
		const FVector DefaultUpVector = BaseTransform.GetUnitAxis(EAxis::Z);
		FVector UpVector = (bUseLookUpAxis) ? BaseTransform.TransformVector(LookUp_Axis.Axis) : DefaultUpVector;
		// if parallel with up vector, find something else
		if (FMath::Abs(FVector::DotProduct(UpVector, TransformedLookAtAxis)) > (1.f - ZERO_ANIMWEIGHT_THRESH))
		{
			UpVector = BaseTransform.GetUnitAxis(EAxis::X);
		}

		FVector RightVector = FVector::CrossProduct(TransformedLookAtAxis, UpVector);
		FMatrix Matrix;
		FVector Location = BaseTransform.GetLocation();
		Matrix.SetAxes(&TransformedLookAtAxis, &RightVector, &UpVector, &Location);
		return Matrix;
	};

	// did not apply any of LocaltoWorld
	if(PDI && MeshComp)
	{
		FTransform LocalToWorld = MeshComp->GetComponentTransform();
		FTransform ComponentTransform = CachedOriginalTransform * LocalToWorld;
		FTransform LookAtTransform = CachedLookAtTransform * LocalToWorld;
		FTransform TargetTrasnform = CachedTargetCoordinate * LocalToWorld;
		FVector BoneLocation = LookAtTransform.GetLocation();

		// we're using interpolation, so print previous location
		const bool bUseInterpolation = InterpolationTime > 0.f;
		if(bUseInterpolation)
		{
			// this only will be different if we're interpolating
			DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedPreviousTargetLocation), FColor(0, 255, 0), 5.f, SDPG_World);
		}

		// current look at location (can be clamped or interpolating)
		DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedCurrentLookAtLocation), FColor::Yellow, 5.f, SDPG_World);
		DrawWireStar(PDI, CachedCurrentLookAtLocation, 5.0f, FColor::Yellow, SDPG_World);

		// draw current target information
		DrawDashedLine(PDI, BoneLocation, LocalToWorld.TransformPosition(CachedCurrentTargetLocation), FColor::Blue, 5.f, SDPG_World);
		DrawWireStar(PDI, CachedCurrentTargetLocation, 5.0f, FColor::Blue, SDPG_World);

		// draw the angular clamp
		if (LookAtClamp > 0.f)
		{
			float Angle = FMath::DegreesToRadians(LookAtClamp);
			float ConeSize = 30.f;
			DrawCone(PDI, FScaleMatrix(ConeSize) * CalculateLookAtMatrixFromTransform(ComponentTransform), Angle, Angle, 20, false, FLinearColor::Green, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
		}

		// draw directional  - lookat and look up
		DrawDirectionalArrow(PDI, CalculateLookAtMatrixFromTransform(LookAtTransform), FLinearColor::Red, 20, 5, SDPG_World);
		DrawCoordinateSystem(PDI, BoneLocation, LookAtTransform.GetRotation().Rotator(), 20.f, SDPG_Foreground);
		DrawCoordinateSystem(PDI, TargetTrasnform.GetLocation(), TargetTrasnform.GetRotation().Rotator(), 20.f, SDPG_Foreground);
	}
}
#endif // WITH_EDITOR

void FAnimNode_LookAt::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	BoneToModify.Initialize(RequiredBones);
	LookAtTarget.InitializeBoneReferences(RequiredBones);
}

void FAnimNode_LookAt::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	AccumulatedInterpoolationTime = FMath::Clamp(AccumulatedInterpoolationTime+Context.GetDeltaTime(), 0.f, InterpolationTime);;
}

void FAnimNode_LookAt::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

	LookAtTarget.Initialize(Context.AnimInstanceProxy);

	// initialize
	LookUp_Axis.Initialize();
	if (LookUp_Axis.Axis.IsZero())
	{
		UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-up axis specified in LookAt node. Reverting to default."));
		LookUp_Axis.Axis = DefaultLookUpAxis;
	}
	LookAt_Axis.Initialize();
	if (LookAt_Axis.Axis.IsZero())
	{
		UE_LOG(LogAnimation, Warning, TEXT("Zero-length look-at axis specified in LookAt node. Reverting to default."));
		LookAt_Axis.Axis = DefaultLookAtAxis;
	}
}
