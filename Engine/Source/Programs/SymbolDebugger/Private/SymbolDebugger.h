// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SSymbolDebugger.h"
#include "AsyncWork.h"
#include "CrashDebugHelper.h"

/**
 *	Async inspection helper class
 */
class FSymbolDebugger_AsyncInspect : public FNonAbandonableTask
{
public:
	/** Constructor */
	FSymbolDebugger_AsyncInspect(const FString& InCrashDumpName, const FString& InEngineVersion, const FString& InChangelist, ICrashDebugHelper* InCrashHelperModule);

	/** Performs work on thread */
	void DoWork();

	/** Returns true if the task should be aborted.  Called from within the task processing code itself via delegate */
	bool ShouldAbort() const
	{
		return AskedToAbortCount.GetValue() > 0;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FSymbolDebugger_AsyncInspect, STATGROUP_ThreadPoolAsyncTasks );
	}

	/** The found name */
	const FString& GetResults_LabelName() const
	{
		return Result_LabelName;
	}
	
	/** The found engine version */
	const FString& GetResults_EngineVersion() const
	{
		return Result_EngineVersionName;
	}

	/** The found platform name */
	const FString& GetResults_Platform() const
	{
		return Result_PlatformName;
	}

private:

	ICrashDebugHelper* CrashHelperModule;

	/** True if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter AskedToAbortCount;

	/** CrashDump file being inspected */
	FString CrashDumpName;
	/** EngineVersion being inspected */
	FString EngineVersionName;
	/** Changelist being inspected */
	FString ChangelistName;

	/** The found name */
	FString Result_LabelName;
	/** The found engine version */
	FString Result_EngineVersionName;
	/** The found platform name */
	FString Result_PlatformName;
};

/**
 *	Async syncing helper class
 */
class FSymbolDebugger_AsyncSyncFiles : public FNonAbandonableTask
{
public:
	/** Constructor */
	FSymbolDebugger_AsyncSyncFiles(const FString& InSourceControlLabel, const FString& InPlatform, ICrashDebugHelper* InCrashHelperModule);

	/** Performs work on thread */
	void DoWork();

	/** Returns true if the task should be aborted.  Called from within the task processing code itself via delegate */
	bool ShouldAbort() const
	{
		return AskedToAbortCount.GetValue() > 0;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FSymbolDebugger_AsyncSyncFiles, STATGROUP_ThreadPoolAsyncTasks );
	}

	/** 
	 *	@return bool	true if succeeded, false if not
	 */
	bool DidSucceed() const
	{
		return bResult_Succeeded;
	}

private:

	ICrashDebugHelper* CrashHelperModule;

	/** True if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter AskedToAbortCount;

	/** Source control label being sunk */
	FString SourceControlLabel;
	/** Platform being sunk */
	FString Platform;

	/** The results */
	bool bResult_Succeeded;
};

/**
 *	Async syncing helper class
 */
class FSymbolDebugger_LaunchDebugger : public FNonAbandonableTask
{
public:
	/** Constructor */
	FSymbolDebugger_LaunchDebugger(const FString& InCrashDumpName);

	/** Performs work on thread */
	void DoWork();

	/** Returns true if the task should be aborted.  Called from within the task processing code itself via delegate */
	bool ShouldAbort() const
	{
		return AskedToAbortCount.GetValue() > 0;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FSymbolDebugger_LaunchDebugger, STATGROUP_ThreadPoolAsyncTasks );
	}

	/** 
	 *	@return bool	true if succeeded, false if not
	 */
	bool DidSucceed() const
	{
		return bResult_Succeeded;
	}

private:
	/** True if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter AskedToAbortCount;

	/** CrashDump file */
	FString CrashDumpName;
	
	/** The results */
	bool bResult_Succeeded;
};

/**
 *	Async inspection helper class
 */
class FSymbolDebugger_ProcessCrashDump : public FNonAbandonableTask
{
public:
	/** Constructor */
	FSymbolDebugger_ProcessCrashDump(const FString& InCrashDumpName, ICrashDebugHelper* InCrashHelperModule);

	/** Performs work on thread */
	void DoWork();

	/** Returns true if the task should be aborted.  Called from within the task processing code itself via delegate */
	bool ShouldAbort() const
	{
		return AskedToAbortCount.GetValue() > 0;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FSymbolDebugger_ProcessCrashDump, STATGROUP_ThreadPoolAsyncTasks );
	}

	/** The found name */
	const FString& GetResults_LabelName() const
	{
		return Result_LabelName;
	}
	
	/** The found engine version */
	const FString& GetResults_EngineVersion() const
	{
		return Result_EngineVersionName;
	}

	/** The found platform name */
	const FString& GetResults_Platform() const
	{
		return Result_PlatformName;
	}

	/** 
	 *	@return bool	true if succeeded, false if not
	 */
	bool DidSucceed() const
	{
		return bResult_Succeeded;
	}

private:

	ICrashDebugHelper* CrashHelperModule;

	/** True if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter AskedToAbortCount;

	/** CrashDump file being inspected */
	FString CrashDumpName;

	/** The results */
	bool bResult_Succeeded;
	/** The found name */
	FString Result_LabelName;
	/** The found engine version */
	FString Result_EngineVersionName;
	/** The found platform name */
	FString Result_PlatformName;
};


/**
 *	A helper class for performing the various operations required by the application
 */
class FSymbolDebugger : public TSharedFromThis<FSymbolDebugger>
{
public:
	/** Constructor */
	FSymbolDebugger();

	/** Get the current method being used */
	SSymbolDebugger::ESymbolDebuggerMethods GetCurrentMethod() const
	{
		return CurrentMethod;
	}

	/** Set the current method */
	bool SetCurrentMethod(SSymbolDebugger::ESymbolDebuggerMethods InNewMethod);

	/** Get the current method text */
	FString GetMethodText() const;

	/** Set the method text */
	bool SetMethodText(const FString& InNewMethodText);

	/** Select a crash dump file via the file open dialog */
	bool OnFileOpen(TSharedRef<SWidget> ParentWidget);

	/** Get the current action being performed */
	SSymbolDebugger::ESymbolDebuggerActions GetCurrentAction() const
	{
		return CurrentAction;
	}

	/**
	 *	Get the give TextField name
	 *
	 *	@param	InTextField			The textField of interest
	 *
	 *	@return	FString				The name
	 */
	FString GetTextField(SSymbolDebugger::ESymbolDebuggerTextFields InTextField);

	/**
	 *	Set the give TextField name
	 *
	 *	@param	InTextField			The textField of interest
	 *	@param	InNewName			The new name
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool SetTextField(SSymbolDebugger::ESymbolDebuggerTextFields InTextField,const FString& InNewName);

	/**
	 *	Is the given textfield enabled?
	 *
	 *	@param	InTextField			The textField of interest
	 *
	 *	@return	bool				true if enabled, false if not
	 */
	bool IsTextFieldEnabled(SSymbolDebugger::ESymbolDebuggerTextFields InTextField);

	/**
	 *	Is the given action enabled?
	 *
	 *	@param	InAction			The action to perform
	 *
	 *	@return	bool				true if enabled, false if not
	 */
	bool IsActionEnabled(SSymbolDebugger::ESymbolDebuggerActions InAction);

	/**
	 *	Handle the given action
	 *
	 *	@param	InAction			The action to perform
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool OnAction(SSymbolDebugger::ESymbolDebuggerActions InAction);

	/**
	 *	Get the status text to display
	 *
	 *	@return	FString		The status text
	 */
	FString GetStatusText() const;

	/**
	 *	Has the given action completed? 
	 *
	 *	@param	InAction						The action of interest
	 *
	 *	@return	ESymbolDebuggerActionResults	The Results of the action
	 */
	SSymbolDebugger::ESymbolDebuggerActionResults ActionHasCompleted(SSymbolDebugger::ESymbolDebuggerActions InAction);

	/**
	 *	Tick the helper
	 */
	void Tick();

protected:
	/**
	 *	Inspect to determine the info for syncing required debugging files.
	 *
	 *	@return	bool		true if successful, false if not
	 */
	bool OnInspect();

	/**
	 *	Sync the required files for debugging.
	 *
	 *	@return	bool		true if successful, false if not
	 */
	bool OnSync();

	/**
	 *	Launch the debugger
	 *
	 *	@return	bool		true if successful, false if not
	 */
	bool OnDebug();

	/**
	 *	Process the crash dump - inspect, sync and launch the debugger
	 *
	 *	@return	bool		true if successful, false if not
	 */
	bool OnProcess();

	/**
	 *	Inspect the given CrashDump to determine the info for syncing required debugging files.
	 *
	 *	@param	InCrashDumpName		The full path of the crashdump file
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool InspectCrashDump(const FString& InCrashDumpName);

	/**
	 *	Inspect the given EngineVersion to determine the info for syncing required debugging files.
	 *
	 *	@param	InEngineVersion		The engine version
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool InspectEngineVersion(const FString& InEngineVersion);

	/**
	 *	Inspect the given Changelist to determine the info for syncing required debugging files.
	 *
	 *	@param	InChangelist		The changelist
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool InspectChangelist(const FString& InChangelist);

	/**
	 *	Sync the required debugging files from the given label for the given platform
	 *
	 *	@param	InLabelName			The label
	 *	@param	InPlatform			The platform
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool SyncFiles(const FString& InLabelName, const FString& InPlatform);

	/**
	 *	Launch the debugger for the given crash dump
	 *
	 *	@param	InCrashDumpName		The full path of the crashdump file
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool DebugCrashDump(const FString& InCrashDumpName);

	/**
	 *	Inspect, sync and launch the debugger for the given crash dump
	 *
	 *	@param	InCrashDumpName		The full path of the crashdump file
	 *
	 *	@return	bool				true if successful, false if not
	 */
	bool ProcessCrashDump(const FString& InCrashDumpName);

	/** The current method for symbol handling */
	SSymbolDebugger::ESymbolDebuggerMethods CurrentMethod;
	/** The current action being performed */
	SSymbolDebugger::ESymbolDebuggerActions CurrentAction;
	/** The last action completed */
	SSymbolDebugger::ESymbolDebuggerActions LastAction;

	/** The current settings */
	/** The name of the crash dump file */
	FString CrashDumpName;
	/** The name of the platform */
	FString PlatformName;
	/** The name of the source control label */
	FString SourceControlLabelName;
	/** The engine version */
	FString EngineVersionName;
	/** The changelist */
	FString ChangelistName;
	/** The symbol store */
	FString SymbolStoreName;
	/** The remote debug IP addr */
	FString RemoteDebugIPName;
	/** The depot name */
	FString DepotName;

	/** Async task for inspecting */
	TSharedPtr<FAsyncTask<class FSymbolDebugger_AsyncInspect> > InspectionTask;

	/** Async task for syncing */
	TSharedPtr<FAsyncTask<class FSymbolDebugger_AsyncSyncFiles> > SyncTask;
	/** The results from syncing */
	bool bSyncSucceeded;

	/** Async task for launching the debugger */
	TSharedPtr<FAsyncTask<class FSymbolDebugger_LaunchDebugger> > LaunchDebuggerTask;
	/** The results from launching the debugger */
	bool bLaunchDebugSucceeded;

	/** Async task for processing a crash dump */
	TSharedPtr<FAsyncTask<class FSymbolDebugger_ProcessCrashDump> > ProcessCrashDumpTask;
	/** The results from processing the crash dump */
	bool bProcessCrashDumpSucceeded;

	ICrashDebugHelper* CrashHelperModule;
};
