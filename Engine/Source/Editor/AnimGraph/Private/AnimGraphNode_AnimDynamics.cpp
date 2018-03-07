// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_AnimDynamics.h"
#include "EngineGlobals.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Input/SButton.h"

// Details includes
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "AnimationCustomVersion.h"
#include "Animation/AnimInstance.h"

#define LOCTEXT_NAMESPACE "AnimDynamicsNode"

FText UAnimGraphNode_AnimDynamics::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Anim Dynamics");
}

void UAnimGraphNode_AnimDynamics::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent * PreviewSkelMeshComp) const
{
	if(LastPreviewComponent != PreviewSkelMeshComp)
	{
		LastPreviewComponent = PreviewSkelMeshComp;
	}

	// If we want to preview the live node, process it here
	if(bPreviewLive)
	{
		FAnimNode_AnimDynamics* ActivePreviewNode = GetPreviewDynamicsNode();

		if(ActivePreviewNode)
		{
			for(int32 BodyIndex = 0 ; BodyIndex < ActivePreviewNode->GetNumBodies() ; ++BodyIndex)
			{
				const FAnimPhysRigidBody& Body = ActivePreviewNode->GetPhysBody(BodyIndex);
				FTransform BodyTransform(Body.Pose.Orientation, Body.Pose.Position);

				for(const FAnimPhysShape& Shape : Body.Shapes)
				{
					for(const FIntVector& Triangle : Shape.Triangles)
					{
						for(int32 Idx = 0 ; Idx < 3 ; ++Idx)
						{
							int32 Next = (Idx + 1) % 3;

							FVector FirstVertPosition = BodyTransform.TransformPosition(Shape.Vertices[Triangle[Idx]]);
							FVector SecondVertPosition = BodyTransform.TransformPosition(Shape.Vertices[Triangle[Next]]);

							PDI->DrawLine(FirstVertPosition, SecondVertPosition, AnimDynamicsNodeConstants::ActiveBodyDrawColor, SDPG_Foreground, AnimDynamicsNodeConstants::ShapeLineWidth);
						}
					}
				}
				
				const int32 BoneIndex = PreviewSkelMeshComp->GetBoneIndex(ActivePreviewNode->BoundBone.BoneName);
				if(BoneIndex != INDEX_NONE)
				{
					FTransform BodyJointTransform = PreviewSkelMeshComp->GetBoneTransform(BoneIndex);
					FTransform ShapeOriginalTransform = BodyJointTransform;

					// Draw pin location
					FVector LocalPinOffset = BodyTransform.Rotator().RotateVector(Node.GetBodyLocalJointOffset(BodyIndex));
					PDI->DrawLine(Body.Pose.Position, Body.Pose.Position + LocalPinOffset, FLinearColor::Green, SDPG_Foreground, AnimDynamicsNodeConstants::ShapeLineWidth);

					// Draw basis at body location
					FVector Origin = BodyTransform.GetTranslation();
					FVector XAxis(1.0f, 0.0f, 0.0f);
					FVector YAxis(0.0f, 1.0f, 0.0f);
					FVector ZAxis(0.0f, 0.0f, 1.0f);

					XAxis = BodyTransform.TransformVector(XAxis);
					YAxis = BodyTransform.TransformVector(YAxis);
					ZAxis = BodyTransform.TransformVector(ZAxis);

					PDI->DrawLine(Origin, Origin + XAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Red, SDPG_Foreground, AnimDynamicsNodeConstants::TransformLineWidth);
					PDI->DrawLine(Origin, Origin + YAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Green, SDPG_Foreground, AnimDynamicsNodeConstants::TransformLineWidth);
					PDI->DrawLine(Origin, Origin + ZAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Blue, SDPG_Foreground, AnimDynamicsNodeConstants::TransformLineWidth);

					if(bShowLinearLimits)
					{
						DrawLinearLimits(PDI, BodyJointTransform, *ActivePreviewNode);
					}

					if(bShowAngularLimits)
					{
						FTransform AngularLimitsTM(BodyJointTransform.GetRotation(), BodyTransform.GetTranslation() + LocalPinOffset);
						DrawAngularLimits(PDI, AngularLimitsTM, *ActivePreviewNode);
					}

					if(bShowCollisionSpheres && Body.CollisionType != AnimPhysCollisionType::CoM)
					{
						// Draw collision sphere
						DrawWireSphere(PDI, BodyTransform, FLinearColor(FColor::Cyan), Body.SphereCollisionRadius, 24, SDPG_Foreground, 0.2f);
					}
				}
			}

			// Only draw the planar limit once
			if(bShowPlanarLimit && ActivePreviewNode->PlanarLimits.Num() > 0)
			{
				for(FAnimPhysPlanarLimit& PlanarLimit : ActivePreviewNode->PlanarLimits)
				{
					FTransform LimitPlaneTransform = PlanarLimit.PlaneTransform;
					const int32 LimitDrivingBoneIdx = PreviewSkelMeshComp->GetBoneIndex(PlanarLimit.DrivingBone.BoneName);

					if(LimitDrivingBoneIdx != INDEX_NONE)
					{
						LimitPlaneTransform *= PreviewSkelMeshComp->GetComponentSpaceTransforms()[LimitDrivingBoneIdx];
					}

					DrawPlane10x10(PDI, LimitPlaneTransform.ToMatrixNoScale(), 200.0f, FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f), GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
					DrawDirectionalArrow(PDI, FRotationMatrix(FRotator(90.0f, 0.0f, 0.0f)) * LimitPlaneTransform.ToMatrixNoScale(), FLinearColor::Blue, 50.0f, 20.0f, SDPG_Foreground, 0.5f);
				}
			}

			if(bShowSphericalLimit && ActivePreviewNode->SphericalLimits.Num() > 0)
			{
				for(FAnimPhysSphericalLimit& SphericalLimit : ActivePreviewNode->SphericalLimits)
				{
					FTransform SphereTransform = FTransform::Identity;
					SphereTransform.SetTranslation(SphericalLimit.SphereLocalOffset);
					
					const int32 DrivingBoneIdx = PreviewSkelMeshComp->GetBoneIndex(SphericalLimit.DrivingBone.BoneName);

					if(DrivingBoneIdx != INDEX_NONE)
					{
						SphereTransform *= PreviewSkelMeshComp->GetComponentSpaceTransforms()[DrivingBoneIdx];
					}

					DrawSphere(PDI, SphereTransform.GetLocation(), FRotator::ZeroRotator, FVector(SphericalLimit.LimitRadius), 24, 6, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
					DrawWireSphere(PDI, SphereTransform, FLinearColor::Black, SphericalLimit.LimitRadius, 24, SDPG_World);
				}
			}
		}
	}
	else
	{
		const int32 BoneIndex = PreviewSkelMeshComp->GetBoneIndex(Node.BoundBone.BoneName);

		if(BoneIndex != INDEX_NONE)
		{
			// World space transform
			const FTransform BoneTransform = PreviewSkelMeshComp->GetBoneTransform(BoneIndex);
			FTransform ShapeTransform = BoneTransform;
			ShapeTransform.SetTranslation(ShapeTransform.GetTranslation() - Node.LocalJointOffset);
			
			for(const FIntVector& Triangle : EditPreviewShape.Triangles)
			{
				for(int32 Idx = 0 ; Idx < 3 ; ++Idx)
				{
					int32 Next = (Idx + 1) % 3;

					FVector FirstVertPosition = ShapeTransform.TransformPosition(EditPreviewShape.Vertices[Triangle[Idx]]);
					FVector SecondVertPosition = ShapeTransform.TransformPosition(EditPreviewShape.Vertices[Triangle[Next]]);

					PDI->DrawLine(FirstVertPosition, SecondVertPosition, AnimDynamicsNodeConstants::ShapeDrawColor, SDPG_Foreground, AnimDynamicsNodeConstants::ShapeLineWidth);
				}
			}

			// COM
			FVector Origin = ShapeTransform.GetTranslation();
			FVector XAxis(1.0f, 0.0f, 0.0f);
			FVector YAxis(0.0f, 1.0f, 0.0f);
			FVector ZAxis(0.0f, 0.0f, 1.0f);

			XAxis = ShapeTransform.TransformVector(XAxis);
			YAxis = ShapeTransform.TransformVector(YAxis);
			ZAxis = ShapeTransform.TransformVector(ZAxis);

			PDI->DrawLine(Origin, Origin + XAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Red, SDPG_Foreground, 0.5f);
			PDI->DrawLine(Origin, Origin + YAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Green, SDPG_Foreground, 0.5f);
			PDI->DrawLine(Origin, Origin + ZAxis * AnimDynamicsNodeConstants::TransformBasisScale, FLinearColor::Blue, SDPG_Foreground, 0.5f);

			// Local joint offset
			FVector JointOffset = ShapeTransform.Rotator().RotateVector(Node.LocalJointOffset);
			PDI->DrawLine(ShapeTransform.GetTranslation(), ShapeTransform.GetTranslation() + JointOffset, FLinearColor::Green, SDPG_Foreground, AnimDynamicsNodeConstants::ShapeLineWidth);

			if (bShowLinearLimits)
			{
				DrawLinearLimits(PDI, ShapeTransform, Node);
			}

			if (bShowAngularLimits)
			{
				DrawAngularLimits(PDI, ShapeTransform, Node);
			}
		}
	}
}

void UAnimGraphNode_AnimDynamics::GetOnScreenDebugInfo(TArray<FText>& DebugInfo, FAnimNode_Base* RuntimeAnimNode, USkeletalMeshComponent* PreviewSkelMeshComp) const
{
	if(RuntimeAnimNode)
	{
		FAnimNode_AnimDynamics* PreviewNode = static_cast<FAnimNode_AnimDynamics*>(RuntimeAnimNode);
		int32 NumBones = PreviewNode->GetNumBoundBones();
		for(int32 ChainBoneIndex = 0; ChainBoneIndex < NumBones; ++ChainBoneIndex)
		{
			if(const FBoneReference* BoneRef = PreviewNode->GetBoundBoneReference(ChainBoneIndex))
			{
				const int32 SkelBoneIndex = PreviewSkelMeshComp->GetBoneIndex(BoneRef->BoneName);
				if(SkelBoneIndex != INDEX_NONE)
				{
					FTransform BoneTransform = PreviewSkelMeshComp->GetBoneTransform(SkelBoneIndex);
					DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenName", "Anim Dynamics (Bone:{0})"), FText::FromName(BoneRef->BoneName)));
					DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenTranslation", "    Translation: {0}"), FText::FromString(BoneTransform.GetTranslation().ToString())));
					DebugInfo.Add(FText::Format(LOCTEXT("DebugOnScreenRotation", "    Rotation: {0}"), FText::FromString(BoneTransform.Rotator().ToString())));
				}
			}
		}
	}
}

void UAnimGraphNode_AnimDynamics::DrawLinearLimits(FPrimitiveDrawInterface* PDI, FTransform ShapeTransform, const FAnimNode_AnimDynamics& NodeToVisualise) const
{
	// Draw linear limits
	FVector LinearLimitHalfExtents(NodeToVisualise.ConstraintSetup.LinearAxesMax - NodeToVisualise.ConstraintSetup.LinearAxesMin);
	// Add a tiny bit so we can see collapsed axes
	LinearLimitHalfExtents += FVector(0.1f);
	LinearLimitHalfExtents /= 2.0f;
	FVector LinearLimitsCenter = NodeToVisualise.ConstraintSetup.LinearAxesMin + LinearLimitHalfExtents;
	FTransform LinearLimitsTransform = ShapeTransform;
	LinearLimitsTransform.SetTranslation(LinearLimitsTransform.GetTranslation() + LinearLimitsTransform.TransformVector(LinearLimitsCenter));

	DrawBox(PDI, LinearLimitsTransform.ToMatrixWithScale(), LinearLimitHalfExtents, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_Foreground);
}

FText UAnimGraphNode_AnimDynamics::GetControllerDescription() const
{
	return LOCTEXT("Description", "Anim Dynamics");
}

void UAnimGraphNode_AnimDynamics::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> PreviewFlagHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UAnimGraphNode_AnimDynamics, bPreviewLive));

	IDetailCategoryBuilder& PreviewCategory = DetailBuilder.EditCategory(TEXT("Preview"));
	PreviewCategory.AddProperty(PreviewFlagHandle);

	FDetailWidgetRow& WidgetRow = PreviewCategory.AddCustomRow(LOCTEXT("ResetButtonRow", "Reset"));

	WidgetRow
	[
		SNew(SButton)
			.Text(LOCTEXT("ResetButtonText", "Reset Simulation"))
			.ToolTipText(LOCTEXT("ResetButtonToolTip", "Resets the simulation for this node"))
			.OnClicked(FOnClicked::CreateStatic(&UAnimGraphNode_AnimDynamics::ResetButtonClicked, &DetailBuilder))
	];
}

void UAnimGraphNode_AnimDynamics::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	
}

FText UAnimGraphNode_AnimDynamics::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("ControllerDescription"), GetControllerDescription());
	Arguments.Add(TEXT("BoundBoneName"), FText::FromName(Node.BoundBone.BoneName));
	if(Node.bChain)
	{
		Arguments.Add(TEXT("ChainEndBoneName"), FText::FromName(Node.ChainEnd.BoneName));
	}

	if(TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		if(Node.BoundBone.BoneName == NAME_None || (Node.bChain && Node.ChainEnd.BoneName == NAME_None))
		{
			return GetControllerDescription();
		}

		if(Node.bChain)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimDynamicsNodeTitleSmallChain", "{ControllerDescription} - Chain: {BoundBoneName} -> {ChainEndBoneName}"), Arguments), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimDynamicsNodeTitleSmall", "{ControllerDescription} - Bone: {BoundBoneName}"), Arguments), this);
		}
	}
	else
	{
		if(Node.bChain)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimDynamicsNodeTitleLargeChain", "{ControllerDescription}\nChain: {BoundBoneName} -> {ChainEndBoneName}"), Arguments), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimDynamicsNodeTitleLarge", "{ControllerDescription}\nBone: {BoundBoneName}"), Arguments), this);
		}
	}

	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_AnimDynamics::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Regenerate render shape(s)
	EditPreviewShape = FAnimPhysShape::MakeBox(Node.BoxExtents);
}

void UAnimGraphNode_AnimDynamics::DrawAngularLimits(FPrimitiveDrawInterface* PDI, FTransform JointTransform, const FAnimNode_AnimDynamics& NodeToVisualize) const
{
	FVector XAxis = JointTransform.GetUnitAxis(EAxis::X);
	FVector YAxis = JointTransform.GetUnitAxis(EAxis::Y);
	FVector ZAxis = JointTransform.GetUnitAxis(EAxis::Z);

	const FVector& MinAngles = NodeToVisualize.ConstraintSetup.AngularLimitsMin;
	const FVector& MaxAngles = NodeToVisualize.ConstraintSetup.AngularLimitsMax;
	FVector AngleRange = MaxAngles - MinAngles;
	FVector Middle = MinAngles + AngleRange * 0.5f;

	if (AngleRange.X > 0.0f && AngleRange.X < 180.0f)
	{
		FTransform XAxisConeTM(YAxis, XAxis ^ YAxis, XAxis, JointTransform.GetTranslation());
		XAxisConeTM.SetRotation(FQuat(XAxis, FMath::DegreesToRadians(-Middle.X)) * XAxisConeTM.GetRotation());
		DrawCone(PDI, FScaleMatrix(30.0f) * XAxisConeTM.ToMatrixWithScale(), FMath::DegreesToRadians(AngleRange.X / 2.0f), 0.0f, 24, false, FLinearColor::White, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
	}

	if (AngleRange.Y > 0.0f && AngleRange.Y < 180.0f)
	{
		FTransform YAxisConeTM(ZAxis, YAxis ^ ZAxis, YAxis, JointTransform.GetTranslation());
		YAxisConeTM.SetRotation(FQuat(YAxis, FMath::DegreesToRadians(Middle.Y)) * YAxisConeTM.GetRotation());
		DrawCone(PDI, FScaleMatrix(30.0f) * YAxisConeTM.ToMatrixWithScale(), FMath::DegreesToRadians(AngleRange.Y / 2.0f), 0.0f, 24, false, FLinearColor::White, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
	}

	if (AngleRange.Z > 0.0f && AngleRange.Z < 180.0f)
	{
		FTransform ZAxisConeTM(XAxis, ZAxis ^ XAxis, ZAxis, JointTransform.GetTranslation());
		ZAxisConeTM.SetRotation(FQuat(ZAxis, FMath::DegreesToRadians(Middle.Z)) * ZAxisConeTM.GetRotation());
		DrawCone(PDI, FScaleMatrix(30.0f) * ZAxisConeTM.ToMatrixWithScale(), FMath::DegreesToRadians(AngleRange.Z / 2.0f), 0.0f, 24, false, FLinearColor::White, GEngine->DebugEditorMaterial->GetRenderProxy(false), SDPG_World);
	}
}

void UAnimGraphNode_AnimDynamics::PostLoad()
{
	Super::PostLoad();

	EditPreviewShape = FAnimPhysShape::MakeBox(Node.BoxExtents);
}

void UAnimGraphNode_AnimDynamics::ResetSim()
{
	FAnimNode_AnimDynamics* PreviewNode = GetPreviewDynamicsNode();
	if(PreviewNode)
	{
		PreviewNode->RequestInitialise();
	}
}

FAnimNode_AnimDynamics* UAnimGraphNode_AnimDynamics::GetPreviewDynamicsNode() const
{
	FAnimNode_AnimDynamics* ActivePreviewNode = nullptr;

	if(LastPreviewComponent && LastPreviewComponent->GetAnimInstance())
	{
		UAnimInstance* Instance = LastPreviewComponent->GetAnimInstance();
		if(UAnimBlueprintGeneratedClass* Class = Cast<UAnimBlueprintGeneratedClass>(Instance->GetClass()))
		{
			ActivePreviewNode = Class->GetPropertyInstance<FAnimNode_AnimDynamics>(Instance, NodeGuid);
		}
	}

	return ActivePreviewNode;
}

FReply UAnimGraphNode_AnimDynamics::ResetButtonClicked(IDetailLayoutBuilder* DetailLayoutBuilder)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjectsList = DetailLayoutBuilder->GetSelectedObjects();
	for(TWeakObjectPtr<UObject> Object : SelectedObjectsList)
	{
		if(UAnimGraphNode_AnimDynamics* AnimDynamicsNode = Cast<UAnimGraphNode_AnimDynamics>(Object.Get()))
		{
			AnimDynamicsNode->ResetSim();
		}
	}
	
	return FReply::Handled();
}

void UAnimGraphNode_AnimDynamics::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FAnimationCustomVersion::GUID);

	const int32 CustomAnimVersion = Ar.CustomVer(FAnimationCustomVersion::GUID);
	
	if(CustomAnimVersion < FAnimationCustomVersion::AnimDynamicsAddAngularOffsets)
	{
		FAnimPhysConstraintSetup& ConSetup = Node.ConstraintSetup;
		ConSetup.AngularLimitsMin = FVector(-ConSetup.AngularXAngle_DEPRECATED, -ConSetup.AngularYAngle_DEPRECATED, -ConSetup.AngularZAngle_DEPRECATED);
		ConSetup.AngularLimitsMax = FVector(ConSetup.AngularXAngle_DEPRECATED, ConSetup.AngularYAngle_DEPRECATED, ConSetup.AngularZAngle_DEPRECATED);
	}
}

#undef LOCTEXT_NAMESPACE
