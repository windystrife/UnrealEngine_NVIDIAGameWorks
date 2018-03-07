// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EditModes/PoseDriverEditMode.h"
#include "SceneManagement.h"
#include "AnimNodes/AnimNode_PoseDriver.h"
#include "AnimGraphNode_PoseDriver.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

void FPoseDriverEditMode::EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode)
{
	RuntimeNode = static_cast<FAnimNode_PoseDriver*>(InRuntimeNode);
	GraphNode = CastChecked<UAnimGraphNode_PoseDriver>(InEditorNode);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FPoseDriverEditMode::ExitMode()
{
	RuntimeNode = nullptr;
	GraphNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

static FLinearColor GetColorFromWeight(float InWeight)
{
	return FMath::Lerp(FLinearColor::White, FLinearColor::Red, InWeight);
}

/** Hit proxy for selecting targets */
struct HPDTargetHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()

	int32 TargetIndex;

	HPDTargetHitProxy(int32 InTargetIndex)
		: HHitProxy(HPP_World)
		, TargetIndex(InTargetIndex)
	{
	}

	// HHitProxy interface
	virtual EMouseCursor::Type GetMouseCursor() override { return EMouseCursor::Crosshairs; }
	// End of HHitProxy interface
};

IMPLEMENT_HIT_PROXY(HPDTargetHitProxy, HHitProxy)


void FPoseDriverEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	// Tell graph node last comp we were used on. A bit ugly, but no easy way to get from details customization to editor instance
	GraphNode->LastPreviewComponent = SkelComp;

	static const float DrawLineWidth = 0.1f;
	static const float DrawAxisLength = 20.f;
	static const float DrawPosSize = 2.f;


	// Iterate over each bone in the 'source bones' array
	for (int32 SourceIdx=0; SourceIdx< RuntimeNode->SourceBones.Num(); SourceIdx++)
	{
		const FBoneReference& SourceBoneRef = RuntimeNode->SourceBones[SourceIdx];

		// Get mesh bone index
		int32 BoneIndex = SkelComp->GetBoneIndex(SourceBoneRef.BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			// Get transform of driven bone, used as basis for drawing
			FTransform BoneWorldTM = SkelComp->GetBoneTransform(BoneIndex);
			FVector BonePos = BoneWorldTM.GetLocation();

			// Transform that we are evaluating pose in
			FTransform EvalSpaceTM = SkelComp->GetComponentToWorld();

			// If specifying space to eval in, get that space
			int32 EvalSpaceBoneIndex = SkelComp->GetBoneIndex(RuntimeNode->EvalSpaceBone.BoneName);
			FName ParentBoneName = SkelComp->GetParentBone(SourceBoneRef.BoneName);
			int32 ParentBoneIndex = SkelComp->GetBoneIndex(ParentBoneName);
			if (EvalSpaceBoneIndex != INDEX_NONE)
			{
				EvalSpaceTM = SkelComp->GetBoneTransform(EvalSpaceBoneIndex);
			}
			// Otherwise, just use parent bone
			else if (ParentBoneIndex != INDEX_NONE)
			{
				EvalSpaceTM = SkelComp->GetBoneTransform(ParentBoneIndex);
			}

			// Get source bone TM from last frame
			if (RuntimeNode->SourceBoneTMs.IsValidIndex(SourceIdx))
			{
				FTransform SourceBoneTM = RuntimeNode->SourceBoneTMs[SourceIdx];

				// Rotation drawing
				if (RuntimeNode->DriveSource == EPoseDriverSource::Rotation)
				{
					FVector LocalVec = SourceBoneTM.TransformVectorNoScale(RuntimeNode->RBFParams.GetTwistAxisVector());
					FVector WorldVec = EvalSpaceTM.TransformVectorNoScale(LocalVec);
					PDI->DrawLine(BonePos, BonePos + (WorldVec*DrawAxisLength), FLinearColor::Green, SDPG_Foreground, DrawLineWidth);
				}
				// Translation drawing
				else if (RuntimeNode->DriveSource == EPoseDriverSource::Translation)
				{
					FVector LocalPos = SourceBoneTM.GetTranslation();
					FVector WorldPos = EvalSpaceTM.TransformPosition(LocalPos);
					DrawWireDiamond(PDI, FTranslationMatrix(WorldPos), DrawPosSize, FLinearColor::Green, SDPG_Foreground);
				}

				// Build array of weight for every target
				TArray<float> PerTargetWeights;
				PerTargetWeights.AddZeroed(RuntimeNode->PoseTargets.Num());
				for (const FRBFOutputWeight& Weight : RuntimeNode->OutputWeights)
				{
					PerTargetWeights[Weight.TargetIndex] = Weight.TargetWeight;
				}

				// Draw every target for this bone
				for (int32 TargetIdx = 0; TargetIdx < RuntimeNode->PoseTargets.Num(); TargetIdx++)
				{
					// Check we have a target transform for this bone
					const FPoseDriverTarget& PoseTarget = RuntimeNode->PoseTargets[TargetIdx];
					if (PoseTarget.BoneTransforms.IsValidIndex(SourceIdx))
					{
						const FPoseDriverTransform& TargetTM = PoseTarget.BoneTransforms[SourceIdx];

						bool bSelected = (GraphNode->SelectedTargetIndex == TargetIdx);
						float AxisLength = bSelected ? (DrawAxisLength * 1.5f) : DrawAxisLength;
						float LineWidth = bSelected ? (DrawLineWidth * 3.f) : DrawLineWidth;
						float PosSize = bSelected ? (DrawPosSize * 1.5f) : DrawPosSize;

						PDI->SetHitProxy(new HPDTargetHitProxy(TargetIdx));

						// Rotation drawing
						if (RuntimeNode->DriveSource == EPoseDriverSource::Rotation)
						{
							FVector LocalVec = TargetTM.TargetRotation.RotateVector(RuntimeNode->RBFParams.GetTwistAxisVector());
							FVector WorldVec = EvalSpaceTM.TransformVectorNoScale(LocalVec);
							PDI->DrawLine(BonePos, BonePos + (WorldVec*AxisLength), GetColorFromWeight(PerTargetWeights[TargetIdx]), SDPG_Foreground, LineWidth);
						}
						// Translation drawing
						else if (RuntimeNode->DriveSource == EPoseDriverSource::Translation)
						{
							FVector LocalPos = TargetTM.TargetTranslation;
							FVector WorldPos = EvalSpaceTM.TransformPosition(LocalPos);
							DrawWireDiamond(PDI, FTranslationMatrix(WorldPos), PosSize, GetColorFromWeight(PerTargetWeights[TargetIdx]), SDPG_Foreground, LineWidth);
						}

						PDI->SetHitProxy(nullptr);
					}
				}
			}
		}
	}
}

bool FPoseDriverEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	bool bResult = FAnimNodeEditMode::HandleClick(InViewportClient, HitProxy, Click);

	if (HitProxy != nullptr && HitProxy->IsA(HPDTargetHitProxy::StaticGetType()))
	{
		HPDTargetHitProxy* TargetHitProxy = static_cast<HPDTargetHitProxy*>(HitProxy);
		GraphNode->SelectedTargetIndex = TargetHitProxy->TargetIndex;
		GraphNode->SelectedTargetChangeDelegate.Broadcast();
		bResult = true;
	}

	return bResult;
}

#undef LOCTEXT_NAMESPACE
