// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructBox.h"
#include "StructBoxLibrary.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct FStubStruct
{
	GENERATED_USTRUCT_BODY()

	FStubStruct() { }
};

UCLASS()
class UStructBoxLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "StructBox", meta = (CustomStructureParam = "CustomStruct"))
	static bool GetStructFromBox(const FStructBox& StructBox, FStubStruct& CustomStruct);

	DECLARE_FUNCTION(execGetStructFromBox)
	{
		PARAM_PASSED_BY_REF(StructBox, UStructProperty, FStructBox);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;

		Stack.StepCompiledIn<UStructProperty>(NULL);
		void* DstStructAddr = Stack.MostRecentPropertyAddress;
		auto DstStructProperty = Cast<UStructProperty>(Stack.MostRecentProperty);

		bool bResult = false;
		if (DstStructAddr && DstStructProperty && StructBox.IsValid())
		{
			if (StructBox.ScriptStruct == DstStructProperty->Struct)
			{
				StructBox.ScriptStruct->CopyScriptStruct(DstStructAddr, StructBox.StructMemory);
				bResult = true;
			}
		}

		P_FINISH;

		*(bool*)RESULT_PARAM = bResult;
	}

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "StructBox", meta = (CustomStructureParam = "CustomStruct"))
	static void SetStructInBox(UPARAM(ref) FStructBox& StructBox, const FStubStruct& CustomStruct);

	DECLARE_FUNCTION(execSetStructInBox)
	{
		PARAM_PASSED_BY_REF(StructBox, UStructProperty, FStructBox);

		Stack.MostRecentPropertyAddress = nullptr;
		Stack.MostRecentProperty = nullptr;

		Stack.StepCompiledIn<UStructProperty>(NULL);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		auto SrcStructProperty = Cast<UStructProperty>(Stack.MostRecentProperty);

		StructBox.Destroy(StructBox.ScriptStruct);

		if (SrcStructAddr && SrcStructProperty)
		{
			StructBox.ScriptStruct = SrcStructProperty->Struct;
			// TODO: CHECK compatibility 
			StructBox.Create(StructBox.ScriptStruct);
			StructBox.ScriptStruct->CopyScriptStruct(StructBox.StructMemory, SrcStructAddr);
		}

		P_FINISH;
	}
};
