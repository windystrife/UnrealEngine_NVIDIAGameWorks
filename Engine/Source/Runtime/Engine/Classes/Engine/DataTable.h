// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "DataTableUtils.h"
#include "DataTable.generated.h"

class Error;
class UDataTable;
template <class CharType> struct TPrettyJsonPrintPolicy;

// forward declare JSON writer
template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;


class UDataTable;


/**
 * Base class for all table row structs to inherit from.
 */
USTRUCT(BlueprintInternalUseOnly)
struct FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	FTableRowBase() { }
	virtual ~FTableRowBase() { }

	/** 
	 * Can be overridden by subclasses; Called whenever the owning data table is imported or re-imported.
	 * Allows for custom fix-ups, parsing, etc. after initial data is read in.
	 * 
	 * @param InDataTable					The data table that owns this row
	 * @param InRowName						The name of the row we're performing fix-up on
	 * @param OutCollectedImportProblems	List of problems accumulated during import; Can be added to via this method
	 */
	virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems) {}
};


/**
 * Imported spreadsheet table.
 */
UCLASS(MinimalAPI, BlueprintType)
class UDataTable
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Structure to use for each row of the table, must inherit from FTableRowBase */
	UPROPERTY()
	UScriptStruct*			RowStruct;

	/** Map of name of row to row data structure. */
	TMap<FName, uint8*>		RowMap;

	//~ Begin UObject Interface.
	ENGINE_API virtual void FinishDestroy() override;
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	ENGINE_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	ENGINE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
#if WITH_EDITORONLY_DATA
	ENGINE_API FName GetRowStructName() const;
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void PostLoad() override;

	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	/** The filename imported to create this object. Relative to this object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath_DEPRECATED;

	/** The name of the RowStruct we were using when we were last saved */
	UPROPERTY()
	FName RowStructName;
#endif	// WITH_EDITORONLY_DATA

	//~ End  UObject Interface

	//~ Begin UDataTable Interface

	/** Get all of the rows in the table, regardless of name */
	template <class T>
	void GetAllRows(const FString& ContextString, OUT TArray<T*>& OutRowArray) const
	{
		if (RowStruct == nullptr)
		{
			UE_LOG(LogDataTable, Error, TEXT("UDataTable::FindRow : DataTable '%s' has no RowStruct specified (%s)."), *GetPathName(), *ContextString);
		}
		else if (!RowStruct->IsChildOf(T::StaticStruct()))
		{
			UE_LOG(LogDataTable, Error, TEXT("UDataTable::FindRow : Incorrect type specified for DataTable '%s' (%s)."), *GetPathName(), *ContextString);
		}
		else
		{
			for (TMap<FName, uint8*>::TConstIterator RowMapIter(RowMap.CreateConstIterator()); RowMapIter; ++RowMapIter)
			{
				OutRowArray.Add(reinterpret_cast<T*>(RowMapIter.Value()));
			}
		}	
	}

	/** Function to find the row of a table given its name. */
	template <class T>
	T* FindRow(FName RowName, const FString& ContextString, bool bWarnIfRowMissing = true) const
	{
		if(RowStruct == nullptr)
		{
			UE_LOG(LogDataTable, Error, TEXT("UDataTable::FindRow : '%s' specified no row for DataTable '%s'."), *ContextString, *GetPathName());
			return nullptr;
		}

		if(!RowStruct->IsChildOf(T::StaticStruct()))
		{
			UE_CLOG(bWarnIfRowMissing, LogDataTable, Error, TEXT("UDataTable::FindRow : '%s' specified incorrect type for DataTable '%s'."), *ContextString, *GetPathName());
			return nullptr;
		}

		if(RowName == NAME_None)
		{
			UE_CLOG(bWarnIfRowMissing, LogDataTable, Warning, TEXT("UDataTable::FindRow : '%s' requested invalid row 'None' from DataTable '%s'."), *ContextString, *GetPathName());
			return nullptr;
		}

		uint8* const* RowDataPtr = RowMap.Find(RowName);
		if (RowDataPtr == nullptr)
		{
			if (bWarnIfRowMissing)
			{
				UE_LOG(LogDataTable, Warning, TEXT("UDataTable::FindRow : '%s' requested row '%s' not in DataTable '%s'."), *ContextString, *RowName.ToString(), *GetPathName());
			}
			return nullptr;
		}

		uint8* RowData = *RowDataPtr;
		check(RowData);

		return reinterpret_cast<T*>(RowData);
	}

	/** Returns the column property where PropertyName matches the name of the column property. Returns nullptr if no match is found or the match is not a supported table property */
	ENGINE_API UProperty* FindTableProperty(const FName& PropertyName) const;

	uint8* FindRowUnchecked(FName RowName, bool MustExist=false) const
	{
		if(RowStruct == nullptr)
		{
			//UE_CLOG(MustExist, LogDataTable, Error, TEXT("UDataTable::FindRow : DataTable '%s' has no RowStruct specified (%s)."), *GetPathName(), *ContextString);
			return nullptr;
		}

		if(RowName == NAME_None)
		{
			//UE_CLOG(MustExist, LogDataTable, Warning, TEXT("UDataTable::FindRow : NAME_None is invalid row name for DataTable '%s' (%s)."), *GetPathName(), *ContextString);
			return nullptr;
		}

		uint8* const* RowDataPtr = RowMap.Find(RowName);

		if(RowDataPtr == nullptr)
		{
			return nullptr;
		}

		uint8* RowData = *RowDataPtr;
		check(RowData);

		return RowData;
	}

	/** Empty the table info (will not clear RowStruct) */
	ENGINE_API void EmptyTable();

	ENGINE_API TArray<FName> GetRowNames() const;

	/** Removes a single row from the DataTable by name. Just returns if row is not found. */
	ENGINE_API void RemoveRow(FName RowName);

	/** Copies RowData into table. That is: create Row if not found and copy data into the RowMap based on RowData. This is a "copy in" operation, so changing the passed in RowData after the fact does nothing. */
	ENGINE_API void AddRow(FName RowName, const FTableRowBase& RowData);

#if WITH_EDITOR

private:

	//when RowStruct is being modified, row data is stored serialized with tags
	TArray<uint8> RowsSerializedWithTags;
	TSet<UObject*> TemporarilyReferencedObjects;

public:

	ENGINE_API void CleanBeforeStructChange();
	ENGINE_API void RestoreAfterStructChange();

	/** Output entire contents of table as a string */
	ENGINE_API FString GetTableAsString(const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;

	/** Output entire contents of table as CSV */
	ENGINE_API FString GetTableAsCSV(const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;

	/** Output entire contents of table as JSON */
	ENGINE_API FString GetTableAsJSON(const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;

	/** Output entire contents of table as JSON */
	ENGINE_API bool WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter, const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;

	/** Output the fields from a particular row (use RowMap to get RowData) to an existing JsonWriter */
	ENGINE_API bool WriteRowAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter, const void* RowData, const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;

	/** 
	 *	Create table from CSV style comma-separated string. 
	 *	RowStruct must be defined before calling this function. 
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromCSVString(const FString& InString);

	/** 
	*	Create table from JSON style string. 
	*	RowStruct must be defined before calling this function. 
	*	@return	Set of problems encountered while processing input
	*/
	ENGINE_API TArray<FString> CreateTableFromJSONString(const FString& InString);

	/** Get an array of all the column titles, using the friendly display name from the property */
	ENGINE_API TArray<FString> GetColumnTitles() const;

	/** Get an array of all the column titles, using the unique name from the property */
	ENGINE_API TArray<FString> GetUniqueColumnTitles() const;

	TArray<UProperty*> GetTablePropertyArray(const TArray<const TCHAR*>& Cells, UStruct* RowStruct, TArray<FString>& OutProblems);

	/** Get array for each row in the table. The first row is the titles*/
	ENGINE_API TArray< TArray<FString> > GetTableData(const EDataTableExportFlags InDTExportFlags = EDataTableExportFlags::None) const;
#endif //WITH_EDITOR

	//~ End UDataTable Interface

private:
	void SaveStructData(FArchive& Ar);
	void LoadStructData(FArchive& Ar);

	/**
	 * Called whenever new data is imported into the data table via CreateTableFrom*; Alerts each imported row and gives the
	 * row struct a chance to operate on the imported data
	 * 
	 * @param OutCollectedImportProblems	[OUT] Array of strings of import problems
	 */
	void OnPostDataImported(OUT TArray<FString>& OutCollectedImportProblems);


	UScriptStruct& GetEmptyUsingStruct() const;
};


/** Handle to a particular row in a table*/
USTRUCT(BlueprintType)
struct ENGINE_API FDataTableRowHandle
{
	GENERATED_USTRUCT_BODY()

	FDataTableRowHandle()
		: DataTable(nullptr)
		, RowName(NAME_None)
	{

	}

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableRowHandle)
	const class UDataTable*	DataTable;

	/** Name of row in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableRowHandle)
	FName				RowName;

	/** Returns true if this handle is specifically pointing to nothing */
	bool IsNull() const
	{
		return DataTable == nullptr && RowName == NAME_None;
	}

	/** Get the row straight from the row handle */
	template <class T>
	T* GetRow(const FString& ContextString) const
	{
		if(DataTable == nullptr)
		{
			if (RowName != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableRowHandle::FindRow : No DataTable for row %s (%s)."), *RowName.ToString(), *ContextString);
			}
			return nullptr;
		}

		return DataTable->FindRow<T>(RowName, ContextString);
	}

	FString ToDebugString(bool bUseFullPath = false) const
	{
		if (DataTable == nullptr)
		{
			return FString::Printf(TEXT("No Data Table Specified, Row: %s"), *RowName.ToString());
		}

		return FString::Printf(TEXT("Table: %s, Row: %s"), bUseFullPath ? *DataTable->GetPathName() : *DataTable->GetName(), *RowName.ToString());
	}

	bool operator==(FDataTableRowHandle const& Other) const;
	bool operator!=(FDataTableRowHandle const& Other) const;
	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits< FDataTableRowHandle > : public TStructOpsTypeTraitsBase2< FDataTableRowHandle >
{
	enum
	{
		WithPostSerialize = true,
	};
};

/** Handle to a particular row in a table*/
USTRUCT(BlueprintType)
struct ENGINE_API FDataTableCategoryHandle
{
	GENERATED_USTRUCT_BODY()

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	const class UDataTable*	DataTable;

	/** Name of column in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	FName				ColumnName;

	/** Contents of rows in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=DataTableCategoryHandle)
	FName				RowContents;

	/** Returns true if this handle is specifically pointing to nothing */
	bool IsNull() const
	{
		return DataTable == nullptr && ColumnName == NAME_None && RowContents == NAME_None;
	}

	/** Searches DataTable for all rows that contain entries with RowContents in the column named ColumnName and returns them. */
	template <class T>
	void GetRows(TArray<T*>& OutRows, const FString& ContextString) const
	{
		OutRows.Empty();
		if (DataTable == nullptr)
		{
			if (RowContents != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableCategoryHandle::FindRow : No DataTable for row %s (%s)."), *RowContents.ToString(), *ContextString);
			}

			return;
		}

		if (ColumnName == NAME_None)
		{
			if (RowContents != NAME_None)
			{
				UE_LOG(LogDataTable, Warning, TEXT("FDataTableCategoryHandle::FindRow : No Column selected for row %s (%s)."), *RowContents.ToString(), *ContextString);
			}

			return;
		}

		// Find the property that matches the desired column (ColumnName)
		UProperty* Property = DataTable->FindTableProperty(ColumnName);
		if (Property == nullptr)
		{
			return;
		}

		// check each row to see if the value in the Property element is the one we're looking for (RowContents). If it is, add the row to OutRows
		uint8* RowContentsAsBinary = (uint8*)FMemory_Alloca(Property->GetSize());
		Property->InitializeValue(RowContentsAsBinary);
		if (Property->ImportText(*RowContents.ToString(), RowContentsAsBinary, PPF_None, nullptr) == nullptr)
		{
			Property->DestroyValue(RowContentsAsBinary);
			return;
		}

		for (auto RowIt = DataTable->RowMap.CreateConstIterator(); RowIt; ++RowIt)
		{
			uint8* RowData = RowIt.Value();

			if (Property->Identical(Property->ContainerPtrToValuePtr<void>(RowData, 0), RowContentsAsBinary, PPF_None))
			{
				OutRows.Add((T*)RowData);
			}
		}
		Property->DestroyValue(RowContentsAsBinary);

		return;
	}

	bool operator==(FDataTableCategoryHandle const& Other) const;
	bool operator!=(FDataTableCategoryHandle const& Other) const;
};


/** Macro to call GetRow with a correct error info. Assumed to be called from within a UObject */
#define GETROW_REPORTERROR(Handle, Template) Handle.GetRow<Template>(FString::Printf(TEXT("%s.%s"), *GetPathName(), TEXT(#Handle)))
#define GETROWOBJECT_REPORTERROR(Object, Handle, Template) Handle.GetRow<Template>(FString::Printf(TEXT("%s.%s"), *Object->GetPathName(), TEXT(#Handle)))
