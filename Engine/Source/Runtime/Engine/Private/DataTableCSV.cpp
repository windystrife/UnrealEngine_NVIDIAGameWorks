// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DataTableCSV.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Serialization/Csv/CsvParser.h"
#include "Engine/UserDefinedStruct.h"

#if WITH_EDITOR

FDataTableExporterCSV::FDataTableExporterCSV(const EDataTableExportFlags InDTExportFlags, FString& OutExportText)
	: DTExportFlags(InDTExportFlags)
	, ExportedText(OutExportText)
{
}

FDataTableExporterCSV::~FDataTableExporterCSV()
{
}

bool FDataTableExporterCSV::WriteTable(const UDataTable& InDataTable)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	// Write the header (column titles)
	ExportedText += TEXT("---");
	for (TFieldIterator<UProperty> It(InDataTable.RowStruct); It; ++It)
	{
		UProperty* BaseProp = *It;
		check(BaseProp);

		ExportedText += TEXT(",");
		ExportedText += DataTableUtils::GetPropertyExportName(BaseProp, DTExportFlags);
	}
	ExportedText += TEXT("\n");

	// Write each row
	for (auto RowIt = InDataTable.RowMap.CreateConstIterator(); RowIt; ++RowIt)
	{
		FName RowName = RowIt.Key();
		ExportedText += RowName.ToString();

		uint8* RowData = RowIt.Value();
		WriteRow(InDataTable.RowStruct, RowData);

		ExportedText += TEXT("\n");
	}

	return true;
}

bool FDataTableExporterCSV::WriteRow(const UScriptStruct* InRowStruct, const void* InRowData)
{
	if (!InRowStruct)
	{
		return false;
	}

	for (TFieldIterator<UProperty> It(InRowStruct); It; ++It)
	{
		UProperty* BaseProp = *It;
		check(BaseProp);

		const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InRowData, 0);
		WriteStructEntry(InRowData, BaseProp, Data);
	}

	return true;
}

bool FDataTableExporterCSV::WriteStructEntry(const void* InRowData, UProperty* InProperty, const void* InPropertyData)
{
	ExportedText += TEXT(",");

	const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
	ExportedText += TEXT("\"");
	ExportedText += PropertyValue.Replace(TEXT("\""), TEXT("\"\""));
	ExportedText += TEXT("\"");

	return true;
}


FDataTableImporterCSV::FDataTableImporterCSV(UDataTable& InDataTable, FString InCSVData, TArray<FString>& OutProblems)
	: DataTable(&InDataTable)
	, CSVData(MoveTemp(InCSVData))
	, ImportProblems(OutProblems)
{
}

FDataTableImporterCSV::~FDataTableImporterCSV()
{
}

bool FDataTableImporterCSV::ReadTable()
{
	if (CSVData.IsEmpty())
	{
		ImportProblems.Add(TEXT("Input data is empty."));
		return false;
	}

	// Check we have a RowStruct specified
	if (!DataTable->RowStruct)
	{
		ImportProblems.Add(TEXT("No RowStruct specified."));
		return false;
	}

	const FCsvParser Parser(CSVData);
	const auto& Rows = Parser.GetRows();

	// Must have at least 2 rows (column names + data)
	if(Rows.Num() <= 1)
	{
		ImportProblems.Add(TEXT("Too few rows."));
		return false;
	}

	// Find property for each column
	TArray<UProperty*> ColumnProps = DataTable->GetTablePropertyArray(Rows[0], DataTable->RowStruct, ImportProblems);

	// Empty existing data
	DataTable->EmptyTable();

	// Iterate over rows
	for(int32 RowIdx=1; RowIdx<Rows.Num(); RowIdx++)
	{
		const TArray<const TCHAR*>& Cells = Rows[RowIdx];

		// Need at least 1 cells (row name)
		if(Cells.Num() < 1)
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' has too few cells."), RowIdx));
			continue;
		}

		// Need enough columns in the properties!
		if( ColumnProps.Num() < Cells.Num() )
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' has more cells than properties, is there a malformed string?"), RowIdx));
			continue;
		}

		// Get row name
		FName RowName = DataTableUtils::MakeValidName(Cells[0]);

		// Check its not 'none'
		if(RowName == NAME_None)
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), RowIdx));
			continue;
		}

		// Check its not a duplicate
		if(DataTable->RowMap.Find(RowName) != NULL)
		{
			ImportProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
			continue;
		}

		// Allocate data to store information, using UScriptStruct to know its size
		uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->GetStructureSize());
		DataTable->RowStruct->InitializeStruct(RowData);
		// And be sure to call DestroyScriptStruct later

		if (auto UDStruct = Cast<const UUserDefinedStruct>(DataTable->RowStruct))
		{
			UDStruct->InitializeDefaultValue(RowData);
		}

		// Add to row map
		DataTable->RowMap.Add(RowName, RowData);

		// Now iterate over cells (skipping first cell, that was row name)
		for(int32 CellIdx=1; CellIdx<Cells.Num(); CellIdx++)
		{
			// Try and assign string to data using the column property
			UProperty* ColumnProp = ColumnProps[CellIdx];
			const FString CellValue = Cells[CellIdx];
			FString Error = DataTableUtils::AssignStringToProperty(CellValue, ColumnProp, RowData);

			// If we failed, output a problem string
			if(Error.Len() > 0)
			{
				FString ColumnName = (ColumnProp != NULL) 
					? DataTableUtils::GetPropertyDisplayName(ColumnProp, ColumnProp->GetName())
					: FString(TEXT("NONE"));
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *CellValue, *ColumnName, *RowName.ToString(), *Error));
			}
		}

		// Problem if we didn't have enough cells on this row
		if(Cells.Num() < ColumnProps.Num())
		{
			ImportProblems.Add(FString::Printf(TEXT("Too few cells on row '%s'."), *RowName.ToString()));			
		}
	}

	DataTable->Modify(true);

	return true;
}

#endif // WITH_EDITOR
