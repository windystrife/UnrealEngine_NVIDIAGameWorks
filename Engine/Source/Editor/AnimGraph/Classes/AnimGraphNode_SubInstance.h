// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_SubInstance.h"

#include "AnimGraphNode_SubInstance.generated.h"

class FCompilerResultsLog;
class IDetailLayoutBuilder;
class IPropertyHandle;

UCLASS(MinimalAPI)
class UAnimGraphNode_SubInstance : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_SubInstance Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput = NULL) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	//~ End UEdGraphNode Interface.

	// Gets the property on InOwnerInstanceClass that corresponds to InInputPin
	void ANIMGRAPH_API GetInstancePinProperty(const UClass* InOwnerInstanceClass, UEdGraphPin* InInputPin, UProperty*& OutProperty);

	// Gets the unique name for the property linked to a given pin
	FString ANIMGRAPH_API GetPinTargetVariableName(const UEdGraphPin* InPin) const;

	// Detail panel customizations
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	/** List of property names we know to exist on the target class, so we can detect when
	 *  Properties are added or removed on reconstruction
	 */
	UPROPERTY()
	TArray<FName> KnownExposableProperties;

	/** Names of properties the user has chosen to expose */
	UPROPERTY()
	TArray<FName> ExposedPropertyNames;

	// Searches the instance class for properties that we can expose (public and BP visible)
	void GetExposableProperties(TArray<UProperty*>& OutExposableProperties) const;
	// Gets a property's type as FText (for UI)
	FText GetPropertyTypeText(UProperty* Property);
	// Given a new class, rebuild the known property list (for tracking class changes and moving pins)
	void RebuildExposedProperties(UClass* InNewClass);

	// Finds out whether there is a loop in the graph formed by sub instances from this node
	bool HasInstanceLoop();

	// Finds out whether there is a loop in the graph formed by sub instances from CurrNode, used by HasInstanceLoop. VisitedNodes and NodeStack are required
	// to track the graph links
	// VisitedNodes - Node we have searched the links of, so we don't do it twice
	// NodeStack - The currently considered chain of nodes. If a loop is detected this will contain the chain that causes the loop
	static bool HasInstanceLoop_Recursive(UAnimGraphNode_SubInstance* CurrNode, TArray<FGuid>& VisitedNodes, TArray<FGuid>& NodeStack);

	// ----- UI CALLBACKS ----- //
	// If given property exposed on this node
	ECheckBoxState IsPropertyExposed(FName PropertyName) const;
	// User chose to expose, or unexpose a property
	void OnPropertyExposeCheckboxChanged(ECheckBoxState NewState, FName PropertyName);
	// User changed the instance class
	void OnInstanceClassChanged(IDetailLayoutBuilder* DetailBuilder);

	// Gets path to the currently selected instance class' blueprint
	FString GetCurrentInstanceBlueprintPath() const;
	// Filter callback for blueprints (only accept matching skeletons)
	bool OnShouldFilterInstanceBlueprint(const FAssetData& AssetData) const;
	// Instance blueprint was changed by user
	void OnSetInstanceBlueprint(const FAssetData& AssetData, TSharedRef<IPropertyHandle> InstanceClassPropHandle);
	// ----- END UI CALLBACKS ----- //
};
