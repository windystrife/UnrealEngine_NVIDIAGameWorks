// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CodeProjectItem.h"

DECLARE_DELEGATE_TwoParams(FOnDirectoryScanned, const FString& /*InPathName*/, ECodeProjectItemType::Type /*InType*/);

class FDirectoryScanner
{
public:
	static bool Tick();

	static void AddDirectory(const FString& PathName, const FOnDirectoryScanned& OnDirectoryScanned);

	static bool IsScanning() ;

public:
	static TArray<struct FDirectoryScannerCommand*> CommandQueue;

	static bool bDataDirty;
};
