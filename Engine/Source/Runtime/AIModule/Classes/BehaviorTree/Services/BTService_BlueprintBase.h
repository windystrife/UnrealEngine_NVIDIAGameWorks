// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTService.h"
#include "BTService_BlueprintBase.generated.h"

class AActor;
class AAIController;
class APawn;
class UBehaviorTree;

/**
 *  Base class for blueprint based service nodes. Do NOT use it for creating native c++ classes!
 *
 *  When service receives Deactivation event, all latent actions associated this instance are being removed.
 *  This prevents from resuming activity started by Activation, but does not handle external events.
 *  Please use them safely (unregister at abort) and call IsServiceActive() when in doubt.
 */

UCLASS(Abstract, Blueprintable)
class AIMODULE_API UBTService_BlueprintBase : public UBTService
{
	GENERATED_UCLASS_BODY()

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual void OnInstanceDestroyed(UBehaviorTreeComponent& OwnerComp) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual void SetOwner(AActor* ActorOwner) override;

#if WITH_EDITOR
	virtual bool UsesBlueprint() const override;
#endif

protected:
	/** Cached AIController owner of BehaviorTreeComponent. */
	UPROPERTY(Transient)
	AAIController* AIOwner;

	/** Cached actor owner of BehaviorTreeComponent. */
	UPROPERTY(Transient)
	AActor* ActorOwner;

	// Gets the description for our service
	virtual FString GetStaticServiceDescription() const override;

	/** properties with runtime values, stored only in class default object */
	TArray<UProperty*> PropertyData;

	/** show detailed information about properties */
	UPROPERTY(EditInstanceOnly, Category=Description)
	uint32 bShowPropertyDetails : 1;

	/** show detailed information about implemented events */
	UPROPERTY(EditInstanceOnly, Category = Description)
	uint32 bShowEventDetails : 1;

	/** set if ReceiveTick is implemented by blueprint */
	uint32 ReceiveTickImplementations : 2;
	
	/** set if ReceiveActivation is implemented by blueprint */
	uint32 ReceiveActivationImplementations : 2;

	/** set if ReceiveDeactivation is implemented by blueprint */
	uint32 ReceiveDeactivationImplementations : 2;

	/** set if ReceiveSearchStart is implemented by blueprint */
	uint32 ReceiveSearchStartImplementations : 2;

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void OnSearchStart(FBehaviorTreeSearchData& SearchData) override;

	/** tick function
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveTick(AActor* OwnerActor, float DeltaSeconds);

	/** task search enters branch of tree
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveSearchStart(AActor* OwnerActor);

	/** service became active
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveActivation(AActor* OwnerActor);

	/** service became inactive
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveDeactivation(AActor* OwnerActor);

	/** Alternative AI version of ReceiveTick function.
	 *	@see ReceiveTick for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveTickAI(AAIController* OwnerController, APawn* ControlledPawn, float DeltaSeconds);

	/** Alternative AI version of ReceiveSearchStart function.
	 *	@see ReceiveSearchStart for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveSearchStartAI(AAIController* OwnerController, APawn* ControlledPawn);

	/** Alternative AI version of ReceiveActivation function.
	 *	@see ReceiveActivation for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveActivationAI(AAIController* OwnerController, APawn* ControlledPawn);

	/** Alternative AI version of ReceiveDeactivation function.
	 *	@see ReceiveDeactivation for more details
	 *	@Note that if both generic and AI event versions are implemented only the more
	 *	suitable one will be called, meaning the AI version if called for AI, generic one otherwise */
	UFUNCTION(BlueprintImplementableEvent, Category = AI)
	void ReceiveDeactivationAI(AAIController* OwnerController, APawn* ControlledPawn);

	/** check if service is currently being active */
	UFUNCTION(BlueprintCallable, Category="AI|BehaviorTree")
	bool IsServiceActive() const;
};
