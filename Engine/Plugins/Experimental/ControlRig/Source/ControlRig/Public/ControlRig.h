// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Animation/ControlRigInterface.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/SubclassOf.h"
#include "ControlRig.generated.h"

/** Delegate used to optionally gather inputs before evaluating a ControlRig */
DECLARE_DYNAMIC_DELEGATE(FPreEvaluateGatherInputs);

/** Runs logic for mapping input data to transforms (the "Rig") */
UCLASS(Blueprintable, Abstract, editinlinenew)
class CONTROLRIG_API UControlRig : public UObject, public IControlRigInterface
{
	GENERATED_BODY()

	friend class UK2Node_ControlRig;
	friend class UK2Node_ControlRigOutput;
	friend class UK2Node_ControlRigEvaluator;
	friend class UControlRigComponent;

public:
	static const FName AnimationInputMetaName;
	static const FName AnimationOutputMetaName;

private:
	/** Current delta time */
	float DeltaTime;

	/** Allocated sub-instances of ControlRigs */
	UPROPERTY(Transient)
	TArray<UControlRig*> SubControlRigs;

public:
	UControlRig();

	/** Get the current delta time */
	UFUNCTION(BlueprintPure, Category = "Animation")
	float GetDeltaTime() const;

	/** Set the current delta time */
	void SetDeltaTime(float InDeltaTime);

	/** Find the actor we are bound to, if any */
	virtual AActor* GetHostingActor() const;

#if WITH_EDITOR
	/** Get the category of this ControlRig (for display in menus) */
	virtual FText GetCategory() const;

	/** Get the tooltip text to display for this node (displayed in graphs and from context menus) */
	virtual FText GetTooltipText() const;
#endif

	/** UObject interface */
	virtual UWorld* GetWorld() const override;

	/** Initialize things for the ControlRig */
	virtual void Initialize();

	/** Bind to a runtime object */
	virtual void BindToObject(UObject* InObject);

	/** Unbind from the current bound runtime object */
	virtual void UnbindFromObject();

	/** 
	 * Check whether we are bound to the supplied object. 
	 * This can be distinct from a direct pointer comparison (e.g. in the case of an actor passed
	 * to BindToObject, we may actually bind to one of its components).
	 */
	virtual bool IsBoundToObject(UObject* InObject) const;

	/** Get the current object we are bound to */
	virtual UObject* GetBoundObject() const;

	/** IControlRigInterface implementation */
	virtual void PreEvaluate() override;
	virtual void Evaluate() override;
	virtual void PostEvaluate() override;

protected:
	/** Get the host of this animation ControlRig */
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintInternalUseOnly = "true"))
	UControlRig* GetOrAllocateSubControlRig(TSubclassOf<UControlRig> ControlRigClass, int32 AllocationIndex);

	/** Initialize event for blueprints to use */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animation", meta = (CallInEditor = "true"))
	void OnInitialize();

	/** Evaluate event for blueprints to use */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animation", meta = (BlueprintInternalUseOnly = "true", CallInEditor = "true"))
	void OnEvaluate();

	/** Evaluate another animation ControlRig */
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintInternalUseOnly = "true"))
	static UControlRig* EvaluateControlRig(UControlRig* Target);

	/** Evaluate another animation ControlRig with inputs. */
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintInternalUseOnly = "true"))
	static UControlRig* EvaluateControlRigWithInputs(UControlRig* Target, FPreEvaluateGatherInputs PreEvaluate);

	/** Get any components we should depend on */
	virtual void GetTickDependencies(TArray<FTickPrerequisite, TInlineAllocator<1>>& OutTickPrerequisites) {}
};