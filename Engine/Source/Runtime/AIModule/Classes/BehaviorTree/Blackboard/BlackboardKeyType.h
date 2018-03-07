// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BlackboardKeyType.generated.h"

class UBlackboardComponent;

namespace EBlackboardCompare
{
	enum Type
	{
		Less = -1, 	
		Equal = 0, 
		Greater = 1,

		NotEqual = 1,	// important, do not change
	};
}

namespace EBlackboardKeyOperation
{
	enum Type
	{
		Basic,
		Arithmetic,
		Text,
	};
}

UENUM()
namespace EBasicKeyOperation
{
	enum Type
	{
		Set				UMETA(DisplayName="Is Set"),
		NotSet			UMETA(DisplayName="Is Not Set"),
	};
}

UENUM()
namespace EArithmeticKeyOperation
{
	enum Type
	{
		Equal			UMETA(DisplayName="Is Equal To"),
		NotEqual		UMETA(DisplayName="Is Not Equal To"),
		Less			UMETA(DisplayName="Is Less Than"),
		LessOrEqual		UMETA(DisplayName="Is Less Than Or Equal To"),
		Greater			UMETA(DisplayName="Is Greater Than"),
		GreaterOrEqual	UMETA(DisplayName="Is Greater Than Or Equal To"),
	};
}

UENUM()
namespace ETextKeyOperation
{
	enum Type
	{
		Equal			UMETA(DisplayName="Is Equal To"),
		NotEqual		UMETA(DisplayName="Is Not Equal To"),
		Contain			UMETA(DisplayName="Contains"),
		NotContain		UMETA(DisplayName="Not Contains"),
	};
}

struct FBlackboardInstancedKeyMemory
{
	/** index of instanced key in UBlackboardComponent::InstancedKeys */
	int32 KeyIdx;
};

UCLASS(EditInlineNew, Abstract, CollapseCategories, AutoExpandCategories=(Blackboard))
class AIMODULE_API UBlackboardKeyType : public UObject
{
	GENERATED_UCLASS_BODY()

	/** handle dynamic data size */
	virtual void PreInitialize(UBlackboardComponent& OwnerComp);

	/** handle instancing if needed */
	void InitializeKey(UBlackboardComponent& OwnerComp, FBlackboard::FKey KeyID);

	/** does it match settings in filter? */
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const;

	/** extract location from entry, supports instanced keys */
	bool WrappedGetLocation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, FVector& Location) const;

	/** extract rotation from entry, supports instanced keys */
	bool WrappedGetRotation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, FRotator& Rotation) const;

	/** free value before removing from blackboard, supports instanced keys */
	void WrappedFree(UBlackboardComponent& OwnerComp, uint8* MemoryBlock);

	/** sets value to the default, supports instanced keys */
	void WrappedClear(const UBlackboardComponent& OwnerComp, uint8* MemoryBlock) const;

	/** check if key has stored value, supports instanced keys */
	bool WrappedIsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;

	/** various value testing, used by decorators, supports instanced keys */
	bool WrappedTestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const;
	bool WrappedTestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const;
	bool WrappedTestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const;

	/** describe params of arithmetic test */
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const;

	/** convert value to text, supports instanced keys */
	FString WrappedDescribeValue(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;

	/** description of params for property view */
	virtual FString DescribeSelf() const;

	/** create replacement key for deprecated data */
	virtual UBlackboardKeyType* UpdateDeprecatedKey();

	/** @return key instance if bCreateKeyInstance was set */
	const UBlackboardKeyType* GetKeyInstance(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;
	UBlackboardKeyType* GetKeyInstance(UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;

	/** compares two values */
	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const;

	/** @return true if key wants to be instanced */
	bool HasInstance() const;

	/** @return true if this object is instanced key */
	bool IsInstanced() const;

	/** get ValueSize */
	uint16 GetValueSize() const;

	/** get test supported by this type */
	EBlackboardKeyOperation::Type GetTestOperation() const;

protected:

	/** size of value for this type */
	uint16 ValueSize;

	/** decorator operation supported with this type */
	TEnumAsByte<EBlackboardKeyOperation::Type> SupportedOp;

	/** set automatically for node instances */
	uint8 bIsInstanced : 1;

	/** if set, key will be instanced instead of using memory block */
	uint8 bCreateKeyInstance : 1;

	/** helper function for reading typed data from memory block */
	template<typename T>
	static T GetValueFromMemory(const uint8* MemoryBlock)
	{
		return *((T*)MemoryBlock);
	}

	/** helper function for writing typed data to memory block, returns true if value has changed */
	template<typename T>
	static bool SetValueInMemory(uint8* MemoryBlock, const T& Value)
	{
		const bool bChanged = *((T*)MemoryBlock) != Value;
		*((T*)MemoryBlock) = Value;
		
		return bChanged;
	}

	/** helper function for witting weak object data to memory block, returns true if value has changed */
	template<typename T>
	static bool SetWeakObjectInMemory(uint8* MemoryBlock, const TWeakObjectPtr<T>& Value)
	{
		TWeakObjectPtr<T>* PrevValue = (TWeakObjectPtr<T>*)MemoryBlock;
		const bool bChanged =
			(Value.IsValid(false, true) != PrevValue->IsValid(false, true)) ||
			(Value.IsStale(false, true) != PrevValue->IsStale(false, true)) ||
			(*PrevValue) != Value;

		*((TWeakObjectPtr<T>*)MemoryBlock) = Value;

		return bChanged;
	}

	friend UBlackboardComponent;
	
	/** copy value from other key, works directly on provided memory/properties */
	virtual void CopyValues(UBlackboardComponent& OwnerComp, uint8* MemoryBlock, const UBlackboardKeyType* SourceKeyOb, const uint8* SourceBlock);

	/** initialize memory, works directly on provided memory/properties */
	virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock);

	/** free value before removing from blackboard, works directly on provided memory/properties */
	virtual void FreeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock);

	/** extract location from entry, works directly on provided memory/properties */
	virtual bool GetLocation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, FVector& Location) const;
	
	/** extract rotation from entry, works directly on provided memory/properties */
	virtual bool GetRotation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, FRotator& Rotation) const;

	/** sets value to the default, works directly on provided memory/properties */
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock);

	/** check if key has stored value, works directly on provided memory/properties */
	virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;

	/** various value testing, works directly on provided memory/properties */
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const;
	virtual bool TestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const;
	virtual bool TestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const;

	/** convert value to text, works directly on provided memory/properties */
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE uint16 UBlackboardKeyType::GetValueSize() const
{
	return ValueSize;
}

FORCEINLINE EBlackboardKeyOperation::Type UBlackboardKeyType::GetTestOperation() const
{
	return SupportedOp; 
}

FORCEINLINE bool UBlackboardKeyType::HasInstance() const
{
	return bCreateKeyInstance;
}

FORCEINLINE bool UBlackboardKeyType::IsInstanced() const
{
	return bIsInstanced;
}
