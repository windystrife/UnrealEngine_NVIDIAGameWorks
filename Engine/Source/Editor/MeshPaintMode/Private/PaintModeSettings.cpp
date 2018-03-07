// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaintModeSettings.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

UPaintModeSettings::UPaintModeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	PaintMode(EPaintMode::Vertices)
{
}

UPaintModeSettings* UPaintModeSettings::Get()
{
	static UPaintModeSettings* Settings = nullptr;
	if (!Settings)
	{
		Settings = DuplicateObject<UPaintModeSettings>(GetMutableDefault<UPaintModeSettings>(), GetTransientPackage());
		Settings->AddToRoot();
	}

	return Settings;
}

