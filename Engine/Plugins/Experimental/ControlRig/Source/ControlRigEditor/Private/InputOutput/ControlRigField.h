// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/UnrealType.h"

class FKismetCompilerContext;
class UEdGraph;

/** Information about an input/output field */
class IControlRigField
{
public:
	virtual ~IControlRigField() {}

	/** Get the field we refer to */
	virtual UField* GetField() const = 0;

	/** Get the name of this field */
	virtual FName GetName() const = 0;

	/** Get the string to display on the pin this field generates */
	virtual FString GetPinString() const = 0;

	/** Get the name to display for this field */
	virtual FText GetDisplayNameText() const = 0;

	/** Get the pin type to use for this field */
	virtual FEdGraphPinType GetPinType() const = 0;

	/** Whether this field can be disabled using the pin's checkbox in the details panel */
	virtual bool CanBeDisabled() const = 0;

	/** Create an output of this type during the node expansion step */
	virtual void ExpandPin(UClass* Class, FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphNode* InSourceNode, UEdGraphPin* InPin, UEdGraphPin* InSelfPin, bool bMoveExecPins, UEdGraphPin*& InOutExecPin) const = 0;
};

/** Information about an input/output property */
class FControlRigProperty : public IControlRigField
{
public:
	FControlRigProperty(UProperty* InProperty);

	// IControlRigField interface
	virtual UField* GetField() const override { return Property; }
	virtual FName GetName() const override { return Property->GetFName(); }
	virtual FString GetPinString() const override { return GetName().ToString(); }
	virtual FText GetDisplayNameText() const override  { return Property->GetDisplayNameText(); }
	virtual FEdGraphPinType GetPinType() const override  { return PinType; }
	virtual bool CanBeDisabled() const { return true; }
	virtual void ExpandPin(UClass* Class, FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphNode* InSourceNode, UEdGraphPin* InPin, UEdGraphPin* InSelfPin, bool bMoveExecPins, UEdGraphPin*& InOutExecPin) const override;

	/** The field that we use as input/output */
	UProperty* Property;

	/** Pin type we use for the field */
	FEdGraphPinType PinType;
};

/** 
 * We support function I/O using parameters mapped to specific names.
 * The name is provided as a literal and the value as a pin.
 */
class FControlRigFunction_Name : public IControlRigField
{
public:
	FControlRigFunction_Name(const FName& InLabel, UFunction* InFunction, UProperty* InNameProperty, UProperty* InValueProperty);

	// IControlRigField interface
	virtual UField* GetField() const override { return Function; }
	virtual FName GetName() const override { return Function->GetFName(); }
	virtual FString GetPinString() const override { return DisplayText.ToString(); }
	virtual FText GetDisplayNameText() const override { return DisplayText; }
	virtual FEdGraphPinType GetPinType() const override { return PinType; }
	virtual bool CanBeDisabled() const { return false; }
	virtual void ExpandPin(UClass* Class, FKismetCompilerContext& InCompilerContext, UEdGraph* InSourceGraph, UEdGraphNode* InSourceNode, UEdGraphPin* InPin, UEdGraphPin* InSelfPin, bool bMoveExecPins, UEdGraphPin*& InOutExecPin) const override;

	/** Label to display in UI */
	FName Label;

	/** The field that we use as input/output */
	UFunction* Function;

	/** Property used to specify the name */
	UProperty* NameProperty;

	/** Property used to specify the value, either return param or value */
	UProperty* ValueProperty;

	/** Pin type we use for the field */
	FEdGraphPinType PinType;

	/** Display text for UI */
	FText DisplayText;
};
