// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Curves/CurveOwnerInterface.h"
#include "CurveTable.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogCurveTable, Log, All);


// forward declare JSON writer
template <class CharType>
struct TPrettyJsonPrintPolicy;
template <class CharType, class PrintPolicy>
class TJsonWriter;


/**
 * Imported spreadsheet table as curves.
 */
UCLASS(MinimalAPI)
class UCurveTable
	: public UObject
	, public FCurveOwnerInterface
{
	GENERATED_UCLASS_BODY()

	/** Map of name of row to row data structure. */
	TMap<FName, FRichCurve*>	RowMap;

	//~ Begin UObject Interface.
	virtual void FinishDestroy() override;
	virtual void Serialize( FArchive& Ar ) override;

#if WITH_EDITORONLY_DATA
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	/** The filename imported to create this object. Relative to this object's package, BaseDir() or absolute */
	UPROPERTY()
	FString ImportPath_DEPRECATED;
	
#endif	// WITH_EDITORONLY_DATA

	//~ End  UObject Interface

	//~ Begin FCurveOwnerInterface Interface.
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual bool RepointCurveOwner(const FPackageReloadedEvent& InPackageReloadedEvent, FCurveOwnerInterface*& OutNewCurveOwner) const override
	{
		return RepointCurveOwnerAsset(InPackageReloadedEvent, this, OutNewCurveOwner);
	}
	//~ End FCurveOwnerInterface Interface.

	//~ Begin UCurveTable Interface

	/** Function to find the row of a table given its name. */
	FRichCurve* FindCurve(FName RowName, const FString& ContextString, bool WarnIfNotFound=true) const
	{
		if(RowName == NAME_None)
		{
			UE_CLOG(WarnIfNotFound, LogCurveTable, Warning, TEXT("UCurveTable::FindCurve : NAME_None is invalid row name for CurveTable '%s' (%s)."), *GetPathName(), *ContextString);
			return nullptr;
		}

		FRichCurve* const* FoundCurve = RowMap.Find(RowName);

		if(FoundCurve == nullptr)
		{
			UE_CLOG(WarnIfNotFound, LogCurveTable, Warning, TEXT("UCurveTable::FindCurve : Row '%s' not found in CurveTable '%s' (%s)."), *RowName.ToString(), *GetPathName(), *ContextString);
			return nullptr;
		}

		return (FRichCurve*)*FoundCurve;
	}

	/** Output entire contents of table as a string */
	ENGINE_API FString GetTableAsString() const;

	/** Output entire contents of table as CSV */
	ENGINE_API FString GetTableAsCSV() const;

	/** Output entire contents of table as JSON */
	ENGINE_API FString GetTableAsJSON() const;

	/** Output entire contents of table as JSON. bAsArray true will write is as a JSON array, false will write it as a series of named objects*/
	ENGINE_API bool WriteTableAsJSON(const TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > >& JsonWriter,bool bAsArray = true) const;

	/** 
	 *	Create table from CSV style comma-separated string. 
	 *	RowCurve must be defined before calling this function. 
	 *  @param InString The string representing the CurveTable
	 *  @param InterpMode The mode of interpolation to use for the curves
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromCSVString(const FString& InString, ERichCurveInterpMode InterpMode = RCIM_Linear);

	/** 
	 *	Create table from JSON string. 
	 *	RowCurve must be defined before calling this function. 
	 *  @param InString The string representing the CurveTable
	 *  @param InterpMode The mode of interpolation to use for the curves
	 *	@return	Set of problems encountered while processing input
	 */
	ENGINE_API TArray<FString> CreateTableFromJSONString(const FString& InString, ERichCurveInterpMode InterpMode = RCIM_Linear);

	/** Empty the table info (will not clear RowCurve) */
	ENGINE_API void EmptyTable();

protected:
	/** Util that removes invalid chars and then make an FName
	 * @param InString The string to create a valid name from
	 * @return A valid name from InString
	 */
	static FName MakeValidName(const FString& InString);

};


/**
 * Handle to a particular row in a table.
 */
USTRUCT(BlueprintType)
struct ENGINE_API FCurveTableRowHandle
{
	GENERATED_USTRUCT_BODY()

	FCurveTableRowHandle()
		: CurveTable(nullptr)
		, RowName(NAME_None)
	{ }

	/** Pointer to table we want a row from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CurveTableRowHandle)
	const class UCurveTable*	CurveTable;

	/** Name of row in the table that we want */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CurveTableRowHandle)
	FName				RowName;

	/** Returns true if the curve is valid */
	bool IsValid(const FString& ContextString) const
	{
		return (GetCurve(ContextString) != nullptr);
	}

	/** Returns true if this handle is specifically pointing to nothing */
	bool IsNull() const
	{
		return CurveTable == nullptr && RowName == NAME_None;
	}

	/** Get the curve straight from the row handle */
	FRichCurve* GetCurve(const FString& ContextString) const;

	/** Evaluate the curve if it is valid
	 * @param XValue The input X value to the curve
	 * @param ContextString A string to provide context for where this operation is being carried out
	 * @return The value of the curve if valid, 0 if not
	 */
	float Eval(float XValue,const FString& ContextString) const;

	/** Evaluate the curve if it is valid
	 * @param XValue The input X value to the curve
	 * @param YValue The output Y value from the curve
	 * @param ContextString A string to provide context for where this operation is being carried out
	 * @return True if it filled out YValue with a valid number, false otherwise
	 */
	bool Eval(float XValue, float* YValue,const FString& ContextString) const;

	bool operator==(const FCurveTableRowHandle& Other) const;
	bool operator!=(const FCurveTableRowHandle& Other) const;
	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits< FCurveTableRowHandle > : public TStructOpsTypeTraitsBase2< FCurveTableRowHandle >
{
	enum
	{
		WithPostSerialize = true,
	};
};

/** Macro to call GetCurve with a correct error info. Assumed to be called within a UObject */
#define GETCURVE_REPORTERROR(Handle) Handle.GetCurve(FString::Printf(TEXT("%s.%s"), *GetPathName(), TEXT(#Handle)))

/** Macro to call GetCurve with a correct error info. */
#define GETCURVE_REPORTERROR_WITHPATHNAME(Handle, PathNameString) Handle.GetCurve(FString::Printf(TEXT("%s.%s"), *PathNameString, TEXT(#Handle)))
