// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Vector.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Vector"))
class AIMODULE_API UBlackboardKeyType_Vector : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FVector FDataType; 
	static const FDataType InvalidValue;
	
	static FVector GetValue(const UBlackboardKeyType_Vector* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Vector* KeyOb, uint8* RawData, const FVector& Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

protected:
	virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* RawData) override;
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool GetLocation(const UBlackboardComponent& OwnerComp, const uint8* RawData, FVector& Location) const override;
	virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
