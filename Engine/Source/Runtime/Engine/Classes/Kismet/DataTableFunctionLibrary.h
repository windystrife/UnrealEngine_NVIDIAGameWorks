// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/DataTable.h"
#include "Class.h" // for FStructUtils
#include "DataTableFunctionLibrary.generated.h"

class UCurveTable;

/** Enum used to indicate success or failure of EvaluateCurveTableRow. */
UENUM()
namespace EEvaluateCurveTableResult
{
    enum Type
    {
        /** Found the row successfully. */
        RowFound,
        /** Failed to find the row. */
        RowNotFound,
    };
}

UCLASS()
class ENGINE_API UDataTableFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "DataTable", meta = (ExpandEnumAsExecs="OutResult", DataTablePin="CurveTable"))
	static void EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY,const FString& ContextString);
    
	UFUNCTION(BlueprintCallable, Category = "DataTable")
	static void GetDataTableRowNames(UDataTable* Table, TArray<FName>& OutRowNames);

    /** Get a Row from a DataTable given a RowName */
    UFUNCTION(BlueprintCallable, CustomThunk, Category = "DataTable", meta=(CustomStructureParam = "OutRow", BlueprintInternalUseOnly="true"))
    static bool GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow);
    
	static bool Generic_GetDataTableRowFromName(UDataTable* Table, FName RowName, void* OutRowPtr);

    /** Based on UDataTableFunctionLibrary::GetDataTableRow */
    DECLARE_FUNCTION(execGetDataTableRowFromName)
    {
        P_GET_OBJECT(UDataTable, Table);
        P_GET_PROPERTY(UNameProperty, RowName);
        
        Stack.StepCompiledIn<UStructProperty>(NULL);
        void* OutRowPtr = Stack.MostRecentPropertyAddress;

		P_FINISH;
		bool bSuccess = false;
		
		UStructProperty* StructProp = Cast<UStructProperty>(Stack.MostRecentProperty);
		if (!Table)
		{
			FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				NSLOCTEXT("GetDataTableRow", "MissingTableInput", "Failed to resolve the table input. Be sure the DataTable is valid.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
		}
		else if(StructProp && OutRowPtr)
		{
			UScriptStruct* OutputType = StructProp->Struct;
			UScriptStruct* TableType  = Table->RowStruct;
		
			const bool bCompatible = (OutputType == TableType) || 
				(OutputType->IsChildOf(TableType) && FStructUtils::TheSameLayout(OutputType, TableType));
			if (bCompatible)
			{
				P_NATIVE_BEGIN;
				bSuccess = Generic_GetDataTableRowFromName(Table, RowName, OutRowPtr);
				P_NATIVE_END;
			}
			else
			{
				FBlueprintExceptionInfo ExceptionInfo(
					EBlueprintExceptionType::AccessViolation,
					NSLOCTEXT("GetDataTableRow", "IncompatibleProperty", "Incompatible output parameter; the data table's type is not the same as the return type.")
					);
				FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
			}
		}
		else
		{
			FBlueprintExceptionInfo ExceptionInfo(
				EBlueprintExceptionType::AccessViolation,
				NSLOCTEXT("GetDataTableRow", "MissingOutputProperty", "Failed to resolve the output parameter for GetDataTableRow.")
			);
			FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
		}
		*(bool*)RESULT_PARAM = bSuccess;
    }
};
