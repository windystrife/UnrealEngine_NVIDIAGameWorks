// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OutputLogModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "SDebugConsole.h"
#include "SOutputLog.h"
#include "SDeviceOutputLog.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

IMPLEMENT_MODULE( FOutputLogModule, OutputLog );

namespace OutputLogModule
{
	static const FName OutputLogTabName = FName(TEXT("OutputLog"));
	static const FName DeviceOutputLogTabName = FName(TEXT("DeviceOutputLog"));
}

/** This class is to capture all log output even if the log window is closed */
class FOutputLogHistory : public FOutputDevice
{
public:

	FOutputLogHistory()
	{
		GLog->AddOutputDevice(this);
		GLog->SerializeBacklog(this);
	}

	~FOutputLogHistory()
	{
		// At shutdown, GLog may already be null
		if( GLog != NULL )
		{
			GLog->RemoveOutputDevice(this);
		}
	}

	/** Gets all captured messages */
	const TArray< TSharedPtr<FLogMessage> >& GetMessages() const
	{
		return Messages;
	}

protected:

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
		// Capture all incoming messages and store them in history
		SOutputLog::CreateLogMessages(V, Verbosity, Category, Messages);
	}

private:

	/** All log messsges since this module has been started */
	TArray< TSharedPtr<FLogMessage> > Messages;
};

/** Our global output log app spawner */
static TSharedPtr<FOutputLogHistory> OutputLogHistory;

TSharedRef<SDockTab> SpawnOutputLog( const FSpawnTabArgs& Args )
{
	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Log.TabIcon"))
		.TabRole( ETabRole::NomadTab )
		.Label( NSLOCTEXT("OutputLog", "TabTitle", "Output Log") )
		[
			SNew(SOutputLog).Messages( OutputLogHistory->GetMessages() )
		];
}

TSharedRef<SDockTab> SpawnDeviceOutputLog( const FSpawnTabArgs& Args )
{
	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("Log.TabIcon"))
		.TabRole( ETabRole::NomadTab )
		.Label( NSLOCTEXT("OutputLog", "DeviceTabTitle", "Device Output Log") )
		[
			SNew(SDeviceOutputLog)
		];
}

void FOutputLogModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::OutputLogTabName, FOnSpawnTab::CreateStatic( &SpawnOutputLog ) )
		.SetDisplayName(NSLOCTEXT("UnrealEditor", "OutputLogTab", "Output Log"))
		.SetTooltipText(NSLOCTEXT("UnrealEditor", "OutputLogTooltipText", "Open the Output Log tab."))
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory() )
		.SetIcon( FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon") );

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OutputLogModule::DeviceOutputLogTabName, FOnSpawnTab::CreateStatic( &SpawnDeviceOutputLog ) )
		.SetDisplayName(NSLOCTEXT("UnrealEditor", "DeviceOutputLogTab", "Device Output Log"))
		.SetTooltipText(NSLOCTEXT("UnrealEditor", "DeviceOutputLogTooltipText", "Open the Device Output Log tab."))
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory() )
		.SetIcon( FSlateIcon(FEditorStyle::GetStyleSetName(), "Log.TabIcon") )
		.SetAutoGenerateMenuEntry(false); // remove once not Experimental
	
	OutputLogHistory = MakeShareable(new FOutputLogHistory);
}

void FOutputLogModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OutputLogModule::OutputLogTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OutputLogModule::DeviceOutputLogTabName);
	}
}

TSharedRef< SWidget > FOutputLogModule::MakeConsoleInputBox( TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox ) const
{
	TSharedRef< SConsoleInputBox > NewConsoleInputBox = SNew( SConsoleInputBox );
	OutExposedEditableTextBox = NewConsoleInputBox->GetEditableTextBox();
	return NewConsoleInputBox;
}


void FOutputLogModule::ToggleDebugConsoleForWindow( const TSharedRef< SWindow >& Window, const EDebugConsoleStyle::Type InStyle, const FDebugConsoleDelegates& DebugConsoleDelegates )
{
	bool bShouldOpen = true;
	// Close an existing console box, if there is one
	TSharedPtr< SWidget > PinnedDebugConsole( DebugConsole.Pin() );
	if( PinnedDebugConsole.IsValid() )
	{
		// If the console is already open close it unless it is in a different window.  In that case reopen it on that window
		bShouldOpen = false;
		TSharedPtr< SWindow > WindowForExistingConsole = FSlateApplication::Get().FindWidgetWindow(PinnedDebugConsole.ToSharedRef());
		if (WindowForExistingConsole.IsValid())
		{
			WindowForExistingConsole->RemoveOverlaySlot(PinnedDebugConsole.ToSharedRef());
			DebugConsole.Reset();
		}

		if( WindowForExistingConsole != Window )
		{
			// Console is being opened on another window
			bShouldOpen = true;
		}
	}
	
	TSharedPtr<SDockTab> ActiveTab = FGlobalTabmanager::Get()->GetActiveTab();
	if (ActiveTab.IsValid() && ActiveTab->GetLayoutIdentifier() == FTabId(OutputLogModule::OutputLogTabName))
	{
		FGlobalTabmanager::Get()->DrawAttention(ActiveTab.ToSharedRef());
		bShouldOpen = false;
	}

	if( bShouldOpen )
	{
		const EDebugConsoleStyle::Type DebugConsoleStyle = InStyle;
		TSharedRef< SDebugConsole > DebugConsoleRef = SNew( SDebugConsole, DebugConsoleStyle, this, &DebugConsoleDelegates );
		DebugConsole = DebugConsoleRef;

		const int32 MaximumZOrder = MAX_int32;
		Window->AddOverlaySlot( MaximumZOrder )
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Center)
		.Padding( 10.0f )
		[
			DebugConsoleRef
		];

		// Force keyboard focus
		DebugConsoleRef->SetFocusToEditableText();
	}
}


void FOutputLogModule::CloseDebugConsole()
{
	TSharedPtr< SWidget > PinnedDebugConsole( DebugConsole.Pin() );

	if( PinnedDebugConsole.IsValid() )
	{
		TSharedPtr< SWindow > WindowForExistingConsole = FSlateApplication::Get().FindWidgetWindow(PinnedDebugConsole.ToSharedRef());
		if (WindowForExistingConsole.IsValid())
		{
			WindowForExistingConsole->RemoveOverlaySlot( PinnedDebugConsole.ToSharedRef() );
			DebugConsole.Reset();
		}
	}
}
