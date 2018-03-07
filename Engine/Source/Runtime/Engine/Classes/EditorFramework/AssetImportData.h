// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/SecureHash.h"
#include "AssetImportData.generated.h"

/** Struct that is used to store an array of asset import data as an asset registry tag */
USTRUCT()
struct ENGINE_API FAssetImportInfo
{
	GENERATED_BODY()
	
#if WITH_EDITORONLY_DATA

	FAssetImportInfo() {}

	FAssetImportInfo(const FAssetImportInfo& In) : SourceFiles(In.SourceFiles) { }
	FAssetImportInfo& operator=(const FAssetImportInfo& In) { SourceFiles = In.SourceFiles; return *this; }

	FAssetImportInfo(FAssetImportInfo&& In) : SourceFiles(MoveTemp(In.SourceFiles)) { }
	FAssetImportInfo& operator=(FAssetImportInfo&& In) { SourceFiles = MoveTemp(In.SourceFiles); return *this; }

	struct FSourceFile
	{
		FSourceFile(FString InRelativeFilename, const FDateTime& InTimestamp = 0, const FMD5Hash& InFileHash = FMD5Hash())
			: RelativeFilename(MoveTemp(InRelativeFilename))
			, Timestamp(InTimestamp)
			, FileHash(InFileHash)
		{}

		/** The path to the file that this asset was imported from. Relative to either the asset's package, BaseDir(), or absolute */
		FString RelativeFilename;

		/** The timestamp of the file when it was imported (as UTC). 0 when unknown. */
		FDateTime Timestamp;

		/** The MD5 hash of the file when it was imported. Invalid when unknown. */
		FMD5Hash FileHash;
	};

	/** Convert this import information to JSON */
	FString ToJson() const;

	/** Attempt to parse an asset import structure from the specified json string. */
	static TOptional<FAssetImportInfo> FromJson(FString InJsonString);

	/** Insert information pertaining to a new source file to this structure */
	void Insert(const FSourceFile& InSourceFile) { SourceFiles.Add(InSourceFile); }

	/** Array of information pertaining to the source files that this asset was imported from */
	TArray<FSourceFile> SourceFiles;

#endif // WITH_EDITORONLY_DATA
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnImportDataChanged, const FAssetImportInfo& /*OldData*/, const class UAssetImportData* /* NewData */);

/* todo: Make this class better suited to multiple import paths - maybe have FAssetImportInfo use a map rather than array? */
UCLASS(EditInlineNew)
class ENGINE_API UAssetImportData : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	/** Only valid in the editor */
	virtual bool IsEditorOnly() const override { return true; }

#if WITH_EDITORONLY_DATA

	/** Path to the resource used to construct this static mesh. Relative to the object's package, BaseDir() or absolute */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	/** Source file data describing the files that were used to import this asset. */
	UPROPERTY(VisibleAnywhere, Category="File Path")
	FAssetImportInfo SourceData;

public:

	/** Static event that is broadcast whenever any asset has updated its import data */
	static FOnImportDataChanged OnImportDataChanged;

	/** Update this import data using the specified file. Called when an asset has been imported from a file. */
	void Update(const FString& AbsoluteFilename, FMD5Hash *Md5Hash = nullptr);

	//@third party BEGIN SIMPLYGON
	/** Update this import data using the specified filename and Precomputed Hash. */
	void Update(const FString& AbsoluteFileName, const FMD5Hash PreComputedHash);
	//@third party END SIMPLYGON

	/** Update this import data using the specified filename. Will not update the imported timestamp or MD5 (so we can update files when they move). */
	void UpdateFilenameOnly(const FString& InPath);

	/** Update the filename at the specific specified index. Will not update the imported timestamp or MD5 (so we can update files when they move). */
	void UpdateFilenameOnly(const FString& InPath, int32 Index);

	/** Helper function to return the first filename stored in this data. The resulting filename will be absolute (ie, not relative to the asset).  */
	FString GetFirstFilename() const;

	/** Const access to the source file data */
	const FAssetImportInfo& GetSourceData() const { return SourceData; }

	/** Extract all the (resolved) filenames from this data  */
	void ExtractFilenames(TArray<FString>& AbsoluteFilenames) const;

	/** Extract all the (resolved) filenames from this data  */
	TArray<FString> ExtractFilenames() const;

	/** Resolve a filename that is relative to either the specified package, BaseDir() or absolute */
	static FString ResolveImportFilename(const FString& InRelativePath, const UPackage* Outermost);

	virtual void PostLoad() override;

protected:

	/** Convert an absolute import path so that it's relative to either this object's package, BaseDir() or leave it absolute */
	FString SanitizeImportFilename(const FString& InPath) const;

	/** Resolve a filename that is relative to either this object's package, BaseDir() or absolute */
	FString ResolveImportFilename(const FString& InRelativePath) const;

	/** Overridden serialize function to write out the underlying data as json */
	virtual void Serialize(FArchive& Ar) override;
	
#endif		// WITH_EDITORONLY_DATA
};


