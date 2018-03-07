// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node.h"
#include "UObject/SoftObjectPath.h"
#include "K2Node_ControlRig.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
struct FSlateIcon;
class IControlRigField;

/** Support struct for labeled inputs */
USTRUCT()
struct FUserLabeledField
{
	GENERATED_BODY()

	/** User-defined label */
	UPROPERTY(EditAnywhere, Category = "Labeled Field")
	FString Label;

	/** The name of the field */
	UPROPERTY(EditAnywhere, Category = "Labeled Field")
	FName FieldName;
};

/** Base class for animation ControlRig-related nodes */
UCLASS(Abstract)
class UK2Node_ControlRig : public UK2Node
{
	GENERATED_UCLASS_BODY()

	friend class FControlRigInputOutputDetailsCustomization;
	friend class FUserLabeledFieldCustomization;

	/** 
	 * The ControlRig class we last referenced.
	 * We need to cache this here as we need to access it during compilation when CDOs etc. may be in flux.
	 */
	UPROPERTY()
	mutable FSoftClassPath ControlRigClass;

	/** Disabled input pins */
	UPROPERTY(EditAnywhere, Category = "Inputs")
	TArray<FName> DisabledInputs;

	/** Disabled output pins */
	UPROPERTY(EditAnywhere, Category = "Outputs")
	TArray<FName> DisabledOutputs;

	/** Labeled input pins */
	UPROPERTY(EditAnywhere, Category = "Inputs")
	TArray<FUserLabeledField> LabeledInputs;

	/** Labeled output pins */
	UPROPERTY(EditAnywhere, Category = "Outputs")
	TArray<FUserLabeledField> LabeledOutputs;

public:

	// UEdGraphNode Interface.
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	// UK2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& CompilerContext) const override;
	virtual bool ShouldShowNodeProperties() const { return true; }
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput = nullptr) const;
	virtual void HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName) override;
	virtual bool ReferencesVariable(const FName& InVarName, const UStruct* InScope) const override;

	/** Check whether an input pin is disabled by name */
	bool IsInputPinDisabled(const FName& PinName) const { return DisabledInputs.Contains(PinName); }

	/** Disable or enable the specified input pin */
	void SetInputPinDisabled(const FName& PinName, bool bDisabled);

	/** Check whether this ControlRig has an input pin */
	bool HasInputPin(const FName& PinName) const;

	/** Check whether an output pin is disabled by name */
	bool IsOutputPinDisabled(const FName& PinName) const { return DisabledOutputs.Contains(PinName); }

	/** Disable or enable the specified output pin */
	void SetOutputPinDisabled(const FName& PinName, bool bDisabled);

	/** Check whether this ControlRig has an output pin */
	bool HasOutputPin(const FName& PinName) const;

protected:
	/** Check whether this node can display any inputs */
	virtual bool HasInputs() const { return true; }

	/** Check whether this node can display any outputs */
	virtual bool HasOutputs() const { return true; }

	/** 
	 * Get the context in which this node's I/O is considered (i.e. blueprint, class etc.). This allows us to look in specific places to find 
	 * the ControlRig class to use. See implementation of GetControlRigClass()
	 */
	virtual const UClass* GetControlRigClassImpl() const PURE_VIRTUAL(GetControlRigClassImpl, return nullptr;);

	/** Get what pin direction this node uses to represent a ControlRig input */
	virtual EEdGraphPinDirection GetInputDirection() const { return EGPD_Input; }

	/** Get what pin direction this node uses to represent a ControlRig output */
	virtual EEdGraphPinDirection GetOutputDirection() const { return EGPD_Output; }

	/** Get the input parameter pins */
	void GetInputParameterPins(const TArray<FName>& DisabledPins, TArray<UEdGraphPin*>& OutPins, TArray<TSharedRef<IControlRigField>>& OutFieldInfo) const;

	/** Get the input variable names */
	TArray<TSharedRef<IControlRigField>> GetInputVariableInfo() const;
	virtual TArray<TSharedRef<IControlRigField>> GetInputVariableInfo(const TArray<FName>& DisabledPins) const;

	/** Get the output parameter pins */
	void GetOutputParameterPins(const TArray<FName>& DisabledPins, TArray<UEdGraphPin*>& OutPins, TArray<TSharedRef<IControlRigField>>& OutFieldInfo) const;

	/** Get the output variable names */
	TArray<TSharedRef<IControlRigField>> GetOutputVariableInfo() const;
	virtual TArray<TSharedRef<IControlRigField>> GetOutputVariableInfo(const TArray<FName>& DisabledPins) const;

	/** Helper function for derived classes implementing GetControlRigClassImpl */
	static UClass* GetControlRigClassFromBlueprint(const UBlueprint* Blueprint);

	/** Get the ControlRig class of this component */
	UClass* GetControlRigClass() const;

	/** Create a ControlRig field from a field on the ControlRig class, if possible */
	virtual TSharedPtr<IControlRigField> CreateControlRigField(UField* Field) const;

	/** Create a labeled ControlRig field from a field on the ControlRig class, if possible */
	virtual TSharedPtr<IControlRigField> CreateLabeledControlRigField(UField* Field, const FString& Label, bool bIsInputContext) const;

	/** Check whether we can create a labeled ControlRig field from a field on the ControlRig class */
	virtual bool CanCreateLabeledControlRigField(UField* Field, bool bIsInputContext) const;

	/** Get all fields that act as inputs for this ControlRig */
	virtual void GetInputFields(const TArray<FName>& DisabledPins, TArray<TSharedRef<IControlRigField>>& OutFields) const;

	/** Get all fields that act as outputs for this ControlRig */
	virtual void GetOutputFields(const TArray<FName>& DisabledPins, TArray<TSharedRef<IControlRigField>>& OutFields) const;

	/** Get all potential labeled fields that act as inputs for this ControlRig */
	virtual void GetPotentialLabeledInputFields(TArray<UField*>& OutFields) const;

	/** Get all potential labeled fields that act as outputs for this ControlRig */
	virtual void GetPotentialLabeledOutputFields(TArray<UField*>& OutFields) const;
};
