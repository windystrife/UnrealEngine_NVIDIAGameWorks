// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SymbolDebuggerApp.h"
#include "SSymbolDebugger.h"
#include "SymbolDebugger.h"
#include "RequiredProgramMainCPPInclude.h"
#include "CrashDebugHelperModule.h"
#include "AsyncWork.h"
#include "ISourceControlModule.h"
#include "EditorStyleSet.h"
#include "Interfaces/IEditorStyleModule.h"

IMPLEMENT_APPLICATION(SymbolDebugger, "SymbolDebugger");

void RunSymbolDebugger(const TCHAR* CommandLine)
{
	// start up the main loop
	GEngineLoop.PreInit(CommandLine);

	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication( GetStandardStandaloneRenderer() );

	// The source control plugins currently rely on EditorStyle being loaded
	FModuleManager::LoadModuleChecked<IEditorStyleModule>("EditorStyle");

	// Load in the perforce source control plugin, as standalone programs don't currently support plugins and
	// we don't support any other provider apart from Perforce in this module.
	IModuleInterface& PerforceSourceControlModule = FModuleManager::LoadModuleChecked<IModuleInterface>( FName( "PerforceSourceControl" ) );

	// make sure our provider is set to Perforce
	ISourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<ISourceControlModule>( FName( "SourceControl" ) );
	SourceControlModule.SetProvider(FName("Perforce"));

	// Create the symbol debugger helper
	TSharedPtr<FSymbolDebugger> SymbolDebugger = MakeShareable(new FSymbolDebugger());
	checkf(SymbolDebugger.IsValid(), TEXT("Failed to create SymbolDebugger"));

	// open up the SymbolDebugger windows
	{
		TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("SymbolDebugger", "SymbolDebuggerAppName", "Symbol Debugger"))
		.ClientSize(FVector2D(400, 300))
		[
			SNew(SSymbolDebugger)
			.OnGetCurrentMethod(SymbolDebugger.Get(), &FSymbolDebugger::GetCurrentMethod)
			.OnSetCurrentMethod(SymbolDebugger.Get(), &FSymbolDebugger::SetCurrentMethod)
			.OnGetMethodText(SymbolDebugger.Get(), &FSymbolDebugger::GetMethodText)
			.OnSetMethodText(SymbolDebugger.Get(), &FSymbolDebugger::SetMethodText)
			.OnFileOpen(SymbolDebugger.Get(), &FSymbolDebugger::OnFileOpen)
			.OnGetTextField(SymbolDebugger.Get(), &FSymbolDebugger::GetTextField)
			.OnSetTextField(SymbolDebugger.Get(), &FSymbolDebugger::SetTextField)
			.OnGetCurrentAction(SymbolDebugger.Get(), &FSymbolDebugger::GetCurrentAction)
			.IsActionEnabled(SymbolDebugger.Get(), &FSymbolDebugger::IsActionEnabled)
			.OnAction(SymbolDebugger.Get(), &FSymbolDebugger::OnAction)
			.OnGetStatusText(SymbolDebugger.Get(), &FSymbolDebugger::GetStatusText)
			.HasActionCompleted(SymbolDebugger.Get(), &FSymbolDebugger::ActionHasCompleted)
		];
	
		FSlateApplication::Get().AddWindow(Window);
	}

#if WITH_SHARED_POINTER_TESTS
	SharedPointerTesting::TestSharedPointer< ESPMode::Fast >();
	SharedPointerTesting::TestSharedPointer< ESPMode::ThreadSafe >();
#endif

	// loop while the server does the rest
	double LastTime = FPlatformTime::Seconds();
	while (!GIsRequestingExit)
	{
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		ISourceControlModule::Get().Tick();

		// Tick the helper
		SymbolDebugger->Tick();

		// Sleep
		FPlatformProcess::Sleep(0);
	}

	PerforceSourceControlModule.ShutdownModule();

	FSlateApplication::Shutdown();
}

