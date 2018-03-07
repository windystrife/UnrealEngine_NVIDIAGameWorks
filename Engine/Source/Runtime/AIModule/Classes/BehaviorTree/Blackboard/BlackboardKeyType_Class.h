// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Class.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Class"))
class AIMODULE_API UBlackboardKeyType_Class : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef UClass* FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly, meta=(AllowAbstract="1"))
	UClass* BaseClass;
	
	static UClass* GetValue(const UBlackboardKeyType_Class* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Class* KeyOb, uint8* RawData, UClass* Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	virtual FString DescribeSelf() const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
