// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNodes/AnimNode_PoseDriver.h"
#include "AnimGraphNode_PoseHandler.h"
#include "AnimGraphNode_PoseDriver.generated.h"

class FCompilerResultsLog;

UCLASS(Experimental)
class ANIMGRAPH_API UAnimGraphNode_PoseDriver : public UAnimGraphNode_PoseHandler
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_PoseDriver Node;

	/** Used to indicate selected target to edit mode drawing */
	int32 SelectedTargetIndex;
	/** Delegate to call when selection changes */
	FSimpleMulticastDelegate SelectedTargetChangeDelegate;

public:

	/** Get the current preview node instance */
	FAnimNode_PoseDriver* GetPreviewPoseDriverNode() const;

	/** Util to replace current contents of PoseTargets with info from assigned PoseAsset */
	void CopyTargetsFromPoseAsset();

	/** Automatically modify TargetScale for each PoseTarget, based on distance to nearest neighbor */
	void AutoSetTargetScales(float& OutMaxDistance);

	/** Adds a new target, reallocating transforms array appropriately */
	void AddNewTarget();

	/** Reallocates transforms arrays as necessary to accommodate source bones */
	void ReserveTargetTransforms();

	/** Used to refer back to preview instance in anim tools */
	UPROPERTY(Transient)
	USkeletalMeshComponent* LastPreviewComponent;

	// Begin UObject Interface.
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject Interface.

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	virtual FEditorModeID GetEditorMode() const override;
	virtual EAnimAssetHandlerType SupportsAssetClass(const UClass* AssetClass) const override;
	virtual void CopyNodeDataToPreviewNode(FAnimNode_Base* InPreviewNode) override;
	// End of UAnimGraphNode_Base interface

protected:
	// UAnimGraphNode_PoseHandler interface
	virtual bool IsPoseAssetRequired() { return false; }
	virtual FAnimNode_PoseHandler* GetPoseHandlerNode() override { return &Node; }
	virtual const FAnimNode_PoseHandler* GetPoseHandlerNode() const override { return &Node; }
	// End of UAnimGraphNode_PoseHandler interface
};
