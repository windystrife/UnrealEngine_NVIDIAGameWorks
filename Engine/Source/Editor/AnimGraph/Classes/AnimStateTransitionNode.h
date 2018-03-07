// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimStateMachineTypes.h"
#include "AnimStateNodeBase.h"

#include "AnimStateTransitionNode.generated.h"

class UBlendProfile;
class UCurveFloat;
class UEdGraph;
class UEdGraphPin;

UCLASS(MinimalAPI, config=Editor)
class UAnimStateTransitionNode : public UAnimStateNodeBase
{
	GENERATED_UCLASS_BODY()

	// The transition logic graph for this transition (returning a boolean)
	UPROPERTY()
	class UEdGraph* BoundGraph;

	// The animation graph for this transition if it uses custom blending (returning a pose)
	UPROPERTY()
	class UEdGraph* CustomTransitionGraph;

	// The priority order of this transition. If multiple transitions out of a state go
	// true at the same time, the one with the smallest priority order will take precedent
	UPROPERTY(EditAnywhere, Category=Transition)
	int32 PriorityOrder;

	// The duration to cross-fade for
	UPROPERTY(EditAnywhere, Config, Category=Transition)
	float CrossfadeDuration;

	// The type of blending to use in the crossfade
	UPROPERTY()
	TEnumAsByte<ETransitionBlendMode::Type> CrossfadeMode_DEPRECATED;

	UPROPERTY(EditAnywhere, Category=Transition)
	EAlphaBlendOption BlendMode;

	UPROPERTY(EditAnywhere, Category=Transition)
	UCurveFloat* CustomBlendCurve;

	// The blend profile to use to evaluate this transition per-bone
	UPROPERTY(EditAnywhere, Category=Transition)
	UBlendProfile* BlendProfile;

	// Try setting the rule automatically based on most relevant player node's remaining time and the CrossfadeDuration of the transition, ignoring the internal time
	UPROPERTY(EditAnywhere, Category=Transition)
	bool bAutomaticRuleBasedOnSequencePlayerInState;

	// What transition logic to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transition)
	TEnumAsByte<ETransitionLogicType::Type> LogicType;

	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent TransitionStart;

	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent TransitionEnd;

	UPROPERTY(EditAnywhere, Category=Events)
	FAnimNotifyEvent TransitionInterrupt;

	/** This transition can go both directions */
	UPROPERTY(EditAnywhere, Category=Transition)
	bool Bidirectional;

	/** The rules for this transition may be shared with other transition nodes */
	UPROPERTY()
	bool bSharedRules;

	/** The cross-fade settings of this node may be shared */
	UPROPERTY()
	bool bSharedCrossfade;

	/** What we call this transition if we are shared ('Transition X to Y' can't be used since its used in multiple places) */
	UPROPERTY()
	FString	SharedRulesName;

	/** Shared rules guid useful when copying between different state machines */
	UPROPERTY()
	FGuid SharedRulesGuid;

	/** Color we draw in the editor as if we are shared */
	UPROPERTY()
	FLinearColor SharedColor;

	UPROPERTY()
	FString SharedCrossfadeName;

	UPROPERTY()
	FGuid SharedCrossfadeGuid;

	UPROPERTY()
	int32 SharedCrossfadeIdx;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool CanDuplicateNode() const override { return true; }
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;
	virtual void PostPlacedNewNode() override;
	virtual void DestroyNode() override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UAnimStateNodeBase Interface
	virtual UEdGraph* GetBoundGraph() const override { return BoundGraph; }
	virtual void ClearBoundGraph() override { BoundGraph = nullptr; }
	virtual UEdGraphPin* GetInputPin() const override { return Pins[0]; }
	virtual UEdGraphPin* GetOutputPin() const override { return Pins[1]; }
	//~ End UAnimStateNodeBase Interface

	// @return the name of this state
	ANIMGRAPH_API FString GetStateName() const override;

	ANIMGRAPH_API UAnimStateNodeBase* GetPreviousState() const;
	ANIMGRAPH_API UAnimStateNodeBase* GetNextState() const;
	ANIMGRAPH_API void CreateConnections(UAnimStateNodeBase* PreviousState, UAnimStateNodeBase* NextState);
	
	ANIMGRAPH_API bool IsBoundGraphShared();

	ANIMGRAPH_API void MakeRulesShareable(FString ShareName);
	ANIMGRAPH_API void MakeCrossfadeShareable(FString ShareName);

	ANIMGRAPH_API void UnshareRules();
	ANIMGRAPH_API void UnshareCrossade();

	ANIMGRAPH_API void UseSharedRules(const UAnimStateTransitionNode* Node);
	ANIMGRAPH_API void UseSharedCrossfade(const UAnimStateTransitionNode* Node);
	
	void CopyCrossfadeSettings(const UAnimStateTransitionNode* SrcNode);
	void PropagateCrossfadeSettings();

	ANIMGRAPH_API bool IsReverseTrans(const UAnimStateNodeBase* Node);
protected:
	void CreateBoundGraph();
	void CreateCustomTransitionGraph();

public:
	UEdGraph* GetCustomTransitionGraph() const { return CustomTransitionGraph; }
};
