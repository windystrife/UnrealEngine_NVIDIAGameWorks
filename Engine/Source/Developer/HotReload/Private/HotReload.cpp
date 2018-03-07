// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "Misc/OutputDeviceNull.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Containers/Ticker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "IHotReload.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "HotReloadLog.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "IPluginManager.h"
#include "DesktopPlatformModule.h"
#if WITH_ENGINE
#include "Engine/Engine.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "HotReloadClassReinstancer.h"
#include "EngineAnalytics.h"
#endif
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY(LogHotReload);

#define LOCTEXT_NAMESPACE "HotReload"

namespace EThreeStateBool
{
	enum Type
	{
		False,
		True,
		Unknown
	};

	static bool ToBool(EThreeStateBool::Type Value)
	{
		switch (Value)
		{
		case EThreeStateBool::False:
			return false;
		case EThreeStateBool::True:
			return true;
		default:
			UE_LOG(LogHotReload, Fatal, TEXT("Can't convert EThreeStateBool to bool value because it's Unknown"));			
			break;
		}
		return false;
	}

	static EThreeStateBool::Type FromBool(bool Value)
	{
		return Value ? EThreeStateBool::True : EThreeStateBool::False;
	}
};

/**
 * Module for HotReload support
 */
class FHotReloadModule : public IHotReloadModule, FSelfRegisteringExec
{
public:

	FHotReloadModule()
	{
		ModuleCompileReadPipe = nullptr;
		bRequestCancelCompilation = false;
		bIsAnyGameModuleLoaded = EThreeStateBool::Unknown;
		bDirectoryWatcherInitialized = false;
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** FSelfRegisteringExec implementation */
	virtual bool Exec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar ) override;

	/** IHotReloadInterface implementation */
	virtual void Tick() override;
	virtual void SaveConfig() override;
	virtual bool RecompileModule(const FName InModuleName, const bool bReloadAfterRecompile, FOutputDevice &Ar, bool bFailIfGeneratedCodeChanges = true, bool bForceCodeProject = false) override;
	virtual bool IsCurrentlyCompiling() const override { return ModuleCompileProcessHandle.IsValid(); }
	virtual void RequestStopCompilation() override { bRequestCancelCompilation = true; }
	virtual void AddHotReloadFunctionRemap(Native NewFunctionPointer, Native OldFunctionPointer) override;	
	virtual ECompilationResult::Type RebindPackages(TArray< UPackage* > Packages, TArray< FName > DependentModules, const bool bWaitForCompletion, FOutputDevice &Ar) override;
	virtual ECompilationResult::Type DoHotReloadFromEditor(const bool bWaitForCompletion) override;
	virtual FHotReloadEvent& OnHotReload() override { return HotReloadEvent; }	
	virtual FModuleCompilerStartedEvent& OnModuleCompilerStarted() override { return ModuleCompilerStartedEvent; }
	virtual FModuleCompilerFinishedEvent& OnModuleCompilerFinished() override { return ModuleCompilerFinishedEvent; }
	virtual FString GetModuleCompileMethod(FName InModuleName) override;
	virtual bool IsAnyGameModuleLoaded() override;

private:
	/**
	 * Enumerates compilation methods for modules.
	 */
	enum class EModuleCompileMethod
	{
		Runtime,
		External,
		Unknown
	};

	/**
	 * Helper structure to hold on to module state while asynchronously recompiling DLLs
	 */
	struct FModuleToRecompile
	{
		/** Name of the module */
		FString ModuleName;

		/** Desired module file name suffix, or empty string if not needed */
		FString ModuleFileSuffix;

		/** The module file name to use after a compilation succeeds, or an empty string if not changing */
		FString NewModuleFilename;
	};

	/**
	 * Helper structure to store the compile time and method for a module
	 */
	struct FModuleCompilationData
	{
		/** Has a timestamp been set for the .dll file */
		bool bHasFileTimeStamp;

		/** Last known timestamp for the .dll file */
		FDateTime FileTimeStamp;

		/** Last known compilation method of the .dll file */
		EModuleCompileMethod CompileMethod;

		FModuleCompilationData()
			: bHasFileTimeStamp(false)
			, CompileMethod(EModuleCompileMethod::Unknown)
		{ }
	};

	/**
	 * Adds a callback to directory watcher for the game binaries folder.
	 */
	void RefreshHotReloadWatcher();

	/**
	 * Adds a directory watch on the binaries directory under the given folder.
	 */
	void AddHotReloadDirectory(IDirectoryWatcher* DirectoryWatcher, const FString& BaseDir);

	/**
	 * Removes a directory watcher callback
	 */
	void ShutdownHotReloadWatcher();

	/**
	 * Performs hot-reload from IDE (when game DLLs change)
	 */
	void DoHotReloadFromIDE();

	/**
	* Performs internal module recompilation
	*/
	ECompilationResult::Type RebindPackagesInternal(TArray<UPackage*>&& Packages, TArray<FName>&& DependentModules, const bool bWaitForCompletion, FOutputDevice& Ar);

	/**
	 * Does the actual hot-reload, unloads old modules, loads new ones
	 */
	ECompilationResult::Type DoHotReloadInternal(const TMap<FString, FString>& ChangedModuleNames, const TArray<UPackage*>& Packages, const TArray<FName>& InDependentModules, FOutputDevice& HotReloadAr);

	/**
	 * Finds all references to old CDOs and replaces them with the new ones.
	 * Skipping UBlueprintGeneratedClass::OverridenArchetypeForCDO as it's the
	 * only one needed.
	 */
	void ReplaceReferencesToReconstructedCDOs();

#if WITH_ENGINE
	void RegisterForReinstancing(UClass* OldClass, UClass* NewClass);
	void ReinstanceClasses();

	/**
	 * Called from CoreUObject to re-instance hot-reloaded classes
	 */
	void ReinstanceClass(UClass* OldClass, UClass* NewClass, const TMap<UClass*, UClass*>& OldToNewClassesMap);
#endif

	/**
	 * Tick function for FTicker: checks for re-loaded modules and does hot-reload from IDE
	 */
	bool Tick(float DeltaTime);

	/**
	 * Directory watcher callback
	 */
	void OnHotReloadBinariesChanged(const TArray<struct FFileChangeData>& FileChanges);

	/**
	 * Strips hot-reload suffix from module filename.
	 */
	static void StripModuleSuffixFromFilename(FString& InOutModuleFilename, const FString& ModuleName);

	/**
	 * Sends analytics event about the re-load
	 */
	static void RecordAnalyticsEvent(const TCHAR* ReloadFrom, ECompilationResult::Type Result, double Duration, int32 PackageCount, int32 DependentModulesCount);

	/**
	 * Declares a function type that is executed after a module recompile has finished.
	 *
	 * ChangedModules: A map between the names of the modules that have changed and their filenames.
	 * bRecompileFinished: Signals whether compilation has finished.
	 * CompilationResult: Shows whether compilation was successful or not.
	 */
	typedef TFunction<void(const TMap<FString, FString>& ChangedModules, bool bRecompileFinished, ECompilationResult::Type CompilationResult)> FRecompileModulesCallback;

	/**
	 * Tries to recompile the specified modules in the background.  When recompiling finishes, the specified callback
	 * delegate will be triggered, passing along a bool that tells you whether the compile action succeeded.  This
	 * function never tries to unload modules or to reload the modules after they finish compiling.  You should do
	 * that yourself in the recompile completion callback!
	 *
	 * @param ModuleNames Names of the modules to recompile
	 * @param RecompileModulesCallback Callback function to execute after compilation finishes (whether successful or not.)
	 * @param bWaitForCompletion True if the function should not return until recompilation attempt has finished and callbacks have fired
	 * @param Ar Output device for logging compilation status
	 * @return	True if the recompile action was kicked off successfully.  If this returns false, then the recompile callback will never fire.  In the case where bWaitForCompletion=false, this will also return false if the compilation failed for any reason.
	 */
	bool RecompileModulesAsync( const TArray< FName > ModuleNames, FRecompileModulesCallback&& InRecompileModulesCallback, const bool bWaitForCompletion, FOutputDevice &Ar );

	/** Called for successfully re-complied module */
	void OnModuleCompileSucceeded(FName ModuleName, const FString& NewModuleFilename);

	/**
	 * Tries to recompile the specified DLL using UBT. Does not interact with modules. This is a low level routine.
	 *
	 * @param ModuleNames List of modules to recompile, including the module name and optional file suffix.
	 * @param Ar Output device for logging compilation status.
	 * @param bForceCodeProject Even if it's a non-code project, treat it as code-based project
	 */
	bool RecompileModuleDLLs(const TArray< FModuleToRecompile >& ModuleNames, FOutputDevice& Ar, bool bFailIfGeneratedCodeChanges, bool bForceCodeProject);

	/** Returns arguments to pass to UnrealBuildTool when compiling modules */
	static FString MakeUBTArgumentsForModuleCompiling();

	/** 
	 *	Starts compiling DLL files for one or more modules.
	 *
	 *	@param GameName The name of the game.
	 *	@param ModuleNames The list of modules to compile.
	 *	@param InRecompileModulesCallback Callback function to make when module recompiles.
	 *	@param Ar
	 *	@param bInFailIfGeneratedCodeChanges If true, fail the compilation if generated headers change.
	 *	@param InAdditionalCmdLineArgs Additional arguments to pass to UBT.
	 *  @param bForceCodeProject Compile as code-based project even if there's no game modules loaded
	 *	@return true if successful, false otherwise.
	 */
	bool StartCompilingModuleDLLs(const FString& GameName, const TArray< FModuleToRecompile >& ModuleNames, 
		FRecompileModulesCallback&& InRecompileModulesCallback, FOutputDevice& Ar, bool bInFailIfGeneratedCodeChanges, 
		const FString& InAdditionalCmdLineArgs, bool bForceCodeProject);

	/** Launches UnrealBuildTool with the specified command line parameters */
	bool InvokeUnrealBuildToolForCompile(const FString& InCmdLineParams, FOutputDevice &Ar);

	/** Checks to see if a pending compilation action has completed and optionally waits for it to finish.  If completed, fires any appropriate callbacks and reports status provided bFireEvents is true. */
	void CheckForFinishedModuleDLLCompile(const bool bWaitForCompletion, bool& bCompileStillInProgress, bool& bCompileSucceeded, FOutputDevice& Ar, bool bFireEvents = true);

	/** Called when the compile data for a module need to be update in memory and written to config */
	void UpdateModuleCompileData(FName ModuleName);

	/** Called when a new module is added to the manager to get the saved compile data from config */
	static void ReadModuleCompilationInfoFromConfig(FName ModuleName, FModuleCompilationData& CompileData);

	/** Saves the module's compile data to config */
	static void WriteModuleCompilationInfoToConfig(FName ModuleName, const FModuleCompilationData& CompileData);

	/** Access the module's file and read the timestamp from the file system. Returns true if the timestamp was read successfully. */
	bool GetModuleFileTimeStamp(FName ModuleName, FDateTime& OutFileTimeStamp) const;

	/** Checks if the specified array of modules to recompile contains only game modules */
	bool ContainsOnlyGameModules(const TArray< FModuleToRecompile >& ModuleNames) const;

	/** Callback registered with ModuleManager to know if any new modules have been loaded */
	void ModulesChangedCallback(FName ModuleName, EModuleChangeReason ReasonForChange);

	/** Callback registered with PluginManager to know if any new plugins have been created */
	void PluginMountedCallback(IPlugin& Plugin);

	/** FTicker delegate (hot-reload from IDE) */
	FTickerDelegate TickerDelegate;

	/** Handle to the registered TickerDelegate */
	FDelegateHandle TickerDelegateHandle;

	/** Handle to the registered delegate above */
	TMap<FString, FDelegateHandle> BinariesFolderChangedDelegateHandles;

	/** True if currently hot-reloading from editor (suppresses hot-reload from IDE) */
	bool bIsHotReloadingFromEditor;
	
	/** New module DLLs */
	TMap<FString, FString> NewModules;

	/** Moduels that have been recently recompiled from the editor **/
	TSet<FString> ModulesRecentlyCompiledInTheEditor;

	/** Delegate broadcast when a module has been hot-reloaded */
	FHotReloadEvent HotReloadEvent;

	/** Array of modules that we're currently recompiling */
	TArray< FModuleToRecompile > ModulesBeingCompiled;

	/** Array of modules that we're going to recompile */
	TArray< FModuleToRecompile > ModulesThatWereBeingRecompiled;

	/** Last known compilation data for each module */
	TMap<FName, TSharedRef<FModuleCompilationData>> ModuleCompileData;

	/** Multicast delegate which will broadcast a notification when the compiler starts */
	FModuleCompilerStartedEvent ModuleCompilerStartedEvent;
	
	/** Multicast delegate which will broadcast a notification when the compiler finishes */
	FModuleCompilerFinishedEvent ModuleCompilerFinishedEvent;

	/** When compiling a module using an external application, stores the handle to the process that is running */
	FProcHandle ModuleCompileProcessHandle;

	/** When compiling a module using an external application, this is the process read pipe handle */
	void* ModuleCompileReadPipe;

	/** When compiling a module using an external application, this is the text that was read from the read pipe handle */
	FString ModuleCompileReadPipeText;

	/** Callback to execute after an asynchronous recompile has completed (whether successful or not.) */
	FRecompileModulesCallback RecompileModulesCallback;

	/** true if we should attempt to cancel the current async compilation */
	bool bRequestCancelCompilation;

	/** Tracks the validity of the game module existence */
	EThreeStateBool::Type bIsAnyGameModuleLoaded;

	/** True if the directory watcher has been successfully initialized */
	bool bDirectoryWatcherInitialized;

	/** Reconstructed CDOs map during hot-reload. */
	TMap<UObject*, UObject*> ReconstructedCDOsMap;

	/** Keeps record of hot-reload session starting time. */
	double HotReloadStartTime;
};

namespace HotReloadDefs
{
	const static FString CompilationInfoConfigSection("ModuleFileTracking");

	// These strings should match the values of the enum EModuleCompileMethod in ModuleManager.h
	// and should be handled in ReadModuleCompilationInfoFromConfig() & WriteModuleCompilationInfoToConfig() below
	const static FString CompileMethodRuntime("Runtime");
	const static FString CompileMethodExternal("External");
	const static FString CompileMethodUnknown("Unknown");

	// Add one minute epsilon to timestamp comparision
	const static FTimespan TimeStampEpsilon(0, 1, 0);
}

IMPLEMENT_MODULE(FHotReloadModule, HotReload);

namespace UE4HotReload_Private
{
	/**
	 * Gets editor runs directory.
	 */
	FString GetEditorRunsDir()
	{
		FString TempDir = FPaths::EngineIntermediateDir();

		return FPaths::Combine(*TempDir, TEXT("EditorRuns"));
	}

	/**
	 * Creates a file that informs UBT that the editor is currently running.
	 */
	void CreateFileThatIndicatesEditorRunIfNeeded()
	{
#if WITH_EDITOR
		IPlatformFile& FS = IPlatformFile::GetPlatformPhysical();

		FString EditorRunsDir = GetEditorRunsDir();
		FString FileName = FPaths::Combine(*EditorRunsDir, *FString::Printf(TEXT("%d"), FPlatformProcess::GetCurrentProcessId()));

		if (FS.FileExists(*FileName))
		{
			if (!GIsEditor)
			{
				FS.DeleteFile(*FileName);
			}
		}
		else
		{
			if (GIsEditor)
			{
				if (!FS.CreateDirectory(*EditorRunsDir))
				{
					return;
				}

				delete FS.OpenWrite(*FileName); // Touch file.
			}
		}
#endif // WITH_EDITOR
	}

	/**
	 * Deletes file left by CreateFileThatIndicatesEditorRunIfNeeded function.
	 */
	void DeleteFileThatIndicatesEditorRunIfNeeded()
	{
#if WITH_EDITOR
		IPlatformFile& FS = IPlatformFile::GetPlatformPhysical();

		FString EditorRunsDir = GetEditorRunsDir();
		FString FileName = FPaths::Combine(*EditorRunsDir, *FString::Printf(TEXT("%d"), FPlatformProcess::GetCurrentProcessId()));

		if (FS.FileExists(*FileName))
		{
			FS.DeleteFile(*FileName);
		}
#endif // WITH_EDITOR
	}

	/**
	 * Gets all currently loaded game module names and optionally, the file names for those modules
	 */
	TArray<FString> GetGameModuleNames(const FModuleManager& ModuleManager)
	{
		TArray<FString> Result;

		// Ask the module manager for a list of currently-loaded gameplay modules
		TArray<FModuleStatus> ModuleStatuses;
		ModuleManager.QueryModules(ModuleStatuses);

		for (FModuleStatus& ModuleStatus : ModuleStatuses)
		{
			// We only care about game modules that are currently loaded
			if (ModuleStatus.bIsLoaded && ModuleStatus.bIsGameModule)
			{
				Result.Add(MoveTemp(ModuleStatus.Name));
			}
		}

		return Result;
	}

	/**
	 * Gets all currently loaded game module names and optionally, the file names for those modules
	 */
	TMap<FString, FString> GetGameModuleFilenames(const FModuleManager& ModuleManager)
	{
		TMap<FString, FString> Result;

		// Ask the module manager for a list of currently-loaded gameplay modules
		TArray< FModuleStatus > ModuleStatuses;
		ModuleManager.QueryModules(ModuleStatuses);

		for (FModuleStatus& ModuleStatus : ModuleStatuses)
		{
			// We only care about game modules that are currently loaded
			if (ModuleStatus.bIsLoaded && ModuleStatus.bIsGameModule)
			{
				Result.Add(MoveTemp(ModuleStatus.Name), MoveTemp(ModuleStatus.FilePath));
			}
		}

		return Result;
	}

	struct FPackagesAndDependentNames
	{
		TArray<UPackage*> Packages;
		TArray<FName> DependentNames;
	};

	/**
	 * Gets named packages and the names dependents.
	 */
	FPackagesAndDependentNames SplitByPackagesAndDependentNames(const TArray<FString>& ModuleNames)
	{
		FPackagesAndDependentNames Result;

		for (const FString& ModuleName : ModuleNames)
		{
			FString PackagePath = TEXT("/Script/") + ModuleName;

			if (UPackage* Package = FindPackage(nullptr, *PackagePath))
			{
				Result.Packages.Add(Package);
			}
			else
			{
				Result.DependentNames.Add(*ModuleName);
			}
		}

		return Result;
	}
}

void FHotReloadModule::StartupModule()
{
	UE4HotReload_Private::CreateFileThatIndicatesEditorRunIfNeeded();

	bIsHotReloadingFromEditor = false;

#if WITH_ENGINE
	// Register re-instancing delegate (Core)
	FCoreUObjectDelegates::RegisterClassForHotReloadReinstancingDelegate.AddRaw(this, &FHotReloadModule::RegisterForReinstancing);
	FCoreUObjectDelegates::ReinstanceHotReloadedClassesDelegate.AddRaw(this, &FHotReloadModule::ReinstanceClasses);
#endif

	// Register directory watcher delegate
	RefreshHotReloadWatcher();

	// Register hot-reload from IDE ticker
	TickerDelegate = FTickerDelegate::CreateRaw(this, &FHotReloadModule::Tick);
	TickerDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickerDelegate);

	FModuleManager::Get().OnModulesChanged().AddRaw(this, &FHotReloadModule::ModulesChangedCallback);

	IPluginManager::Get().OnNewPluginMounted().AddRaw(this, &FHotReloadModule::PluginMountedCallback);
}

void FHotReloadModule::ShutdownModule()
{
	FTicker::GetCoreTicker().RemoveTicker(TickerDelegateHandle);
	ShutdownHotReloadWatcher();

	UE4HotReload_Private::DeleteFileThatIndicatesEditorRunIfNeeded();
}

bool FHotReloadModule::Exec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !UE_BUILD_SHIPPING
	if ( FParse::Command( &Cmd, TEXT( "Module" ) ) )
	{
#if WITH_HOT_RELOAD
		// Recompile <ModuleName>
		if( FParse::Command( &Cmd, TEXT( "Recompile" ) ) )
		{
			const FString ModuleNameStr = FParse::Token( Cmd, 0 );
			if( !ModuleNameStr.IsEmpty() )
			{
				const FName ModuleName( *ModuleNameStr );
				const bool bReloadAfterRecompile = true;
				const bool bForceCodeProject = false;
				const bool bFailIfGeneratedCodeChanges = true;
				RecompileModule( ModuleName, bReloadAfterRecompile, Ar, bFailIfGeneratedCodeChanges, bForceCodeProject);
			}

			return true;
		}
#endif // WITH_HOT_RELOAD
	}
#endif // !UE_BUILD_SHIPPING
	return false;
}

void FHotReloadModule::Tick()
{
	// We never want to block on a pending compile when checking compilation status during Tick().  We're
	// just checking so that we can fire callbacks if and when compilation has finished.
	const bool bWaitForCompletion = false;

	// Ignored output variables
	bool bCompileStillInProgress = false;
	bool bCompileSucceeded = false;
	FOutputDeviceNull NullOutput;
	CheckForFinishedModuleDLLCompile( bWaitForCompletion, bCompileStillInProgress, bCompileSucceeded, NullOutput );
}
	
void FHotReloadModule::SaveConfig()
{
	// Find all the modules
	TArray<FModuleStatus> Modules;
	FModuleManager::Get().QueryModules(Modules);

	// Update the compile data for each one
	for( const FModuleStatus &Module : Modules )
	{
		UpdateModuleCompileData(*Module.Name);
	}
}

FString FHotReloadModule::GetModuleCompileMethod(FName InModuleName)
{
	if (!ModuleCompileData.Contains(InModuleName))
	{
		UpdateModuleCompileData(InModuleName);
	}

	switch(ModuleCompileData.FindChecked(InModuleName).Get().CompileMethod)
	{
	case EModuleCompileMethod::External:
		return HotReloadDefs::CompileMethodExternal;
	case EModuleCompileMethod::Runtime:
		return HotReloadDefs::CompileMethodRuntime;
	default:
		return HotReloadDefs::CompileMethodUnknown;
	}
}

bool FHotReloadModule::RecompileModule(const FName InModuleName, const bool bReloadAfterRecompile, FOutputDevice &Ar, bool bFailIfGeneratedCodeChanges, bool bForceCodeProject)
{
#if WITH_HOT_RELOAD
	UE_LOG(LogHotReload, Log, TEXT("Recompiling module %s..."), *InModuleName.ToString());

	// This is an internal request for hot-reload (not from IDE)
	bIsHotReloadingFromEditor = true;
	// A list of modules that have been recompiled in the editor is going to prevent false
	// hot-reload from IDE events as this call is blocking any potential callbacks coming from the filesystem
	// and bIsHotReloadingFromEditor may not be enough to prevent those from being treated as actual hot-reload from IDE modules
	ModulesRecentlyCompiledInTheEditor.Empty();


	FFormatNamedArguments Args;
	Args.Add( TEXT("CodeModuleName"), FText::FromName( InModuleName ) );
	const FText StatusUpdate = FText::Format( NSLOCTEXT("ModuleManager", "Recompile_SlowTaskName", "Compiling {CodeModuleName}..."), Args );

	FScopedSlowTask SlowTask(2, StatusUpdate);
	SlowTask.MakeDialog();

	ModuleCompilerStartedEvent.Broadcast(false); // we never perform an async compile

	FModuleManager& ModuleManager = FModuleManager::Get();

	// Update our set of known modules, in case we don't already know about this module
	ModuleManager.AddModule( InModuleName );

	// Only use rolling module names if the module was already loaded into memory.  This allows us to try compiling
	// the module without actually having to unload it first.
	const bool bWasModuleLoaded = ModuleManager.IsModuleLoaded( InModuleName );
	const bool bUseRollingModuleNames = bWasModuleLoaded;

	SlowTask.EnterProgressFrame();

	bool bWasSuccessful = true;
	if( bUseRollingModuleNames )
	{
		// First, try to compile the module.  If the module is already loaded, we won't unload it quite yet.  Instead
		// make sure that it compiles successfully.

		// Find a unique file name for the module
		FString UniqueSuffix;
		FString UniqueModuleFileName;
		ModuleManager.MakeUniqueModuleFilename( InModuleName, UniqueSuffix, UniqueModuleFileName );

		TArray< FModuleToRecompile > ModulesToRecompile;
		FModuleToRecompile ModuleToRecompile;
		ModuleToRecompile.ModuleName = InModuleName.ToString();
		ModuleToRecompile.ModuleFileSuffix = UniqueSuffix;
		ModuleToRecompile.NewModuleFilename = UniqueModuleFileName;
		ModulesToRecompile.Add( ModuleToRecompile );
		ModulesRecentlyCompiledInTheEditor.Add(FPaths::ConvertRelativePathToFull(UniqueModuleFileName));
		bWasSuccessful = RecompileModuleDLLs(ModulesToRecompile, Ar, bFailIfGeneratedCodeChanges, bForceCodeProject);		
	}

	SlowTask.EnterProgressFrame();
	
	if( bWasSuccessful )
	{
		// Shutdown the module if it's already running
		if( bWasModuleLoaded )
		{
			Ar.Logf( TEXT( "Unloading module before compile." ) );
			ModuleManager.UnloadOrAbandonModuleWithCallback( InModuleName, Ar );
		}

		if( !bUseRollingModuleNames )
		{
			// Try to recompile the DLL
			TArray< FModuleToRecompile > ModulesToRecompile;
			FModuleToRecompile ModuleToRecompile;
			ModuleToRecompile.ModuleName = InModuleName.ToString();			
			if (ModuleManager.IsModuleLoaded(InModuleName))
			{
				ModulesRecentlyCompiledInTheEditor.Add(FPaths::ConvertRelativePathToFull(ModuleManager.GetModuleFilename(InModuleName)));
			}
			else
			{
				ModuleToRecompile.NewModuleFilename = ModuleManager.GetGameBinariesDirectory() / FModuleManager::GetCleanModuleFilename(InModuleName, true);
				ModulesRecentlyCompiledInTheEditor.Add(FPaths::ConvertRelativePathToFull(ModuleToRecompile.NewModuleFilename));
			}
			ModulesToRecompile.Add( ModuleToRecompile );
			bWasSuccessful = RecompileModuleDLLs(ModulesToRecompile, Ar, bFailIfGeneratedCodeChanges, bForceCodeProject);
		}

		// Reload the module if it was loaded before we recompiled
		if( bWasSuccessful && (bWasModuleLoaded || bForceCodeProject) && bReloadAfterRecompile )
		{
			TGuardValue<bool> GuardIsHotReload(GIsHotReload, true);
			Ar.Logf( TEXT( "Reloading module %s after successful compile." ), *InModuleName.ToString() );
			bWasSuccessful = ModuleManager.LoadModuleWithCallback( InModuleName, Ar );
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	if (bForceCodeProject && bWasSuccessful)
	{
		HotReloadEvent.Broadcast( false );
	}

	bIsHotReloadingFromEditor = false;	

	return bWasSuccessful;
#else
	return false;
#endif // WITH_HOT_RELOAD
}

/** Type hash for a UObject Function Pointer, maybe not a great choice, but it should be sufficient for the needs here. **/
inline uint32 GetTypeHash(Native A)
{
	return *(uint32*)&A;
}

/** Map from old function pointer to new function pointer for hot reload. */
static TMap<Native, Native> HotReloadFunctionRemap;

static TSet<UBlueprint*> HotReloadBPSetToRecompile;
static TSet<UBlueprint*> HotReloadBPSetToRecompileBytecodeOnly;

/** Adds and entry for the UFunction native pointer remap table */
void FHotReloadModule::AddHotReloadFunctionRemap(Native NewFunctionPointer, Native OldFunctionPointer)
{
	Native OtherNewFunction = HotReloadFunctionRemap.FindRef(OldFunctionPointer);
	check(!OtherNewFunction || OtherNewFunction == NewFunctionPointer);
	check(NewFunctionPointer);
	check(OldFunctionPointer);
	HotReloadFunctionRemap.Add(OldFunctionPointer, NewFunctionPointer);
}

ECompilationResult::Type FHotReloadModule::DoHotReloadFromEditor(const bool bWaitForCompletion)
{
	// Get all game modules we want to compile
	const FModuleManager& ModuleManager = FModuleManager::Get();
	TArray<FString> GameModuleNames = UE4HotReload_Private::GetGameModuleNames(ModuleManager);

	int32 NumPackagesToRebind = 0;
	int32 NumDependentModules = 0;

	ECompilationResult::Type Result = ECompilationResult::Unsupported;

	// Analytics
	double Duration = 0.0;

	if (GameModuleNames.Num() > 0)
	{
		FScopedDurationTimer Timer(Duration);

		UE4HotReload_Private::FPackagesAndDependentNames PackagesAndDependentNames = UE4HotReload_Private::SplitByPackagesAndDependentNames(GameModuleNames);

		NumPackagesToRebind = PackagesAndDependentNames.Packages.Num();
		NumDependentModules = PackagesAndDependentNames.DependentNames.Num();
		Result = RebindPackagesInternal(MoveTemp(PackagesAndDependentNames.Packages), MoveTemp(PackagesAndDependentNames.DependentNames), bWaitForCompletion, *GLog);
	}

	RecordAnalyticsEvent(TEXT("Editor"), Result, Duration, NumPackagesToRebind, NumDependentModules);

	return Result;
}

ECompilationResult::Type FHotReloadModule::DoHotReloadInternal(const TMap<FString, FString>& ChangedModules, const TArray<UPackage*>& Packages, const TArray<FName>& InDependentModules, FOutputDevice& HotReloadAr)
{
#if WITH_HOT_RELOAD

	FModuleManager& ModuleManager = FModuleManager::Get();

	ModuleManager.ResetModulePathsCache();

	
	FFeedbackContext& ErrorsFC = UClass::GetDefaultPropertiesFeedbackContext();
	ErrorsFC.ClearWarningsAndErrors();

	// Rebind the hot reload DLL 
	TGuardValue<bool> GuardIsHotReload(GIsHotReload, true);
	TGuardValue<bool> GuardIsInitialLoad(GIsInitialLoad, true);
	HotReloadFunctionRemap.Empty(); // redundant

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); // we create a new CDO in the transient package...this needs to go away before we try again.

	// Load the new modules up
	bool bReloadSucceeded = false;
	ECompilationResult::Type Result = ECompilationResult::Unsupported;
	for (UPackage* Package : Packages)
	{
		FString PackageName = Package->GetName();
		FString ShortPackageName = FPackageName::GetShortName(PackageName);

		if (!ChangedModules.Contains(ShortPackageName))
		{
			continue;
		}

		FName ShortPackageFName = *ShortPackageName;

		// Abandon the old module.  We can't unload it because various data structures may be living
		// that have vtables pointing to code that would become invalidated.
		ModuleManager.AbandonModuleWithCallback(ShortPackageFName);

		// Load the newly-recompiled module up (it will actually have a different DLL file name at this point.)
		bReloadSucceeded = ModuleManager.LoadModule(ShortPackageFName) != nullptr;
		if (!bReloadSucceeded)
		{
			HotReloadAr.Logf(ELogVerbosity::Warning, TEXT("HotReload failed, reload failed %s."), *PackageName);
			Result = ECompilationResult::OtherCompilationError;
			break;
		}
	}

	// Load dependent modules.
	for (FName ModuleName : InDependentModules)
	{
		FString ModuleNameStr = ModuleName.ToString();
		if (!ChangedModules.Contains(ModuleNameStr))
		{
			continue;
		}

		ModuleManager.UnloadOrAbandonModuleWithCallback(ModuleName, HotReloadAr);
		const bool bLoaded = ModuleManager.LoadModuleWithCallback(ModuleName, HotReloadAr);
		if (!bLoaded)
		{
			HotReloadAr.Logf(ELogVerbosity::Warning, TEXT("Unable to reload module %s"), *ModuleName.GetPlainNameString());
		}
	}

	if (ErrorsFC.GetNumErrors() || ErrorsFC.GetNumWarnings())
	{
		TArray<FString> AllErrorsAndWarnings;
		ErrorsFC.GetErrorsAndWarningsAndEmpty(AllErrorsAndWarnings);

		FString AllInOne;
		for (const FString& ErrorOrWarning : AllErrorsAndWarnings)
		{
			AllInOne += ErrorOrWarning;
			AllInOne += TEXT("\n");
		}
		HotReloadAr.Logf(ELogVerbosity::Warning, TEXT("Some classes could not be reloaded:\n%s"), *AllInOne);
	}

	if (bReloadSucceeded)
	{
		int32 NumFunctionsRemapped = 0;
		// Remap all native functions (and gather scriptstructs)
		TArray<UScriptStruct*> ScriptStructs;
		for (FRawObjectIterator It; It; ++It)
		{
			if (UFunction* Function = Cast<UFunction>(static_cast<UObject*>(It->Object)))
			{
				if (Native NewFunction = HotReloadFunctionRemap.FindRef(Function->GetNativeFunc()))
				{
					++NumFunctionsRemapped;
					Function->SetNativeFunc(NewFunction);
				}
			}

			if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(static_cast<UObject*>(It->Object)))
			{
				if (Packages.ContainsByPredicate([=](UPackage* Package) { return ScriptStruct->IsIn(Package); }) && !ScriptStruct->HasAnyFlags(RF_ClassDefaultObject) && ScriptStruct->GetCppStructOps())
				{
					ScriptStructs.Add(ScriptStruct);
				}
			}
		}
		// now let's set up the script structs...this relies on super behavior, so null them all, then set them all up. Internally this sets them up hierarchically.
		for (UScriptStruct* Script : ScriptStructs)
		{
			Script->ClearCppStructOps();
		}
		for (UScriptStruct* Script : ScriptStructs)
		{
			Script->PrepareCppStructOps();
			check(Script->GetCppStructOps());
		}
		// Make sure new classes have the token stream assembled
		UClass::AssembleReferenceTokenStreams();

		HotReloadAr.Logf(ELogVerbosity::Display, TEXT("HotReload successful (%d functions remapped  %d scriptstructs remapped)"), NumFunctionsRemapped, ScriptStructs.Num());

		HotReloadFunctionRemap.Empty();

		ReplaceReferencesToReconstructedCDOs();

		// Force GC to collect reinstanced objects
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);

		Result = ECompilationResult::Succeeded;
	}


	HotReloadEvent.Broadcast( !bIsHotReloadingFromEditor );

	HotReloadAr.Logf(ELogVerbosity::Display, TEXT("HotReload took %4.1fs."), FPlatformTime::Seconds() - HotReloadStartTime);

	bIsHotReloadingFromEditor = false;
	return Result;

#else

	bIsHotReloadingFromEditor = false;
	return ECompilationResult::Unsupported;

#endif
}

void FHotReloadModule::ReplaceReferencesToReconstructedCDOs()
{
	if (ReconstructedCDOsMap.Num() == 0)
	{
		return;
	}

	// Thread pool manager. We need new thread pool with increased
	// amount of stack size. Standard GThreadPool was encountering
	// stack overflow error during serialization.
	static struct FReplaceReferencesThreadPool
	{
		FReplaceReferencesThreadPool()
		{
			Pool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();
			verify(Pool->Create(NumThreadsInThreadPool, 256 * 1024));
		}

		~FReplaceReferencesThreadPool()
		{
			Pool->Destroy();
		}

		FQueuedThreadPool* GetPool() { return Pool; }

	private:
		FQueuedThreadPool* Pool;
	} ThreadPoolManager;

	// Async task to enable multithreaded CDOs reference search.
	class FFindRefTask : public FNonAbandonableTask
	{
	public:
		explicit FFindRefTask(const TMap<UObject*, UObject*>& InReconstructedCDOsMap, int32 ReserveElements)
			: ReconstructedCDOsMap(InReconstructedCDOsMap)
		{
			ObjectsArray.Reserve(ReserveElements);
		}

		void DoWork()
		{
			for (UObject* Object : ObjectsArray)
			{
				class FReplaceCDOReferencesArchive : public FArchiveUObject
				{
				public:
					FReplaceCDOReferencesArchive(UObject* InPotentialReferencer, const TMap<UObject*, UObject*>& InReconstructedCDOsMap)
						: ReconstructedCDOsMap(InReconstructedCDOsMap)
						, PotentialReferencer(InPotentialReferencer)
					{
						ArIsObjectReferenceCollector = true;
						ArIgnoreOuterRef = true;
					}

					virtual FString GetArchiveName() const override
					{
						return TEXT("FReplaceCDOReferencesArchive");
					}

					FArchive& operator<<(UObject*& ObjRef)
					{
						UObject* Obj = ObjRef;

						if (Obj && Obj != PotentialReferencer)
						{
							if (UObject* const* FoundObj = ReconstructedCDOsMap.Find(Obj))
							{
								ObjRef = *FoundObj;
							}
						}
					
						return *this;
					}

					const TMap<UObject*, UObject*>& ReconstructedCDOsMap;
					UObject* PotentialReferencer;
				};

				FReplaceCDOReferencesArchive FindRefsArchive(Object, ReconstructedCDOsMap);
				Object->Serialize(FindRefsArchive);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FFindRefTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		TArray<UObject*> ObjectsArray;

	private:
		const TMap<UObject*, UObject*>& ReconstructedCDOsMap;
	};

	const int32 NumberOfThreads = FPlatformMisc::NumberOfWorkerThreadsToSpawn();
	const int32 NumObjects = GUObjectArray.GetObjectArrayNum();
	const int32 ObjectsPerTask = FMath::CeilToInt((float)NumObjects / NumberOfThreads);

	// Create tasks.
	TArray<FAsyncTask<FFindRefTask>> Tasks;
	Tasks.Reserve(NumberOfThreads);

	for (int32 TaskId = 0; TaskId < NumberOfThreads; ++TaskId)
	{
		Tasks.Emplace(ReconstructedCDOsMap, ObjectsPerTask);
	}

	// Distribute objects uniformly between tasks.
	int32 CurrentTaskId = 0;
	for (FObjectIterator ObjIter; ObjIter; ++ObjIter)
	{
		UObject* CurObject = *ObjIter;

		if (CurObject->IsPendingKill())
		{
			continue;
		}

		Tasks[CurrentTaskId].GetTask().ObjectsArray.Add(CurObject);
		CurrentTaskId = (CurrentTaskId + 1) % NumberOfThreads;
	}

	// Run async tasks in worker threads.
	for (FAsyncTask<FFindRefTask>& Task : Tasks)
	{
		Task.StartBackgroundTask(ThreadPoolManager.GetPool());
	}

	// Wait until tasks are finished
	for (FAsyncTask<FFindRefTask>& AsyncTask : Tasks)
	{
		AsyncTask.EnsureCompletion();
	}

	ReconstructedCDOsMap.Empty();
}

ECompilationResult::Type FHotReloadModule::RebindPackages(TArray<UPackage*> InPackages, TArray<FName> DependentModules, const bool bWaitForCompletion, FOutputDevice &Ar)
{
	ECompilationResult::Type Result = ECompilationResult::Unknown;

	// Get game packages
	const FModuleManager& ModuleManager = FModuleManager::Get();
	TArray<FString> GameModuleNames = UE4HotReload_Private::GetGameModuleNames(ModuleManager);
	UE4HotReload_Private::FPackagesAndDependentNames PackagesAndDependentNames = UE4HotReload_Private::SplitByPackagesAndDependentNames(GameModuleNames);

	// Get a set of source packages combined with game packages
	TSet<UPackage*> PackagesIncludingGame(InPackages);
	int32 NumInPackages = PackagesIncludingGame.Num();
	PackagesIncludingGame.Append(PackagesAndDependentNames.Packages);

	// Check if there was any overlap
	bool bInPackagesIncludeGame = PackagesIncludingGame.Num() < NumInPackages + PackagesAndDependentNames.Packages.Num();

	// If any of those modules were game modules, we'll compile those too
	TArray<UPackage*> Packages;
	TArray<FName>     Dependencies;
	if (bInPackagesIncludeGame)
	{
		Packages     = PackagesIncludingGame.Array();
		Dependencies = MoveTemp(PackagesAndDependentNames.DependentNames);
	}
	else
	{
		Packages = InPackages;
	}

	double Duration = 0.0;

	int32 NumPackages         = InPackages.Num();
	int32 NumDependentModules = DependentModules.Num();

	{
		FScopedDurationTimer RebindTimer(Duration);
		Result = RebindPackagesInternal(MoveTemp(Packages), MoveTemp(Dependencies), bWaitForCompletion, Ar);
	}
	RecordAnalyticsEvent(TEXT("Rebind"), Result, Duration, Packages.Num(), Dependencies.Num());

	return Result;
}

ECompilationResult::Type FHotReloadModule::RebindPackagesInternal(TArray<UPackage*>&& InPackages, TArray<FName>&& DependentModules, const bool bWaitForCompletion, FOutputDevice& Ar)
{
	ECompilationResult::Type Result = ECompilationResult::Unsupported;
#if WITH_HOT_RELOAD
	bool bCanRebind = InPackages.Num() > 0;

	// Verify that we're going to be able to rebind the specified packages
	if (bCanRebind)
	{
		for (UPackage* Package : InPackages)
		{
			check(Package);

			if (Package->GetOuter() != NULL)
			{
				Ar.Logf(ELogVerbosity::Warning, TEXT("Could not rebind package for %s, package is either not bound yet or is not a DLL."), *Package->GetName());
				bCanRebind = false;
				break;
			}
		}
	}

	// We can only proceed if a compile isn't already in progress
	if (IsCurrentlyCompiling())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("Could not rebind package because a module compile is already in progress."));
		bCanRebind = false;
	}

	if (bCanRebind)
	{
		FModuleManager::Get().ResetModulePathsCache();

		bIsHotReloadingFromEditor = true;

		HotReloadStartTime = FPlatformTime::Seconds();

		TArray< FName > ModuleNames;
		for (UPackage* Package : InPackages)
		{
			// Attempt to recompile this package's module
			FName ShortPackageName = FPackageName::GetShortFName(Package->GetFName());
			ModuleNames.Add(ShortPackageName);
		}

		// Add dependent modules.
		ModuleNames.Append(DependentModules);

		// Start compiling modules
		const bool bCompileStarted = RecompileModulesAsync(
			ModuleNames,
			[this, InPackages/*=MoveTemp(InPackages)*/, DependentModules/*=MoveTemp(DependentModules)*/, &Ar](const TMap<FString, FString>& ChangedModules, bool bRecompileFinished, ECompilationResult::Type CompilationResult)
			{
				if (ECompilationResult::Failed(CompilationResult) && bRecompileFinished)
				{
					Ar.Logf(ELogVerbosity::Warning, TEXT("HotReload failed, recompile failed"));
					return;
				}

				DoHotReloadInternal(ChangedModules, InPackages, DependentModules, Ar);
			},
			bWaitForCompletion,
			Ar);

		if (bCompileStarted)
		{
			if (bWaitForCompletion)
			{
				Ar.Logf(ELogVerbosity::Warning, TEXT("HotReload operation took %4.1fs."), float(FPlatformTime::Seconds() - HotReloadStartTime));
				bIsHotReloadingFromEditor = false;
			}
			else
			{
				Ar.Logf(ELogVerbosity::Warning, TEXT("Starting HotReload took %4.1fs."), float(FPlatformTime::Seconds() - HotReloadStartTime));
			}
			Result = ECompilationResult::Succeeded;
		}
		else
		{
			Ar.Logf(ELogVerbosity::Warning, TEXT("RebindPackages failed because the compiler could not be started."));
			Result = ECompilationResult::OtherCompilationError;
			bIsHotReloadingFromEditor = false;
		}
	}
	else
#endif
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("RebindPackages not possible for specified packages (or application was compiled in monolithic mode.)"));
	}
	return Result;
}

#if WITH_ENGINE
namespace {
	static TArray<TPair<UClass*, UClass*> >& GetClassesToReinstance()
	{
		static TArray<TPair<UClass*, UClass*> > Data;
		return Data;
	}
}

void FHotReloadModule::RegisterForReinstancing(UClass* OldClass, UClass* NewClass)
{
	TPair<UClass*, UClass*> Pair;
	
	Pair.Key = OldClass;
	Pair.Value = NewClass;

	TArray<TPair<UClass*, UClass*> >& ClassesToReinstance = GetClassesToReinstance();
	ClassesToReinstance.Add(MoveTemp(Pair));
}

void FHotReloadModule::ReinstanceClasses()
{
#if WITH_HOT_RELOAD
	if (GIsHotReload)
	{
		UClass::AssembleReferenceTokenStreams();
	}
#endif // WITH_HOT_RELOAD

	TArray<TPair<UClass*, UClass*> >& ClassesToReinstance = GetClassesToReinstance();

	TMap<UClass*, UClass*> OldToNewClassesMap;
	for (const TPair<UClass*, UClass*>& Pair : ClassesToReinstance)
	{
		// Don't allow reinstancing of UEngine classes
		if (Pair.Key->IsChildOf(UEngine::StaticClass()))
		{
			UE_LOG(LogHotReload, Warning, TEXT("Engine class '%s' has changed but will be ignored for hot reload"), *Pair.Key->GetName());
			continue;
		}

		if (Pair.Value != nullptr)
		{
			OldToNewClassesMap.Add(Pair.Key, Pair.Value);
		}
	}

	for (const TPair<UClass*, UClass*>& Pair : ClassesToReinstance)
	{
		// Don't allow reinstancing of UEngine classes
		if (!Pair.Key->IsChildOf(UEngine::StaticClass()))
		{
			ReinstanceClass(Pair.Key, Pair.Value, OldToNewClassesMap);
		}
	}

	ClassesToReinstance.Empty();
}

void FHotReloadModule::ReinstanceClass(UClass* OldClass, UClass* NewClass, const TMap<UClass*, UClass*>& OldToNewClassesMap)
{	
	TSharedPtr<FHotReloadClassReinstancer> ReinstanceHelper = FHotReloadClassReinstancer::Create(NewClass, OldClass, OldToNewClassesMap, ReconstructedCDOsMap, HotReloadBPSetToRecompile, HotReloadBPSetToRecompileBytecodeOnly);
	if (ReinstanceHelper->ClassNeedsReinstancing())
	{
		UE_LOG(LogHotReload, Log, TEXT("Re-instancing %s after hot-reload."), NewClass ? *NewClass->GetName() : *OldClass->GetName());
		ReinstanceHelper->ReinstanceObjectsAndUpdateDefaults();
	}
}
#endif

void FHotReloadModule::OnHotReloadBinariesChanged(const TArray<struct FFileChangeData>& FileChanges)
{
	if (bIsHotReloadingFromEditor)
	{
		// DO NOTHING, this case is handled by RebindPackages
		return;
	}

	const FModuleManager& ModuleManager = FModuleManager::Get();
	TMap<FString, FString> GameModuleFilenames = UE4HotReload_Private::GetGameModuleFilenames(ModuleManager);

	if (GameModuleFilenames.Num() == 0)
	{
		return;
	}

	// Check if any of the game DLLs has been added
	for (auto& Change : FileChanges)
	{
		// Ignore changes that aren't introducing a new file.
		//
		// On the Mac the Add event is for a temporary linker(?) file that gets immediately renamed
		// to a dylib. In the future we may want to support modified event for all platforms anyway once
		// shadow copying works with hot-reload.
#if PLATFORM_MAC
		if (Change.Action != FFileChangeData::FCA_Modified)
#else
		if (Change.Action != FFileChangeData::FCA_Added)
#endif
		{
			continue;
		}

		// Ignore files that aren't of module type
		FString Filename = FPaths::GetCleanFilename(Change.Filename);
		if (!Filename.EndsWith(FPlatformProcess::GetModuleExtension()))
		{
			continue;
		}

		for (const TPair<FString, FString>& NameFilename : GameModuleFilenames)
		{
			// Handle module files which have already been hot-reloaded.
			FString BaseName = FPaths::GetBaseFilename(NameFilename.Value);
			StripModuleSuffixFromFilename(BaseName, NameFilename.Key);

			// Hot reload always adds a numbered suffix preceded by a hyphen, but otherwise the module name must match exactly!
			if (!Filename.StartsWith(BaseName + TEXT("-")))
			{
				continue;
			}

			if (NewModules.Contains(NameFilename.Key))
			{
				continue;
			}

			if (ModulesRecentlyCompiledInTheEditor.Contains(FPaths::ConvertRelativePathToFull(Change.Filename)))
			{
				continue;
			}

			// Add to queue. We do not hot-reload here as there may potentially be other modules being compiled.
			NewModules.Emplace(NameFilename.Key, Change.Filename);
			UE_LOG(LogHotReload, Log, TEXT("New module detected: %s"), *Filename);
		}
	}
}

void FHotReloadModule::StripModuleSuffixFromFilename(FString& InOutModuleFilename, const FString& ModuleName)
{
	// First hyphen is where the UE4Edtior prefix ends
	int32 FirstHyphenIndex = INDEX_NONE;
	if (InOutModuleFilename.FindChar('-', FirstHyphenIndex))
	{
		// Second hyphen means we already have a hot-reloaded module or other than Development config module
		int32 SecondHyphenIndex = FirstHyphenIndex;
		do
		{
			SecondHyphenIndex = InOutModuleFilename.Find(TEXT("-"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SecondHyphenIndex + 1);
			if (SecondHyphenIndex != INDEX_NONE)
			{
				// Make sure that the section between hyphens is the expected module name. This guards against cases where module name has a hyphen inside.
				FString HotReloadedModuleName = InOutModuleFilename.Mid(FirstHyphenIndex + 1, SecondHyphenIndex - FirstHyphenIndex - 1);
				if (HotReloadedModuleName == ModuleName)
				{
					InOutModuleFilename = InOutModuleFilename.Mid(0, SecondHyphenIndex);
					SecondHyphenIndex = INDEX_NONE;
				}
			}
		} while (SecondHyphenIndex != INDEX_NONE);
	}
}

void FHotReloadModule::RefreshHotReloadWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		// Watch the game directory
		AddHotReloadDirectory(DirectoryWatcher, FPaths::ProjectDir());

		// Also watch all the game plugin directories
		for(const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
		{
			if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project && Plugin->GetDescriptor().Modules.Num() > 0)
			{
				AddHotReloadDirectory(DirectoryWatcher, Plugin->GetBaseDir());
			}
		}
	}
}

void FHotReloadModule::AddHotReloadDirectory(IDirectoryWatcher* DirectoryWatcher, const FString& BaseDir)
{
	FString BinariesPath = FPaths::ConvertRelativePathToFull(BaseDir / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory());
	if (FPaths::DirectoryExists(BinariesPath) && !BinariesFolderChangedDelegateHandles.Contains(BinariesPath))
	{
		IDirectoryWatcher::FDirectoryChanged BinariesFolderChangedDelegate = IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FHotReloadModule::OnHotReloadBinariesChanged);

		FDelegateHandle Handle;
		if (DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(BinariesPath, BinariesFolderChangedDelegate, Handle))
		{
			BinariesFolderChangedDelegateHandles.Add(BinariesPath, Handle);
		}
	}
}

void FHotReloadModule::ShutdownHotReloadWatcher()
{
	FDirectoryWatcherModule* DirectoryWatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if( DirectoryWatcherModule != nullptr )
	{
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule->Get();
		if (DirectoryWatcher)
		{
			for (const TPair<FString, FDelegateHandle>& Pair : BinariesFolderChangedDelegateHandles)
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Pair.Key, Pair.Value);
			}
		}
	}
}

bool FHotReloadModule::Tick(float DeltaTime)
{
	if (NewModules.Num())
	{
#if WITH_EDITOR
		if (GEditor)
		{
			// Don't allow hot reloading if we're running networked PIE instances
			// The reason, is it's fairly complicated to handle the re-wiring that needs to happen when we re-instance objects like player controllers, possessed pawns, etc...
			const TIndirectArray<FWorldContext>& WorldContextList = GEditor->GetWorldContexts();

			for (const FWorldContext& WorldContext : WorldContextList)
			{
				if (WorldContext.World() && WorldContext.World()->WorldType == EWorldType::PIE && WorldContext.World()->NetDriver)
				{
					return true;		// Don't allow automatic hot reloading if we're running PIE instances
				}
			}
		}
#endif // WITH_EDITOR

		// We have new modules in the queue, but make sure UBT has finished compiling all of them
		if (!FDesktopPlatformModule::Get()->IsUnrealBuildToolRunning())
		{
			DoHotReloadFromIDE();
			NewModules.Empty();
		}
		else
		{
			UE_LOG(LogHotReload, Verbose, TEXT("Detected %d reloaded modules but UnrealBuildTool is still running"), NewModules.Num());
		}
	}
	return true;
}

void FHotReloadModule::DoHotReloadFromIDE()
{
	const FModuleManager& ModuleManager = FModuleManager::Get();
	IFileManager& FileManager = IFileManager::Get();

	int32 NumPackagesToRebind = 0;
	int32 NumDependentModules = 0;

	ECompilationResult::Type Result = ECompilationResult::Unsupported;

	double Duration = 0.0;

	TArray<FString> GameModuleNames = UE4HotReload_Private::GetGameModuleNames(ModuleManager);

	if (GameModuleNames.Num() > 0)
	{
		FScopedDurationTimer Timer(Duration);

		// Remove any modules whose files have disappeared - this can happen if a compile event has
		// failed and deleted a DLL that was there previously.
		for (auto It = NewModules.CreateIterator(); It; ++It)
		{
			if (!FileManager.FileExists(*It->Value))
			{
				It.RemoveCurrent();
			}
		}
		if (NewModules.Num() == 0)
		{
			return;
		}

		UE_LOG(LogHotReload, Log, TEXT("Starting Hot-Reload from IDE"));

		HotReloadStartTime = FPlatformTime::Seconds();

		FScopedSlowTask SlowTask(100.f, LOCTEXT("CompilingGameCode", "Compiling Game Code"));
		SlowTask.MakeDialog();

		// Update compile data before we start compiling
		for (const TPair<FString, FString>& NewModule : NewModules)
		{
			// Move on 10% / num items
			SlowTask.EnterProgressFrame(10.f/NewModules.Num());

			FName ModuleName = *NewModule.Key;

			UpdateModuleCompileData(ModuleName);
			OnModuleCompileSucceeded(ModuleName, NewModule.Value);
		}

		SlowTask.EnterProgressFrame(10);
		UE4HotReload_Private::FPackagesAndDependentNames PackagesAndDependentNames = UE4HotReload_Private::SplitByPackagesAndDependentNames(GameModuleNames);
		SlowTask.EnterProgressFrame(80);

		NumPackagesToRebind = PackagesAndDependentNames.Packages.Num();
		NumDependentModules = PackagesAndDependentNames.DependentNames.Num();
		Result = DoHotReloadInternal(NewModules, PackagesAndDependentNames.Packages, PackagesAndDependentNames.DependentNames, *GLog);
	}

	RecordAnalyticsEvent(TEXT("IDE"), Result, Duration, NumPackagesToRebind, NumDependentModules);
}

void FHotReloadModule::RecordAnalyticsEvent(const TCHAR* ReloadFrom, ECompilationResult::Type Result, double Duration, int32 PackageCount, int32 DependentModulesCount)
{
#if WITH_ENGINE
	if (FEngineAnalytics::IsAvailable())
	{
		TArray< FAnalyticsEventAttribute > ReloadAttribs;
		ReloadAttribs.Add(FAnalyticsEventAttribute(TEXT("ReloadFrom"), ReloadFrom));
		ReloadAttribs.Add(FAnalyticsEventAttribute(TEXT("Result"), ECompilationResult::ToString(Result)));
		ReloadAttribs.Add(FAnalyticsEventAttribute(TEXT("Duration"), FString::Printf(TEXT("%.4lf"), Duration)));
		ReloadAttribs.Add(FAnalyticsEventAttribute(TEXT("Packages"), FString::Printf(TEXT("%d"), PackageCount)));
		ReloadAttribs.Add(FAnalyticsEventAttribute(TEXT("DependentModules"), FString::Printf(TEXT("%d"), DependentModulesCount)));
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.HotReload"), ReloadAttribs);
	}
#endif
}

bool FHotReloadModule::RecompileModulesAsync( const TArray< FName > ModuleNames, FRecompileModulesCallback&& InRecompileModulesCallback, const bool bWaitForCompletion, FOutputDevice &Ar )
{
#if WITH_HOT_RELOAD
	// NOTE: This method of recompiling always using a rolling file name scheme, since we never want to unload before
	// we start recompiling, and we need the output DLL to be unlocked before we invoke the compiler

	ModuleCompilerStartedEvent.Broadcast(!bWaitForCompletion); // we perform an async compile providing we're not waiting for completion

	FModuleManager& ModuleManager = FModuleManager::Get();

	TArray< FModuleToRecompile > ModulesToRecompile;
	for( FName CurModuleName : ModuleNames )
	{
		// Update our set of known modules, in case we don't already know about this module
		ModuleManager.AddModule( CurModuleName );

		// Find a unique file name for the module
		FModuleToRecompile ModuleToRecompile;
		ModuleToRecompile.ModuleName = CurModuleName.ToString();
		ModuleManager.MakeUniqueModuleFilename( CurModuleName, ModuleToRecompile.ModuleFileSuffix, ModuleToRecompile.NewModuleFilename );

		ModulesToRecompile.Add( ModuleToRecompile );
	}

	// Kick off compilation!
	const FString AdditionalArguments = MakeUBTArgumentsForModuleCompiling();
	const bool bFailIfGeneratedCodeChanges = false;
	const bool bForceCodeProject = false;
	bool bWasSuccessful = StartCompilingModuleDLLs(FApp::GetProjectName(), ModulesToRecompile, MoveTemp(InRecompileModulesCallback), Ar, bFailIfGeneratedCodeChanges, AdditionalArguments, bForceCodeProject);
	if (bWasSuccessful)
	{
		// Go ahead and check for completion right away.  This is really just so that we can handle the case
		// where the user asked us to wait for the compile to finish before returning.
		bool bCompileStillInProgress = false;
		bool bCompileSucceeded = false;
		FOutputDeviceNull NullOutput;
		CheckForFinishedModuleDLLCompile( bWaitForCompletion, bCompileStillInProgress, bCompileSucceeded, NullOutput );
		if( !bCompileStillInProgress && !bCompileSucceeded )
		{
			bWasSuccessful = false;
		}
	}

	return bWasSuccessful;
#else
	return false;
#endif // WITH_HOT_RELOAD
}

void FHotReloadModule::OnModuleCompileSucceeded(FName ModuleName, const FString& NewModuleFilename)
{
	// If the compile succeeded, update the module info entry with the new file name for this module
	FModuleManager::Get().SetModuleFilename(ModuleName, NewModuleFilename);

#if WITH_HOT_RELOAD
	// UpdateModuleCompileData() should have been run before compiling so the
	// data in ModuleInfo should be correct for the pre-compile dll file.
	FModuleCompilationData& CompileData = ModuleCompileData.FindChecked(ModuleName).Get();

	FDateTime FileTimeStamp;
	bool bGotFileTimeStamp = GetModuleFileTimeStamp(ModuleName, FileTimeStamp);

	CompileData.bHasFileTimeStamp = bGotFileTimeStamp;
	CompileData.FileTimeStamp = FileTimeStamp;

	if (CompileData.bHasFileTimeStamp)
	{
		CompileData.CompileMethod = EModuleCompileMethod::Runtime;
	}
	else
	{
		CompileData.CompileMethod = EModuleCompileMethod::Unknown;
	}
	WriteModuleCompilationInfoToConfig(ModuleName, CompileData);
#endif
}

bool FHotReloadModule::RecompileModuleDLLs(const TArray< FModuleToRecompile >& ModuleNames, FOutputDevice& Ar, bool bFailIfGeneratedCodeChanges, bool bForceCodeProject)
{
	bool bCompileSucceeded = false;
#if WITH_HOT_RELOAD
	const FString AdditionalArguments = MakeUBTArgumentsForModuleCompiling();
	if (StartCompilingModuleDLLs(FApp::GetProjectName(), ModuleNames, nullptr, Ar, bFailIfGeneratedCodeChanges, AdditionalArguments, bForceCodeProject))
	{
		const bool bWaitForCompletion = true;	// Always wait
		bool bCompileStillInProgress = false;
		CheckForFinishedModuleDLLCompile( bWaitForCompletion, bCompileStillInProgress, bCompileSucceeded, Ar );
	}
#endif
	return bCompileSucceeded;
}

FString FHotReloadModule::MakeUBTArgumentsForModuleCompiling()
{
	FString AdditionalArguments;
	if ( FPaths::IsProjectFilePathSet() )
	{
		// We have to pass FULL paths to UBT
		FString FullProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());

		// @todo projectdirs: Currently non-installed projects that exist under the UE4 root are compiled by UBT with no .uproject file
		//     name passed in (see bIsProjectTarget in VCProject.cs), which causes intermediate libraries to be saved to the Engine
		//     intermediate folder instead of the project's intermediate folder.  We're emulating this behavior here for module
		//     recompiling, so that compiled modules will be able to find their import libraries in the original folder they were compiled.
		if( FApp::IsEngineInstalled() || !FullProjectPath.StartsWith( FPaths::ConvertRelativePathToFull( FPaths::RootDir() ) ) )
		{
			const FString ProjectFilenameWithQuotes = FString::Printf(TEXT("\"%s\""), *FullProjectPath);
			AdditionalArguments += FString::Printf(TEXT("%s "), *ProjectFilenameWithQuotes);
		}
	}

	// Use new FastPDB option to cut down linking time. Currently disabled due to problems with missing symbols in VS2015.
	// AdditionalArguments += TEXT(" -FastPDB");

	return AdditionalArguments;
}

bool FHotReloadModule::StartCompilingModuleDLLs(const FString& GameName, const TArray< FModuleToRecompile >& ModuleNames, 
	FRecompileModulesCallback&& InRecompileModulesCallback, FOutputDevice& Ar, bool bInFailIfGeneratedCodeChanges, 
	const FString& InAdditionalCmdLineArgs, bool bForceCodeProject)
{
#if WITH_HOT_RELOAD
	// Keep track of what we're compiling
	ModulesBeingCompiled = ModuleNames;
	ModulesThatWereBeingRecompiled = ModulesBeingCompiled;

	const TCHAR* BuildPlatformName = FPlatformMisc::GetUBTPlatform();
	const TCHAR* BuildConfigurationName = FModuleManager::GetUBTConfiguration();

	RecompileModulesCallback = MoveTemp(InRecompileModulesCallback);

	// Pass a module file suffix to UBT if we have one
	FString ModuleArg;
	if (ModuleNames.Num())
	{
		Ar.Logf(TEXT("Candidate modules for hot reload:"));
		for( const FModuleToRecompile& Module : ModuleNames )
		{
			if( !Module.ModuleFileSuffix.IsEmpty() )
			{
				ModuleArg += FString::Printf( TEXT( " -ModuleWithSuffix %s %s" ), *Module.ModuleName, *Module.ModuleFileSuffix );
			}
			else
			{
				ModuleArg += FString::Printf( TEXT( " -Module %s" ), *Module.ModuleName );
			}
			Ar.Logf( TEXT( "  %s" ), *Module.ModuleName );

			// prepare the compile info in the FModuleInfo so that it can be compared after compiling
			FName ModuleFName(*Module.ModuleName);
			UpdateModuleCompileData(ModuleFName);
		}
	}

	FString ExtraArg;
#if UE_EDITOR
	// NOTE: When recompiling from the editor, we're passed the game target name, not the editor target name, but we'll
	//       pass "-editorrecompile" to UBT which tells UBT to figure out the editor target to use for this game, since
	//       we can't possibly know what the target is called from within the engine code.
	ExtraArg = TEXT( "-editorrecompile " );
#endif

	if (bInFailIfGeneratedCodeChanges)
	{
		// Additional argument to let UHT know that we can only compile the module if the generated code didn't change
		ExtraArg += TEXT( "-FailIfGeneratedCodeChanges " );
	}

	// If there's nothing to compile, don't bother linking the DLLs as the old ones are up-to-date
	ExtraArg += TEXT("-canskiplink ");

	// Shared PCH does no work with hot-reloading engine/editor modules as we don't scan all modules for them.
	if (!ContainsOnlyGameModules(ModuleNames))
	{
		ExtraArg += TEXT("-nosharedpch ");
	}
	
	FString TargetName = GameName;

#if WITH_EDITOR
	// If there are no game modules loaded, then it's not a code-based project and the target
	// for UBT should be the editor.
	if (!bForceCodeProject && !IsAnyGameModuleLoaded())
	{
		TargetName = TEXT("UE4Editor");
	}
#endif

	FString CmdLineParams = FString::Printf( TEXT( "%s%s %s %s %s%s" ), 
		*TargetName, *ModuleArg, 
		BuildPlatformName, BuildConfigurationName, 
		*ExtraArg, *InAdditionalCmdLineArgs );

	const bool bInvocationSuccessful = InvokeUnrealBuildToolForCompile(CmdLineParams, Ar);
	if ( !bInvocationSuccessful )
	{
		// No longer compiling modules
		ModulesBeingCompiled.Empty();

		ModuleCompilerFinishedEvent.Broadcast(FString(), ECompilationResult::OtherCompilationError, false);

		// Fire task completion delegate 
		
		if (RecompileModulesCallback)
		{
			RecompileModulesCallback( TMap<FString, FString>(), false, ECompilationResult::OtherCompilationError );
			RecompileModulesCallback = nullptr;
		}
	}

	return bInvocationSuccessful;
#else
	return false;
#endif
}

bool FHotReloadModule::InvokeUnrealBuildToolForCompile(const FString& InCmdLineParams, FOutputDevice &Ar)
{
#if WITH_HOT_RELOAD

	// Make sure we're not already compiling something!
	check(!IsCurrentlyCompiling());

	// Setup output redirection pipes, so that we can harvest compiler output and display it ourselves
	void* PipeRead = NULL;
	void* PipeWrite = NULL;

	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));
	ModuleCompileReadPipeText = TEXT("");

	FProcHandle ProcHandle = FDesktopPlatformModule::Get()->InvokeUnrealBuildToolAsync(InCmdLineParams, Ar, PipeRead, PipeWrite);

	// We no longer need the Write pipe so close it.
	// We DO need the Read pipe however...
	FPlatformProcess::ClosePipe(0, PipeWrite);

	if (!ProcHandle.IsValid())
	{
		// We're done with the process handle now
		ModuleCompileProcessHandle.Reset();
		ModuleCompileReadPipe = NULL;
	}
	else
	{
		ModuleCompileProcessHandle = ProcHandle;
		ModuleCompileReadPipe = PipeRead;
	}

	return ProcHandle.IsValid();
#else
	return false;
#endif // WITH_HOT_RELOAD
}

void FHotReloadModule::CheckForFinishedModuleDLLCompile(const bool bWaitForCompletion, bool& bCompileStillInProgress, bool& bCompileSucceeded, FOutputDevice& Ar, bool bFireEvents)
{
#if WITH_HOT_RELOAD
	bCompileStillInProgress = false;
	ECompilationResult::Type CompilationResult = ECompilationResult::OtherCompilationError;

	// Is there a compilation in progress?
	if( !IsCurrentlyCompiling() )
	{
		Ar.Logf(TEXT("Error: CheckForFinishedModuleDLLCompile: There is no compilation in progress right now"));
		return;
	}

	bCompileStillInProgress = true;

	FText StatusUpdate;
	if ( ModulesBeingCompiled.Num() > 0 )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("CodeModuleName"), FText::FromString( ModulesBeingCompiled[0].ModuleName ) );
		StatusUpdate = FText::Format( NSLOCTEXT("FModuleManager", "CompileSpecificModuleStatusMessage", "{CodeModuleName}: Compiling modules..."), Args );
	}
	else
	{
		StatusUpdate = NSLOCTEXT("FModuleManager", "CompileStatusMessage", "Compiling modules...");
	}

	FScopedSlowTask SlowTask(0, StatusUpdate, GIsSlowTask);
	SlowTask.MakeDialog();

	// Check to see if the compile has finished yet
	int32 ReturnCode = -1;
	while (bCompileStillInProgress)
	{
		// Store the return code in a temp variable for now because it still gets overwritten
		// when the process is running.
		int32 ProcReturnCode = -1;
		if( FPlatformProcess::GetProcReturnCode( ModuleCompileProcessHandle, &ProcReturnCode ) )
		{
			ReturnCode = ProcReturnCode;
			bCompileStillInProgress = false;
		}
		
		if (bRequestCancelCompilation)
		{
			FPlatformProcess::TerminateProc(ModuleCompileProcessHandle);
			bCompileStillInProgress = bRequestCancelCompilation = false;
		}

		if( bCompileStillInProgress )
		{
			ModuleCompileReadPipeText += FPlatformProcess::ReadPipe(ModuleCompileReadPipe);

			if( !bWaitForCompletion )
			{
				// We haven't finished compiling, but we were asked to return immediately

				break;
			}

			SlowTask.EnterProgressFrame(0.0f);

			// Give up a small timeslice if we haven't finished recompiling yet
			FPlatformProcess::Sleep( 0.01f );
		}
	}
	
	bRequestCancelCompilation = false;

	if( bCompileStillInProgress )
	{
		Ar.Logf(TEXT("Error: CheckForFinishedModuleDLLCompile: Compilation is still in progress"));
		return;
	}

	// Compilation finished, now we need to grab all of the text from the output pipe
	ModuleCompileReadPipeText += FPlatformProcess::ReadPipe(ModuleCompileReadPipe);

	// This includes 'canceled' (-1) and 'up-to-date' (-2)
	CompilationResult = (ECompilationResult::Type)ReturnCode;

	// If compilation succeeded for all modules, go back to the modules and update their module file names
	// in case we recompiled the modules to a new unique file name.  This is needed so that when the module
	// is reloaded after the recompile, we load the new DLL file name, not the old one.
	// Note that we don't want to do anything in case the build was canceled or source code has not changed.
	TMap<FString, FString> ChangedModules;
	if(CompilationResult == ECompilationResult::Succeeded)
	{
		ChangedModules.Reserve(ModulesThatWereBeingRecompiled.Num());
		for( FModuleToRecompile& CurModule : ModulesThatWereBeingRecompiled )
		{
			// Were we asked to assign a new file name for this module?
			if( CurModule.NewModuleFilename.IsEmpty() )
			{
				continue;
			}

			if (IFileManager::Get().FileSize(*CurModule.NewModuleFilename) <= 0)
			{
				continue;
			}

			// If the file doesn't exist, then assume it doesn't needs rebinding because it wasn't recompiled
			FDateTime FileTimeStamp = IFileManager::Get().GetTimeStamp(*CurModule.NewModuleFilename);
			if (FileTimeStamp == FDateTime::MinValue())
			{
				continue;
			}

			FName ModuleName = *CurModule.ModuleName;

			// If the file is the same as what we remembered it was then assume it doesn't needs rebinding because it wasn't recompiled
			TSharedRef<FModuleCompilationData>* CompileDataPtr = ModuleCompileData.Find(ModuleName);
			if (CompileDataPtr && (*CompileDataPtr)->FileTimeStamp == FileTimeStamp)
			{
				continue;
			}

			// If the compile succeeded, update the module info entry with the new file name for this module
			OnModuleCompileSucceeded(ModuleName, CurModule.NewModuleFilename);

			// Move modules
			ChangedModules.Emplace(MoveTemp(CurModule.ModuleName), MoveTemp(CurModule.NewModuleFilename));
		}
	}
	ModulesThatWereBeingRecompiled.Empty();

	// We're done with the process handle now
	FPlatformProcess::CloseProc(ModuleCompileProcessHandle);
	ModuleCompileProcessHandle.Reset();

	FPlatformProcess::ClosePipe(ModuleCompileReadPipe, 0);

	Ar.Log(*ModuleCompileReadPipeText);
	const FString FinalOutput = ModuleCompileReadPipeText;
	ModuleCompileReadPipe = NULL;
	ModuleCompileReadPipeText = TEXT("");

	// No longer compiling modules
	ModulesBeingCompiled.Empty();

	bCompileSucceeded = !ECompilationResult::Failed(CompilationResult);

	if ( bFireEvents )
	{
		const bool bShowLogOnSuccess = false;
		ModuleCompilerFinishedEvent.Broadcast(FinalOutput, CompilationResult, !bCompileSucceeded || bShowLogOnSuccess);

		// Fire task completion delegate 
		if (RecompileModulesCallback)
		{
			RecompileModulesCallback( ChangedModules, true, CompilationResult );
			RecompileModulesCallback = nullptr;
		}
	}
#endif // WITH_HOT_RELOAD
}

void FHotReloadModule::UpdateModuleCompileData(FName ModuleName)
{
	// Find or create a compile data object for this module
	TSharedRef<FModuleCompilationData>* CompileDataPtr = ModuleCompileData.Find(ModuleName);
	if(CompileDataPtr == nullptr)
	{
		CompileDataPtr = &ModuleCompileData.Add(ModuleName, TSharedRef<FModuleCompilationData>(new FModuleCompilationData()));
	}

	// reset the compile data before updating it
	FModuleCompilationData& CompileData = CompileDataPtr->Get();
	CompileData.bHasFileTimeStamp = false;
	CompileData.FileTimeStamp = FDateTime(0);
	CompileData.CompileMethod = EModuleCompileMethod::Unknown;

#if WITH_HOT_RELOAD
	ReadModuleCompilationInfoFromConfig(ModuleName, CompileData);

	FDateTime FileTimeStamp;
	bool bGotFileTimeStamp = GetModuleFileTimeStamp(ModuleName, FileTimeStamp);

	if (!bGotFileTimeStamp)
	{
		// File missing? Reset the cached timestamp and method to defaults and save them.
		CompileData.bHasFileTimeStamp = false;
		CompileData.FileTimeStamp = FDateTime(0);
		CompileData.CompileMethod = EModuleCompileMethod::Unknown;
		WriteModuleCompilationInfoToConfig(ModuleName, CompileData);
	}
	else
	{
		if (CompileData.bHasFileTimeStamp)
		{
			if (FileTimeStamp > CompileData.FileTimeStamp + HotReloadDefs::TimeStampEpsilon)
			{
				// The file is newer than the cached timestamp
				// The file must have been compiled externally
				CompileData.FileTimeStamp = FileTimeStamp;
				CompileData.CompileMethod = EModuleCompileMethod::External;
				WriteModuleCompilationInfoToConfig(ModuleName, CompileData);
			}
		}
		else
		{
			// The cached timestamp and method are default value so this file has no history yet
			// We can only set its timestamp and save
			CompileData.bHasFileTimeStamp = true;
			CompileData.FileTimeStamp = FileTimeStamp;
			WriteModuleCompilationInfoToConfig(ModuleName, CompileData);
		}
	}
#endif
}

void FHotReloadModule::ReadModuleCompilationInfoFromConfig(FName ModuleName, FModuleCompilationData& CompileData)
{
	FString DateTimeString;
	if (GConfig->GetString(*HotReloadDefs::CompilationInfoConfigSection, *FString::Printf(TEXT("%s.TimeStamp"), *ModuleName.ToString()), DateTimeString, GEditorPerProjectIni))
	{
		FDateTime TimeStamp;
		if (!DateTimeString.IsEmpty() && FDateTime::Parse(DateTimeString, TimeStamp))
		{
			CompileData.bHasFileTimeStamp = true;
			CompileData.FileTimeStamp = TimeStamp;

			FString CompileMethodString;
			if (GConfig->GetString(*HotReloadDefs::CompilationInfoConfigSection, *FString::Printf(TEXT("%s.LastCompileMethod"), *ModuleName.ToString()), CompileMethodString, GEditorPerProjectIni))
			{
				if (CompileMethodString.Equals(HotReloadDefs::CompileMethodRuntime, ESearchCase::IgnoreCase))
				{
					CompileData.CompileMethod = EModuleCompileMethod::Runtime;
				}
				else if (CompileMethodString.Equals(HotReloadDefs::CompileMethodExternal, ESearchCase::IgnoreCase))
				{
					CompileData.CompileMethod = EModuleCompileMethod::External;
				}
			}
		}
	}
}

void FHotReloadModule::WriteModuleCompilationInfoToConfig(FName ModuleName, const FModuleCompilationData& CompileData)
{
	FString DateTimeString;
	if (CompileData.bHasFileTimeStamp)
	{
		DateTimeString = CompileData.FileTimeStamp.ToString();
	}

	GConfig->SetString(*HotReloadDefs::CompilationInfoConfigSection, *FString::Printf(TEXT("%s.TimeStamp"), *ModuleName.ToString()), *DateTimeString, GEditorPerProjectIni);

	FString CompileMethodString = HotReloadDefs::CompileMethodUnknown;
	if (CompileData.CompileMethod == EModuleCompileMethod::Runtime)
	{
		CompileMethodString = HotReloadDefs::CompileMethodRuntime;
	}
	else if (CompileData.CompileMethod == EModuleCompileMethod::External)
	{
		CompileMethodString = HotReloadDefs::CompileMethodExternal;
	}

	GConfig->SetString(*HotReloadDefs::CompilationInfoConfigSection, *FString::Printf(TEXT("%s.LastCompileMethod"), *ModuleName.ToString()), *CompileMethodString, GEditorPerProjectIni);
}

bool FHotReloadModule::GetModuleFileTimeStamp(FName ModuleName, FDateTime& OutFileTimeStamp) const
{
	FString Filename = FModuleManager::Get().GetModuleFilename(ModuleName);
	if (IFileManager::Get().FileSize(*Filename) > 0)
	{
		OutFileTimeStamp = FDateTime(IFileManager::Get().GetTimeStamp(*Filename));
		return true;
	}
	return false;
}

bool FHotReloadModule::IsAnyGameModuleLoaded()
{
	if (bIsAnyGameModuleLoaded == EThreeStateBool::Unknown)
	{
		bool bGameModuleFound = false;
		// Ask the module manager for a list of currently-loaded gameplay modules
		TArray< FModuleStatus > ModuleStatuses;
		FModuleManager::Get().QueryModules(ModuleStatuses);

		for (auto ModuleStatusIt = ModuleStatuses.CreateConstIterator(); ModuleStatusIt; ++ModuleStatusIt)
		{
			const FModuleStatus& ModuleStatus = *ModuleStatusIt;

			// We only care about game modules that are currently loaded
			if (ModuleStatus.bIsLoaded && ModuleStatus.bIsGameModule)
			{
				// There is at least one loaded game module.
				bGameModuleFound = true;
				break;
			}
		}
		bIsAnyGameModuleLoaded = EThreeStateBool::FromBool(bGameModuleFound);
	}
	return EThreeStateBool::ToBool(bIsAnyGameModuleLoaded);
}

bool FHotReloadModule::ContainsOnlyGameModules(const TArray<FModuleToRecompile>& ModulesToCompile) const
{
	const FString AbsoluteProjectDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	bool bOnlyGameModules = true;
	for (auto& ModuleToCompile : ModulesToCompile)
	{
		const FString FullModulePath(FPaths::ConvertRelativePathToFull(ModuleToCompile.NewModuleFilename));
		if (!FullModulePath.StartsWith(AbsoluteProjectDir))
		{
			bOnlyGameModules = false;
			break;
		}
	}
	return bOnlyGameModules;
}

void FHotReloadModule::ModulesChangedCallback(FName ModuleName, EModuleChangeReason ReasonForChange)
{
	// Force update game modules state on the next call to IsAnyGameModuleLoaded
	bIsAnyGameModuleLoaded = EThreeStateBool::Unknown;
	
	// If the hot reload directory watcher hasn't been initialized yet (because the binaries directory did not exist) try to initialize it now
	if (!bDirectoryWatcherInitialized)
	{
		RefreshHotReloadWatcher();
		bDirectoryWatcherInitialized = true;
	}
}

void FHotReloadModule::PluginMountedCallback(IPlugin& Plugin)
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));

	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();
	if (DirectoryWatcher)
	{
		if (Plugin.GetLoadedFrom() == EPluginLoadedFrom::Project && Plugin.GetDescriptor().Modules.Num() > 0)
		{
			AddHotReloadDirectory(DirectoryWatcher, Plugin.GetBaseDir());
		}
	}
}

#undef LOCTEXT_NAMESPACE
