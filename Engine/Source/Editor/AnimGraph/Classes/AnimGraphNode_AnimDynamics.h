// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Input/Reply.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "Animation/AnimPhysicsSolver.h"
#include "BoneControllers/AnimNode_AnimDynamics.h"
#include "AnimGraphNode_SkeletalControlBase.h"

#include "AnimGraphNode_AnimDynamics.generated.h"

class FCompilerResultsLog;
class FPrimitiveDrawInterface;
class IDetailLayoutBuilder;
class USkeletalMeshComponent;

namespace AnimDynamicsNodeConstants
{
	const FLinearColor ShapeDrawColor = FLinearColor::White;
	const FLinearColor ActiveBodyDrawColor = FLinearColor::Yellow;
	const float ShapeLineWidth = 0.07f;
	const float BodyLineWidth = 0.07f;
	const float TransformLineWidth = 0.05f;
	const float TransformBasisScale = 10.0f;
}

UCLASS()
class UAnimGraphNode_AnimDynamics : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_AnimDynamics Node;

	/** Preview the live physics object on the mesh */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bPreviewLive;

	/** Show linear (prismatic) limits in the viewport */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bShowLinearLimits;

	/** Show angular limit ranges in the viewport */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bShowAngularLimits;

	/** Show planar limit info (actual plane, plane normal) in the viewport */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bShowPlanarLimit;

	/** Show spherical limits in the viewport (preview live only) */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bShowSphericalLimit;

	/** If planar limits are enabled and the collision mode isn't CoM, draw sphere collision sizes */
	UPROPERTY(EditAnywhere, Category = Preview)
	bool bShowCollisionSpheres;

	UPROPERTY(Transient)
	mutable USkeletalMeshComponent* LastPreviewComponent;

public:

	virtual void PostLoad() override;

	static FReply ResetButtonClicked(IDetailLayoutBuilder* DetailLayoutBuilder);
	void ResetSim();

	// UObject
	virtual void Serialize(FArchive& Ar) override;

	// UEdGraphNode_Base
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

	// UAnimGraphNode_Base
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	// UAnimGraphNode_SkeletalControlBase
	virtual void Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& DebugInfo, FAnimNode_Base* RuntimeAnimNode, USkeletalMeshComponent* PreviewSkelMeshComp) const override;

	FAnimNode_AnimDynamics* GetPreviewDynamicsNode() const;

protected:

	// Keep a version of the current shape for rendering
	FAnimPhysShape EditPreviewShape;

	virtual FText GetControllerDescription() const override;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void DrawLinearLimits(FPrimitiveDrawInterface* PDI, FTransform ShapeTransform, const FAnimNode_AnimDynamics& NodeToVisualise) const;
	void DrawAngularLimits(FPrimitiveDrawInterface* PDI, FTransform JointTransform, const FAnimNode_AnimDynamics& NodeToVisualize) const;

	// UAnimGraphNode_SkeletalControlBase protected interface
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
	// End of UAnimGraphNode_SkeletalControlBase protected interface

private:
	FNodeTitleTextTable CachedNodeTitles;
};
