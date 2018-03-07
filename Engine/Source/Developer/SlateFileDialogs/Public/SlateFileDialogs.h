// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISlateFileDialogModule.h"

class FSlateFileDialogsStyle;

class FSlateFileDialogsModule : public ISlateFileDialogsModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	//ISlateFileDialogModule interface

	bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex);

	bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames);

	bool OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		FString& OutFoldername);

	bool SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames);

	ISlateFileDialogsModule* Get();

	FSlateFileDialogsStyle *GetFileDialogsStyle() { return FileDialogStyle; }

private:
	ISlateFileDialogsModule *SlateFileDialog;

	FSlateFileDialogsStyle	*FileDialogStyle;
};
