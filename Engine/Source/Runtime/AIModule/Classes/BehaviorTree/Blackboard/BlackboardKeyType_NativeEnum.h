// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_NativeEnum.generated.h"

// DEPRECATED, please use UBlackboardKeyType_Enum instead

UCLASS(NotEditInlineNew, HideDropDown)
class AIMODULE_API UBlackboardKeyType_NativeEnum : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef uint8 FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	FString EnumName;

	UPROPERTY()
	UEnum* EnumType;

	virtual UBlackboardKeyType* UpdateDeprecatedKey() override;

	static uint8 GetValue(const UBlackboardKeyType_NativeEnum* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_NativeEnum* KeyOb, uint8* RawData, uint8 Value);
};
