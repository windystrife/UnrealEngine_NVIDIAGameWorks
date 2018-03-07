// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "UObject/ObjectMacros.h"

#include "K2Node_GetSequenceBindings.generated.h"


struct FAssetData;
class UEdGraphPin;
class UMovieSceneSequence;


USTRUCT()
struct FGetSequenceBindingGuidMapping
{
	GENERATED_BODY()

	UPROPERTY()
	FString PinName;

	UPROPERTY()
	FGuid Guid;
};


UCLASS(MinimalAPI, deprecated)
class UDEPRECATED_K2Node_GetSequenceBindings
	: public UK2Node
{
public:
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGetSequenceBindingGuidMapping> BindingGuids;

	UPROPERTY(EditAnywhere, Category="Sequence")
	UMovieSceneSequence* Sequence;

	TOptional<FGuid> GetGuidFromPin(UEdGraphPin* Pin) const;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool IsNodePure() const override { return true; }
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	void SetSequence(const FAssetData& InAssetData);
	void UpdatePins();
};
