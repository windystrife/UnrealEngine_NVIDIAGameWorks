// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_CallFunction.h"
#include "BlueprintActionFilter.h"
#include "K2Node_AnimGetter.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UAnimBlueprint;
class UAnimGraphNode_Base;
class UAnimStateNodeBase;

USTRUCT()
struct FNodeSpawnData
{
	GENERATED_BODY()

	FNodeSpawnData();

	// Title to use for the spawned node
	UPROPERTY()
	FText CachedTitle;

	// The node the spawned getter accesses
	UPROPERTY()
	UAnimGraphNode_Base* SourceNode;

	// The state node the spawned getter accesses
	UPROPERTY()
	UAnimStateNodeBase* SourceStateNode;

	// The instance class the spawned getter is defined on
	UPROPERTY()
	UClass* AnimInstanceClass;

	// The blueprint the getter is valid within
	UPROPERTY()
	const UAnimBlueprint* SourceBlueprint;

	// The UFunction (as a UField) 
	UPROPERTY()
	UField* Getter;

	// String of combined valid contexts for the spawned getter
	UPROPERTY()
	FString GetterContextString;
};

UCLASS(MinimalAPI)
class UK2Node_AnimGetter : public UK2Node_CallFunction
{
	GENERATED_BODY()
public:

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// end of UK2Node interface

	// The node that is required for the getter
	UPROPERTY()
	UAnimGraphNode_Base* SourceNode;

	// UAnimStateNode doesn't use the same hierarchy so we need to have a seperate property here to handle
	// those.
	UPROPERTY()
	UAnimStateNodeBase* SourceStateNode;

	// The UAnimInstance derived class that implements the getter we are running
	UPROPERTY()
	UClass* GetterClass;

	// The anim blueprint that generated this getter
	UPROPERTY()
	const UAnimBlueprint* SourceAnimBlueprint;

	// Cached node title
	UPROPERTY()
	FText CachedTitle;

	// List of valid contexts for the node
	UPROPERTY()
	TArray<FString> Contexts;

protected:
	
	//UFunction* GetSourceBlueprintFunction() const;

	/** Returns whether or not the provided UFunction requires the named parameter */
	bool GetterRequiresParameter(const UFunction* Getter, FString ParamName) const;

	/** Checks the cached context strings to make sure this getter is valid within the provided schema */
	bool IsContextValidForSchema(const UEdGraphSchema* Schema) const;

	/** Passed to blueprint spawners to configure spawned nodes */
	void PostSpawnNodeSetup(UEdGraphNode* NewNode, bool bIsTemplateNode, FNodeSpawnData SpawnData);
};
