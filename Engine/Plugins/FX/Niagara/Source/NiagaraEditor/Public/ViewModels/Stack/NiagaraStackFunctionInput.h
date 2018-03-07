// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterHandle.h"
#include "NiagaraStackFunctionInput.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraNodeAssignment;
class UNiagaraNodeParameterMapSet;
class FStructOnScope;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackObject;
class UNiagaraScript;
class UEdGraphPin;
class UNiagaraDataInterface;
class UNiagaraStackEditorData;

/** Represents a single module input in the module stack view model. */
UCLASS()
class NIAGARAEDITOR_API UNiagaraStackFunctionInput : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	/** Defines different modes which are used to provide the value for this function input. */
	enum class EValueMode
	{
		/** The value is set to a constant stored locally with this input. */
		Local,
		/** The value is linked to a parameter defined outside of this function. */
		Linked,
		/** The value is provided by a secondary dynamic input function. */
		Dynamic,
		/** The value is provided by a data interface object. */
		Data,
		/** The value source for this input was not set, or couldn't be determined. */
		Invalid
	};

	DECLARE_MULTICAST_DELEGATE(FOnValueChanged);
	DECLARE_MULTICAST_DELEGATE(FOnPinnedChanged);

public:
	UNiagaraStackFunctionInput();

	virtual void BeginDestroy() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** 
	 * Sets the input data for this entry.
	 * @param InSystemViewModel The view model for the system which owns the stack containing this entry.
	 * @param InEmitterViewModel The view model for the emitter which owns the stack containing this entry.
	 * @param InStackEditorData The stack editor data for this input.
	 * @param InModuleNode The module function call which owns this input entry. NOTE: This input might not be an input to the module function call, it may be an input to a dynamic input function call which is owned by the module.
	 * @param InInputFunctionCallNode The function call which this entry is an input to. NOTE: This node can be a module function call node or a dynamic input node.
	 * @param InInputParameterHandle The input parameter handle for the function call.
	 * @param InInputType The type of this input.
	 */
	void Initialize(
		TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
		TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
		UNiagaraStackEditorData& InStackEditorData,
		UNiagaraNodeFunctionCall& InModuleNode,
		UNiagaraNodeFunctionCall& InInputFunctionCallNode,
		FString InInputParameterHandle,
		FNiagaraTypeDefinition InInputType);

	/** Gets the function call node which owns this input. */
	const UNiagaraNodeFunctionCall& GetInputFunctionCallNode() const;

	/** Gets the current value mode */
	EValueMode GetValueMode();

	/** Gets whether or not this input can be pinned. */
	bool GetCanBePinned() const;

	/** Gets the type of this input. */
	const FNiagaraTypeDefinition& GetInputType() const;

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FName GetTextStyleName() const override;
	virtual bool GetCanExpand() const override;
	virtual int32 GetItemIndentLevel() const override;

	/** Sets the item indent level for this stack entry. */
	void SetItemIndentLevel(int32 InItemIndentLevel);

	/** Gets the path of parameter handles from the owning module to the function call which owns this input. */
	const TArray<FNiagaraParameterHandle>& GetInputParameterHandlePath() const;

	/** Gets the parameter handle which defined this input in the module. */
	const FNiagaraParameterHandle& GetInputParameterHandle() const;

	/** Gets the handle to the linked value for this input if there is one. */
	const FNiagaraParameterHandle& GetLinkedValueHandle() const;

	/** Sets the value of this input to a linked parameter handle. */
	void SetLinkedValueHandle(const FNiagaraParameterHandle& InParameterHandle);

	/** Gets the current set of available parameter handes which can be assigned to this input. */
	void GetAvailableParameterHandles(TArray<FNiagaraParameterHandle>& AvailableParameterHandles);

	/** Gets the dynamic input node providing the value for this input, if one is available. */
	UNiagaraNodeFunctionCall* GetDynamicInputNode() const;

	/** Gets the dynamic inputs available for this input. */
	void GetAvailableDynamicInputs(TArray<UNiagaraScript*>& AvailableDynamicInputs);

	/** Sets the dynamic input script for this input. */
	void SetDynamicInput(UNiagaraScript* DynamicInput);

	/** Gets the current struct value of this input is there is one. */
	TSharedPtr<FStructOnScope> GetLocalValueStruct();

	/** Gets the current data object value of this input is there is one. */
	UNiagaraDataInterface* GetDataValueObject();

	/** Gets whether or not this input is pinned in the UI. */
	bool GetIsPinned() const;

	/** Sets whether or not this input is pinned in the UI. */
	void SetIsPinned(bool bIsPinned);

	/** Called to notify the input that an ongoing change to it's value has begun. */
	void NotifyBeginLocalValueChange();

	/** Called to notify the input that an ongoing change to it's value has ended. */
	void NotifyEndLocalValueChange();

	/** Sets this input's local value. */
	void SetLocalValue(TSharedRef<FStructOnScope> InLocalValue);

	/** Returns whether or not the value or handle of this input has been overridden and can be reset. */
	bool CanReset() const;

	/** Resets the value and handle of this input to the value and handle defined in the module. */
	void Reset();

	/** Returns whether or not this input can be renamed. */
	bool CanRenameInput() const;

	/** Gets whether this input has a rename pending. */
	bool GetIsRenamePending() const;

	/** Sets whether this input has a rename pending. */
	void SetIsRenamePending(bool bIsRenamePending);

	/** Renames this input to the name specified. */
	void RenameInput(FString NewName);

	/** Gets the namespaces which new parameters for this input can be read from. */
	void GetNamespacesForNewParameters(TArray<FString>& OutNamespacesForNewParameters) const;

	/** Gets a multicast delegate which is called whenever the value on this input changes. */
	FOnValueChanged& OnValueChanged();

	/** Gets a multicast delegate which is called whenever the pinned state of this input changes. */
	FOnPinnedChanged& OnPinnedChanged();

protected:
	//~ UNiagaraStackEntry interface
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	struct FDataValues
	{
		enum class EDefaultValueOwner
		{
			LocallyOwned,
			FunctionOwned,
			Invalid
		};

		FDataValues()
			: ValueObject(nullptr)
			, DefaultValueObject(nullptr)
			, DefaultValueOwner(EDefaultValueOwner::Invalid)
			, bIsValid(false)
		{
		}

		FDataValues(UNiagaraDataInterface* InValueObject, UNiagaraDataInterface* InDefaultValueObject, EDefaultValueOwner InDefaultValueOwner)
			: ValueObject(InValueObject)
			, DefaultValueObject(InDefaultValueObject)
			, DefaultValueOwner(InDefaultValueOwner)
			, bIsValid(true)
		{
			checkf(DefaultValueObject == nullptr || DefaultValueOwner != EDefaultValueOwner::Invalid, TEXT("Must specify a valid owner if the default value object is not null"));
		}

		UNiagaraDataInterface* GetValueObject() const
		{
			return ValueObject;
		}

		UNiagaraDataInterface* GetDefaultValueObject() const
		{
			return DefaultValueObject;
		}

		UNiagaraDataInterface*& GetDefaultValueObjectRef()
		{
			return DefaultValueObject;
		}

		EDefaultValueOwner GetDefaultValueOwner() const
		{
			return DefaultValueOwner;
		}

		bool IsValid() const
		{
			return bIsValid;
		}

	private:
		UNiagaraDataInterface* ValueObject;
		UNiagaraDataInterface* DefaultValueObject;
		EDefaultValueOwner DefaultValueOwner;
		bool bIsValid;
	};

	struct FInputValues
	{
		FInputValues()
			: Mode(EValueMode::Invalid)
		{
		}

		TSharedPtr<FStructOnScope> GetLocalStructToReuse();
		UNiagaraDataInterface* GetDataDefaultValueObjectToReuse();

		EValueMode Mode;
		TSharedPtr<FStructOnScope> LocalStruct;
		FNiagaraParameterHandle LinkedHandle;
		TWeakObjectPtr<UNiagaraNodeFunctionCall> DynamicNode;
		FDataValues DataObjects;
	};

private:
	/** Refreshes the current values for this input from the state of the graph. */
	void RefreshValues();

	/** Called whenever the graph which generated this input changes. */
	void OnGraphChanged(const struct FEdGraphEditAction& InAction);

	/** Gets the graph node which owns the local overrides for the module that owns this input if it exists. */
	UNiagaraNodeParameterMapSet* GetOverrideNode() const;

	/** Gets the graph node which owns the local overrides for the module that owns this input this input.  
	  * This will create the node and add it to the graph if it doesn't exist. */
	UNiagaraNodeParameterMapSet& GetOrCreateOverrideNode();

	/** Gets the default value pin from the map get node which generated this input. */
	UEdGraphPin* GetDefaultPin() const;

	/** Gets the pin on the override node which is associated with this input if it exists. */
	UEdGraphPin* GetOverridePin() const;

	/** Gets the pin on the override node which is associated with this input.  If either the override node or
	  * pin don't exist, they will be created. */
	UEdGraphPin& GetOrCreateOverridePin();

	/** Tries to get a local value for this input if it exists by checking the graph data directly. */
	bool TryGetCurrentLocalValue(TSharedPtr<FStructOnScope>& LocalValue, UEdGraphPin& ValuePin, TSharedPtr<FStructOnScope> OldValueToReuse);

	/** Tries to get a data interface value for this input if it exists by checking the graph data directly .*/
	bool TryGetCurrentDataValue(FDataValues& DataValues, UEdGraphPin* OverrideValuePin, UEdGraphPin& DefaultValuePin, UNiagaraDataInterface* LocallyOwnedDefaultDataValueObjectToReuse);

	/** Tries to get the linked value parameter handle for this input if it exists by checking the graph directly. */
	bool TryGetCurrentLinkedValue(FNiagaraParameterHandle& LinkedValue, UEdGraphPin& ValuePin);

	/** Gets the dynamic input node providing a value to this input if one exists. */
	bool TryGetCurrentDynamicValue(TWeakObjectPtr<UNiagaraNodeFunctionCall>& DynamicValue, UEdGraphPin* OverridePin);

	/** Recursively removes all nodes connected to the override pin. */
	static void RemoveAllNodesConnectedToOverridePin(UEdGraphPin& OverridePin, UNiagaraStackFunctionInput* OwningInput);

private:
	/** The stack editor data for this function input. */
	UNiagaraStackEditorData* StackEditorData;

	/** The module function call which owns this input entry. NOTE: This input might not be an input to the module function
		call, it may be an input to a dynamic input function call which is owned by the module. */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> OwningModuleNode;

	/** The function call which this entry is an input to. NOTE: This node can be a module function call node or a dynamic input node. */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> OwningFunctionCallNode;

	/** The assignment node which owns this input.  This is only valid for inputs of assignment modules. */
	TWeakObjectPtr<UNiagaraNodeAssignment> OwningAssignmentNode;

	/** The nigara type definition for this input. */
	FNiagaraTypeDefinition InputType;

	/** Whether or not this input can be pinned. */
	bool bCanBePinned;

	/** A unique key for this input for looking up editor only UI data. */
	FString StackEditorDataKey;

	/** An array representing the path of Namespace.Name handles starting from the owning module to this function input. */
	TArray<FNiagaraParameterHandle> InputParameterHandlePath;

	/** The parameter handle which defined this input in the module graph. */
	FNiagaraParameterHandle InputParameterHandle;

	/** The parameter handle which defined this input in the module graph, aliased for use in the current emitter
	  * graph.  This only affects parameter handles which are local module handles. */
	FNiagaraParameterHandle AliasedInputParameterHandle;

	/** The name of this input for display in the UI. */
	FText DisplayName;

	/** Pointers and handles to the various values this input can have. */
	FInputValues InputValues;

	/** A cached pointer to the override node for this input if it exists.  This value is cached here since the
	  * UI reads this value every frame due to attribute updates. */
	mutable TOptional<UNiagaraNodeParameterMapSet*> OverrideNodeCache;

	/** A cached pointer to the override pin for this input if it exists.  This value is cached here since the
	  * UI reads this value every frame due to attribute updates. */
	mutable TOptional<UEdGraphPin*> OverridePinCache;

	/** A flag to prevent handling graph changes when it's being updated directly by this object. */
	bool bUpdatingGraphDirectly;

	/** A handle for removing the graph changed delegate. */
	FDelegateHandle GraphChangedHandle;

	/** A multicast delegate which is called when the value of this input is changed. */
	FOnValueChanged ValueChangedDelegate;

	/** A multicast delegate which is called when the pinned state of this input is changed. */
	FOnPinnedChanged PinnedChangedDelegate;

	/** The item indent level for this stack entry. */
	int32 ItemIndentLevel;
};