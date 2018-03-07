// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkRetargetAsset.h"

#include "Animation/Skeleton.h"
#include "LiveLinkTypes.h"
#include "BonePose.h"
#include "AnimTypes.h"

ULiveLinkRetargetAsset::ULiveLinkRetargetAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULiveLinkRetargetAsset::BuildCurveData(const FLiveLinkSubjectFrame& InFrame, const FCompactPose& InPose, FBlendedCurve& OutCurve) const
{
	const USkeleton* Skeleton = InPose.GetBoneContainer().GetSkeletonAsset();

	for (int32 CurveIdx = 0; CurveIdx < InFrame.CurveKeyData.CurveNames.Num(); ++CurveIdx)
	{
		const FOptionalCurveElement& Curve = InFrame.Curves[CurveIdx];
		if (Curve.IsValid())
		{
			FName CurveName = InFrame.CurveKeyData.CurveNames[CurveIdx];

			SmartName::UID_Type UID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, CurveName);
			if (UID != SmartName::MaxUID)
			{
				OutCurve.Set(UID, Curve.Value);
			}
		}
	}
}
