// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MainFrameModule.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "GameProjectGenerationModule.h"
#include "MessageLogModule.h"
#include "MRUFavoritesList.h"
#include "OutputLogModule.h"
#include "EditorStyleSet.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Sound/SoundBase.h"
#include "ISourceCodeAccessor.h"
#include "ISourceCodeAccessModule.h"
#include "Menus/MainMenu.h"
#include "Frame/RootWindowLocation.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Developer/HotReload/Public/IHotReload.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Commands/GenericCommands.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Editor/EditorPerformanceSettings.h"
#include "HAL/PlatformApplicationMisc.h"

DEFINE_LOG_CATEGORY(LogMainFrame);
#define LOCTEXT_NAMESPACE "FMainFrameModule"


const FText StaticGetApplicationTitle( const bool bIncludeGameName )
{
	static const FText ApplicationTitle = NSLOCTEXT("UnrealEditor", "ApplicationTitle", "Unreal Editor");

	if (bIncludeGameName && FApp::HasProjectName())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("GameName"), FText::FromString( FString( FApp::GetProjectName())));
		Args.Add(TEXT("AppTitle"), ApplicationTitle);

		const EBuildConfigurations::Type BuildConfig = FApp::GetBuildConfiguration();

		if (BuildConfig != EBuildConfigurations::Shipping && BuildConfig != EBuildConfigurations::Development && BuildConfig != EBuildConfigurations::Unknown)
		{
			Args.Add( TEXT("Config"), EBuildConfigurations::ToText(BuildConfig));

			return FText::Format( NSLOCTEXT("UnrealEditor", "AppTitleGameNameWithConfig", "{GameName} [{Config}] - {AppTitle}"), Args );
		}

		return FText::Format( NSLOCTEXT("UnrealEditor", "AppTitleGameName", "{GameName} - {AppTitle}"), Args );
	}

	return ApplicationTitle;
}


/* IMainFrameModule implementation
 *****************************************************************************/

void FMainFrameModule::CreateDefaultMainFrame( const bool bStartImmersive, const bool bStartPIE )
{
	if (!IsWindowInitialized())
	{
		FRootWindowLocation DefaultWindowLocation;

		bool bEmbedTitleAreaContent = true;
		bool bIsUserSizable = true;
		bool bSupportsMaximize = true;
		bool bSupportsMinimize = true;
		EAutoCenter CenterRules = EAutoCenter::None;
		FText WindowTitle;
		if ( ShouldShowProjectDialogAtStartup() )
		{
			// Force tabs restored from layout that have no window (the LevelEditor tab) to use a docking area with
			// embedded title area content.  We need to override the behavior here because we're creating the actual
			// window ourselves instead of letting the tab management system create it for us.
			bEmbedTitleAreaContent = false;

			// Do not maximize the window initially. Keep a small dialog feel.
			DefaultWindowLocation.InitiallyMaximized = false;

			DefaultWindowLocation.WindowSize = GetProjectBrowserWindowSize();

			bIsUserSizable = true;
			bSupportsMaximize = true;
			bSupportsMinimize = true;
			CenterRules = EAutoCenter::PreferredWorkArea;;
			// When opening the project dialog, show "Project Browser" in the window title
			WindowTitle = LOCTEXT("ProjectBrowserDialogTitle", "Unreal Project Browser");
		}
		else
		{
			if( bStartImmersive )
			{
				// Start maximized if we are in immersive mode
				DefaultWindowLocation.InitiallyMaximized = true;
			}

			const bool bIncludeGameName = true;
			WindowTitle = GetApplicationTitle( bIncludeGameName );
		}

		TSharedRef<SWindow> RootWindow = SNew(SWindow)
			.AutoCenter(CenterRules)
			.Title( WindowTitle )
			.IsInitiallyMaximized( DefaultWindowLocation.InitiallyMaximized )
			.ScreenPosition( DefaultWindowLocation.ScreenPosition )
			.ClientSize( DefaultWindowLocation.WindowSize )
			.CreateTitleBar( !bEmbedTitleAreaContent )
			.SizingRule( bIsUserSizable ? ESizingRule::UserSized : ESizingRule::FixedSize )
			.SupportsMaximize( bSupportsMaximize )
			.SupportsMinimize( bSupportsMinimize );

		const bool bShowRootWindowImmediately = false;
		FSlateApplication::Get().AddWindow( RootWindow, bShowRootWindowImmediately );

		FGlobalTabmanager::Get()->SetRootWindow(RootWindow);
		FSlateNotificationManager::Get().SetRootWindow(RootWindow);

		TSharedPtr<SWidget> MainFrameContent;
		bool bLevelEditorIsMainTab = false;
		if ( ShouldShowProjectDialogAtStartup() )
		{
			MainFrameContent = FGameProjectGenerationModule::Get().CreateGameProjectDialog(/*bAllowProjectOpening=*/true, /*bAllowProjectCreate=*/true);
		}
		else
		{
			// Get desktop metrics
			FDisplayMetrics DisplayMetrics;
			FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

			const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

			// Setup a position and size for the main frame window that's centered in the desktop work area
			const float CenterScale = 0.65f;
			const FVector2D DisplaySize(
				DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
				DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );
			const FVector2D WindowSize = (CenterScale * DisplaySize) / DPIScale;

			TSharedRef<FTabManager::FLayout> LoadedLayout = FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni,
				// We persist the positioning of the level editor and the content browser.
				// The asset editors currently do not get saved.
				FTabManager::NewLayout( "UnrealEd_Layout_v1.4" )
				->AddArea
				(
					// level editor window
					FTabManager::NewPrimaryArea()
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(2.0f)
						->AddTab("LevelEditor", ETabState::OpenedTab)
						->AddTab("DockedToolkit", ETabState::ClosedTab)
					)
				)
				->AddArea
				(
					// content browser window
					FTabManager::NewArea(WindowSize)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.0f)
						->AddTab("ContentBrowser1Tab", ETabState::ClosedTab)
					)
				)
				->AddArea
				(
					// toolkits window
					FTabManager::NewArea(WindowSize)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.0f)
						->AddTab("StandaloneToolkit", ETabState::ClosedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.35f)
						->AddTab("MergeTool", ETabState::ClosedTab)
					)
				)
				->AddArea
				(
					// settings window
					FTabManager::NewArea(WindowSize)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(1.0f)
						->AddTab("EditorSettings", ETabState::ClosedTab)
						->AddTab("ProjectSettings", ETabState::ClosedTab)
						->AddTab("PluginsEditor", ETabState::ClosedTab)
					)
				)
			);

			MainFrameContent = FGlobalTabmanager::Get()->RestoreFrom( LoadedLayout, RootWindow, bEmbedTitleAreaContent );
			bLevelEditorIsMainTab = true;
		}

		RootWindow->SetContent(MainFrameContent.ToSharedRef());

		TSharedPtr<SDockTab> MainTab;
		if ( bLevelEditorIsMainTab )
		{
			MainTab = FGlobalTabmanager::Get()->InvokeTab( FTabId("LevelEditor") );

			// make sure we only allow the message log to be shown when we have a level editor main tab
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
			MessageLogModule.EnableMessageLogDisplay(!FApp::IsUnattended());
		}

		// Initialize the main frame window
		MainFrameHandler->OnMainFrameGenerated( MainTab, RootWindow );
		
		// Show the window!
		MainFrameHandler->ShowMainFrameWindow( RootWindow, bStartImmersive, bStartPIE );
		
		MRUFavoritesList = new FMainMRUFavoritesList;
		MRUFavoritesList->ReadFromINI();

		MainFrameCreationFinishedEvent.Broadcast(RootWindow, ShouldShowProjectDialogAtStartup());
	}
}


TSharedRef<SWidget> FMainFrameModule::MakeMainMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > Extender ) const
{
	return FMainMenu::MakeMainMenu( TabManager, Extender );
}


TSharedRef<SWidget> FMainFrameModule::MakeMainTabMenu( const TSharedPtr<FTabManager>& TabManager, const TSharedRef< FExtender > Extender ) const
{
	return FMainMenu::MakeMainTabMenu( TabManager, Extender );
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> FMainFrameModule::MakeDeveloperTools() const
{
	struct Local
	{
		static FText GetFrameRateAsString() 
		{
			// Clamp to avoid huge averages at startup or after hitches
			const float AverageFPS = 1.0f / FSlateApplication::Get().GetAverageDeltaTime();
			const float ClampedFPS = ( AverageFPS < 0.0f || AverageFPS > 4000.0f ) ? 0.0f : AverageFPS;

			static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(1)
				.SetMaximumFractionalDigits(1);
			return FText::AsNumber( ClampedFPS, &FormatOptions );
		}

		static FText GetFrameTimeAsString() 
		{
			// Clamp to avoid huge averages at startup or after hitches
			const float AverageMS = FSlateApplication::Get().GetAverageDeltaTime() * 1000.0f;
			const float ClampedMS = ( AverageMS < 0.0f || AverageMS > 4000.0f ) ? 0.0f : AverageMS;

			static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(1)
				.SetMaximumFractionalDigits(1);
			static const FText FrameTimeFmt = FText::FromString(TEXT("{0} ms"));
			return FText::Format( FrameTimeFmt, FText::AsNumber( ClampedMS, &FormatOptions ) );
		}

		static FText GetMemoryAsString() 
		{
			// Only refresh process memory allocated after every so often, to reduce fixed frame time overhead
			static SIZE_T StaticLastTotalAllocated = 0;
			static int32 QueriesUntilUpdate = 1;
			if( --QueriesUntilUpdate <= 0 )
			{
				// Query OS for process memory used
				FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
				StaticLastTotalAllocated = MemoryStats.UsedPhysical;

				// Wait 60 queries until we refresh memory again
				QueriesUntilUpdate = 60;
			}

			static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(2)
				.SetMaximumFractionalDigits(2);
			static const FText MemorySizeFmt = FText::FromString(TEXT("{0} mb"));
			return FText::Format( MemorySizeFmt, FText::AsNumber( (float)StaticLastTotalAllocated / ( 1024.0f * 1024.0f ), &FormatOptions ) );
		}

		static FText GetUObjectCountAsString() 
		{
			return FText::AsNumber(GUObjectArray.GetObjectArrayNumMinusAvailable());
		}

		static void OpenVideo( FString SourceFilePath )
		{
			FPlatformProcess::ExploreFolder( *( FPaths::GetPath(SourceFilePath) ) );
		}


		/** @return Returns true if frame rate and memory should be displayed in the UI */
		static EVisibility ShouldShowFrameRateAndMemory()
		{
			return GetDefault<UEditorPerformanceSettings>()->bShowFrameRateAndMemory ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
		}
	};



	// We need the output log module in order to instantiate SConsoleInputBox widgets
	const FOutputLogModule& OutputLogModule = FModuleManager::LoadModuleChecked< FOutputLogModule >(TEXT("OutputLog"));

	const FSlateFontInfo& SmallFixedFont = FEditorStyle::GetFontStyle(TEXT("MainFrame.DebugTools.SmallFont") );
	const FSlateFontInfo& NormalFixedFont = FEditorStyle::GetFontStyle(TEXT("MainFrame.DebugTools.NormalFont") );
	const FSlateFontInfo& LabelFont = FEditorStyle::GetFontStyle(TEXT("MainFrame.DebugTools.LabelFont") );

	TSharedPtr< SWidget > DeveloperTools;
	TSharedPtr< SEditableTextBox > ExposedEditableTextBox;

	TSharedRef<SWidget> FrameRateAndMemoryWidget =
		SNew( SHorizontalBox )
		.Visibility_Static( &Local::ShouldShowFrameRateAndMemory )

		// FPS
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 0.0f, 0.0f, 4.0f, 0.0f )
		[
			SNew( SHorizontalBox )
			.Visibility( GIsDemoMode ? EVisibility::Collapsed : EVisibility::HitTestInvisible )
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Bottom)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("FrameRateLabel", "FPS: ") )
				.Font( LabelFont )
				.ColorAndOpacity( FLinearColor( 0.3f, 0.3f, 0.3f ) )
			]
			+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew( STextBlock )
					.Text_Static( &Local::GetFrameRateAsString )
					.Font(NormalFixedFont)
					.ColorAndOpacity( FLinearColor( 0.6f, 0.6f, 0.6f ) )
				]
			+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding( 4.0f, 0.0f, 0.0f, 0.0f )
				[
					SNew( STextBlock )
					.Text( LOCTEXT("FrameRate/FrameTime", "/") )
					.Font( SmallFixedFont )
					.ColorAndOpacity( FLinearColor( 0.4f, 0.4f, 0.4f ) )
				]
			+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding( 4.0f, 0.0f, 0.0f, 0.0f )
				[
					SNew( STextBlock )
					.Text_Static( &Local::GetFrameTimeAsString )
					.Font( SmallFixedFont )
					.ColorAndOpacity( FLinearColor( 0.4f, 0.4f, 0.4f ) )
				]
		]

		// Memory
		+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 4.0f, 0.0f, 4.0f, 0.0f )
			[
				SNew( SHorizontalBox )
				.Visibility( GIsDemoMode ? EVisibility::Collapsed : EVisibility::HitTestInvisible )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew( STextBlock )
					.Text( LOCTEXT("MemoryLabel", "Mem: ") )
					.Font( LabelFont )
					.ColorAndOpacity( FLinearColor( 0.3f, 0.3f, 0.3f ) )
				]
				+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SNew( STextBlock )
						.Text_Static( &Local::GetMemoryAsString )
						.Font(NormalFixedFont)
						.ColorAndOpacity( FLinearColor( 0.6f, 0.6f, 0.6f ) )
					]
			]

		// UObject count
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 4.0f, 0.0f, 4.0f, 0.0f )
		[
			SNew( SHorizontalBox )
				.Visibility( GIsDemoMode ? EVisibility::Collapsed : EVisibility::HitTestInvisible )
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew( STextBlock )
						.Text( LOCTEXT("UObjectCountLabel", "Objs: ") )
						.Font( LabelFont )
						.ColorAndOpacity( FLinearColor( 0.3f, 0.3f, 0.3f ) )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				[
					SNew( STextBlock )
						.Text_Static( &Local::GetUObjectCountAsString )
						.Font(NormalFixedFont)
						.ColorAndOpacity( FLinearColor( 0.6f, 0.6f, 0.6f ) )
				]
		]
	;


	// Invisible border, so that we can animate our box panel size
	return SNew( SBorder )
		.Visibility( EVisibility::SelfHitTestInvisible )
		.Padding( FMargin(0.0f, 0.0f, 0.0f, 1.0f) )
		.VAlign(VAlign_Bottom)
		.BorderImage( FEditorStyle::GetBrush("NoBorder") )
		[
			SNew( SHorizontalBox )
			.Visibility( EVisibility::SelfHitTestInvisible )

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 0.0f )
			[
				FrameRateAndMemoryWidget
			]
			
			//+ SHorizontalBox::Slot()
			//.AutoWidth()
			//.VAlign(VAlign_Bottom)
			//.Padding( 0.0f )
			//[
			//	SNew(SBox)
			//	.Padding( FMargin( 4.0f, 0.0f, 0.0f, 0.0f ) )
			//	[
			//		OutputLogModule.MakeConsoleInputBox( ExposedEditableTextBox )
			//	]
			//]
		];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void FMainFrameModule::SetLevelNameForWindowTitle( const FString& InLevelFileName )
{
	LoadedLevelName = (InLevelFileName.Len() > 0)
		? FPaths::GetBaseFilename(InLevelFileName)
		: NSLOCTEXT("UnrealEd", "Untitled", "Untitled" ).ToString();
}


/* IModuleInterface implementation
 *****************************************************************************/

void FMainFrameModule::StartupModule( )
{
	MRUFavoritesList = NULL;

	ensureMsgf(!IsRunningGame(), TEXT("The MainFrame module should only be loaded when running the editor.  Code that extends the editor, adds menu items, etc... should not run when running in -game mode or in a non-WITH_EDITOR build"));
	MainFrameHandler = MakeShareable(new FMainFrameHandler);

	FGenericCommands::Register();
	FMainFrameCommands::Register();

	SetLevelNameForWindowTitle(TEXT(""));

	// Register to find out about when hot reload completes, so we can show a notification
	IHotReloadModule& HotReloadModule = IHotReloadModule::Get();
	HotReloadModule.OnModuleCompilerStarted().AddRaw( this, &FMainFrameModule::HandleLevelEditorModuleCompileStarted );
	HotReloadModule.OnModuleCompilerFinished().AddRaw( this, &FMainFrameModule::HandleLevelEditorModuleCompileFinished );
	HotReloadModule.OnHotReload().AddRaw( this, &FMainFrameModule::HandleHotReloadFinished );

#if WITH_EDITOR
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	SourceCodeAccessModule.OnLaunchingCodeAccessor().AddRaw( this, &FMainFrameModule::HandleCodeAccessorLaunching );
	SourceCodeAccessModule.OnDoneLaunchingCodeAccessor().AddRaw( this, &FMainFrameModule::HandleCodeAccessorLaunched );
	SourceCodeAccessModule.OnOpenFileFailed().AddRaw( this, &FMainFrameModule::HandleCodeAccessorOpenFileFailed );
#endif

	// load sounds
	CompileStartSound = LoadObject<USoundBase>(NULL, TEXT("/Engine/EditorSounds/Notifications/CompileStart_Cue.CompileStart_Cue"));
	CompileStartSound->AddToRoot();
	CompileSuccessSound = LoadObject<USoundBase>(NULL, TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue"));
	CompileSuccessSound->AddToRoot();
	CompileFailSound = LoadObject<USoundBase>(NULL, TEXT("/Engine/EditorSounds/Notifications/CompileFailed_Cue.CompileFailed_Cue"));
	CompileFailSound->AddToRoot();

	ModuleCompileStartTime = 0.0f;

	// migrate old layout settings
	FLayoutSaveRestore::MigrateConfig(GEditorPerProjectIni, GEditorLayoutIni);
}


void FMainFrameModule::ShutdownModule( )
{
	// Destroy the main frame window
	TSharedPtr< SWindow > ParentWindow( GetParentWindow() );
	if( ParentWindow.IsValid() )
	{
		ParentWindow->DestroyWindowImmediately();
	}

	MainFrameHandler.Reset();

	FMainFrameCommands::Unregister();

	if( IHotReloadModule::IsAvailable() )
	{
		IHotReloadModule& HotReloadModule = IHotReloadModule::Get();
		HotReloadModule.OnHotReload().RemoveAll( this );
		HotReloadModule.OnModuleCompilerStarted().RemoveAll( this );
		HotReloadModule.OnModuleCompilerFinished().RemoveAll( this );
	}

#if WITH_EDITOR
	if(FModuleManager::Get().IsModuleLoaded("SourceCodeAccess"))
	{
		ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::GetModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
		SourceCodeAccessModule.OnLaunchingCodeAccessor().RemoveAll( this );
		SourceCodeAccessModule.OnDoneLaunchingCodeAccessor().RemoveAll( this );
		SourceCodeAccessModule.OnOpenFileFailed().RemoveAll( this );
	}
#endif

	if(CompileStartSound != NULL)
	{
		if (!GExitPurge)
		{
			CompileStartSound->RemoveFromRoot();
		}
		CompileStartSound = NULL;
	}

	if(CompileSuccessSound != NULL)
	{
		if (!GExitPurge)
		{
			CompileSuccessSound->RemoveFromRoot();
		}
		CompileSuccessSound = NULL;
	}

	if(CompileFailSound != NULL)
	{
		if (!GExitPurge)
		{
			CompileFailSound->RemoveFromRoot();
		}
		CompileFailSound = NULL;
	}
}


/* FMainFrameModule implementation
 *****************************************************************************/

bool FMainFrameModule::ShouldShowProjectDialogAtStartup( ) const
{
	return !FApp::HasProjectName();
}


/* FMainFrameModule event handlers
 *****************************************************************************/

void FMainFrameModule::HandleLevelEditorModuleCompileStarted( bool bIsAsyncCompile )
{
	ModuleCompileStartTime = FPlatformTime::Seconds();

	if (CompileNotificationPtr.IsValid())
	{
		CompileNotificationPtr.Pin()->ExpireAndFadeout();
	}

	if ( GEditor )
	{
		GEditor->PlayEditorSound(CompileStartSound);
	}

	FNotificationInfo Info( NSLOCTEXT("MainFrame", "RecompileInProgress", "Compiling C++ Code") );
	Info.Image = FEditorStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode"));
	Info.ExpireDuration = 5.0f;
	Info.bFireAndForget = false;
	
	// We can only show the cancel button on async builds
	if (bIsAsyncCompile)
	{
		Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("CancelC++Compilation", "Cancel"), FText(), FSimpleDelegate::CreateRaw(this, &FMainFrameModule::OnCancelCodeCompilationClicked)));
	}

	CompileNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);

	if (CompileNotificationPtr.IsValid())
	{
		CompileNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FMainFrameModule::OnCancelCodeCompilationClicked()
{
	IHotReloadModule::Get().RequestStopCompilation();
}

void FMainFrameModule::HandleLevelEditorModuleCompileFinished(const FString& LogDump, ECompilationResult::Type CompilationResult, bool bShowLog)
{
	// Track stats
	{
		const float ModuleCompileDuration = (float)(FPlatformTime::Seconds() - ModuleCompileStartTime);
		UE_LOG(LogMainFrame, Log, TEXT("MainFrame: Module compiling took %.3f seconds"), ModuleCompileDuration);

		if( FEngineAnalytics::IsAvailable() )
		{
			TArray< FAnalyticsEventAttribute > CompileAttribs;
			CompileAttribs.Add(FAnalyticsEventAttribute(TEXT("Duration"), FString::Printf(TEXT("%.3f"), ModuleCompileDuration)));
			CompileAttribs.Add(FAnalyticsEventAttribute(TEXT("Result"), ECompilationResult::ToString(CompilationResult)));
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Modules.Recompile"), CompileAttribs);
		}
	}

	TSharedPtr<SNotificationItem> NotificationItem = CompileNotificationPtr.Pin();

	if (NotificationItem.IsValid())
	{
		if (!ECompilationResult::Failed(CompilationResult))
		{
			if ( GEditor )
			{
				GEditor->PlayEditorSound(CompileSuccessSound);
			}

			NotificationItem->SetText(NSLOCTEXT("MainFrame", "RecompileComplete", "Compile Complete!"));
			NotificationItem->SetExpireDuration( 5.0f );
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		}
		else
		{
			struct Local
			{
				static void ShowCompileLog()
				{
					FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
					MessageLogModule.OpenMessageLog(FCompilerResultsLog::GetLogName());
				}
			};

			if ( GEditor )
			{
				GEditor->PlayEditorSound(CompileFailSound);
			}

			if (CompilationResult == ECompilationResult::FailedDueToHeaderChange)
			{
				NotificationItem->SetText(NSLOCTEXT("MainFrame", "RecompileFailedDueToHeaderChange", "Compile failed due to the header changes. Close the editor and recompile project in IDE to apply changes."));
			}
			else if (CompilationResult == ECompilationResult::Canceled)
			{
				NotificationItem->SetText(NSLOCTEXT("MainFrame", "RecompileCanceled", "Compile Canceled!"));
			}
			else
			{
				NotificationItem->SetText(NSLOCTEXT("MainFrame", "RecompileFailed", "Compile Failed!"));
			}
			
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
			NotificationItem->SetHyperlink(FSimpleDelegate::CreateStatic(&Local::ShowCompileLog));
			NotificationItem->SetExpireDuration(30.0f);
		}

		NotificationItem->ExpireAndFadeout();

		CompileNotificationPtr.Reset();
	}
}


void FMainFrameModule::HandleHotReloadFinished( bool bWasTriggeredAutomatically )
{
	// Only play the notification for hot reloads that were triggered automatically.  If the user triggered the hot reload, they'll
	// have a different visual cue for that, such as the "Compiling Complete!" notification
	if( bWasTriggeredAutomatically )
	{
		FNotificationInfo Info( LOCTEXT("HotReloadFinished", "Hot Reload Complete!") );
		Info.Image = FEditorStyle::GetBrush(TEXT("LevelEditor.RecompileGameCode"));
		Info.FadeInDuration = 0.1f;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = 1.5f;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = true;
		Info.bUseLargeFont = true;
		Info.bFireAndForget = false;
		Info.bAllowThrottleWhenFrameRateIsLow = false;
		auto NotificationItem = FSlateNotificationManager::Get().AddNotification( Info );
		NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		NotificationItem->ExpireAndFadeout();
	
		GEditor->PlayEditorSound(CompileSuccessSound);
	}
}


void FMainFrameModule::HandleCodeAccessorLaunched( const bool WasSuccessful )
{
	TSharedPtr<SNotificationItem> NotificationItem = CodeAccessorNotificationPtr.Pin();
	
	if (NotificationItem.IsValid())
	{
		ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
		const FText AccessorNameText = SourceCodeAccessModule.GetAccessor().GetNameText();

		if (WasSuccessful)
		{
			NotificationItem->SetText( FText::Format(LOCTEXT("CodeAccessorLoadComplete", "{0} loaded!"), AccessorNameText) );
			NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		}
		else
		{
			NotificationItem->SetText( FText::Format(LOCTEXT("CodeAccessorLoadFailed", "{0} failed to launch!"), AccessorNameText) );
			NotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		}

		NotificationItem->ExpireAndFadeout();
		CodeAccessorNotificationPtr.Reset();
	}
}


void FMainFrameModule::HandleCodeAccessorLaunching()
{
	if (CodeAccessorNotificationPtr.IsValid())
	{
		CodeAccessorNotificationPtr.Pin()->ExpireAndFadeout();
	}

	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	const FText AccessorNameText = SourceCodeAccessModule.GetAccessor().GetNameText();

	FNotificationInfo Info( FText::Format(LOCTEXT("CodeAccessorLoadInProgress", "Loading {0}"), AccessorNameText) );
	Info.bFireAndForget = false;

	CodeAccessorNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	CodeAccessorNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
}

void FMainFrameModule::HandleCodeAccessorOpenFileFailed(const FString& Filename)
{
	auto* Info = new FNotificationInfo(FText::Format(LOCTEXT("FileNotFound", "Could not find code file, {0}"), FText::FromString(Filename)));
	Info->ExpireDuration = 3.0f;
	FSlateNotificationManager::Get().QueueNotification(Info);
}

#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FMainFrameModule, MainFrame);
