// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataTable;
enum class EDataTableExportFlags : uint8;

#if WITH_EDITOR

class FDataTableExporterCSV
{
public:
	FDataTableExporterCSV(const EDataTableExportFlags InDTExportFlags, FString& OutExportText);

	~FDataTableExporterCSV();

	bool WriteTable(const UDataTable& InDataTable);

	bool WriteRow(const UScriptStruct* InRowStruct, const void* InRowData);

private:
	bool WriteStructEntry(const void* InRowData, UProperty* InProperty, const void* InPropertyData);

	EDataTableExportFlags DTExportFlags;
	FString& ExportedText;
};

class FDataTableImporterCSV
{
public:
	FDataTableImporterCSV(UDataTable& InDataTable, FString InCSVData, TArray<FString>& OutProblems);

	~FDataTableImporterCSV();

	bool ReadTable();

private:
	UDataTable* DataTable;
	FString CSVData;
	TArray<FString>& ImportProblems;
};

#endif // WITH_EDITOR
