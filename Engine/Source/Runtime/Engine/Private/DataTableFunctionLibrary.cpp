// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/DataTableFunctionLibrary.h"
#include "Engine/CurveTable.h"

UDataTableFunctionLibrary::UDataTableFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDataTableFunctionLibrary::EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY, TEnumAsByte<EEvaluateCurveTableResult::Type>& OutResult, float& OutXY,const FString& ContextString)
{
	FCurveTableRowHandle Handle;
	Handle.CurveTable = CurveTable;
	Handle.RowName = RowName;
	
	bool found = Handle.Eval(InXY, &OutXY,ContextString);
	
	if (found)
	{
	    OutResult = EEvaluateCurveTableResult::RowFound;
	}
	else
	{
	    OutResult = EEvaluateCurveTableResult::RowNotFound;
	}
}

bool UDataTableFunctionLibrary::Generic_GetDataTableRowFromName(UDataTable* Table, FName RowName, void* OutRowPtr)
{
	bool bFoundRow = false;

	if (OutRowPtr && Table)
	{
		void* RowPtr = Table->FindRowUnchecked(RowName);

		if (RowPtr != NULL)
		{
			UScriptStruct* StructType = Table->RowStruct;

			if (StructType != NULL)
			{
				StructType->CopyScriptStruct(OutRowPtr, RowPtr);
				bFoundRow = true;
			}
		}
	}

	return bFoundRow;
}

bool UDataTableFunctionLibrary::GetDataTableRowFromName(UDataTable* Table, FName RowName, FTableRowBase& OutRow)
{
	// We should never hit this!  stubs to avoid NoExport on the class.
	check(0);
	return false;
}

void UDataTableFunctionLibrary::GetDataTableRowNames(UDataTable* Table, TArray<FName>& OutRowNames)
{
	if (Table)
	{
		OutRowNames = Table->GetRowNames();
	}
	else
	{
		OutRowNames.Empty();
	}
}
