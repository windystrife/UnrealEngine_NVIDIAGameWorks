// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class IUSDImporterModule : public IModuleInterface
{

public:
	static inline IUSDImporterModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IUSDImporterModule >( "USDImporter" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "USDImporter" );
	}

	virtual class UUSDImporter* GetImporter() = 0;
};

