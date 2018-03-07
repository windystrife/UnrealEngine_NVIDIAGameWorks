// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "AnimationNodes/SGraphNodeSequencePlayer.h"
#include "Widgets/Input/SSlider.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"

/////////////////////////////////////////////////////
// SGraphNodeSequencePlayer

void SGraphNodeSequencePlayer::Construct(const FArguments& InArgs, UAnimGraphNode_Base* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();

	SAnimationGraphNode::Construct(SAnimationGraphNode::FArguments(), InNode);
}

void SGraphNodeSequencePlayer::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
}

FText SGraphNodeSequencePlayer::GetPositionTooltip() const
{
	float Position;
	float Length;
	int32 FrameCount;
	if (GetSequencePositionInfo(/*out*/ Position, /*out*/ Length, /*out*/ FrameCount))
	{
		const int32 Minutes = FMath::TruncToInt(Position/60.0f);
		const int32 Seconds = FMath::TruncToInt(Position) % 60;
		const int32 Hundredths = FMath::TruncToInt(FMath::Fractional(Position)*100);

		FString MinuteStr;
		if (Minutes > 0)
		{
			MinuteStr = FString::Printf(TEXT("%dm"), Minutes);
		}

		const FString SecondStr = FString::Printf(TEXT("%02ds"), Seconds);

		const FString HundredthsStr = FString::Printf(TEXT(".%02d"), Hundredths);

		const int32 CurrentFrame = FMath::TruncToInt((Position / Length) * FrameCount);

		const FString FramesStr = FString::Printf(TEXT("Frame %d"), CurrentFrame);

		return FText::FromString(FString::Printf(TEXT("%s (%s%s%s)"), *FramesStr, *MinuteStr, *SecondStr, *HundredthsStr));
	}

	return NSLOCTEXT("SGraphNodeSequencePlayer", "PositionToolTip_Default", "Position");
}

void SGraphNodeSequencePlayer::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}

void SGraphNodeSequencePlayer::CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox)
{
	FLinearColor Yellow(0.9f, 0.9f, 0.125f);

	MainBox->AddSlot()
	.AutoHeight()
	.VAlign( VAlign_Fill )
	.Padding(FMargin(0, 4, 0, 0))
	[
		SNew(SSlider)
		.ToolTipText(this, &SGraphNodeSequencePlayer::GetPositionTooltip)
		.Visibility(this, &SGraphNodeSequencePlayer::GetSliderVisibility)
		.Value(this, &SGraphNodeSequencePlayer::GetSequencePositionRatio)
		.OnValueChanged(this, &SGraphNodeSequencePlayer::SetSequencePositionRatio)
		.Locked(false)
		.SliderHandleColor(Yellow)
	];
}

FAnimNode_SequencePlayer* SGraphNodeSequencePlayer::GetSequencePlayer() const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(GraphNode))
	{
		if (UObject* ActiveObject = Blueprint->GetObjectBeingDebugged())
		{
			if (UAnimGraphNode_SequencePlayer* VisualSequencePlayer = Cast<UAnimGraphNode_SequencePlayer>(GraphNode))
			{
				if (UAnimBlueprintGeneratedClass* Class = Cast<UAnimBlueprintGeneratedClass>((UObject*)ActiveObject->GetClass()))
				{
					return Class->GetPropertyInstance<FAnimNode_SequencePlayer>(ActiveObject, VisualSequencePlayer);
				}
			}
		}
	}
	return NULL;
}

EVisibility SGraphNodeSequencePlayer::GetSliderVisibility() const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(GraphNode))
	{
		if (UProperty* Property = FKismetDebugUtilities::FindClassPropertyForNode(Blueprint, GraphNode))
		{
			if (UObject* ActiveObject = Blueprint->GetObjectBeingDebugged())
			{
				return EVisibility::Visible;
			}
		}
	}

	return EVisibility::Collapsed;
}

bool SGraphNodeSequencePlayer::GetSequencePositionInfo(float& Out_Position, float& Out_Length, int32& Out_FrameCount) const
{
	if(FAnimNode_SequencePlayer* SequencePlayer = GetSequencePlayer())
	{
		if (UAnimSequenceBase* BoundSequence = SequencePlayer->Sequence)
		{
			Out_Position = SequencePlayer->GetAccumulatedTime();
			Out_Length = BoundSequence->SequenceLength;
			Out_FrameCount = BoundSequence->GetNumberOfFrames();

			return true;
		}
	}

	Out_Position = 0.0f;
	Out_Length = 0.0f;
	Out_FrameCount = 0;
	return false;
}

float SGraphNodeSequencePlayer::GetSequencePositionRatio() const
{
	float Position;
	float Length;
	int32 FrameCount;
	if (GetSequencePositionInfo(/*out*/ Position, /*out*/ Length, /*out*/ FrameCount))
	{
		return Position / Length;
	}
	return 0.0f;
}

void SGraphNodeSequencePlayer::SetSequencePositionRatio(float NewRatio)
{
	if(FAnimNode_SequencePlayer* SequencePlayer = GetSequencePlayer())
	{
		if (SequencePlayer->Sequence != NULL)
		{
			const float NewTime = NewRatio * SequencePlayer->Sequence->SequenceLength;
			SequencePlayer->SetAccumulatedTime(NewTime);
		}
	}
}
