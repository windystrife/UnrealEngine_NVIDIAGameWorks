// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbcImportSettings.h"
#include "UObject/Class.h"
#include "UObject/Package.h"

UAbcImportSettings::UAbcImportSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ImportType = EAlembicImportType::StaticMesh;
	bReimport = false;
}

UAbcImportSettings* UAbcImportSettings::Get()
{	
	static UAbcImportSettings* DefaultSettings = nullptr;
	if (!DefaultSettings)
	{
		// This is a singleton, use default object
		DefaultSettings = DuplicateObject(GetMutableDefault<UAbcImportSettings>(), GetTransientPackage());
		DefaultSettings->AddToRoot();
	}
	
	return DefaultSettings;
}
