// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
* Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
* code everywhere.  And we can have consistent naming for all our files.
*
*/

// Core includes.
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"

FLoadTimeTracker::FLoadTimeTracker()
{
	ResetRawLoadTimes();
}

void FLoadTimeTracker::ReportScopeTime(double ScopeTime, const FName ScopeLabel)
{
	check(IsInGameThread());
	TArray<double>& LoadTimes = TimeInfo.FindOrAdd(ScopeLabel);
	LoadTimes.Add(ScopeTime);
}


void FLoadTimeTracker::DumpHighLevelLoadTimes() const
{
	double TotalTime = 0.0;
	UE_LOG(LogLoad, Log, TEXT("------------- Load times -------------"));
	for(auto Itr = TimeInfo.CreateConstIterator(); Itr; ++Itr)
	{
		const FString KeyName = Itr.Key().ToString();
		const TArray<double>& LoadTimes = Itr.Value();
		if(LoadTimes.Num() == 1)
		{
			TotalTime += Itr.Value()[0];
			UE_LOG(LogLoad, Log, TEXT("%s: %f"), *KeyName, Itr.Value()[0]);
		}
		else
		{
			double InnerTotal = 0.0;
			for(int Index = 0; Index < LoadTimes.Num(); ++Index)
			{
				InnerTotal += Itr.Value()[Index];
				UE_LOG(LogLoad, Log, TEXT("%s[%d]: %f"), *KeyName, Index, LoadTimes[Index]);
			}

			UE_LOG(LogLoad, Log, TEXT("    Sub-Total: %f"), InnerTotal);

			TotalTime += InnerTotal;
		}
		
	}
	UE_LOG(LogLoad, Log, TEXT("------------- ---------- -------------"));
	UE_LOG(LogLoad, Log, TEXT("Total Load times: %f"), TotalTime);
}

void FLoadTimeTracker::ResetHighLevelLoadTimes()
{
	static bool bActuallyReset = !FParse::Param(FCommandLine::Get(), TEXT("NoLoadTrackClear"));
	if(bActuallyReset)
	{
		TimeInfo.Reset();
	}
}

void FLoadTimeTracker::DumpRawLoadTimes() const
{
#if ENABLE_LOADTIME_RAW_TIMINGS
	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));
	UE_LOG(LogStreaming, Display, TEXT("Async Loading Stats"));
	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));
	UE_LOG(LogStreaming, Display, TEXT("AsyncLoadingTime: %f"), AsyncLoadingTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateAsyncPackagesFromQueueTime: %f"), CreateAsyncPackagesFromQueueTime);
	UE_LOG(LogStreaming, Display, TEXT("ProcessAsyncLoadingTime: %f"), ProcessAsyncLoadingTime);
	UE_LOG(LogStreaming, Display, TEXT("ProcessLoadedPackagesTime: %f"), ProcessLoadedPackagesTime);
	//UE_LOG(LogStreaming, Display, TEXT("SerializeTaggedPropertiesTime: %f"), SerializeTaggedPropertiesTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateLinkerTime: %f"), CreateLinkerTime);
	UE_LOG(LogStreaming, Display, TEXT("FinishLinkerTime: %f"), FinishLinkerTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateImportsTime: %f"), CreateImportsTime);
	UE_LOG(LogStreaming, Display, TEXT("CreateExportsTime: %f"), CreateExportsTime);
	UE_LOG(LogStreaming, Display, TEXT("PreLoadObjectsTime: %f"), PreLoadObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("PostLoadObjectsTime: %f"), PostLoadObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("PostLoadDeferredObjectsTime: %f"), PostLoadDeferredObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("FinishObjectsTime: %f"), FinishObjectsTime);
	UE_LOG(LogStreaming, Display, TEXT("MaterialPostLoad: %f"), MaterialPostLoad);
	UE_LOG(LogStreaming, Display, TEXT("MaterialInstancePostLoad: %f"), MaterialInstancePostLoad);
	UE_LOG(LogStreaming, Display, TEXT("SerializeInlineShaderMaps: %f"), SerializeInlineShaderMaps);
	UE_LOG(LogStreaming, Display, TEXT("MaterialSerializeTime: %f"), MaterialSerializeTime);
	UE_LOG(LogStreaming, Display, TEXT("MaterialInstanceSerializeTime: %f"), MaterialInstanceSerializeTime);
	UE_LOG(LogStreaming, Display, TEXT(""));
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_CreateLoader: %f"), LinkerLoad_CreateLoader);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializePackageFileSummary: %f"), LinkerLoad_SerializePackageFileSummary);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeNameMap: %f"), LinkerLoad_SerializeNameMap);
	UE_LOG(LogStreaming, Display, TEXT("\tProcessingEntries: %f"), LinkerLoad_SerializeNameMap_ProcessingEntries);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeGatherableTextDataMap: %f"), LinkerLoad_SerializeGatherableTextDataMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeImportMap: %f"), LinkerLoad_SerializeImportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeExportMap: %f"), LinkerLoad_SerializeExportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FixupImportMap: %f"), LinkerLoad_FixupImportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FixupExportMap: %f"), LinkerLoad_FixupExportMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializeDependsMap: %f"), LinkerLoad_SerializeDependsMap);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_SerializePreloadDependencies: %f"), LinkerLoad_SerializePreloadDependencies);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_CreateExportHash: %f"), LinkerLoad_CreateExportHash);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FindExistingExports: %f"), LinkerLoad_FindExistingExports);
	UE_LOG(LogStreaming, Display, TEXT("LinkerLoad_FinalizeCreation: %f"), LinkerLoad_FinalizeCreation);

	UE_LOG(LogStreaming, Display, TEXT("Package_FinishLinker: %f"), Package_FinishLinker);
	UE_LOG(LogStreaming, Display, TEXT("Package_LoadImports: %f"), Package_LoadImports);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateImports: %f"), Package_CreateImports);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateLinker: %f"), Package_CreateLinker);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateExports: %f"), Package_CreateExports);
	UE_LOG(LogStreaming, Display, TEXT("Package_PreLoadObjects: %f"), Package_PreLoadObjects);
	UE_LOG(LogStreaming, Display, TEXT("Package_ExternalReadDependencies: %f"), Package_ExternalReadDependencies);
	UE_LOG(LogStreaming, Display, TEXT("Package_PostLoadObjects: %f"), Package_PostLoadObjects);
	UE_LOG(LogStreaming, Display, TEXT("Package_Tick: %f"), Package_Tick);
	UE_LOG(LogStreaming, Display, TEXT("Package_CreateAsyncPackagesFromQueue: %f"), Package_CreateAsyncPackagesFromQueue);
	UE_LOG(LogStreaming, Display, TEXT("Package_EventIOWait: %f"), Package_EventIOWait);

	UE_LOG(LogStreaming, Display, TEXT("TickAsyncLoading_ProcessLoadedPackages: %f"), TickAsyncLoading_ProcessLoadedPackages);

	UE_LOG(LogStreaming, Display, TEXT("Package_Temp1: %f"), Package_Temp1);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp2: %f"), Package_Temp2);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp3: %f"), Package_Temp3);
	UE_LOG(LogStreaming, Display, TEXT("Package_Temp4: %f"), Package_Temp4);

	UE_LOG(LogStreaming, Display, TEXT("Graph_AddNode: %f     %u"), Graph_AddNode, Graph_AddNodeCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_AddArc: %f     %u"), Graph_AddArc, Graph_AddArcCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_RemoveNode: %f     %u"), Graph_RemoveNode, Graph_RemoveNodeCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_RemoveNodeFire: %f     %u"), Graph_RemoveNodeFire, Graph_RemoveNodeFireCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_DoneAddingPrerequistesFireIfNone: %f     %u"), Graph_DoneAddingPrerequistesFireIfNone, Graph_DoneAddingPrerequistesFireIfNoneCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_DoneAddingPrerequistesFireIfNoneFire: %f     %u"), Graph_DoneAddingPrerequistesFireIfNoneFire, Graph_DoneAddingPrerequistesFireIfNoneFireCnt);
	UE_LOG(LogStreaming, Display, TEXT("Graph_Misc: %f     %u"), Graph_Misc, Graph_MiscCnt);
	UE_LOG(LogStreaming, Display, TEXT("-------------------------------------------------"));

#endif

}

void FLoadTimeTracker::ResetRawLoadTimes()
{
#if ENABLE_LOADTIME_RAW_TIMINGS
	CreateAsyncPackagesFromQueueTime = 0.0;
	ProcessAsyncLoadingTime = 0.0;
	ProcessLoadedPackagesTime = 0.0;
	SerializeTaggedPropertiesTime = 0.0;
	CreateLinkerTime = 0.0;
	FinishLinkerTime = 0.0;
	CreateImportsTime = 0.0;
	CreateExportsTime = 0.0;
	PreLoadObjectsTime = 0.0;
	PostLoadObjectsTime = 0.0;
	PostLoadDeferredObjectsTime = 0.0;
	FinishObjectsTime = 0.0;
	MaterialPostLoad = 0.0;
	MaterialInstancePostLoad = 0.0;
	SerializeInlineShaderMaps = 0.0;
	MaterialSerializeTime = 0.0;
	MaterialInstanceSerializeTime = 0.0;
	AsyncLoadingTime = 0.0;
	CreateMetaDataTime = 0.0;

	LinkerLoad_CreateLoader = 0.0;
	LinkerLoad_SerializePackageFileSummary = 0.0;
	LinkerLoad_SerializeNameMap = 0.0;
	LinkerLoad_SerializeGatherableTextDataMap = 0.0;
	LinkerLoad_SerializeImportMap = 0.0;
	LinkerLoad_SerializeExportMap = 0.0;
	LinkerLoad_FixupImportMap = 0.0;
	LinkerLoad_FixupExportMap = 0.0;
	LinkerLoad_SerializeDependsMap = 0.0;
	LinkerLoad_SerializePreloadDependencies = 0.0;
	LinkerLoad_CreateExportHash = 0.0;
	LinkerLoad_FindExistingExports = 0.0;
	LinkerLoad_FinalizeCreation = 0.0;

	Package_FinishLinker = 0.0;
	Package_LoadImports = 0.0;
	Package_CreateImports = 0.0;
	Package_CreateLinker = 0.0;
	Package_CreateExports = 0.0;
	Package_PreLoadObjects = 0.0;
	Package_ExternalReadDependencies = 0.0;
	Package_PostLoadObjects = 0.0;
	Package_Tick = 0.0;
	Package_CreateAsyncPackagesFromQueue = 0.0;
	Package_CreateMetaData = 0.0;
	Package_EventIOWait = 0.0;

	Package_Temp1 = 0.0;
	Package_Temp2 = 0.0;
	Package_Temp3 = 0.0;
	Package_Temp4 = 0.0;

	Graph_AddNode = 0.0;
	Graph_AddNodeCnt = 0;

	Graph_AddArc = 0.0;
	Graph_AddArcCnt = 0;

	Graph_RemoveNode = 0.0;
	Graph_RemoveNodeCnt = 0;

	Graph_RemoveNodeFire = 0.0;
	Graph_RemoveNodeFireCnt = 0;

	Graph_DoneAddingPrerequistesFireIfNone = 0.0;
	Graph_DoneAddingPrerequistesFireIfNoneCnt = 0;

	Graph_DoneAddingPrerequistesFireIfNoneFire = 0.0;
	Graph_DoneAddingPrerequistesFireIfNoneFireCnt = 0;

	Graph_Misc = 0.0;
	Graph_MiscCnt = 0;

	TickAsyncLoading_ProcessLoadedPackages = 0.0;

	LinkerLoad_SerializeNameMap_ProcessingEntries = 0.0;

#endif

}

static FAutoConsoleCommand LoadTimerDumpCmd(
	TEXT("LoadTimes.DumpTracking"),
	TEXT("Dump high level load times being tracked"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::DumpHighLevelLoadTimesStatic)
	);
static FAutoConsoleCommand LoadTimerDumpLowCmd(
	TEXT("LoadTimes.DumpTrackingLow"),
	TEXT("Dump low level load times being tracked"),
	FConsoleCommandDelegate::CreateStatic(&FLoadTimeTracker::DumpRawLoadTimesStatic)
	);
