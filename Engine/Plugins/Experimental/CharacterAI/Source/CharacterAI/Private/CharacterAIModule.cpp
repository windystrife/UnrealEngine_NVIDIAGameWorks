// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CharacterAIModule.h"
#include "Modules/ModuleManager.h"
#include "CharacterAIPrivate.h"

//////////////////////////////////////////////////////////////////////////
// FCharacterAIModule

class FCharacterAIModule : public ICharacterAIModuleInterface
{
public:
	virtual void StartupModule() override
	{
		check(GConfig);		
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FCharacterAIModule, CharacterAI);
DEFINE_LOG_CATEGORY(LogCharacterAI);
