// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node.h"
#include "K2Node_GetDataTableRow.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UDataTable;
class UEdGraph;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_GetDataTableRow : public UK2Node
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetTooltipText() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostReconstructNode() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PreloadRequiredAssets() override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	//~ End UK2Node Interface

	/** Get the return type of our struct */
	UScriptStruct* GetReturnTypeForStruct();

	/** Get the then output pin */
	UEdGraphPin* GetThenPin() const;
	/** Get the Data Table input pin */
	UEdGraphPin* GetDataTablePin(const TArray<UEdGraphPin*>* InPinsToSearch=NULL) const;
	/** Get the spawn transform input pin */	
	UEdGraphPin* GetRowNamePin() const;
    /** Get the exec output pin for when the row was not found */
	UEdGraphPin* GetRowNotFoundPin() const;
	/** Get the result output pin */
	UEdGraphPin* GetResultPin() const;

	/** Get the type of the TableRow to return */
	UScriptStruct* GetDataTableRowStructType() const;

	void OnDataTableRowListChanged(const UDataTable* DataTable);
private:
	/**
	 * Takes the specified "MutatablePin" and sets its 'PinToolTip' field (according
	 * to the specified description)
	 * 
	 * @param   MutatablePin	The pin you want to set tool-tip text on
	 * @param   PinDescription	A string describing the pin's purpose
	 */
	void SetPinToolTip(UEdGraphPin& MutatablePin, const FText& PinDescription) const;

	/** Set the return type of our struct */
	void SetReturnTypeForStruct(UScriptStruct* InClass);
	/** Queries for the authoritative return type, then modifies the return pin to match */
	void RefreshOutputPinType();
	/** Triggers a refresh which will update the node's widget; aimed at updating the dropdown menu for the RowName input */
	void RefreshRowNameOptions();

	/** Tooltip text for this node. */
	FText NodeTooltip;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};
