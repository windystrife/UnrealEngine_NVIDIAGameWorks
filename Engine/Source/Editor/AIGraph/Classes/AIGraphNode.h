// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "AIGraphTypes.h"
#include "AIGraphNode.generated.h"

class UEdGraph;
class UEdGraphPin;
class UEdGraphSchema;
struct FDiffResults;
struct FDiffSingleResult;

UCLASS()
class AIGRAPH_API UAIGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** instance class */
	UPROPERTY()
	struct FGraphNodeClassData ClassData;

	UPROPERTY()
	UObject* NodeInstance;

	UPROPERTY(transient)
	UAIGraphNode* ParentNode;

	UPROPERTY()
	TArray<UAIGraphNode*> SubNodes;

	/** subnode index assigned during copy operation to connect nodes again on paste */
	UPROPERTY()
	int32 CopySubNodeIndex;

	/** if set, all modifications (including delete/cut) are disabled */
	UPROPERTY()
	uint32 bIsReadOnly : 1;

	/** if set, this node will be always considered as subnode */
	UPROPERTY()
	uint32 bIsSubNode : 1;

	/** error message for node */
	UPROPERTY()
	FString ErrorMessage;

	//~ Begin UEdGraphNode Interface
	virtual class UAIGraph* GetAIGraph();
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual void PostPlacedNewNode() override;
	virtual void PrepareForCopying() override;
	virtual bool CanDuplicateNode() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual void DestroyNode() override;
	virtual FText GetTooltipText() const override;
	virtual void NodeConnectionListChanged() override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;
	virtual void FindDiffs(class UEdGraphNode* OtherNode, struct FDiffResults& Results) override;
	virtual FString GetPropertyNameAndValueForDiff(const UProperty* Prop, const uint8* PropertyAddr) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
#endif
	// End UObject

	// @return the input pin for this state
	virtual UEdGraphPin* GetInputPin(int32 InputIndex = 0) const;
	// @return the output pin for this state
	virtual UEdGraphPin* GetOutputPin(int32 InputIndex = 0) const;
	virtual UEdGraph* GetBoundGraph() const { return NULL; }

	virtual FText GetDescription() const;
	virtual void PostCopyNode();

	void AddSubNode(UAIGraphNode* SubNode, class UEdGraph* ParentGraph);
	void RemoveSubNode(UAIGraphNode* SubNode);
	virtual void RemoveAllSubNodes();
	virtual void OnSubNodeRemoved(UAIGraphNode* SubNode);
	virtual void OnSubNodeAdded(UAIGraphNode* SubNode);

	virtual int32 FindSubNodeDropIndex(UAIGraphNode* SubNode) const;
	virtual void InsertSubNodeAt(UAIGraphNode* SubNode, int32 DropIndex);

	/** check if node is subnode */
	virtual bool IsSubNode() const;

	/** initialize instance object  */
	virtual void InitializeInstance();

	/** reinitialize node instance */
	virtual bool RefreshNodeClass();

	/** updates ClassData from node instance */
	virtual void UpdateNodeClassData();

	/** Check if node instance uses blueprint for its implementation */
	bool UsesBlueprint() const;

	/** check if node has any errors, used for assigning colors on graph */
	virtual bool HasErrors() const;

	static void UpdateNodeClassDataFrom(UClass* InstanceClass, FGraphNodeClassData& UpdatedData);

protected:

	virtual void ResetNodeOwner();
};
