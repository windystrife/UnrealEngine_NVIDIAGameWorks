// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SymbolDebugger.h"
#include "SymbolDebuggerApp.h"
#include "CrashDebugHelperModule.h"
#include "SSymbolDebugger.h"
#include "DesktopPlatformModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

//-----------------------------------------------------------------------------
//	FSymbolDebugger_AsyncInspect
//-----------------------------------------------------------------------------
FSymbolDebugger_AsyncInspect::FSymbolDebugger_AsyncInspect(const FString& InCrashDumpName, const FString& InEngineVersion, const FString& InChangelist, ICrashDebugHelper* InCrashHelperModule)
	: CrashHelperModule(InCrashHelperModule)
	, AskedToAbortCount( 0 )
	, CrashDumpName(InCrashDumpName)
	, EngineVersionName(InEngineVersion)
	, ChangelistName(InChangelist)
{
}

void FSymbolDebugger_AsyncInspect::DoWork()
{
// 	if (CrashDumpName.Len() > 0)
// 	{
// 		FCrashDebugInfo CrashDebugInfo;
// 		if (CrashHelperModule->ParseCrashDump(CrashDumpName, CrashDebugInfo) == true)
// 		{
// 			Result_LabelName = CrashDebugInfo.SourceControlLabel;
// 			Result_EngineVersionName = FString::FromInt(CrashDebugInfo.EngineVersion);
// 			Result_PlatformName = CrashDebugInfo.PlatformName;
// 		}
// 		else
// 		{
// 			Result_LabelName = TEXT("");
// 			Result_EngineVersionName = TEXT("");
// 			Result_PlatformName = TEXT("");
// 		}
// 	}
// 	else if ((EngineVersionName.Len() > 0) || (ChangelistName.Len() > 0))
// 	{
// 		const FString FoundLabel;// = CrashHelperModule->GetLabelFromChangelistNumber( FCString::Atoi( *ChangelistName ) );
// 
// 		if (FoundLabel.Len() > 0)
// 		{
// 			Result_EngineVersionName = TEXT( "" );
// 			Result_LabelName = FoundLabel;
// 		}
// 		else
// 		{
// 			Result_LabelName = TEXT("");
// 		}
// 	}
}


//-----------------------------------------------------------------------------
//	FSymbolDebugger_AsyncSyncFiles
//-----------------------------------------------------------------------------
FSymbolDebugger_AsyncSyncFiles::FSymbolDebugger_AsyncSyncFiles(const FString& InSourceControlLabel, const FString& InPlatform, ICrashDebugHelper* InCrashHelperModule)
	: CrashHelperModule(InCrashHelperModule)
	, AskedToAbortCount( 0 )
	, SourceControlLabel(InSourceControlLabel)
	, Platform(InPlatform)
	, bResult_Succeeded(false)
{
}

void FSymbolDebugger_AsyncSyncFiles::DoWork()
{
// 	if (CrashHelperModule->SyncRequiredFilesForDebuggingFromLabel(SourceControlLabel, Platform) == true)
// 	{
// 		bResult_Succeeded = true;
// 	}
// 	else
// 	{
// 		bResult_Succeeded = false;
// 	}
}

//-----------------------------------------------------------------------------
//	FSymbolDebugger_LaunchDebugger
//-----------------------------------------------------------------------------
FSymbolDebugger_LaunchDebugger::FSymbolDebugger_LaunchDebugger(const FString& InCrashDumpName)
	: AskedToAbortCount( 0 )
	, CrashDumpName(InCrashDumpName)
	, bResult_Succeeded(false)
{
}

void FSymbolDebugger_LaunchDebugger::DoWork()
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*CrashDumpName);
	bResult_Succeeded = true;
}

//-----------------------------------------------------------------------------
//	FSymbolDebugger_ProcessCrashDump
//-----------------------------------------------------------------------------
FSymbolDebugger_ProcessCrashDump::FSymbolDebugger_ProcessCrashDump(const FString& InCrashDumpName, ICrashDebugHelper* InCrashHelperModule)
	: CrashHelperModule(InCrashHelperModule)
	, AskedToAbortCount( 0 )
	, CrashDumpName(InCrashDumpName)
	, bResult_Succeeded(false)
	, Result_LabelName(TEXT(""))
	, Result_EngineVersionName(TEXT(""))
	, Result_PlatformName(TEXT(""))
{
}

/** Performs work on thread */
void FSymbolDebugger_ProcessCrashDump::DoWork()
{
// 	bResult_Succeeded = false;
// 
// 	FCrashDebugInfo CrashDebugInfo;
// 	if (CrashHelperModule->ParseCrashDump(CrashDumpName, CrashDebugInfo) == true)
// 	{
// 		Result_LabelName = CrashDebugInfo.SourceControlLabel;
// 		Result_EngineVersionName = FString::FromInt(CrashDebugInfo.EngineVersion);
// 		Result_PlatformName = CrashDebugInfo.PlatformName;
// 
//  		if (CrashHelperModule->SyncRequiredFilesForDebuggingFromLabel(Result_LabelName, Result_PlatformName) == true)
//  		{
//  			FPlatformProcess::LaunchFileInDefaultExternalApplication(*CrashDumpName);
//  			bResult_Succeeded = true;
//  		}
// 	}
}

//-----------------------------------------------------------------------------
//	FSymbolDebugger
//-----------------------------------------------------------------------------
FSymbolDebugger::FSymbolDebugger()
	: CurrentMethod(SSymbolDebugger::DebugMethod_CrashDump)
	, CurrentAction(SSymbolDebugger::DebugAction_None)
	, LastAction(SSymbolDebugger::DebugAction_None)
	, CrashDumpName(TEXT(""))
	, PlatformName(TEXT(""))
	, SourceControlLabelName(TEXT(""))
	, EngineVersionName(TEXT(""))
	, ChangelistName(TEXT(""))
	, SymbolStoreName(TEXT(""))
	, RemoteDebugIPName(TEXT(""))
	, bSyncSucceeded(false)
	, bLaunchDebugSucceeded(false)
	, bProcessCrashDumpSucceeded(false)
{
	//@todo. Make this an ini setting of the last one used?
	CrashDumpName = TEXT("K:/TestProjects/92132_MiniDump.dmp");

	FString LocalSymbolStore;
	if (GConfig->GetString(TEXT("Engine.CrashDebugHelper"), TEXT("LocalSymbolStore"), LocalSymbolStore, GEngineIni) == true)
	{
		SymbolStoreName = LocalSymbolStore;
	}

	CrashHelperModule = FModuleManager::LoadModuleChecked<FCrashDebugHelperModule>(FName("CrashDebugHelper")).Get();
	check(CrashHelperModule != nullptr);
	//DepotName = CrashHelperModule->GetDepotName();
}

bool FSymbolDebugger::SetCurrentMethod(SSymbolDebugger::ESymbolDebuggerMethods InNewMethod)
{
	CurrentMethod = InNewMethod;

	return true;
}

FString FSymbolDebugger::GetMethodText() const
{
	switch (CurrentMethod)
	{
	case SSymbolDebugger::DebugMethod_CrashDump:
		return CrashDumpName;
	case SSymbolDebugger::DebugMethod_EngineVersion:
		return EngineVersionName;
	case SSymbolDebugger::DebugMethod_ChangeList:
		return ChangelistName;
	case SSymbolDebugger::DebugMethod_SourceControlLabel:
		return SourceControlLabelName;
	}

	return TEXT("");
}

bool FSymbolDebugger::SetMethodText(const FString& InNewMethodText)
{
	switch (CurrentMethod)
	{
	case SSymbolDebugger::DebugMethod_CrashDump:
		CrashDumpName = InNewMethodText;
		break;
	case SSymbolDebugger::DebugMethod_EngineVersion:
		EngineVersionName = InNewMethodText;
		break;
	case SSymbolDebugger::DebugMethod_ChangeList:
		ChangelistName = InNewMethodText;
		break;
	case SSymbolDebugger::DebugMethod_SourceControlLabel:
		SourceControlLabelName = InNewMethodText;
		break;
	}
	return true;
}

bool FSymbolDebugger::OnFileOpen(TSharedRef<SWidget> ParentWidget)
{
	//@todo. Get the extension from the CrashDebugHelper
	// Prompt the user for the filenames
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform != nullptr)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(ParentWidget);
		void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowHandle, 
			TEXT("Open crash dump file..."), 
			TEXT(""), 
			TEXT(""), 
			TEXT("CrashDump Files (*.dmp)|*.dmp"), 
			EFileDialogFlags::None, 
			OpenFilenames);
	}

	if (bOpened == true)
	{
		if (OpenFilenames.Num() > 0)
		{
			CrashDumpName = OpenFilenames[0];
		}
		else
		{
			bOpened = false;
		}
	}

	return bOpened;
}

FString FSymbolDebugger::GetTextField(SSymbolDebugger::ESymbolDebuggerTextFields InTextField)
{
	switch (InTextField)
	{
	case SSymbolDebugger::TextField_CrashDump:
		return CrashDumpName;
	case SSymbolDebugger::TextField_EngineVersion:
		return EngineVersionName;
	case SSymbolDebugger::TextField_ChangeList:
		return ChangelistName;
	case SSymbolDebugger::TextField_Label:
		return SourceControlLabelName;
	case SSymbolDebugger::TextField_Platform:
		return PlatformName;
	case SSymbolDebugger::TextField_SymbolStore:
		return SymbolStoreName;
	case SSymbolDebugger::TextField_RemoteDebugIP:
		return RemoteDebugIPName;
	case SSymbolDebugger::TextField_SourceControlDepot:
		return DepotName;
	}

	return TEXT("");
}

bool FSymbolDebugger::SetTextField(SSymbolDebugger::ESymbolDebuggerTextFields InTextField,const FString& InNewName)
{
	bool bResult = true;
	switch (InTextField)
	{
	case SSymbolDebugger::TextField_CrashDump:
		CrashDumpName = InNewName;
		break;
	case SSymbolDebugger::TextField_EngineVersion:
		EngineVersionName = InNewName;
		break;
	case SSymbolDebugger::TextField_ChangeList:
		ChangelistName = InNewName;
		break;
	case SSymbolDebugger::TextField_Label:
		SourceControlLabelName = InNewName;
		break;
	case SSymbolDebugger::TextField_Platform:
		PlatformName = InNewName;
		break;
	case SSymbolDebugger::TextField_SymbolStore:
		SymbolStoreName = InNewName;
		break;
	case SSymbolDebugger::TextField_RemoteDebugIP:
		RemoteDebugIPName = InNewName;
		break;
	case SSymbolDebugger::TextField_SourceControlDepot:
		{
			DepotName = InNewName;
			//CrashHelperModule->SetDepotName(DepotName);
		}
		break;
	default:
		bResult = false;
		break;
	}

	return bResult;
}

bool FSymbolDebugger::IsTextFieldEnabled(SSymbolDebugger::ESymbolDebuggerTextFields InTextField)
{
	bool bIsEnabled = false;
	switch (InTextField)
	{
	case SSymbolDebugger::TextField_CrashDump:
	case SSymbolDebugger::TextField_SymbolStore:
	case SSymbolDebugger::TextField_Label:
	case SSymbolDebugger::TextField_ChangeList:
	case SSymbolDebugger::TextField_EngineVersion:
		// Never enabled
		break;
	case SSymbolDebugger::TextField_RemoteDebugIP:
	case SSymbolDebugger::TextField_SourceControlDepot:
		// Always enabled
		bIsEnabled = true;
		break;
	case SSymbolDebugger::TextField_Platform:
		{
			// Only enabled for EngineVersion, Changelist, and Label
			if (CurrentMethod != SSymbolDebugger::DebugMethod_CrashDump)
			{
				bIsEnabled = true;
			}
		}
		break;
	}

	return bIsEnabled;
}

bool FSymbolDebugger::IsActionEnabled(SSymbolDebugger::ESymbolDebuggerActions InAction)
{
	switch (InAction)
	{
	case SSymbolDebugger::DebugAction_Inspect:
		{
			if (
				((CurrentMethod == SSymbolDebugger::DebugMethod_CrashDump) && (CrashDumpName.Len() > 0)) ||
				((CurrentMethod == SSymbolDebugger::DebugMethod_EngineVersion) && (EngineVersionName.Len() > 0)) ||
				((CurrentMethod == SSymbolDebugger::DebugMethod_ChangeList) && (ChangelistName.Len() > 0))
				)
			{
				return true;
			}
		}
		break;
	case SSymbolDebugger::DebugAction_Sync:
		{
			if ((SourceControlLabelName.Len() > 0) && (SymbolStoreName.Len() > 0))
			{
				if (CurrentMethod != SSymbolDebugger::DebugMethod_CrashDump)
				{
					if (PlatformName.Len() <= 0)
					{
						// Must have a platform specified
						return false;
					}

					if (CurrentMethod == SSymbolDebugger::DebugMethod_EngineVersion)
					{
						if (EngineVersionName.Len() <= 0)
						{
							return false;
						}
					}
					else if (CurrentMethod == SSymbolDebugger::DebugMethod_ChangeList)
					{
						if (ChangelistName.Len() <= 0)
						{
							return false;
						}
					}
					else if (CurrentMethod == SSymbolDebugger::DebugMethod_SourceControlLabel)
					{
						if (SourceControlLabelName.Len() <= 0)
						{
							return false;
						}
					}
				}
				return true;
			}
		}
		break;
	case SSymbolDebugger::DebugAction_Debug:
		{
			if (CurrentMethod == SSymbolDebugger::DebugMethod_CrashDump)
			{
				if (((SourceControlLabelName.Len() > 0) || (ChangelistName.Len() > 0)) && (SymbolStoreName.Len() > 0))
				{
					return true;
				}
			}
		}
		break;
	}
	return false;
}

bool FSymbolDebugger::OnAction(SSymbolDebugger::ESymbolDebuggerActions InAction)
{
	switch (InAction)
	{
	case SSymbolDebugger::DebugAction_Inspect:
		return OnInspect();
	case SSymbolDebugger::DebugAction_Sync:
		return OnSync();
	case SSymbolDebugger::DebugAction_Debug:
		return OnDebug();
	case SSymbolDebugger::DebugAction_Process:
		return OnProcess();
	}
	return false;
}

FString FSymbolDebugger::GetStatusText() const
{
	switch (CurrentAction)
	{
	case SSymbolDebugger::DebugAction_Inspect:
		return TEXT("Inspecting...");
	case SSymbolDebugger::DebugAction_Sync:
		return TEXT("Syncing...");
	case SSymbolDebugger::DebugAction_Debug:
		return TEXT("Launching debugger...");
	case SSymbolDebugger::DebugAction_Process:
		return TEXT("Processing crash dump...");
	}
	return TEXT("");
}

SSymbolDebugger::ESymbolDebuggerActionResults FSymbolDebugger::ActionHasCompleted(SSymbolDebugger::ESymbolDebuggerActions InAction)
{
	if (InAction == SSymbolDebugger::DebugAction_None)
	{
		return SSymbolDebugger::DebugResult_Success;
	}

	if (CurrentAction == SSymbolDebugger::DebugAction_None)
	{
		if (InAction == SSymbolDebugger::DebugAction_Inspect)
		{
			if (LastAction == SSymbolDebugger::DebugAction_Inspect)
			{
				// Clear the last action
				LastAction = SSymbolDebugger::DebugAction_None;

				if (SourceControlLabelName.Len() > 0)
				{
					return SSymbolDebugger::DebugResult_Success;
				}

				return SSymbolDebugger::DebugResult_Failure;
			}
		}
		else if (InAction == SSymbolDebugger::DebugAction_Sync)
		{
			if (LastAction == SSymbolDebugger::DebugAction_Sync)
			{
				// Clear the last action
				LastAction = SSymbolDebugger::DebugAction_None;

				if (bSyncSucceeded == true)
				{
					return SSymbolDebugger::DebugResult_Success;
				}

				return SSymbolDebugger::DebugResult_Failure;
			}
		}
		else if (InAction == SSymbolDebugger::DebugAction_Debug)
		{
			if (LastAction == SSymbolDebugger::DebugAction_Debug)
			{
				// Clear the last action
				LastAction = SSymbolDebugger::DebugAction_None;

				if (bLaunchDebugSucceeded == true)
				{
					return SSymbolDebugger::DebugResult_Success;
				}

				return SSymbolDebugger::DebugResult_Failure;
			}
		}
		else if (InAction == SSymbolDebugger::DebugAction_Process)
		{
			if (LastAction == SSymbolDebugger::DebugAction_Process)
			{
				// Clear the last action
				LastAction = SSymbolDebugger::DebugAction_None;

				if (bProcessCrashDumpSucceeded == true)
				{
					return SSymbolDebugger::DebugResult_Success;
				}

				return SSymbolDebugger::DebugResult_Failure;
			}
		}
	}

	return SSymbolDebugger::DebugResult_InProgress;
}

void FSymbolDebugger::Tick()
{
	static bool s_bFirstTick = true;
	if (s_bFirstTick)
	{
		// Initialize source control and show the login window
		CrashHelperModule->InitSourceControl(true);

		s_bFirstTick = false;
	}

	if (CurrentAction == SSymbolDebugger::DebugAction_Inspect)
	{
		if (InspectionTask.IsValid() && InspectionTask->IsDone())
		{
			SourceControlLabelName = InspectionTask->GetTask().GetResults_LabelName();
			if (CurrentMethod == SSymbolDebugger::DebugMethod_CrashDump)
			{
				PlatformName = InspectionTask->GetTask().GetResults_Platform();
				EngineVersionName = InspectionTask->GetTask().GetResults_EngineVersion();
			}

			InspectionTask = nullptr;

			LastAction = CurrentAction;
			CurrentAction = SSymbolDebugger::DebugAction_None;
		}
	}
	else if (CurrentAction == SSymbolDebugger::DebugAction_Sync)
	{
		if (SyncTask.IsValid() && SyncTask->IsDone())
		{
			bSyncSucceeded = SyncTask->GetTask().DidSucceed();

			SyncTask = nullptr;

			LastAction = CurrentAction;
			CurrentAction = SSymbolDebugger::DebugAction_None;
		}
	}
	else if (CurrentAction == SSymbolDebugger::DebugAction_Debug)
	{
		if (LaunchDebuggerTask.IsValid() && LaunchDebuggerTask->IsDone())
		{
			bLaunchDebugSucceeded = LaunchDebuggerTask->GetTask().DidSucceed();

			LaunchDebuggerTask = nullptr;

			LastAction = CurrentAction;
			CurrentAction = SSymbolDebugger::DebugAction_None;
		}
	}
	else if (CurrentAction == SSymbolDebugger::DebugAction_Process)
	{
		if (ProcessCrashDumpTask.IsValid() && ProcessCrashDumpTask->IsDone())
		{
			bProcessCrashDumpSucceeded = ProcessCrashDumpTask->GetTask().DidSucceed();

			ProcessCrashDumpTask = nullptr;

			LastAction = CurrentAction;
			CurrentAction = SSymbolDebugger::DebugAction_None;
		}
	}
}

bool FSymbolDebugger::OnInspect()
{
	switch (CurrentMethod)
	{
	case SSymbolDebugger::DebugMethod_CrashDump:
		return InspectCrashDump(CrashDumpName);
	case SSymbolDebugger::DebugMethod_EngineVersion:
		return InspectEngineVersion(EngineVersionName);
	case SSymbolDebugger::DebugMethod_ChangeList:
		return InspectChangelist(ChangelistName);
	}

	return false;
}

bool FSymbolDebugger::OnSync()
{
	if ((SourceControlLabelName.Len() > 0) && (PlatformName.Len() > 0))
	{
		return SyncFiles(SourceControlLabelName, PlatformName);
	}

	return false;
}

bool FSymbolDebugger::OnDebug()
{
	return DebugCrashDump(CrashDumpName);
}

bool FSymbolDebugger::OnProcess()
{
	return ProcessCrashDump(CrashDumpName);
}

bool FSymbolDebugger::InspectCrashDump(const FString& InCrashDumpName)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("InspectCrashDump called with %s\n"), *InCrashDumpName);

	CurrentAction = SSymbolDebugger::DebugAction_Inspect;
	// Start the async task
	InspectionTask = MakeShareable(new FAsyncTask<FSymbolDebugger_AsyncInspect>(InCrashDumpName, TEXT(""), TEXT(""), CrashHelperModule));
	InspectionTask->StartBackgroundTask();

	return true;
}

bool FSymbolDebugger::InspectEngineVersion(const FString& InEngineVersion)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("InspectEngineVersion called with %s\n"), *InEngineVersion);

	CurrentAction = SSymbolDebugger::DebugAction_Inspect;
	// Start the async task
	InspectionTask = MakeShareable(new FAsyncTask<FSymbolDebugger_AsyncInspect>(TEXT(""), InEngineVersion, TEXT(""), CrashHelperModule));
	InspectionTask->StartBackgroundTask();

	return true;
}

bool FSymbolDebugger::InspectChangelist(const FString& InChangelist)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("InspectChangelist called with %s\n"), *InChangelist);

	CurrentAction = SSymbolDebugger::DebugAction_Inspect;
	// Start the async task
	InspectionTask = MakeShareable(new FAsyncTask<FSymbolDebugger_AsyncInspect>(TEXT(""), TEXT(""), InChangelist, CrashHelperModule));
	InspectionTask->StartBackgroundTask();

	return true;
}

bool FSymbolDebugger::SyncFiles(const FString& InLabelName, const FString& InPlatform)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("SyncFiles called with %s for %s\n"), *InLabelName, *InPlatform);

	CurrentAction = SSymbolDebugger::DebugAction_Sync;
	// Start the async task
	SyncTask = MakeShareable(new FAsyncTask<FSymbolDebugger_AsyncSyncFiles>(InLabelName, InPlatform, CrashHelperModule));
	SyncTask->StartBackgroundTask();

	return true;
}

bool FSymbolDebugger::DebugCrashDump(const FString& InCrashDumpName)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("DebugCrashDump called with %s\n"), *InCrashDumpName);

	CurrentAction = SSymbolDebugger::DebugAction_Debug;
	// Start the async task
	LaunchDebuggerTask = MakeShareable(new FAsyncTask<FSymbolDebugger_LaunchDebugger>(InCrashDumpName));
	LaunchDebuggerTask->StartBackgroundTask();

	return true;
}

bool FSymbolDebugger::ProcessCrashDump(const FString& InCrashDumpName)
{
	if (CurrentAction != SSymbolDebugger::DebugAction_None)
	{
		// Can't launch an action with another in progress
		return false;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ProcessCrashDump called with %s\n"), *InCrashDumpName);

	CurrentAction = SSymbolDebugger::DebugAction_Process;
	// Start the async task
	ProcessCrashDumpTask = MakeShareable(new FAsyncTask<FSymbolDebugger_ProcessCrashDump>(InCrashDumpName, CrashHelperModule));
	ProcessCrashDumpTask->StartBackgroundTask();

	return true;
}
