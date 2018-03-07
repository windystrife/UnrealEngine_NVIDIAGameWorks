// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"

const UBlackboardKeyType_Name::FDataType UBlackboardKeyType_Name::InvalidValue = NAME_None;

UBlackboardKeyType_Name::UBlackboardKeyType_Name(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FName);
	SupportedOp = EBlackboardKeyOperation::Text;
}

FName UBlackboardKeyType_Name::GetValue(const UBlackboardKeyType_Name* KeyOb, const uint8* RawData)
{
	return GetValueFromMemory<FName>(RawData);
}

bool UBlackboardKeyType_Name::SetValue(UBlackboardKeyType_Name* KeyOb, uint8* RawData, const FName& Value)
{
	return SetValueInMemory<FName>(RawData, Value);
}

EBlackboardCompare::Type UBlackboardKeyType_Name::CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
	const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const
{
	const FName MyValue = GetValue(this, MemoryBlock);
	const FName OtherValue = GetValue((UBlackboardKeyType_Name*)OtherKeyOb, OtherMemoryBlock);

	return (MyValue == OtherValue) ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

FString UBlackboardKeyType_Name::DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const
{
	return GetValue(this, RawData).ToString();
}

bool UBlackboardKeyType_Name::TestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const
{
	const FString Value = GetValue(this, MemoryBlock).ToString();
	switch (Op)
	{
	case ETextKeyOperation::Equal:			return (Value == OtherString);
	case ETextKeyOperation::NotEqual:		return (Value != OtherString);
	case ETextKeyOperation::Contain:		return (Value.Contains(OtherString) == true);
	case ETextKeyOperation::NotContain:		return (Value.Contains(OtherString) == false);
	default: break;
	}

	return false;
}
