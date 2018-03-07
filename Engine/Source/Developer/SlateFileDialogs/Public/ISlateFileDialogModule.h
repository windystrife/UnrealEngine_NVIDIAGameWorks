// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
 * Interface for slate file dialog modules.
 */
class ISlateFileDialogsModule : public IModuleInterface
{
public:
	/**
	 * Virtual destructor.
	 */
	virtual ~ISlateFileDialogsModule( ) { }
	
	virtual bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames, int32& OutFilterIndex)
	{
		return false;
	}
	
	virtual bool OpenFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
	{
		return false;
	}
	
	virtual bool OpenDirectoryDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		FString& OutFoldername)
	{
		return false;
	}
	
	virtual bool SaveFileDialog(const void* ParentWindowHandle, const FString& DialogTitle, const FString& DefaultPath,
		const FString& DefaultFile, const FString& FileTypes, uint32 Flags, TArray<FString>& OutFilenames)
	{
		return false;
	}

	virtual ISlateFileDialogsModule* Get() { return nullptr; }

private:
	ISlateFileDialogsModule *SlateFileDialog;
};
