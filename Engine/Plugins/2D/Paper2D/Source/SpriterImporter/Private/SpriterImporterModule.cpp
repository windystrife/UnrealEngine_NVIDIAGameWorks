// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "SpriterImporterLog.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "SpriterImporter"

DEFINE_LOG_CATEGORY(LogSpriterImporter);

//////////////////////////////////////////////////////////////////////////
// FSpriterImporterModule

class FSpriterImporterModule : public FDefaultModuleImpl
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

IMPLEMENT_MODULE(FSpriterImporterModule, SpriterImporter);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
