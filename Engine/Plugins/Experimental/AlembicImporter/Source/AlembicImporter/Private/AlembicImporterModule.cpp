// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AlembicImporterModule.h"
#include "AlembicLibraryModule.h"

class FAlembicImporterModule : public IAlembicImporterModuleInterface
{
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked< IAlembicLibraryModule >("AlembicLibrary");
	}
};

IMPLEMENT_MODULE(FAlembicImporterModule, AlembicImporter);