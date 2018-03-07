// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNode_LiveLinkPose.h"
#include "ILiveLinkClient.h"

#include "Features/IModularFeatures.h"

#include "AnimInstanceProxy.h"

#include "LiveLinkRemapAsset.h"

FAnimNode_LiveLinkPose::FAnimNode_LiveLinkPose() 
	: RetargetAsset(ULiveLinkRemapAsset::StaticClass())
	, LiveLinkClient(nullptr)
{
}

void FAnimNode_LiveLinkPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();

	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	}

	CurrentRetargetAsset = nullptr;
}

void FAnimNode_LiveLinkPose::Update_AnyThread(const FAnimationUpdateContext & Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	// Protection as a class graph pin does not honour rules on abstract classes and NoClear
	if (!RetargetAsset.Get() || RetargetAsset.Get()->HasAnyClassFlags(CLASS_Abstract))
	{
		RetargetAsset = ULiveLinkRemapAsset::StaticClass();
	}

	if (!CurrentRetargetAsset || RetargetAsset != CurrentRetargetAsset->GetClass())
	{
		CurrentRetargetAsset = NewObject<ULiveLinkRetargetAsset>(Context.AnimInstanceProxy->GetAnimInstanceObject(), *RetargetAsset);
	}
}

void FAnimNode_LiveLinkPose::Evaluate_AnyThread(FPoseContext& Output)
{
	Output.ResetToRefPose();

	if (!LiveLinkClient || !CurrentRetargetAsset)
	{
		return;
	}

	

	if(const FLiveLinkSubjectFrame* Subject = LiveLinkClient->GetSubjectData(SubjectName))
	{
		CurrentRetargetAsset->BuildPoseForSubject(*Subject, Output.Pose, Output.Curve);
	}
}


void FAnimNode_LiveLinkPose::OnLiveLinkClientRegistered(const FName& Type, class IModularFeature* ModularFeature)
{
	if (Type == ILiveLinkClient::ModularFeatureName && !LiveLinkClient)
	{
		LiveLinkClient = static_cast<ILiveLinkClient*>(ModularFeature);
	}
}

void FAnimNode_LiveLinkPose::OnLiveLinkClientUnregistered(const FName& Type, class IModularFeature* ModularFeature)
{
	if (Type == ILiveLinkClient::ModularFeatureName && ModularFeature == LiveLinkClient)
	{
		LiveLinkClient = nullptr;
	}
}
