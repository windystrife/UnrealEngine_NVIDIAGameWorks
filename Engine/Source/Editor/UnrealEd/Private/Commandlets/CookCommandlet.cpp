// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookCommandlet.cpp: Commandlet for cooking content
=============================================================================*/

#include "Commandlets/CookCommandlet.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/RedirectCollector.h"
#include "IPlatformFileSandboxWrapper.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Settings/ProjectPackagingSettings.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "Serialization/ArrayWriter.h"
#include "PackageHelperFunctions.h"
#include "GlobalShader.h"
#include "ShaderCompiler.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "INetworkFileSystemModule.h"
#include "GameDelegates.h"
#include "CookerSettings.h"
#include "ShaderCompiler.h"
#include "HAL/MemoryMisc.h"
#include "ProfilingDebugging/CookStats.h"
#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogCookCommandlet, Log, All);

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsET.h"

namespace DetailedCookStats
{
	FString CookProject;
	FString TargetPlatforms;
	double CookWallTimeSec = 0.0;
	double StartupWallTimeSec = 0.0;
	double CookByTheBookTimeSec = 0.0;
	double StartCookByTheBookTimeSec = 0.0;
	extern double TickCookOnTheSideTimeSec;
	extern double TickCookOnTheSideLoadPackagesTimeSec;
	extern double TickCookOnTheSideResolveRedirectorsTimeSec;
	extern double TickCookOnTheSideSaveCookedPackageTimeSec;
	extern double TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec;
	extern double TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec;
	extern double GameCookModificationDelegateTimeSec;
	double TickLoopGCTimeSec = 0.0;
	double TickLoopRecompileShaderRequestsTimeSec = 0.0;
	double TickLoopShaderProcessAsyncResultsTimeSec = 0.0;
	double TickLoopProcessDeferredCommandsTimeSec = 0.0;
	double TickLoopTickCommandletStatsTimeSec = 0.0;

	FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		const FString StatName(TEXT("Cook.Profile"));
		TArray<FCookStatsManager::StringKeyValue> Attrs;
		#define ADD_COOK_STAT_FLT(Path, Name) AddStat(StatName, FCookStatsManager::CreateKeyValueArray(TEXT("Path"), TEXT(Path), TEXT(#Name), Name))
		ADD_COOK_STAT_FLT(" 0", CookWallTimeSec);
		ADD_COOK_STAT_FLT(" 0. 0", StartupWallTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1", CookByTheBookTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 0", StartCookByTheBookTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 0. 0", GameCookModificationDelegateTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1", TickCookOnTheSideTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 0", TickCookOnTheSideLoadPackagesTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 1", TickCookOnTheSideSaveCookedPackageTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 1. 0", TickCookOnTheSideResolveRedirectorsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 2", TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 1. 3", TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 2", TickLoopGCTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 3", TickLoopRecompileShaderRequestsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 4", TickLoopShaderProcessAsyncResultsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 5", TickLoopProcessDeferredCommandsTimeSec);
		ADD_COOK_STAT_FLT(" 0. 1. 6", TickLoopTickCommandletStatsTimeSec);
		#undef ADD_COOK_STAT_FLT
	});

	/** Gathers the cook stats registered with the FCookStatsManager delegate and logs them as a CSV. */
	static void LogCookStats(const FString& CookCmdLine)
	{
		// Optionally create an analytics provider to send stats to for central collection.
		if (GIsBuildMachine || FParse::Param(FCommandLine::Get(), TEXT("SendCookAnalytics")))
		{
			FString APIServerET;
			// This value is set by an INI private to Epic.
			if (GConfig->GetString(TEXT("CookAnalytics"), TEXT("APIServer"), APIServerET, GEngineIni))
			{
				TSharedPtr<IAnalyticsProviderET> CookAnalytics = FAnalyticsET::Get().CreateAnalyticsProvider(FAnalyticsET::Config(TEXT("Cook"), APIServerET, FString(), true));
				if (CookAnalytics.IsValid())
				{
					// start the session
					CookAnalytics->SetUserID(FString(FPlatformProcess::ComputerName()) + FString(TEXT("\\")) + FString(FPlatformProcess::UserName(false)));
					CookAnalytics->StartSession(MakeAnalyticsEventAttributeArray(
						TEXT("Project"), CookProject,
						TEXT("CmdLine"), CookCmdLine,
						TEXT("IsBuildMachine"), GIsBuildMachine,
						TEXT("TargetPlatforms"), TargetPlatforms
					));
					// Sends each cook stat to the analytics provider.
					auto SendCookStatsToAnalytics = [CookAnalytics](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
					{
						// convert all stats directly to an analytics event
						TArray<FAnalyticsEventAttribute> StatAttrs;
						StatAttrs.Reset(StatAttributes.Num());
						for (const auto& Attr : StatAttributes)
						{
							StatAttrs.Emplace(Attr.Key, Attr.Value);
						}
						CookAnalytics->RecordEvent(StatName, StatAttrs);
					};
					FCookStatsManager::LogCookStats(SendCookStatsToAnalytics);
				}
			}
		}

		/** Used for custom logging of DDC Resource usage stats. */
		struct FDDCResourceUsageStat
		{
		public:
			FDDCResourceUsageStat(FString InAssetType, double InTotalTimeSec, bool bIsGameThreadTime, double InSizeMB, int64 InAssetsBuilt) : AssetType(MoveTemp(InAssetType)), TotalTimeSec(InTotalTimeSec), GameThreadTimeSec(bIsGameThreadTime ? InTotalTimeSec : 0.0), SizeMB(InSizeMB), AssetsBuilt(InAssetsBuilt) {}
			void Accumulate(const FDDCResourceUsageStat& OtherStat)
			{
				TotalTimeSec += OtherStat.TotalTimeSec;
				GameThreadTimeSec += OtherStat.GameThreadTimeSec;
				SizeMB += OtherStat.SizeMB;
				AssetsBuilt += OtherStat.AssetsBuilt;
			}
			FString AssetType;
			double TotalTimeSec;
			double GameThreadTimeSec;
			double SizeMB;
			int64 AssetsBuilt;
		};

		/** Used for custom TSet comparison of DDC Resource usage stats. */
		struct FDDCResourceUsageStatKeyFuncs : BaseKeyFuncs<FDDCResourceUsageStat, FString, false>
		{
			static const FString& GetSetKey(const FDDCResourceUsageStat& Element) { return Element.AssetType; }
			static bool Matches(const FString& A, const FString& B) { return A == B; }
			static uint32 GetKeyHash(const FString& Key) { return GetTypeHash(Key); }
		};

		/** Used to store profile data for custom logging. */
		struct FCookProfileData
		{
		public:
			FCookProfileData(FString InPath, FString InKey, FString InValue) : Path(MoveTemp(InPath)), Key(MoveTemp(InKey)), Value(MoveTemp(InValue)) {}
			FString Path;
			FString Key;
			FString Value;
		};

		// instead of printing the usage stats generically, we capture them so we can log a subset of them in an easy-to-read way.
		TSet<FDDCResourceUsageStat, FDDCResourceUsageStatKeyFuncs> DDCResourceUsageStats;
		TArray<FCookStatsManager::StringKeyValue> DDCSummaryStats;
		TArray<FCookProfileData> CookProfileData;

		/** this functor will take a collected cooker stat and log it out using some custom formatting based on known stats that are collected.. */
		auto LogStatsFunc = [&DDCResourceUsageStats, &DDCSummaryStats, &CookProfileData](const FString& StatName, const TArray<FCookStatsManager::StringKeyValue>& StatAttributes)
		{
			// Some stats will use custom formatting to make a visibly pleasing summary.
			bool bStatUsedCustomFormatting = false;

			if (StatName == TEXT("DDC.Usage"))
			{
				// Don't even log this detailed DDC data. It's mostly only consumable by ingestion into pivot tools.
				bStatUsedCustomFormatting = true;
			}
			else if (StatName.EndsWith(TEXT(".Usage"), ESearchCase::IgnoreCase))
			{
				// Anything that ends in .Usage is assumed to be an instance of FCookStats.FDDCResourceUsageStats. We'll log that using custom formatting.
				FString AssetType = StatName;
				AssetType.RemoveFromEnd(TEXT(".Usage"), ESearchCase::IgnoreCase);
				// See if the asset has a subtype (found via the "Node" parameter")
				const FCookStatsManager::StringKeyValue* AssetSubType = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Node"); });
				if (AssetSubType && AssetSubType->Value.Len() > 0)
				{
					AssetType += FString::Printf(TEXT(" (%s)"), *AssetSubType->Value);
				}
				// Pull the Time and Size attributes and AddOrAccumulate them into the set of stats. Ugly string/container manipulation code courtesy of UE4/C++.
				const FCookStatsManager::StringKeyValue* AssetTimeSecAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("TimeSec"); });
				double AssetTimeSec = 0.0;
				if (AssetTimeSecAttr)
				{
					Lex::FromString(AssetTimeSec, *AssetTimeSecAttr->Value);
				}
				const FCookStatsManager::StringKeyValue* AssetSizeMBAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("MB"); });
				double AssetSizeMB = 0.0;
				if (AssetSizeMBAttr)
				{
					Lex::FromString(AssetSizeMB, *AssetSizeMBAttr->Value);
				}
				const FCookStatsManager::StringKeyValue* ThreadNameAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("ThreadName"); });
				bool bIsGameThreadTime = ThreadNameAttr != nullptr && ThreadNameAttr->Value == TEXT("GameThread");

				const FCookStatsManager::StringKeyValue* HitOrMissAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("HitOrMiss"); });
				bool bWasMiss = HitOrMissAttr != nullptr && HitOrMissAttr->Value == TEXT("Miss");
				int64 AssetsBuilt = 0;
				if (bWasMiss)
				{
					const FCookStatsManager::StringKeyValue* CountAttr = StatAttributes.FindByPredicate([](const FCookStatsManager::StringKeyValue& Item) { return Item.Key == TEXT("Count"); });
					if (CountAttr)
					{
						Lex::FromString(AssetsBuilt, *CountAttr->Value);
					}
				}


				FDDCResourceUsageStat Stat(AssetType, AssetTimeSec, bIsGameThreadTime, AssetSizeMB, AssetsBuilt);
				FDDCResourceUsageStat* ExistingStat = DDCResourceUsageStats.Find(Stat.AssetType);
				if (ExistingStat)
				{
					ExistingStat->Accumulate(Stat);
				}
				else
				{
					DDCResourceUsageStats.Add(Stat);
				}
				bStatUsedCustomFormatting = true;
			}
			else if (StatName == TEXT("DDC.Summary"))
			{
				DDCSummaryStats = StatAttributes;
				bStatUsedCustomFormatting = true;
			}
			else if (StatName == TEXT("Cook.Profile"))
			{
				if (StatAttributes.Num() >= 2)
				{
					CookProfileData.Emplace(StatAttributes[0].Value, StatAttributes[1].Key, StatAttributes[1].Value);
				}
				bStatUsedCustomFormatting = true;
			}

			// if a stat doesn't use custom formatting, just spit out the raw info.
			if (!bStatUsedCustomFormatting)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%s"), *StatName);
				// log each key/value pair, with the equal signs lined up.
				for (const auto& Attr : StatAttributes)
				{
					UE_LOG(LogCookCommandlet, Display, TEXT("    %s=%s"), *Attr.Key, *Attr.Value);
				}
			}
		};

		UE_LOG(LogCookCommandlet, Display, TEXT("Misc Cook Stats"));
		UE_LOG(LogCookCommandlet, Display, TEXT("==============="));
		FCookStatsManager::LogCookStats(LogStatsFunc);

		// DDC Usage stats are custom formatted, and the above code just accumulated them into a TSet. Now log it with our special formatting for readability.
		if (CookProfileData.Num() > 0)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("Cook Profile"));
			UE_LOG(LogCookCommandlet, Display, TEXT("============"));
			for (const auto& ProfileEntry : CookProfileData)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%s.%s=%s"), *ProfileEntry.Path, *ProfileEntry.Key, *ProfileEntry.Value);
			}
		}
		if (DDCSummaryStats.Num() > 0)
		{
			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("DDC Summary Stats"));
			UE_LOG(LogCookCommandlet, Display, TEXT("================="));
			for (const auto& Attr : DDCSummaryStats)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%-14s=%10s"), *Attr.Key, *Attr.Value);
			}
		}
		if (DDCResourceUsageStats.Num() > 0)
		{
			// sort the list
			TArray<FDDCResourceUsageStat> SortedDDCResourceUsageStats;
			SortedDDCResourceUsageStats.Empty(DDCResourceUsageStats.Num());
			for (const FDDCResourceUsageStat& Stat : DDCResourceUsageStats)
			{
				SortedDDCResourceUsageStats.Emplace(Stat);
			}
			SortedDDCResourceUsageStats.Sort([](const FDDCResourceUsageStat& LHS, const FDDCResourceUsageStat& RHS)
			{
				return LHS.TotalTimeSec > RHS.TotalTimeSec;
			});

			UE_LOG(LogCookCommandlet, Display, TEXT(""));
			UE_LOG(LogCookCommandlet, Display, TEXT("DDC Resource Stats"));
			UE_LOG(LogCookCommandlet, Display, TEXT("======================================================================================================="));
			UE_LOG(LogCookCommandlet, Display, TEXT("Asset Type                          Total Time (Sec)  GameThread Time (Sec)  Assets Built  MB Processed"));
			UE_LOG(LogCookCommandlet, Display, TEXT("----------------------------------  ----------------  ---------------------  ------------  ------------"));
			for (const FDDCResourceUsageStat& Stat : SortedDDCResourceUsageStats)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("%-34s  %16.2f  %21.2f  %12d  %12.2f"), *Stat.AssetType, Stat.TotalTimeSec, Stat.GameThreadTimeSec, Stat.AssetsBuilt, Stat.SizeMB);
			}
		}
	}
}
#endif

UCookCommandlet::UCookCommandlet( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

	LogToConsole = false;
}

bool UCookCommandlet::CookOnTheFly( FGuid InstanceId, int32 Timeout, bool bForceClose )
{
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();

	struct FScopeRootObject
	{
		UObject *Object;
		FScopeRootObject( UObject *InObject ) : Object( InObject )
		{
			Object->AddToRoot();
		}

		~FScopeRootObject()
		{
			Object->RemoveFromRoot();
		}
	};

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	FScopeRootObject S(CookOnTheFlyServer);

	UCookerSettings const* CookerSettings = GetDefault<UCookerSettings>();
	ECookInitializationFlags IterateFlags = ECookInitializationFlags::Iterative;

	ECookInitializationFlags CookFlags = ECookInitializationFlags::None;
	CookFlags |= bIterativeCooking ? IterateFlags : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookOnTheFlyServer->Initialize( ECookMode::CookOnTheFly, CookFlags );

	bool BindAnyPort = InstanceId.IsValid();

	if ( CookOnTheFlyServer->StartNetworkFileServer(BindAnyPort) == false )
	{
		return false;
	}

	if ( InstanceId.IsValid() )
	{
		if ( CookOnTheFlyServer->BroadcastFileserverPresence(InstanceId) == false )
		{
			return false;
		}
	}

	// Garbage collection should happen when either
	//	1. We have cooked a map (configurable asset type)
	//	2. We have cooked non-map packages and...
	//		a. we have accumulated 50 (configurable) of these since the last GC.
	//		b. we have been idle for 20 (configurable) seconds.
	struct FCookOnTheFlyGCController
	{
	public:
		FCookOnTheFlyGCController(const UCookOnTheFlyServer* COtFServer)
			: PackagesPerGC(COtFServer->GetPackagesPerGC())
			, IdleTimeToGC(COtFServer->GetIdleTimeToGC())
			, bShouldGC(true)
			, PackagesCookedSinceLastGC(0)
			, LastCookActionTime(FPlatformTime::Seconds())
		{}

		/** Tntended to be called with stats from a UCookOnTheFlyServer::TickCookOnTheSide() call. Determines if we should be calling GC after TickCookOnTheSide(). */
		void Update(uint32 CookedCount, UCookOnTheFlyServer::ECookOnTheSideResult ResultFlags)
		{
			if (ResultFlags & (UCookOnTheFlyServer::COSR_CookedMap | UCookOnTheFlyServer::COSR_CookedPackage | UCookOnTheFlyServer::COSR_WaitingOnCache))
			{
				LastCookActionTime = FPlatformTime::Seconds();
			}
			
			if (ResultFlags & UCookOnTheFlyServer::COSR_RequiresGC)
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Cooker cooked a map since last gc... collecting garbage"));
				bShouldGC |= true;
			}

			PackagesCookedSinceLastGC += CookedCount;
			if ((PackagesPerGC > 0) && (PackagesCookedSinceLastGC > PackagesPerGC))
			{
				UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has exceeded max number of non map packages since last gc"));
				bShouldGC |= true;
			}

			// we don't want to gc if we are waiting on cache of objects. this could clean up objects which we will need to reload next frame
			bPosteponeGC = (ResultFlags & UCookOnTheFlyServer::COSR_WaitingOnCache) != 0;
		}

		/** Runs GC if Update() determined it should happen. Also checks the idle time against the limit, and runs GC then if packages have been loaded. */
		void ConditionallyCollectGarbage(const UCookOnTheFlyServer* COtFServer)
		{
			if (!bShouldGC)
			{
				if (PackagesCookedSinceLastGC > 0 && IdleTimeToGC > 0)
				{
					double IdleTime = FPlatformTime::Seconds() - LastCookActionTime;
					if (IdleTime >= IdleTimeToGC)
					{
						UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has been idle for long time gc"));
						bShouldGC |= true;
					}
				}

				if (!bShouldGC && COtFServer->HasExceededMaxMemory())
				{
					UE_LOG(LogCookCommandlet, Display, TEXT("Cooker has exceeded max memory usage collecting garbage"));
					bShouldGC |= true;
				}
			}

			if (bShouldGC && !bPosteponeGC)
			{	
				Reset();
				UE_LOG(LogCookCommandlet, Display, TEXT("GC..."));
				CollectGarbage(RF_NoFlags);
			}
		}

	private:
		/** Resets counters and flags used to determine when we should GC. */
		void Reset()
		{
			bShouldGC = false;
			PackagesCookedSinceLastGC = 0;
		}

	private:
		const uint32 PackagesPerGC;
		const double IdleTimeToGC;

		bool   bShouldGC;
		uint32 PackagesCookedSinceLastGC;
		double LastCookActionTime;
		bool   bPosteponeGC;

	} CookOnTheFlyGCController(CookOnTheFlyServer);

	FDateTime LastConnectionTime = FDateTime::UtcNow();
	bool bHadConnection = false;

	while (!GIsRequestingExit)
	{
		uint32 CookedPkgCount = 0;
		uint32 TickResults = CookOnTheFlyServer->TickCookOnTheSide(/*TimeSlice =*/10.f, CookedPkgCount);

		// Flush the asset registry before GC
		FAssetRegistryModule::TickAssetRegistry(-1.0f);

		CookOnTheFlyGCController.Update(CookedPkgCount, (UCookOnTheFlyServer::ECookOnTheSideResult)TickResults);
		CookOnTheFlyGCController.ConditionallyCollectGarbage(CookOnTheFlyServer);

		// force at least a tick shader compilation even if we are requesting stuff
		CookOnTheFlyServer->TickRecompileShaderRequests();
		GShaderCompilingManager->ProcessAsyncResults(true, false);

		while ( (CookOnTheFlyServer->HasCookRequests() == false) && !GIsRequestingExit)
		{
			CookOnTheFlyServer->TickRecompileShaderRequests();

			// Shaders need to be updated
			GShaderCompilingManager->ProcessAsyncResults(true, false);

			ProcessDeferredCommands();

			// handle server timeout
			if (InstanceId.IsValid() || bForceClose)
			{
				if (CookOnTheFlyServer->NumConnections() > 0)
				{
					bHadConnection = true;
					LastConnectionTime = FDateTime::UtcNow();
				}

				if ((FDateTime::UtcNow() - LastConnectionTime) > FTimespan::FromSeconds(Timeout))
				{
					uint32 Result = FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "FileServerIdle", "The file server did not receive any connections in the past 3 minutes. Would you like to shut it down?"));

					if (Result == EAppReturnType::No && !bForceClose)
					{
						LastConnectionTime = FDateTime::UtcNow();
					}
					else
					{
						GIsRequestingExit = true;
					}
				}
				else if (bHadConnection && (CookOnTheFlyServer->NumConnections() == 0) && bForceClose) // immediately shut down if we previously had a connection and now do not
				{
					GIsRequestingExit = true;
				}
			}

			CookOnTheFlyGCController.ConditionallyCollectGarbage(CookOnTheFlyServer);
		}
	}

	CookOnTheFlyServer->EndNetworkFileServer();
	return true;
}

/* UCommandlet interface
 *****************************************************************************/

int32 UCookCommandlet::Main(const FString& CmdLineParams)
{
	COOK_STAT(double CookStartTime = FPlatformTime::Seconds());
	Params = CmdLineParams;
	ParseCommandLine(*Params, Tokens, Switches);

	bCookOnTheFly = Switches.Contains(TEXT("COOKONTHEFLY"));   // Prototype cook-on-the-fly server
	bCookAll = Switches.Contains(TEXT("COOKALL"));   // Cook everything
	bLeakTest = Switches.Contains(TEXT("LEAKTEST"));   // Test for UObject leaks
	bUnversioned = Switches.Contains(TEXT("UNVERSIONED"));   // Save all cooked packages without versions. These are then assumed to be current version on load. This is dangerous but results in smaller patch sizes.
	bGenerateStreamingInstallManifests = Switches.Contains(TEXT("MANIFESTS"));   // Generate manifests for building streaming install packages
	bIterativeCooking = Switches.Contains(TEXT("ITERATE"));
	bSkipEditorContent = Switches.Contains(TEXT("SKIPEDITORCONTENT")); // This won't save out any packages in Engine/COntent/Editor*
	bErrorOnEngineContentUse = Switches.Contains(TEXT("ERRORONENGINECONTENTUSE"));
	bUseSerializationForGeneratingPackageDependencies = Switches.Contains(TEXT("UseSerializationForGeneratingPackageDependencies"));
	bCookSinglePackage = Switches.Contains(TEXT("cooksinglepackage"));
	bVerboseCookerWarnings = Switches.Contains(TEXT("verbosecookerwarnings"));
	bPartialGC = Switches.Contains(TEXT("Partialgc"));
	
	COOK_STAT(DetailedCookStats::CookProject = FApp::GetProjectName());

	if ( bCookOnTheFly )
	{
		// parse instance identifier
		FString InstanceIdString;
		bool bForceClose = Switches.Contains(TEXT("FORCECLOSE"));

		FGuid InstanceId;
		if (FParse::Value(*Params, TEXT("InstanceId="), InstanceIdString))
		{
			if (!FGuid::Parse(InstanceIdString, InstanceId))
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Invalid InstanceId on command line: %s"), *InstanceIdString);
			}
		}

		int32 Timeout = 180;
		if (!FParse::Value(*Params, TEXT("timeout="), Timeout))
		{
			Timeout = 180;
		}

		CookOnTheFly( InstanceId, Timeout, bForceClose);
	}
	else
	{
		
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const TArray<ITargetPlatform*>& Platforms = TPM.GetActiveTargetPlatforms();

		TArray<FString> FilesInPath;
				
		CookByTheBook(Platforms, FilesInPath);

		// Use -LogCookStats to log the results to the command line after the cook (happens automatically on a build machine)
		COOK_STAT(
		{
			double Now = FPlatformTime::Seconds();
			DetailedCookStats::CookWallTimeSec = Now - GStartTime;
			DetailedCookStats::StartupWallTimeSec = CookStartTime - GStartTime;
			DetailedCookStats::LogCookStats(CmdLineParams);
		});
	}
	
	return 0;
}

bool UCookCommandlet::CookByTheBook( const TArray<ITargetPlatform*>& Platforms, TArray<FString>& FilesInPath )
{
	COOK_STAT(FScopedDurationTimer CookByTheBookTimer(DetailedCookStats::CookByTheBookTimeSec));
	UCookOnTheFlyServer *CookOnTheFlyServer = NewObject<UCookOnTheFlyServer>();

	struct FScopeRootObject
	{
		UObject *Object;
		FScopeRootObject( UObject *InObject ) : Object( InObject )
		{
			Object->AddToRoot();
		}

		~FScopeRootObject()
		{
			Object->RemoveFromRoot();
		}
	};

	// make sure that the cookonthefly server doesn't get cleaned up while we are garbage collecting below :)
	FScopeRootObject S(CookOnTheFlyServer);

	UCookerSettings const* CookerSettings = GetDefault<UCookerSettings>();
	ECookInitializationFlags IterateFlags = ECookInitializationFlags::Iterative;

	if (Switches.Contains(TEXT("IterateSharedCookedbuild")))
	{
		// Add shared build flag to method flag, and enable iterative
		IterateFlags |= ECookInitializationFlags::IterateSharedBuild;
		
		bIterativeCooking = true;
	}
	
	ECookInitializationFlags CookFlags = ECookInitializationFlags::IncludeServerMaps;
	CookFlags |= bIterativeCooking ? IterateFlags : ECookInitializationFlags::None;
	CookFlags |= bSkipEditorContent ? ECookInitializationFlags::SkipEditorContent : ECookInitializationFlags::None;	
	CookFlags |= bUseSerializationForGeneratingPackageDependencies ? ECookInitializationFlags::UseSerializationForPackageDependencies : ECookInitializationFlags::None;
	CookFlags |= bUnversioned ? ECookInitializationFlags::Unversioned : ECookInitializationFlags::None;
	CookFlags |= bVerboseCookerWarnings ? ECookInitializationFlags::OutputVerboseCookerWarnings : ECookInitializationFlags::None;
	CookFlags |= bPartialGC ? ECookInitializationFlags::EnablePartialGC : ECookInitializationFlags::None;
	bool bTestCook = Switches.Contains(TEXT("TestCook"));
	CookFlags |= bTestCook ? ECookInitializationFlags::TestCook : ECookInitializationFlags::None;
	CookFlags |= Switches.Contains(TEXT("LogDebugInfo")) ? ECookInitializationFlags::LogDebugInfo : ECookInitializationFlags::None;
	CookFlags |= Switches.Contains(TEXT("IgnoreIniSettingsOutOfDate")) || CookerSettings->bIgnoreIniSettingsOutOfDateForIteration ? ECookInitializationFlags::IgnoreIniSettingsOutOfDate : ECookInitializationFlags::None;
	CookFlags |= Switches.Contains(TEXT("IgnoreScriptPackagesOutOfDate")) || CookerSettings->bIgnoreScriptPackagesOutOfDateForIteration ? ECookInitializationFlags::IgnoreScriptPackagesOutOfDate : ECookInitializationFlags::None;

	TArray<UClass*> FullGCAssetClasses;
	if (FullGCAssetClassNames.Num())
	{
		for (const auto& ClassName : FullGCAssetClassNames)
		{
			UClass* ClassToForceFullGC = FindObject<UClass>(nullptr, *ClassName);
			if (ClassToForceFullGC)
			{
				FullGCAssetClasses.Add(ClassToForceFullGC);
			}
			else
			{
				UE_LOG(LogCookCommandlet, Warning, TEXT("Configured to force full GC for assets of type (%s) but that class does not exist."), *ClassName);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// parse commandline options 

	FString DLCName;
	FParse::Value( *Params, TEXT("DLCNAME="), DLCName);

	FString ChildCookFile;
	FParse::Value(*Params, TEXT("cookchild="), ChildCookFile);

	int32 ChildCookIdentifier = -1;
	FParse::Value(*Params, TEXT("childIdentifier="), ChildCookIdentifier);

	int32 NumProcesses = 0;
	FParse::Value(*Params, TEXT("numcookerstospawn="), NumProcesses);

	FString BasedOnReleaseVersion;
	FParse::Value( *Params, TEXT("BasedOnReleaseVersion="), BasedOnReleaseVersion);

	FString CreateReleaseVersion;
	FParse::Value( *Params, TEXT("CreateReleaseVersion="), CreateReleaseVersion);

	FString OutputDirectoryOverride;
	FParse::Value( *Params, TEXT("OutputDir="), OutputDirectoryOverride);

	TArray<FString> CmdLineMapEntries;
	TArray<FString> CmdLineDirEntries;
	TArray<FString> CmdLineCultEntries;
	TArray<FString> CmdLineNeverCookDirEntries;
	for (int32 SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		const FString& Switch = Switches[SwitchIdx];

		auto GetSwitchValueElements = [&Switch](const FString SwitchKey) -> TArray<FString>
		{
			TArray<FString> ValueElements;
			if (Switch.StartsWith(SwitchKey + TEXT("=")) == true)
			{
				FString ValuesList = Switch.Right(Switch.Len() - (SwitchKey + TEXT("=")).Len());

				// Allow support for -KEY=Value1+Value2+Value3 as well as -KEY=Value1 -KEY=Value2
				for (int32 PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive); PlusIdx != INDEX_NONE; PlusIdx = ValuesList.Find(TEXT("+"), ESearchCase::CaseSensitive))
				{
					const FString ValueElement = ValuesList.Left(PlusIdx);
					ValueElements.Add(ValueElement);

					ValuesList = ValuesList.Right(ValuesList.Len() - (PlusIdx + 1));
				}
				ValueElements.Add(ValuesList);
			}
			return ValueElements;
		};

		// Check for -MAP=<name of map> entries
		CmdLineMapEntries += GetSwitchValueElements(TEXT("MAP"));

		// Check for -COOKDIR=<path to directory> entries
		const FString CookDirPrefix = TEXT("COOKDIR=");
		if (Switch.StartsWith(CookDirPrefix))
		{
			FString Entry = Switch.Mid(CookDirPrefix.Len()).TrimQuotes();
			FPaths::NormalizeDirectoryName(Entry);
			CmdLineDirEntries.Add(Entry);
		}

		// Check for -COOKCULTURES=<culture name> entries
		CmdLineCultEntries += GetSwitchValueElements(TEXT("COOKCULTURES"));
	}

	// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
		for (const auto& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
		{
			CmdLineDirEntries.Add(AbsoluteGameContentDir / DirToCook.Path);
		}

	}

	CookOnTheFlyServer->Initialize(ECookMode::CookByTheBook, CookFlags, OutputDirectoryOverride);

	// for backwards compat use the FullGCAssetClasses that we got from the cook commandlet ini section
	if (FullGCAssetClasses.Num() > 0)
	{
		CookOnTheFlyServer->SetFullGCAssetClasses(FullGCAssetClasses);
	}

	// Add any map sections specified on command line
	TArray<FString> AlwaysCookMapList;

	// Add the default map section
	//GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), AlwaysCookMapList);

	TArray<FString> MapList;
	// Add any map sections specified on command line
	/*GEditor->ParseMapSectionIni(*Params, MapList);

	if (MapList.Num() == 0 && !bCookSinglePackage)
	{
		// If we didn't find any maps look in the project settings for maps

		UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());

		for (const auto& MapToCook : PackagingSettings->MapsToCook)
		{
			MapList.Add(MapToCook.FilePath);
		}
	}*/

	// Add any map specified on the command line.
	for (const auto& MapName : CmdLineMapEntries)
	{
		MapList.Add(MapName);
	}

	
	TArray<FString> MapIniSections;
	FString SectionStr;
	if (FParse::Value(*Params, TEXT("MAPINISECTION="), SectionStr))
	{
		if (SectionStr.Contains(TEXT("+")))
		{
			TArray<FString> Sections;
			SectionStr.ParseIntoArray(Sections, TEXT("+"), true);
			for (int32 Index = 0; Index < Sections.Num(); Index++)
			{
				MapIniSections.Add(Sections[Index]);
			}
		}
		else
		{
			MapIniSections.Add(SectionStr);
		}
	}
	
	// If we still don't have any mapsList check if the allmaps ini section is filled out
	// this is for backwards compatibility
	if (MapList.Num() == 0 && MapIniSections.Num() == 0)
	{
		MapIniSections.Add(FString(TEXT("AllMaps")));
	}

	if (!bCookSinglePackage)
	{
		// Put the always cook map list at the front of the map list
		AlwaysCookMapList.Append(MapList);
		Swap(MapList, AlwaysCookMapList);
	}

	// Set the list of cultures to cook as those on the commandline, if specified.
	// Otherwise, use the project packaging settings.
	TArray<FString> CookCultures;
	if (Switches.ContainsByPredicate([](const FString& Switch) -> bool
		{
			return Switch.StartsWith("COOKCULTURES=");
		}))
	{
		CookCultures = CmdLineCultEntries;
	}
	else
	{
		UProjectPackagingSettings* const PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
		CookCultures = PackagingSettings->CulturesToStage;
	}

	//////////////////////////////////////////////////////////////////////////
	// start cook by the book 
	ECookByTheBookOptions CookOptions = ECookByTheBookOptions::None;

	CookOptions |= bLeakTest ? ECookByTheBookOptions::LeakTest : ECookByTheBookOptions::None; 
	CookOptions |= bCookAll ? ECookByTheBookOptions::CookAll : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("MAPSONLY")) ? ECookByTheBookOptions::MapsOnly : ECookByTheBookOptions::None;
	CookOptions |= Switches.Contains(TEXT("NODEV")) ? ECookByTheBookOptions::NoDevContent : ECookByTheBookOptions::None;

	const ECookByTheBookOptions SinglePackageFlags = ECookByTheBookOptions::NoAlwaysCookMaps | ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages | ECookByTheBookOptions::NoInputPackages | ECookByTheBookOptions::NoSlatePackages | ECookByTheBookOptions::DisableUnsolicitedPackages | ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
	CookOptions |= bCookSinglePackage ? SinglePackageFlags : ECookByTheBookOptions::None;

	UCookOnTheFlyServer::FCookByTheBookStartupOptions StartupOptions;

	StartupOptions.TargetPlatforms = Platforms;
	Swap( StartupOptions.CookMaps, MapList );
	Swap( StartupOptions.CookDirectories, CmdLineDirEntries );
	Swap( StartupOptions.NeverCookDirectories, CmdLineNeverCookDirEntries);
	Swap( StartupOptions.CookCultures, CookCultures );
	Swap( StartupOptions.DLCName, DLCName );
	Swap( StartupOptions.BasedOnReleaseVersion, BasedOnReleaseVersion );
	Swap( StartupOptions.CreateReleaseVersion, CreateReleaseVersion );
	Swap( StartupOptions.IniMapSections, MapIniSections);
	StartupOptions.CookOptions = CookOptions;
	StartupOptions.bErrorOnEngineContentUse = bErrorOnEngineContentUse;
	StartupOptions.bGenerateDependenciesForMaps = Switches.Contains(TEXT("GenerateDependenciesForMaps"));
	StartupOptions.bGenerateStreamingInstallManifests = bGenerateStreamingInstallManifests;
	StartupOptions.ChildCookFileName = ChildCookFile;
	StartupOptions.ChildCookIdentifier = ChildCookIdentifier;
	StartupOptions.NumProcesses = NumProcesses;

	COOK_STAT(
	{
		for (const auto& Platform : Platforms)
		{
			DetailedCookStats::TargetPlatforms += Platform->PlatformName() + TEXT("+");
		}
		if (!DetailedCookStats::TargetPlatforms.IsEmpty())
		{
			DetailedCookStats::TargetPlatforms.RemoveFromEnd(TEXT("+"));
		}
	});
	
	do
	{
		{
			COOK_STAT(FScopedDurationTimer StartCookByTheBookTimer(DetailedCookStats::StartCookByTheBookTimeSec));
			CookOnTheFlyServer->StartCookByTheBook(StartupOptions);
		}


		// Garbage collection should happen when either
		//	1. We have cooked a map (configurable asset type)
		//	2. We have cooked non-map packages and...
		//		a. we have accumulated 50 (configurable) of these since the last GC.
		//		b. we have been idle for 20 (configurable) seconds.
		bool bShouldGC = false;
		FString GCReason;

		// megamoth
		uint32 NonMapPackageCountSinceLastGC = 0;

		const uint32 PackagesPerGC = CookOnTheFlyServer->GetPackagesPerGC();
		const double IdleTimeToGC = CookOnTheFlyServer->GetIdleTimeToGC();
		const uint64 MaxMemoryAllowance = CookOnTheFlyServer->GetMaxMemoryAllowance();
		const uint32 PackagesPerPartialGC = CookOnTheFlyServer->GetPackagesPerPartialGC();

		double LastCookActionTime = FPlatformTime::Seconds();

		FDateTime LastConnectionTime = FDateTime::UtcNow();
		bool bHadConnection = false;

		while (CookOnTheFlyServer->IsCookByTheBookRunning())
		{
			DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "CookByTheBook.MainLoop" ), STAT_CookByTheBook_MainLoop, STATGROUP_LoadTime );
			{
				uint32 TickResults = 0;
				static const float CookOnTheSideTimeSlice = 10.0f;

				TickResults = CookOnTheFlyServer->TickCookOnTheSide( CookOnTheSideTimeSlice, NonMapPackageCountSinceLastGC );

				{
					COOK_STAT(FScopedDurationTimer ShaderProcessAsyncTimer(DetailedCookStats::TickLoopShaderProcessAsyncResultsTimeSec));
					GShaderCompilingManager->ProcessAsyncResults(true, false);
				}
				
				// Flush the asset registry before GC
				FAssetRegistryModule::TickAssetRegistry(-1.0f);

				auto DumpMemStats = []()
				{
					FGenericMemoryStats MemStats;
					GMalloc->GetAllocatorStats(MemStats);
					for (const auto& Item : MemStats.Data)
					{
						UE_LOG(LogCookCommandlet, Display, TEXT("Item %s = %d"), *Item.Key, Item.Value);
					}
				};


				const bool bHasExceededMaxMemory = CookOnTheFlyServer->HasExceededMaxMemory();
				// We should GC if we have packages to collect and we've been idle for some time.
				const bool bExceededPackagesPerGC = (PackagesPerGC > 0) && (NonMapPackageCountSinceLastGC > PackagesPerGC);
				const bool bWaitingOnObjectCache = ((TickResults & UCookOnTheFlyServer::COSR_WaitingOnCache) == 0);


				if (!bWaitingOnObjectCache && bExceededPackagesPerGC) // if we are waiting on things to cache then ignore the exceeded packages per gc
				{
					bShouldGC = true;
					GCReason = TEXT("Exceeded packages per GC");
				}
				else if (bHasExceededMaxMemory) // if we are exceeding memory then we need to gc (this can cause thrashing if the cooker loads the same stuff into memory next tick
				{
					bShouldGC = true;
					GCReason = TEXT("Exceeded Max Memory");

					int32 JobsToLogAt = GShaderCompilingManager->GetNumRemainingJobs();

					UE_LOG(LogCookCommandlet, Display, TEXT("Detected max mem exceeded - forcing shader compilation flush"));
					while ( true )
					{
						int32 NumRemainingJobs = GShaderCompilingManager->GetNumRemainingJobs();
						if ( NumRemainingJobs < 1000)
						{
							UE_LOG(LogCookCommandlet, Display, TEXT("Finished flushing shader jobs at %d"), NumRemainingJobs);
							break;
						}

						if (NumRemainingJobs < JobsToLogAt )
						{
							UE_LOG(LogCookCommandlet, Display, TEXT("Flushing shader jobs, remaining jobs %d"), NumRemainingJobs);
						}

						GShaderCompilingManager->ProcessAsyncResults(false, false);

						FPlatformProcess::Sleep(0.05);

						// GShaderCompilingManager->FinishAllCompilation();
					}
				}
				else if ((TickResults & UCookOnTheFlyServer::COSR_RequiresGC) != 0) // cooker loaded some object which needs to be cleaned up before the cooker can proceed so force gc
				{
					GCReason = TEXT("COSR_RequiresGC");
					bShouldGC = true;
				}

				bShouldGC |= bTestCook; // testing cooking / gc path


				if (bShouldGC )
				{
					bool bDidGC = true;

					if ( bPartialGC )
					{
						// markup packages 
						if ( PackagesPerGC < PackagesPerPartialGC )
						{
							bDidGC = false;
						}
						else
						{
							COOK_STAT(FScopedDurationTimer GCTimer(DetailedCookStats::TickLoopGCTimeSec));
							UE_LOG(LogCookCommandlet, Display, TEXT("GarbageCollection... partial gc"));

							CookOnTheFlyServer->MarkGCPackagesToKeepForCooker();


							DumpMemStats();

							int32 NumObjectsBeforeGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
							int32 NumObjectsAvailableBeforeGC = GUObjectArray.GetObjectArrayNum();
							CollectGarbage(RF_KeepForCooker, true);

							int32 NumObjectsAfterGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
							int32 NumObjectsAvailableAfterGC = GUObjectArray.GetObjectArrayNum();
							UE_LOG(LogCookCommandlet, Display, TEXT("Partial GC before %d available %d after %d available %d"), NumObjectsBeforeGC, NumObjectsAvailableBeforeGC, NumObjectsAfterGC, NumObjectsAvailableAfterGC);

							DumpMemStats();
						}
				
					}
					else
					{
					
						bShouldGC = false;

						int32 NumObjectsBeforeGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
						int32 NumObjectsAvailableBeforeGC = GUObjectArray.GetObjectArrayNum();

						UE_LOG(LogCookCommandlet, Display, TEXT("GarbageCollection... (%s)"), *GCReason);
						GCReason = FString();


						DumpMemStats();

						COOK_STAT(FScopedDurationTimer GCTimer(DetailedCookStats::TickLoopGCTimeSec));
						CollectGarbage(RF_NoFlags);

						int32 NumObjectsAfterGC = GUObjectArray.GetObjectArrayNumMinusAvailable();
						int32 NumObjectsAvailableAfterGC = GUObjectArray.GetObjectArrayNum();
						UE_LOG(LogCookCommandlet, Display, TEXT("Full GC before %d available %d after %d available %d"), NumObjectsBeforeGC, NumObjectsAvailableBeforeGC, NumObjectsAfterGC, NumObjectsAvailableAfterGC);

						DumpMemStats();
					}

					if ( bDidGC )
					{
						NonMapPackageCountSinceLastGC = 0;
					}
				}
				
				
				{
					COOK_STAT(FScopedDurationTimer RecompileTimer(DetailedCookStats::TickLoopRecompileShaderRequestsTimeSec));
					CookOnTheFlyServer->TickRecompileShaderRequests();

					FPlatformProcess::Sleep( 0.0f );
				}

				{
					COOK_STAT(FScopedDurationTimer ProcessDeferredCommandsTimer(DetailedCookStats::TickLoopProcessDeferredCommandsTimeSec));
					ProcessDeferredCommands();
				}
			}

			{
				COOK_STAT(FScopedDurationTimer TickCommandletStatsTimer(DetailedCookStats::TickLoopTickCommandletStatsTimeSec));
				FStats::TickCommandletStats();
			}
		}
	} while (bTestCook);

	VerifyEDLCookInfo();

	return true;
}

bool UCookCommandlet::HasExceededMaxMemory(uint64 MaxMemoryAllowance) const
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	uint64 UsedMemory = MemStats.UsedPhysical;
	if ( (UsedMemory >= MaxMemoryAllowance) && 
		(MaxMemoryAllowance > 0u) )
	{
		return true;
	}
	return false;
}

void UCookCommandlet::ProcessDeferredCommands()
{
#if PLATFORM_MAC
	// On Mac we need to process Cocoa events so that the console window for CookOnTheFlyServer is interactive
	FPlatformApplicationMisc::PumpMessages(true);
#endif

	// update task graph
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	// execute deferred commands
	for (int32 DeferredCommandsIndex = 0; DeferredCommandsIndex<GEngine->DeferredCommands.Num(); ++DeferredCommandsIndex)
	{
		GEngine->Exec( GWorld, *GEngine->DeferredCommands[DeferredCommandsIndex], *GLog);
	}

	GEngine->DeferredCommands.Empty();
}
