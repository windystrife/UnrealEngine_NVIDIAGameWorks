// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Rotator.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Rotator"))
class AIMODULE_API UBlackboardKeyType_Rotator : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()
	
	typedef FRotator FDataType; 
	static const FDataType InvalidValue;

	static FRotator GetValue(const UBlackboardKeyType_Rotator* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Rotator* KeyOb, uint8* RawData, const FRotator& Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

protected:
	virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* RawData) override;
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool GetRotation(const UBlackboardComponent& OwnerComp, const uint8* RawData, FRotator& Rotation) const override;
	virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* RawData) override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
