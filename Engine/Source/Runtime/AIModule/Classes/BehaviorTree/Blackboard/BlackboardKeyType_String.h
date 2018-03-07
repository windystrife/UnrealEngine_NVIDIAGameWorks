// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_String.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="String"))
class AIMODULE_API UBlackboardKeyType_String : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FString FDataType;
	static const FDataType InvalidValue;

	static FString GetValue(const UBlackboardKeyType_String* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_String* KeyOb, uint8* RawData, const FString& Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	UPROPERTY()
	FString StringValue;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const override;
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
	virtual void CopyValues(UBlackboardComponent& OwnerComp, uint8* MemoryBlock, const UBlackboardKeyType* SourceKeyOb, const uint8* SourceBlock) override;
};
