// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Switch.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsK2Node_SwitchGameplayTag.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS(MinimalAPI)
class UGameplayTagsK2Node_SwitchGameplayTag : public UK2Node_Switch
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = PinOptions)
	TArray<FGameplayTag> PinTags;

	//UPROPERTY(EditAnywhere, Category = PinOptions)
	//bool UseInputsOnGraph;

	UPROPERTY()
	TArray<FName> PinNames;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	// UK2Node_Switch Interface
	GAMEPLAYTAGSEDITOR_API virtual void AddPinToSwitchNode() override;
	virtual FString GetUniquePinName() override;
	virtual FEdGraphPinType GetPinType() const override;
	GAMEPLAYTAGSEDITOR_API virtual FEdGraphPinType GetInnerCaseType() const override;
	// End of UK2Node_Switch Interface

	virtual FString GetPinNameGivenIndex(int32 Index) override;

protected:
	virtual void CreateFunctionPin() override;
	virtual void CreateSelectionPin() override;
	virtual void CreateCasePins() override;
	virtual void RemovePin(UEdGraphPin* TargetPin) override;
};
