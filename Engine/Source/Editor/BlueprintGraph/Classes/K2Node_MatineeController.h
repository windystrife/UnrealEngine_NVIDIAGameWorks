// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "K2Node_MatineeController.generated.h"

class AActor;
class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
class UEdGraphPin;

UCLASS(MinimalAPI)
class UK2Node_MatineeController : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** The matinee actor in the level that this node controls */
	UPROPERTY(EditAnywhere, Category=K2Node_MatineeController)
	class AMatineeActor* MatineeActor;

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void PreloadRequiredAssets() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanDuplicateNode() const override { return false; }
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual AActor* GetReferencedLevelActor() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	/** Gets the "finished playing matinee sequence" pin */
	UEdGraphPin* GetFinishedPin() const;

private:
	void OnEventKeyframeAdded(const class AMatineeActor* InMatineeActor, const FName& InPinName, int32 InIndex);
	void OnEventKeyframeRenamed(const class AMatineeActor* InMatineeActor, const FName& InOldPinName, const FName& InNewPinName);
	void OnEventKeyframeRemoved(const class AMatineeActor* InMatineeActor, const TArray<FName>& InPinNames);
};

