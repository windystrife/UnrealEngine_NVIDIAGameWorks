// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Int.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Int"))
class AIMODULE_API UBlackboardKeyType_Int : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef int32 FDataType;
	static const FDataType InvalidValue;

	static int32 GetValue(const UBlackboardKeyType_Int* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Int* KeyOb, uint8* RawData, int32 Value);

	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const override;
	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const override;
};
