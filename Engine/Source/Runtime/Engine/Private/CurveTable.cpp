// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/CurveTable.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/Csv/CsvParser.h"

#include "EditorFramework/AssetImportData.h"

DEFINE_LOG_CATEGORY(LogCurveTable);

DECLARE_CYCLE_STAT(TEXT("CurveTableRowHandle Eval"),STAT_CurveTableRowHandleEval,STATGROUP_Engine);


//////////////////////////////////////////////////////////////////////////
UCurveTable::UCurveTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** Util that removes invalid chars and then make an FName */
FName UCurveTable::MakeValidName(const FString& InString)
{
	FString InvalidChars(INVALID_NAME_CHARACTERS);

	FString FixedString;
	TArray<TCHAR>& FixedCharArray = FixedString.GetCharArray();

	// Iterate over input string characters
	for(int32 CharIdx=0; CharIdx<InString.Len(); CharIdx++)
	{
		// See if this char occurs in the InvalidChars string
		FString Char = InString.Mid( CharIdx, 1 );
		if( !InvalidChars.Contains(Char) )
		{
			// Its ok, add to result
			FixedCharArray.Add(Char[0]);
		}
	}
	FixedCharArray.Add(0);

	return FName(*FixedString);
}

void UCurveTable::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar); // When loading, this should load our RowCurve!	

	if(Ar.IsLoading())
	{
		int32 NumRows;
		Ar << NumRows;

		for(int32 RowIdx = 0; RowIdx < NumRows; RowIdx++)
		{
			// Load row name
			FName RowName;
			Ar << RowName;

			// Load row data
			FRichCurve* NewCurve = new FRichCurve();
			FRichCurve::StaticStruct()->SerializeTaggedProperties(Ar, (uint8*)NewCurve, FRichCurve::StaticStruct(), NULL);

			// Add to map
			RowMap.Add(RowName, NewCurve);
		}
	}
	else if(Ar.IsSaving())
	{
		int32 NumRows = RowMap.Num();
		Ar << NumRows;

		// Now iterate over rows in the map
		for ( auto RowIt = RowMap.CreateIterator(); RowIt; ++RowIt )
		{
			// Save out name
			FName RowName = RowIt.Key();
			Ar << RowName;

			// Save out data
			FRichCurve* Curve = RowIt.Value();
			FRichCurve::StaticStruct()->SerializeTaggedProperties(Ar, (uint8*)Curve, FRichCurve::StaticStruct(), NULL);
		}
	}
}

void UCurveTable::FinishDestroy()
{
	Super::FinishDestroy();

	EmptyTable(); // Free memory when UObject goes away
}

#if WITH_EDITORONLY_DATA
void UCurveTable::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}

void UCurveTable::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}

	Super::PostInitProperties();
}

void UCurveTable::PostLoad()
{
	Super::PostLoad();
	if (!ImportPath_DEPRECATED.IsEmpty() && AssetImportData)
	{
		FAssetImportInfo Info;
		Info.Insert(FAssetImportInfo::FSourceFile(ImportPath_DEPRECATED));
		AssetImportData->SourceData = MoveTemp(Info);
	}
}
#endif

FString UCurveTable::GetTableAsString() const
{
	FString Result;

	if(RowMap.Num() > 0)
	{
		TArray<FName> Names;
		TArray<FRichCurve*> Curves;
		
		// get the row names and curves they represent
		RowMap.GenerateKeyArray(Names);
		RowMap.GenerateValueArray(Curves);

		// Determine the curve with the longest set of data, for headers
		int32 LongestCurveIndex = 0;
		for(int32 CurvesIdx = 1; CurvesIdx < Curves.Num(); CurvesIdx++)
		{
			if(Curves[CurvesIdx]->GetNumKeys() > Curves[LongestCurveIndex]->GetNumKeys())
			{
				LongestCurveIndex = CurvesIdx;
			}
		}

		// First row, column titles, taken from the longest curve
		Result += TEXT("---");
		for (auto It(Curves[LongestCurveIndex]->GetKeyIterator()); It; ++It)
		{
			Result += FString::Printf(TEXT(",%f"), It->Time);
		}
		Result += TEXT("\n");

		// display all the curves
		for(int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); CurvesIdx++)
		{
			// show name of curve
			Result += Names[CurvesIdx].ToString();

			// show data of curve
			for (auto It(Curves[CurvesIdx]->GetKeyIterator()); It; ++It)
			{
				Result += FString::Printf(TEXT(",%f"), It->Value);
			}

			Result += TEXT("\n");
		}
	}
	else
	{
		Result += FString(TEXT("No data in row curve!\n"));
	}
	return Result;
}

FString UCurveTable::GetTableAsCSV() const
{
	FString Result;

	if(RowMap.Num() > 0)
	{
		TArray<FName> Names;
		TArray<FRichCurve*> Curves;
		
		// get the row names and curves they represent
		RowMap.GenerateKeyArray(Names);
		RowMap.GenerateValueArray(Curves);

		// Determine the curve with the longest set of data, for headers
		int32 LongestCurveIndex = 0;
		for (int32 CurvesIdx = 1; CurvesIdx < Curves.Num(); CurvesIdx++)
		{
			if(Curves[CurvesIdx]->GetNumKeys() > Curves[LongestCurveIndex]->GetNumKeys())
			{
				LongestCurveIndex = CurvesIdx;
			}
		}

		// First row, column titles, taken from the longest curve
		Result += TEXT("---");
		for (auto It(Curves[LongestCurveIndex]->GetKeyIterator()); It; ++It)
		{
			Result += FString::Printf(TEXT(",%f"), It->Time);
		}
		Result += TEXT("\n");

		// display all the curves
		for (int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); CurvesIdx++)
		{
			// show name of curve
			Result += Names[CurvesIdx].ToString();

			// show data of curve
			for (auto It(Curves[CurvesIdx]->GetKeyIterator()); It; ++It)
			{
				Result += FString::Printf(TEXT(",%f"), It->Value);
			}

			Result += TEXT("\n");
		}
	}

	return Result;
}

FString UCurveTable::GetTableAsJSON() const
{
	// use the pretty print policy since these values are usually getting dumpped for check-in to P4 (or for inspection)
	FString Result;
	TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&Result);
	if (!WriteTableAsJSON(JsonWriter))
	{
		return TEXT("No data in row curve!\n");
	}
	JsonWriter->Close();
	return Result;
}

bool UCurveTable::WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter, bool bAsArray) const
{
	if(RowMap.Num() <= 0)
	{
		return false;
	}
	TArray<FName> Names;
	TArray<FRichCurve*> Curves;

	// get the row names and curves they represent
	RowMap.GenerateKeyArray(Names);
	RowMap.GenerateValueArray(Curves);

	// Determine the curve with the longest set of data, for headers
	int32 LongestCurveIndex = 0;
	for(int32 CurvesIdx = 1; CurvesIdx < Curves.Num(); CurvesIdx++)
	{
		if(Curves[CurvesIdx]->GetNumKeys() > Curves[LongestCurveIndex]->GetNumKeys())
		{
			LongestCurveIndex = CurvesIdx;
		}
	}

	if (bAsArray)
	{
		JsonWriter->WriteArrayStart();
	}

	// display all the curves
	for(int32 CurvesIdx = 0; CurvesIdx < Curves.Num(); CurvesIdx++)
	{
		if (bAsArray)
		{
			JsonWriter->WriteObjectStart();
			// show name of curve
			JsonWriter->WriteValue(TEXT("Name"),Names[CurvesIdx].ToString());
		}
		else
		{
			JsonWriter->WriteObjectStart(Names[CurvesIdx].ToString());
		}
		// show data of curve
		auto LongIt(Curves[LongestCurveIndex]->GetKeyIterator());
		for (auto It(Curves[CurvesIdx]->GetKeyIterator()); It; ++It)
		{
			JsonWriter->WriteValue(FString::Printf(TEXT("%d"), (int32)LongIt->Time), It->Value);
			++LongIt;
		}
		JsonWriter->WriteObjectEnd();
	}

	if(bAsArray)
	{
		JsonWriter->WriteArrayEnd();
	}
	return true;
}

void UCurveTable::EmptyTable()
{
	// Iterate over all rows in table and free mem
	for ( auto RowIt = RowMap.CreateIterator(); RowIt; ++RowIt )
	{
		FRichCurve* RowData = RowIt.Value();
		delete RowData;
	}

	// Finally empty the map
	RowMap.Empty();
}


/** */
void GetCurveValues(const TArray<const TCHAR*>& Cells, TArray<float>* Values)
{
	// Need at least 2 columns, first column is skipped, will contain row names
	if(Cells.Num() >= 2)
	{
		// first element always NULL - as first column is row names
		for(int32 ColIdx = 1; ColIdx < Cells.Num(); ColIdx++)
		{
			Values->Add(FCString::Atof(Cells[ColIdx]));
		}
	}
}

TArray<FString> UCurveTable::CreateTableFromCSVString(const FString& InString, ERichCurveInterpMode InterpMode)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

	const FCsvParser Parser(InString);
	const FCsvParser::FRows& Rows = Parser.GetRows();

	// Must have at least 2 rows (x values + y values for at least one row)
	if(Rows.Num() <= 1)
	{
		OutProblems.Add(FString(TEXT("Too few rows.")));
		return OutProblems;
	}

	// Empty existing data
	EmptyTable();

	TArray<float> XValues;
	GetCurveValues(Rows[0], &XValues);

	// Iterate over rows
	for(int32 RowIdx = 1; RowIdx < Rows.Num(); RowIdx++)
	{
		const TArray<const TCHAR*>& Row = Rows[RowIdx];

		// Need at least 1 cells (row name)
		if(Row.Num() < 1)
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' has too few cells."), RowIdx));
			continue;
		}

		// Get row name
		FName RowName = MakeValidName(Row[0]);

		// Check its not 'none'
		if(RowName == NAME_None)
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), RowIdx));
			continue;
		}

		// Check its not a duplicate
		if(RowMap.Find(RowName) != NULL)
		{
			OutProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
			continue;
		}

		TArray<float> YValues;
		GetCurveValues(Row, &YValues);

		if(XValues.Num() != YValues.Num())
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%s' does not have the right number of columns."), *RowName.ToString()));
			continue;
		}

		FRichCurve* NewCurve = new FRichCurve();
		// Now iterate over cells (skipping first cell, that was row name)
		for(int32 ColumnIdx = 0; ColumnIdx < XValues.Num(); ColumnIdx++)
		{
			FKeyHandle KeyHandle = NewCurve->AddKey(XValues[ColumnIdx], YValues[ColumnIdx]);
			NewCurve->SetKeyInterpMode(KeyHandle, InterpMode);
		}

		RowMap.Add(RowName, NewCurve);
	}

	Modify(true);
	return OutProblems;
}

TArray<FString> UCurveTable::CreateTableFromJSONString(const FString& InString, ERichCurveInterpMode InterpMode)
{
	// Array used to store problems about table creation
	TArray<FString> OutProblems;

	if (InString.IsEmpty())
	{
		OutProblems.Add(TEXT("Input data is empty."));
		return OutProblems;
	}

	TArray< TSharedPtr<FJsonValue> > ParsedTableRows;
	{
		const TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(InString);
		if (!FJsonSerializer::Deserialize(JsonReader, ParsedTableRows) || ParsedTableRows.Num() == 0)
		{
			OutProblems.Add(FString::Printf(TEXT("Failed to parse the JSON data. Error: %s"), *JsonReader->GetErrorMessage()));
			return OutProblems;
		}
	}

	// Empty existing data
	EmptyTable();

	// Iterate over rows
	for (int32 RowIdx = 0; RowIdx < ParsedTableRows.Num(); ++RowIdx)
	{
		const TSharedPtr<FJsonValue>& ParsedTableRowValue = ParsedTableRows[RowIdx];
		TSharedPtr<FJsonObject> ParsedTableRowObject = ParsedTableRowValue->AsObject();
		if (!ParsedTableRowObject.IsValid())
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' is not a valid JSON object."), RowIdx));
			continue;
		}

		// Get row name
		static const FString RowNameJsonKey = TEXT("Name");
		const FName RowName = MakeValidName(ParsedTableRowObject->GetStringField(RowNameJsonKey));

		// Check its not 'none'
		if (RowName == NAME_None)
		{
			OutProblems.Add(FString::Printf(TEXT("Row '%d' missing a name."), RowIdx));
			continue;
		}

		// Check its not a duplicate
		if (RowMap.Find(RowName) != NULL)
		{
			OutProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
			continue;
		}

		// Add a key for each entry in this row
		FRichCurve* NewCurve = new FRichCurve();
		for (const auto& ParsedTableRowEntry : ParsedTableRowObject->Values)
		{
			// Skip the name entry
			if (ParsedTableRowEntry.Key == RowNameJsonKey)
			{
				continue;
			}

			// Make sure we have a valid float key
			float EntryKey = 0.0f;
			if (!Lex::TryParseString(EntryKey, *ParsedTableRowEntry.Key))
			{
				OutProblems.Add(FString::Printf(TEXT("Key '%s' on row '%s' is not a float and cannot be parsed."), *ParsedTableRowEntry.Key, *RowName.ToString()));
				continue;
			}

			// Make sure we have a valid float value
			double EntryValue = 0.0;
			if (!ParsedTableRowEntry.Value->TryGetNumber(EntryValue))
			{
				OutProblems.Add(FString::Printf(TEXT("Entry '%s' on row '%s' is not a float and cannot be parsed."), *ParsedTableRowEntry.Key, *RowName.ToString()));
				continue;
			}

			FKeyHandle KeyHandle = NewCurve->AddKey(EntryKey, static_cast<float>(EntryValue));
			NewCurve->SetKeyInterpMode(KeyHandle, InterpMode);
		}

		RowMap.Add(RowName, NewCurve);
	}

	Modify(true);
	return OutProblems;
}

TArray<FRichCurveEditInfoConst> UCurveTable::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;

	for (auto Iter = RowMap.CreateConstIterator(); Iter; ++Iter)
	{
		Curves.Add(FRichCurveEditInfoConst(Iter.Value(), Iter.Key()));
	}

	return Curves;
}

TArray<FRichCurveEditInfo> UCurveTable::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	
	for (auto Iter = RowMap.CreateIterator(); Iter; ++Iter)
	{
		Curves.Add(FRichCurveEditInfo(Iter.Value(), Iter.Key()));
	}

	return Curves;
}

void UCurveTable::ModifyOwner()
{
	Modify(true);
}

void UCurveTable::MakeTransactional()
{
	SetFlags(GetFlags() | RF_Transactional);
}

void UCurveTable::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{

}

bool UCurveTable::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	for (auto Iter = RowMap.CreateConstIterator(); Iter; ++Iter)
	{
		if (CurveInfo.CurveToEdit == Iter.Value())
		{
			return true;
		}
	}

	return false;
}



TArray<const UObject*> UCurveTable::GetOwners() const
{
	TArray<const UObject*> Owners;
	Owners.Add(this);

	return Owners;
}

//////////////////////////////////////////////////////////////////////////


FRichCurve* FCurveTableRowHandle::GetCurve(const FString& ContextString) const
{
	if(CurveTable == NULL)
	{
		if (RowName != NAME_None)
		{
			UE_LOG(LogCurveTable, Warning, TEXT("FCurveTableRowHandle::FindRow : No CurveTable for row %s (%s)."), *RowName.ToString(), *ContextString);
		}
		return NULL;
	}

	return CurveTable->FindCurve(RowName, ContextString);
}

float FCurveTableRowHandle::Eval(float XValue,const FString& ContextString) const
{
	SCOPE_CYCLE_COUNTER(STAT_CurveTableRowHandleEval); 

	FRichCurve* Curve = GetCurve(ContextString);
	if(Curve != NULL)
	{
		return Curve->Eval(XValue);
	}

	return 0;
}

bool FCurveTableRowHandle::Eval(float XValue, float* YValue, const FString& ContextString) const
{
	SCOPE_CYCLE_COUNTER(STAT_CurveTableRowHandleEval); 

	FRichCurve* Curve = GetCurve(ContextString);
	if(Curve != NULL && YValue != NULL)
	{
		*YValue = Curve->Eval(XValue);
		return true;
	}

	return false;
}

bool FCurveTableRowHandle::operator==(const FCurveTableRowHandle& Other) const
{
	return ((Other.CurveTable == CurveTable) && (Other.RowName == RowName));
}

bool FCurveTableRowHandle::operator!=(const FCurveTableRowHandle& Other) const
{
	return ((Other.CurveTable != CurveTable) || (Other.RowName != RowName));
}
void FCurveTableRowHandle::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsSaving() && !IsNull() && CurveTable)
	{
		// Note which row we are pointing to for later searching
		Ar.MarkSearchableName(CurveTable, RowName);
	}
}
