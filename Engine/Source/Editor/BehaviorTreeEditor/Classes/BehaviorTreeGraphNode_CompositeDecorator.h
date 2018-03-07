// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.generated.h"

class UEdGraph;

UCLASS()
class UBehaviorTreeGraphNode_CompositeDecorator : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()

	// The logic graph for this decorator (returning a boolean)
	UPROPERTY()
	class UEdGraph* BoundGraph;

	UPROPERTY(EditAnywhere, Category=Description)
	FString CompositeName;

	/** if set, all logic operations will be shown in description */
	UPROPERTY(EditAnywhere, Category=Description)
	uint32 bShowOperations : 1;

	/** updated with internal graph changes, set when decorators inside can abort flow */
	UPROPERTY()
	uint32 bCanAbortFlow : 1;

	uint32 bHasBrokenInstances : 1;

	FString GetNodeTypeDescription() const;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetDescription() const override;
	virtual void PostPlacedNewNode() override;
	virtual void PostLoad() override;
	virtual UEdGraph* GetBoundGraph() const override { return BoundGraph; }
	virtual bool IsSubNode() const override;
	virtual bool HasErrors() const override;
	virtual bool RefreshNodeClass() override;
	virtual void UpdateNodeClassData() override;

	virtual void PrepareForCopying() override;
	virtual void PostCopyNode() override;

	int32 SpawnMissingNodes(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32 StartIndex);
	void CollectDecoratorData(TArray<class UBTDecorator*>& NodeInstances, TArray<struct FBTDecoratorLogic>& Operations) const;
	void SetDecoratorData(class UBTCompositeNode* InParentNode, uint8 InChildIndex);
	void InitializeDecorator(class UBTDecorator* InnerDecorator);
	void OnBlackboardUpdate();
	void OnInnerGraphChanged();
	void BuildDescription();
	void UpdateBrokenInstances();

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void ResetExecutionRange();

	/** Execution index range of internal nodes, used by debugger */
	uint16 FirstExecutionIndex;
	uint16 LastExecutionIndex;

protected:
	void CreateBoundGraph();

	UPROPERTY()
	class UBTCompositeNode* ParentNodeInstance;

	uint8 ChildIndex;

	UPROPERTY()
	FString CachedDescription;
};
