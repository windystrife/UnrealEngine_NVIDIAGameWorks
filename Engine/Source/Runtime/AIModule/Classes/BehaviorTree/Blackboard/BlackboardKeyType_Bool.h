// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Bool.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Bool"))
class AIMODULE_API UBlackboardKeyType_Bool : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef bool FDataType;
	static const FDataType InvalidValue;

	static bool GetValue(const UBlackboardKeyType_Bool* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Bool* KeyOb, uint8* RawData, bool bValue);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
