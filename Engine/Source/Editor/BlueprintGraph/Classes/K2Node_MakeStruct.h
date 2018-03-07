// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_StructMemberSet.h"
#include "K2Node_MakeStruct.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

// Pure kismet node that creates a struct with specified values for each member
UCLASS(MinimalAPI)
class UK2Node_MakeStruct : public UK2Node_StructMemberSet
{
	GENERATED_UCLASS_BODY()

	/** Helper property to handle upgrades from an old system of displaying pins for
	 *	the override values that properties referenced as a conditional of being set in a struct */
	UPROPERTY()
	bool bMadeAfterOverridePinRemoval;

	/**
	* Returns false if:
	*   1. The Struct has a 'native make' method
	* Returns true if:
	*   1. The Struct is tagged as BlueprintType
	*   and
	*   2. The Struct has any property that is tagged as CPF_BlueprintVisible and is not CPF_BlueprintReadOnly
	*/
	BLUEPRINTGRAPH_API static bool CanBeMade(const UScriptStruct* Struct, bool bForInternalUse = false);
	
	/** Can this struct be used as a split pin */
	BLUEPRINTGRAPH_API static bool CanBeSplit(const UScriptStruct* Struct);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void PreloadRequiredAssets() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void PostPlacedNewNode() override;
	//~ End  UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return false; }
	virtual bool IsNodePure() const override { return true; }
	virtual bool DrawNodeAsVariable() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ConvertDeprecatedNode(UEdGraph* Graph, bool bOnlySafeChanges) override;
	//~ End K2Node Interface

protected:
	struct FMakeStructPinManager : public FStructOperationOptionalPinManager
	{
		const uint8* const SampleStructMemory;
	public:
		FMakeStructPinManager(const uint8* InSampleStructMemory);

		bool HasAdvancedPins() const { return bHasAdvancedPins; }
	protected:
		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override;
		virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override;
		virtual bool CanTreatPropertyAsOptional(UProperty* TestProperty) const override;

		/** set by GetRecordDefaults(), mutable as it is a const function */
		mutable bool bHasAdvancedPins;
	};

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};
