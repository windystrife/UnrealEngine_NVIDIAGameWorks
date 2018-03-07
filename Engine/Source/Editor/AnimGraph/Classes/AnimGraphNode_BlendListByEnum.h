// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "NodeDependingOnEnumInterface.h"
#include "AnimGraphNode_BlendListBase.h"
#include "AnimNodes/AnimNode_BlendListByEnum.h"
#include "AnimGraphNode_BlendListByEnum.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS(MinimalAPI)
class UAnimGraphNode_BlendListByEnum : public UAnimGraphNode_BlendListBase, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_BlendListByEnum Node;
	
protected:
	/** Name of the enum being switched on */
	UPROPERTY()
	UEnum* BoundEnum;

	UPROPERTY()
	TArray<FName> VisibleEnumEntries;
public:
	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	// End of UK2Node interface

	// UAnimGraphNode_Base interface
	virtual FString GetNodeCategory() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const override;
	virtual void ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog) override;
	virtual void BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog) override;
	virtual void PreloadRequiredAssets() override;
	// End of UAnimGraphNode_Base interface

	//@TODO: Generalize this behavior (returning a list of actions/delegates maybe?)
	virtual void RemovePinFromBlendList(UEdGraphPin* Pin);

	// INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return BoundEnum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override {return true;}
	// End of INodeDependingOnEnumInterface
protected:
	// Exposes a pin corresponding to the specified element name
	void ExposeEnumElementAsPin(FName EnumElementName);

	// Gets information about the specified pin.  If both bIsPosePin and bIsTimePin are false, the index is meaningless
	static void GetPinInformation(const FString& InPinName, int32& Out_PinIndex, bool& Out_bIsPosePin, bool& Out_bIsTimePin);

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};
