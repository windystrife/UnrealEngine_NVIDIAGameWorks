// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerCommands.h"
#include "Misc/Paths.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Stats/StatsData.h"
#include "ProfilerSession.h"
#include "Widgets/SMultiDumpBrowser.h"
#include "Widgets/SProfilerWindow.h"
#include "DesktopPlatformModule.h"
#include "HAL/FileManagerGeneric.h"

#define LOCTEXT_NAMESPACE "FProfilerCommands"


/*-----------------------------------------------------------------------------
	FProfilerCommands
-----------------------------------------------------------------------------*/

FProfilerCommands::FProfilerCommands()
	: TCommands<FProfilerCommands>(
		TEXT( "ProfilerCommand" ), // Context name for fast lookup
		NSLOCTEXT("Contexts", "ProfilerCommand", "Profiler Command"), // Localized context name for displaying
		NAME_None, // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
{ }


/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FProfilerCommands::RegisterCommands()
{
	/*-----------------------------------------------------------------------------
		Global and custom commands.	
	-----------------------------------------------------------------------------*/

	UI_COMMAND( ToggleDataPreview, 	"Data Preview", "Toggles the data preview", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Control, EKeys::R ) );
	UI_COMMAND( ToggleDataCapture, "Data Capture", "Toggles the data capture", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Control, EKeys::C ) );
	UI_COMMAND( ToggleShowDataGraph, "Show Data Graph", "Toggles showing all data graphs", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND( OpenEventGraph, "Open Event Graph", "Opens a new event graph", EUserInterfaceActionType::Button, FInputChord() );

	/*-----------------------------------------------------------------------------
		Global commands.
	-----------------------------------------------------------------------------*/

	UI_COMMAND( ProfilerManager_Save, "Save", "Saves all collected data to file or files", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::S ) );
	UI_COMMAND( StatsProfiler, "Statistics", "Enables the Stats Profiler", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Control, EKeys::P ) );
#if PLATFORM_MAC
	UI_COMMAND( MemoryProfiler, "Memory", "Enables the Memory Profiler", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Command, EKeys::M ) );
	UI_COMMAND( FPSChart, "FPS Chart", "Shows the FPS Chart", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Command, EKeys::H ) );
#else
	UI_COMMAND( MemoryProfiler, "Memory", "Enables the Memory Profiler", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Control, EKeys::M ) );
	UI_COMMAND( FPSChart, "FPS Chart", "Shows the FPS Chart", EUserInterfaceActionType::ToggleButton, FInputChord( EModifierKey::Control, EKeys::H ) );
#endif

	UI_COMMAND( OpenSettings, "Settings", "Opens the settings for the profiler", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::O ) );

	UI_COMMAND( ProfilerManager_Load, "Load", "Loads profiler data", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control, EKeys::L ) );
	UI_COMMAND(ProfilerManager_LoadMultiple, "Load Folder", "Loads multiple stats dumps", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Shift, EKeys::L));
	UI_COMMAND(ProfilerManager_ToggleLivePreview, "Live preview", "Toggles the real time live preview", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND( DataGraph_ToggleViewMode, "Toggle graph view mode", "Toggles the data graph view mode between time based and index based", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND( DataGraph_ViewMode_SetTimeBased, "Time based", "Sets the data graph view mode to the time based", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( DataGraph_ViewMode_SetIndexBased, "Index based", "Sets the data graph view mode to the index based", EUserInterfaceActionType::RadioButton, FInputChord() );

	UI_COMMAND( EventGraph_SelectAllFrames, "Select all frames", "Selects all frames in the data graph and displays them in the event graph", EUserInterfaceActionType::Button, FInputChord() );
}
PRAGMA_ENABLE_OPTIMIZATION

//static_assert(sizeof(FProfilerActionManager) == 0, "Cannot contain any variables at this moment.");


/*-----------------------------------------------------------------------------
	FProfilerMenuBuilder
-----------------------------------------------------------------------------*/

void FProfilerMenuBuilder::AddMenuEntry( FMenuBuilder& MenuBuilder, const TSharedPtr< FUICommandInfo >& UICommandInfo, const FUIAction& UIAction )
{
	MenuBuilder.AddMenuEntry
	( 
		UICommandInfo->GetLabel(),
		UICommandInfo->GetDescription(),
		UICommandInfo->GetIcon(),
		UIAction,
		NAME_None, 
		UICommandInfo->GetUserInterfaceType()
	);
}


/*-----------------------------------------------------------------------------
	ToggleDataPreview
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ToggleDataPreview_Global()
{
	This->CommandList->MapAction( This->GetCommands().ToggleDataPreview, ToggleDataPreview_Custom( FGuid() ) );
}

const FUIAction FProfilerActionManager::ToggleDataPreview_Custom( const FGuid SessionInstanceID ) const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_Execute, SessionInstanceID );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_CanExecute, SessionInstanceID );
	UIAction.GetActionCheckState = FGetActionCheckState::CreateRaw( this, &FProfilerActionManager::ToggleDataPreview_GetCheckState, SessionInstanceID );
	return UIAction;
}

void FProfilerActionManager::ToggleDataPreview_Execute( const FGuid SessionInstanceID )
{	
	const bool bDataPreviewing = !This->IsDataPreviewing();
	This->SetDataPreview( bDataPreviewing );

	if (!bDataPreviewing)
	{
		This->bLivePreview = false;
	}
}

bool FProfilerActionManager::ToggleDataPreview_CanExecute( const FGuid SessionInstanceID ) const
{
	const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->ActiveInstanceID.IsValid();
	return bCanExecute;
}

ECheckBoxState FProfilerActionManager::ToggleDataPreview_GetCheckState( const FGuid SessionInstanceID ) const
{
	const bool bDataPreview = This->IsDataPreviewing();
	return bDataPreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


/*-----------------------------------------------------------------------------
	ProfilerManager_ToggleLivePreview
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ProfilerManager_ToggleLivePreview_Global()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_CanExecute );
	UIAction.GetActionCheckState = FGetActionCheckState::CreateRaw( this, &FProfilerActionManager::ProfilerManager_ToggleLivePreview_GetCheckState );

	This->CommandList->MapAction( This->GetCommands().ProfilerManager_ToggleLivePreview, UIAction );
}

void FProfilerActionManager::ProfilerManager_ToggleLivePreview_Execute()
{
	This->bLivePreview = !This->bLivePreview;
}

bool FProfilerActionManager::ProfilerManager_ToggleLivePreview_CanExecute() const
{
	const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->ActiveInstanceID.IsValid();
	return bCanExecute;
}

ECheckBoxState FProfilerActionManager::ProfilerManager_ToggleLivePreview_GetCheckState() const
{
	return This->bLivePreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


/*-----------------------------------------------------------------------------
	ProfilerManager_Load
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ProfilerManager_Load()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_Load_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ProfilerManager_Load_CanExecute );

	This->CommandList->MapAction( This->GetCommands().ProfilerManager_Load, UIAction );
}

void FProfilerActionManager::Map_ProfilerManager_LoadMultiple()
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw(this, &FProfilerActionManager::ProfilerManager_LoadMultiple_Execute);
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw(this, &FProfilerActionManager::ProfilerManager_Load_CanExecute);

	This->CommandList->MapAction(This->GetCommands().ProfilerManager_LoadMultiple, UIAction);
}

void FProfilerActionManager::ProfilerManager_Load_Execute()
{
	// @see FStatConstants::StatsFileExtension
	TArray<FString> OutFiles;
	const FString ProfilingDirectory = *FPaths::ConvertRelativePathToFull( *FPaths::ProfilingDir() );

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if( DesktopPlatform != NULL )
	{
		bOpened = DesktopPlatform->OpenFileDialog
		(
			NULL, 
			LOCTEXT("ProfilerManager_LoadFile_Desc", "Open profiler capture file...").ToString(),
			ProfilingDirectory, 
			TEXT(""), 
			LOCTEXT("ProfilerManager_Load_FileFilter", "Stats files (*.ue4stats)|*.ue4stats|Raw Stats files (*.ue4statsraw)|*.ue4statsraw").ToString(), 
			EFileDialogFlags::None,
			OutFiles
		);
	}

	if( bOpened == true )
	{
		if( OutFiles.Num() == 1 )
		{
			const FString DraggedFileExtension = FPaths::GetExtension( OutFiles[0], true );
			if( DraggedFileExtension == FStatConstants::StatsFileExtension )
			{
				This->LoadProfilerCapture( OutFiles[0] );

			}
			else if( DraggedFileExtension == FStatConstants::StatsFileRawExtension )
			{
				This->LoadRawStatsFile( OutFiles[0] );
			}
		}
	}
}


void FProfilerActionManager::ProfilerManager_LoadMultiple_Execute()
{
	// @see FStatConstants::StatsFileExtension
	FString OutFolder;
	const FString ProfilingDirectory = *FPaths::ConvertRelativePathToFull(*FPaths::ProfilingDir());

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform != NULL)
	{
		bOpened = DesktopPlatform->OpenDirectoryDialog(NULL,
			LOCTEXT("ProfilerManager_Load_Desc", "Open capture folder...").ToString(),
			ProfilingDirectory,
			OutFolder);
	}

	if (bOpened == true)
	{
		This->ProfilerWindow.Pin()->MultiDumpBrowser->Clear();

		if (!OutFolder.IsEmpty())
		{
			TArray<FString> FoundFiles;
			FFileManagerGeneric::Get().FindFiles(FoundFiles, *OutFolder, TEXT(".ue4stats"));
			for (FString &FilePath : FoundFiles)
			{
				const TCHAR* PathDelimiter = FPlatformMisc::GetDefaultPathSeparator();
				SMultiDumpBrowser::FFileDescriptor *Desc = new SMultiDumpBrowser::FFileDescriptor();
				Desc->FullPath = OutFolder + PathDelimiter + FilePath;
				Desc->DisplayName = FilePath;
				This->ProfilerWindow.Pin()->MultiDumpBrowser->AddFile(Desc);
			}
			This->ProfilerWindow.Pin()->MultiDumpBrowser->Update();
		}
	}
}


bool FProfilerActionManager::ProfilerManager_Load_CanExecute() const
{
	const bool bIsConnectionActive = This->IsDataCapturing() || This->IsDataPreviewing() || This->IsLivePreview();
	return !(bIsConnectionActive && This->ProfilerType == EProfilerSessionTypes::Live);
}


/*-----------------------------------------------------------------------------
	ToggleDataCapture
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_ToggleDataCapture_Global()
{
	This->CommandList->MapAction( This->GetCommands().ToggleDataCapture, ToggleDataCapture_Custom( FGuid() ) );
}

const FUIAction FProfilerActionManager::ToggleDataCapture_Custom( const FGuid SessionInstanceID ) const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_Execute, SessionInstanceID );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_CanExecute, SessionInstanceID );
	UIAction.GetActionCheckState = FGetActionCheckState::CreateRaw( this, &FProfilerActionManager::ToggleDataCapture_GetCheckState, SessionInstanceID );
	return UIAction;
}

void FProfilerActionManager::ToggleDataCapture_Execute( const FGuid SessionInstanceID )
{
	const bool bDataCapturing = This->IsDataCapturing();
	This->SetDataCapture( !bDataCapturing );

	// Assumes that when data capturing is off, we have captured stats files on the service side.
	const bool bNewDataCapturing = This->IsDataCapturing();
	if (!bNewDataCapturing)
	{
		EAppReturnType::Type Result = FPlatformMisc::MessageBoxExt
		( 
			EAppMsgType::YesNo, 
			*LOCTEXT("TransferServiceSideCaptureQuestion", "Would like to transfer the captured stats file(s) to this machine? This may take some time.").ToString(), 
			*LOCTEXT("Question", "Question").ToString() 
		);

		if( Result == EAppReturnType::Yes )
		{
			This->ProfilerClient->RequestLastCapturedFile();
		}
	}
}

bool FProfilerActionManager::ToggleDataCapture_CanExecute( const FGuid SessionInstanceID ) const
{
	const bool bCanExecute = This->ActiveSession.IsValid() && This->ProfilerType == EProfilerSessionTypes::Live && This->ActiveInstanceID.IsValid();
	return bCanExecute;
}

ECheckBoxState FProfilerActionManager::ToggleDataCapture_GetCheckState( const FGuid SessionInstanceID ) const
{
	const bool bDataCapturing = This->IsDataCapturing();
	return bDataCapturing ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


/*-----------------------------------------------------------------------------
		OpenSettings
-----------------------------------------------------------------------------*/

void FProfilerActionManager::Map_OpenSettings_Global()
{
	This->CommandList->MapAction( This->GetCommands().OpenSettings, OpenSettings_Custom() );
}

const FUIAction FProfilerActionManager::OpenSettings_Custom() const
{
	FUIAction UIAction;
	UIAction.ExecuteAction = FExecuteAction::CreateRaw( this, &FProfilerActionManager::OpenSettings_Execute );
	UIAction.CanExecuteAction = FCanExecuteAction::CreateRaw( this, &FProfilerActionManager::OpenSettings_CanExecute );
	return UIAction;
}

void FProfilerActionManager::OpenSettings_Execute()
{
	This->GetProfilerWindow()->OpenProfilerSettings();
}

bool FProfilerActionManager::OpenSettings_CanExecute() const
{
	return !This->Settings.IsEditing();
}

#undef LOCTEXT_NAMESPACE
