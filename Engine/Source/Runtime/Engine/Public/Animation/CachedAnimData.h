// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CachedAnimData.generated.h"

class UAnimInstance;

/**
 * This file contains a number of helper structures that can be used to process state-machine-
 * related data in C++. This includes relevancy, state weights, animation time etc.
 */

USTRUCT(BlueprintType)
struct ENGINE_API FCachedAnimStateData
{
	GENERATED_USTRUCT_BODY()

	FCachedAnimStateData()
		: MachineIndex(INDEX_NONE)
		, StateIndex(INDEX_NONE)
		, bInitialized(false)
	{}

	/** Name of StateMachine State is in */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateMachineName;

	/** Name of State to Cache */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateName;

	/** Did it find a matching StateMachine and State in the AnimGraph? */
	bool IsValid(UAnimInstance& InAnimInstance) const;
	
	/** Is the State Machine relevant? (Has any weight) */
	float IsMachineRelevant(UAnimInstance& InAnimInstance) const;
	
	/** Global weight of state in AnimGraph */
	float GetGlobalWeight(UAnimInstance& InAnimInstance) const;

	/** Local weight of state inside of state machine. */
	float GetWeight(UAnimInstance& InAnimInstance) const;

	/** Is State Full Weight? */
	bool IsFullWeight(UAnimInstance& InAnimInstance) const;

	/** Is State relevant? */
	bool IsRelevant(UAnimInstance& InAnimInstance) const;

	/** Is State active? */
	bool IsActiveState(class UAnimInstance& InAnimInstance) const;

private:
	mutable int32 MachineIndex;
	mutable int32 StateIndex;
	mutable bool bInitialized;
};

USTRUCT(BlueprintType)
struct ENGINE_API FCachedAnimStateArray
{
	GENERATED_USTRUCT_BODY()

	FCachedAnimStateArray()
		: bCheckedValidity(false)
		, bCachedIsValid(true)
	{}

	/** Array of states */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	TArray<FCachedAnimStateData> States;

	float GetTotalWeight(UAnimInstance& InAnimInstance) const;
	bool IsFullWeight(UAnimInstance& InAnimInstance) const;
	bool IsRelevant(UAnimInstance& InAnimInstance) const;

private:
	bool IsValid(UAnimInstance& InAnimInstance) const;
	mutable bool bCheckedValidity;
	mutable bool bCachedIsValid;
};

USTRUCT(BlueprintType)
struct ENGINE_API FCachedAnimAssetPlayerData
{
	GENERATED_USTRUCT_BODY()

	FCachedAnimAssetPlayerData()
		: Index(INDEX_NONE)
		, bInitialized(false)
	{}

	/** Name of StateMachine State is in */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateMachineName;

	/** Name of State to Cache */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateName;

	float GetAssetPlayerTime(UAnimInstance& InAnimInstance) const;

	float GetAssetPlayerTimeRatio(UAnimInstance& InAnimInstance) const;

private:
	void CacheIndices(UAnimInstance& InAnimInstance) const;

	mutable int32 Index;
	mutable bool bInitialized;
};

USTRUCT(BlueprintType)
struct ENGINE_API FCachedAnimRelevancyData
{
	GENERATED_USTRUCT_BODY()

	FCachedAnimRelevancyData()
		: MachineIndex(INDEX_NONE)
		, StateIndex(INDEX_NONE)
		, bInitialized(false)
	{}

	/** Name of StateMachine State is in */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateMachineName;

	/** Name of State to Cache */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateName;

	float GetRelevantAnimTime(UAnimInstance& InAnimInstance) const;
	float GetRelevantAnimTimeRemaining(UAnimInstance& InAnimInstance) const;
	float GetRelevantAnimTimeRemainingFraction(UAnimInstance& InAnimInstance) const;

private:
	void CacheIndices(UAnimInstance& InAnimInstance) const;

private:
	mutable int32 MachineIndex;
	mutable int32 StateIndex;
	mutable bool bInitialized;
};

USTRUCT(BlueprintType)
struct ENGINE_API FCachedAnimTransitionData
{
	GENERATED_USTRUCT_BODY()

	FCachedAnimTransitionData()
		: MachineIndex(INDEX_NONE)
		, TransitionIndex(INDEX_NONE)
		, bInitialized(false)
	{}

	/** Name of StateMachine State is in */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName StateMachineName;

	/** Name of From State to Cache */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName FromStateName;

	/** Name of To State to Cache */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "State Machine")
	FName ToStateName;

	float GetCrossfadeDuration(UAnimInstance& InAnimInstance) const;

private:
	void CacheIndices(UAnimInstance& InAnimInstance) const;

private:
	mutable int32 MachineIndex;
	mutable int32 TransitionIndex;
	mutable bool bInitialized;
};