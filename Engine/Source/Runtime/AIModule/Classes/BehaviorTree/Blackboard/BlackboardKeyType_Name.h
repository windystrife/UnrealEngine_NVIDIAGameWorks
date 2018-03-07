// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Name.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Name"))
class AIMODULE_API UBlackboardKeyType_Name : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FName FDataType;
	static const FDataType InvalidValue;

	static FName GetValue(const UBlackboardKeyType_Name* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Name* KeyOb, uint8* RawData, const FName& Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const override;
};
