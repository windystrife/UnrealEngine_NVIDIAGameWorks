// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "SAnimationScrubPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimBlueprint.h"
#include "AnimPreviewInstance.h"
#include "SScrubControlPanel.h"
#include "ScopedTransaction.h"
#include "Animation/BlendSpaceBase.h"
#include "AnimationEditorPreviewScene.h"

#define LOCTEXT_NAMESPACE "AnimationScrubPanel"

void SAnimationScrubPanel::Construct( const SAnimationScrubPanel::FArguments& InArgs, const TSharedRef<IPersonaPreviewScene>& InPreviewScene)
{
	bSliderBeingDragged = false;
	LockedSequence = InArgs._LockedSequence;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;

	PreviewScenePtr = InPreviewScene;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		.AddMetaData<FTagMetaData>(TEXT("AnimScrub.Scrub"))
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Fill) 
		.VAlign(VAlign_Center)
		.FillWidth(1)
		.Padding(0.0f)
		[
			SAssignNew(ScrubControlPanel, SScrubControlPanel)
			.IsEnabled(true)//this, &SAnimationScrubPanel::DoesSyncViewport)
			.Value(this, &SAnimationScrubPanel::GetScrubValue)
			.NumOfKeys(this, &SAnimationScrubPanel::GetNumOfFrames)
			.SequenceLength(this, &SAnimationScrubPanel::GetSequenceLength)
			.DisplayDrag(this, &SAnimationScrubPanel::GetDisplayDrag)
			.OnValueChanged(this, &SAnimationScrubPanel::OnValueChanged)
			.OnBeginSliderMovement(this, &SAnimationScrubPanel::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &SAnimationScrubPanel::OnEndSliderMovement)
			.OnClickedForwardPlay(this, &SAnimationScrubPanel::OnClick_Forward)
			.OnClickedForwardStep(this, &SAnimationScrubPanel::OnClick_Forward_Step)
			.OnClickedForwardEnd(this, &SAnimationScrubPanel::OnClick_Forward_End)
			.OnClickedBackwardPlay(this, &SAnimationScrubPanel::OnClick_Backward)
			.OnClickedBackwardStep(this, &SAnimationScrubPanel::OnClick_Backward_Step)
			.OnClickedBackwardEnd(this, &SAnimationScrubPanel::OnClick_Backward_End)
			.OnClickedToggleLoop(this, &SAnimationScrubPanel::OnClick_ToggleLoop)
			.OnClickedRecord(this, &SAnimationScrubPanel::OnClick_Record)
			.OnGetLooping(this, &SAnimationScrubPanel::IsLoopStatusOn)
			.OnGetPlaybackMode(this, &SAnimationScrubPanel::GetPlaybackMode)
			.OnGetRecording(this, &SAnimationScrubPanel::IsRecording)
			.ViewInputMin(InArgs._ViewInputMin)
			.ViewInputMax(InArgs._ViewInputMax)
			.OnSetInputViewRange(InArgs._OnSetInputViewRange)
			.OnCropAnimSequence( this, &SAnimationScrubPanel::OnCropAnimSequence )
			.OnAddAnimSequence( this, &SAnimationScrubPanel::OnInsertAnimSequence )
			.OnAppendAnimSequence(this, &SAnimationScrubPanel::OnAppendAnimSequence )
			.OnReZeroAnimSequence( this, &SAnimationScrubPanel::OnReZeroAnimSequence )
			.bAllowZoom(InArgs._bAllowZoom)
			.IsRealtimeStreamingMode(this, &SAnimationScrubPanel::IsRealtimeStreamingMode)
		]
	];
}

FReply SAnimationScrubPanel::OnClick_Forward_Step()
{
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		bool bShouldStepCloth = FMath::Abs(PreviewInstance->GetLength() - PreviewInstance->GetCurrentTime()) > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepForward();

		if(SMC && bShouldStepCloth)
		{
			SMC->bPerformSingleClothingTick = true;
		}
	}
	else if (SMC)
	{
		//@TODO: Should we hardcode 30 Hz here?
		{
			const float TargetFramerate = 30.0f;

			// Advance a single frame, leaving it paused afterwards
			SMC->GlobalAnimRateScale = 1.0f;
			SMC->TickAnimation(1.0f / TargetFramerate, false);
			SMC->GlobalAnimRateScale = 0.0f;
		}
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Forward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(PreviewInstance->GetLength(), false);
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward_Step()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();
	if (PreviewInstance)
	{
		bool bShouldStepCloth = PreviewInstance->GetCurrentTime() > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepBackward();

		if(SMC && bShouldStepCloth)
		{
			SMC->bPerformSingleClothingTick = true;
		}
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(0.f, false);
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent();

	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying) 
		{
			PreviewInstance->SetPlaying(false);
			
			if(SMC && SMC->bPauseClothingSimulationWithAnim)
			{
				SMC->SuspendClothingSimulation();
			}
		}
		// if not playing, play forward
		else 
		{
			//if we're at the end of the animation, jump back to the beginning before playing
			if ( GetScrubValue() >= GetSequenceLength() )
			{
				PreviewInstance->SetPosition(0.0f, false);
			}

			PreviewInstance->SetReverse(false);
			PreviewInstance->SetPlaying(true);

			if(SMC && SMC->bPauseClothingSimulationWithAnim)
			{
				SMC->ResumeClothingSimulation();
			}
		}
	}
	else if(SMC)
	{
		SMC->GlobalAnimRateScale = (SMC->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
	}

	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Backward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if currently playing forward, just simply turn on reverse
		if (!bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(true);
		}
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);
		}
		else
		{
			//if we're at the beginning of the animation, jump back to the end before playing
			if ( GetScrubValue() <= 0.0f )
			{
				PreviewInstance->SetPosition(GetSequenceLength(), false);
			}

			PreviewInstance->SetPlaying(true);
			PreviewInstance->SetReverse(true);
		}
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_ToggleLoop()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsLooping = PreviewInstance->IsLooping();
		PreviewInstance->SetLooping(!bIsLooping);
	}
	return FReply::Handled();
}

FReply SAnimationScrubPanel::OnClick_Record()
{
	StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->RecordAnimation();

	return FReply::Handled();
}

bool SAnimationScrubPanel::IsLoopStatusOn() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return (PreviewInstance && PreviewInstance->IsLooping());
}

EPlaybackMode::Type SAnimationScrubPanel::GetPlaybackMode() const
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			return PreviewInstance->IsReverse() ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
		return EPlaybackMode::Stopped;
	}
	else if (UDebugSkelMeshComponent* SMC = GetPreviewScene()->GetPreviewMeshComponent())
	{
		return (SMC->GlobalAnimRateScale > 0.0f) ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
	}
	
	return EPlaybackMode::Stopped;
}

bool SAnimationScrubPanel::IsRecording() const
{
	return StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->IsRecording();
}

bool SAnimationScrubPanel::IsRealtimeStreamingMode() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return ( ! (PreviewInstance && PreviewInstance->GetCurrentAsset()) );
}

void SAnimationScrubPanel::OnValueChanged(float NewValue)
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPosition(NewValue);
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			DebugData->SetSnapshotIndexByTime(Instance, NewValue);
		}
	}
}

// make sure viewport is freshes
void SAnimationScrubPanel::OnBeginSliderMovement()
{
	bSliderBeingDragged = true;

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		PreviewInstance->SetPlaying(false);
	}
}

void SAnimationScrubPanel::OnEndSliderMovement(float NewValue)
{
	bSliderBeingDragged = false;
}

uint32 SAnimationScrubPanel::GetNumOfFrames() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		float Length = PreviewInstance->GetLength();
		// if anim sequence, use correct num frames
		int32 NumFrames = (int32) (Length/0.0333f); 
		if (PreviewInstance->GetCurrentAsset())
		{
			if (PreviewInstance->GetCurrentAsset()->IsA(UAnimSequenceBase::StaticClass()))
			{
				NumFrames = CastChecked<UAnimSequenceBase>(PreviewInstance->GetCurrentAsset())->GetNumberOfFrames();
			}
			else if(PreviewInstance->GetCurrentAsset()->IsA(UBlendSpaceBase::StaticClass()))
			{
				// Blendspaces dont display frame notches, so just return 0 here
				NumFrames = 0;
			}
		}
		return NumFrames;
	}
	else if (LockedSequence)
	{
		return LockedSequence->GetNumberOfFrames();
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return DebugData->GetSnapshotLengthInFrames();
		}
	}

	return 1;
}

float SAnimationScrubPanel::GetSequenceLength() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		return PreviewInstance->GetLength();
	}
	else if (LockedSequence)
	{
		return LockedSequence->SequenceLength;
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return Instance->LifeTimer;
		}
	}

	return 0.f;
}

bool SAnimationScrubPanel::DoesSyncViewport() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();

	return (( LockedSequence==NULL && PreviewInstance ) || ( LockedSequence && PreviewInstance && PreviewInstance->GetCurrentAsset() == LockedSequence ));
}

void SAnimationScrubPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bSliderBeingDragged)
	{
		GetPreviewScene()->InvalidateViews();
	}
}

class UAnimSingleNodeInstance* SAnimationScrubPanel::GetPreviewInstance() const
{
	UDebugSkelMeshComponent* PreviewMeshComponent = GetPreviewScene()->GetPreviewMeshComponent();
	return PreviewMeshComponent && PreviewMeshComponent->IsPreviewOn()? PreviewMeshComponent->PreviewInstance : NULL;
}

float SAnimationScrubPanel::GetScrubValue() const
{
	if (DoesSyncViewport())
	{
		UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
		if (PreviewInstance)
		{
			return PreviewInstance->GetCurrentTime(); 
		}
	}
	else
	{
		UAnimInstance* Instance;
		FAnimBlueprintDebugData* DebugData;
		if (GetAnimBlueprintDebugData(/*out*/ Instance, /*out*/ DebugData))
		{
			return Instance->CurrentLifeTimerScrubPosition;
		}
	}

	return 0.f;
}

void SAnimationScrubPanel::ReplaceLockedSequence(class UAnimSequenceBase* NewLockedSequence)
{
	LockedSequence = NewLockedSequence;
}

UAnimInstance* SAnimationScrubPanel::GetAnimInstanceWithBlueprint() const
{
	if (UDebugSkelMeshComponent* DebugComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		UAnimInstance* Instance = DebugComponent->GetAnimInstance();

		if ((Instance != NULL) && (Instance->GetClass()->ClassGeneratedBy != NULL))
		{
			return Instance;
		}
	}

	return NULL;
}

bool SAnimationScrubPanel::GetAnimBlueprintDebugData(UAnimInstance*& Instance, FAnimBlueprintDebugData*& DebugInfo) const
{
	Instance = GetAnimInstanceWithBlueprint();

	if (Instance != NULL)
	{
		// Avoid updating the instance if we're replaying the past
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Cast<UAnimBlueprintGeneratedClass>(Instance->GetClass()))
		{
			if (UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(AnimBlueprintClass->ClassGeneratedBy))
			{
				if (Blueprint->GetObjectBeingDebugged() == Instance)
				{
					DebugInfo = &(AnimBlueprintClass->GetAnimBlueprintDebugData());
					return true;
				}
			}
		}
	}

	return false;
}

void SAnimationScrubPanel::OnCropAnimSequence( bool bFromStart, float CurrentTime )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		float Length = PreviewInstance->GetLength();
		if (PreviewInstance->GetCurrentAsset())
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset() );
			if( AnimSequence )
			{
				const FScopedTransaction Transaction( LOCTEXT("CropAnimSequence", "Crop Animation Sequence") );

				//Call modify to restore slider position
				PreviewInstance->Modify();

				//Call modify to restore anim sequence current state
				AnimSequence->Modify();

				// Crop the raw anim data.
				AnimSequence->CropRawAnimData( CurrentTime, bFromStart );

				//Resetting slider position to the first frame
				PreviewInstance->SetPosition( 0.0f, false );

				OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
			}
		}
	}
}

void SAnimationScrubPanel::OnAppendAnimSequence( bool bFromStart, int32 NumOfFrames )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Crop the raw anim data.
			int32 StartFrame = (bFromStart)? 0 : AnimSequence->NumFrames - 1;
			int32 EndFrame = StartFrame + NumOfFrames;
			int32 CopyFrame = StartFrame;
			AnimSequence->InsertFramesToRawAnimData(StartFrame, EndFrame, CopyFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
		}
	}
}

void SAnimationScrubPanel::OnInsertAnimSequence( bool bBefore, int32 CurrentFrame )
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewInstance->GetCurrentAsset());
		if(AnimSequence)
		{
			const FScopedTransaction Transaction(LOCTEXT("InsertAnimSequence", "Insert Animation Sequence"));

			//Call modify to restore slider position
			PreviewInstance->Modify();

			//Call modify to restore anim sequence current state
			AnimSequence->Modify();

			// Crop the raw anim data.
			int32 StartFrame = (bBefore)? CurrentFrame : CurrentFrame + 1;
			int32 EndFrame = StartFrame + 1;
			AnimSequence->InsertFramesToRawAnimData(StartFrame, EndFrame, CurrentFrame);

			OnSetInputViewRange.ExecuteIfBound(0, AnimSequence->SequenceLength);
		}
	}
}

void SAnimationScrubPanel::OnReZeroAnimSequence(int32 FrameIndex)
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if(PreviewInstance)
	{
		UDebugSkelMeshComponent* PreviewSkelComp = GetPreviewScene()->GetPreviewMeshComponent();

		if (PreviewInstance->GetCurrentAsset() && PreviewSkelComp )
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>( PreviewInstance->GetCurrentAsset() );
			if( AnimSequence )
			{
				const FScopedTransaction Transaction( LOCTEXT("ReZeroAnimation", "ReZero Animation Sequence") );

				//Call modify to restore anim sequence current state
				AnimSequence->Modify();

				// As above, animations don't have any idea of hierarchy, so we don't know for sure if track 0 is the root bone's track.
				FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationTrack(0);

				// Find vector that would translate current root bone location onto origin.
				FVector FrameTransform = FVector::ZeroVector;
				if (FrameIndex == INDEX_NONE)
				{
					// Use current transform
					FrameTransform = PreviewSkelComp->GetComponentSpaceTransforms()[0].GetLocation();
				}
				else if(RawTrack.PosKeys.IsValidIndex(FrameIndex))
				{
					// Use transform at frame
					FrameTransform = RawTrack.PosKeys[FrameIndex];
				}

				FVector ApplyTranslation = -1.f * FrameTransform;

				// Convert into world space
				FVector WorldApplyTranslation = PreviewSkelComp->GetComponentTransform().TransformVector(ApplyTranslation);
				ApplyTranslation = PreviewSkelComp->GetComponentTransform().InverseTransformVector(WorldApplyTranslation);

				for(int32 i=0; i<RawTrack.PosKeys.Num(); i++)
				{
					RawTrack.PosKeys[i] += ApplyTranslation;
				}

				// Handle Raw Data changing
				AnimSequence->MarkRawDataAsModified();
				AnimSequence->OnRawDataChanged();

				AnimSequence->MarkPackageDirty();
			}
		}
	}
}

bool SAnimationScrubPanel::GetDisplayDrag() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance && PreviewInstance->GetCurrentAsset())
	{
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
