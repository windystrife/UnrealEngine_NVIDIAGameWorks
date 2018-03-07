// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraNode.h"
#include "SlateTypes.h"
#include "EdGraph/EdGraph.h"

#include "NiagaraNodeWithDynamicPins.generated.h"

class UEdGraphPin;
class SNiagaraGraphPinAdd;


/** A base node for niagara nodes with pins which can be dynamically added and removed by the user. */
UCLASS()
class UNiagaraNodeWithDynamicPins : public UNiagaraNode
{
public:
	GENERATED_BODY()

	//~ UEdGraphNode interface
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;

	/** Requests a new pin be added to the node with the specified direction, type, and name. */
	UEdGraphPin* RequestNewTypedPin(EEdGraphPinDirection Direction, const FNiagaraTypeDefinition& Type, FString InName);

	/** Requests a new pin be added to the node with the specified direction and type. */
	UEdGraphPin* RequestNewTypedPin(EEdGraphPinDirection Direction, const FNiagaraTypeDefinition& Type);

	/** Helper to identify if a pin is an Add pin.*/
	bool IsAddPin(const UEdGraphPin* Pin) const;

	/** Determine whether or not a Niagara type is supported for an Add Pin possibility.*/
	virtual bool AllowNiagaraTypeForAddPin(const FNiagaraTypeDefinition& InType);

	/** Used in tandem with SNiagaraGraphPinAdd to generate the menu for selecting the pin to add.*/
	virtual TSharedRef<SWidget> GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin);

protected:
	/** Creates an add pin on the node for the specified direction. */
	void CreateAddPin(EEdGraphPinDirection Direction);

	/** Called when a new typed pin is added by the user. */
	virtual void OnNewTypedPinAdded(UEdGraphPin* NewPin) { }

	/** Called when a pin is renamed. */
	virtual void OnPinRenamed(UEdGraphPin* RenamedPin) { }

	/** Called to determine if a pin can be renamed by the user. */
	virtual bool CanRenamePin(const UEdGraphPin* Pin) const;

	/** Called to determine if a pin can be removed by the user. */
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const;

	/** Called to determine if a pin can be moved by the user.*/
	virtual bool CanMovePin(const UEdGraphPin* Pin) const;

	/** Used GenerateAddPinMenu to build a list of supported types.*/
	virtual void BuildTypeMenu(FMenuBuilder& InMenuBuilder, const FString& InWorkingName, SNiagaraGraphPinAdd* InPin);

	/** Removes a pin from this node with a transaction. */
	virtual void RemoveDynamicPin(UEdGraphPin* Pin);

	virtual void MoveDynamicPin(UEdGraphPin* Pin, int32 DirectionToMove);

private:

	/** Gets the display text for a pin. */
	FText GetPinNameText(UEdGraphPin* Pin) const;

	/** Called when a pin's name text is committed. */
	void PinNameTextCommitted(const FText& Text, ETextCommit::Type CommitType, UEdGraphPin* Pin);

public:
	/** The sub category for add pins. */
	static const FString AddPinSubCategory;
};
