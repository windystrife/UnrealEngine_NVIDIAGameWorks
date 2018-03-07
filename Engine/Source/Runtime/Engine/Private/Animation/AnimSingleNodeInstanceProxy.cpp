// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "AnimationRuntime.h"
#include "Animation/AnimComposite.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimSingleNodeInstance.h"

void FAnimSingleNodeInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	CurrentAsset = NULL;
#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
#endif

	UpdateCounter.Reset();

	// it's already doing it when evaluate
	BlendSpaceInput = FVector::ZeroVector;
	CurrentTime = 0.f;

	// initialize node manually 
	FAnimationInitializeContext InitContext(this);
	SingleNode.Initialize_AnyThread(InitContext);
}

bool FAnimSingleNodeInstanceProxy::Evaluate(FPoseContext& Output)
{
	SingleNode.Evaluate_AnyThread(Output);

	return true;
}

#if WITH_EDITORONLY_DATA
void FAnimSingleNodeInstanceProxy::PropagatePreviewCurve(FPoseContext& Output) 
{
	USkeleton* MySkeleton = GetSkeleton();
	for (auto Iter = PreviewCurveOverride.CreateConstIterator(); Iter; ++Iter)
	{
		const FName& Name = Iter.Key();
		const float Value = Iter.Value();

		FSmartName PreviewCurveName;

		if (MySkeleton->GetSmartNameByName(USkeleton::AnimCurveMappingName, Name, PreviewCurveName))
		{
			Output.Curve.Set(PreviewCurveName.UID, Value);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void FAnimSingleNodeInstanceProxy::UpdateAnimationNode(float DeltaSeconds)
{
	UpdateCounter.Increment();

	FAnimationUpdateContext UpdateContext(this, DeltaSeconds);
	SingleNode.Update_AnyThread(UpdateContext);
}

void FAnimSingleNodeInstanceProxy::PostUpdate(UAnimInstance* InAnimInstance) const
{
	FAnimInstanceProxy::PostUpdate(InAnimInstance);

	// sync up playing state for active montage instances
	int32 EvaluationDataIndex = 0;
	const TArray<FMontageEvaluationState>& EvaluationData = GetMontageEvaluationData();
	for (FAnimMontageInstance* MontageInstance : InAnimInstance->MontageInstances)
	{
		if (MontageInstance->Montage && MontageInstance->GetWeight() > ZERO_ANIMWEIGHT_THRESH)
		{
			// sanity check we are playing the same montage
			check(MontageInstance->Montage == EvaluationData[EvaluationDataIndex].Montage);
			MontageInstance->bPlaying = EvaluationData[EvaluationDataIndex].bIsPlaying;
			EvaluationDataIndex++;
		}
	}
}

void FAnimSingleNodeInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) 
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
#if WITH_EDITOR
	// @fixme only do this in pose asset
// 	// copy data to PreviewPoseOverride
// 	TMap<FName, float> PoseCurveList;
// 
// 	InAnimInstance->GetAnimationCurveList(ACF_DrivesPose, PoseCurveList);
// 
// 	if (PoseCurveList.Num() > 0)
// 	{
// 		PreviewPoseOverride.Append(PoseCurveList);
// 	}
#endif // WITH_EDITOR
}
void FAnimSingleNodeInstanceProxy::InitializeObjects(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::InitializeObjects(InAnimInstance);

	UAnimSingleNodeInstance* AnimSingleNodeInstance = CastChecked<UAnimSingleNodeInstance>(InAnimInstance);
	CurrentAsset = AnimSingleNodeInstance->CurrentAsset;
}

void FAnimSingleNodeInstanceProxy::ClearObjects()
{
	FAnimInstanceProxy::ClearObjects();

	CurrentAsset = nullptr;
}

void FAnimSingleNodeInstanceProxy::SetPreviewCurveOverride(const FName& PoseName, float Value, bool bRemoveIfZero)
{
	float *CurveValPtr = PreviewCurveOverride.Find(PoseName);
	bool bShouldAddToList = bRemoveIfZero == false || FPlatformMath::Abs(Value) > ZERO_ANIMWEIGHT_THRESH;
	if (bShouldAddToList)
	{
		if (CurveValPtr)
		{
			// sum up, in the future we might normalize, but for now this just sums up
			// this won't work well if all of them have full weight - i.e. additive 
			*CurveValPtr = Value;
		}
		else
		{
			PreviewCurveOverride.Add(PoseName, Value);
		}
	}
	// if less than ZERO_ANIMWEIGHT_THRESH
	// no reason to keep them on the list
	else 
	{
		// remove if found
		PreviewCurveOverride.Remove(PoseName);
	}
}

void FAnimSingleNodeInstanceProxy::UpdateMontageWeightForSlot(const FName CurrentSlotNodeName, float InGlobalNodeWeight)
{
	GetSlotWeight(CurrentSlotNodeName, WeightInfo.SlotNodeWeight, WeightInfo.SourceWeight, WeightInfo.TotalNodeWeight);
	UpdateSlotNodeWeight(CurrentSlotNodeName, WeightInfo.SlotNodeWeight, InGlobalNodeWeight);
}

void FAnimSingleNodeInstanceProxy::SetMontagePreviewSlot(FName PreviewSlot)
{
	SingleNode.ActiveMontageSlot = PreviewSlot;
}

void FAnimSingleNodeInstanceProxy::InternalBlendSpaceEvaluatePose(class UBlendSpaceBase* BlendSpace, TArray<FBlendSampleData>& BlendSampleDataCache, FPoseContext& OutContext)
{
	if (BlendSpace->IsValidAdditive())
	{
		FCompactPose& OutPose = OutContext.Pose;
		FBlendedCurve& OutCurve = OutContext.Curve;
		FCompactPose AdditivePose;
		FBlendedCurve AdditiveCurve;
		AdditivePose.SetBoneContainer(&OutPose.GetBoneContainer());
		AdditiveCurve.InitFrom(OutCurve);

#if WITH_EDITORONLY_DATA
		if (BlendSpace->PreviewBasePose)
		{
			BlendSpace->PreviewBasePose->GetBonePose(/*out*/ OutPose, /*out*/OutCurve, FAnimExtractContext(PreviewPoseCurrentTime));
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
			// otherwise, get ref pose
			OutPose.ResetToRefPose();
		}

		BlendSpace->GetAnimationPose(BlendSampleDataCache, AdditivePose, AdditiveCurve);

		enum EAdditiveAnimationType AdditiveType = BlendSpace->bRotationBlendInMeshSpace? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase;

		FAnimationRuntime::AccumulateAdditivePose(OutPose, AdditivePose, OutCurve, AdditiveCurve, 1.f, AdditiveType);
	}
	else
	{
		BlendSpace->GetAnimationPose(BlendSampleDataCache, OutContext.Pose, OutContext.Curve);
	}
}

void FAnimSingleNodeInstanceProxy::SetAnimationAsset(class UAnimationAsset* NewAsset, USkeletalMeshComponent* MeshComponent, bool bIsLooping, float InPlayRate)
{
	bLooping = bIsLooping;
	PlayRate = InPlayRate;
	CurrentTime = 0.f;
	BlendSpaceInput = FVector::ZeroVector;
	BlendSampleData.Reset();
	MarkerTickRecord.Reset();
	UpdateBlendspaceSamples(BlendSpaceInput);

#if WITH_EDITORONLY_DATA
	PreviewPoseCurrentTime = 0.0f;
	PreviewCurveOverride.Reset();
#endif

	
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(NewAsset))
	{
		BlendSpace->InitializeFilter(&BlendFilter);
	}
}

void FAnimSingleNodeInstanceProxy::UpdateBlendspaceSamples(FVector InBlendInput)
{
	if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(CurrentAsset))
	{
		float OutCurrentTime = 0.f;
		FMarkerTickRecord TempMarkerTickRecord;
		BlendSpaceAdvanceImmediate(BlendSpace, InBlendInput, BlendSampleData, BlendFilter, false, 1.f, 0.f, OutCurrentTime, TempMarkerTickRecord);
	}
}

void FAnimSingleNodeInstanceProxy::SetReverse(bool bInReverse)
{
	bReverse = bInReverse;
	if (bInReverse)
	{
		PlayRate = -FMath::Abs(PlayRate);
	}
	else
	{
		PlayRate = FMath::Abs(PlayRate);
	}

// reverse support is a bit tricky for montage
// since we don't have delegate when it reached to the beginning
// for now I comment this out and do not support
// I'd like the buttons to be customizable per asset types -
// 	TTP 233456	ANIM: support different scrub controls per asset type
/*
	FAnimMontageInstance * CurMontageInstance = GetActiveMontageInstance();
	if ( CurMontageInstance )
	{
		if ( bReverse == (CurMontageInstance->PlayRate > 0.f) )
		{
			CurMontageInstance->PlayRate *= -1.f;
		}
	}*/
}

void FAnimSingleNodeInstanceProxy::SetBlendSpaceInput(const FVector& InBlendInput)
{
	BlendSpaceInput = InBlendInput;
}

void FAnimNode_SingleNode::Evaluate_AnyThread(FPoseContext& Output)
{
	const bool bCanProcessAdditiveAnimationsLocal
#if WITH_EDITOR
		= Proxy->bCanProcessAdditiveAnimations;
#else
		= false;
#endif

	if (Proxy->CurrentAsset != NULL && !Proxy->CurrentAsset->HasAnyFlags(RF_BeginDestroyed))
	{
		//@TODO: animrefactor: Seems like more code duplication than we need
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Proxy->CurrentAsset))
		{
			Proxy->InternalBlendSpaceEvaluatePose(BlendSpace, Proxy->BlendSampleData, Output);
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(Proxy->CurrentAsset))
		{
			if (Sequence->IsValidAdditive())
			{
				FAnimExtractContext ExtractionContext(Proxy->CurrentTime, Sequence->bEnableRootMotion);

				if (bCanProcessAdditiveAnimationsLocal)
				{
					Sequence->GetAdditiveBasePose(Output.Pose, Output.Curve, ExtractionContext);
				}
				else
				{
					Output.ResetToRefPose();
				}

				FCompactPose AdditivePose;
				FBlendedCurve AdditiveCurve;
				AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				AdditiveCurve.InitFrom(Output.Curve);
				Sequence->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractionContext);

				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, Sequence->AdditiveAnimType);
				Output.Pose.NormalizeRotations();
			}
			else
			{
				// if SkeletalMesh isn't there, we'll need to use skeleton
				Sequence->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(Proxy->CurrentTime, Sequence->bEnableRootMotion));
			}
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(Proxy->CurrentAsset))
		{
			FAnimExtractContext ExtractionContext(Proxy->CurrentTime, Proxy->ShouldExtractRootMotion());
			const FAnimTrack& AnimTrack = Composite->AnimationTrack;

			// find out if this is additive animation
			if (AnimTrack.IsAdditive())
			{
#if WITH_EDITORONLY_DATA
				if (bCanProcessAdditiveAnimationsLocal && Composite->PreviewBasePose)
				{
					Composite->PreviewBasePose->GetAdditiveBasePose(Output.Pose, Output.Curve, ExtractionContext);
				}
				else
#endif
				{
					// get base pose - for now we only support ref pose as base
					Output.Pose.ResetToRefPose();
				}

				EAdditiveAnimationType AdditiveAnimType = AnimTrack.IsRotationOffsetAdditive()? AAT_RotationOffsetMeshSpace : AAT_LocalSpaceBase;

				FCompactPose AdditivePose;
				FBlendedCurve AdditiveCurve;
				AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				AdditiveCurve.InitFrom(Output.Curve);
				Composite->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractionContext);

				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, AdditiveAnimType);
			}
			else
			{
				//doesn't handle additive yet
				Composite->GetAnimationPose(Output.Pose, Output.Curve, ExtractionContext);
			}
		}
		else if (UAnimMontage* Montage = Cast<UAnimMontage>(Proxy->CurrentAsset))
		{
			// for now only update first slot
			// in the future, add option to see which slot to see
			if (Montage->SlotAnimTracks.Num() > 0)
			{
				FCompactPose LocalSourcePose;
				FBlendedCurve LocalSourceCurve;
				LocalSourcePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
				LocalSourceCurve.InitFrom(Output.Curve);
			
				FAnimTrack const* const AnimTrack = Montage->GetAnimationData(ActiveMontageSlot);
				if (AnimTrack && AnimTrack->IsAdditive())
				{
#if WITH_EDITORONLY_DATA
					// if montage is additive, we need to have base pose for the slot pose evaluate
					if (bCanProcessAdditiveAnimationsLocal && Montage->PreviewBasePose && Montage->SequenceLength > 0.f)
					{
						Montage->PreviewBasePose->GetBonePose(LocalSourcePose, LocalSourceCurve, FAnimExtractContext(Proxy->CurrentTime));
					}
					else
#endif // WITH_EDITORONLY_DATA
					{
						LocalSourcePose.ResetToRefPose();
					}
				}
				else
				{
					LocalSourcePose.ResetToRefPose();
				}

				Proxy->SlotEvaluatePose(ActiveMontageSlot, LocalSourcePose, LocalSourceCurve, Proxy->WeightInfo.SourceWeight, Output.Pose, Output.Curve, Proxy->WeightInfo.SlotNodeWeight, Proxy->WeightInfo.TotalNodeWeight);
			}
		}
		else
		{
			// pose asset is handled by preview instance : pose blend node
			// and you can't drag pose asset to level to create single node instance. 
			Output.ResetToRefPose();
		}

#if WITH_EDITORONLY_DATA
		// have to propagate output curve before pose asset as it can use pose curve data
		Proxy->PropagatePreviewCurve(Output);

		// if it has preview pose asset, we have to handle that after we do all animation
		if (const UPoseAsset* PoseAsset = Proxy->CurrentAsset->PreviewPoseAsset)
		{
			USkeleton* MySkeleton = Proxy->CurrentAsset->GetSkeleton();
			// if skeleton doesn't match it won't work
			if (PoseAsset->GetSkeleton() == MySkeleton)
			{
				const TArray<FSmartName>& PoseNames = PoseAsset->GetPoseNames();

				int32 TotalPoses = PoseNames.Num();
				FAnimExtractContext ExtractContext;
				ExtractContext.PoseCurves.AddZeroed(TotalPoses);

				for (const auto& PoseName : PoseNames)
				{
					if (PoseName.UID != SmartName::MaxUID)
					{
						int32 PoseIndex = PoseNames.Find(PoseName);
						if (PoseIndex != INDEX_NONE)
						{
							ExtractContext.PoseCurves[PoseIndex] = Output.Curve.Get(PoseName.UID);
						}
					}
				}

				if (PoseAsset->IsValidAdditive())
				{
					FCompactPose AdditivePose;
					FBlendedCurve AdditiveCurve;
					AdditivePose.SetBoneContainer(&Output.Pose.GetBoneContainer());
					AdditiveCurve.InitFrom(Output.Curve);
					float Weight = PoseAsset->GetAnimationPose(AdditivePose, AdditiveCurve, ExtractContext);
					FAnimationRuntime::AccumulateAdditivePose(Output.Pose, AdditivePose, Output.Curve, AdditiveCurve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
				}
				else
				{
					FPoseContext LocalCurrentPose(Output);
					FPoseContext LocalSourcePose(Output);

					LocalSourcePose = Output;

					if (PoseAsset->GetAnimationPose(LocalCurrentPose.Pose, LocalCurrentPose.Curve, ExtractContext))
					{
						TArray<float> BoneWeights;
						BoneWeights.AddZeroed(LocalCurrentPose.Pose.GetNumBones());
						// once we get it, we have to blend by weight
						FAnimationRuntime::BlendTwoPosesTogetherPerBone(LocalCurrentPose.Pose, LocalSourcePose.Pose, LocalCurrentPose.Curve, LocalSourcePose.Curve, BoneWeights, Output.Pose, Output.Curve);
					}
				}
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
#if WITH_EDITORONLY_DATA
		// even if you don't have any asset curve, we want to output this curve values
		Proxy->PropagatePreviewCurve(Output);
#endif // WITH_EDITORONLY_DATA
	}
}

void FAnimNode_SingleNode::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	float NewPlayRate = Proxy->PlayRate;
	UAnimSequence* PreviewBasePose = NULL;

	if (Proxy->bPlaying == false)
	{
		// we still have to tick animation when bPlaying is false because 
		NewPlayRate = 0.f;
	}

	if(Proxy->CurrentAsset != NULL)
	{
		FAnimGroupInstance* SyncGroup;
		if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Proxy->CurrentAsset))
		{
			FAnimTickRecord& TickRecord = Proxy->CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			Proxy->MakeBlendSpaceTickRecord(TickRecord, BlendSpace, Proxy->BlendSpaceInput, Proxy->BlendSampleData, Proxy->BlendFilter, Proxy->bLooping, NewPlayRate, 1.f, /*inout*/ Proxy->CurrentTime, Proxy->MarkerTickRecord);
#if WITH_EDITORONLY_DATA
			PreviewBasePose = BlendSpace->PreviewBasePose;
#endif
		}
		else if (UAnimSequence* Sequence = Cast<UAnimSequence>(Proxy->CurrentAsset))
		{
			FAnimTickRecord& TickRecord = Proxy->CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			Proxy->MakeSequenceTickRecord(TickRecord, Sequence, Proxy->bLooping, NewPlayRate, 1.f, /*inout*/ Proxy->CurrentTime, Proxy->MarkerTickRecord);
			// if it's not looping, just set play to be false when reached to end
			if (!Proxy->bLooping)
			{
				const float CombinedPlayRate = NewPlayRate*Sequence->RateScale;
				if ((CombinedPlayRate < 0.f && Proxy->CurrentTime <= 0.f) || (CombinedPlayRate > 0.f && Proxy->CurrentTime >= Sequence->SequenceLength))
				{
					Proxy->SetPlaying(false);
				}
			}
		}
		else if(UAnimComposite* Composite = Cast<UAnimComposite>(Proxy->CurrentAsset))
		{
			FAnimTickRecord& TickRecord = Proxy->CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			Proxy->MakeSequenceTickRecord(TickRecord, Composite, Proxy->bLooping, NewPlayRate, 1.f, /*inout*/ Proxy->CurrentTime, Proxy->MarkerTickRecord);
			// if it's not looping, just set play to be false when reached to end
			if (!Proxy->bLooping)
			{
				const float CombinedPlayRate = NewPlayRate*Composite->RateScale;
				if ((CombinedPlayRate < 0.f && Proxy->CurrentTime <= 0.f) || (CombinedPlayRate > 0.f && Proxy->CurrentTime >= Composite->SequenceLength))
				{
					Proxy->SetPlaying(false);
				}
			}
		}
		else if (UAnimMontage * Montage = Cast<UAnimMontage>(Proxy->CurrentAsset))
		{
			// Full weight , if you don't have slot track, you won't be able to see animation playing
			if ( Montage->SlotAnimTracks.Num() > 0 )
			{
				Proxy->UpdateMontageWeightForSlot(ActiveMontageSlot, 1.f);
			}
			// get the montage position
			// @todo anim: temporarily just choose first slot and show the location
			const FMontageEvaluationState* ActiveMontageEvaluationState = Proxy->GetActiveMontageEvaluationState();
			if (ActiveMontageEvaluationState)
			{
				Proxy->CurrentTime = ActiveMontageEvaluationState->MontagePosition;
			}
			else if (Proxy->bPlaying)
			{
				Proxy->SetPlaying(false);
			}
#if WITH_EDITORONLY_DATA
			PreviewBasePose = Montage->PreviewBasePose;
#endif
		}
		else if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(Proxy->CurrentAsset))
		{
			FAnimTickRecord& TickRecord = Proxy->CreateUninitializedTickRecord(INDEX_NONE, /*out*/ SyncGroup);
			Proxy->MakePoseAssetTickRecord(TickRecord, PoseAsset, 1.f);
		}
	}

#if WITH_EDITORONLY_DATA
	if(PreviewBasePose)
	{
		float MoveDelta = Proxy->GetDeltaSeconds() * NewPlayRate;
		const bool bIsPreviewPoseLooping = true;

		FAnimationRuntime::AdvanceTime(bIsPreviewPoseLooping, MoveDelta, Proxy->PreviewPoseCurrentTime, PreviewBasePose->SequenceLength);
	}
#endif
}
