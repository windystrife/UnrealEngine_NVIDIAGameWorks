// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "PaperTiledImporterLog.h"
#include "Modules/ModuleManager.h"

//////////////////////////////////////////////////////////////////////////
// FPaperTiledImporterModule

class FPaperTiledImporterModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FPaperTiledImporterModule, PaperTiledImporter);
DEFINE_LOG_CATEGORY(LogPaperTiledImporter);
