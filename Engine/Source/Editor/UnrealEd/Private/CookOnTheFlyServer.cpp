// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookOnTheFlyServer.cpp: handles polite cook requests via network ;)
=============================================================================*/

#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Serialization/CustomVersion.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
#include "Misc/RedirectCollector.h"
#include "Engine/Level.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/AssetManager.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture.h"
#include "SceneUtils.h"
#include "Settings/ProjectPackagingSettings.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "FileServerMessages.h"
#include "Internationalization/Culture.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "GlobalShader.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IAudioFormat.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/ITextureFormat.h"
#include "IMessageContext.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "INetworkFileServer.h"
#include "INetworkFileSystemModule.h"
#include "PlatformInfo.h"

#include "AssetRegistryModule.h"
#include "AssetRegistryState.h"
#include "CookerSettings.h"
#include "BlueprintNativeCodeGenModule.h"

#include "GameDelegates.h"
#include "IPAddress.h"

#include "IPluginManager.h"
#include "ProjectDescriptor.h"
#include "IProjectManager.h"

// cook by the book requirements
#include "Commandlets/AssetRegistryGenerator.h"
#include "Engine/WorldComposition.h"

// error message log
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

// shader compiler processAsyncResults
#include "ShaderCompiler.h"
#include "ShaderCodeLibrary.h"
#include "Engine/LevelStreaming.h"
#include "Engine/TextureLODSettings.h"
#include "ProfilingDebugging/CookStats.h"

#include "Misc/NetworkVersion.h"

#include "ParallelFor.h"

#define LOCTEXT_NAMESPACE "Cooker"

DEFINE_LOG_CATEGORY_STATIC(LogCook, Log, All);

#define DEBUG_COOKONTHEFLY 0
#define OUTPUT_TIMING 1

#define PROFILE_NETWORK 0

#if OUTPUT_TIMING
#include "ProfilingDebugging/ScopedTimers.h"

#define HIERARCHICAL_TIMER 1
#define PERPACKAGE_TIMER 0

#define REMAPPED_PLUGGINS TEXT("RemappedPlugins")

struct FTimerInfo
{
public:

	FTimerInfo( FTimerInfo &&InTimerInfo )
	{
		Swap( Name, InTimerInfo.Name );
		Length = InTimerInfo.Length;
	}

	FTimerInfo( const FTimerInfo &InTimerInfo )
	{
		Name = InTimerInfo.Name;
		Length = InTimerInfo.Length;
	}

	FTimerInfo( FString &&InName, double InLength ) : Name(MoveTemp(InName)), Length(InLength) { }

	FString Name;
	double Length;
};

#if HIERARCHICAL_TIMER
struct FHierarchicalTimerInfo : public FTimerInfo
{
public:
	FHierarchicalTimerInfo *Parent;
	TMap<FString, FHierarchicalTimerInfo*> Children;

	FHierarchicalTimerInfo(const FHierarchicalTimerInfo& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Children = InTimerInfo.Children;
		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FHierarchicalTimerInfo&& InTimerInfo) : FTimerInfo(InTimerInfo)
	{
		Swap(Children, InTimerInfo.Children);

		for (auto& Child : Children)
		{
			Child.Value->Parent = this;
		}
	}

	FHierarchicalTimerInfo(FString &&Name) : FTimerInfo(MoveTemp(Name), 0.0)
	{
	}

	~FHierarchicalTimerInfo()
	{
		Parent = NULL;
		ClearChildren();
	}

	void ClearChildren()
	{
		for (auto& Child : Children)
		{
			delete Child.Value;
		}
		Children.Empty();
	}

	FHierarchicalTimerInfo* FindChild(const FString& InName)
	{
		FHierarchicalTimerInfo* Child = (FHierarchicalTimerInfo*)(Children.FindRef(InName));
		if ( !Child )
		{
			FString Temp = InName;
			Child = Children.Add(InName, new FHierarchicalTimerInfo(MoveTemp(Temp)));
			Child->Parent = this;
		}

		check(Child);
		return Child;
	}
};

static TMap<FName, int32> IntStats;
void IncIntStat(const FName& Name, const int32 Amount)
{
	int32* Value = IntStats.Find(Name);

	if (Value == NULL)
	{
		IntStats.Add(Name, Amount);
	}
	else
	{
		*Value += Amount;
	}
}


static FHierarchicalTimerInfo RootTimerInfo(FString(TEXT("Root")));
static FHierarchicalTimerInfo* CurrentTimerInfo = &RootTimerInfo;
#endif


static TArray<FTimerInfo> GTimerInfo;



struct FScopeTimer
{
private:
	bool Started;
	bool DecrementScope;
	static int GScopeDepth;
#if HIERARCHICAL_TIMER
	FHierarchicalTimerInfo* HeirarchyTimerInfo;
#endif
public:

	FScopeTimer( const FScopeTimer &outer )
	{
		Index = outer.Index;
		DecrementScope = false;
		Started = false;
	}

	FScopeTimer( const FString &InName, bool IncrementScope = false )
	{
		DecrementScope = IncrementScope;
		
		FString Name = InName;
		for ( int i =0; i < GScopeDepth; ++i )
		{
			Name = FString(TEXT("  ")) + Name;
		}
		if( DecrementScope)
		{
			++GScopeDepth;
		}

#if HIERARCHICAL_TIMER
		HeirarchyTimerInfo = CurrentTimerInfo->FindChild(Name);
		CurrentTimerInfo = HeirarchyTimerInfo;
#endif
#if PERPACKAGE_TIMER
		Index = GTimerInfo.Emplace(MoveTemp(Name), 0.0);
#endif
		Started = false;
	}

	void Start()
	{
		if ( !Started )
		{
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length -= FPlatformTime::Seconds();
#endif
			Started = true;
#if HIERARCHICAL_TIMER
			HeirarchyTimerInfo->Length -= FPlatformTime::Seconds();
#endif
		}
	}

	void Stop()
	{
		if ( Started )
		{
#if HIERARCHICAL_TIMER
			HeirarchyTimerInfo->Length += FPlatformTime::Seconds();
#endif
#if PERPACKAGE_TIMER
			GTimerInfo[Index].Length += FPlatformTime::Seconds();
#endif
			Started = false;
		}
	}

	~FScopeTimer()
	{
		Stop();
#if HIERARCHICAL_TIMER
		check(CurrentTimerInfo == HeirarchyTimerInfo);
		CurrentTimerInfo = HeirarchyTimerInfo->Parent;
#endif
		if ( DecrementScope )
		{
			--GScopeDepth;
		}
	}

	int Index;
};

int FScopeTimer::GScopeDepth = 0;



void OutputTimers()
{
#if PERPACKAGE_TIMER
	if (GTimerInfo.Num() <= 0)
	{
		return;
	}
	
	static FArchive *OutputDevice = nullptr;

	static TMap<FString, int32> TimerIndexMap;
	
	if ( OutputDevice == nullptr )
	{
		OutputDevice = IFileManager::Get().CreateFileWriter(TEXT("CookOnTheFlyServerTiming.csv") );
	}

	TArray<FString> OutputValues;
	OutputValues.AddZeroed(TimerIndexMap.Num());

	bool OutputTimerIndexMap = false;
	for ( const FTimerInfo& TimerInfo : GTimerInfo )
	{
		int *IndexPtr = TimerIndexMap.Find(TimerInfo.Name);
		int Index = 0;
		if (IndexPtr == nullptr)
		{
			Index = TimerIndexMap.Num();
			TimerIndexMap.Add( TimerInfo.Name, Index );
			OutputValues.AddZeroed();
			OutputTimerIndexMap = true;
		}
		else
		{
			Index = *IndexPtr;
		}

		OutputValues[Index] = FString::Printf(TEXT("%f"),TimerInfo.Length);
	}
	static FString NewLine = FString(TEXT("\r\n"));

	if (OutputTimerIndexMap)
	{
		TArray<FString> OutputHeader;
		OutputHeader.AddZeroed( TimerIndexMap.Num() );
		for ( const auto& TimerIndex : TimerIndexMap )
		{
			int32 LocalIndex = TimerIndex.Value;
			OutputHeader[LocalIndex] = TimerIndex.Key;
		}

		for ( FString OutputString : OutputHeader)
		{
			OutputString.Append(TEXT(", "));
			OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
		}
		
		OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );
	}

	for ( FString OutputString : OutputValues)
	{
		OutputString.Append(TEXT(", "));
		OutputDevice->Serialize( (void*)(*OutputString), OutputString.Len() * sizeof(TCHAR));
	}
	OutputDevice->Serialize( (void*)*NewLine, NewLine.Len() * sizeof(TCHAR) );

	OutputDevice->Flush();

	UE_LOG( LogCook, Display, TEXT("Timing information for cook") );
	UE_LOG( LogCook, Display, TEXT("Name\tlength(ms)") );
	for ( const FTimerInfo& TimerInfo : GTimerInfo )
	{
		UE_LOG( LogCook, Display, TEXT("%s\t%.2f"), *TimerInfo.Name, TimerInfo.Length * 1000.0f );
	}

	// first item is the total
	if ( GTimerInfo.Num() > 0 && ( ( GTimerInfo[0].Length * 1000.0f ) > 40.0f ) )
	{
		UE_LOG( LogCook, Display, TEXT("Cook tick exceeded 40ms by  %f"), GTimerInfo[0].Length * 1000.0f  );
	}

	GTimerInfo.Empty();
#endif
}
#if HIERARCHICAL_TIMER
void OutputHierarchyTimers(const FHierarchicalTimerInfo* TimerInfo, int32& Depth)
{
	UE_LOG(LogCook, Display, TEXT("  %s: %fms"), *TimerInfo->Name, TimerInfo->Length * 1000);

	++Depth;
	for (const auto& ChildInfo : TimerInfo->Children)
	{
		OutputHierarchyTimers(ChildInfo.Value, Depth);
	}
	--Depth;
}

void OutputHierarchyTimers()
{
	UE_LOG(LogCook, Display, TEXT("Hierarchy Timer Information:"));

	FHierarchicalTimerInfo* TimerInfo = &RootTimerInfo;
	int32 Depth = 0;
	OutputHierarchyTimers(TimerInfo, Depth);

	UE_LOG(LogCook, Display, TEXT("IntStats:"));
	for (const auto& IntStat : IntStats)
	{
		UE_LOG(LogCook, Display, TEXT("  %s=%d"), *IntStat.Key.ToString(), IntStat.Value);
	}
}

void ClearHierarchyTimers()
{
	RootTimerInfo.ClearChildren();
}
#endif

#define CREATE_TIMER(name, incrementScope) FScopeTimer ScopeTimer##name(#name, incrementScope); 

#define SCOPE_TIMER(name) CREATE_TIMER(name, true); ScopeTimer##name.Start();
#define STOP_TIMER( name ) ScopeTimer##name.Stop();


#define ACCUMULATE_TIMER(name) CREATE_TIMER(name, false);
#define ACCUMULATE_TIMER_SCOPE(name) FScopeTimer ScopeTimerInner##name(ScopeTimer##name); ScopeTimerInner##name.Start();
#define ACCUMULATE_TIMER_START(name) ScopeTimer##name.Start();
#define ACCUMULATE_TIMER_STOP(name) ScopeTimer##name.Stop();
#if HIERARCHICAL_TIMER
#define INC_INT_STAT( name, amount ) { static FName StaticName##name(#name); IncIntStat(StaticName##name, amount); }
#else
#define INC_INT_STAT( name, amount ) 
#endif

#define OUTPUT_TIMERS() OutputTimers();
#define OUTPUT_HIERARCHYTIMERS() OutputHierarchyTimers();
#define CLEAR_HIERARCHYTIMERS() ClearHierarchyTimers();
#else
#define CREATE_TIMER(name)

#define SCOPE_TIMER(name)
#define STOP_TIMER(name)

#define ACCUMULATE_TIMER(name) 
#define ACCUMULATE_TIMER_SCOPE(name) 
#define ACCUMULATE_TIMER_START(name) 
#define ACCUMULATE_TIMER_STOP(name) 
#define INC_INT_STAT( name, amount ) 

#define OUTPUT_TIMERS()
#define OUTPUT_HIERARCHYTIMERS()
#define CLEAR_HIERARCHYTIMERS() 
#endif


#if PROFILE_NETWORK
double TimeTillRequestStarted = 0.0;
double TimeTillRequestForfilled = 0.0;
double TimeTillRequestForfilledError = 0.0;
double WaitForAsyncFilesWrites = 0.0;
FEvent *NetworkRequestEvent = nullptr;
#endif

#if ENABLE_COOK_STATS
namespace DetailedCookStats
{
	//Externable so CookCommandlet can pick them up and merge them with it's cook stats
	double TickCookOnTheSideTimeSec = 0.0;
	double TickCookOnTheSideLoadPackagesTimeSec = 0.0;
	double TickCookOnTheSideResolveRedirectorsTimeSec = 0.0;
	double TickCookOnTheSideSaveCookedPackageTimeSec = 0.0;
	double TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec = 0.0;
	double TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec = 0.0;
	double GameCookModificationDelegateTimeSec = 0.0;
}
#endif

//////////////////////////////////////////////////////////////////////////
// FCookTimer
// used as a helper to timeslice cooker functions
//////////////////////////////////////////////////////////////////////////

struct FCookerTimer
{
	const bool bIsRealtimeMode;
	const double StartTime;
	const float &TimeSlice;
	const int MaxNumPackagesToSave; // maximum packages to save before exiting tick (this should never really hit unless we are not using realtime mode)
	int NumPackagesSaved;

	FCookerTimer(const float &InTimeSlice, bool bInIsRealtimeMode, int InMaxNumPackagesToSave = 50) :
		bIsRealtimeMode(bInIsRealtimeMode), StartTime(FPlatformTime::Seconds()), TimeSlice(InTimeSlice),
		MaxNumPackagesToSave(InMaxNumPackagesToSave), NumPackagesSaved(0)
	{
	}
	inline double GetTimeTillNow()
	{
		return FPlatformTime::Seconds() - StartTime;
	}
	bool IsTimeUp()
	{
		if (bIsRealtimeMode)
		{
			if ((FPlatformTime::Seconds() - StartTime) > TimeSlice)
			{
				return true;
			}
		}
		if (NumPackagesSaved >= MaxNumPackagesToSave)
		{
			return true;
		}
		return false;
	}

	inline void SavedPackage()
	{
		++NumPackagesSaved;
	}

	inline double GetTimeRemain()
	{
		return TimeSlice - (FPlatformTime::Seconds() - StartTime);
	}
};

////////////////////////////////////////////////////////////////
/// Cook on the fly server
///////////////////////////////////////////////////////////////


DECLARE_CYCLE_STAT(TEXT("Precache Derived data for platform"), STAT_TickPrecacheCooking, STATGROUP_Cooking);
DECLARE_CYCLE_STAT(TEXT("Tick cooking"), STAT_TickCooker, STATGROUP_Cooking);



/* helper structs functions
 *****************************************************************************/

/** Helper to pass a recompile request to game thread */
struct FRecompileRequest
{
	struct FShaderRecompileData RecompileData;
	volatile bool bComplete;
};


/** Helper to assign to any variable for a scope period */
template<class T>
struct FScopeAssign
{
private:
	T* Setting;
	T OriginalValue;
public:
	FScopeAssign(T& InSetting, const T NewValue)
	{
		Setting = &InSetting;
		OriginalValue = *Setting;
		*Setting = NewValue;
	}
	~FScopeAssign()
	{
		*Setting = OriginalValue;
	}
};


class FPackageSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	TArray<FString>& FoundFiles;
public:
	FPackageSearchVisitor(TArray<FString>& InFoundFiles)
		: FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.EndsWith(TEXT(".uasset")) || Filename.EndsWith(TEXT(".umap")))
			{
				FoundFiles.Add(Filename);
			}
		}
		return true;
	}
};

class FAdditionalPackageSearchVisitor: public IPlatformFile::FDirectoryVisitor
{
	TSet<FString>& FoundMapFilesNoExt;
	TArray<FString>& FoundOtherFiles;
public:
	FAdditionalPackageSearchVisitor(TSet<FString>& InFoundMapFiles, TArray<FString>& InFoundOtherFiles)
		: FoundMapFilesNoExt(InFoundMapFiles), FoundOtherFiles(InFoundOtherFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			if (Filename.EndsWith(TEXT(".uasset")) || Filename.EndsWith(TEXT(".umap")))
			{
				FoundMapFilesNoExt.Add(FPaths::SetExtension(Filename, ""));
			}
			else if ( Filename.EndsWith(TEXT(".uexp")) || Filename.EndsWith(TEXT(".ubulk")) )
			{
				FoundOtherFiles.Add(Filename);
			}
		}
		return true;
	}
};

const FString& GetAssetRegistryPath()
{
	static const FString AssetRegistryPath = FPaths::ProjectDir();
	return AssetRegistryPath;
}

FString GetChildCookerResultFilename(const FString& ResponseFilename)
{
	FString Result = ResponseFilename + TEXT("Result.txt");
	return Result;
}

FString GetChildCookerManifestFilename(const FString ResponseFilename)
{
	FString Result = ResponseFilename + TEXT("Manifest.txt");
	return Result;
}

/**
 * Return the release asset registry filename for the release version supplied
 */
FString GetReleaseVersionAssetRegistryPath(const FString& ReleaseVersion, const FName& PlatformName )
{
	// cache the part of the path which is static because getting the ProjectDir is really slow and also string manipulation
	const static FString ProjectDirectory = FPaths::ProjectDir() / FString(TEXT("Releases"));
	return  ProjectDirectory / ReleaseVersion / PlatformName.ToString();
}

const FString& GetAssetRegistryFilename()
{
	static const FString AssetRegistryFilename = FString(TEXT("AssetRegistry.bin"));
	return AssetRegistryFilename;
}

const FString& GetDevelopmentAssetRegistryFilename()
{
	static const FString DevelopmentAssetRegistryFilename = FString(TEXT("DevelopmentAssetRegistry.bin"));
	return DevelopmentAssetRegistryFilename;
}

/**
 * Uses the FMessageLog to log a message
 * 
 * @param Message to log
 * @param Severity of the message
 */
void LogCookerMessage( const FString& MessageText, EMessageSeverity::Type Severity)
{
	FMessageLog MessageLog("CookResults");

	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(Severity);

	Message->AddToken( FTextToken::Create( FText::FromString(MessageText) ) );
	// Message->AddToken(FTextToken::Create(MessageLogTextDetail)); 
	// Message->AddToken(FDocumentationToken::Create(TEXT("https://docs.unrealengine.com/latest/INT/Platforms/iOS/QuickStart/6/index.html"))); 
	MessageLog.AddMessage(Message);

	MessageLog.Notify(FText(), EMessageSeverity::Warning, false);
}



/* FIlename caching functions
 *****************************************************************************/

FString UCookOnTheFlyServer::GetCachedPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const FName& PackageName ) const 
{
	return Cache( PackageName ).StandardFileFName;
}


FString UCookOnTheFlyServer::GetCachedPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).PackageFilename;
}

FString UCookOnTheFlyServer::GetCachedStandardPackageFilename( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFilename;
}

FName UCookOnTheFlyServer::GetCachedStandardPackageFileFName( const UPackage* Package ) const 
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache( Package->GetFName() ).StandardFileFName;
}


bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const FName& PackageName ) const
{
	check(IsInGameThread());
	return PackageFilenameCache.Remove( PackageName ) >= 1;
}

bool UCookOnTheFlyServer::ClearPackageFilenameCacheForPackage( const UPackage* Package ) const
{
	check(IsInGameThread());
	return PackageFilenameCache.Remove( Package->GetFName() ) >= 1;
}

const FString& UCookOnTheFlyServer::GetCachedSandboxFilename( const UPackage* Package, TUniquePtr<class FSandboxPlatformFile>& InSandboxFile ) const 
{
	check(IsInGameThread());

	FName PackageFName = Package->GetFName();
	static TMap<FName, FString> CachedSandboxFilenames;
	FString* CachedSandboxFilename = CachedSandboxFilenames.Find(PackageFName);
	if ( CachedSandboxFilename )
		return *CachedSandboxFilename;

	const FString& PackageFilename = GetCachedPackageFilename(Package);
	FString SandboxFilename = ConvertToFullSandboxPath(*PackageFilename, true );

	return CachedSandboxFilenames.Add( PackageFName, MoveTemp(SandboxFilename) );
}

const UCookOnTheFlyServer::FCachedPackageFilename& UCookOnTheFlyServer::Cache(const FName& PackageName) const 
{
	check( IsInGameThread() );
	FCachedPackageFilename *Cached = PackageFilenameCache.Find( PackageName );
	if ( Cached != NULL )
	{
		return *Cached;
	}
	// cache all the things, like it's your birthday!

	FString Filename;
	FString PackageFilename;
	FString StandardFilename;
	FName StandardFileFName = NAME_None;
	if (FPackageName::DoesPackageExist(PackageName.ToString(), NULL, &Filename))
	{
		StandardFilename = PackageFilename = FPaths::ConvertRelativePathToFull(Filename);


		FPaths::MakeStandardFilename(StandardFilename);
		StandardFileFName = FName(*StandardFilename);
	}
	PackageFilenameToPackageFNameCache.Add(StandardFileFName, PackageName);
	return PackageFilenameCache.Emplace( PackageName, FCachedPackageFilename(MoveTemp(PackageFilename),MoveTemp(StandardFilename), StandardFileFName) );
}


const FName* UCookOnTheFlyServer::GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const
{
	check(IsInGameThread());
	const FName* Result = PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
	if ( Result )
	{
		return Result;
	}

	FName PackageName = StandardPackageFilename;
	FString PotentialLongPackageName = StandardPackageFilename.ToString();
	if (FPackageName::IsValidLongPackageName(PotentialLongPackageName) == false)
	{
		PotentialLongPackageName = FPackageName::FilenameToLongPackageName(PotentialLongPackageName);
		PackageName = FName(*PotentialLongPackageName);
	}

	Cache(PackageName);

	return PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
}

void UCookOnTheFlyServer::ClearPackageFilenameCache() const 
{
	check(IsInGameThread());
	PackageFilenameCache.Empty();
	PackageFilenameToPackageFNameCache.Empty();
}


/* UCookOnTheFlyServer functions
 *****************************************************************************/

UCookOnTheFlyServer::UCookOnTheFlyServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	CurrentCookMode(ECookMode::CookOnTheFly),
	CookByTheBookOptions(nullptr),
	CookFlags(ECookInitializationFlags::None),
	bIsInitializingSandbox(false),
	bIgnoreMarkupPackageAlreadyLoaded(false),
	bIsSavingPackage(false),
	AssetRegistry(nullptr)
{
}

UCookOnTheFlyServer::~UCookOnTheFlyServer()
{
	FCoreDelegates::OnFConfigCreated.RemoveAll(this);
	FCoreDelegates::OnFConfigDeleted.RemoveAll(this);

	check(TickChildCookers() == true);
	if ( CookByTheBookOptions )
	{		
		delete CookByTheBookOptions;
		CookByTheBookOptions = NULL;
	}
}

// this tick only happens in the editor cook commandlet directly calls tick on the side
void UCookOnTheFlyServer::Tick(float DeltaTime)
{
	check(IsCookingInEditor());

	if (IsCookByTheBookMode() && !IsCookByTheBookRunning() && !GIsSlowTask)
	{
		// if we are in the editor then precache some stuff ;)
		TArray<const ITargetPlatform*> CacheTargetPlatforms;
		const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>();
		ITargetPlatform* TargetPlatform = nullptr;
		if (PlaySettings && (PlaySettings->LastExecutedLaunchModeType == LaunchMode_OnDevice))
		{
			FString DeviceName = PlaySettings->LastExecutedLaunchDevice.Left(PlaySettings->LastExecutedLaunchDevice.Find(TEXT("@")));
			CacheTargetPlatforms.Add(GetTargetPlatformManager()->FindTargetPlatform(DeviceName));
		}
		if (CacheTargetPlatforms.Num() > 0)
		{
			// early out all the stuff we don't care about 
			if (!IsCookFlagSet(ECookInitializationFlags::BuildDDCInBackground))
			{
				return;
			}
			TickPrecacheObjectsForPlatforms(0.001, CacheTargetPlatforms);
		}
	}

	/*
	// this is still being worked on
	if ( IsCookOnTheFlyMode() )
	{
		if ( !CookRequests.HasItems() )
		{
			OpportunisticSaveInMemoryPackages();
		}
	}
	*/

	uint32 CookedPackagesCount = 0;
	const static float CookOnTheSideTimeSlice = 0.1f; // seconds
	TickCookOnTheSide( CookOnTheSideTimeSlice, CookedPackagesCount);
	TickRecompileShaderRequests();
}

bool UCookOnTheFlyServer::IsTickable() const 
{ 
	return IsCookFlagSet(ECookInitializationFlags::AutoTick); 
}

TStatId UCookOnTheFlyServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCookServer, STATGROUP_Tickables);
}

const TArray<ITargetPlatform*>& UCookOnTheFlyServer::GetCookingTargetPlatforms() const
{
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

	FString PlatformStr;
	if ( FParse::Value(FCommandLine::Get(), TEXT("TARGETPLATFORM="), PlatformStr) == false )
	{
		FString ValueName = TEXT("DefaultTargetPlatform");

		if ( IsCookingInEditor() )
		{
			ValueName += TEXT("Editor");
		}
		if ( IsCookOnTheFlyMode() )
		{
			ValueName += TEXT("OnTheFly");
		}


		TArray<FString> TargetPlatformNames;
		// see if we have specified in an ini file which target platforms we should use
		if ( GConfig->GetArray(TEXT("CookSettings"), *ValueName, TargetPlatformNames, GEditorIni) )
		{
			for ( const FString& TargetPlatformName : TargetPlatformNames )
			{
				ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(TargetPlatformName);

				if ( TargetPlatform )
				{
					CookingTargetPlatforms.AddUnique(TargetPlatform);
				}
				else
				{
					UE_LOG(LogCook, Warning, TEXT("Unable to resolve targetplatform name %s"), *TargetPlatformName );
				}
			}
		}
	}


	if ( CookingTargetPlatforms.Num() == 0 )
	{
		const TArray<ITargetPlatform*>& Platforms = TPM.GetCookingTargetPlatforms();

		CookingTargetPlatforms = Platforms;
	}

	return CookingTargetPlatforms;
}

bool UCookOnTheFlyServer::StartNetworkFileServer( const bool BindAnyPort )
{
	check( IsCookOnTheFlyMode() );
	//GetDerivedDataCacheRef().WaitForQuiescence(false);

#if PROFILE_NETWORK
	NetworkRequestEvent = FPlatformProcess::GetSynchEventFromPool();
#endif
	ValidateCookOnTheFlySettings();

	GenerateAssetRegistry();

	InitializeSandbox();

	const TArray<ITargetPlatform*>& Platforms = GetCookingTargetPlatforms();

	// When cooking on the fly the full registry is saved at the beginning
	// in cook by the book asset registry is saved after the cook is finished
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		FAssetRegistryGenerator* Generator = RegistryGenerators.FindRef(FName(*Platforms[Index]->PlatformName()));
		if (Generator)
		{
			Generator->SaveAssetRegistry(GetSandboxAssetRegistryFilename(), false);
		}
	}

	// start the listening thread
	FNewConnectionDelegate NewConnectionDelegate(FNewConnectionDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerNewConnection));
	FFileRequestDelegate FileRequestDelegate(FFileRequestDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerFileRequest));
	FRecompileShadersDelegate RecompileShadersDelegate(FRecompileShadersDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders));
	FSandboxPathDelegate SandboxPathDelegate(FSandboxPathDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkGetSandboxPath));
	FInitialPrecookedListDelegate InitialPrecookedListDelegate(FInitialPrecookedListDelegate::CreateUObject(this, &UCookOnTheFlyServer::HandleNetworkGetPrecookedList));


	FNetworkFileDelegateContainer NetworkFileDelegateContainer;
	NetworkFileDelegateContainer.NewConnectionDelegate = NewConnectionDelegate;
	NetworkFileDelegateContainer.InitialPrecookedListDelegate = InitialPrecookedListDelegate;
	NetworkFileDelegateContainer.FileRequestDelegate = FileRequestDelegate;
	NetworkFileDelegateContainer.RecompileShadersDelegate = RecompileShadersDelegate;
	NetworkFileDelegateContainer.SandboxPathOverrideDelegate = SandboxPathDelegate;
	
	NetworkFileDelegateContainer.OnFileModifiedCallback = &FileModifiedDelegate;


	INetworkFileServer *TcpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, NetworkFileDelegateContainer, ENetworkFileServerProtocol::NFSP_Tcp);
	if ( TcpFileServer )
	{
		NetworkFileServers.Add(TcpFileServer);
	}

	// cookonthefly server for html5 -- NOTE: if this is crashing COTF servers, please ask for Nick.Shin (via Josh.Adams)
#if 1
	INetworkFileServer *HttpFileServer = FModuleManager::LoadModuleChecked<INetworkFileSystemModule>("NetworkFileSystem")
		.CreateNetworkFileServer(true, BindAnyPort ? 0 : -1, NetworkFileDelegateContainer, ENetworkFileServerProtocol::NFSP_Http);
	if ( HttpFileServer )
	{
		NetworkFileServers.Add( HttpFileServer );
	}
#endif

	// loop while waiting for requests
	GIsRequestingExit = false;
	return true;
}


bool UCookOnTheFlyServer::BroadcastFileserverPresence( const FGuid &InstanceId )
{
	
	TArray<FString> AddressStringList;

	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		TArray<TSharedPtr<FInternetAddr> > AddressList;
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ((NetworkFileServer == NULL || !NetworkFileServer->IsItReadyToAcceptConnections() || !NetworkFileServer->GetAddressList(AddressList)))
		{
			LogCookerMessage( FString(TEXT("Failed to create network file server")), EMessageSeverity::Error );
			UE_LOG(LogCook, Error, TEXT("Failed to create network file server"));
			continue;
		}

		// broadcast our presence
		if (InstanceId.IsValid())
		{
			for (int32 AddressIndex = 0; AddressIndex < AddressList.Num(); ++AddressIndex)
			{
				AddressStringList.Add(FString::Printf( TEXT("%s://%s"), *NetworkFileServer->GetSupportedProtocol(),  *AddressList[AddressIndex]->ToString(true)));
			}

		}
	}

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint = FMessageEndpoint::Builder("UCookOnTheFlyServer").Build();

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Publish(new FFileServerReady(AddressStringList, InstanceId), EMessageScope::Network);
	}		
	
	return true;
}

/*----------------------------------------------------------------------------
	FArchiveFindReferences.
----------------------------------------------------------------------------*/
/**
 * Archive for gathering all the object references to other objects
 */
class FArchiveFindReferences : public FArchiveUObject
{
private:
	/**
	 * I/O function.  Called when an object reference is encountered.
	 *
	 * @param	Obj		a pointer to the object that was encountered
	 */
	FArchive& operator<<( UObject*& Obj ) override
	{
		if( Obj )
		{
			FoundObject( Obj );
		}
		return *this;
	}

	virtual FArchive& operator<< (struct FSoftObjectPtr& Value) override
	{
		if ( Value.Get() )
		{
			Value.Get()->Serialize( *this );
		}
		return *this;
	}
	virtual FArchive& operator<< (struct FSoftObjectPath& Value) override
	{
		if ( Value.ResolveObject() )
		{
			Value.ResolveObject()->Serialize( *this );
		}
		return *this;
	}


	void FoundObject( UObject* Object )
	{
		if ( RootSet.Find(Object) == NULL )
		{
			if ( Exclude.Find(Object) == INDEX_NONE )
			{
				// remove this check later because don't want this happening in development builds
				//check(RootSetArray.Find(Object)==INDEX_NONE);

				RootSetArray.Add( Object );
				RootSet.Add(Object);
				Found.Add(Object);
			}
		}
	}


	/**
	 * list of Outers to ignore;  any objects encountered that have one of
	 * these objects as an Outer will also be ignored
	 */
	TArray<UObject*> &Exclude;

	/** list of objects that have been found */
	TSet<UObject*> &Found;
	
	/** the objects to display references to */
	TArray<UObject*> RootSetArray;
	/** Reflection of the rootsetarray */
	TSet<UObject*> RootSet;

public:

	/**
	 * Constructor
	 * 
	 * @param	inOutputAr		archive to use for logging results
	 * @param	inOuter			only consider objects that do not have this object as its Outer
	 * @param	inSource		object to show references for
	 * @param	inExclude		list of objects that should be ignored if encountered while serializing SourceObject
	 */
	FArchiveFindReferences( TSet<UObject*> InRootSet, TSet<UObject*> &inFound, TArray<UObject*> &inExclude )
		: Exclude(inExclude)
		, Found(inFound)
		, RootSet(InRootSet)
	{
		ArIsObjectReferenceCollector = true;
		ArIsSaving = true;

		for ( UObject* Object : RootSet )
		{
			RootSetArray.Add( Object );
		}
		
		// loop through all the objects in the root set and serialize them
		for ( int RootIndex = 0; RootIndex < RootSetArray.Num(); ++RootIndex )
		{
			UObject* SourceObject = RootSetArray[RootIndex];

			// quick sanity check
			check(SourceObject);
			check(SourceObject->IsValidLowLevel());

			SourceObject->Serialize( *this );
		}

	}

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const override { return TEXT("FArchiveFindReferences"); }
};

void UCookOnTheFlyServer::GetDependentPackages(const TSet<UPackage*>& RootPackages, TSet<FName>& FoundPackages)
{
	check(IsChildCooker() == false);

	TSet<FName> RootPackageFNames;
	for (const UPackage* RootPackage : RootPackages)
	{
		RootPackageFNames.Add(RootPackage->GetFName());
	}


	GetDependentPackages(RootPackageFNames, FoundPackages);

}


void UCookOnTheFlyServer::GetDependentPackages( const TSet<FName>& RootPackages, TSet<FName>& FoundPackages )
{
	check(IsChildCooker() == false);

	TArray<FName> FoundPackagesArray;
	for (const FName& RootPackage : RootPackages)
	{
		FoundPackagesArray.Add(RootPackage);
		FoundPackages.Add(RootPackage);
	}

	int FoundPackagesCounter = 0;
	while ( FoundPackagesCounter < FoundPackagesArray.Num() )
	{
		TArray<FName> PackageDependencies;
		if (AssetRegistry->GetDependencies(FoundPackagesArray[FoundPackagesCounter], PackageDependencies) == false)
		{
			// this could happen if we are in the editor and the dependency list is not up to date

			if (IsCookingInEditor() == false)
			{
				UE_LOG(LogCook, Fatal, TEXT("Unable to find package %s in asset registry.  Can't generate cooked asset registry"), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find package %s in asset registry, cooked asset registry information may be invalid "), *FoundPackagesArray[FoundPackagesCounter].ToString());
			}
		}
		++FoundPackagesCounter;
		for ( const FName& OriginalPackageDependency : PackageDependencies )
		{
			// check(PackageDependency.ToString().StartsWith(TEXT("/")));
			FName PackageDependency = OriginalPackageDependency;
			FString PackageDependencyString = PackageDependency.ToString();

			FText OutReason;
			const bool bIncludeReadOnlyRoots = true; // Dependency packages are often script packages (read-only)
			if (!FPackageName::IsValidLongPackageName(PackageDependencyString, bIncludeReadOnlyRoots, &OutReason))
			{
				const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
					FText::FromString(PackageDependencyString), OutReason);

				LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("%s"), *( FailMessage.ToString() ));
				continue;
			}
			else if (FPackageName::IsScriptPackage(PackageDependencyString) || FPackageName::IsMemoryPackage(PackageDependencyString))
			{
				continue;
			}

			if ( FoundPackages.Contains(PackageDependency) == false )
			{
				FoundPackages.Add(PackageDependency);
				FoundPackagesArray.Add( PackageDependency );
			}
		}
	}

}

void UCookOnTheFlyServer::GetDependencies( const TSet<UPackage*>& Packages, TSet<UObject*>& Found)
{
	TSet<UObject*> RootSet;

	for (UPackage* Package : Packages)
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(Package, ObjectsInPackage, true);
		for (UObject* Obj : ObjectsInPackage)
		{
			RootSet.Add(Obj);
			Found.Add(Obj);
		}
	}

	TArray<UObject*> Exclude;
	FArchiveFindReferences ArFindReferences( RootSet, Found, Exclude );
}

bool UCookOnTheFlyServer::ContainsMap(const FName& PackageName) const
{
	TArray<FAssetData> Assets;
	ensure(AssetRegistry->GetAssetsByPackageName(PackageName, Assets, true));

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.GetClass()->IsChildOf(UWorld::StaticClass()) || Asset.GetClass()->IsChildOf(ULevel::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

bool UCookOnTheFlyServer::ContainsRedirector(const FName& PackageName, TMap<FName, FName>& RedirectedPaths) const
{
	bool bFoundRedirector = false;
	TArray<FAssetData> Assets;
	ensure(AssetRegistry->GetAssetsByPackageName(PackageName, Assets, true));

	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsRedirector())
		{
			FName RedirectedPath;
			FString RedirectedPathString;
			if (Asset.GetTagValue("DestinationObject", RedirectedPathString))
			{
				ConstructorHelpers::StripObjectClass(RedirectedPathString);
				RedirectedPath = FName(*RedirectedPathString);
				FAssetData DestinationData = AssetRegistry->GetAssetByObjectPath(RedirectedPath, true);
				TSet<FName> SeenPaths;

				SeenPaths.Add(RedirectedPath);

				// Need to follow chain of redirectors
				while (DestinationData.IsRedirector())
				{
					if (DestinationData.GetTagValue("DestinationObject", RedirectedPathString))
					{
						ConstructorHelpers::StripObjectClass(RedirectedPathString);
						RedirectedPath = FName(*RedirectedPathString);

						if (SeenPaths.Contains(RedirectedPath))
						{
							// Recursive, bail
							DestinationData = FAssetData();
						}
						else
						{
							SeenPaths.Add(RedirectedPath);
							DestinationData = AssetRegistry->GetAssetByObjectPath(RedirectedPath, true);
						}
					}
					else
					{
						// Can't extract
						DestinationData = FAssetData();						
					}
				}

				// DestinationData may be invalid if this is a subobject, check package as well
				bool bDestinationValid = DestinationData.IsValid();

				if (!bDestinationValid)
				{
					FName StandardPackageName = GetCachedStandardPackageFileFName(FName(*FPackageName::ObjectPathToPackageName(RedirectedPathString)));
					if (StandardPackageName != NAME_None)
					{
						bDestinationValid = true;
					}
				}

				if (bDestinationValid)
				{
					RedirectedPaths.Add(Asset.ObjectPath, RedirectedPath);
				}
				else
				{
					RedirectedPaths.Add(Asset.ObjectPath, NAME_None);
					UE_LOG(LogCook, Log, TEXT("Found redirector in package %s pointing to deleted object %s"), *PackageName.ToString(), *RedirectedPathString);
				}

				bFoundRedirector = true;
			}
		}
	}
	return bFoundRedirector;
}

bool UCookOnTheFlyServer::IsCookingInEditor() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;
}

bool UCookOnTheFlyServer::IsRealtimeMode() const 
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor;
}

bool UCookOnTheFlyServer::IsCookByTheBookMode() const
{
	return CurrentCookMode == ECookMode::CookByTheBookFromTheEditor || CurrentCookMode == ECookMode::CookByTheBook;
}

bool UCookOnTheFlyServer::IsCookOnTheFlyMode() const
{
	return CurrentCookMode == ECookMode::CookOnTheFly || CurrentCookMode == ECookMode::CookOnTheFlyFromTheEditor; 
}

FString UCookOnTheFlyServer::GetBaseDirectoryForDLC() const
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(CookByTheBookOptions->DlcName);
	check(Plugin.IsValid());
	return Plugin->GetBaseDir();
}

COREUOBJECT_API extern bool GOutputCookingWarnings;

bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const TArray<FName>& TargetPlatforms, const bool bForceFrontOfQueue)
{
	FFilePlatformRequest FileRequest(StandardPackageFName, TargetPlatforms);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}


bool UCookOnTheFlyServer::RequestPackage(const FName& StandardPackageFName, const bool bForceFrontOfQueue)
{
	check(IsCookByTheBookMode());
	// need target platforms if we are not in cook by the book mode
	FFilePlatformRequest FileRequest(StandardPackageFName, CookByTheBookOptions->TargetPlatformNames);
	CookRequests.EnqueueUnique(FileRequest, bForceFrontOfQueue);
	return true;
}

// should only call this after tickchildcookers returns false
void UCookOnTheFlyServer::CleanUpChildCookers()
{
	if (IsCookByTheBookMode())
	{
		for (FChildCooker& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			check(ChildCooker.bFinished == true);
			if (ChildCooker.Thread != nullptr)
			{
				ChildCooker.Thread->WaitForCompletion();
				delete ChildCooker.Thread;
				ChildCooker.Thread = nullptr;
			}

		}

		
	}
}

bool UCookOnTheFlyServer::TickChildCookers()
{
	if (IsCookByTheBookMode())
	{
		for (FChildCooker& ChildCooker : CookByTheBookOptions->ChildCookers)
		{
			if (ChildCooker.bFinished == false)
			{
				return false;
			}
		}
	}
	return true;
}


// callback just before the garbage collector gets called.
void UCookOnTheFlyServer::PreGarbageCollect()
{
	PackageReentryData.Empty();
}

UCookOnTheFlyServer::FReentryData& UCookOnTheFlyServer::GetReentryData(const UPackage* Package) const
{
	FReentryData& CurrentReentryData = PackageReentryData.FindOrAdd(Package->GetFName());

	if ( (CurrentReentryData.bIsValid == false) && (Package->IsFullyLoaded() == true))
	{
		CurrentReentryData.bIsValid = true;
		CurrentReentryData.FileName = Package->GetFName();
		GetObjectsWithOuter(Package, CurrentReentryData.CachedObjectsInOuter);	
}
	return CurrentReentryData;
}


uint32 UCookOnTheFlyServer::TickCookOnTheSide( const float TimeSlice, uint32 &CookedPackageCount, ECookTickFlags TickFlags )
{
	COOK_STAT(FScopedDurationTimer TickTimer(DetailedCookStats::TickCookOnTheSideTimeSec));
	FCookerTimer Timer(TimeSlice, IsRealtimeMode());

	uint32 Result = 0;

	if (IsChildCooker() == false)
	{
		if (AssetRegistry == nullptr || AssetRegistry->IsLoadingAssets())
		{
			// early out
			return Result;
		}
	}

	// This is all the target platforms which we needed to process requests for this iteration
	// we use this in the unsolicited packages processing below
	TArray<FName> AllTargetPlatformNames;
	
	if (CurrentCookMode == ECookMode::CookByTheBook)
	{
		if (CookRequests.HasItems() == false)
		{
			SCOPE_TIMER(WaitingForChildCookers);
			// we have nothing left to cook so we are now waiting for child cookers to finish
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
			
		}
		else
		{
			if (TickChildCookers() == false)
			{
				Result |= COSR_WaitingOnChildCookers;
			}
		}
	}

	while (!GIsRequestingExit || CurrentCookMode == ECookMode::CookByTheBook)
	{
		// if we just cooked a map then don't process anything the rest of this tick
		if ( Result & COSR_RequiresGC )
		{
			break;
		}

		if ( IsCookByTheBookMode() )
		{
			check( CookByTheBookOptions );
			if ( CookByTheBookOptions->bCancel )
			{
				CancelCookByTheBook();
			}
		}

		FFilePlatformRequest ToBuild;
		
		//bool bIsUnsolicitedRequest = false;
		if ( CookRequests.HasItems() )
		{
			CookRequests.Dequeue( &ToBuild );
		}
		else
		{
			// no more to do this tick break out and do some other stuff
			break;
		}

#if PROFILE_NETWORK
		if (NetworkRequestEvent)
		{
			NetworkRequestEvent->Trigger();
		}
#endif

		// prevent autosave from happening until we are finished cooking
		// causes really bad hitches
		if ( GUnrealEd )
		{
			const static float SecondsWarningTillAutosave = 10.0f;
			GUnrealEd->GetPackageAutoSaver().ForceMinimumTimeTillAutoSave(SecondsWarningTillAutosave);
		}

		if (CookedPackages.Exists(ToBuild))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package for platform already cooked %s, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			continue;
		}

#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Processing package %s"), *ToBuild.GetFilename().ToString());
#endif
		SCOPE_TIMER(TickCookOnTheSide);

		check( ToBuild.IsValid() );
		const TArray<FName>& TargetPlatformNames = ToBuild.GetPlatformNames();

		TArray<UPackage*> PackagesToSave;

#if OUTPUT_TIMING
		//FScopeTimer PackageManualTimer( ToBuild.GetFilename().ToString(), false );
#endif

		for ( const FName& PlatformName : TargetPlatformNames )
		{
			AllTargetPlatformNames.AddUnique(PlatformName);
		}

		for ( const FName& PlatformName : AllTargetPlatformNames )
		{
			if ( ToBuild.HasPlatform(PlatformName) == false )
			{
				ToBuild.AddPlatform(PlatformName);
			}
		}

		const FString BuildFilename = ToBuild.GetFilename().ToString();

		// if we have no target platforms then we want to cook because this will cook for all target platforms in that case
		bool bShouldCook = TargetPlatformNames.Num() > 0 ? false : ShouldCook( BuildFilename, NAME_None );
		{
			SCOPE_TIMER(ShouldCook);
			for ( int Index = 0; Index < TargetPlatformNames.Num(); ++Index )
			{
				bShouldCook |= ShouldCook( ToBuild.GetFilename().ToString(), TargetPlatformNames[Index] );
			}
		}
		
		if( CookByTheBookOptions && CookByTheBookOptions->bErrorOnEngineContentUse )
		{
			check(IsCookingDLC());
			FString DLCPath = FPaths::Combine(*GetBaseDirectoryForDLC(), TEXT("Content"));
			if ( ToBuild.GetFilename().ToString().StartsWith(DLCPath) == false ) // if we don't start with the dlc path then we shouldn't be cooking this data 
			{
				UE_LOG(LogCook, Error, TEXT("Engine or Game content %s is being referenced by DLC!"), *ToBuild.GetFilename().ToString() );
				bShouldCook = false;
			}
		}

		check(IsInGameThread());
		if (NeverCookPackageList.Contains(ToBuild.GetFilename()))
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Package %s requested but is in the never cook package list, discarding request"), *ToBuild.GetFilename().ToString());
#endif
			bShouldCook = false;
		}


		if ( bShouldCook ) // if we should cook the package then cook it otherwise add it to the list of already cooked packages below
		{
			UPackage* Package = LoadPackageForCooking(BuildFilename);

			if ( Package )
			{
				FString Name = Package->GetPathName();
				FString PackageFilename(GetCachedStandardPackageFilename(Package));
				if (PackageFilename != BuildFilename)
				{
					// we have saved something which we didn't mean to load 
					//  sounds unpossible.... but it is due to searching for files and such
					//  mark the original request as processed (if this isn't actually the file they were requesting then it will fail)
					//	and then also save our new request as processed so we don't do it again
					UE_LOG(LogCook, Verbose, TEXT("Request for %s received going to save %s"), *BuildFilename, *PackageFilename);

					CookedPackages.Add(FFilePlatformCookedPackage(ToBuild.GetFilename(), TargetPlatformNames));

					ToBuild.SetFilename(PackageFilename);
				}
				PackagesToSave.AddUnique(Package);
			}
			else
			{
				Result |= COSR_ErrorLoadingPackage;
			}
		}

		
		if ( PackagesToSave.Num() == 0 )
		{
			// if we are iterative cooking the package might already be cooked
			// so just add the package to the cooked packages list
			// this could also happen if the source file doesn't exist which is often as we request files with different extensions when we are searching for files
			// just return that we processed the cook request
			// the network file manager will then handle the missing file and search somewhere else
			UE_LOG(LogCook, Verbose, TEXT("Not cooking package %s"), *ToBuild.GetFilename().ToString());

			// did not cook this package 
#if DO_CHECK
			// make sure this package doesn't exist
			for (const FName& TargetPlatformName : ToBuild.GetPlatformNames())
			{
				const FString SandboxFilename = ConvertToFullSandboxPath(ToBuild.GetFilename().ToString(), true, TargetPlatformName.ToString());
				if (IFileManager::Get().FileExists(*SandboxFilename))
				{
					// if we find the file this means it was cooked on a previous cook, however source package can't be found now. 
					// this could be because the source package was deleted or renamed, and we are using iterative cooking
					// perhaps in this case we should delete it?
					UE_LOG(LogCook, Warning, TEXT("Found cooked file which shouldn't exist as it failed loading %s"), *SandboxFilename); 
					IFileManager::Get().Delete(*SandboxFilename);
				}
			}
#endif
			CookedPackages.Add( FFilePlatformCookedPackage( ToBuild.GetFilename(), TargetPlatformNames) );
			continue;
		}

		bool bIsAllDataCached = true;

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		TArray<const ITargetPlatform*> TargetPlatforms;
		for ( const FName& TargetPlatformName : AllTargetPlatformNames )
		{
			TargetPlatforms.Add( TPM.FindTargetPlatform( TargetPlatformName.ToString() ) );
		}


		GShaderCompilingManager->ProcessAsyncResults(true, false);


		if ( PackagesToSave.Num() )
		{
			SCOPE_TIMER(CallBeginCacheForCookedPlatformData);
			// cache the resources for this package for each platform
			
			bIsAllDataCached &= BeginPackageCacheForCookedPlatformData(PackagesToSave[0], TargetPlatforms, Timer);
			if( bIsAllDataCached )
			{
				bIsAllDataCached &= FinishPackageCacheForCookedPlatformData(PackagesToSave[0], TargetPlatforms, Timer);
			}
		}


		bool ShouldTickPrecache = true;

		// if we are ready to save then don't waste time precaching other stuff
		if ( bIsAllDataCached == true )
		{
			ShouldTickPrecache = false;
		}
		// don't do this if we are in a commandlet because the save section will prefetch 
		if (!IsRealtimeMode())
		{
			ShouldTickPrecache = false;
		}
		else
		{
			// if we are doing no shader compilation right now try and precache something so that we load up the cpu
			if ( GShaderCompilingManager->GetNumRemainingJobs() == 0 )
			{
				ShouldTickPrecache = true;
			}
		}

		// cook on the fly mode we don't want to precache here because save package is going to stall on this package, we don't want to flood the system with precache requests before we stall
		if (IsCookOnTheFlyMode()) 
		{
			ShouldTickPrecache = false;
		}

		// if we are in the cook commandlet then this data will get cached in the save package section
		// if ( (bIsAllDataCached == false) && IsRealtimeMode())
		if (ShouldTickPrecache)
		{
			double PrecacheTimeSlice = Timer.GetTimeRemain();
			if (PrecacheTimeSlice > 0.0f)
			{
				TickPrecacheObjectsForPlatforms(PrecacheTimeSlice, TargetPlatforms);
			}
		}

		int32 FirstUnsolicitedPackage = PackagesToSave.Num();

		UE_LOG(LogCook, Verbose, TEXT("Finding unsolicited packages for package %s"), *ToBuild.GetFilename().ToString());
		bool ContainsFullAssetGCClasses = false;
		GetAllUnsolicitedPackages(PackagesToSave, AllTargetPlatformNames, ContainsFullAssetGCClasses);
		if (ContainsFullAssetGCClasses)
		{
			Result |= COSR_RequiresGC;
		}


		
		// in cook by the book bail out early because shaders and compiled for the primary package we are trying to save. 
		// note in this case we also put the package at the end of the queue that queue might be reordered if we do partial gc
		if ((bIsAllDataCached == false) && IsCookByTheBookMode() && !IsRealtimeMode())
		{
			// don't load anymore stuff unless we have space and we don't already have enough stuff to save
			if ((!(Result & COSR_RequiresGC)) &&
				(HasExceededMaxMemory() == false) &&
				((Timer.NumPackagesSaved + PackagesToSave.Num()) < Timer.MaxNumPackagesToSave)) // if we don't need to GC and also we have memory then load some more packages ;)
			{
				GShaderCompilingManager->ProcessAsyncResults(true, false); // we can afford to do work here because we are essentually requing this package for processing later
				Timer.SavedPackage();  // this is a special case to prevent infinite loop, if we only have one package we might fall through this and could loop forever.  
				CookRequests.EnqueueUnique(ToBuild, false);
				continue;
			}
		}

		bool bFinishedSave = SaveCookedPackages( PackagesToSave, AllTargetPlatformNames, TargetPlatforms, Timer, FirstUnsolicitedPackage, CookedPackageCount, Result );


		// TODO: Daniel: this is reference code needs to be reimplemented on the callee side.
		//  cooker can't depend on callee being able to garbage collect
		// collect garbage
		if ( CookByTheBookOptions && CookByTheBookOptions->bLeakTest && bFinishedSave )
		{
			check( CurrentCookMode == ECookMode::CookByTheBook);
			UE_LOG(LogCook, Display, TEXT("Full GC..."));

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			for (FObjectIterator It; It; ++It)
			{
				if (!CookByTheBookOptions->LastGCItems.Contains(FWeakObjectPtr(*It)))
				{
					UE_LOG(LogCook, Warning, TEXT("\tLeaked %s"), *(It->GetFullName()));
					CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
				}
			}
		}

		
		// if we are cook on the fly (even in editor) and we have cook requests, try process them straight away
		/*if ( IsCookOnTheFlyMode() && CookRequests.HasItems() )
		{
			continue;
		}*/
		
		if ( Timer.IsTimeUp() )
		{
			break;
		}
	}
	

	if ( IsCookOnTheFlyMode() && (IsCookingInEditor() == false) )
	{
		static int32 TickCounter = 0;
		++TickCounter;
		if ( TickCounter > 50 )
		{
			// dump stats every 50 ticks or so
			DumpStats();
			TickCounter = 0;
		}
	}

	OUTPUT_TIMERS();

	if (CookByTheBookOptions)
	{
		CookByTheBookOptions->CookTime += Timer.GetTimeTillNow();
	}

	if (IsCookByTheBookRunning() &&
		(CookRequests.HasItems() == false) &&
		(!(Result & COSR_WaitingOnChildCookers)))
	{
		check(IsCookByTheBookMode());

		// if we are out of stuff and we are in cook by the book from the editor mode then we finish up
		CookByTheBookFinished();
	}

	return Result;
}

bool UCookOnTheFlyServer::BeginPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, FCookerTimer& Timer) const
{
	COOK_STAT(FScopedDurationTimer DurationTimer(DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec));

#if DEBUG_COOKONTHEFLY 
	UE_LOG(LogCook, Display, TEXT("Caching objects for package %s"), *Package->GetFName().ToString());
#endif
	MakePackageFullyLoaded(Package);
	FReentryData& CurrentReentryData = GetReentryData(Package);

	if (CurrentReentryData.bIsValid == false)
		return true;

	if (CurrentReentryData.bBeginCacheFinished)
		return true;

	for (; CurrentReentryData.BeginCacheCount < CurrentReentryData.CachedObjectsInOuter.Num(); ++CurrentReentryData.BeginCacheCount)
	{
		UObject* Obj = CurrentReentryData.CachedObjectsInOuter[CurrentReentryData.BeginCacheCount];
		for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
		{
			const FName ClassFName = Obj->GetClass()->GetFName();
			int32* CurrentAsyncCache = CurrentAsyncCacheForType.Find(ClassFName);
			if ( CurrentAsyncCache != nullptr )
			{
				if ( *CurrentAsyncCache <= 0 )
				{
					return false;
				}

				int32* Value = CurrentReentryData.BeginCacheCallCount.Find(ClassFName);
				if ( !Value )
				{
					CurrentReentryData.BeginCacheCallCount.Add(ClassFName,1);
				}
				else
				{
					*Value += 1;
				}
				CurrentAsyncCache -= 1;
			}

			if (Obj->IsA(UMaterialInterface::StaticClass()))
			{
				if (GShaderCompilingManager->GetNumRemainingJobs() > MaxConcurrentShaderJobs)
				{
#if DEBUG_COOKONTHEFLY
					UE_LOG(LogCook, Display, TEXT("Delaying shader compilation of material %s"), *Obj->GetFullName());
#endif
					return false;
				}
			}
			Obj->BeginCacheForCookedPlatformData(TargetPlatform);
		}

		if (Timer.IsTimeUp())
		{
#if DEBUG_COOKONTHEFLY
			UE_LOG(LogCook, Display, TEXT("Object %s took too long to cache"), *Obj->GetFullName());
#endif
			return false;
		}
	}

	CurrentReentryData.bBeginCacheFinished = true;
	return true;

}

bool UCookOnTheFlyServer::FinishPackageCacheForCookedPlatformData(UPackage* Package, const TArray<const ITargetPlatform*>& TargetPlatforms, FCookerTimer& Timer) const
{
	COOK_STAT(FScopedDurationTimer DurationTimer(DetailedCookStats::TickCookOnTheSideFinishPackageCacheForCookedPlatformDataTimeSec));

	MakePackageFullyLoaded(Package);
	FReentryData& CurrentReentryData = GetReentryData(Package);

	if (CurrentReentryData.bIsValid == false)
		return true;

	if (CurrentReentryData.bFinishedCacheFinished)
		return true;

	for (UObject* Obj : CurrentReentryData.CachedObjectsInOuter)
	{
		for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
		{
			COOK_STAT(double CookerStatSavedValue = DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec);

			if (Obj->IsA(UMaterialInterface::StaticClass()))
			{
				if (Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform) == false)
				{
					if (GShaderCompilingManager->GetNumRemainingJobs() > MaxConcurrentShaderJobs)
					{
						return false;
					}
				}
			}

			// These begin cache calls should be quick 
			// because they will just be checking that the data is already cached and kicking off new multithreaded requests if not
			// all sync requests should have been caught in the first begincache call above
			Obj->BeginCacheForCookedPlatformData(TargetPlatform);
			// We want to measure inclusive time for this function, but not accumulate into the BeginXXX timer, so subtract these times out of the BeginTimer.
			COOK_STAT(DetailedCookStats::TickCookOnTheSideBeginPackageCacheForCookedPlatformDataTimeSec = CookerStatSavedValue);
			if (Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform) == false)
			{
#if DEBUG_COOKONTHEFLY
				UE_LOG(LogCook, Display, TEXT("Object %s isn't cached yet"), *Obj->GetFullName());
#endif
				/*if ( Obj->IsA(UMaterial::StaticClass()) )
				{
				if (GShaderCompilingManager->HasShaderJobs() == false)
				{
				UE_LOG(LogCook, Warning, TEXT("Shader compiler is in a bad state!  Shader %s is finished compile but shader compiling manager did not notify shader.  "), *Obj->GetPathName());
				}
				}*/
				return false;
			}
		}
	}

	for (UObject* Obj : CurrentReentryData.CachedObjectsInOuter)
	{
		// if this objects data is cached then we can call FinishedCookedPLatformDataCache
		// we can only safely call this when we are finished caching this object completely.
		// this doesn't ever happen for cook in editor or cook on the fly mode
		if (CurrentCookMode == ECookMode::CookByTheBook)
		{
			check(!IsCookingInEditor());
			// this might be run multiple times for a single object
			Obj->WillNeverCacheCookedPlatformDataAgain();
		}
	}

	// all these objects have finished so release their async begincache back to the pool
	for (const auto& FinishedCached : CurrentReentryData.BeginCacheCallCount )
	{
		int32* Value = CurrentAsyncCacheForType.Find( FinishedCached.Key );
		check( Value);
		*Value += FinishedCached.Value;
	}
	CurrentReentryData.BeginCacheCallCount.Empty();

	CurrentReentryData.bFinishedCacheFinished = true;
	return true;
}

UPackage* UCookOnTheFlyServer::LoadPackageForCooking(const FString& BuildFilename)
{
	COOK_STAT(FScopedDurationTimer LoadPackagesTimer(DetailedCookStats::TickCookOnTheSideLoadPackagesTimeSec));
	UPackage *Package = NULL;
	{
		FString PackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(BuildFilename, PackageName))
		{
			Package = FindObject<UPackage>(ANY_PACKAGE, *PackageName);
		}
	}

#if DEBUG_COOKONTHEFLY
	UE_LOG(LogCook, Display, TEXT("Processing request %s"), *BuildFilename);
#endif
	static TSet<FString> CookWarningsList;
	if (CookWarningsList.Contains(BuildFilename) == false)
	{
		CookWarningsList.Add(BuildFilename);
		GOutputCookingWarnings = IsCookFlagSet(ECookInitializationFlags::OutputVerboseCookerWarnings);
	}

	//  if the package is already loaded then try to avoid reloading it :)
	if ((Package == NULL) || (Package->IsFullyLoaded() == false))
	{
		GIsCookerLoadingPackage = true;
		SCOPE_TIMER(LoadPackage);
		Package = LoadPackage(NULL, *BuildFilename, LOAD_None);
		INC_INT_STAT(LoadPackage, 1);

		GIsCookerLoadingPackage = false;
	}
#if DEBUG_COOKONTHEFLY
	else
	{
		UE_LOG(LogCook, Display, TEXT("Package already loaded %s avoiding reload"), *BuildFilename);
	}
#endif

	if (Package == NULL)
	{
		if ((!IsCookOnTheFlyMode()) || (!IsCookingInEditor()))
		{
			LogCookerMessage(FString::Printf(TEXT("Error loading %s!"), *BuildFilename), EMessageSeverity::Error);
			UE_LOG(LogCook, Error, TEXT("Error loading %s!"), *BuildFilename);
		}
	}
	GOutputCookingWarnings = false;
	return Package;
}


void UCookOnTheFlyServer::OpportunisticSaveInMemoryPackages()
{
	const float TimeSlice = 0.01f;
	FCookerTimer Timer(TimeSlice, IsRealtimeMode());
	TArray<UPackage*> PackagesToSave;
	bool ContainsFullAssetGCClasses = false;

	TArray<FName> TargetPlatformNames;
	for (const auto& TargetPlatform : PresaveTargetPlatforms)
	{
		TargetPlatformNames.Add(FName(*TargetPlatform->PlatformName()));
	}


	GetAllUnsolicitedPackages(PackagesToSave, TargetPlatformNames, ContainsFullAssetGCClasses);

	
	uint32 CookedPackageCount = 0;
	uint32 Result = 0;
	SaveCookedPackages( PackagesToSave, TargetPlatformNames, PresaveTargetPlatforms, Timer, 0, CookedPackageCount, Result);
}

void UCookOnTheFlyServer::GetAllUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, const TArray<FName>& TargetPlatformNames, bool& ContainsFullAssetGCClasses )
{
	// generate a list of other packages which were loaded with this one
	if (!IsCookByTheBookMode() || (CookByTheBookOptions->bDisableUnsolicitedPackages == false))
	{
		{
			SCOPE_TIMER(PostLoadPackageFixup);
			for (TObjectIterator<UPackage> It; It; ++It)
			{
				const FName StandardPackageName = GetCachedStandardPackageFileFName(*It);
				if (CookedPackages.Exists(StandardPackageName, TargetPlatformNames))
				{
					continue;
				}
				PostLoadPackageFixup(*It);
			}
		}
		SCOPE_TIMER(UnsolicitedMarkup);
		GetUnsolicitedPackages(PackagesToSave, ContainsFullAssetGCClasses, TargetPlatformNames);
	}
}

bool UCookOnTheFlyServer::SaveCookedPackages(TArray<UPackage*>& PackagesToSave, const TArray<FName>& TargetPlatformNames, const TArray<const ITargetPlatform*>& TargetPlatformsToCache, FCookerTimer& Timer, 
	int32 FirstUnsolicitedPackage, uint32& CookedPackageCount , uint32& Result)
{
	bool bIsAllDataCached = true;

	const TArray<FName>& AllTargetPlatformNames = TargetPlatformNames;
	

	bool bFinishedSave = true;

	if (PackagesToSave.Num())
	{
		int32 OriginalPackagesToSaveCount = PackagesToSave.Num();
		SCOPE_TIMER(SavingPackages);
		for (int32 I = 0; I < PackagesToSave.Num(); ++I)
		{
			UPackage* Package = PackagesToSave[I];
			if (Package->IsLoadedByEditorPropertiesOnly() && UncookedEditorOnlyPackages.Contains(Package->GetFName()))
			{
				// We already attempted to cook this package and it's still not referenced by any non editor-only properties.
				continue;
			}

			// This package is valid, so make sure it wasn't previously marked as being an uncooked editor only package or it would get removed from the
			// asset registry at the end of the cook
			UncookedEditorOnlyPackages.Remove(Package->GetFName());

			const FName StandardPackageFilename = GetCachedStandardPackageFileFName(Package);
			check(IsInGameThread());
			if (NeverCookPackageList.Contains(StandardPackageFilename))
			{
				// refuse to save this package, it's clearly one of the undesirables
				continue;
			}


			FName PackageFName = GetCachedStandardPackageFileFName(Package);
			TArray<FName> SaveTargetPlatformNames = AllTargetPlatformNames;
			TArray<FName> CookedTargetPlatforms;
			if (CookedPackages.GetCookedPlatforms(PackageFName, CookedTargetPlatforms))
			{
				for (const FName& CookedPlatform : CookedTargetPlatforms)
				{
					SaveTargetPlatformNames.Remove(CookedPlatform);
				}
			}

			// we somehow already cooked this package not sure how that can happen because the PackagesToSave list should have already filtered this
			if (SaveTargetPlatformNames.Num() == 0)
			{
				UE_LOG(LogCook, Warning, TEXT("Allready saved this package not sure how this got here!"));
				// already saved this package
				continue;
			}


			// if we are processing unsolicited packages we can optionally not save these right now
			// the unsolicited packages which we missed now will be picked up on next run
			// we want to do this in cook on the fly also, if there is a new network package request instead of saving unsolicited packages we can process the requested package

			bool bShouldFinishTick = false;

			if (Timer.IsTimeUp() && IsCookByTheBookMode() )
			{
				bShouldFinishTick = true;
				// our timeslice is up
			}

			// if we are cook the fly then save the package which was requested as fast as we can because the client is waiting on it

			bool ProcessingUnsolicitedPackages = (I >= FirstUnsolicitedPackage);
			bool bForceSavePackage = false;

			if (IsCookOnTheFlyMode())
			{
				if (ProcessingUnsolicitedPackages)
				{
					SCOPE_TIMER(WaitingForCachedCookedPlatformData);
					if (CookRequests.HasItems())
					{
						bShouldFinishTick = true;
					}
					if (Timer.IsTimeUp())
					{
						bShouldFinishTick = true;
						// our timeslice is up
					}
					bool bFinishedCachingCookedPlatformData = false;
					// if we are in realtime mode then don't wait forever for the package to be ready
					while ((!Timer.IsTimeUp()) && IsRealtimeMode() && (bShouldFinishTick == false))
					{
						if (FinishPackageCacheForCookedPlatformData(Package, TargetPlatformsToCache, Timer) == true)
						{
							bFinishedCachingCookedPlatformData = true;
							break;
						}

						GShaderCompilingManager->ProcessAsyncResults(true, false);
						// sleep for a bit
						FPlatformProcess::Sleep(0.0f);
					}
					bShouldFinishTick |= !bFinishedCachingCookedPlatformData;
				}
				else
				{
					if (!IsRealtimeMode())
					{
						bForceSavePackage = true;
					}
				}
			}

			bool AllObjectsCookedDataCached = true;
			bool HasCheckedAllPackagesAreCached = (I >= OriginalPackagesToSaveCount);

			MakePackageFullyLoaded(Package);

			if ( IsCookOnTheFlyMode() )
			{
				// never want to requeue packages
				HasCheckedAllPackagesAreCached = true;
			}

			// if we are forcing save the package then it doesn't matter if we call FinishPackageCacheForCookedPlatformData
			if (!bShouldFinishTick && !bForceSavePackage)
			{
				AllObjectsCookedDataCached = FinishPackageCacheForCookedPlatformData(Package, TargetPlatformsToCache, Timer);
				if (AllObjectsCookedDataCached == false)
				{
					GShaderCompilingManager->ProcessAsyncResults(true, false);
					AllObjectsCookedDataCached = FinishPackageCacheForCookedPlatformData(Package, TargetPlatformsToCache, Timer);
				}
			}

			// if we are in realtime mode and this package isn't ready to be saved then we should exit the tick here so we don't save it while in launch on
			if (IsRealtimeMode() &&
				(AllObjectsCookedDataCached == false) &&
				HasCheckedAllPackagesAreCached)
			{
				bShouldFinishTick = true;
			}

			if (bShouldFinishTick && (!bForceSavePackage))
			{
				SCOPE_TIMER(EnqueueUnsavedPackages);
				// enqueue all the packages which we were about to save
				Timer.SavedPackage();  // this is a special case to prevent infinite loop, if we only have one package we might fall through this and could loop forever.  
				int32 NumPackagesToRequeue = PackagesToSave.Num();

				if (IsCookOnTheFlyMode())
				{
					NumPackagesToRequeue = FirstUnsolicitedPackage;
				}
				
				for (int32 RemainingIndex = I; RemainingIndex < NumPackagesToRequeue; ++RemainingIndex)
				{
					FName StandardFilename = GetCachedStandardPackageFileFName(PackagesToSave[RemainingIndex]);
					CookRequests.EnqueueUnique(FFilePlatformRequest(StandardFilename, AllTargetPlatformNames));
				}
				Result |= COSR_WaitingOnCache;

				// break out of the loop
				bFinishedSave = false;
				break;
			}


			// don't precache other packages if our package isn't ready but we are going to save it.   This will fill up the worker threads with extra shaders which we may need to flush on 
			if ((!IsCookOnTheFlyMode()) &&
				(!IsRealtimeMode() || AllObjectsCookedDataCached == true))
			{
				// precache platform data for next package 
				UPackage *NextPackage = PackagesToSave[FMath::Min(PackagesToSave.Num() - 1, I + 1)];
				UPackage *NextNextPackage = PackagesToSave[FMath::Min(PackagesToSave.Num() - 1, I + 2)];
				if (NextPackage != Package)
				{
					SCOPE_TIMER(PrecachePlatformDataForNextPackage);
					BeginPackageCacheForCookedPlatformData(NextPackage, TargetPlatformsToCache, Timer);
				}
				if (NextNextPackage != NextPackage)
				{
					SCOPE_TIMER(PrecachePlatformDataForNextNextPackage);
					BeginPackageCacheForCookedPlatformData(NextNextPackage, TargetPlatformsToCache, Timer);
				}
			}

			// if we are running the cook commandlet
			// if we already went through the entire package list then don't keep requeuing requests
			if ((HasCheckedAllPackagesAreCached == false) &&
				(AllObjectsCookedDataCached == false) &&
				(bForceSavePackage == false) &&
				IsCookByTheBookMode() )
			{
				// check(IsCookByTheBookMode() || ProcessingUnsolicitedPackages == true);
				// add to back of queue
				PackagesToSave.Add(Package);
				// UE_LOG(LogCook, Display, TEXT("Delaying save for package %s"), *PackageFName.ToString());
				continue;
			}

			if ( HasCheckedAllPackagesAreCached && (AllObjectsCookedDataCached == false) )
			{
				UE_LOG(LogCook, Display, TEXT("Forcing save package %s because was already requeued once"), *PackageFName.ToString());
			}


			bool bShouldSaveAsync = true;
			FString Temp;
			if (FParse::Value(FCommandLine::Get(), TEXT("-diffagainstcookdirectory="), Temp) || FParse::Value(FCommandLine::Get(), TEXT("-breakonfile="), Temp))
			{
				// async save doesn't work with this flags
				bShouldSaveAsync = false;
			}

			TArray<bool> SucceededSavePackage;
			TArray<FSavePackageResultStruct> SavePackageResults;
			{
				COOK_STAT(FScopedDurationTimer TickCookOnTheSideSaveCookedPackageTimer(DetailedCookStats::TickCookOnTheSideSaveCookedPackageTimeSec));
				SCOPE_TIMER(SaveCookedPackage);
				uint32 SaveFlags = SAVE_KeepGUID | (bShouldSaveAsync ? SAVE_Async : SAVE_None) | (IsCookFlagSet(ECookInitializationFlags::Unversioned) ? SAVE_Unversioned : 0);

				bool KeepEditorOnlyPackages = false;
				// removing editor only packages only works when cooking in commandlet and non iterative cooking
				// also doesn't work in multiprocess cooking
				KeepEditorOnlyPackages = !(IsCookByTheBookMode() && !IsCookingInEditor());
				KeepEditorOnlyPackages |= IsCookFlagSet(ECookInitializationFlags::Iterative);
				KeepEditorOnlyPackages |= IsChildCooker() || (CookByTheBookOptions && CookByTheBookOptions->ChildCookers.Num() > 0);
				SaveFlags |= KeepEditorOnlyPackages ? SAVE_KeepEditorOnlyCookedPackages : SAVE_None;

				GOutputCookingWarnings = IsCookFlagSet(ECookInitializationFlags::OutputVerboseCookerWarnings);
				SaveCookedPackage(Package, SaveFlags, SaveTargetPlatformNames, SavePackageResults);
				GOutputCookingWarnings = false;
				check(SaveTargetPlatformNames.Num() == SavePackageResults.Num());
				for (int iResultIndex = 0; iResultIndex < SavePackageResults.Num(); iResultIndex++)
				{
					FSavePackageResultStruct& SavePackageResult = SavePackageResults[iResultIndex];

					if (SavePackageResult == ESavePackageResult::Success || SavePackageResult == ESavePackageResult::GenerateStub || SavePackageResult == ESavePackageResult::ReplaceCompletely)
					{
						SucceededSavePackage.Add(true);
						// Update flags used to determine garbage collection.
						if (Package->ContainsMap())
						{
							Result |= COSR_CookedMap;
						}
						else
						{
							++CookedPackageCount;
							Result |= COSR_CookedPackage;
						}

						// Update asset registry
						if (CookByTheBookOptions)
						{
							FAssetRegistryGenerator* Generator = RegistryGenerators.FindRef(SaveTargetPlatformNames[iResultIndex]);
							if (Generator)
							{
								FAssetPackageData* PackageData = Generator->GetAssetPackageData(Package->GetFName());
								PackageData->DiskSize = SavePackageResult.TotalFileSize;
							}
						}

					}
					else
					{
						SucceededSavePackage.Add(false);
					}
				}
				check(SavePackageResults.Num() == SucceededSavePackage.Num());
				Timer.SavedPackage();
			}

			if (IsCookingInEditor() == false)
			{
				SCOPE_TIMER(ClearAllCachedCookedPlatformData);
				TArray<UObject*> ObjectsInPackage;
				GetObjectsWithOuter(Package, ObjectsInPackage);
				for (UObject* Object : ObjectsInPackage)
				{
					Object->ClearAllCachedCookedPlatformData();
				}
			}

				//@todo ResetLoaders outside of this (ie when Package is NULL) causes problems w/ default materials
			FName StandardFilename = GetCachedStandardPackageFileFName(Package);

			// We always want to mark package as processed unless it wasn't saved because it was referenced by editor-only data
			// in which case we may still need to save it later when new content loads it through non editor-only references
			if (StandardFilename != NAME_None)
			{
				// mark the package as cooked
				FFilePlatformCookedPackage FileRequest(StandardFilename, SaveTargetPlatformNames, SucceededSavePackage);
				bool bWasReferencedOnlyByEditorOnlyData = false;
				for (const FSavePackageResultStruct& SavePackageResult : SavePackageResults)
				{
					if (SavePackageResult == ESavePackageResult::ReferencedOnlyByEditorOnlyData)
					{
						bWasReferencedOnlyByEditorOnlyData = true;
						// if this is the case all of the packages should be referenced only by editor only data
					}
				}
				if (!bWasReferencedOnlyByEditorOnlyData)
				{
					CookedPackages.Add(FileRequest);
					if ((CurrentCookMode == ECookMode::CookOnTheFly) && (I >= FirstUnsolicitedPackage))
					{
						// this is an unsolicited package
						if (FPaths::FileExists(FileRequest.GetFilename().ToString()) == true)
						{
							UnsolicitedCookedPackages.AddCookedPackage(FileRequest);
#if DEBUG_COOKONTHEFLY
							UE_LOG(LogCook, Display, TEXT("UnsolicitedCookedPackages: %s"), *FileRequest.GetFilename().ToString());
#endif
						}
					}
				}
				else
				{
					UncookedEditorOnlyPackages.AddUnique(Package->GetFName());
				}
			}
			else
			{
				for (const bool bSucceededSavePackage : SucceededSavePackage)
				{
					check(bSucceededSavePackage == false);
				}
			}
		}
	}
	return true;
}


void UCookOnTheFlyServer::PostLoadPackageFixup(UPackage* Package)
{
	if (Package->ContainsMap())
	{
		// load sublevels
		UWorld* World = UWorld::FindWorldInPackage(Package);
		check(World);

		World->PersistentLevel->HandleLegacyMapBuildData();

		// child cookers can't process maps
		// check(IsChildCooker() == false);
		if (IsCookByTheBookMode())
		{
			GIsCookerLoadingPackage = true;
			// TArray<FString> PreviouslyCookedPackages;
			if (World->StreamingLevels.Num())
			{
				//World->LoadSecondaryLevels(true, &PreviouslyCookedPackages);
				World->LoadSecondaryLevels(true, NULL);
			}
			GIsCookerLoadingPackage = false;
			TArray<FString> NewPackagesToCook;

			// Collect world composition tile packages to cook
			if (World->WorldComposition)
			{
				World->WorldComposition->CollectTilesToCook(NewPackagesToCook);
			}

			for (const FString& PackageName : NewPackagesToCook)
			{
				FName StandardPackageFName = GetCachedStandardPackageFileFName(FName(*PackageName));
				if (StandardPackageFName != NAME_None)
				{
					if (IsChildCooker())
					{
						check(IsCookByTheBookMode());
						// notify the main cooker that it should make sure this package gets cooked
						CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
					}
					else
					{
						RequestPackage(StandardPackageFName, false);
					}
				}
			}
		}

	}
}


void UCookOnTheFlyServer::TickPrecacheObjectsForPlatforms(const float TimeSlice, const TArray<const ITargetPlatform*>& TargetPlatforms) 
{
	
	SCOPE_CYCLE_COUNTER(STAT_TickPrecacheCooking);

	
	FCookerTimer Timer(TimeSlice, true);

	++LastUpdateTick;
	if (LastUpdateTick > 50 ||
		((CachedMaterialsToCacheArray.Num() == 0) && (CachedTexturesToCacheArray.Num() == 0)))
	{
		LastUpdateTick = 0;
		TArray<UObject*> Materials;
		GetObjectsOfClass(UMaterial::StaticClass(), Materials, true);
		for (UObject* Material : Materials)
		{
			if ( Material->GetOutermost() == GetTransientPackage())
				continue;

			CachedMaterialsToCacheArray.Add(Material);
		}
		TArray<UObject*> Textures;
		GetObjectsOfClass(UTexture::StaticClass(), Textures, true);
		for (UObject* Texture : Textures)
		{
			if (Texture->GetOutermost() == GetTransientPackage())
				continue;

			CachedTexturesToCacheArray.Add(Texture);
		}
	}

	if (Timer.IsTimeUp())
		return;

	bool AllMaterialsCompiled = true;
	// queue up some shaders for compilation

	while (CachedMaterialsToCacheArray.Num() > 0)
	{
		UMaterial* Material = (UMaterial*)(CachedMaterialsToCacheArray[0].Get());
		CachedMaterialsToCacheArray.RemoveAtSwap(0, 1, false);

		if (Material == nullptr)
		{
			continue;
		}

		for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
		{
			if (!TargetPlatform)
			{
				continue;
			}
			if (!Material->IsCachedCookedPlatformDataLoaded(TargetPlatform))
			{
				Material->BeginCacheForCookedPlatformData(TargetPlatform);
				AllMaterialsCompiled = false;
			}
		}

		if (Timer.IsTimeUp())
			return;

		if (GShaderCompilingManager->GetNumRemainingJobs() > MaxPrecacheShaderJobs)
		{
			return;
		}
	}


	if (!AllMaterialsCompiled)
	{
		return;
	}

	while (CachedTexturesToCacheArray.Num() > 0)
	{
		UTexture* Texture = (UTexture*)(CachedTexturesToCacheArray[0].Get());
		CachedTexturesToCacheArray.RemoveAtSwap(0, 1, false);

		if (Texture == nullptr)
		{
			continue;
		}

		for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
		{
			if (!TargetPlatform)
			{
				continue;
			}
			Texture->BeginCacheForCookedPlatformData(TargetPlatform);
		}
		if (Timer.IsTimeUp())
			return;
	}

}

bool UCookOnTheFlyServer::HasExceededMaxMemory() const
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();

	//  if we have less emmory free then we should have then gc some stuff
	if ((MemStats.AvailablePhysical < MinFreeMemory) && 
		(MinFreeMemory != 0) )
	{
		UE_LOG(LogCook, Display, TEXT("Available physical memory low %d kb, exceeded max memory"), MemStats.AvailablePhysical / 1024);
		return true;
	}

	// don't gc if we haven't reached our min gc level yet
	if (MemStats.UsedVirtual < MinMemoryBeforeGC)
	{
		return false;
	}

	//uint64 UsedMemory = MemStats.UsedVirtual; 
	uint64 UsedMemory = MemStats.UsedPhysical; //should this be used virtual?
	if ((UsedMemory >= MaxMemoryAllowance) &&
		(MaxMemoryAllowance > 0u))
	{
		UE_LOG(LogCook, Display, TEXT("Used memory high %d kb, exceeded max memory"), MemStats.UsedPhysical / 1024);
		return true;
	}

	

	return false;
}

void UCookOnTheFlyServer::GetUnsolicitedPackages(TArray<UPackage*>& PackagesToSave, bool &ContainsFullGCAssetClasses, const TArray<FName>& TargetPlatformNames) const
{
	TSet<UPackage*> PackagesToSaveSet;
	for (UPackage* Package : PackagesToSave)
	{
		PackagesToSaveSet.Add(Package);
	}
	PackagesToSave.Empty();
			
	TArray<UObject*> ObjectsInOuter;
	{
		SCOPE_TIMER(GetObjectsWithOuter);
		GetObjectsWithOuter(NULL, ObjectsInOuter, false);
	}

	TArray<FName> PackageNames;
	PackageNames.Empty(ObjectsInOuter.Num());
	{
		SCOPE_TIMER(GeneratePackageNames);
		ACCUMULATE_TIMER(UnsolicitedPackageAlreadyCooked);
		ACCUMULATE_TIMER(PackageCast);
		ACCUMULATE_TIMER(FullGCAssetsContains);
		ACCUMULATE_TIMER(AddUnassignedPackageToManifest);
		ACCUMULATE_TIMER(GetCachedName);
		ACCUMULATE_TIMER(AddToPackageList);
		for (int32 Index = 0; Index < ObjectsInOuter.Num(); Index++)
		{
			ACCUMULATE_TIMER_START(PackageCast);
			UPackage* Package = Cast<UPackage>(ObjectsInOuter[Index]);
			ACCUMULATE_TIMER_STOP(PackageCast);

			ACCUMULATE_TIMER_START(FullGCAssetsContains);
			UObject* Object = ObjectsInOuter[Index];
			if (FullGCAssetClasses.Contains(Object->GetClass()))
			{
				ContainsFullGCAssetClasses = true;
			}
			
			ACCUMULATE_TIMER_STOP(FullGCAssetsContains);

			if (Package)
			{
				ACCUMULATE_TIMER_START(GetCachedName);
				FName StandardPackageFName = GetCachedStandardPackageFileFName(Package);
				ACCUMULATE_TIMER_STOP(GetCachedName);
				if (StandardPackageFName == NAME_None)
					continue;

				ACCUMULATE_TIMER_START(UnsolicitedPackageAlreadyCooked);
				// package is already cooked don't care about processing it again here
				/*if ( CookRequests.Exists( StandardPackageFName, AllTargetPlatformNames) )
					continue;*/

				if (CookedPackages.Exists(StandardPackageFName, TargetPlatformNames))
					continue;
				ACCUMULATE_TIMER_STOP(UnsolicitedPackageAlreadyCooked);
				
				ACCUMULATE_TIMER_START(AddToPackageList);

				if (StandardPackageFName != NAME_None) // if we have name none that means we are in core packages or something...
				{
					if (IsChildCooker())
					{
						// notify the main cooker that it should make sure this package gets cooked
						CookByTheBookOptions->ChildUnsolicitedPackages.Add(StandardPackageFName);
					}
					else
					{
						// check if the package has already been saved
						bool bAlreadyInSet = false;
						PackagesToSaveSet.Add(Package, &bAlreadyInSet);
						if ( bAlreadyInSet == false )
						{
							UE_LOG(LogCook, Verbose, TEXT("Found unsolicited package to cook %s"), *Package->GetName());
						}
						continue;
					}
				}
				ACCUMULATE_TIMER_STOP(AddToPackageList);
			}
		}
	}
	
	for (UPackage* Package : PackagesToSaveSet )
	{
		PackagesToSave.Add(Package);
	}
}

void UCookOnTheFlyServer::OnObjectModified( UObject *ObjectMoving )
{
	if (IsGarbageCollecting())
	{
		return;
	}
	OnObjectUpdated( ObjectMoving );
}

void UCookOnTheFlyServer::OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (IsGarbageCollecting())
	{
		return;
	}
	if ( PropertyChangedEvent.Property == nullptr && 
		PropertyChangedEvent.MemberProperty == nullptr )
	{
		// probably nothing changed... 
		return;
	}

	OnObjectUpdated( ObjectBeingModified );
}

void UCookOnTheFlyServer::OnObjectSaved( UObject* ObjectSaved )
{
	if (GIsCookerLoadingPackage)
	{
		// This is the cooker saving a cooked package, ignore
		return;
	}

	UPackage* Package = ObjectSaved->GetOutermost();
	if (Package == nullptr || Package == GetTransientPackage())
	{
		return;
	}

	MarkPackageDirtyForCooker(Package);

	// Register the package filename as modified. We don't use the cache because the file may not exist on disk yet at this point
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
	ModifiedAssetFilenames.Add(FName(*PackageFilename));
}

void UCookOnTheFlyServer::OnObjectUpdated( UObject *Object )
{
	// get the outer of the object
	UPackage *Package = Object->GetOutermost();

	MarkPackageDirtyForCooker( Package );
}

void UCookOnTheFlyServer::MarkPackageDirtyForCooker( UPackage *Package )
{
	if ( Package->RootPackageHasAnyFlags(PKG_PlayInEditor) )
	{
		return;
	}
	if (Package->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ContainsScript | PKG_CompiledIn) == true && !GetClass()->HasAnyClassFlags(CLASS_DefaultConfig | CLASS_Config))
	{
		return;
	}

	if (Package == GetTransientPackage())
	{
		return;
	}

	if ( FPackageName::IsMemoryPackage(Package->GetName()))
	{
		return;
	}

	if ( !bIsSavingPackage )
	{
		// could have just cooked a file which we might need to write
		UPackage::WaitForAsyncFileWrites();

		// force that package to be recooked
		const FString Name = Package->GetPathName();

		FName PackageFFileName = GetCachedStandardPackageFileFName(Package);

		if ( PackageFFileName == NAME_None )
		{
			ClearPackageFilenameCacheForPackage( Package );
			return;
		}

		UE_LOG(LogCook, Verbose, TEXT("Modification detected to package %s"), *PackageFFileName.ToString());

		if ( IsCookingInEditor() )
		{
			if ( IsCookByTheBookMode() )
			{
				TArray<FName> CookedPlatforms;
				// if we have already cooked this package and we have made changes then recook ;)
				if ( CookedPackages.GetCookedPlatforms(PackageFFileName, CookedPlatforms) )
				{
					if (IsCookByTheBookRunning())
					{
						// if this package was previously cooked and we are doing a cook by the book 
						// we need to recook this package before finishing cook by the book
						CookRequests.EnqueueUnique(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
					}
					else
					{
						CookByTheBookOptions->PreviousCookRequests.Add(FFilePlatformRequest(PackageFFileName, CookedPlatforms));
					}
				}
			}
			else if ( IsCookOnTheFlyMode() )
			{
				if ( FileModifiedDelegate.IsBound() ) 
				{
					const FString PackageName = PackageFFileName.ToString();
					FileModifiedDelegate.Broadcast(PackageName);
					if ( PackageName.EndsWith(".uasset") || PackageName.EndsWith(".umap"))
					{
						FileModifiedDelegate.Broadcast( FPaths::ChangeExtension(PackageName, TEXT(".uexp")) );
						FileModifiedDelegate.Broadcast( FPaths::ChangeExtension(PackageName, TEXT(".ubulk")) );
						FileModifiedDelegate.Broadcast( FPaths::ChangeExtension(PackageName, TEXT(".ufont")) );
					}
				}
			}
			else
			{
				// this is here if we add a new mode and don't implement this it will crash instead of doing undesireable behaviour 
				check( true);
			}
		}
		CookedPackages.RemoveFile( PackageFFileName );
	}
}

void UCookOnTheFlyServer::EndNetworkFileServer()
{
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		// shutdown the server
		NetworkFileServer->Shutdown();
		delete NetworkFileServer;
		NetworkFileServer = NULL;
	}
	NetworkFileServers.Empty();
}

void UCookOnTheFlyServer::SetFullGCAssetClasses( const TArray<UClass*>& InFullGCAssetClasses)
{
	FullGCAssetClasses = InFullGCAssetClasses;
}

uint32 UCookOnTheFlyServer::GetPackagesPerGC() const
{
	return PackagesPerGC;
}

uint32 UCookOnTheFlyServer::GetPackagesPerPartialGC() const
{
	return MaxNumPackagesBeforePartialGC;
}


int32 UCookOnTheFlyServer::GetMaxConcurrentShaderJobs() const
{
	return MaxConcurrentShaderJobs;
}

double UCookOnTheFlyServer::GetIdleTimeToGC() const
{
	return IdleTimeToGC;
}

uint64 UCookOnTheFlyServer::GetMaxMemoryAllowance() const
{
	return MaxMemoryAllowance;
}
PRAGMA_DISABLE_OPTIMIZATION;
const TArray<FName>& UCookOnTheFlyServer::GetFullPackageDependencies(const FName& PackageName ) const
{
	TArray<FName>* PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
	if ( !PackageDependencies )
	{

		static const FName NAME_CircularReference(TEXT("CircularReference"));
		static int32 UniqueArrayCounter = 0;
		++UniqueArrayCounter;
		FName CircularReferenceArrayName = FName(NAME_CircularReference,UniqueArrayCounter);
		{
			// can't initialize the PackageDependencies array here because we call GetFullPackageDependencies below and that could recurse and resize CachedFullPackageDependencies
			TArray<FName>& TempPackageDependencies = CachedFullPackageDependencies.Add(PackageName); // IMPORTANT READ ABOVE COMMENT
			// initialize temppackagedependencies to a dummy dependency so that we can detect circular references
			TempPackageDependencies.Add(CircularReferenceArrayName);
			// when someone finds the circular reference name they look for this array name in the CachedFullPackageDependencies map
			// and add their own package name to it, so that they can get fixed up 
			CachedFullPackageDependencies.Add(CircularReferenceArrayName);
		}

		TArray<FName> ChildDependencies;
		if ( AssetRegistry->GetDependencies(PackageName, ChildDependencies, EAssetRegistryDependencyType::All) )
		{
			TArray<FName> Dependencies = ChildDependencies;
			Dependencies.AddUnique(PackageName);
			for ( const FName& ChildDependency : ChildDependencies)
			{
				const TArray<FName>& ChildPackageDependencies = GetFullPackageDependencies(ChildDependency);
				for ( const FName& ChildPackageDependency : ChildPackageDependencies )
				{
							if ( ChildPackageDependency == CircularReferenceArrayName )
							{
								continue;
							}
							if ( ChildPackageDependency.GetComparisonIndex() == NAME_CircularReference.GetComparisonIndex() )
							{
								// add our self to the package which we are circular referencing
								TArray<FName>& TempCircularReference = CachedFullPackageDependencies.FindChecked(ChildPackageDependency);
								TempCircularReference.AddUnique(PackageName); // add this package name so that it's dependencies get fixed up when the outer loop returns
							}

					Dependencies.AddUnique(ChildPackageDependency);
				}
			}

			// all these packages referenced us apparently so fix them all up
			const TArray<FName>& PackagesForFixup = CachedFullPackageDependencies.FindChecked(CircularReferenceArrayName);
			for ( const FName FixupPackage : PackagesForFixup )
			{
				TArray<FName> &FixupList = CachedFullPackageDependencies.FindChecked(FixupPackage);
				// check( FixupList.Contains( CircularReferenceArrayName) );
				ensure( FixupList.Remove(CircularReferenceArrayName) == 1 );
				for( const FName AdditionalDependency : Dependencies )
				{
					FixupList.AddUnique(AdditionalDependency);
					if ( AdditionalDependency.GetComparisonIndex() == NAME_CircularReference.GetComparisonIndex() )
					{
						// add our self to the package which we are circular referencing
						TArray<FName>& TempCircularReference = CachedFullPackageDependencies.FindChecked(AdditionalDependency);
						TempCircularReference.AddUnique(FixupPackage); // add this package name so that it's dependencies get fixed up when the outer loop returns
					}
				}
			}
			CachedFullPackageDependencies.Remove(CircularReferenceArrayName);

			PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
			check(PackageDependencies);

			Swap(*PackageDependencies, Dependencies);
		}
		else
		{
			PackageDependencies = CachedFullPackageDependencies.Find(PackageName);
			PackageDependencies->Add(PackageName);
		}
	}
	return *PackageDependencies;
}
PRAGMA_ENABLE_OPTIMIZATION;
void UCookOnTheFlyServer::MarkGCPackagesToKeepForCooker()
{
	// just saved this package will the cooker need this package again this cook?
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Object = *It;
		Object->ClearFlags(RF_KeepForCooker);
	}

	TSet<FName> KeepPackages;
	// first see if the package is in the required to be saved list
	// then see if the package is needed by any of the packages which are required to be saved

	TMap<FName, int32> PackageDependenciesCount;
	for (const FName& QueuedPackage : CookRequests.GetQueue())
	{
		const FName* PackageName = GetCachedPackageFilenameToPackageFName(QueuedPackage);
		if ( !PackageName )
		{
			PackageDependenciesCount.Add(QueuedPackage, 0);
			continue;
		}
		const TArray<FName>& NeededPackages = GetFullPackageDependencies(*PackageName);
		const FName StandardFName = QueuedPackage;
		PackageDependenciesCount.Add(StandardFName, NeededPackages.Num());
		KeepPackages.Append(NeededPackages);
	}

	TSet<FName> LoadedPackages;
	for ( TObjectIterator<UPackage> It; It; ++It )
	{
		UPackage* Package = (UPackage*)(*It);
		if ( KeepPackages.Contains(Package->GetFName()) )
		{
			LoadedPackages.Add( GetCachedStandardPackageFileFName(Package->GetFName()) );
			const FReentryData& ReentryData = GetReentryData(Package);
			Package->SetFlags(RF_KeepForCooker);
			for (UObject* Obj : ReentryData.CachedObjectsInOuter)
			{
				Obj->SetFlags(RF_KeepForCooker);
			}
		}
	}

	// sort the cook requests by the packages which are loaded first
	// then sort by the number of dependencies which are referenced by the package
	// we want to process the packages with the highest dependencies so that they can be evicted from memory and are likely to be able to be released on next gc pass
	CookRequests.Sort([=, &PackageDependenciesCount,&LoadedPackages](const FName& A, const FName& B)
		{
			int32 ADependencies = PackageDependenciesCount.FindChecked(A);
			int32 BDependencies = PackageDependenciesCount.FindChecked(B);
			bool ALoaded = LoadedPackages.Contains(A);
			bool BLoaded = LoadedPackages.Contains(B);
			return (ALoaded == BLoaded) ? (ADependencies > BDependencies) : ALoaded > BLoaded;
		}
	);
}

void UCookOnTheFlyServer::BeginDestroy()
{
	EndNetworkFileServer();

	Super::BeginDestroy();

}

void UCookOnTheFlyServer::TickRecompileShaderRequests()
{
	// try to pull off a request
	FRecompileRequest* Request = NULL;

	RecompileRequests.Dequeue(&Request);

	// process it
	if (Request)
	{
		HandleNetworkFileServerRecompileShaders(Request->RecompileData);


		// all done! other thread can unblock now
		Request->bComplete = true;
	}
}


void UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FSavePackageResultStruct>& SavePackageResults)
{
	TArray<FName> TargetPlatformNames; 
	return SaveCookedPackage( Package, SaveFlags, TargetPlatformNames, SavePackageResults);
}

bool UCookOnTheFlyServer::ShouldCook(const FString& InFileName, const FName &InPlatformName)
{
	return true;
}

bool UCookOnTheFlyServer::ShouldConsiderCompressedPackageFileLengthRequirements() const
{
	bool bConsiderCompressedPackageFileLengthRequirements = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bConsiderCompressedPackageFileLengthRequirements"), bConsiderCompressedPackageFileLengthRequirements, GEditorIni);
	return bConsiderCompressedPackageFileLengthRequirements;
}

bool UCookOnTheFlyServer::MakePackageFullyLoaded(UPackage* Package) const
{
	if ( Package->IsFullyLoaded() )
		return true;

	bool bPackageFullyLoaded = false;
	GIsCookerLoadingPackage = true;
	Package->FullyLoad();
	//LoadPackage(NULL, *Package->GetName(), LOAD_None);
	GIsCookerLoadingPackage = false;
	if (!Package->IsFullyLoaded())
	{
		LogCookerMessage(FString::Printf(TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"),
			*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set")), EMessageSeverity::Warning);

		UE_LOG(LogCook, Warning, TEXT("Package %s supposed to be fully loaded but isn't. RF_WasLoaded is %s"),
			*Package->GetName(), Package->HasAnyFlags(RF_WasLoaded) ? TEXT("set") : TEXT("not set"));
	}
	else
	{
		bPackageFullyLoaded = true;
	}
	// If fully loading has caused a blueprint to be regenerated, make sure we eliminate all meta data outside the package
	UMetaData* MetaData = Package->GetMetaData();
	MetaData->RemoveMetaDataOutsidePackage();

	return bPackageFullyLoaded;
}

void UCookOnTheFlyServer::SaveCookedPackage(UPackage* Package, uint32 SaveFlags, TArray<FName> &TargetPlatformNames, TArray<FSavePackageResultStruct>& SavePackageResults)
{
	check( SavePackageResults.Num() == 0);
	check( bIsSavingPackage == false );
	bIsSavingPackage = true;
	FString Filename(GetCachedPackageFilename(Package));

	// Don't resolve, just add to request list as needed
	TSet<FName> SoftObjectPackages;

	GRedirectCollector.ProcessSoftObjectPathPackageList(Package->GetFName(), false, SoftObjectPackages);
	
	for (FName SoftObjectPackage : SoftObjectPackages)
	{
		TMap<FName, FName> RedirectedPaths;

		// If this is a redirector, extract destination from asset registry
		if (ContainsRedirector(SoftObjectPackage, RedirectedPaths))
		{
			for (TPair<FName, FName>& RedirectedPath : RedirectedPaths)
			{
				GRedirectCollector.AddAssetPathRedirection(RedirectedPath.Key, RedirectedPath.Value);
			}
		}

		// Verify package actually exists
		FName StandardPackageName = GetCachedStandardPackageFileFName(SoftObjectPackage);

		if (StandardPackageName != NAME_None && IsCookByTheBookMode() && !CookByTheBookOptions->bDisableUnsolicitedPackages)
		{
			// Add to front of request queue as an unsolicited package
			RequestPackage(StandardPackageName, true);
		}
	}

	if (Filename.Len() != 0 )
	{
		if (Package->HasAnyPackageFlags(PKG_ReloadingForCooker))
		{
			UE_LOG(LogCook, Warning, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
			UE_LOG(LogCook, Fatal, TEXT("Package %s marked as reloading for cook by was requested to save"), *Package->GetPathName());
		}

		FString Name = Package->GetPathName();

		// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
		Filename = ConvertToFullSandboxPath(*Filename, true);

		uint32 OriginalPackageFlags = Package->GetPackageFlags();
		UWorld* World = nullptr;
		EObjectFlags FlagsToCook = RF_Public;

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

		static TArray<ITargetPlatform*> ActiveStartupPlatforms = TPM.GetCookingTargetPlatforms();

		TArray<ITargetPlatform*> Platforms;

		if ( TargetPlatformNames.Num() )
		{
			const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();

			for (const FName TargetPlatformFName : TargetPlatformNames)
			{
				const FString TargetPlatformName = TargetPlatformFName.ToString();

				for (ITargetPlatform *TargetPlatform  : TargetPlatforms)
				{
					if ( TargetPlatform->PlatformName() == TargetPlatformName )
					{
						Platforms.Add( TargetPlatform );
					}
				}
			}
		}
		else
		{
			Platforms = ActiveStartupPlatforms;

			for (ITargetPlatform *Platform : Platforms)
			{
				TargetPlatformNames.Add(FName(*Platform->PlatformName()));
			}
		}
		
		for (int32 PlatformIndex = 0; PlatformIndex < Platforms.Num(); ++PlatformIndex )
		{
			SavePackageResults.Add(FSavePackageResultStruct(ESavePackageResult::Success));
			ITargetPlatform* Target = Platforms[PlatformIndex];
			FString PlatFilename = Filename.Replace(TEXT("[Platform]"), *Target->PlatformName());

			FSavePackageResultStruct& Result = SavePackageResults[PlatformIndex];

			bool bCookPackage = true;

			// don't save Editor resources from the Engine if the target doesn't have editoronly data
			if (IsCookFlagSet(ECookInitializationFlags::SkipEditorContent) &&
				(Name.StartsWith(TEXT("/Engine/Editor")) || Name.StartsWith(TEXT("/Engine/VREditor"))) &&
				!Target->HasEditorOnlyData())
			{
				bCookPackage = false;
			}

			if (bCookPackage == true)
			{
				bool bPackageFullyLoaded = false;
				if (bPackageFullyLoaded == false)
				{
					SCOPE_TIMER(LoadPackage);

					bPackageFullyLoaded = MakePackageFullyLoaded(Package);

					// look for a world object in the package (if there is one, there's a map)
					World = UWorld::FindWorldInPackage(Package);

					if (World)
					{
						FlagsToCook = RF_NoFlags;
					}
				}

				if (bPackageFullyLoaded)
				{
					UE_LOG(LogCook, Display, TEXT("Cooking %s -> %s"), *Package->GetName(), *PlatFilename);

					bool bSwap = (!Target->IsLittleEndian()) ^ (!PLATFORM_LITTLE_ENDIAN);


					if (!Target->HasEditorOnlyData())
					{
						Package->SetPackageFlags(PKG_FilterEditorOnly);
					}
					else
					{
						Package->ClearPackageFlags(PKG_FilterEditorOnly);
					}

					if (World)
					{
						// Fixup legacy lightmaps before saving
						// This should be done after loading, but Core loads UWorlds with LoadObject so there's no opportunity to handle this fixup on load
						World->PersistentLevel->HandleLegacyMapBuildData();
					}

					// need to subtract 32 because the SavePackage code creates temporary files with longer file names then the one we provide
					// projects may ignore this restriction if desired
					static bool bConsiderCompressedPackageFileLengthRequirements = ShouldConsiderCompressedPackageFileLengthRequirements();
					const int32 CompressedPackageFileLengthRequirement = bConsiderCompressedPackageFileLengthRequirements ? 32 : 0;
					const FString FullFilename = FPaths::ConvertRelativePathToFull(PlatFilename);
					if (FullFilename.Len() >= (PLATFORM_MAX_FILEPATH_LENGTH - CompressedPackageFileLengthRequirement))
					{
						LogCookerMessage(FString::Printf(TEXT("Couldn't save package, filename is too long: %s"), *PlatFilename), EMessageSeverity::Error);
						UE_LOG(LogCook, Error, TEXT("Couldn't save package, filename is too long :%s"), *PlatFilename);
						Result = ESavePackageResult::Error;
					}
					else
					{
						SCOPE_TIMER(GEditorSavePackage);
						GIsCookerLoadingPackage = true;
						Result = GEditor->Save(Package, World, FlagsToCook, *PlatFilename, GError, NULL, bSwap, false, SaveFlags, Target, FDateTime::MinValue(), false);
						GIsCookerLoadingPackage = false;
						{
							SCOPE_TIMER(ConvertingBlueprints);
							IBlueprintNativeCodeGenModule::Get().Convert(Package, Result.Result, *(Target->PlatformName()));
						}
						INC_INT_STAT(SavedPackage, 1);

						// If package was actually saved check with asset manager to make sure it wasn't excluded for being a development or never cook package. We do this after Editor Only filtering
						if (Result == ESavePackageResult::Success && UAssetManager::IsValid())
						{
							if (!UAssetManager::Get().VerifyCanCookPackage(Package->GetFName()))
							{
								Result = ESavePackageResult::Error;
							}
						}
					}
				}
				else
				{
					LogCookerMessage(FString::Printf(TEXT("Unable to cook package for platform because it is unable to be loaded: %s"), *PlatFilename), EMessageSeverity::Error);
					UE_LOG(LogCook, Display, TEXT("Unable to cook package for platform because it is unable to be loaded %s -> %s"), *Package->GetName(), *PlatFilename);
					Result = ESavePackageResult::Error;
				}
			}
		}

		Package->SetPackageFlagsTo(OriginalPackageFlags);
	}

	check(bIsSavingPackage == true);
	bIsSavingPackage = false;

}


void UCookOnTheFlyServer::Initialize( ECookMode::Type DesiredCookMode, ECookInitializationFlags InCookFlags, const FString &InOutputDirectoryOverride )
{
	OutputDirectoryOverride = InOutputDirectoryOverride;
	CurrentCookMode = DesiredCookMode;
	CookFlags = InCookFlags;

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddUObject(this, &UCookOnTheFlyServer::PreGarbageCollect);

	if (IsCookByTheBookMode() && !IsCookingInEditor())
	{
		FCoreUObjectDelegates::PackageCreatedForLoad.AddUObject(this, &UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded);
	}

	if (IsCookingInEditor())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UCookOnTheFlyServer::OnObjectPropertyChanged);
		FCoreUObjectDelegates::OnObjectModified.AddUObject(this, &UCookOnTheFlyServer::OnObjectModified);
		FCoreUObjectDelegates::OnObjectSaved.AddUObject(this, &UCookOnTheFlyServer::OnObjectSaved);

		FCoreDelegates::OnTargetPlatformChangedSupportedFormats.AddUObject(this, &UCookOnTheFlyServer::OnTargetPlatformChangedSupportedFormats);
	}

	FCoreDelegates::OnFConfigCreated.AddUObject(this, &UCookOnTheFlyServer::OnFConfigCreated);
	FCoreDelegates::OnFConfigDeleted.AddUObject(this, &UCookOnTheFlyServer::OnFConfigDeleted);

	bool bUseFullGCAssetClassNames = true;
	GConfig->GetBool(TEXT("CookSettings"), TEXT("bUseFullGCAssetClassNames"), bUseFullGCAssetClassNames, GEditorIni);
	
	MaxPrecacheShaderJobs = FPlatformMisc::NumberOfCores() - 1; // number of cores -1 is a good default allows the editor to still be responsive to other shader requests and allows cooker to take advantage of multiple processors while the editor is running
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxPrecacheShaderJobs"), MaxPrecacheShaderJobs, GEditorIni);

	MaxConcurrentShaderJobs = FPlatformMisc::NumberOfCores() * 4; // number of cores -1 is a good default allows the editor to still be responsive to other shader requests and allows cooker to take advantage of multiple processors while the editor is running
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxConcurrentShaderJobs"), MaxConcurrentShaderJobs, GEditorIni);

	if (bUseFullGCAssetClassNames)
	{
		TArray<FString> FullGCAssetClassNames;
		GConfig->GetArray( TEXT("CookSettings"), TEXT("FullGCAssetClassNames"), FullGCAssetClassNames, GEditorIni );
		for ( const FString& FullGCAssetClassName : FullGCAssetClassNames )
		{
			UClass* FullGCAssetClass = FindObject<UClass>(ANY_PACKAGE, *FullGCAssetClassName, true);
			if( FullGCAssetClass == NULL )
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find full gc asset class name %s may result in bad cook"), *FullGCAssetClassName);
			}
			else
			{
				FullGCAssetClasses.Add( FullGCAssetClass );
			}
		}
		if ( FullGCAssetClasses.Num() == 0 )
		{
			// default to UWorld
			FullGCAssetClasses.Add( UWorld::StaticClass() );
		}
	}


	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	TArray<FString> PresaveTargetPlatformNames;
	if ( GConfig->GetArray(TEXT("CookSettings"), TEXT("PresaveTargetPlatforms"), PresaveTargetPlatformNames, GEditorIni ) )
	{
		for ( const auto& PresaveTargetPlatformName : PresaveTargetPlatformNames )
		{
			ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(PresaveTargetPlatformName);
			if (TargetPlatform)
			{
				PresaveTargetPlatforms.Add(TargetPlatform);
			}
		}
	}

	PackagesPerGC = 500;
	int32 ConfigPackagesPerGC = 0;
	if (GConfig->GetInt( TEXT("CookSettings"), TEXT("PackagesPerGC"), ConfigPackagesPerGC, GEditorIni ))
	{
		// Going unsigned. Make negative values 0
		PackagesPerGC = ConfigPackagesPerGC > 0 ? ConfigPackagesPerGC : 0;
	}

	IdleTimeToGC = 20.0;
	GConfig->GetDouble( TEXT("CookSettings"), TEXT("IdleTimeToGC"), IdleTimeToGC, GEditorIni );

	int32 MaxMemoryAllowanceInMB = 8 * 1024;
	GConfig->GetInt( TEXT("CookSettings"), TEXT("MaxMemoryAllowance"), MaxMemoryAllowanceInMB, GEditorIni );
	MaxMemoryAllowanceInMB = FMath::Max(MaxMemoryAllowanceInMB, 0);
	MaxMemoryAllowance = MaxMemoryAllowanceInMB * 1024LL * 1024LL;
	
	int32 MinMemoryBeforeGCInMB = 0; // 6 * 1024;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinMemoryBeforeGC"), MinMemoryBeforeGCInMB, GEditorIni);
	MinMemoryBeforeGCInMB = FMath::Max(MinMemoryBeforeGCInMB, 0);
	MinMemoryBeforeGC = MinMemoryBeforeGCInMB * 1024LL * 1024LL;
	MinMemoryBeforeGC = FMath::Min(MaxMemoryAllowance, MinMemoryBeforeGC);

	int32 MinFreeMemoryInMB = 0;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinFreeMemory"), MinFreeMemoryInMB, GEditorIni);
	MinFreeMemoryInMB = FMath::Max(MinFreeMemoryInMB, 0);
	MinFreeMemory = MinFreeMemoryInMB * 1024LL * 1024LL;

	// check the amount of OS memory and use that number minus the reserved memory nubmer
	int32 MinReservedMemoryInMB = 0;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MinReservedMemory"), MinReservedMemoryInMB, GEditorIni);
	MinReservedMemoryInMB = FMath::Max(MinReservedMemoryInMB, 0);
	int64 MinReservedMemory = MinReservedMemoryInMB * 1024LL * 1024LL;
	if ( MinReservedMemory )
	{
		int64 TotalRam = FPlatformMemory::GetPhysicalGBRam() * 1024LL * 1024LL * 1024LL;
		MaxMemoryAllowance = FMath::Min<int64>( MaxMemoryAllowance, TotalRam - MinReservedMemory );
	}

	MaxNumPackagesBeforePartialGC = 400;
	GConfig->GetInt(TEXT("CookSettings"), TEXT("MaxNumPackagesBeforePartialGC"), MaxNumPackagesBeforePartialGC, GEditorIni);
	
	GConfig->GetArray(TEXT("CookSettings"), TEXT("ConfigSettingBlacklist"), ConfigSettingBlacklist, GEditorIni);

	UE_LOG(LogCook, Display, TEXT("Max memory allowance for cook %dmb min free memory %dmb"), MaxMemoryAllowanceInMB, MinFreeMemoryInMB);


	{
		const FConfigSection* CacheSettings = GConfig->GetSectionPrivate(TEXT("CookPlatformDataCacheSettings"), false, true, GEditorIni);
		if ( CacheSettings )
		{
			for ( const auto& CacheSetting : *CacheSettings )
			{
				
				const FString& ReadString = CacheSetting.Value.GetValue();
				int32 ReadValue = FCString::Atoi(*ReadString);
				int32 Count = FMath::Max( 2,  ReadValue );
				MaxAsyncCacheForType.Add( CacheSetting.Key,  Count );
			}
		}
		CurrentAsyncCacheForType = MaxAsyncCacheForType;
	}


	if (IsCookByTheBookMode())
	{
		CookByTheBookOptions = new FCookByTheBookOptions();
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if ((*It) != GetTransientPackage())
			{
				CookByTheBookOptions->StartupPackages.Add(It->GetFName());
				UE_LOG(LogCook, Verbose, TEXT("Cooker startup package %s"), *It->GetName());
			}
		}
	}
	
	UE_LOG(LogCook, Display, TEXT("Mobile HDR setting %d"), IsMobileHDR());

	// See if there are any plugins that need to be remapped for the sandbox
	const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
	if (Project != nullptr)
	{
		PluginsToRemap = IPluginManager::Get().GetEnabledPlugins();
		TArray<FString> AdditionalPluginDirs = Project->GetAdditionalPluginDirectories();
		// Remove any plugin that is not in the additional directories since they are handled normally
		for (int32 Index = PluginsToRemap.Num() - 1; Index >= 0; Index--)
		{
			bool bRemove = true;
			for (const FString& PluginDir : AdditionalPluginDirs)
			{
				// If this plugin is in a directory that needs remapping
				if (PluginsToRemap[Index]->GetBaseDir().StartsWith(PluginDir))
				{
					bRemove = false;
					break;
				}
			}
			if (bRemove)
			{
				PluginsToRemap.RemoveAt(Index);
			}
		}
	}
}

bool UCookOnTheFlyServer::Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("package")))
	{
		FString PackageName;
		if (!FParse::Value(Cmd, TEXT("name="), PackageName))
		{
			Ar.Logf(TEXT("Required package name for cook package function. \"cook package name=<name> platform=<platform>\""));
			return true;
		}

		FString PlatformName;
		if (!FParse::Value(Cmd, TEXT("platform="), PlatformName))
		{
			Ar.Logf(TEXT("Required package name for cook package function. \"cook package name=<name> platform=<platform>\""));
			return true;
		}

		if (FPackageName::IsShortPackageName(PackageName))
		{
			FString OutFilename;
			if (FPackageName::SearchForPackageOnDisk(PackageName, NULL, &OutFilename))
			{
				PackageName = OutFilename;
			}
		}

		FName RawPackageName(*PackageName);
		TArray<FName> PackageNames;
		PackageNames.Add(RawPackageName);

		GenerateLongPackageNames(PackageNames);
		

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(PlatformName);
		if (TargetPlatform == nullptr)
		{
			Ar.Logf(TEXT("Target platform %s wasn't found."), *PlatformName);
			return true;
		}

		FCookByTheBookStartupOptions StartupOptions;

		StartupOptions.TargetPlatforms.Add(TargetPlatform);
		for (const FName& StandardPackageName : PackageNames)
		{
			FName PackageFileFName = GetCachedStandardPackageFileFName(StandardPackageName);
			StartupOptions.CookMaps.Add(StandardPackageName.ToString());
		}
		StartupOptions.CookOptions = ECookByTheBookOptions::NoAlwaysCookMaps | ECookByTheBookOptions::NoDefaultMaps | ECookByTheBookOptions::NoGameAlwaysCookPackages | ECookByTheBookOptions::NoInputPackages | ECookByTheBookOptions::NoSlatePackages | ECookByTheBookOptions::DisableUnsolicitedPackages | ECookByTheBookOptions::ForceDisableSaveGlobalShaders;
		
		StartCookByTheBook(StartupOptions);
	}
	else if (FParse::Command(&Cmd, TEXT("clearall")))
	{
		StopAndClearCookedData();
	}
	else if (FParse::Command(&Cmd, TEXT("stats")))
	{
		DumpStats();
	}

	return false;
}

void UCookOnTheFlyServer::DumpStats()
{
	OUTPUT_TIMERS();
	OUTPUT_HIERARCHYTIMERS();
#if PROFILE_NETWORK
	UE_LOG(LogCook, Display, TEXT("Network Stats \n"
		"TimeTillRequestStarted %f\n"
		"TimeTillRequestForfilled %f\n"
		"TimeTillRequestForfilledError %f\n"
		"WaitForAsyncFilesWrites %f\n"),
		TimeTillRequestStarted,
		TimeTillRequestForfilled,
		TimeTillRequestForfilledError,

		WaitForAsyncFilesWrites);
#endif
}

uint32 UCookOnTheFlyServer::NumConnections() const
{
	int Result= 0;
	for ( int i = 0; i < NetworkFileServers.Num(); ++i )
	{
		INetworkFileServer *NetworkFileServer = NetworkFileServers[i];
		if ( NetworkFileServer )
		{
			Result += NetworkFileServer->NumConnections();
		}
	}
	return Result;
}

FString UCookOnTheFlyServer::GetOutputDirectoryOverride() const
{
	FString OutputDirectory = OutputDirectoryOverride;
	// Output directory override.	
	if (OutputDirectory.Len() <= 0)
	{
		if ( IsCookingDLC() )
		{
			check( IsCookByTheBookMode() );
			OutputDirectory = FPaths::Combine(*GetBaseDirectoryForDLC(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		else if ( IsCookingInEditor() )
		{
			// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
			OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("EditorCooked"), TEXT("[Platform]"));
		}
		else
		{
			// Full path so that the sandbox wrapper doesn't try to re-base it under Sandboxes
			OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		}
		
		OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	}
	else if (!OutputDirectory.Contains(TEXT("[Platform]"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) )
	{
		// Output directory needs to contain [Platform] token to be able to cook for multiple targets.
		if ( IsCookByTheBookMode() )
		{
			const TArray<ITargetPlatform*>& TargetPlatforms = GetCookingTargetPlatforms();

			// more then one target platform specified append "[platform]" to output directory so that multiple platforms can be cooked
			check( TargetPlatforms.Num() == 1 );
		}
		else
		{
			// cook on the fly we have to platform because we don't know which platforms we are cooking for up front
			OutputDirectory = FPaths::Combine(*OutputDirectory, TEXT("[Platform]"));
		}
	}
	FPaths::NormalizeDirectoryName(OutputDirectory);

	return OutputDirectory;
}

template<class T>
void GetVersionFormatNumbersForIniVersionStrings( TArray<FString>& IniVersionStrings, const FString& FormatName, const TArray<const T> &FormatArray )
{
	for ( const T& Format : FormatArray )
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for ( const FName& SupportedFormat : SupportedFormats )
		{
			int32 VersionNumber = Format->GetVersion(SupportedFormat);
			FString IniVersionString = FString::Printf( TEXT("%s:%s:VersionNumber%d"), *FormatName, *SupportedFormat.ToString(), VersionNumber);
			IniVersionStrings.Emplace( IniVersionString );
		}
	}
}




template<class T>
void GetVersionFormatNumbersForIniVersionStrings(TMap<FString, FString>& IniVersionMap, const FString& FormatName, const TArray<T> &FormatArray)
{
	for (const T& Format : FormatArray)
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for (const FName& SupportedFormat : SupportedFormats)
		{
			int32 VersionNumber = Format->GetVersion(SupportedFormat);
			FString IniVersionString = FString::Printf(TEXT("%s:%s:VersionNumber"), *FormatName, *SupportedFormat.ToString());
			IniVersionMap.Add(IniVersionString, FString::Printf(TEXT("%d"), VersionNumber));
		}
	}
}


void GetAdditionalCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, TMap<FString, FString>& IniVersionMap )
{

	TArray<FString> VersionedRValues;
	GConfig->GetArray(TEXT("CookSettings"), TEXT("VersionedIntRValues"), VersionedRValues, GEditorIni);

	for (const FString& RValue : VersionedRValues)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(*RValue);
		if (CVar)
		{
			IniVersionMap.Add(*RValue, FString::Printf(TEXT("%d"), CVar->GetValueOnGameThread()));
		}
	}


	// these settings are covered by the general search through accessed ini settings 
	/*const UTextureLODSettings& LodSettings = TargetPlatform->GetTextureLODSettings();

	UEnum* TextureGroupEnum = FindObject<UEnum>(NULL, TEXT("Engine.TextureGroup"));
	UEnum* TextureMipGenSettingsEnum = FindObject<UEnum>(NULL, TEXT("Engine.TextureMipGenSettings"));

	for (int I = 0; I < TextureGroup::TEXTUREGROUP_MAX; ++I)
	{
		FString TextureGroupString = TextureGroupEnum->GetNameStringByValue(I);
		const TextureMipGenSettings& MipGenSettings = LodSettings.GetTextureMipGenSettings((TextureGroup)(I));
		FString MipGenVersionString = FString::Printf(TEXT("TextureLODGroupMipGenSettings:%s#%s"), *TextureGroupString, *TextureMipGenSettingsEnum->GetNameStringByIndex((int32)(MipGenSettings)));
		//IniVersionStrings.Emplace(MoveTemp(MipGenVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMipGenSettings:%s"), *TextureGroupString), *TextureMipGenSettingsEnum->GetNameStringByIndex((int32)(MipGenSettings)));

		const int32 MinMipCount = LodSettings.GetMinLODMipCount((TextureGroup)(I));
		// FString MinMipVersionString = FString::Printf(TEXT("TextureLODGroupMinMipCount:%s#%d"), *TextureGroupEnum->GetNameStringByIndex(I), MinMipCount);
		//IniVersionStrings.Emplace(MoveTemp(MinMipVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMinMipCount:%s"), *TextureGroupString), FString::Printf(TEXT("%d"), MinMipCount));

		const int32 MaxMipCount = LodSettings.GetMaxLODMipCount((TextureGroup)(I));
		// FString MaxMipVersionString = FString::Printf(TEXT("TextureLODGroupMaxMipCount:%s#%d"), *TextureGroupEnum->GetNameStringByIndex(I), MaxMipCount);
		//IniVersionStrings.Emplace(MoveTemp(MaxMipVersionString));
		IniVersionMap.Add(FString::Printf(TEXT("TextureLODGroupMaxMipCount:%s"), *TextureGroupString), FString::Printf(TEXT("%d"), MaxMipCount));
	}*/

	// save off the ddc version numbers also
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);

	{
		TArray<FName> AllWaveFormatNames;
		TargetPlatform->GetAllWaveFormats(AllWaveFormatNames);
		TArray<const IAudioFormat*> SupportedWaveFormats;
		for ( const auto& WaveName : AllWaveFormatNames )
		{
			const IAudioFormat* AudioFormat = TPM->FindAudioFormat(WaveName);
			if (AudioFormat)
			{
				SupportedWaveFormats.Add(AudioFormat);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find audio format \"%s\" which is required by \"%s\""), *WaveName.ToString(), *TargetPlatform->PlatformName());
			}
			
		}
		GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("AudioFormat"), SupportedWaveFormats);
	}

	{
		TArray<FName> AllTextureFormats;
		TargetPlatform->GetAllTextureFormats(AllTextureFormats);
		TArray<const ITextureFormat*> SupportedTextureFormats;
		for (const auto& TextureName : AllTextureFormats)
		{
			const ITextureFormat* TextureFormat = TPM->FindTextureFormat(TextureName);
			if ( TextureFormat )
			{
				SupportedTextureFormats.Add(TextureFormat);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find texture format \"%s\" which is required by \"%s\""), *TextureName.ToString(), *TargetPlatform->PlatformName());
			}
		}
		GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("TextureFormat"), SupportedTextureFormats);
	}

	{
		TArray<FName> AllFormatNames;
		TargetPlatform->GetAllTargetedShaderFormats(AllFormatNames);
		TArray<const IShaderFormat*> SupportedFormats;
		for (const auto& FormatName : AllFormatNames)
		{
			const IShaderFormat* Format = TPM->FindShaderFormat(FormatName);
			if ( Format )
			{
				SupportedFormats.Add(Format);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Unable to find shader \"%s\" which is required by format \"%s\""), *FormatName.ToString(), *TargetPlatform->PlatformName());
			}
		}
		GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("ShaderFormat"), SupportedFormats);
	}


	// TODO: Add support for physx version tracking, currently this happens so infrequently that invalidating a cook based on it is not essential
	//GetVersionFormatNumbersForIniVersionStrings(IniVersionMap, TEXT("PhysXCooking"), TPM->GetPhysXCooking());


	if ( FParse::Param( FCommandLine::Get(), TEXT("fastcook") ) )
	{
		IniVersionMap.Add(TEXT("fastcook"));
	}


	static const FCustomVersionContainer& CustomVersionContainer = FCustomVersionContainer::GetRegistered();
	for (const auto& CustomVersion : CustomVersionContainer.GetAllVersions())
	{
		FString CustomVersionString = FString::Printf(TEXT("%s:%s"), *CustomVersion.GetFriendlyName().ToString(), *CustomVersion.Key.ToString());
		FString CustomVersionValue = FString::Printf(TEXT("%d"), CustomVersion.Version);
		IniVersionMap.Add(CustomVersionString, CustomVersionValue);
	}

	FString UE4Ver = FString::Printf(TEXT("PackageFileVersions:%d"), GPackageFileUE4Version);
	FString UE4Value = FString::Printf(TEXT("%d"), GPackageFileLicenseeUE4Version);
	IniVersionMap.Add(UE4Ver, UE4Value);

	/*FString UE4EngineVersionCompatibleName = TEXT("EngineVersionCompatibleWith");
	FString UE4EngineVersionCompatible = FEngineVersion::CompatibleWith().ToString();
	
	if ( UE4EngineVersionCompatible.Len() )
	{
		IniVersionMap.Add(UE4EngineVersionCompatibleName, UE4EngineVersionCompatible);
	}*/

	IniVersionMap.Add(TEXT("MaterialShaderMapDDCVersion"), *GetMaterialShaderMapDDCKey());
	IniVersionMap.Add(TEXT("GlobalDDCVersion"), *GetGlobalShaderMapDDCKey());
}



bool UCookOnTheFlyServer::GetCurrentIniVersionStrings( const ITargetPlatform* TargetPlatform, FIniSettingContainer& IniVersionStrings ) const
{
	IniVersionStrings = AccessedIniStrings;

	// this should be called after the cook is finished
	TArray<FString> IniFiles;
	GConfig->GetConfigFilenames(IniFiles);

	TMap<FString, int32> MultiMapCounter;

	for ( const FString& ConfigFilename : IniFiles )
	{
		if ( ConfigFilename.Contains(TEXT("CookedIniVersion.txt")) )
		{
			continue;
		}

		const FConfigFile *ConfigFile = GConfig->FindConfigFile(ConfigFilename);
		ProcessAccessedIniSettings(ConfigFile, IniVersionStrings);
		
	}

	for (const FConfigFile* ConfigFile : OpenConfigFiles)
	{
		ProcessAccessedIniSettings(ConfigFile, IniVersionStrings);
	}


	// remove any which are filtered out
	for ( const FString& Filter : ConfigSettingBlacklist )
	{
		TArray<FString> FilterArray;
		Filter.ParseIntoArray( FilterArray, TEXT(":"));

		FString *ConfigFileName = nullptr;
		FString *SectionName = nullptr;
		FString *ValueName = nullptr;
		switch ( FilterArray.Num() )
		{
		case 3:
			ValueName = &FilterArray[2];
		case 2:
			SectionName = &FilterArray[1];
		case 1:
			ConfigFileName = &FilterArray[0];
			break;
		default:
			continue;
		}

		if ( ConfigFileName )
		{
			for ( auto ConfigFile = IniVersionStrings.CreateIterator(); ConfigFile; ++ConfigFile )
			{
				if ( ConfigFile.Key().ToString().MatchesWildcard(*ConfigFileName) )
				{
					if ( SectionName )
					{
						for ( auto Section = ConfigFile.Value().CreateIterator(); Section; ++Section )
						{
							if ( Section.Key().ToString().MatchesWildcard(*SectionName))
							{
								if (ValueName)
								{
									for ( auto Value = Section.Value().CreateIterator(); Value; ++Value )
									{
										if ( Value.Key().ToString().MatchesWildcard(*ValueName))
										{
											Value.RemoveCurrent();
										}
									}
								}
								else
								{
									Section.RemoveCurrent();
								}
							}
						}
					}
					else
					{
						ConfigFile.RemoveCurrent();
					}
				}
			}
		}
	}
	return true;
}


bool UCookOnTheFlyServer::GetCookedIniVersionStrings(const ITargetPlatform* TargetPlatform, FIniSettingContainer& OutIniSettings, TMap<FString,FString>& OutAdditionalSettings) const
{
	const FString EditorIni = FPaths::ProjectDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxEditorIni = ConvertToFullSandboxPath(*EditorIni, true);


	const FString PlatformSandboxEditorIni = SandboxEditorIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());

	TArray<FString> SavedIniVersionedParams;

	FConfigFile ConfigFile;
	ConfigFile.Read(*PlatformSandboxEditorIni);

	

	const static FName NAME_UsedSettings(TEXT("UsedSettings")); 
	const FConfigSection* UsedSettings = ConfigFile.Find(NAME_UsedSettings.ToString());
	if (UsedSettings == nullptr)
	{
		return false;
	}


	const static FName NAME_AdditionalSettings(TEXT("AdditionalSettings"));
	const FConfigSection* AdditionalSettings = ConfigFile.Find(NAME_AdditionalSettings.ToString());
	if (AdditionalSettings == nullptr)
	{
		return false;
	}


	for (const auto& UsedSetting : *UsedSettings )
	{
		FName Key = UsedSetting.Key;
		const FConfigValue& UsedValue = UsedSetting.Value;

		TArray<FString> SplitString;
		Key.ToString().ParseIntoArray(SplitString, TEXT(":"));

		if (SplitString.Num() != 4)
		{
			UE_LOG(LogCook, Warning, TEXT("Found unparsable ini setting %s for platform %s, invalidating cook."), *Key.ToString(), *TargetPlatform->PlatformName());
			return false;
		}


		check(SplitString.Num() == 4); // We generate this ini file in SaveCurrentIniSettings
		const FString& Filename = SplitString[0];
		const FString& SectionName = SplitString[1];
		const FString& ValueName = SplitString[2];
		const int32 ValueIndex = FCString::Atoi(*SplitString[3]);

		auto& OutFile = OutIniSettings.FindOrAdd(FName(*Filename));
		auto& OutSection = OutFile.FindOrAdd(FName(*SectionName));
		auto& ValueArray = OutSection.FindOrAdd(FName(*ValueName));
		if ( ValueArray.Num() < (ValueIndex+1) )
		{
			ValueArray.AddZeroed( ValueIndex - ValueArray.Num() +1 );
		}
		ValueArray[ValueIndex] = UsedValue.GetSavedValue();
	}



	for (const auto& AdditionalSetting : *AdditionalSettings)
	{
		const FName& Key = AdditionalSetting.Key;
		const FString& Value = AdditionalSetting.Value.GetSavedValue();
		OutAdditionalSettings.Add(Key.ToString(), Value);
	}

	return true;
}



void UCookOnTheFlyServer::OnFConfigCreated(const FConfigFile* Config)
{
	if (IniSettingRecurse)
	{
		return;
	}

	OpenConfigFiles.Add(Config);
}

void UCookOnTheFlyServer::OnFConfigDeleted(const FConfigFile* Config)
{
	if (IniSettingRecurse)
	{
		return;
	}

	ProcessAccessedIniSettings(Config, AccessedIniStrings);

	OpenConfigFiles.Remove(Config);
}


void UCookOnTheFlyServer::ProcessAccessedIniSettings(const FConfigFile* Config, FIniSettingContainer& OutAccessedIniStrings) const
{	
	if (Config->Name == NAME_None)
	{
		return;
	}
	// try figure out if this config file is for a specific platform 
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& Platforms = TPM.GetTargetPlatforms();
	FString PlatformName;
	bool bFoundPlatformName = false;
	for (ITargetPlatform* Platform : Platforms )
	{
		FString CurrentPlatformName = Platform->IniPlatformName();
		for ( const auto& SourceIni : Config->SourceIniHierarchy )
		{
			if ( SourceIni.Value.Filename.Contains(CurrentPlatformName) )
			{
				PlatformName = CurrentPlatformName;
				bFoundPlatformName = true;
				break;
			}
		}
		if ( bFoundPlatformName )
		{
			break;
		}
	}

	


	FString ConfigName = bFoundPlatformName ? FString::Printf(TEXT("%s.%s"),*PlatformName, *Config->Name.ToString()) : Config->Name.ToString();
	const FName& ConfigFName = FName(*ConfigName);
	
	for ( auto& ConfigSection : *Config )
	{
		TSet<FName> ProcessedValues; 
		const FName SectionName = FName(*ConfigSection.Key);

		if ( SectionName.GetPlainNameString().Contains(TEXT(":")) )
		{
			UE_LOG(LogCook, Verbose, TEXT("Ignoring ini section checking for section name %s because it contains ':'"), *SectionName.ToString() );
			continue;
		}

		for ( auto& ConfigValue : ConfigSection.Value )
		{
			const FName& ValueName = ConfigValue.Key;
			if ( ProcessedValues.Contains(ValueName) )
				continue;

			ProcessedValues.Add(ValueName);

			if (ValueName.GetPlainNameString().Contains(TEXT(":")))
			{
				UE_LOG(LogCook, Verbose, TEXT("Ignoring ini section checking for section name %s because it contains ':'"), *ValueName.ToString());
				continue;
			}

			
			TArray<FConfigValue> ValueArray;
			ConfigSection.Value.MultiFind( ValueName, ValueArray, true );

			bool bHasBeenAccessed = false;
			for (const auto& ValueArrayEntry : ValueArray)
			{
				if (ValueArrayEntry.HasBeenRead())
				{
					bHasBeenAccessed = true;
					break;
				}
			}

			if ( bHasBeenAccessed )
			{
				auto& AccessedConfig = OutAccessedIniStrings.FindOrAdd(ConfigFName);
				auto& AccessedSection = AccessedConfig.FindOrAdd(SectionName);
				auto& AccessedKey = AccessedSection.FindOrAdd(ValueName);
				AccessedKey.Empty();
				for ( const auto& ValueArrayEntry : ValueArray )
				{
					FString RemovedColon = ValueArrayEntry.GetSavedValue().Replace(TEXT(":"), TEXT(""));
					AccessedKey.Add(RemovedColon);
				}
			}
			
		}
	}
}




bool UCookOnTheFlyServer::IniSettingsOutOfDate(const ITargetPlatform* TargetPlatform) const
{
	FScopeAssign<bool> A = FScopeAssign<bool>(IniSettingRecurse, true);

	FIniSettingContainer OldIniSettings;
	TMap<FString, FString> OldAdditionalSettings;
	if ( GetCookedIniVersionStrings(TargetPlatform, OldIniSettings, OldAdditionalSettings) == false)
	{
		UE_LOG(LogCook, Display, TEXT("Unable to read previous cook inisettings for platform %s invalidating cook"), *TargetPlatform->PlatformName());
		return true;
	}

	// compare against current settings
	TMap<FString, FString> CurrentAdditionalSettings;
	GetAdditionalCurrentIniVersionStrings(TargetPlatform, CurrentAdditionalSettings);

	for ( const auto& OldIniSetting : OldAdditionalSettings)
	{
		const FString* CurrentValue = CurrentAdditionalSettings.Find(OldIniSetting.Key);
		if ( !CurrentValue )
		{
			UE_LOG(LogCook, Display, TEXT("Previous cook had additional ini setting: %s current cook is missing this setting."), *OldIniSetting.Key);
			return true;
		}

		if ( *CurrentValue != OldIniSetting.Value )
		{
			UE_LOG(LogCook, Display, TEXT("Additional Setting from previous cook %s doesn't match %s %s"), *OldIniSetting.Key, **CurrentValue, *OldIniSetting.Value );
			return true;
		}
	}

	for ( const auto& OldIniFile : OldIniSettings )
	{
		const FName& ConfigNameKey = OldIniFile.Key;

		TArray<FString> ConfigNameArray;
		ConfigNameKey.ToString().ParseIntoArray(ConfigNameArray, TEXT("."));
		FString Filename;
		FString PlatformName;
		bool bFoundPlatformName = false;
		if ( ConfigNameArray.Num() <= 1 )
		{
			Filename = ConfigNameKey.ToString();
		}
		else if ( ConfigNameArray.Num() == 2 )
		{
			PlatformName = ConfigNameArray[0];
			Filename = ConfigNameArray[1];
			bFoundPlatformName = true;
		}
		else
		{
			UE_LOG( LogCook, Warning, TEXT("Found invalid file name in old ini settings file Filename %s settings file %s"), *ConfigNameKey.ToString(), *TargetPlatform->PlatformName() );
			return true;
		}
		
		const FConfigFile* ConfigFile = nullptr;
		FConfigFile Temp;
		if ( bFoundPlatformName)
		{
			GConfig->LoadLocalIniFile(Temp, *Filename, true, *PlatformName );
			ConfigFile = &Temp;
		}
		else
		{
			ConfigFile = GConfig->Find(Filename, false);
		}
		FName FileFName = FName(*Filename);
		if ( !ConfigFile )
		{
			for( const auto& File : *GConfig )
			{
				if (File.Value.Name == FileFName)
				{
					ConfigFile = &File.Value;
					break;
				}
			}
			if ( !ConfigFile )
			{
				UE_LOG(LogCook, Display, TEXT("Unable to find config file %s invalidating inisettings"), *FString::Printf(TEXT("%s %s"), *PlatformName, *Filename));
				return true;
			}
		}
		for ( const auto& OldIniSection : OldIniFile.Value )
		{
			
			const FName& SectionName = OldIniSection.Key;
			const FConfigSection* IniSection = ConfigFile->Find( SectionName.ToString() );

			const FString BlackListSetting = *FString::Printf(TEXT("%s.%s:%s"), *PlatformName, *Filename, *SectionName.ToString());

			if ( IniSection == nullptr )
			{
				UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, Current section doesn't exist"), *FString::Printf(TEXT("%s %s %s"), *PlatformName, *Filename, *SectionName.ToString()));
				UE_LOG(LogCook, Display, TEXT("To avoid this add blacklist setting to DefaultEditor.ini [CookSettings] %s"), *BlackListSetting);
				return true;
			}

			for ( const auto& OldIniValue : OldIniSection.Value )
			{
				const FName& ValueName = OldIniValue.Key;

				TArray<FConfigValue> CurrentValues;
				IniSection->MultiFind( ValueName, CurrentValues, true );

				if ( CurrentValues.Num() != OldIniValue.Value.Num() )
				{
					UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, missmatched num array elements %d != %d "), *FString::Printf(TEXT("%s %s %s %s"), *PlatformName, *Filename, *SectionName.ToString(), *ValueName.ToString()), CurrentValues.Num(), OldIniValue.Value.Num());
					UE_LOG(LogCook, Display, TEXT("To avoid this add blacklist setting to DefaultEditor.ini [CookSettings] %s"), *BlackListSetting);
					return true;
				}
				for ( int Index = 0; Index < CurrentValues.Num(); ++Index )
				{
					const FString FilteredCurrentValue = CurrentValues[Index].GetSavedValue().Replace(TEXT(":"), TEXT(""));
					if ( FilteredCurrentValue != OldIniValue.Value[Index] )
					{
						UE_LOG(LogCook, Display, TEXT("Inisetting is different for %s, value %s != %s invalidating cook"),  *FString::Printf(TEXT("%s %s %s %s %d"),*PlatformName, *Filename, *SectionName.ToString(), *ValueName.ToString(), Index), *CurrentValues[Index].GetSavedValue(), *OldIniValue.Value[Index] );
						UE_LOG(LogCook, Display, TEXT("To avoid this add blacklist setting to DefaultEditor.ini [CookSettings] %s"), *BlackListSetting);
						return true;
					}
				}
				
			}
		}
	}

	return false;
}

bool UCookOnTheFlyServer::SaveCurrentIniSettings(const ITargetPlatform* TargetPlatform) const
{

	auto S = FScopeAssign<bool>(IniSettingRecurse, true);
	check(IsChildCooker() == false);

	TMap<FString, FString> AdditionalIniSettings;
	GetAdditionalCurrentIniVersionStrings(TargetPlatform, AdditionalIniSettings);

	FIniSettingContainer CurrentIniSettings;
	GetCurrentIniVersionStrings(TargetPlatform, CurrentIniSettings);

	const FString EditorIni = FPaths::ProjectDir() / TEXT("CookedIniVersion.txt");
	const FString SandboxEditorIni = ConvertToFullSandboxPath(*EditorIni, true);


	const FString PlatformSandboxEditorIni = SandboxEditorIni.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());


	FConfigFile ConfigFile;
	// ConfigFile.Read(*PlatformSandboxEditorIni);

	ConfigFile.Dirty = true;
	const static FName NAME_UsedSettings(TEXT("UsedSettings"));
	ConfigFile.Remove(NAME_UsedSettings.ToString());
	FConfigSection& UsedSettings = ConfigFile.FindOrAdd(NAME_UsedSettings.ToString());


	{
		SCOPE_TIMER(ProcessingAccessedStrings)
		for (const auto& CurrentIniFilename : CurrentIniSettings)
		{
			const FName& Filename = CurrentIniFilename.Key;
			for ( const auto& CurrentSection : CurrentIniFilename.Value )
			{
				const FName& Section = CurrentSection.Key;
				for ( const auto& CurrentValue : CurrentSection.Value )
				{
					const FName& ValueName = CurrentValue.Key;
					const TArray<FString>& Values = CurrentValue.Value;

					for ( int Index = 0; Index < Values.Num(); ++Index )
					{
						FString NewKey = FString::Printf(TEXT("%s:%s:%s:%d"), *Filename.ToString(), *Section.ToString(), *ValueName.ToString(), Index);
						UsedSettings.Add(FName(*NewKey), Values[Index]);
					}
				}
			}
		}
	}


	const static FName NAME_AdditionalSettings(TEXT("AdditionalSettings"));
	ConfigFile.Remove(NAME_AdditionalSettings.ToString());
	FConfigSection& AdditionalSettings = ConfigFile.FindOrAdd(NAME_AdditionalSettings.ToString());

	for (const auto& AdditionalIniSetting : AdditionalIniSettings)
	{
		AdditionalSettings.Add( FName(*AdditionalIniSetting.Key), AdditionalIniSetting.Value );
	}

	ConfigFile.Write(PlatformSandboxEditorIni);


	return true;

}

FString UCookOnTheFlyServer::ConvertCookedPathToUncookedPath(const FString& CookedRelativeFilename) const 
{
	// Check for remapped plugins' cooked content
	if (PluginsToRemap.Num() > 0 && CookedRelativeFilename.Contains(REMAPPED_PLUGGINS))
	{
		int32 RemappedIndex = CookedRelativeFilename.Find(REMAPPED_PLUGGINS);
		check(RemappedIndex >= 0);
		static uint32 RemappedPluginStrLen = FCString::Strlen(REMAPPED_PLUGGINS);
		// Snip everything up through the RemappedPlugins/ off so we can find the plugin it corresponds to
		FString PluginPath = CookedRelativeFilename.RightChop(RemappedIndex + RemappedPluginStrLen + 1);
		FString FullUncookedPath;
		// Find the plugin that owns this content
		for (TSharedRef<IPlugin> Plugin : PluginsToRemap)
		{
			if (PluginPath.StartsWith(Plugin->GetName()))
			{
				FullUncookedPath = Plugin->GetContentDir();
				static uint32 ContentStrLen = FCString::Strlen(TEXT("Content/"));
				// Chop off the pluginName/Content since it's part of the full path
				FullUncookedPath /= PluginPath.RightChop(Plugin->GetName().Len() + ContentStrLen);
				break;
			}
		}

		if (FullUncookedPath.Len() > 0)
		{
			return FullUncookedPath;
		}
		// Otherwise fall through to sandbox handling
	}

	const FString CookedFilename = FPaths::ConvertRelativePathToFull(CookedRelativeFilename);


	FString SandboxDirectory = SandboxFile->GetSandboxDirectory();

	SandboxDirectory.ReplaceInline(TEXT("[PLATFORM]"), TEXT(""));
	SandboxDirectory.ReplaceInline(TEXT("//"), TEXT("/"));

	FString CookedFilenameNoSandbox = CookedFilename;
	CookedFilenameNoSandbox.RemoveFromStart(SandboxDirectory);
	int32 EndOfPlatformIndex = 0;

	// assume at this point the cook platform is the next thing on the path
	FString CookedFilenameNoPlatform = CookedFilename;

	if (CookedFilenameNoSandbox.FindChar(TEXT('/'), EndOfPlatformIndex))
	{
		CookedFilenameNoPlatform = SandboxFile->GetSandboxDirectory() / CookedFilenameNoSandbox.Mid(EndOfPlatformIndex);
		CookedFilenameNoPlatform.ReplaceInline(TEXT("//"), TEXT("/"));
	}
	
	// after removing the cooked platform we can use the sandbox file to convert back to a uncooked path
	FString FullUncookedPath = SandboxFile->ConvertFromSandboxPath(*CookedFilenameNoPlatform);

	// make the result a standard filename (relative)
	FPaths::MakeStandardFilename(FullUncookedPath);
	return FullUncookedPath;


}

void UCookOnTheFlyServer::GetAllCookedFiles(TMap<FName, FName>& UncookedPathToCookedPath, const FString& SandboxPath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> CookedFiles;
	FPackageSearchVisitor PackageSearch(CookedFiles);
	PlatformFile.IterateDirectoryRecursively(*SandboxPath, PackageSearch);
	for (const FString& CookedFile : CookedFiles)
	{
		const FName CookedFName(*CookedFile);

		const FString CookedFullPath = FPaths::ConvertRelativePathToFull(CookedFile);
		const FString UncookedFilename = ConvertCookedPathToUncookedPath(CookedFullPath);

		const FName UncookedFName(*UncookedFilename);
		UncookedPathToCookedPath.Add(UncookedFName, CookedFName);
	}
}

void UCookOnTheFlyServer::PopulateCookedPackagesFromDisk(const TArray<ITargetPlatform*>& Platforms)
{
	check(IsChildCooker() == false);

	// See what files are out of date in the sandbox folder
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		TArray<FString> CookedPackagesToDelete;

		ITargetPlatform* Target = Platforms[Index];
		FString SandboxPath = GetSandboxDirectory(Target->PlatformName());
		FName PlatformFName(*Target->PlatformName());

		FString EngineSandboxPath = SandboxFile->ConvertToSandboxPath(*FPaths::EngineDir()) + TEXT("/");
		EngineSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString GameSandboxPath = SandboxFile->ConvertToSandboxPath(*(FPaths::ProjectDir() + TEXT("a.txt")));
		GameSandboxPath.ReplaceInline(TEXT("a.txt"), TEXT(""));
		GameSandboxPath.ReplaceInline(TEXT("[Platform]"), *Target->PlatformName());

		FString LocalGamePath = FPaths::ProjectDir();
		if (FPaths::IsProjectFilePathSet())
		{
			LocalGamePath = FPaths::GetPath(FPaths::GetProjectFilePath()) + TEXT("/");
		}

		FString LocalEnginePath = FPaths::EngineDir();

		static bool bFindCulprit = false; //debugging setting if we want to find culprit for iterative cook issues

		// Registry generator already exists
		FAssetRegistryGenerator* PlatformAssetRegistry = RegistryGenerators.FindRef(PlatformFName);

		// Load the platform cooked asset registry file
		const FString CookedAssetRegistry = FPaths::ProjectDir() / GetDevelopmentAssetRegistryFilename();
		const FString SandboxCookedAssetRegistryFilename = ConvertToFullSandboxPath(*CookedAssetRegistry, true, Target->PlatformName());

		bool bIsIterateSharedBuild = IsCookFlagSet(ECookInitializationFlags::IterateSharedBuild);

		if (bIsIterateSharedBuild)
		{
			// see if the shared build is newer then the current cooked content in the local directory
			FDateTime CurrentLocalCookedBuild = IFileManager::Get().GetTimeStamp(*SandboxCookedAssetRegistryFilename);

			// iterate on the shared build if the option is set
			FString SharedCookedAssetRegistry = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("SharedIterativeBuild"), *Target->PlatformName(), TEXT("Cooked"), GetDevelopmentAssetRegistryFilename());

			FDateTime CurrentIterativeCookedBuild = IFileManager::Get().GetTimeStamp(*SharedCookedAssetRegistry);



			if ( (CurrentIterativeCookedBuild >= CurrentLocalCookedBuild) && 
				(CurrentIterativeCookedBuild != FDateTime::MinValue()) )
			{
				// clean the sandbox 
				ClearPlatformCookedData(FName(*Target->PlatformName()));
				FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);

				// SaveCurrentIniSettings(Target); // use this if we don't care about ini safty.
				// copy the ini settings from the shared cooked build. 
				const FString SharedCookedIniFile = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("SharedIterativeBuild"), *Target->PlatformName(), TEXT("Cooked"), TEXT("CookedIniVersion.txt"));
				const FString SandboxCookedIniFile = ConvertToFullSandboxPath(*(FPaths::ProjectDir() / TEXT("CookedIniVersion.txt")), true).Replace(TEXT("[Platform]"), *Target->PlatformName());

				IFileManager::Get().Copy(*SandboxCookedIniFile, *SharedCookedIniFile);

				bool bIniSettingsOutOfDate = IniSettingsOutOfDate(Target);
				if (bIniSettingsOutOfDate && !IsCookFlagSet(ECookInitializationFlags::IgnoreIniSettingsOutOfDate))
				{
					UE_LOG(LogCook, Display, TEXT("Shared iterative build ini settings out of date, not using shared cooked build"));
				}
				else
				{
					if (bIniSettingsOutOfDate)
					{
						UE_LOG(LogCook, Display, TEXT("Shared iterative build ini settings out of date, but we don't care"));
					}

					UE_LOG(LogCook, Display, TEXT("Shared iterative build is newer then local cooked build, iteratively cooking from shared build "));
					PlatformAssetRegistry->LoadPreviousAssetRegistry(SharedCookedAssetRegistry);
				}
			}
			else
			{
				UE_LOG(LogCook, Display, TEXT("Local cook is newer then shared cooked build, iterativly cooking from local build"));
				PlatformAssetRegistry->LoadPreviousAssetRegistry(SandboxCookedAssetRegistryFilename);
			}
		}
		else
		{
			PlatformAssetRegistry->LoadPreviousAssetRegistry(SandboxCookedAssetRegistryFilename);
		}

		// Get list of changed packages
		TSet<FName> ModifiedPackages, NewPackages, RemovedPackages, IdenticalCookedPackages, IdenticalUncookedPackages;

		// We recurse modifications up the reference chain because it is safer, if this ends up being a significant issue in some games we can add a command line flag
		bool bRecurseModifications = true;
		bool bRecurseScriptModifications = !IsCookFlagSet(ECookInitializationFlags::IgnoreScriptPackagesOutOfDate);
		PlatformAssetRegistry->ComputePackageDifferences(ModifiedPackages, NewPackages, RemovedPackages, IdenticalCookedPackages, IdenticalUncookedPackages, bRecurseModifications, bRecurseScriptModifications);

		// check the files on disk 
		TMap<FName, FName> UncookedPathToCookedPath;
		// get all the on disk cooked files
		GetAllCookedFiles(UncookedPathToCookedPath, SandboxPath);

		const static FName NAME_DummyCookedFilename(TEXT("DummyCookedFilename")); // pls never name a package dummycookedfilename otherwise shit might go wonky
		if (bIsIterateSharedBuild)
		{
			TSet<FName> ExistingPackages = ModifiedPackages;
			ExistingPackages.Append(RemovedPackages);
			ExistingPackages.Append(IdenticalCookedPackages);
			ExistingPackages.Append(IdenticalUncookedPackages);

			// if we are iterating of a shared build the cooked files might not exist in the cooked directory because we assume they are packaged in the pak file (which we don't want to extract)
			for (FName PackageName : ExistingPackages)
			{
				FString Filename;
				if (FPackageName::DoesPackageExist(PackageName.ToString(), nullptr, &Filename))
				{
					UncookedPathToCookedPath.Add(FName(*Filename), NAME_DummyCookedFilename);
				}
			}
		}

		uint32 NumPackagesConsidered = UncookedPathToCookedPath.Num();
		uint32 NumPackagesUnableToFindCookedPackageInfo = 0;
		uint32 NumPackagesFileHashMismatch = 0;
		uint32 NumPackagesKept = 0;
		uint32 NumMarkedFailedSaveKept = 0;
		uint32 NumPackagesRemoved = 0;

		for (const auto& CookedPaths : UncookedPathToCookedPath)
		{
			const FName CookedFile = CookedPaths.Value;
			const FName UncookedFilename = CookedPaths.Key;
			const FName* FoundPackageName = GetCachedPackageFilenameToPackageFName(UncookedFilename);
			if ( !FoundPackageName )
			{
				// Source file no longer exists
				++NumPackagesRemoved;
				continue;
			}
			const FName PackageName = *FoundPackageName;
			bool bShouldKeep = true;

			if (ModifiedPackages.Contains(PackageName))
			{
				++NumPackagesFileHashMismatch;
				bShouldKeep = false;
			}
			else if (NewPackages.Contains(PackageName) || RemovedPackages.Contains(PackageName))
			{
				++NumPackagesUnableToFindCookedPackageInfo;
				bShouldKeep = false;
			}
			else if (IdenticalUncookedPackages.Contains(PackageName))
			{
				// These are packages which failed to save the first time 
				// most likely because they are editor only packages
				bShouldKeep = false;
			}
				
			if (CookedFile == NAME_DummyCookedFilename)
			{
				check(IFileManager::Get().FileExists(*CookedFile.ToString()) == false);
			}

			TArray<FName> PlatformNames;
			PlatformNames.Add(PlatformFName);

			if (bShouldKeep)
			{
				// Add this package to the CookedPackages list so that we don't try cook it again
				if (CookedFile != NAME_DummyCookedFilename)
				{
					check(IFileManager::Get().FileExists(*CookedFile.ToString()));
				}
				TArray<bool> Succeeded;
				Succeeded.Add(true);

				if (IdenticalCookedPackages.Contains(PackageName))
				{
					CookedPackages.Add(FFilePlatformCookedPackage(UncookedFilename, MoveTemp(PlatformNames), MoveTemp(Succeeded)));
					++NumPackagesKept;
				}
			}
			else
			{
				if ( IsCookByTheBookMode() ) // cook on the fly will requeue this package when it wants it 
				{
					// Force cook the modified file
					CookRequests.EnqueueUnique(FFilePlatformRequest(UncookedFilename, PlatformNames));
				}
				if (CookedFile != NAME_DummyCookedFilename)
				{
					// delete the old package 
					const FString CookedFullPath = FPaths::ConvertRelativePathToFull(CookedFile.ToString());
					UE_LOG(LogCook, Verbose, TEXT("Deleting cooked package %s failed filehash test"), *CookedFullPath);
					CookedPackagesToDelete.Add(CookedFullPath);
				}
				else
				{
					// the cooker should rebuild this package because it's not in the cooked package list
					// the new package will have higher priority then the package in the original shared cooked build
					const FString UncookedFilenameString = UncookedFilename.ToString();
					UE_LOG(LogCook, Verbose, TEXT("Shared cooked build: Detected package is out of date %s"), *UncookedFilenameString);
				}
			}
		}

		// Register identical uncooked packages from previous run
		for (FName UncookedPackage : IdenticalUncookedPackages)
		{
			const FName UncookedFilename = GetCachedStandardPackageFileFName(UncookedPackage);

			TArray<FName> PlatformNames;
			PlatformNames.Add(PlatformFName);

			ensure(CookedPackages.Exists(UncookedFilename, PlatformNames, false) == false);

			CookedPackages.Add(FFilePlatformCookedPackage(UncookedFilename, MoveTemp(PlatformNames)));
			++NumMarkedFailedSaveKept;
		}

		UE_LOG(LogCook, Display, TEXT("Iterative cooking summary for %s, \nConsidered: %d, \nFile Hash missmatch: %d, \nPackages Kept: %d, \nPackages failed save kept: %d, \nMissing Cooked Info(expected 0): %d"),
			*Target->PlatformName(),
			NumPackagesConsidered, NumPackagesFileHashMismatch,
			NumPackagesKept, NumMarkedFailedSaveKept,
			NumPackagesUnableToFindCookedPackageInfo);

		auto DeletePackageLambda = [&CookedPackagesToDelete](int32 PackageIndex)
		{
			const FString& CookedFullPath = CookedPackagesToDelete[PackageIndex];
			IFileManager::Get().Delete(*CookedFullPath, true, true, true);
		};
		ParallelFor(CookedPackagesToDelete.Num(), DeletePackageLambda);
	}
}

const FString ExtractPackageNameFromObjectPath( const FString ObjectPath )
{
	// get the path 
	int32 Beginning = ObjectPath.Find(TEXT("'"), ESearchCase::CaseSensitive);
	if ( Beginning == INDEX_NONE )
	{
		return ObjectPath;
	}
	int32 End = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, Beginning + 1);
	if (End == INDEX_NONE )
	{
		End = ObjectPath.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Beginning + 1);
	}
	if ( End == INDEX_NONE )
	{
		// one more use case is that the path is "Class'Path" example "OrionBoostItemDefinition'/Game/Misc/Boosts/XP_1Win" dunno why but this is actually dumb
		if ( ObjectPath[Beginning+1] == '/' )
		{
			return ObjectPath.Mid(Beginning+1);
		}
		return ObjectPath;
	}
	return ObjectPath.Mid(Beginning + 1, End - Beginning - 1);
}

void UCookOnTheFlyServer::CleanSandbox(const bool bIterative)
{
	//child cookers shouldn't clean sandbox we would be deleting the master cookers / other cookers data
	check(IsChildCooker() == false);

	const TArray<ITargetPlatform*>& Platforms = GetCookingTargetPlatforms();

	// before we can delete any cooked files we need to make sure that we have finished writing them
	UPackage::WaitForAsyncFileWrites();

#if OUTPUT_TIMING
	double SandboxCleanTime = 0.0;
#endif
	{
#if OUTPUT_TIMING
		SCOPE_SECONDS_COUNTER(SandboxCleanTime);
		SCOPE_TIMER(CleanSandboxTime);
#endif
		if (bIterative == false)
		{
			// for now we are going to wipe the cooked directory
			for (int32 Index = 0; Index < Platforms.Num(); Index++)
			{
				ITargetPlatform* Target = Platforms[Index];
				UE_LOG(LogCook, Display, TEXT("Cooked content cleared for platform %s"), *Target->PlatformName());

				FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName()); // GetOutputDirectory(Target->PlatformName());
				IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);
				
				ClearPlatformCookedData(FName(*Target->PlatformName()));

				IniSettingsOutOfDate(Target);
				SaveCurrentIniSettings(Target);
			}

		}
		else
		{
			for (const ITargetPlatform* Target : Platforms)
			{
				bool bIniSettingsOutOfDate = IniSettingsOutOfDate(Target);
				if (bIniSettingsOutOfDate)
				{
					if (!IsCookFlagSet(ECookInitializationFlags::IgnoreIniSettingsOutOfDate))
					{
						UE_LOG(LogCook, Display, TEXT("Cook invalidated for platform %s ini settings don't match from last cook, clearing all cooked content"), *Target->PlatformName());

						ClearPlatformCookedData(FName(*Target->PlatformName()));

						FString SandboxDirectory = GetSandboxDirectory(Target->PlatformName());
						IFileManager::Get().DeleteDirectory(*SandboxDirectory, false, true);

						SaveCurrentIniSettings(Target);
					}
					else
					{
						UE_LOG(LogCook, Display, TEXT("Inisettings were out of date for platform %s but we are going with it anyway because IgnoreIniSettingsOutOfDate is set"), *Target->PlatformName());
					}
				}
			}

			// This is fast in asset registry iterate, so just reconstruct
			CookedPackages.Empty();
			PopulateCookedPackagesFromDisk(Platforms);
		}
	}
#if OUTPUT_TIMING
	FString PlatformNames;
	for (const ITargetPlatform* Target : Platforms)
	{
		PlatformNames += Target->PlatformName() + TEXT(" ");
	}
	UE_LOG(LogCook, Display, TEXT("Sandbox cleanup took %5.3f seconds for platforms %s iterative %s"), SandboxCleanTime, *PlatformNames, bIterative ? TEXT("true") : TEXT("false"));
#endif
}

void UCookOnTheFlyServer::GenerateAssetRegistry()
{
	// Cache asset registry for later
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();

	if (!!(CookFlags & ECookInitializationFlags::GeneratedAssetRegistry))
	{
		// Force a rescan of modified package files
		TArray<FString> ModifiedPackageFileList;

		for (FName ModifiedPackage : ModifiedAssetFilenames)
		{
			ModifiedPackageFileList.Add(ModifiedPackage.ToString());
		}

		AssetRegistry->ScanModifiedAssetFiles(ModifiedPackageFileList);

		ModifiedAssetFilenames.Reset();

		// This is cook in the editor on a second pass, so refresh the generators
		for (TPair<FName, FAssetRegistryGenerator*>& Pair : RegistryGenerators)
		{
			Pair.Value->Initialize(CookByTheBookOptions ? CookByTheBookOptions->StartupPackages : TArray<FName>());
		}
		return;
	}
	CookFlags |= ECookInitializationFlags::GeneratedAssetRegistry;

	if (IsChildCooker())
	{
		// don't generate the asset registry
		return;
	}

	double GenerateAssetRegistryTime = 0.0;
	{
		SCOPE_TIMER(GenerateAssetRegistryTime);
		UE_LOG(LogCook, Display, TEXT("Creating asset registry"));

		// Perform a synchronous search of any .ini based asset paths (note that the per-game delegate may
		// have already scanned paths on its own)
		// We want the registry to be fully initialized when generating streaming manifests too.

		// editor will scan asset registry automagically 
		bool bCanDelayAssetregistryProcessing = IsRealtimeMode(); 

		// if we are running in the editor we need the asset registry to be finished loaded before we process any iterative cook requests
		bCanDelayAssetregistryProcessing &= !IsCookFlagSet(ECookInitializationFlags::Iterative); // 

		
		if ( !bCanDelayAssetregistryProcessing)
		{
			TArray<FString> ScanPaths;
			if (GConfig->GetArray(TEXT("AssetRegistry"), TEXT("PathsToScanForCook"), ScanPaths, GEngineIni) > 0 && !AssetRegistry->IsLoadingAssets())
			{
				AssetRegistry->ScanPathsSynchronous(ScanPaths);
			}
			else
			{
				// This will flush the background gather if we're in the editor
				AssetRegistry->SearchAllAssets(true);
			}
		}
	}

	const TArray<ITargetPlatform*>& Platforms = GetCookingTargetPlatforms();

	for (ITargetPlatform* TargetPlatform : Platforms)
	{
		FName PlatformName = FName(*TargetPlatform->PlatformName());

		// make sure we have a registry generate for all the platforms 
		FAssetRegistryGenerator* RegistryGenerator = RegistryGenerators.FindRef(PlatformName);
		if (RegistryGenerator == nullptr)
		{
			RegistryGenerator = new FAssetRegistryGenerator(TargetPlatform);
			RegistryGenerator->CleanManifestDirectories();
			RegistryGenerator->Initialize(CookByTheBookOptions ? CookByTheBookOptions->StartupPackages : TArray<FName>());
			RegistryGenerators.Add(PlatformName, RegistryGenerator);
		}
	}
}

void UCookOnTheFlyServer::GenerateLongPackageNames(TArray<FName>& FilesInPath)
{
	TArray<FName> FilesInPathReverse;
	FilesInPathReverse.Reserve(FilesInPath.Num());

	for( int32 FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
	{
		const FString& FileInPath = FilesInPath[FilesInPath.Num() - FileIndex - 1].ToString();
		if (FPackageName::IsValidLongPackageName(FileInPath))
		{
			const FName FileInPathFName(*FileInPath);
			FilesInPathReverse.AddUnique(FileInPathFName);
		}
		else
		{
			FString LongPackageName;
			FString FailureReason;
			if (FPackageName::TryConvertFilenameToLongPackageName(FileInPath, LongPackageName, &FailureReason))
			{
				const FName LongPackageFName(*LongPackageName);
				FilesInPathReverse.AddUnique(LongPackageFName);
			}
			else
			{
				LogCookerMessage(FString::Printf(TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to generate long package name for %s because %s"), *FileInPath, *FailureReason);
			}
		}
	}
	// Exchange(FilesInPathReverse, FilesInPath);
	FilesInPath.Empty(FilesInPathReverse.Num());
	for ( const FName& Files : FilesInPathReverse )
	{
		FilesInPath.Add(Files);
	}
}

void UCookOnTheFlyServer::AddFileToCook( TArray<FName>& InOutFilesToCook, const FString &InFilename ) const
{ 
	if (!FPackageName::IsScriptPackage(InFilename) && !FPackageName::IsMemoryPackage(InFilename))
	{
		FName InFilenameName = FName(*InFilename );
		if ( InFilenameName == NAME_None)
		{
			return;
		}

		InOutFilesToCook.AddUnique(InFilenameName);
	}
}

void UCookOnTheFlyServer::CollectFilesToCook(TArray<FName>& FilesInPath, const TArray<FString>& CookMaps, const TArray<FString>& InCookDirectories, const TArray<FString> &CookCultures, const TArray<FString> &IniMapSections, ECookByTheBookOptions FilesToCookFlags)
{
#if OUTPUT_TIMING
	SCOPE_TIMER(CollectFilesToCook);
#endif
	UProjectPackagingSettings* PackagingSettings = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());

	bool bCookAll = (!!(FilesToCookFlags & ECookByTheBookOptions::CookAll)) || PackagingSettings->bCookAll;
	bool bMapsOnly = (!!(FilesToCookFlags & ECookByTheBookOptions::MapsOnly)) || PackagingSettings->bCookMapsOnly;
	bool bNoDev = !!(FilesToCookFlags & ECookByTheBookOptions::NoDevContent);

	TArray<FName> InitialPackages = FilesInPath;

	if (IsChildCooker())
	{
		const FString ChildCookFilename = CookByTheBookOptions->ChildCookFilename;
		check(ChildCookFilename.Len());
		FString ChildCookString;
		ensure(FFileHelper::LoadFileToString(ChildCookString, *ChildCookFilename));
	
		TArray<FString> ChildCookArray;
		ChildCookString.ParseIntoArrayLines(ChildCookArray);

		for (const FString& ChildFile : ChildCookArray)
		{
			AddFileToCook(FilesInPath, ChildFile);
		}
		// if we are a child cooker then just add the files for the child cooker and early out 
		return;
	}

	
	TArray<FString> CookDirectories = InCookDirectories;
	
	if (!IsCookingDLC() && 
		!(FilesToCookFlags & ECookByTheBookOptions::NoAlwaysCookMaps))
	{

		{
			TArray<FString> MapList;
			// Add the default map section
			GEditor->LoadMapListFromIni(TEXT("AlwaysCookMaps"), MapList);

			for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
			{
				UE_LOG(LogCook, Verbose, TEXT("Maplist contains has %s "), *MapList[MapIdx]);
				AddFileToCook(FilesInPath, MapList[MapIdx]);
			}
		}


		bool bFoundMapsToCook = CookMaps.Num() > 0;

		{
			TArray<FString> MapList;
			for (const auto& IniMapSection : IniMapSections)
			{
				UE_LOG(LogCook, Verbose, TEXT("Loading map ini section %s "), *IniMapSection);
				GEditor->LoadMapListFromIni(*IniMapSection, MapList);
			}
			for (int32 MapIdx = 0; MapIdx < MapList.Num(); MapIdx++)
			{
				UE_LOG(LogCook, Verbose, TEXT("Maplist contains has %s "), *MapList[MapIdx]);
				AddFileToCook(FilesInPath, MapList[MapIdx]);
				bFoundMapsToCook = true;
			}
		}

		// If we didn't find any maps look in the project settings for maps
		for (const FFilePath& MapToCook : PackagingSettings->MapsToCook)
		{
			UE_LOG(LogCook, Verbose, TEXT("Maps to cook list contains %s "), *MapToCook.FilePath);
			FilesInPath.Add(FName(*MapToCook.FilePath));
			bFoundMapsToCook = true;
		}



		// if we didn't find maps to cook, and we don't have any commandline maps (CookMaps), then cook the allmaps section
		if (bFoundMapsToCook == false && CookMaps.Num() == 0)
		{
			UE_LOG(LogCook, Verbose, TEXT("Loading default map ini section AllMaps "));
			TArray<FString> AllMapsSection;
			GEditor->LoadMapListFromIni(TEXT("AllMaps"), AllMapsSection);
			for (const FString& MapName : AllMapsSection)
			{
				AddFileToCook(FilesInPath, MapName);
			}
		}

		// Also append any cookdirs from the project ini files; these dirs are relative to the game content directory
		{
			const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
			for (const FDirectoryPath& DirToCook : PackagingSettings->DirectoriesToAlwaysCook)
			{
				UE_LOG(LogCook, Verbose, TEXT("Loading directory to always cook %s"), *DirToCook.Path);
				CookDirectories.Add(AbsoluteGameContentDir / DirToCook.Path);
			}
		}


	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::NoGameAlwaysCookPackages))
	{
		COOK_STAT(FScopedDurationTimer TickTimer(DetailedCookStats::GameCookModificationDelegateTimeSec));
		SCOPE_TIMER(CookModificationDelegate);
#define DEBUG_COOKMODIFICATIONDELEGATE 0
#if DEBUG_COOKMODIFICATIONDELEGATE
		TSet<UPackage*> LoadedPackages;
		for ( TObjectIterator<UPackage> It; It; ++It)
		{
			LoadedPackages.Add(*It);
		}
#endif

		// allow the game to fill out the asset registry, as well as get a list of objects to always cook
		TArray<FString> FilesInPathStrings;
		FGameDelegates::Get().GetCookModificationDelegate().ExecuteIfBound(FilesInPathStrings);

		for (const FString& FileString : FilesInPathStrings)
		{
			FilesInPath.Add(FName(*FileString));
		}

		if (UAssetManager::IsValid())
		{
			TArray<FName> PackagesToNeverCook;

			UAssetManager::Get().ModifyCook(FilesInPath, PackagesToNeverCook);

			for (FName NeverCookPackage : PackagesToNeverCook)
			{
				const FName StandardPackageFilename = GetCachedStandardPackageFileFName(NeverCookPackage);

				if (StandardPackageFilename != NAME_None)
				{
					NeverCookPackageList.Add(StandardPackageFilename);
				}
			}
		}
#if DEBUG_COOKMODIFICATIONDELEGATE
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			if ( !LoadedPackages.Contains(*It) )
			{
				UE_LOG(LogCook, Display, TEXT("CookModificationDelegate loaded %s"), *It->GetName());
			}
		}
#endif

		if (UE_LOG_ACTIVE(LogCook, Verbose) )
		{
			for ( const FString& FileName : FilesInPathStrings )
			{
				UE_LOG(LogCook, Verbose, TEXT("Cook modification delegate requested package %s"), *FileName);
			}
		}
	}

	for ( const FString& CurrEntry : CookMaps )
	{
		SCOPE_TIMER(SearchForPackageOnDisk);
		if (FPackageName::IsShortPackageName(CurrEntry))
		{
			FString OutFilename;
			if (FPackageName::SearchForPackageOnDisk(CurrEntry, NULL, &OutFilename) == false)
			{
				LogCookerMessage( FString::Printf(TEXT("Unable to find package for map %s."), *CurrEntry), EMessageSeverity::Warning);
				UE_LOG(LogCook, Warning, TEXT("Unable to find package for map %s."), *CurrEntry);
			}
			else
			{
				AddFileToCook( FilesInPath, OutFilename);
			}
		}
		else
		{
			AddFileToCook( FilesInPath,CurrEntry);
		}
	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::DisableUnsolicitedPackages))
	{
		const FString ExternalMountPointName(TEXT("/Game/"));

		if (IsCookingDLC())
		{
			// get the dlc and make sure we cook that directory 
			FString DLCPath = FPaths::Combine(*GetBaseDirectoryForDLC(), TEXT("Content"));

			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false, false);
			IFileManager::Get().FindFilesRecursive(Files, *DLCPath, *(FString(TEXT("*")) + FPackageName::GetMapPackageExtension()), true, false, false);
			for (int32 Index = 0; Index < Files.Num(); Index++)
			{
				FString StdFile = Files[Index];
				FPaths::MakeStandardFilename(StdFile);
				AddFileToCook(FilesInPath, StdFile);

				// this asset may not be in our currently mounted content directories, so try to mount a new one now
				FString LongPackageName;
				if (!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
				{
					FPackageName::RegisterMountPoint(ExternalMountPointName, DLCPath);
				}
			}
		}

		for (const FString& CurrEntry : CookDirectories)
		{
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *CurrEntry, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
			for (int32 Index = 0; Index < Files.Num(); Index++)
			{
				FString StdFile = Files[Index];
				FPaths::MakeStandardFilename(StdFile);
				AddFileToCook(FilesInPath, StdFile);

				// this asset may not be in our currently mounted content directories, so try to mount a new one now
				FString LongPackageName;
				if (!FPackageName::IsValidLongPackageName(StdFile) && !FPackageName::TryConvertFilenameToLongPackageName(StdFile, LongPackageName))
				{
					FPackageName::RegisterMountPoint(ExternalMountPointName, CurrEntry);
				}
			}
		}

		// If no packages were explicitly added by command line or game callback, add all maps
		if (FilesInPath.Num() == InitialPackages.Num() || bCookAll)
		{
			TArray<FString> Tokens;
			Tokens.Empty(2);
			Tokens.Add(FString("*") + FPackageName::GetAssetPackageExtension());
			Tokens.Add(FString("*") + FPackageName::GetMapPackageExtension());

			uint8 PackageFilter = NORMALIZE_DefaultFlags | NORMALIZE_ExcludeEnginePackages;
			if (bMapsOnly)
			{
				PackageFilter |= NORMALIZE_ExcludeContentPackages;
			}

			if (bNoDev)
			{
				PackageFilter |= NORMALIZE_ExcludeDeveloperPackages;
			}

			// assume the first token is the map wildcard/pathname
			TArray<FString> Unused;
			for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
			{
				TArray<FString> TokenFiles;
				if (!NormalizePackageNames(Unused, TokenFiles, Tokens[TokenIndex], PackageFilter))
				{
					UE_LOG(LogCook, Display, TEXT("No packages found for parameter %i: '%s'"), TokenIndex, *Tokens[TokenIndex]);
					continue;
				}

				for (int32 TokenFileIndex = 0; TokenFileIndex < TokenFiles.Num(); ++TokenFileIndex)
				{
					AddFileToCook(FilesInPath, TokenFiles[TokenFileIndex]);
				}
			}
		}

		// Add any files of the desired cultures localized assets to cook.
		for (const FString& CultureToCookName : CookCultures)
		{
			FCulturePtr CultureToCook = FInternationalization::Get().GetCulture(CultureToCookName);
			if (!CultureToCook.IsValid())
			{
				continue;
			}

			const TArray<FString> CultureNamesToSearchFor = CultureToCook->GetPrioritizedParentCultureNames();

			for (const FString& L10NSubdirectoryName : CultureNamesToSearchFor)
			{
				TArray<FString> Files;
				const FString DirectoryToSearch = FPaths::Combine(*FPaths::ProjectContentDir(), *FString::Printf(TEXT("L10N/%s"), *L10NSubdirectoryName));
				IFileManager::Get().FindFilesRecursive(Files, *DirectoryToSearch, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (FString StdFile : Files)
				{
					UE_LOG(LogCook, Verbose, TEXT("Including culture information %s "), *StdFile);
					FPaths::MakeStandardFilename(StdFile);
					AddFileToCook(FilesInPath, StdFile);
				}
			}
		}
	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::NoDefaultMaps))
	{
		// make sure we cook the default maps
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		static const TArray<ITargetPlatform*>& Platforms = TPM.GetTargetPlatforms();
		for (int32 Index = 0; Index < Platforms.Num(); Index++)
		{
			// load the platform specific ini to get its DefaultMap
			FConfigFile PlatformEngineIni;
			FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, *Platforms[Index]->IniPlatformName());

			// get the server and game default maps and cook them
			FString Obj;
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (IsCookFlagSet(ECookInitializationFlags::IncludeServerMaps))
			{
				if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("ServerDefaultMap"), Obj))
				{
					if (Obj != FName(NAME_None).ToString())
					{
						AddFileToCook(FilesInPath, Obj);
					}
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultServerGameMode"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
			if (PlatformEngineIni.GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameInstanceClass"), Obj))
			{
				if (Obj != FName(NAME_None).ToString())
				{
					AddFileToCook(FilesInPath, Obj);
				}
			}
		}
	}

	if (!(FilesToCookFlags & ECookByTheBookOptions::NoInputPackages))
	{
		// make sure we cook any extra assets for the default touch interface
		// @todo need a better approach to cooking assets which are dynamically loaded by engine code based on settings
		FConfigFile InputIni;
		FString InterfaceFile;
		FConfigCacheIni::LoadLocalIniFile(InputIni, TEXT("Input"), true);
		if (InputIni.GetString(TEXT("/Script/Engine.InputSettings"), TEXT("DefaultTouchInterface"), InterfaceFile))
		{
			if (InterfaceFile != TEXT("None") && InterfaceFile != TEXT(""))
			{
				AddFileToCook(FilesInPath, InterfaceFile);
			}
		}
	}
	//@todo SLATE: This is a hack to ensure all slate referenced assets get cooked.
	// Slate needs to be refactored to properly identify required assets at cook time.
	// Simply jamming everything in a given directory into the cook list is error-prone
	// on many levels - assets not required getting cooked/shipped; assets not put under 
	// the correct folder; etc.
	if ( !(FilesToCookFlags & ECookByTheBookOptions::NoSlatePackages))
	{
		TArray<FString> UIContentPaths;
		TSet <FName> ContentDirectoryAssets; 
		if (GConfig->GetArray(TEXT("UI"), TEXT("ContentDirectories"), UIContentPaths, GEditorIni) > 0)
		{
			for (int32 DirIdx = 0; DirIdx < UIContentPaths.Num(); DirIdx++)
			{
				FString ContentPath = FPackageName::LongPackageNameToFilename(UIContentPaths[DirIdx]);

				TArray<FString> Files;
				IFileManager::Get().FindFilesRecursive(Files, *ContentPath, *(FString(TEXT("*")) + FPackageName::GetAssetPackageExtension()), true, false);
				for (int32 Index = 0; Index < Files.Num(); Index++)
				{
					FString StdFile = Files[Index];
					FName PackageName = FName(*FPackageName::FilenameToLongPackageName(StdFile));
					ContentDirectoryAssets.Add(PackageName);
					FPaths::MakeStandardFilename(StdFile);
					AddFileToCook( FilesInPath, StdFile);
				}
			}
		}

		if (CookByTheBookOptions && CookByTheBookOptions->bGenerateDependenciesForMaps) 
		{
			for (auto& MapDependencyGraph : CookByTheBookOptions->MapDependencyGraphs)
			{
				MapDependencyGraph.Value.Add(FName(TEXT("ContentDirectoryAssets")), ContentDirectoryAssets);
			}
		}
	}

	if (CookByTheBookOptions && !(FilesToCookFlags & ECookByTheBookOptions::DisableUnsolicitedPackages))
	{
		// Gather initial unsolicited package list, this is needed in iterative mode because it may skip cooking all explicit packages and never hit this code
		TArray<UPackage*> UnsolicitedPackages;
		bool ContainsFullAssetGCClasses = false;
		UE_LOG(LogCook, Verbose, TEXT("Finding initial unsolicited packages"));
		GetUnsolicitedPackages(UnsolicitedPackages, ContainsFullAssetGCClasses, CookByTheBookOptions->TargetPlatformNames);

		for (UPackage* UnsolicitedPackage : UnsolicitedPackages)
		{
			AddFileToCook(FilesInPath, UnsolicitedPackage->GetName());
		}
	}
}

bool UCookOnTheFlyServer::IsCookByTheBookRunning() const
{
	return CookByTheBookOptions && CookByTheBookOptions->bRunning;
}


void UCookOnTheFlyServer::SaveGlobalShaderMapFiles(const TArray<ITargetPlatform*>& Platforms)
{
	// we don't support this behavior
	check( !IsCookingDLC() );
	for (int32 Index = 0; Index < Platforms.Num(); Index++)
	{
		// make sure global shaders are up to date!
		TArray<FString> Files;
		FShaderRecompileData RecompileData;
		RecompileData.PlatformName = Platforms[Index]->PlatformName();
		// Compile for all platforms
		RecompileData.ShaderPlatform = -1;
		RecompileData.ModifiedFiles = &Files;
		RecompileData.MeshMaterialMaps = NULL;

		check( IsInGameThread() );

		FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

		RecompileShadersForRemote
			(RecompileData.PlatformName, 
			RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
			OutputDir, 
			RecompileData.MaterialsToLoad, 
			RecompileData.SerializedShaderResources, 
			RecompileData.MeshMaterialMaps, 
			RecompileData.ModifiedFiles);
	}
}

FString UCookOnTheFlyServer::GetSandboxDirectory( const FString& PlatformName ) const
{
	FString Result;
	Result = SandboxFile->GetSandboxDirectory();

	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite ) const
{
	check( SandboxFile );

	FString Result;
	if (bForWrite)
	{
		// Ideally this would be in the Sandbox File but it can't access the project or plugin
		if (PluginsToRemap.Num() > 0)
		{
			// Handle remapping of plugins
			for (TSharedRef<IPlugin> Plugin : PluginsToRemap)
			{
				// If these match, then this content is part of plugin that gets remapped when packaged/staged
				if (FileName.StartsWith(Plugin->GetContentDir()))
				{
					FString SearchFor;
					SearchFor /= Plugin->GetName() / TEXT("Content");
					int32 FoundAt = FileName.Find(SearchFor, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					check(FoundAt != -1);
					// Strip off everything but <PluginName/Content/<remaing path to file>
					FString SnippedOffPath = FileName.RightChop(FoundAt);
					// Put this is in <sandbox path>/RemappedPlugins/<PluginName>/Content/<remaing path to file>
					FString RemappedPath = SandboxFile->GetSandboxDirectory();
					RemappedPath /= REMAPPED_PLUGGINS;
					Result = RemappedPath / SnippedOffPath;
					return Result;
				}
			}
		}
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FileName);
	}
	else
	{
		Result = SandboxFile->ConvertToAbsolutePathForExternalAppForRead(*FileName);
	}

	/*if ( IsCookingDLC() )
	{
		check( IsCookByTheBookRunning() );
		Result.ReplaceInline(TEXT("/Cooked/"), *FString::Printf(TEXT("/CookedDLC_%s/"), *CookByTheBookOptions->DlcName) );
	}*/
	return Result;
}

FString UCookOnTheFlyServer::ConvertToFullSandboxPath( const FString &FileName, bool bForWrite, const FString& PlatformName ) const
{
	FString Result = ConvertToFullSandboxPath( FileName, bForWrite );
	Result.ReplaceInline(TEXT("[Platform]"), *PlatformName);
	return Result;
}

const FString UCookOnTheFlyServer::GetSandboxAssetRegistryFilename()
{
	static const FString RegistryFilename = FPaths::ProjectDir() / GetAssetRegistryFilename();

	if (IsCookingDLC())
	{
		check(IsCookByTheBookMode());
		const FString DLCRegistryFilename = FPaths::Combine(*GetBaseDirectoryForDLC(), GetAssetRegistryFilename());
		return ConvertToFullSandboxPath(*DLCRegistryFilename, true);
	}

	const FString SandboxRegistryFilename = ConvertToFullSandboxPath(*RegistryFilename, true);
	return SandboxRegistryFilename;
}

const FString UCookOnTheFlyServer::GetCookedAssetRegistryFilename(const FString& PlatformName )
{
	const FString CookedAssetRegistryFilename = GetSandboxAssetRegistryFilename().Replace(TEXT("[Platform]"), *PlatformName);
	return CookedAssetRegistryFilename;
}

void UCookOnTheFlyServer::CookByTheBookFinished()
{
	check( IsInGameThread() );
	check( IsCookByTheBookMode() );
	check( CookByTheBookOptions->bRunning == true );


	UPackage::WaitForAsyncFileWrites();

	GetDerivedDataCacheRef().WaitForQuiescence(true);
	
	UCookerSettings const* CookerSettings = GetDefault<UCookerSettings>();

	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	bool const bCacheShaderLibraries = !IsCookingDLC() && (CurrentCookMode == ECookMode::CookByTheBook);
    bool bShaderLibrarySaved = false;
	if (bCacheShaderLibraries && PackagingSettings->bShareMaterialShaderCode)
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		
		// Save shader code map
		{
			for (const FName& TargetPlatformName : CookByTheBookOptions->TargetPlatformNames)
			{
				FString TargetPlatformNameString = TargetPlatformName.ToString();
				const ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(TargetPlatformNameString);
				FString ShaderCodeDir = ConvertToFullSandboxPath(*FPaths::ProjectContentDir(), true, TargetPlatformNameString);
				FString DebugShaderCodeDir = ShaderCodeDir + TEXT("ShaderDebug");
				
				TArray<FName> ShaderFormats;
				TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
				
                bShaderLibrarySaved = FShaderCodeLibrary::SaveShaderCode(ShaderCodeDir, DebugShaderCodeDir, ShaderFormats);
				if(!bShaderLibrarySaved)
				{
					LogCookerMessage(FString::Printf(TEXT("Shared Material Shader Code Library failed for %s."),*TargetPlatformNameString), EMessageSeverity::Error);
				}
			}
		}
	}
	
	if (IsChildCooker())
	{
		// if we are the child cooker create a list of all the packages which we think wes hould have cooked but didn't
		FString UncookedPackageList;
		for (const FName& UncookedPackage : CookByTheBookOptions->ChildUnsolicitedPackages)
		{
			UncookedPackageList.Append(UncookedPackage.ToString() + TEXT("\n\r"));
		}
		FFileHelper::SaveStringToFile(UncookedPackageList, *GetChildCookerResultFilename(CookByTheBookOptions->ChildCookFilename));
		if (IBlueprintNativeCodeGenModule::IsNativeCodeGenModuleLoaded())
		{
			IBlueprintNativeCodeGenModule::Get().SaveManifest();
		}
		
		if (bCacheShaderLibraries && PackagingSettings->bShareMaterialShaderCode)
		{
			FShaderCodeLibrary::Shutdown();
		}
	}
	else
	{
		CleanUpChildCookers();

		if (IBlueprintNativeCodeGenModule::IsNativeCodeGenModuleLoaded())
		{
			SCOPE_TIMER(GeneratingBlueprintAssets)
			IBlueprintNativeCodeGenModule& CodeGenModule = IBlueprintNativeCodeGenModule::Get();

			CodeGenModule.GenerateFullyConvertedClasses(); // While generating fully converted classes the list of necessary stubs is created.
			CodeGenModule.GenerateStubs();

			// merge the manifest for the blueprint code generator:
			for (int32 I = 0; I < CookByTheBookOptions->ChildCookers.Num(); ++I )
			{
				CodeGenModule.MergeManifest(I);
			}

			CodeGenModule.FinalizeManifest();

			// Unload the module as we only need it while cooking. This will also clear the current module's state in order to allow a new cooker pass to function properly.
			FModuleManager::Get().UnloadModule(CodeGenModule.GetModuleName());
		}

		check(CookByTheBookOptions->ChildUnsolicitedPackages.Num() == 0);
		
		// Save modified asset registry with all streaming chunk info generated during cook
		const FString& SandboxRegistryFilename = GetSandboxAssetRegistryFilename();

		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		if (bCacheShaderLibraries && PackagingSettings->bShareMaterialShaderCode && bShaderLibrarySaved)
		{
			if (PackagingSettings->bSharedMaterialNativeLibraries)
			{
				for (const FName& TargetPlatformName : CookByTheBookOptions->TargetPlatformNames)
				{
					FString TargetPlatformNameString = TargetPlatformName.ToString();
					const ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(TargetPlatformNameString);
					FString ShaderCodeDir = ConvertToFullSandboxPath(*FPaths::ProjectContentDir(), true, TargetPlatformNameString);
					FString DebugShaderCodeDir = ShaderCodeDir + TEXT("ShaderDebug");
					
					TArray<FName> ShaderFormats;
					TargetPlatform->GetAllTargetedShaderFormats(ShaderFormats);
					
					if(!FShaderCodeLibrary::PackageNativeShaderLibrary(ShaderCodeDir, DebugShaderCodeDir, ShaderFormats))
					{
						// This is fatal - In this case we should cancel any launch on device operation or package write but we don't want to assert and crash the editor
						LogCookerMessage(FString::Printf(TEXT("Package Native Shader Library failed for %s."),*TargetPlatformNameString), EMessageSeverity::Error);
					}
				}
			}
			
			FShaderCodeLibrary::Shutdown();
		}				
		
		{
			SCOPE_TIMER(SavingCurrentIniSettings)
			for ( const FName& TargetPlatformName : CookByTheBookOptions->TargetPlatformNames )
			{
				const ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(TargetPlatformName.ToString());
				SaveCurrentIniSettings(TargetPlatform);
			}
		}

		{
			SCOPE_TIMER(SavingAssetRegistry);
			for (TPair<FName, FAssetRegistryGenerator*>& Pair : RegistryGenerators)
			{
				TArray<FName> CookedPackagesFilenames;
				TArray<FName> IgnorePackageFilenames;

				const FName& PlatformName = Pair.Key;
				FAssetRegistryGenerator& Generator = *Pair.Value;

				CookedPackages.GetCookedFilesForPlatform(PlatformName, CookedPackagesFilenames, false, true);

				// ignore any packages which failed to cook
				CookedPackages.GetCookedFilesForPlatform(PlatformName, IgnorePackageFilenames, true, false);

				if (IsCookingDLC())
				{
					// remove the previous release cooked packages from the new asset registry, add to ignore list
					SCOPE_TIMER(RemovingOldManifestEntries);
					
					const TArray<FName>* PreviousReleaseCookedPackages = CookByTheBookOptions->BasedOnReleaseCookedPackages.Find(PlatformName);
					if (PreviousReleaseCookedPackages)
					{
						for (FName PreviousReleaseCookedPackage : *PreviousReleaseCookedPackages)
						{
							CookedPackagesFilenames.Remove(PreviousReleaseCookedPackage);
							IgnorePackageFilenames.Add(PreviousReleaseCookedPackage);
						}
					}
				}

				// convert from filenames to package names
				TSet<FName> CookedPackageNames;
				for (FName PackageFilename : CookedPackagesFilenames)
				{
					const FName *FoundLongPackageFName = GetCachedPackageFilenameToPackageFName(PackageFilename);
					CookedPackageNames.Add(*FoundLongPackageFName);
				}

				TSet<FName> IgnorePackageNames;
				for (FName PackageFilename : IgnorePackageFilenames)
				{
					const FName *FoundLongPackageFName = GetCachedPackageFilenameToPackageFName(PackageFilename);
					IgnorePackageNames.Add(*FoundLongPackageFName);
				}

				// ignore packages that weren't cooked because they were only referenced by editor-only properties
				TSet<FName> UncookedEditorOnlyPackageNames;
				UncookedEditorOnlyPackages.GetNames(UncookedEditorOnlyPackageNames);
				for (FName UncookedEditorOnlyPackage : UncookedEditorOnlyPackageNames)
				{
					IgnorePackageNames.Add(UncookedEditorOnlyPackage);
				}
				{
					SCOPE_TIMER(BuildChunkManifest);
					Generator.BuildChunkManifest(CookedPackageNames, IgnorePackageNames, SandboxFile.Get(), CookByTheBookOptions->bGenerateStreamingInstallManifests);
				}
				{
					SCOPE_TIMER(SaveManifests);
					// Always try to save the manifests, this is required to make the asset registry work, but doesn't necessarily write a file
					Generator.SaveManifests(SandboxFile.Get());
				}
				{
					SCOPE_TIMER(SaveRealAssetRegistry);
					Generator.SaveAssetRegistry(SandboxRegistryFilename);

					if (!IsCookFlagSet(ECookInitializationFlags::Iterative))
					{
						Generator.WriteCookerOpenOrder();
					}
				}
				if (IsCreatingReleaseVersion())
				{
					const FString VersionedRegistryPath = GetReleaseVersionAssetRegistryPath(CookByTheBookOptions->CreateReleaseVersion, PlatformName);
					IFileManager::Get().MakeDirectory(*VersionedRegistryPath, true);
					const FString VersionedRegistryFilename = VersionedRegistryPath / GetAssetRegistryFilename();
					const FString CookedAssetRegistryFilename = SandboxRegistryFilename.Replace(TEXT("[Platform]"), *PlatformName.ToString());
					IFileManager::Get().Copy(*VersionedRegistryFilename, *CookedAssetRegistryFilename, true, true);

					// Also copy development registry if it exists
					const FString DevVersionedRegistryFilename = VersionedRegistryFilename.Replace(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));
					const FString DevCookedAssetRegistryFilename = CookedAssetRegistryFilename.Replace(TEXT("AssetRegistry.bin"), TEXT("DevelopmentAssetRegistry.bin"));
					IFileManager::Get().Copy(*DevVersionedRegistryFilename, *DevCookedAssetRegistryFilename, true, true);
				}
			}
		}
	}

	if (CookByTheBookOptions->bGenerateDependenciesForMaps && (IsChildCooker() == false))
	{
		SCOPE_TIMER(GenerateMapDependencies);
		for (auto& MapDependencyGraphIt : CookByTheBookOptions->MapDependencyGraphs)
		{
			BuildMapDependencyGraph(MapDependencyGraphIt.Key);
			WriteMapDependencyGraph(MapDependencyGraphIt.Key);
		}
	}

	CookByTheBookOptions->LastGCItems.Empty();
	const float TotalCookTime = (float)(FPlatformTime::Seconds() - CookByTheBookOptions->CookStartTime);
	UE_LOG(LogCook, Display, TEXT("Cook by the book total time in tick %fs total time %f"), CookByTheBookOptions->CookTime, TotalCookTime);

	CookByTheBookOptions->BasedOnReleaseCookedPackages.Empty();

	CookByTheBookOptions->bRunning = false;

	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();


	UE_LOG(LogCook, Display, TEXT("Peak Used virtual %u Peak Used phsical %u"), MemStats.PeakUsedVirtual / 1024 / 1024, MemStats.PeakUsedPhysical / 1024 / 1024 );

	OUTPUT_HIERARCHYTIMERS();
	CLEAR_HIERARCHYTIMERS();
}

void UCookOnTheFlyServer::BuildMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);

	TArray<FName> PlatformCookedPackages;
	CookedPackages.GetCookedFilesForPlatform(PlatformName, PlatformCookedPackages);

	// assign chunks for all the map packages
	for (const FName& CookedPackage : PlatformCookedPackages)
	{
		TArray<FAssetData> PackageAssets;
		FName Name = FName(*FPackageName::FilenameToLongPackageName(CookedPackage.ToString()));

		if (!ContainsMap(Name))
			continue;

		TSet<FName> DependentPackages;
		TSet<FName> Roots; 

		Roots.Add(Name);

		GetDependentPackages(Roots, DependentPackages);

		MapDependencyGraph.Add(Name, DependentPackages);
	}
}

void UCookOnTheFlyServer::WriteMapDependencyGraph(const FName& PlatformName)
{
	auto& MapDependencyGraph = CookByTheBookOptions->MapDependencyGraphs.FindChecked(PlatformName);

	FString MapDependencyGraphFile = FPaths::ProjectDir() / TEXT("MapDependencyGraph.json");
	// dump dependency graph. 
	FString DependencyString;
	DependencyString += "{";
	for (auto& Ele : MapDependencyGraph)
	{
		TSet<FName>& Deps = Ele.Value;
		FName MapName = Ele.Key;
		DependencyString += TEXT("\t\"") + MapName.ToString() + TEXT("\" : \n\t[\n ");
		for (FName& Val : Deps)
		{
			DependencyString += TEXT("\t\t\"") + Val.ToString() + TEXT("\",\n");
		}
		DependencyString.RemoveFromEnd(TEXT(",\n"));
		DependencyString += TEXT("\n\t],\n");
	}
	DependencyString.RemoveFromEnd(TEXT(",\n"));
	DependencyString += "\n}";

	FString CookedMapDependencyGraphFilePlatform = ConvertToFullSandboxPath(MapDependencyGraphFile, true).Replace(TEXT("[Platform]"), *PlatformName.ToString());
	FFileHelper::SaveStringToFile(DependencyString, *CookedMapDependencyGraphFilePlatform, FFileHelper::EEncodingOptions::ForceUnicode);
}

void UCookOnTheFlyServer::QueueCancelCookByTheBook()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		CookByTheBookOptions->bCancel = true;
	}
}

void UCookOnTheFlyServer::CancelCookByTheBook()
{
	if ( IsCookByTheBookMode() && CookByTheBookOptions->bRunning )
	{
		check(CookByTheBookOptions);
		check( IsInGameThread() );

		// save the cook requests 
		CookRequests.DequeueAllRequests(CookByTheBookOptions->PreviousCookRequests);
		CookByTheBookOptions->bRunning = false;

		SandboxFile = nullptr;
	}	
}

void UCookOnTheFlyServer::StopAndClearCookedData()
{
	if ( IsCookByTheBookMode() )
	{
		check( CookByTheBookOptions != NULL );
		check( CookByTheBookOptions->bRunning == false );
		CancelCookByTheBook();
		CookByTheBookOptions->PreviousCookRequests.Empty();
	}

	RecompileRequests.Empty();
	CookRequests.Empty();
	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearAllCookedData()
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	UnsolicitedCookedPackages.Empty();
	CookedPackages.Empty(); // set of files which have been cooked when needing to recook a file the entry will need to be removed from here
}

void UCookOnTheFlyServer::ClearPlatformCookedData( const FName& PlatformName )
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	CookedPackages.RemoveAllFilesForPlatform( PlatformName );
	TArray<FName> PackageNames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformName, PackageNames);
}

void UCookOnTheFlyServer::ClearCachedCookedPlatformDataForPlatform( const FName& PlatformName )
{

	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	ITargetPlatform* TargetPlatform = TPM.FindTargetPlatform(PlatformName.ToString());
	if ( TargetPlatform )
	{
		for ( TObjectIterator<UObject> It; It; ++It )
		{
			It->ClearCachedCookedPlatformData(TargetPlatform);
		}
	}
}


void UCookOnTheFlyServer::OnTargetPlatformChangedSupportedFormats(const ITargetPlatform* TargetPlatform)
{
	for (TObjectIterator<UObject> It; It; ++It)
	{
		It->ClearCachedCookedPlatformData(TargetPlatform);
	}
}

void UCookOnTheFlyServer::CreateSandboxFile()
{

	// initialize the sandbox file after determining if we are cooking dlc
	// Local sandbox file wrapper. This will be used to handle path conversions,
	// but will not be used to actually write/read files so we can safely
	// use [Platform] token in the sandbox directory name and then replace it
	// with the actual platform name.
	check( SandboxFile == nullptr );
	SandboxFile = MakeUnique<FSandboxPlatformFile>(false);

	// Output directory override.	
	FString OutputDirectory = GetOutputDirectoryOverride();

	// Use SandboxFile to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	SandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
}

void UCookOnTheFlyServer::InitializeSandbox()
{
	if ( SandboxFile == nullptr )
	{
		const TArray<ITargetPlatform*>& TargetPlatforms = GetCookingTargetPlatforms();

		CreateSandboxFile();

		if (!IsChildCooker())
		{
			bIsInitializingSandbox = true;
			CleanSandbox(IsCookFlagSet(ECookInitializationFlags::Iterative));
			bIsInitializingSandbox = false;

		}
	}
	else
	{
		// This is an in-editor cook, do an iterative clean
		CleanSandbox(true);
	}
}

void UCookOnTheFlyServer::TermSandbox()
{
	ClearAllCookedData();
	ClearPackageFilenameCache();
	SandboxFile = nullptr;
}

void UCookOnTheFlyServer::ValidateCookOnTheFlySettings() const
{
}

void UCookOnTheFlyServer::ValidateCookByTheBookSettings() const
{
	if (IsChildCooker())
	{
		// should never be generating dependency maps / streaming install manifests for child cookers
		check(CookByTheBookOptions->bGenerateDependenciesForMaps == false);
		check(CookByTheBookOptions->bGenerateStreamingInstallManifests == false);
	}
}

void UCookOnTheFlyServer::StartCookByTheBook( const FCookByTheBookStartupOptions& CookByTheBookStartupOptions )
{
	SCOPE_TIMER(StartCookByTheBookTime);

	const TArray<ITargetPlatform*>& TargetPlatforms = CookByTheBookStartupOptions.TargetPlatforms;
	const TArray<FString>& CookMaps = CookByTheBookStartupOptions.CookMaps;
	const TArray<FString>& CookDirectories = CookByTheBookStartupOptions.CookDirectories;
	const TArray<FString>& CookCultures = CookByTheBookStartupOptions.CookCultures;
	const TArray<FString>& IniMapSections = CookByTheBookStartupOptions.IniMapSections;
	const ECookByTheBookOptions& CookOptions = CookByTheBookStartupOptions.CookOptions;
	const FString& DLCName = CookByTheBookStartupOptions.DLCName;

	const FString& CreateReleaseVersion = CookByTheBookStartupOptions.CreateReleaseVersion;
	const FString& BasedOnReleaseVersion = CookByTheBookStartupOptions.BasedOnReleaseVersion;

	check( IsInGameThread() );
	check( IsCookByTheBookMode() );

	CookByTheBookOptions->bRunning = true;
	CookByTheBookOptions->bCancel = false;
	CookByTheBookOptions->CookTime = 0.0f;
	CookByTheBookOptions->CookStartTime = FPlatformTime::Seconds();
	CookByTheBookOptions->bGenerateStreamingInstallManifests = CookByTheBookStartupOptions.bGenerateStreamingInstallManifests;
	CookByTheBookOptions->bGenerateDependenciesForMaps = CookByTheBookStartupOptions.bGenerateDependenciesForMaps;
	CookByTheBookOptions->CreateReleaseVersion = CreateReleaseVersion;
	CookByTheBookOptions->ChildCookFilename = CookByTheBookStartupOptions.ChildCookFileName;
	CookByTheBookOptions->bDisableUnsolicitedPackages = !!(CookOptions & ECookByTheBookOptions::DisableUnsolicitedPackages);
	CookByTheBookOptions->ChildCookIdentifier = CookByTheBookStartupOptions.ChildCookIdentifier;
	CookByTheBookOptions->bErrorOnEngineContentUse = CookByTheBookStartupOptions.bErrorOnEngineContentUse;

	GenerateAssetRegistry();

	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();

	NeverCookPackageList.Empty();
	{
		const FString AbsoluteGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

		TArray<FString> NeverCookDirectories = CookByTheBookStartupOptions.NeverCookDirectories;

		for (const FDirectoryPath& DirToNotCook : PackagingSettings->DirectoriesToNeverCook)
		{
			NeverCookDirectories.Add(AbsoluteGameContentDir / DirToNotCook.Path);
		}

		for (const FString& NeverCookDirectory : NeverCookDirectories)
		{
			// add the packages to the never cook package list
			struct FNeverCookDirectoryWalker : public IPlatformFile::FDirectoryVisitor
			{
			private:
				FThreadSafeNameSet &NeverCookPackageList;
			public:
				FNeverCookDirectoryWalker(FThreadSafeNameSet &InNeverCookPackageList) : NeverCookPackageList(InNeverCookPackageList) { }

				// IPlatformFile::FDirectoryVisitor interface
				virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
				{
					if (bIsDirectory)
					{
						return true;
					}
					FString StandardFilename = FString(FilenameOrDirectory);
					FPaths::MakeStandardFilename(StandardFilename);

					NeverCookPackageList.Add(FName(*StandardFilename));
					return true;
				}

			} NeverCookDirectoryWalker(NeverCookPackageList);

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

			PlatformFile.IterateDirectoryRecursively(*NeverCookDirectory, NeverCookDirectoryWalker);
		}

	}
	

	CookByTheBookOptions->TargetPlatformNames.Empty();
	for (const ITargetPlatform* Platform : TargetPlatforms)
	{
		FName PlatformName = FName(*Platform->PlatformName());
		CookByTheBookOptions->TargetPlatformNames.Add(PlatformName); // build list of all target platform names
	}
	const TArray<FName>& TargetPlatformNames = CookByTheBookOptions->TargetPlatformNames;

	ValidateCookByTheBookSettings();

	if ( CookByTheBookOptions->DlcName != DLCName )
	{
		// we are going to change the state of dlc we need to clean out our package filename cache (the generated filename cache is dependent on this key)
		CookByTheBookOptions->DlcName = DLCName;

		TermSandbox();
	}

	// This will either delete the sandbox or iteratively clean it
	InitializeSandbox();

	if (CurrentCookMode == ECookMode::CookByTheBook && !IsCookFlagSet(ECookInitializationFlags::Iterative))
	{
		StartSavingEDLCookInfoForVerification();
	}

	// Note: Nativization only works with "cook by the book" mode and not from within the current editor process.
	if (CurrentCookMode == ECookMode::CookByTheBook
		&& PackagingSettings->BlueprintNativizationMethod != EProjectPackagingBlueprintNativizationMethod::Disabled)
	{
		FNativeCodeGenInitData CodeGenData;
		for (const ITargetPlatform* Entry : CookByTheBookStartupOptions.TargetPlatforms)
		{
			FPlatformNativizationDetails PlatformNativizationDetails;
			IBlueprintNativeCodeGenModule::Get().FillPlatformNativizationDetails(Entry, PlatformNativizationDetails);
			CodeGenData.CodegenTargets.Push(PlatformNativizationDetails);
		}
		CodeGenData.ManifestIdentifier = CookByTheBookStartupOptions.ChildCookIdentifier;
		IBlueprintNativeCodeGenModule::InitializeModule(CodeGenData);
	}

	CookByTheBookOptions->bLeakTest = (CookOptions & ECookByTheBookOptions::LeakTest) != ECookByTheBookOptions::None; // this won't work from the editor this needs to be standalone
	check(!CookByTheBookOptions->bLeakTest || CurrentCookMode == ECookMode::CookByTheBook);

	CookByTheBookOptions->LastGCItems.Empty();
	if (CookByTheBookOptions->bLeakTest)
	{
		for (FObjectIterator It; It; ++It)
		{
			CookByTheBookOptions->LastGCItems.Add(FWeakObjectPtr(*It));
		}
	}

	if (!IsChildCooker())
	{
		for (ITargetPlatform* Platform : TargetPlatforms)
		{
			FName PlatformName = FName(*Platform->PlatformName());

			if (CookByTheBookOptions->bGenerateDependenciesForMaps)
			{
				CookByTheBookOptions->MapDependencyGraphs.Add(PlatformName);
			}
		}
	}
	
	// shader code sharing does not support multiple packages yet
	bool const bCacheShaderLibraries = !IsCookingDLC() && (CurrentCookMode == ECookMode::CookByTheBook);
	if (bCacheShaderLibraries && PackagingSettings->bShareMaterialShaderCode)
	{
		FShaderCodeLibrary::InitForCooking(PackagingSettings->bSharedMaterialNativeLibraries);
	}

	if ( IsCookingDLC() )
	{
		// if we are cooking dlc we must be based of a release version cook
		check( !BasedOnReleaseVersion.IsEmpty() );

		for ( const FName& PlatformName : TargetPlatformNames )
		{
			FString OriginalSandboxRegistryFilename = GetReleaseVersionAssetRegistryPath(BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

			TArray<FName> PackageList;
			// if this check fails probably because the asset registry can't be found or read
			bool bSucceeded = GetAllPackageFilenamesFromAssetRegistry(OriginalSandboxRegistryFilename, PackageList);
			if (!bSucceeded)
			{
				using namespace PlatformInfo;
				// Check all possible flavors 
				// For example release version could be cooked as Android_ETC1 flavor, but DLC can be made as Android_ETC2
				FVanillaPlatformEntry VanillaPlatfromEntry = BuildPlatformHierarchy(PlatformName, EPlatformFilter::CookFlavor);
				for (const FPlatformInfo* PlatformFlaworInfo : VanillaPlatfromEntry.PlatformFlavors)
				{
					OriginalSandboxRegistryFilename = GetReleaseVersionAssetRegistryPath(BasedOnReleaseVersion, PlatformFlaworInfo->PlatformInfoName) / GetAssetRegistryFilename();
					bSucceeded = GetAllPackageFilenamesFromAssetRegistry(OriginalSandboxRegistryFilename, PackageList);
					if (bSucceeded)
					{
						break;
					}
				}
			}
			check( bSucceeded );

			if ( bSucceeded )
			{
				TArray<FName> PlatformNames;
				PlatformNames.Add(PlatformName);
				TArray<bool> Succeeded;
				Succeeded.Add(true);
				for (const FName& PackageFilename : PackageList)
				{
					CookedPackages.Add( FFilePlatformCookedPackage( PackageFilename, PlatformNames, Succeeded) );
				}
			}
			CookByTheBookOptions->BasedOnReleaseCookedPackages.Add(PlatformName, MoveTemp(PackageList));
		}
	}
	
	// don't resave the global shader map files in dlc
	if (!IsCookingDLC() && !IsChildCooker() && !(CookByTheBookStartupOptions.CookOptions & ECookByTheBookOptions::ForceDisableSaveGlobalShaders))
	{
		SaveGlobalShaderMapFiles(TargetPlatforms);
	}

	TArray<FName> FilesInPath;
	TSet<FName> StartupSoftObjectPackages;

	// Get the list of string asset references, for both empty package and all startup packages
	GRedirectCollector.ProcessSoftObjectPathPackageList(NAME_None, false, StartupSoftObjectPackages);

	for (const FName& StartupPackage : CookByTheBookOptions->StartupPackages)
	{
		GRedirectCollector.ProcessSoftObjectPathPackageList(StartupPackage, false, StartupSoftObjectPackages);
	}

	CollectFilesToCook(FilesInPath, CookMaps, CookDirectories, CookCultures, IniMapSections, CookOptions);

	// Add string asset packages after collecting files, to avoid accidentally activating the behavior to cook all maps if none are specified
	for (FName SoftObjectPackage : StartupSoftObjectPackages)
	{
		TMap<FName, FName> RedirectedPaths;

		// If this is a redirector, extract destination from asset registry
		if (ContainsRedirector(SoftObjectPackage, RedirectedPaths))
		{
			for (TPair<FName, FName>& RedirectedPath : RedirectedPaths)
			{
				GRedirectCollector.AddAssetPathRedirection(RedirectedPath.Key, RedirectedPath.Value);
			}
		}

		if (!CookByTheBookOptions->bDisableUnsolicitedPackages)
		{
			AddFileToCook(FilesInPath, SoftObjectPackage.ToString());
		}
	}
	
	if (FilesInPath.Num() == 0)
	{
		LogCookerMessage(FString::Printf(TEXT("No files found to cook.")), EMessageSeverity::Warning);
		UE_LOG(LogCook, Warning, TEXT("No files found."));
	}

	{
#if OUTPUT_TIMING
		SCOPE_TIMER(GenerateLongPackageName);
#endif
		GenerateLongPackageNames(FilesInPath);
	}
	// add all the files for the requested platform to the cook list
	for ( const FName& FileFName : FilesInPath )
	{
		if ( FileFName == NAME_None )
			continue;

		const FName PackageFileFName = GetCachedStandardPackageFileFName(FileFName);
		
		if (PackageFileFName != NAME_None)
		{
			CookRequests.EnqueueUnique( FFilePlatformRequest( PackageFileFName, TargetPlatformNames ) );
		}
		else if (!FLinkerLoad::IsKnownMissingPackage(FileFName))
		{
			const FString FileName = FileFName.ToString();
			LogCookerMessage( FString::Printf(TEXT("Unable to find package for cooking %s"), *FileName), EMessageSeverity::Warning );
			UE_LOG(LogCook, Warning, TEXT("Unable to find package for cooking %s"), *FileName)
		}	
	}


	if ( !IsCookingDLC() && !IsChildCooker() )
	{
		// if we are not cooking dlc then basedOnRelease version just needs to make sure that we cook all the packages which are in the previous release (as well as the new ones)
		if ( !BasedOnReleaseVersion.IsEmpty() )
		{
			// if we are based of a release and we are not cooking dlc then we should always be creating a new one (note that we could be creating the same one we are based of).
			// note that we might erroneously enter here if we are generating a patch instead and we accidentally passed in BasedOnReleaseVersion to the cooker instead of to unrealpak
			check( !CreateReleaseVersion.IsEmpty() );

			for ( const FName& PlatformName : TargetPlatformNames )
			{
				TArray<FName> PlatformArray;
				PlatformArray.Add( PlatformName );

				// if we are based of a cook and we are creating a new one we need to make sure that at least all the old packages are cooked as well as the new ones
				FString OriginalAssetRegistryPath = GetReleaseVersionAssetRegistryPath( BasedOnReleaseVersion, PlatformName ) / GetAssetRegistryFilename();

				TArray<FName> PackageFiles;
				verify( !GetAllPackageFilenamesFromAssetRegistry(OriginalAssetRegistryPath, PackageFiles) );

				for ( const FName& PackageFilename : PackageFiles )
				{
					CookRequests.EnqueueUnique( FFilePlatformRequest( PackageFilename, PlatformArray) );
				}

			}

		}
	}


	// this is to support canceling cooks from the editor
	// this is required to make sure that the cooker is in a good state after cancel occurs
	// if too many packages are being recooked after resume then we may need to figure out a different way to do this
	for ( const FFilePlatformRequest& PreviousRequest : CookByTheBookOptions->PreviousCookRequests )
	{
		CookRequests.EnqueueUnique( PreviousRequest );
	}
	CookByTheBookOptions->PreviousCookRequests.Empty();
	
	if (CookByTheBookStartupOptions.NumProcesses)
	{
		FString ExtraCommandLine;
		StartChildCookers(CookByTheBookStartupOptions.NumProcesses, TargetPlatformNames, ExtraCommandLine);
	}
}

bool UCookOnTheFlyServer::RecompileChangedShaders(const TArray<FName>& TargetPlatforms)
{
	bool bShadersRecompiled = false;
	for (const FName& TargetPlatform : TargetPlatforms)
	{
		bShadersRecompiled |= RecompileChangedShadersForPlatform(TargetPlatform.ToString());
	}
	return bShadersRecompiled;
}

class FChildCookerRunnable : public FRunnable
{
private:
	UCookOnTheFlyServer* CookServer;
	UCookOnTheFlyServer::FChildCooker* ChildCooker;

	void ProcessChildLogOutput()
	{
		// process the log output from the child cooker even if we just finished to ensure that we get the end of the log
		FString PipeContents = FPlatformProcess::ReadPipe(ChildCooker->ReadPipe);
		if (PipeContents.Len())
		{
			TArray<FString> PipeLines;
			PipeContents.ParseIntoArrayLines(PipeLines);

			for (const FString& Line : PipeLines)
			{
				UE_LOG(LogCook, Display, TEXT("Cooker output %s: %s"), *ChildCooker->BaseResponseFileName, *Line);
				if (ChildCooker->bFinished == false)
				{
					FPlatformProcess::Sleep(0.0f); // don't be greedy, log output isn't important compared to cooking performance
				}
			}
		}
	}

public:
	FChildCookerRunnable(UCookOnTheFlyServer::FChildCooker* InChildCooker, UCookOnTheFlyServer* InCookServer) : FRunnable(), CookServer(InCookServer), ChildCooker(InChildCooker)
	{ }

	
	virtual uint32 Run() override
	{
		while (true)
		{
			check(ChildCooker != nullptr);
			check(ChildCooker->bFinished == false); 

			int32 ReturnCode;
			if (FPlatformProcess::GetProcReturnCode(ChildCooker->ProcessHandle, &ReturnCode))
			{
				if (ReturnCode != 0)
				{
					UE_LOG(LogCook, Error, TEXT("Child cooker %s returned error code %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}
				else
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s returned %d"), *ChildCooker->BaseResponseFileName, ReturnCode);
				}


				// if the child cooker completed successfully then it would have output a list of files which the main cooker needs to process
				const FString AdditionalPackagesFileName = GetChildCookerResultFilename(ChildCooker->ResponseFileName);

				FString AdditionalPackages;
				if (FFileHelper::LoadFileToString(AdditionalPackages, *AdditionalPackagesFileName) == false)
				{
					UE_LOG(LogCook, Fatal, TEXT("ChildCooker failed to write out additional packages file to location %s"), *AdditionalPackagesFileName);
				}
				TArray<FString> AdditionalPackageList;
				AdditionalPackages.ParseIntoArrayLines(AdditionalPackageList);

				for (const FString& AdditionalPackage : AdditionalPackageList)
				{
					UE_LOG(LogCook, Display, TEXT("Child cooker %s requested additional package %s to be cooked"), *ChildCooker->BaseResponseFileName, *AdditionalPackage);
					FName Filename = FName(*AdditionalPackage);
					CookServer->RequestPackage(Filename, false);
				}
				ProcessChildLogOutput();

				ChildCooker->ReturnCode = ReturnCode;
				ChildCooker->bFinished = true;
				ChildCooker = nullptr;
				return 1;
			}

			ProcessChildLogOutput();

			FPlatformProcess::Sleep(0.05f);
		}
		return 1;
	}

};

// sue chefs away!!
void UCookOnTheFlyServer::StartChildCookers(int32 NumCookersToSpawn, const TArray<FName>& TargetPlatformNames, const FString& ExtraCmdParams)
{
	SCOPE_TIMER(StartingChildCookers);
	// create a comprehensive list of all the files we need to cook
	// then get the packages with least dependencies and give them to some sue chefs to handle
	
	check(!IsChildCooker());

	// PackageNames contains a sorted list of the packages we want to distribute to child cookers
	TArray<FName> PackageNames;
	// PackageNamesSet is a set of the same information for quick lookup into packagenames array
	TSet<FName> PackageNamesSet;
	PackageNames.Empty(CookRequests.Num());

	// TArray<FFilePlatformRequest>;

	

	for (const FName& CookRequest : CookRequests.GetQueue())
	{
		FString LongPackageName = FPackageName::FilenameToLongPackageName(CookRequest.ToString());

		const FName PackageFName = FName(*LongPackageName);
		PackageNames.Add(PackageFName);
		PackageNamesSet.Add(PackageFName);
	}


	int32 PackageCounter = 0;
	while (PackageCounter < PackageNames.Num())
	{
		const FName& PackageName = PackageNames[PackageCounter];
		++PackageCounter;
		TArray<FName> UnfilteredDependencies;
		AssetRegistry->GetDependencies(PackageName, UnfilteredDependencies);

		for (const FName& Dependency : UnfilteredDependencies)
		{
			if ((FPackageName::IsScriptPackage(Dependency.ToString()) == false) && (FPackageName::IsMemoryPackage(Dependency.ToString()) == false))
			{
				if (PackageNamesSet.Contains(Dependency) == false)
				{
					PackageNamesSet.Add(Dependency);
					PackageNames.Insert(Dependency, PackageCounter);
				}
			}
		}
	}

	TArray<FName> DistributeStandardFilenames;
	for (const FName& DistributeCandidate : PackageNames)
	{
		FText OutReason;
		FString LongPackageName = DistributeCandidate.ToString();
		if (!FPackageName::IsValidLongPackageName(LongPackageName, true, &OutReason))
		{
			const FText FailMessage = FText::Format(LOCTEXT("UnableToGeneratePackageName", "Unable to generate long package name for {0}. {1}"),
				FText::FromString(LongPackageName), OutReason);

			LogCookerMessage(FailMessage.ToString(), EMessageSeverity::Warning);
			UE_LOG(LogCook, Warning, TEXT("%s"), *(FailMessage.ToString()));
			continue;
		}
		else if (FPackageName::IsScriptPackage(LongPackageName)|| FPackageName::IsMemoryPackage(LongPackageName))
		{
			continue;
		}
		DistributeStandardFilenames.Add(FName(*LongPackageName));
	}

	UE_LOG(LogCook, Display, TEXT("Distributing %d packages to %d cookers for processing"), DistributeStandardFilenames.Num(), NumCookersToSpawn);

	FString TargetPlatformString;
	for (const FName& TargetPlatformName : TargetPlatformNames)
	{
		if (TargetPlatformString.Len() != 0)
		{
			TargetPlatformString += TEXT("+");
		}
		TargetPlatformString += TargetPlatformName.ToString();
	}

	// allocate the memory here, this can't change while we are running the child threads which are handling input because they have a ptr to the childcookers array
	CookByTheBookOptions->ChildCookers.Empty(NumCookersToSpawn);

	// start the child cookers and give them each some distribution candidates
	for (int32 CookerCounter = 0; CookerCounter < NumCookersToSpawn; ++CookerCounter )
	{
		// count our selves as a cooker
		int32 NumFilesForCooker = DistributeStandardFilenames.Num() / ((NumCookersToSpawn+1) - CookerCounter);

		// don't spawn a cooker unless it has a minimum amount of files to do
		if (NumFilesForCooker < 5)
		{
			continue;
		}


		int32 ChildCookerIndex = CookByTheBookOptions->ChildCookers.AddDefaulted(1);
		FChildCooker& ChildCooker = CookByTheBookOptions->ChildCookers[ChildCookerIndex];

		ChildCooker.ResponseFileName = FPaths::CreateTempFilename(*(FPaths::ProjectSavedDir() / TEXT("CookingTemp")));
		ChildCooker.BaseResponseFileName = FPaths::GetBaseFilename(ChildCooker.ResponseFileName);

		// FArchive* ResponseFile = IFileManager::CreateFileWriter(ChildCooker.UniqueTempName);
		FString ResponseFileText;

		for (int32 I = 0; I < NumFilesForCooker; ++I)
		{
			FName PackageFName = DistributeStandardFilenames[I];
			FString PackageName = PackageFName.ToString();

			ResponseFileText += FString::Printf(TEXT("%s%s"), *PackageName, LINE_TERMINATOR);

			// these are long package names
			FName StandardPackageName = GetCachedStandardPackageFileFName(PackageFName);
			if (StandardPackageName == NAME_None)
				continue;
#if 0 // validation code can be disabled
			PackageName = FPackageName::LongPackageNameToFilename(PackageName,FPaths::GetExtension(PackageName));
			FPaths::MakeStandardFilename(PackageName);
			FName PackageFName = FName(*PackageName);
			check(PackageFName== StandardPackageName);
#endif	
			TArray<bool> Succeeded;
			for ( int32 T = 0; T < TargetPlatformNames.Num(); ++T )
			{
				Succeeded.Add(true);
			}
			CookedPackages.Add(FFilePlatformCookedPackage(StandardPackageName, TargetPlatformNames, MoveTemp(Succeeded)));
		}
		DistributeStandardFilenames.RemoveAt(0, NumFilesForCooker);

		UE_LOG(LogCook, Display, TEXT("Child cooker %d working on %d files"), CookerCounter, NumFilesForCooker);

		FFileHelper::SaveStringToFile(ResponseFileText, *ChildCooker.ResponseFileName);

		// default commands
		// multiprocess tells unreal in general we shouldn't do things like save ddc, clean shader working directory, and other various multiprocess unsafe things
		FString CommandLine = FString::Printf(TEXT("\"%s\" -run=cook -multiprocess -targetplatform=%s -cookchild=\"%s\" -abslog=\"%sLog.txt\" -childIdentifier=%d %s"), 
			*FPaths::GetProjectFilePath(), *TargetPlatformString, *ChildCooker.ResponseFileName, *ChildCooker.ResponseFileName, CookerCounter, *ExtraCmdParams);

		auto KeepCommandlineValue = [&](const TCHAR* CommandlineToKeep)
		{
			FString CommandlineValue;
			if (FParse::Value(FCommandLine::Get(), CommandlineToKeep, CommandlineValue ))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
				CommandLine += TEXT("=");
				CommandLine += *CommandlineValue;
			}
		};

		auto KeepCommandlineParam = [&](const TCHAR* CommandlineToKeep)
		{
			if (FParse::Param(FCommandLine::Get(), CommandlineToKeep))
			{
				CommandLine += TEXT(" -");
				CommandLine += CommandlineToKeep;
			}
		};


		KeepCommandlineParam(TEXT("NativizeAssets"));
		KeepCommandlineValue(TEXT("ddc="));
		KeepCommandlineParam(TEXT("SkipEditorContent"));
		KeepCommandlineParam(TEXT("compressed"));
		KeepCommandlineParam(TEXT("Unversioned"));
		KeepCommandlineParam(TEXT("buildmachine"));
		KeepCommandlineParam(TEXT("fileopenlog"));
		KeepCommandlineParam(TEXT("stdout"));
		KeepCommandlineParam(TEXT("FORCELOGFLUSH"));
		KeepCommandlineParam(TEXT("CrashForUAT"));
		KeepCommandlineParam(TEXT("AllowStdOutLogVerbosity"));
		KeepCommandlineParam(TEXT("UTF8Output"));

		FString ExecutablePath = FPlatformProcess::ExecutableName(true);

		UE_LOG(LogCook, Display, TEXT("Launching cooker using commandline %s %s"), *ExecutablePath, *CommandLine);

		void* ReadPipe = NULL;
		void* WritePipe = NULL;
		FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

		ChildCooker.ProcessHandle = FPlatformProcess::CreateProc(*ExecutablePath, *CommandLine, false, true, true, NULL, 0, NULL, WritePipe);
		ChildCooker.ReadPipe = ReadPipe;

		// start threads to monitor output and finished state
		ChildCooker.Thread = FRunnableThread::Create(new FChildCookerRunnable(&ChildCooker, this), *FString::Printf(TEXT("ChildCookerInputHandleThreadFor:%s"), *ChildCooker.BaseResponseFileName));
	}

}



/* UCookOnTheFlyServer callbacks
 *****************************************************************************/

void UCookOnTheFlyServer::MaybeMarkPackageAsAlreadyLoaded(UPackage *Package)
{
	// can't use this optimization while cooking in editor
	check(IsCookingInEditor()==false);
	check(IsCookByTheBookMode());

	if (bIgnoreMarkupPackageAlreadyLoaded == true)
	{
		return;
	}

	if (bIsInitializingSandbox)
	{
		return;
	}


	// if the package is already fully loaded then we are not going to mark it up anyway
	if ( Package->IsFullyLoaded() )
	{
		return;
	}

	FName StandardName = GetCachedStandardPackageFileFName(Package);

	// UE_LOG(LogCook, Display, TEXT("Loading package %s"), *StandardName.ToString());

	bool bShouldMarkAsAlreadyProcessed = false;

	TArray<FName> CookedPlatforms;
	if (CookedPackages.GetCookedPlatforms(StandardName, CookedPlatforms))
	{
		bShouldMarkAsAlreadyProcessed = true;
		for (const FName& TargetPlatform : CookByTheBookOptions->TargetPlatformNames)
		{
			if (!CookedPlatforms.Contains(TargetPlatform))
			{
				bShouldMarkAsAlreadyProcessed = false;
				break;
			}
		}


		FString Platforms;
		for (const FName& CookedPlatform : CookedPlatforms)
		{
			Platforms += TEXT(" ");
			Platforms += CookedPlatform.ToString();
		}
		if (IsCookFlagSet(ECookInitializationFlags::LogDebugInfo))
		{
			if (!bShouldMarkAsAlreadyProcessed)
		{
				UE_LOG(LogCook, Display, TEXT("Reloading package %s slowly because it wasn't cooked for all platforms %s."), *StandardName.ToString(), *Platforms);
		}
		else
		{
				UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it's been cooked for platforms %s."), *StandardName.ToString(), *Platforms);
			}
		}
	}

	check(IsInGameThread());
	if (NeverCookPackageList.Contains(StandardName))
	{
		bShouldMarkAsAlreadyProcessed = true;
		UE_LOG(LogCook, Display, TEXT("Marking %s as reloading for cooker because it was requested as never cook package."), *StandardName.ToString());
	}

	if (bShouldMarkAsAlreadyProcessed)
	{
		if (Package->IsFullyLoaded() == false)
		{
			Package->SetPackageFlags(PKG_ReloadingForCooker);
		}
	}
}


bool UCookOnTheFlyServer::HandleNetworkFileServerNewConnection(const FString& VersionInfo, const FString& PlatformName)
{
	const uint32 CL = FEngineVersion::CompatibleWith().GetChangelist();
	const FString Branch = FEngineVersion::CompatibleWith().GetBranch();

	const FString LocalVersionInfo = FString::Printf(TEXT("%s %d"), *Branch, CL);

	UE_LOG(LogCook, Display, TEXT("Connection received of version %s local version %s"), *VersionInfo, *LocalVersionInfo);

	if (LocalVersionInfo != VersionInfo)
	{
		UE_LOG(LogCook, Warning, TEXT("Connection tried to connect with incompatable version"));
		// return false;
	}
	return true;
}

void UCookOnTheFlyServer::GetCookOnTheFlyUnsolicitedFiles(const FName& PlatformName, TArray<FString> UnsolicitedFiles, const FString& Filename)
{
	TArray<FName> UnsolicitedFilenames;
	UnsolicitedCookedPackages.GetPackagesForPlatformAndRemove(PlatformName, UnsolicitedFilenames);

	for (const FName& UnsolicitedFile : UnsolicitedFilenames)
	{
		FString StandardFilename = UnsolicitedFile.ToString();
		FPaths::MakeStandardFilename(StandardFilename);

		// check that the sandboxed file exists... if it doesn't then don't send it back
		// this can happen if the package was saved but the async writer thread hasn't finished writing it to disk yet

		FString SandboxFilename = ConvertToFullSandboxPath(*Filename, true);
		SandboxFilename.ReplaceInline(TEXT("[Platform]"), *PlatformName.ToString());
		if (IFileManager::Get().FileExists(*SandboxFilename))
		{
			UnsolicitedFiles.Add(StandardFilename);
		}
		else
		{
			UE_LOG(LogCook, Warning, TEXT("Unsolicited file doesn't exist in sandbox, ignoring %s"), *Filename);
		}

	}
	UPackage::WaitForAsyncFileWrites();
}

void UCookOnTheFlyServer::HandleNetworkFileServerFileRequest( const FString& Filename, const FString &PlatformName, TArray<FString>& UnsolicitedFiles )
{
	check(IsCookOnTheFlyMode());

	bool bIsCookable = FPackageName::IsPackageExtension(*FPaths::GetExtension(Filename, true));


	FName PlatformFname = FName(*PlatformName);

	if (!bIsCookable)
	{
		GetCookOnTheFlyUnsolicitedFiles(PlatformFname, UnsolicitedFiles, Filename);
		return;
	}

	FString StandardFileName = Filename;
	FPaths::MakeStandardFilename( StandardFileName );

	FName StandardFileFname = FName( *StandardFileName );
	TArray<FName> Platforms;
	Platforms.Add( PlatformFname );
	FFilePlatformRequest FileRequest( StandardFileFname, Platforms);

#if PROFILE_NETWORK
	double StartTime = FPlatformTime::Seconds();
	check(NetworkRequestEvent);
	NetworkRequestEvent->Reset();
#endif
	
	UE_LOG(LogCook, Display, TEXT("Requesting file from cooker %s"), *StandardFileName);

	CookRequests.EnqueueUnique(FileRequest, true);


#if PROFILE_NETWORK
	bool bFoundNetworkEventWait = true;
	while (NetworkRequestEvent->Wait(1) == false)
	{
		// for some reason we missed the stat
		if (CookedPackages.Exists(FileRequest))
		{
			double DeltaTimeTillRequestForfilled = FPlatformTime::Seconds() - StartTime;
			TimeTillRequestForfilled += DeltaTimeTillRequestForfilled;
			TimeTillRequestForfilledError += DeltaTimeTillRequestForfilled;
			StartTime = FPlatformTime::Seconds();
			bFoundNetworkEventWait = false;
			break;
		}
	}
	

	// wait for tick entry here
	TimeTillRequestStarted += FPlatformTime::Seconds() - StartTime;
	StartTime = FPlatformTime::Seconds();
#endif

	while (!CookedPackages.Exists(FileRequest))
	{
		FPlatformProcess::Sleep(0.0001f);
	}

#if PROFILE_NETWORK
	if ( bFoundNetworkEventWait )
	{
		TimeTillRequestForfilled += FPlatformTime::Seconds() - StartTime;
		StartTime = FPlatformTime::Seconds();
	}
#endif
	UE_LOG( LogCook, Display, TEXT("Cook complete %s"), *FileRequest.GetFilename().ToString())

	GetCookOnTheFlyUnsolicitedFiles(PlatformFname, UnsolicitedFiles, Filename);

#if PROFILE_NETWORK
	WaitForAsyncFilesWrites += FPlatformTime::Seconds() - StartTime;
	StartTime = FPlatformTime::Seconds();
#endif
#if DEBUG_COOKONTHEFLY
	UE_LOG( LogCook, Display, TEXT("Processed file request %s"), *Filename );
#endif

}


FString UCookOnTheFlyServer::HandleNetworkGetSandboxPath()
{
	return SandboxFile->GetSandboxDirectory();
}

void UCookOnTheFlyServer::HandleNetworkGetPrecookedList(const FString& PlatformName, TMap<FString, FDateTime>& PrecookedFileList)
{
	FName PlatformFName = FName(*PlatformName);

	TArray<FName> CookedPlatformFiles;
	CookedPackages.GetCookedFilesForPlatform(PlatformFName, CookedPlatformFiles);


	for ( const FName& CookedFile : CookedPlatformFiles)
	{
		const FString SandboxFilename = ConvertToFullSandboxPath(CookedFile.ToString(), true, PlatformName);
		if (IFileManager::Get().FileExists(*SandboxFilename))
		{
			continue;
		}

		PrecookedFileList.Add(CookedFile.ToString(),FDateTime::MinValue());
	}
}


void UCookOnTheFlyServer::HandleNetworkFileServerRecompileShaders(const FShaderRecompileData& RecompileData)
{
	// shouldn't receive network requests unless we are in cook on the fly mode
	check( IsCookOnTheFlyMode() );
	check( !IsCookingDLC() );
	// if we aren't in the game thread, we need to push this over to the game thread and wait for it to finish
	if (!IsInGameThread())
	{
		UE_LOG(LogCook, Display, TEXT("Got a recompile request on non-game thread"));

		// make a new request
		FRecompileRequest* Request = new FRecompileRequest;
		Request->RecompileData = RecompileData;
		Request->bComplete = false;

		// push the request for the game thread to process
		RecompileRequests.Enqueue(Request);

		// wait for it to complete (the game thread will pull it out of the TArray, but I will delete it)
		while (!Request->bComplete)
		{
			FPlatformProcess::Sleep(0);
		}
		delete Request;
		UE_LOG(LogCook, Display, TEXT("Completed recompile..."));

		// at this point, we are done on the game thread, and ModifiedFiles will have been filled out
		return;
	}

	FString OutputDir = GetSandboxDirectory(RecompileData.PlatformName);

	RecompileShadersForRemote
		(RecompileData.PlatformName, 
		RecompileData.ShaderPlatform == -1 ? SP_NumPlatforms : (EShaderPlatform)RecompileData.ShaderPlatform,
		OutputDir, 
		RecompileData.MaterialsToLoad, 
		RecompileData.SerializedShaderResources, 
		RecompileData.MeshMaterialMaps, 
		RecompileData.ModifiedFiles,
		RecompileData.bCompileChangedShaders);
}

bool UCookOnTheFlyServer::GetAllPackageFilenamesFromAssetRegistry( const FString& AssetRegistryPath, TArray<FName>& OutPackageFilenames ) const
{
	FArrayReader SerializedAssetData;
	if (FFileHelper::LoadFileToArray(SerializedAssetData, *AssetRegistryPath))
	{
		FAssetRegistryState TempState;
		FAssetRegistrySerializationOptions LoadOptions;
		LoadOptions.bSerializeDependencies = false;
		LoadOptions.bSerializePackageData = false;

		TempState.Serialize(SerializedAssetData, LoadOptions);

		const TMap<FName, const FAssetData*>& RegistryDataMap = TempState.GetObjectPathToAssetDataMap();

		for (const TPair<FName, const FAssetData*>& RegistryData : RegistryDataMap)
		{
			const FAssetData* NewAssetData = RegistryData.Value;
			FName CachedPackageFileFName = GetCachedStandardPackageFileFName(NewAssetData->ObjectPath);
			if (CachedPackageFileFName != NAME_None)
			{
				OutPackageFilenames.Add(CachedPackageFileFName);
			}
			else
			{
				UE_LOG(LogCook, Warning, TEXT("Could not resolve package %s from %s"), *NewAssetData->ObjectPath.ToString(), *AssetRegistryPath);
			}
		}
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

