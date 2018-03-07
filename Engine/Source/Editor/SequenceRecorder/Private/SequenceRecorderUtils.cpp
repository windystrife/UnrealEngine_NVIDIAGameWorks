// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderUtils.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "AnimationRecorder.h"

namespace SequenceRecorderUtils
{

AActor* GetAttachment(AActor* InActor, FName& SocketName, FName& ComponentName)
{
	AActor* AttachedActor = nullptr;
	if(InActor)
	{
		USceneComponent* RootComponent = InActor->GetRootComponent();
		if(RootComponent && RootComponent->GetAttachParent() != nullptr)
		{
			AttachedActor = RootComponent->GetAttachParent()->GetOwner();
			SocketName = RootComponent->GetAttachSocketName();
			ComponentName = RootComponent->GetAttachParent()->GetFName();
		}
	}

	return AttachedActor;
}

bool RecordSingleNodeInstanceToAnimation(USkeletalMeshComponent* PreviewComponent, UAnimSequence* NewAsset)
{
	UAnimSingleNodeInstance* SingleNodeInstance = (PreviewComponent) ? Cast<UAnimSingleNodeInstance>(PreviewComponent->GetAnimInstance()) : nullptr;
	if (SingleNodeInstance && NewAsset)
	{
		auto RecordMesh = [](USkeletalMeshComponent* InPreviewComponent, UAnimSingleNodeInstance* InSingleNodeInstance, FAnimRecorderInstance& InAnimRecorder, float CurrentTime, float Interval)
		{
			// set time
			InSingleNodeInstance->SetPosition(CurrentTime, false);
			// tick component
			InPreviewComponent->TickComponent(0.f, ELevelTick::LEVELTICK_All, nullptr);
			if (CurrentTime == 0.f)
			{
				// first frame records the current pose, so I'll have to call BeginRecording when CurrentTime == 0.f;
				InAnimRecorder.BeginRecording();
			}
			else
			{
				InAnimRecorder.Update(Interval);
			}
		};

		FAnimRecorderInstance AnimRecorder;
		FAnimationRecordingSettings Setting;
		AnimRecorder.Init(PreviewComponent, NewAsset, Setting);
		float Length = SingleNodeInstance->GetLength();
		const float DefaultSampleRate = (Setting.SampleRate > 0.f) ? Setting.SampleRate : DEFAULT_SAMPLERATE;
		const float Interval = 1.f / DefaultSampleRate;
		float Time = 0.f;
		for (; Time < Length; Time += Interval)
		{
			RecordMesh(PreviewComponent, SingleNodeInstance, AnimRecorder, Time, Interval);
		}

		// get the last time
		const float Remainder = (Length - (Time - Interval));
		if (Remainder >= 0.f)
		{
			RecordMesh(PreviewComponent, SingleNodeInstance, AnimRecorder, Length, Remainder);
		}

		AnimRecorder.FinishRecording(true);
		return true;
	}

	return false;
}

}
