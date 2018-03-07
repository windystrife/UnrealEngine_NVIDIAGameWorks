// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Object.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Object"))
class AIMODULE_API UBlackboardKeyType_Object : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef UObject* FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly, meta=(AllowAbstract="1"))
	UClass* BaseClass;

	static UObject* GetValue(const UBlackboardKeyType_Object* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Object* KeyOb, uint8* RawData, UObject* Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	virtual FString DescribeSelf() const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool GetLocation(const UBlackboardComponent& OwnerComp, const uint8* RawData, FVector& Location) const override;
	virtual bool GetRotation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, FRotator& Rotation) const override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
