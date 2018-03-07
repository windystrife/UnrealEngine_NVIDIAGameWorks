// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AssetData.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "AnimationGraphSchema.generated.h"

class FMenuBuilder;
class UAnimationAsset;

UCLASS(MinimalAPI)
class UAnimationGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_UCLASS_BODY()

	// Common PinNames
	UPROPERTY()
	FString PN_SequenceName;    // PC_Object+PSC_Sequence

	UPROPERTY()
	FName NAME_NeverAsPin;

	UPROPERTY()
	FName NAME_PinHiddenByDefault;

	UPROPERTY()
	FName NAME_PinShownByDefault;

	UPROPERTY()
	FName NAME_AlwaysAsPin;

	UPROPERTY()
	FName NAME_CustomizeProperty;

	UPROPERTY()
	FName NAME_OnEvaluate;
	
	UPROPERTY()
	FName DefaultEvaluationHandlerName;

	//~ Begin UEdGraphSchema Interface.
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual void HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const override;
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const override;
	virtual void DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const override;
	virtual void GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const override;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const override {	return false; }
	virtual bool DoesSupportEventDispatcher() const	override { return false; }
	virtual bool ShouldAlwaysPurgeOnModification() const override { return true; }
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	//~ End UEdGraphSchema Interface.

	//~ Begin UEdGraphSchema_K2 Interface
	virtual const FPinConnectionResponse DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const override;
	virtual bool SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, /*out*/ FName& TargetFunction, /*out*/ UClass*& FunctionOwner) const override;
	virtual bool ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext = NULL, bool bIgnoreArray = false) const override;
	virtual bool DoesSupportAnimNotifyActions() const override;
	//~ End UEdGraphSchema_K2 Interface

	/** Spawn the correct node in the Animation Graph using the given AnimationAsset at the supplied location */
	static void SpawnNodeFromAsset(UAnimationAsset* Asset, const FVector2D& GraphPosition, UEdGraph* Graph, UEdGraphPin* PinIfAvailable);

	/** Update the specified node to a new asset */
	static void UpdateNodeWithAsset(class UK2Node* K2Node, UAnimationAsset* Asset);

	// @todo document
	ANIMGRAPH_API static bool IsPosePin(const FEdGraphPinType& PinType);

	// @todo document
	ANIMGRAPH_API static bool IsLocalSpacePosePin(const FEdGraphPinType& PinType);

	// @todo document
	ANIMGRAPH_API static bool IsComponentSpacePosePin(const FEdGraphPinType& PinType);
};



