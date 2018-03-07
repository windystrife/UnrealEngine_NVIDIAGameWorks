// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IScriptGeneratorPluginInterface.h"
#include "UnrealHeaderTool.h"
#include "UObject/ErrorException.h"
#include "Containers/Algo/FindSortedStringCaseInsensitive.h"

EBuildModuleType::Type EBuildModuleType::Parse(const TCHAR* Value)
{
	static const TCHAR* AlphabetizedTypes[] = {
		TEXT("EngineDeveloper"),
		TEXT("EngineEditor"),
		TEXT("EngineRuntime"),
		TEXT("EngineThirdParty"),
		TEXT("GameDeveloper"),
		TEXT("GameEditor"),
		TEXT("GameRuntime"),
		TEXT("GameThirdParty"),
		TEXT("Program")
	};

	int32 TypeIndex = Algo::FindSortedStringCaseInsensitive(Value, AlphabetizedTypes);
	if (TypeIndex < 0)
	{
		FError::Throwf(TEXT("Unrecognized EBuildModuleType name: %s"), Value);
	}

	static EBuildModuleType::Type AlphabetizedValues[] = {
		EngineDeveloper,
		EngineEditor,
		EngineRuntime,
		EngineThirdParty,
		GameDeveloper,
		GameEditor,
		GameRuntime,
		GameThirdParty,
		Program
	};

	return AlphabetizedValues[TypeIndex];
}
