// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SurveyTitleLocalStorage.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

IOnlineTitleFilePtr FSurveyTitleLocalStorage::Create( const FString& RootDirectory )
{
	return MakeShareable( new FSurveyTitleLocalStorage( RootDirectory ) );
}

FSurveyTitleLocalStorage::FSurveyTitleLocalStorage( const FString& InRootDirectory )
	: RootDirectory( InRootDirectory )
{

}

bool FSurveyTitleLocalStorage::GetFileContents(const FString& DLName, TArray<uint8>& FileContents)
{
	const TArray< uint8 >* ExistingFileContents = DLNameToFileContents.Find( DLName );

	if ( ExistingFileContents == NULL )
	{
		return false;
	}

	FileContents.Append( *ExistingFileContents );
	return true;
}

bool FSurveyTitleLocalStorage::ClearFiles()
{
	FileHeaders.Empty();
	DLNameToFileContents.Empty();
	return true;
}

bool FSurveyTitleLocalStorage::ClearFile(const FString& DLName)
{
	bool ClearedFile = false;
	const FString FileName = GetFileNameFromDLName( DLName );

	for (int Index = 0; Index < FileHeaders.Num(); Index++)
	{
		if ( FileHeaders[Index].DLName == DLName )
		{
			FileHeaders.RemoveAt( Index );
			ClearedFile = true;
		}
	}

	DLNameToFileContents.Remove( DLName );

	return ClearedFile;
}

void FSurveyTitleLocalStorage::DeleteCachedFiles(bool bSkipEnumerated)
{
	// not implemented
}

bool FSurveyTitleLocalStorage::EnumerateFiles(const FPagedQuery& Page)
{
	if (!IFileManager::Get().DirectoryExists(*RootDirectory))
	{
		TriggerOnEnumerateFilesCompleteDelegates(false, TEXT("Directory does not exist"));
		return false;
	}

	const FString WildCard = FString::Printf(TEXT("%s/*"), *RootDirectory);

	TArray<FString> Filenames;
	IFileManager::Get().FindFiles(Filenames, *WildCard, true, false);

	for (int32 FileIdx = 0; FileIdx < Filenames.Num(); ++FileIdx)
	{
		const FString Filename = Filenames[FileIdx];

		FCloudFileHeader NewHeader;
		NewHeader.FileName = Filename;
		NewHeader.DLName = Filename + Lex::ToString(FileIdx);
		NewHeader.FileSize = 0;
		NewHeader.Hash.Empty();

		FileHeaders.Add( NewHeader );
	}

	TriggerOnEnumerateFilesCompleteDelegates(true, FString());

	return true;
}

void FSurveyTitleLocalStorage::GetFileList(TArray<FCloudFileHeader>& InFileHeaders)
{
	InFileHeaders.Append( FileHeaders );
}

bool FSurveyTitleLocalStorage::ReadFile(const FString& DLName)
{
	const TArray< uint8 >* ExistingFileContents = DLNameToFileContents.Find( DLName );

	if ( ExistingFileContents != NULL )
	{
		TriggerOnReadFileCompleteDelegates(true, DLName);
		return true;
	}

	const FString FileName = GetFileNameFromDLName( DLName );

	TArray< uint8 > FileContents;
	if ( !FFileHelper::LoadFileToArray( FileContents, *(RootDirectory + FileName) ) )
	{
		TriggerOnReadFileCompleteDelegates(false, DLName);
		return false;
	}

	DLNameToFileContents.Add( DLName, FileContents );
	TriggerOnReadFileCompleteDelegates(true, DLName);
	return true;
}

FString FSurveyTitleLocalStorage::GetFileNameFromDLName( const FString& DLName ) const
{
	for (int Index = 0; Index < FileHeaders.Num(); Index++)
	{
		if ( FileHeaders[Index].DLName == DLName )
		{
			return FileHeaders[Index].FileName;
		}
	}

	return FString();
}
