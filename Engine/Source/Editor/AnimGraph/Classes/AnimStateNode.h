// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "AnimStateNodeBase.h"
#include "AnimStateNode.generated.h"

class UEdGraph;
class UEdGraphPin;

UENUM()
enum EAnimStateType
{
	AST_SingleAnimation UMETA(DisplayName="Single animation"),
	AST_BlendGraph UMETA(DisplayName="Blend graph"),
};


UCLASS(MinimalAPI)
class UAnimStateNode : public UAnimStateNodeBase
{
	GENERATED_UCLASS_BODY()
public:

	// The animation graph for this state
	UPROPERTY()
	class UEdGraph* BoundGraph;

	// The type of the contents of this state
	UPROPERTY()
	TEnumAsByte<EAnimStateType> StateType;

	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent StateEntered;
	
	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent StateLeft;

	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent StateFullyBlended;

	// Whether or not this state will ALWAYS reset it's state on reentry, regardless of remaining weight
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Events)
	bool bAlwaysResetOnEntry;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanDuplicateNode() const override { return true; }
	virtual void PostPasteNode() override;
	virtual void PostPlacedNewNode() override;
	virtual void DestroyNode() override;
	//~ End UEdGraphNode Interface
	
	//~ Begin UAnimStateNodeBase Interface
	virtual UEdGraphPin* GetInputPin() const override;
	virtual UEdGraphPin* GetOutputPin() const override;
	virtual FString GetStateName() const override;
	virtual void GetTransitionList(TArray<class UAnimStateTransitionNode*>& OutTransitions, bool bWantSortedList = false) override;
	//~ End UAnimStateNodeBase Interface

	// @return the pose pin of the state sink node within the anim graph of this state
	ANIMGRAPH_API UEdGraphPin* GetPoseSinkPinInsideState() const;

public:
	virtual UEdGraph* GetBoundGraph() const override { return BoundGraph; }
	virtual void ClearBoundGraph() override { BoundGraph = nullptr; }
};
