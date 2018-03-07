// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphSchema.h"
#include "MaterialGraphSchema.generated.h"

class UEdGraph;

/** Action to add an expression node to the graph */
USTRUCT()
struct UNREALED_API FMaterialGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	/** Class of expression we want to create */
	UPROPERTY()
	class UClass* MaterialExpressionClass;

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FMaterialGraphSchemaAction_NewNode"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FMaterialGraphSchemaAction_NewNode() 
		: FEdGraphSchemaAction()
		, MaterialExpressionClass(nullptr)
	{}

	FMaterialGraphSchemaAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, FText InKeywords)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords))
		, MaterialExpressionClass(nullptr)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

	/**
	 * Sets the type of a Function input based on an EMaterialValueType value.
	 *
	 * @param	FunctionInput		The function input to set.
	 * @param	MaterialValueType	Value type we want input to accept.
	 */
	void SetFunctionInputType(class UMaterialExpressionFunctionInput* FunctionInput, uint32 MaterialValueType) const;
};

/** Action to add a Material Function call to the graph */
USTRUCT()
struct UNREALED_API FMaterialGraphSchemaAction_NewFunctionCall : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	/** Path to function that we want to call */
	UPROPERTY()
	FString FunctionPath;

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FMaterialGraphSchemaAction_NewFunctionCall"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FMaterialGraphSchemaAction_NewFunctionCall() 
		: FEdGraphSchemaAction()
	{}

	FMaterialGraphSchemaAction_NewFunctionCall(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Action to add a comment node to the graph */
USTRUCT()
struct UNREALED_API FMaterialGraphSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FMaterialGraphSchemaAction_NewComment"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FMaterialGraphSchemaAction_NewComment() 
		: FEdGraphSchemaAction()
	{}

	FMaterialGraphSchemaAction_NewComment(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Action to paste clipboard contents into the graph */
USTRUCT()
struct UNREALED_API FMaterialGraphSchemaAction_Paste : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FMaterialGraphSchemaAction_Paste"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FMaterialGraphSchemaAction_Paste() 
		: FEdGraphSchemaAction()
	{}

	FMaterialGraphSchemaAction_Paste(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

UCLASS(MinimalAPI)
class UMaterialGraphSchema : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Allowable PinType.PinCategory values
	UPROPERTY()
	FString PC_Mask;

	UPROPERTY()
	FString PC_Required;

	UPROPERTY()
	FString PC_Optional;

	UPROPERTY()
	FString PC_MaterialInput;

	// Common PinType.PinSubCategory values
	UPROPERTY()
	FString PSC_Red;

	UPROPERTY()
	FString PSC_Green;

	UPROPERTY()
	FString PSC_Blue;

	UPROPERTY()
	FString PSC_Alpha;

	// Color of certain pins/connections
	UPROPERTY()
	FLinearColor ActivePinColor;

	UPROPERTY()
	FLinearColor InactivePinColor;

	UPROPERTY()
	FLinearColor AlphaPinColor;

	/**
	 *  Add all linked to nodes to this pin to selection
	 *  
	 *	@param	Graph			CurrentGraph
	 *	@param	InGraphPin		Pin clicked on
	 */
	void SelectAllInputNodes(UEdGraph* Graph, UEdGraphPin* InGraphPin);

	/**
	 * Get menu for breaking links to specific nodes
	 *
	 * @param	MenuBuilder	MenuBuilder we are populating
	 * @param	InGraphPin	Pin with links to break
	 */
	void GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, class UEdGraphPin* InGraphPin);

	/**
	 * Connect a pin to one of the Material Function's outputs
	 *
	 * @param	InGraphPin	Pin we are connecting
	 * @param	InFuncPin	Desired output pin
	 */
	void OnConnectToFunctionOutput(UEdGraphPin* InGraphPin, UEdGraphPin* InFuncPin);

	/**
	 * Connect a pin to one of the Material's inputs
	 *
	 * @param	InGraphPin	Pin we are connecting
	 * @param	ConnIndex	Index of the Material input to connect to
	 */
	void OnConnectToMaterial(UEdGraphPin* InGraphPin, int32 ConnIndex);

	UNREALED_API void GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder, const FString& CategoryName, bool bMaterialFunction) const;

	/** Check whether connecting these pins would cause a loop */
	bool ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const;

	/** Check whether the types of pins are compatible */
	bool ArePinsCompatible(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin, FText& ResponseMessage) const;

	/** Gets the type of this pin (must be part of a UMaterialGraphNode_Base) */
	UNREALED_API static uint32 GetMaterialValueType(const UEdGraphPin* MaterialPin);

	//~ Begin UEdGraphSchema Interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override { return true; }
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) override;
	virtual void DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
	virtual bool IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const override;
	virtual int32 GetCurrentVisualizationCacheID() const override;
	virtual void ForceVisualizationCacheClear() const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* NodeToDelete) const override;
	//~ End UEdGraphSchema Interface

private:
	/** Adds actions for all Material Functions */
	void GetMaterialFunctionActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;
	/** Adds action for creating a comment */
	void GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph = NULL) const;
	/**
	 * Checks whether a Material Function has any connections that are compatible with a type/direction
	 *
	 * @param	FunctionAssetData	Asset Data for function to test against (may need to be fully loaded).
	 * @param	TestType			Material Value Type we are testing.
	 * @param	TestDirection		Pin Direction we are testing.
	*/
	bool HasCompatibleConnection(const FAssetData& FunctionAssetData, uint32 TestType, EEdGraphPinDirection TestDirection) const;

private:
	// ID for checking dirty status of node titles against, increases whenever 
	static int32 CurrentCacheRefreshID;
};
