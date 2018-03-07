// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Enum.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Enum"))
class AIMODULE_API UBlackboardKeyType_Enum : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef uint8 FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	UEnum* EnumType;

	/** name of enum defined in c++ code, will take priority over asset from EnumType property */
	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	FString EnumName;

	/** set when EnumName override is valid and active */
	UPROPERTY(Category = Blackboard, VisibleDefaultsOnly)
	uint32 bIsEnumNameValid : 1;

	static uint8 GetValue(const UBlackboardKeyType_Enum* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_Enum* KeyOb, uint8* RawData, uint8 Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	virtual FString DescribeSelf() const override;
	virtual FString DescribeArithmeticParam(int32 IntValue, float FloatValue) const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const override;
};
