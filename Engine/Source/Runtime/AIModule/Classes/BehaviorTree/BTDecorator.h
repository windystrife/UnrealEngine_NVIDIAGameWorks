// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTAuxiliaryNode.h"
#include "BTDecorator.generated.h"

class FBehaviorDecoratorDetails;

enum class EBTDecoratorAbortRequest : uint8
{
	// request execution update when only result of condition changes and active branch of tree can potentially change too
	ConditionResultChanged,

	// request execution update every time as long as condition is still passing
	ConditionPassing,
};

/** 
 * Decorators are supporting nodes placed on parent-child connection, that receive notification about execution flow and can be ticked
 *
 * Because some of them can be instanced for specific AI, following virtual functions are not marked as const:
 *  - OnNodeActivation
 *  - OnNodeDeactivation
 *  - OnNodeProcessed
 *  - OnBecomeRelevant (from UBTAuxiliaryNode)
 *  - OnCeaseRelevant (from UBTAuxiliaryNode)
 *  - TickNode (from UBTAuxiliaryNode)
 *
 * If your node is not being instanced (default behavior), DO NOT change any properties of object within those functions!
 * Template nodes are shared across all behavior tree components using the same tree asset and must store
 * their runtime properties in provided NodeMemory block (allocation size determined by GetInstanceMemorySize() )
 *
 */

UCLASS(Abstract)
class AIMODULE_API UBTDecorator : public UBTAuxiliaryNode
{
	GENERATED_UCLASS_BODY()

	/** wrapper for node instancing: CalculateRawConditionValue */
	bool WrappedCanExecute(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;

	/** wrapper for node instancing: OnNodeActivation  */
	void WrappedOnNodeActivation(FBehaviorTreeSearchData& SearchData) const;
	
	/** wrapper for node instancing: OnNodeDeactivation */
	void WrappedOnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) const;

	/** wrapper for node instancing: OnNodeProcessed */
	void WrappedOnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult) const;

	/** @return flow controller's abort mode */
	EBTFlowAbortMode::Type GetFlowAbortMode() const;

	/** @return true if condition should be inversed */
	bool IsInversed() const;

	virtual FString GetStaticDescription() const override;

	/** modify current flow abort mode, so it can be used with parent composite */
	void UpdateFlowAbortMode();

	/** @return true if current abort mode can be used with parent composite */
	bool IsFlowAbortModeValid() const;

protected:

	/** if set, FlowAbortMode can be set to None */
	uint32 bAllowAbortNone : 1;

	/** if set, FlowAbortMode can be set to LowerPriority and Both */
	uint32 bAllowAbortLowerPri : 1;

	/** if set, FlowAbortMode can be set to Self and Both */
	uint32 bAllowAbortChildNodes : 1;

	/** if set, OnNodeActivation will be used */
	uint32 bNotifyActivation : 1;

	/** if set, OnNodeDeactivation will be used */
	uint32 bNotifyDeactivation : 1;

	/** if set, OnNodeProcessed will be used */
	uint32 bNotifyProcessed : 1;

	/** if set, static description will include default description of inversed condition */
	uint32 bShowInverseConditionDesc : 1;

private:
	/** if set, condition check result will be inversed */
	UPROPERTY(Category = Condition, EditAnywhere)
	uint32 bInverseCondition : 1;

protected:
	/** flow controller settings */
	UPROPERTY(Category=FlowControl, EditAnywhere)
	TEnumAsByte<EBTFlowAbortMode::Type> FlowAbortMode;

	void SetIsInversed(bool bShouldBeInversed);

	/** called when underlying node is activated
	  * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData);

	/** called when underlying node has finished
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult);

	/** called when underlying node was processed (deactivated or failed to activate)
	 * this function should be considered as const (don't modify state of object) if node is not instanced! */
	virtual void OnNodeProcessed(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult);

	/** calculates raw, core value of decorator's condition. Should not include calling IsInversed */
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;

	/** more "flow aware" version of calling RequestExecution(this) on owning behavior tree component
	 *  should be used in external events that may change result of CalculateRawConditionValue
	 */
	void ConditionalFlowAbort(UBehaviorTreeComponent& OwnerComp, EBTDecoratorAbortRequest RequestMode) const;

	friend FBehaviorDecoratorDetails;

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
	DEPRECATED(4.12, "This function is deprecated, please use InitializeParentLink instead.")
	void InitializeDecorator(uint8 InChildIndex);
};


//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE EBTFlowAbortMode::Type UBTDecorator::GetFlowAbortMode() const
{
	return FlowAbortMode;
}

FORCEINLINE bool UBTDecorator::IsInversed() const
{
	return bInverseCondition;
}
