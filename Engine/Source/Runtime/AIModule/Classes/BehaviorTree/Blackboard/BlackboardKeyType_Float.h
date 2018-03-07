// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Float.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Float"))
class AIMODULE_API UBlackboardKeyType_Float : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef float FDataType;
	static const FDataType InvalidValue;

	static float GetValue(const UBlackboardKeyType_Float* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Float* KeyOb, uint8* RawData, float Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const override;
};
