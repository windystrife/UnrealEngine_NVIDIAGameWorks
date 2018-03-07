// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPropertyPath;
class UProperty;
class UStruct;

namespace PropertyHelpers
{

	struct FPropertyAddress
	{
		UProperty* Property;
		void* Address;

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndIndex
	{
		FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

		UProperty* Property;
		int32 ArrayIndex;
	};

	FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName);

	FPropertyAddress FindPropertyRecursive(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index, TArray<UProperty*>& InOutPropertyChain, bool bAllowArrayResize);

	FPropertyAddress FindProperty(void* BasePointer, UStruct* InStruct, const FString& InPropertyPath, TArray<UProperty*>& InOutPropertyChain, bool bAllowArrayResize);

}