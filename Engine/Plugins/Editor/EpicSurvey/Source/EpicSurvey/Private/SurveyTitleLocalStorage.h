// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineTitleFileInterface.h"

class FSurveyTitleLocalStorage : public IOnlineTitleFile
{
public:
	static IOnlineTitleFilePtr Create( const FString& RootDirectory );

public:

	virtual bool GetFileContents(const FString& DLName, TArray<uint8>& FileContents) override;

	virtual bool ClearFiles() override;

	virtual bool ClearFile(const FString& DLName) override;

	virtual void DeleteCachedFiles(bool bSkipEnumerated) override;

	virtual bool EnumerateFiles(const FPagedQuery& Page = FPagedQuery()) override;

	virtual void GetFileList(TArray<FCloudFileHeader>& InFileHeaders) override;

	virtual bool ReadFile(const FString& DLName) override;

private:

	FSurveyTitleLocalStorage( const FString& InRootDirectory );

	FString GetFileNameFromDLName( const FString& DLName ) const;

private:

	FString RootDirectory;
	TArray<FCloudFileHeader> FileHeaders;
	TMap< FString, TArray<uint8> > DLNameToFileContents;
};
