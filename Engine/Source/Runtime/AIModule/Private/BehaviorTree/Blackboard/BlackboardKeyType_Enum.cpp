// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

const UBlackboardKeyType_Enum::FDataType UBlackboardKeyType_Enum::InvalidValue = UBlackboardKeyType_Enum::FDataType(0);

UBlackboardKeyType_Enum::UBlackboardKeyType_Enum(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(uint8);
	SupportedOp = EBlackboardKeyOperation::Arithmetic;
}

uint8 UBlackboardKeyType_Enum::GetValue(const UBlackboardKeyType_Enum* KeyOb, const uint8* RawData)
{
	return GetValueFromMemory<uint8>(RawData);
}

bool UBlackboardKeyType_Enum::SetValue(UBlackboardKeyType_Enum* KeyOb, uint8* RawData, uint8 Value)
{
	return SetValueInMemory<uint8>(RawData, Value);
}

EBlackboardCompare::Type UBlackboardKeyType_Enum::CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
	const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const
{
	const uint8 MyValue = GetValue(this, MemoryBlock);
	const uint8 OtherValue = GetValue((UBlackboardKeyType_Enum*)OtherKeyOb, OtherMemoryBlock);

	return (MyValue > OtherValue) ? EBlackboardCompare::Greater :
		(MyValue < OtherValue) ? EBlackboardCompare::Less :
		EBlackboardCompare::Equal;
}

FString UBlackboardKeyType_Enum::DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const
{
	return EnumType ? EnumType->GetDisplayNameTextByValue(GetValue(this, RawData)).ToString() : FString("UNKNOWN!");
}

FString UBlackboardKeyType_Enum::DescribeSelf() const
{
	return *GetNameSafe(EnumType);
}

bool UBlackboardKeyType_Enum::IsAllowedByFilter(UBlackboardKeyType* FilterOb) const
{
	UBlackboardKeyType_Enum* FilterEnum = Cast<UBlackboardKeyType_Enum>(FilterOb);
	return (FilterEnum && FilterEnum->EnumType == EnumType);
}

bool UBlackboardKeyType_Enum::TestArithmeticOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EArithmeticKeyOperation::Type Op, int32 OtherIntValue, float OtherFloatValue) const
{
	const uint8 Value = GetValue(this, MemoryBlock);
	switch (Op)
	{
	case EArithmeticKeyOperation::Equal:			return (Value == OtherIntValue);
	case EArithmeticKeyOperation::NotEqual:			return (Value != OtherIntValue);
	case EArithmeticKeyOperation::Less:				return (Value < OtherIntValue);
	case EArithmeticKeyOperation::LessOrEqual:		return (Value <= OtherIntValue);
	case EArithmeticKeyOperation::Greater:			return (Value > OtherIntValue);
	case EArithmeticKeyOperation::GreaterOrEqual:	return (Value >= OtherIntValue);
	default: break;
	}

	return false;
}

FString UBlackboardKeyType_Enum::DescribeArithmeticParam(int32 IntValue, float FloatValue) const
{
	return EnumType ? EnumType->GetDisplayNameTextByValue(IntValue).ToString() : FString("UNKNOWN!");
}

#if WITH_EDITOR
void UBlackboardKeyType_Enum::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBlackboardKeyType_Enum, EnumName))
	{
		EnumType = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
	}

	bIsEnumNameValid = EnumType && !EnumName.IsEmpty();
}
#endif
