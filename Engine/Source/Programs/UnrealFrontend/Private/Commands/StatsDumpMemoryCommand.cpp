// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StatsDumpMemoryCommand.h"
#include "IProfilerModule.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

void FStatsMemoryDumpCommand::Run()
{
	FString SourceFilepath;
	FParse::Value(FCommandLine::Get(), TEXT("-INFILE="), SourceFilepath);

	const FName NAME_ProfilerModule = TEXT("Profiler");
	IProfilerModule& ProfilerModule = FModuleManager::LoadModuleChecked<IProfilerModule>(NAME_ProfilerModule);
	ProfilerModule.StatsMemoryDumpCommand(*SourceFilepath);
}
