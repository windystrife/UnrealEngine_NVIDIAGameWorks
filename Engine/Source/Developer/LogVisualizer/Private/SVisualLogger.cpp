// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SVisualLogger.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "Async/Future.h"
#include "Async/Async.h"
#include "EngineGlobals.h"
#include "Debug/DebugDrawService.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "VisualLogger/VisualLogger.h"
#include "LogVisualizerSettings.h"
#include "LogVisualizerSessionSettings.h"
#include "VisualLoggerDatabase.h"
#include "LogVisualizerStyle.h"
#include "VisualLoggerCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "LogVisualizerPrivate.h"
#include "SVisualLoggerToolbar.h"
#include "SVisualLoggerFilters.h"
#include "SVisualLoggerView.h"
#include "SVisualLoggerLogsList.h"
#include "SVisualLoggerStatusView.h"
#include "VisualLoggerTimeSliderController.h"
#include "VisualLoggerRenderingActor.h"
#include "VisualLoggerCanvasRenderer.h"
#include "DesktopPlatformModule.h"
#include "VisualLoggerCameraController.h"
#include "Framework/Application/SlateApplication.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Editor.h"
#endif
#include "ISettingsModule.h"

#include "VisualLogger/VisualLoggerBinaryFileDevice.h"

#define LOCTEXT_NAMESPACE "SVisualLogger"

DEFINE_LOG_CATEGORY_STATIC(LogVisualLogger, Log, All);

/* Local constants
*****************************************************************************/
static const FName ToolbarTabId("Toolbar");
static const FName FiltersTabId("Filters");
static const FName MainViewTabId("MainView");
static const FName LogsListTabId("LogsList");
static const FName StatusViewTabId("StatusView");

namespace LogVisualizer
{
	static const FString LogFileDescription = LOCTEXT("FileTypeDescription", "Visual Log File").ToString();
	static const FString LoadFileTypes = FString::Printf(TEXT("%s (*.bvlog;*.%s)|*.bvlog;*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
	static const FString SaveFileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *LogFileDescription, VISLOG_FILENAME_EXT, VISLOG_FILENAME_EXT);
}

DECLARE_DELEGATE_TwoParams(FOnWorldChanged, UWorld*, UWorld*);

/* SMessagingDebugger constructors
*****************************************************************************/
namespace
{
	static UWorld* GetWorldForGivenObject(const UObject* Object)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Object, EGetWorldErrorMode::ReturnNull);
#if WITH_EDITOR
		UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
		if (GIsEditor && EEngine != nullptr && World == nullptr)
		{
			// lets use PlayWorld during PIE/Simulate and regular world from editor otherwise, to draw debug information
			World = EEngine->PlayWorld != nullptr ? EEngine->PlayWorld : EEngine->GetEditorWorldContext().World();
		}

#endif
		if (!GIsEditor && World == nullptr)
		{
			World = GEngine->GetWorld();
		}

		return World;
	}
}

SVisualLogger::SVisualLogger() 
	: SCompoundWidget(), CommandList(MakeShareable(new FUICommandList))
{ 
	bPausedLogger = false;
	bGotHistogramData = false;

	class FVisualLoggerDevice : public FVisualLogDevice
	{
	public:
		FVisualLoggerDevice(SVisualLogger* InVisualLogger, FOnWorldChanged OnWorldChangedDelegate) : VisualLoggerWidget(InVisualLogger), LastUsedWorld(nullptr), OnWorldChanged(OnWorldChangedDelegate) {}

		virtual ~FVisualLoggerDevice(){}
		virtual void Serialize(const UObject* LogOwner, FName OwnerName, FName OwnerClassName, const FVisualLogEntry& LogEntry) override
		{
			VisualLoggerWidget->OnNewLogEntry(FVisualLogDevice::FVisualLogEntryItem(OwnerName, OwnerClassName, LogEntry));
			UWorld* CurrentWorld = nullptr;
			if (FVisualLogger::Get().GetObjectToWorldMap().Contains(LogOwner))
			{
				CurrentWorld = const_cast<UWorld*>(FVisualLogger::Get().GetObjectToWorldMap()[LogOwner].Get());
			}
			if (LastUsedWorld != CurrentWorld && CurrentWorld != nullptr)
			{
				OnWorldChanged.ExecuteIfBound(LastUsedWorld, CurrentWorld);
				LastUsedWorld = CurrentWorld;
			}
		}
		
		SVisualLogger* VisualLoggerWidget;
		UWorld* LastUsedWorld;
		FOnWorldChanged OnWorldChanged;
	};

	InternalDevice = MakeShareable(new FVisualLoggerDevice(this, FOnWorldChanged::CreateLambda([this](UWorld* PreviousWorld, UWorld* CurrentWorld){ OnNewWorld(CurrentWorld); })));
	FVisualLogger::Get().AddDevice(InternalDevice.Get());
}

SVisualLogger::~SVisualLogger()
{
	GEngine->OnWorldAdded().RemoveAll(this);

	FVisualLogger::Get().RemoveDevice(InternalDevice.Get());
	InternalDevice.Reset();

	UWorld* World = NULL;
#if WITH_EDITOR
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->SavePresistentData();
#endif

	if (LastUsedWorld.IsValid())
	{
		for (TActorIterator<AVisualLoggerRenderingActor> It(LastUsedWorld.Get()); It; ++It)
		{
			LastUsedWorld->DestroyActor(*It);
		}
	}


	UDebugDrawService::Unregister(DrawOnCanvasDelegateHandle);
	VisualLoggerCanvasRenderer.Reset();

	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.RemoveAll(this);
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.RemoveAll(this);
	FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.RemoveAll(this);

	FLogVisualizer::Get().GetEvents().OnFiltersChanged.RemoveAll(this);
	FLogVisualizer::Get().GetEvents().OnLogLineSelectionChanged.Unbind();
	FLogVisualizer::Get().GetEvents().OnKeyboardEvent.Unbind();

	FVisualLoggerDatabase::Get().Reset();
}

/* SMessagingDebugger interface
*****************************************************************************/

void SVisualLogger::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	bPausedLogger = false;
	bGotHistogramData = false;

	FLogVisualizer::Get().SetCurrentVisualizer(SharedThis(this));
	//////////////////////////////////////////////////////////////////////////
	// Visual Logger Events
	FLogVisualizer::Get().GetEvents().OnFiltersChanged.AddRaw(this, &SVisualLogger::OnFiltersChanged);
	FLogVisualizer::Get().GetEvents().OnLogLineSelectionChanged = FOnLogLineSelectionChanged::CreateRaw(this, &SVisualLogger::OnLogLineSelectionChanged);
	FLogVisualizer::Get().GetEvents().OnKeyboardEvent = FOnKeyboardEvent::CreateRaw(this, &SVisualLogger::OnKeyboaedRedirection);
	FLogVisualizer::Get().GetTimeSliderController().Get()->GetTimeSliderArgs().OnScrubPositionChanged = FOnScrubPositionChanged::CreateRaw(this, &SVisualLogger::OnScrubPositionChanged);

	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.AddRaw(this, &SVisualLogger::OnObjectSelectionChanged);
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.AddRaw(this, &SVisualLogger::OnNewItemHandler);
	FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.AddRaw(this, &SVisualLogger::OnItemsSelectionChanged);

	GEngine->OnWorldAdded().AddRaw(this, &SVisualLogger::OnNewWorld);

	//////////////////////////////////////////////////////////////////////////
	// Command Action Lists
	const FVisualLoggerCommands& Commands = FVisualLoggerCommands::Get();
	FUICommandList& ActionList = *CommandList;

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	Settings->LoadPresistentData();

	ActionList.MapAction(Commands.StartRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStartRecordingCommandIsVisible));
	ActionList.MapAction(Commands.StopRecording, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleStopRecordingCommandIsVisible));
	ActionList.MapAction(Commands.Pause, FExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandlePauseCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandlePauseCommandIsVisible));
	ActionList.MapAction(Commands.Resume, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleResumeCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleResumeCommandIsVisible));
	ActionList.MapAction(Commands.LoadFromVLog, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleLoadCommandCanExecute));
	ActionList.MapAction(Commands.SaveToVLog, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));
	ActionList.MapAction(Commands.SaveAllToVLog, FExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveAllCommandExecute), FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute), FIsActionChecked(), FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));
	ActionList.MapAction(Commands.FreeCamera,
		FExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandExecute), 
		FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute), 
		FIsActionChecked::CreateRaw(this, &SVisualLogger::HandleCameraCommandIsChecked),
		FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleCameraCommandCanExecute));
	ActionList.MapAction(Commands.ToggleGraphs,
		FExecuteAction::CreateLambda([](){bool& bEnableGraphsVisualization = ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; bEnableGraphsVisualization = !bEnableGraphsVisualization; }),
		FCanExecuteAction::CreateLambda([this]()->bool{return FVisualLoggerGraphsDatabase::Get().ContainsHistogramGraphs(); }),
		FIsActionChecked::CreateLambda([]()->bool{return ULogVisualizerSessionSettings::StaticClass()->GetDefaultObject<ULogVisualizerSessionSettings>()->bEnableGraphsVisualization; }),
		FIsActionButtonVisible::CreateLambda([this]()->bool{return FVisualLoggerGraphsDatabase::Get().ContainsHistogramGraphs(); }));
	ActionList.MapAction(Commands.ResetData, 
		FExecuteAction::CreateRaw(this, &SVisualLogger::ResetData),
		FCanExecuteAction::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute),
		FIsActionChecked(), 
		FIsActionButtonVisible::CreateRaw(this, &SVisualLogger::HandleSaveCommandCanExecute));


	// Tab Spawners
	TabManager = FGlobalTabmanager::Get()->NewTabManager(ConstructUnderMajorTab);
	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("VisualLoggerGroupName", "Visual Logger"));

	TabManager->RegisterTabSpawner(ToolbarTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, ToolbarTabId))
		.SetDisplayName(LOCTEXT("ToolbarTabTitle", "Toolbar"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "ToolbarTabIcon"));

	TabManager->RegisterTabSpawner(FiltersTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, FiltersTabId))
		.SetDisplayName(LOCTEXT("FiltersTabTitle", "Filters"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "FiltersTabIcon"));

	TabManager->RegisterTabSpawner(MainViewTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, MainViewTabId))
		.SetDisplayName(LOCTEXT("MainViewTabTitle", "MainView"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "MainViewTabIcon"));

	TabManager->RegisterTabSpawner(LogsListTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, LogsListTabId))
		.SetDisplayName(LOCTEXT("LogsListTabTitle", "LogsList"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "LogsListTabIcon"));

	TabManager->RegisterTabSpawner(StatusViewTabId, FOnSpawnTab::CreateRaw(this, &SVisualLogger::HandleTabManagerSpawnTab, StatusViewTabId))
		.SetDisplayName(LOCTEXT("StatusViewTabTitle", "StatusView"))
		.SetGroup(AppMenuGroup)
		.SetIcon(FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), "StatusViewTabIcon"));

	// Default Layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("VisualLoggerLayout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(ToolbarTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(FiltersTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(MainViewTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.6f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(StatusViewTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
					->SetSizeCoefficient(0.3f)
					)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(LogsListTabId, ETabState::OpenedTab)
					->SetHideTabWell(true)
					->SetSizeCoefficient(0.7f)
					)
			)
		);
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &SVisualLogger::HandleTabManagerPersistLayout));

	// Window Menu
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList>());
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateStatic(&SVisualLogger::FillWindowMenu, TabManager),
		"Window"
		);

	MenuBarBuilder.AddMenuEntry(
		LOCTEXT("SettingsMenuLabel", "Settings"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(
			[this](){
				ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
				if (SettingsModule != nullptr)
				{
					SettingsModule->ShowViewer("Editor", "General", "VisualLogger");
				}
			}
		)),
		"Settings"
		);


	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MenuBarBuilder.MakeWidget()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				TabManager->RestoreFrom(Layout, ConstructUnderWindow).ToSharedRef()
			]
		];

	VisualLoggerCanvasRenderer = MakeShareable(new FVisualLoggerCanvasRenderer());

	DrawOnCanvasDelegateHandle = UDebugDrawService::Register(TEXT("VisLog"), FDebugDrawDelegate::CreateRaw(VisualLoggerCanvasRenderer.Get(), &FVisualLoggerCanvasRenderer::DrawOnCanvas));

	Cast<AVisualLoggerRenderingActor>(FVisualLoggerEditorInterface::Get()->GetHelperActor(LastUsedWorld.Get()));
}

void SVisualLogger::OnNewLogEntry(const FVisualLogDevice::FVisualLogEntryItem& Entry)
{
	if (bPausedLogger)
	{
		OnPauseCacheForEntries.Add(Entry);
		return;
	}

	FVisualLoggerDatabase::Get().AddItem(Entry);

	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bStickToRecentData)
	{
		FLogVisualizer::Get().GetTimeSliderController()->CommitScrubPosition(Entry.Entry.TimeStamp, false);
	}
}

void SVisualLogger::HandleMajorTabPersistVisualState()
{
	// save any settings here
}

void SVisualLogger::HandleTabManagerPersistLayout(const TSharedRef<FTabManager::FLayout>& LayoutToSave)
{
	// save any layout here
}

void SVisualLogger::FillWindowMenu(FMenuBuilder& MenuBuilder, const TSharedPtr<FTabManager> TabManager)
{
	if (!TabManager.IsValid())
	{
		return;
	}

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

TSharedRef<SDockTab> SVisualLogger::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier) const
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;
	bool AutoSizeTab = false;

	if (TabIdentifier == ToolbarTabId)
	{ 
		TabWidget = SNew(SVisualLoggerToolbar, CommandList);
		AutoSizeTab = true;
	}
	else if (TabIdentifier == FiltersTabId)
	{
		TabWidget = SAssignNew(VisualLoggerFilters, SVisualLoggerFilters, CommandList);
		AutoSizeTab = true;
	}
	else if (TabIdentifier == MainViewTabId)
	{
		TabWidget = SAssignNew(MainView, SVisualLoggerView, CommandList).OnFiltersSearchChanged(this, &SVisualLogger::OnFiltersSearchChanged);
		AutoSizeTab = false;
	}
	else if (TabIdentifier == LogsListTabId)
	{
		TabWidget = SAssignNew(LogsList, SVisualLoggerLogsList, CommandList);
		AutoSizeTab = false;
	}
	else if (TabIdentifier == StatusViewTabId)
	{
		TabWidget = SAssignNew(StatusView, SVisualLoggerStatusView, CommandList);
		AutoSizeTab = false;
	}

	check(TabWidget.IsValid());
	return SNew(SVisualLoggerTab)
		.ShouldAutosize(AutoSizeTab)
		.TabRole(ETabRole::DocumentTab)
		[
			TabWidget.ToSharedRef()
		];
}

bool SVisualLogger::HandleStartRecordingCommandCanExecute() const
{
	return !FVisualLogger::Get().IsRecording();
}


void SVisualLogger::HandleStartRecordingCommandExecute()
{
	FVisualLogger::Get().SetIsRecording(true);
}


bool SVisualLogger::HandleStartRecordingCommandIsVisible() const
{
	return !FVisualLogger::Get().IsRecording();
}

bool SVisualLogger::HandleStopRecordingCommandCanExecute() const
{
	return FVisualLogger::Get().IsRecording();
}


void SVisualLogger::HandleStopRecordingCommandExecute()
{
	UWorld* World = FLogVisualizer::Get().GetWorld();

	if (FParse::Param(FCommandLine::Get(), TEXT("LogNavOctree")) == true && ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bLogNavOctreeOnStop)
	{
		FVisualLogger::NavigationDataDump(World, LogNavigation, ELogVerbosity::Log, FBox());
	}

	FVisualLogger::Get().SetIsRecording(false);

	if (AVisualLoggerCameraController::IsEnabled(World))
	{
		AVisualLoggerCameraController::DisableCamera(World);
	}

	if (bPausedLogger)
	{
		HandleResumeCommandExecute();
	}
}


bool SVisualLogger::HandleStopRecordingCommandIsVisible() const
{
	return FVisualLogger::Get().IsRecording();
}

bool SVisualLogger::HandlePauseCommandCanExecute() const
{
	return !bPausedLogger;
}

void SVisualLogger::HandlePauseCommandExecute()
{
	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bUsePlayersOnlyForPause)
	{
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != nullptr)
			{
				Context.World()->bPlayersOnlyPending = true;
			}
		}
	}

	bPausedLogger = true;
}

bool SVisualLogger::HandlePauseCommandIsVisible() const
{
	return HandlePauseCommandCanExecute();
}

bool SVisualLogger::HandleResumeCommandCanExecute() const
{
	return bPausedLogger;
}

void SVisualLogger::HandleResumeCommandExecute()
{
	if (ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bUsePlayersOnlyForPause)
	{
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if (Context.World() != nullptr)
			{
				Context.World()->bPlayersOnly = false;
				Context.World()->bPlayersOnlyPending = false;
			}
		}
	}

	bPausedLogger = false;
	for (const auto& CurrentEntry : OnPauseCacheForEntries)
	{
		OnNewLogEntry(CurrentEntry);
	}
	OnPauseCacheForEntries.Reset();

}

bool SVisualLogger::HandleResumeCommandIsVisible() const
{
	return HandleResumeCommandCanExecute();
}

bool SVisualLogger::HandleCameraCommandIsChecked() const
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	return World && AVisualLoggerCameraController::IsEnabled(World);
}

bool SVisualLogger::HandleCameraCommandCanExecute() const
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	return FVisualLogger::Get().IsRecording() && World && (World->bPlayersOnly || World->bPlayersOnlyPending) && World->IsPlayInEditor() && (GEditor && !GEditor->bIsSimulatingInEditor);
}

void SVisualLogger::HandleCameraCommandExecute()
{
	UWorld* World = FLogVisualizer::Get().GetWorld();
	if (AVisualLoggerCameraController::IsEnabled(World))
	{
		AVisualLoggerCameraController::DisableCamera(World);
	}
	else
	{
		// switch debug cam on
		CameraController = AVisualLoggerCameraController::EnableCamera(World);
	}
}

bool SVisualLogger::HandleLoadCommandCanExecute() const
{
	return true;
}

void SVisualLogger::HandleLoadCommandExecute()
{
	FArchive Ar;
	TArray<FVisualLogDevice::FVisualLogEntryItem> RecordedLogs;

	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform)
	{
		const FString DefaultBrowsePath = FString::Printf(TEXT("%slogs/"), *FPaths::ProjectSavedDir());

		bOpened = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			LOCTEXT("OpenProjectBrowseTitle", "Open Project").ToString(),
			DefaultBrowsePath,
			TEXT(""),
			LogVisualizer::LoadFileTypes,
			EFileDialogFlags::None,
			OpenFilenames
			);
	}

	if (bOpened && OpenFilenames.Num() > 0)
	{
		OnNewWorld(GetWorldForGivenObject(nullptr));
		for (int FilenameIndex = 0; FilenameIndex < OpenFilenames.Num(); ++FilenameIndex)
		{
			FString CurrentFileName = OpenFilenames[FilenameIndex];
			const bool bIsBinaryFile = CurrentFileName.Find(TEXT(".bvlog")) != INDEX_NONE;
			if (bIsBinaryFile)
			{
				FArchive* FileAr = IFileManager::Get().CreateFileReader(*CurrentFileName);
				FVisualLoggerHelpers::Serialize(*FileAr, RecordedLogs);
				FileAr->Close();
				delete FileAr;
				FileAr = NULL;

				for (FVisualLogDevice::FVisualLogEntryItem& CurrentItem : RecordedLogs)
				{
					OnNewLogEntry(CurrentItem);
				}
			}
		}
	}
}

bool SVisualLogger::HandleSaveCommandCanExecute() const
{ 
	return FVisualLoggerDatabase::Get().NumberOfRows() > 0;
}

void SVisualLogger::HandleSaveAllCommandExecute()
{
	HandleSaveCommand(true);
}

void SVisualLogger::HandleSaveCommandExecute()
{
	HandleSaveCommand(false);
}

void SVisualLogger::HandleSaveCommand(bool bSaveAllData)
{
	TArray<FName> SelectedRows;
	if (!bSaveAllData)
	{
		SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
	}
	else
	{
		for (auto Iter(FVisualLoggerDatabase::Get().GetConstRowIterator()); Iter; ++Iter)
		{
			SelectedRows.Add((*Iter).GetOwnerName());
		}
	}

	if (SelectedRows.Num())
	{
		// Prompt the user for the filenames
		TArray<FString> SaveFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bSaved = false;
		if (DesktopPlatform)
		{
			const FString DefaultBrowsePath = FString::Printf(TEXT("%slogs/"), *FPaths::ProjectSavedDir());
			bSaved = DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
				LOCTEXT("NewProjectBrowseTitle", "Choose a project location").ToString(),
				DefaultBrowsePath,
				TEXT(""),
				LogVisualizer::SaveFileTypes,
				EFileDialogFlags::None,
				SaveFilenames
				);
		}

		if (bSaved)
		{
			if (SaveFilenames.Num() > 0 && SaveFilenames[0].IsEmpty() == false)
			{
				TArray<FVisualLogDevice::FVisualLogEntryItem> FrameCache;
				for (const FName& CurrentName : SelectedRows)
				{
					FVisualLoggerDBRow& DataRow = FVisualLoggerDatabase::Get().GetRowByName(CurrentName);
					FrameCache.Append(DataRow.GetItems());
				}

				if (FrameCache.Num())
				{
					FArchive* FileArchive = IFileManager::Get().CreateFileWriter(*SaveFilenames[0]);
					if (ensure(FileArchive))
					{
						FVisualLoggerHelpers::Serialize(*FileArchive, FrameCache);
						FileArchive->Close();
						delete FileArchive;
						FileArchive = NULL;
					}
					else
					{
						UE_LOG(LogVisualLogger, Error, TEXT("Failed to create file \"%s\""), *SaveFilenames[0]);
					}
				}
			}
		}
	}
}

void SVisualLogger::ResetData()
{
	bGotHistogramData = false;
	OnPauseCacheForEntries.Reset();

	FLogVisualizer::Get().Reset();
	FVisualLoggerDatabase::Get().Reset();

	FVisualLoggerFilters::Get().Reset();

	if (MainView.IsValid())
	{
		MainView->ResetData();
	}

	if (VisualLoggerFilters.IsValid() && ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bResetDataWithNewSession)
	{
		VisualLoggerFilters->ResetData();
	}

	if (LogsList.IsValid())
	{
		LogsList->ResetData();
	}

	if (StatusView.IsValid())
	{
		StatusView->ResetData();
	}

	if (VisualLoggerCanvasRenderer.IsValid())
	{
		VisualLoggerCanvasRenderer->ResetData();
	}

	if (AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FVisualLoggerEditorInterface::Get()->GetHelperActor(LastUsedWorld.Get())))
	{
		HelperActor->ResetRendering();
	}

	const TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto Iterator = AllExtensions.CreateConstIterator(); Iterator; ++Iterator)
	{
		FVisualLogExtensionInterface* Extension = (*Iterator).Value;
		if (Extension != NULL)
		{
			Extension->ResetData(FVisualLoggerEditorInterface::Get());
		}
	}

	FLogVisualizer::Get().GetEvents().OnLogLineSelectionChanged = FOnLogLineSelectionChanged::CreateRaw(this, &SVisualLogger::OnLogLineSelectionChanged);
	FLogVisualizer::Get().GetEvents().OnKeyboardEvent = FOnKeyboardEvent::CreateRaw(this, &SVisualLogger::OnKeyboaedRedirection);
	FLogVisualizer::Get().GetTimeSliderController().Get()->GetTimeSliderArgs().OnScrubPositionChanged = FOnScrubPositionChanged::CreateRaw(this, &SVisualLogger::OnScrubPositionChanged);
}

void SVisualLogger::OnNewWorld(UWorld* NewWorld)
{
	if (LastUsedWorld.IsValid() && LastUsedWorld != NewWorld)
	{
		for (TActorIterator<AVisualLoggerRenderingActor> It(LastUsedWorld.Get()); It; ++It)
		{
			LastUsedWorld->DestroyActor(*It);
		}
	}
	LastUsedWorld = NewWorld;

	AVisualLoggerRenderingActor* HelperActor = Cast<AVisualLoggerRenderingActor>(FVisualLoggerEditorInterface::Get()->GetHelperActor(LastUsedWorld.Get()));
	if (ensure(HelperActor))
	{
		if (LastUsedWorld.IsValid() == false || ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bResetDataWithNewSession)
		{
			ResetData();
		}

		// reset data and simulate row/item selection to recreate rendering proxy with correct data
		HelperActor->ResetRendering();
		const TArray<FName>& SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
		HelperActor->ObjectSelectionChanged(SelectedRows);
		for (auto& RowName : SelectedRows)
		{
			FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
			HelperActor->OnItemSelectionChanged(DBRow, DBRow.GetCurrentItemIndex());
		}
	}
}

void SVisualLogger::OnObjectSelectionChanged(const TArray<FName>& RowNames)
{
	const float ScrubTime = FLogVisualizer::Get().GetTimeSliderController().Get()->GetTimeSliderArgs().ScrubPosition.Get();
	for (auto RowName : RowNames)
	{
		FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
		if (DBRow.GetCurrentItemIndex() == INDEX_NONE)
		{
			DBRow.MoveTo(DBRow.GetClosestItem(ScrubTime, ScrubTime));
		}
	}
}

void SVisualLogger::OnItemsSelectionChanged(const FVisualLoggerDBRow& ChangedRow, int32 SelectedItemIndex)
{
	const TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto Iterator = AllExtensions.CreateConstIterator(); Iterator; ++Iterator)
	{
		FVisualLogExtensionInterface* Extension = (*Iterator).Value;
		if (Extension != NULL)
		{
			Extension->OnItemsSelectionChanged(FVisualLoggerEditorInterface::Get());
		}
	}
}

void SVisualLogger::OnFiltersChanged()
{
	const uint32 StartCycles = FPlatformTime::Cycles();

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const FString QuickSearchStrng = FVisualLoggerFilters::Get().GetSearchString();

	TArray<TFuture<void> > AllFutures;
	for (auto Iterator = FVisualLoggerDatabase::Get().GetRowIterator(); Iterator; ++Iterator)
	{
		FVisualLoggerDBRow* DBRow = &(*Iterator);
		AllFutures.Add(
			Async<void>(EAsyncExecution::TaskGraph, [this, DBRow]()
			{
				const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = DBRow->GetItems();
				for (int32 Index = 0; Index < Entries.Num(); ++Index)
				{
					UpdateVisibilityForEntry(*DBRow, Index);
				}
			}
		));
	}

	bool bAllFuturesReady = false;
	do
	{
		bAllFuturesReady = true;
		for (TFuture<void>& CurrentFuture : AllFutures)
		{
			bAllFuturesReady &= CurrentFuture.IsReady();
			if (bAllFuturesReady == false)
			{
				break;
			}
		}
		if (bAllFuturesReady == false)
		{
			FPlatformProcess::Sleep(0.01);
		}
	} while (bAllFuturesReady != true);

	for (auto Iterator = FVisualLoggerDatabase::Get().GetRowIterator(); Iterator; ++Iterator)
	{
		FVisualLoggerDBRow& DBRow = *Iterator;
		FVisualLoggerDatabase::Get().SetRowVisibility(DBRow.GetOwnerName(), DBRow.GetNumberOfHiddenItems() != DBRow.GetItems().Num());
	}


	const uint32 EndCycles = FPlatformTime::Cycles();
	const int32 BlockingCycles = int32(FPlatformTime::Cycles() - StartCycles);
	{
		const TArray<FName>& SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
		const float ScrubTime = FLogVisualizer::Get().GetTimeSliderController()->GetTimeSliderArgs().ScrubPosition.Get();
		{
			for (auto RowName : SelectedRows)
			{
				auto& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
				const int32 ClosestItem = DBRow.GetClosestItem(ScrubTime, ScrubTime);
				const TArray<FVisualLogDevice::FVisualLogEntryItem>& Items = DBRow.GetItems();
				if (Items.IsValidIndex(ClosestItem) && Items[ClosestItem].Entry.TimeStamp <= ScrubTime)
				{
					DBRow.MoveTo(ClosestItem);
				}
			}
		}
	}
	UE_LOG(LogVisualLogger, Display, TEXT("SVisualLogger::OnFiltersChanged: %5.2fms"), FPlatformTime::ToMilliseconds(BlockingCycles));
}

void SVisualLogger::OnFiltersSearchChanged(const FText& Filter)
{
	const uint32 StartCycles = FPlatformTime::Cycles();

	FVisualLoggerFilters::Get().SetSearchString(Filter.ToString());

	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const FString QuickSearchStrng = FVisualLoggerFilters::Get().GetSearchString();

	TArray<TFuture<void> > AllFutures;
	for (auto Iterator = FVisualLoggerDatabase::Get().GetRowIterator(); Iterator; ++Iterator)
	{
		FVisualLoggerDBRow* DBRow = &(*Iterator);
			AllFutures.Add(
				Async<void>(EAsyncExecution::TaskGraph, [this, DBRow]()
					{
						const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = DBRow->GetItems();
						for (int32 Index = 0; Index < Entries.Num(); ++Index)
						{
							UpdateVisibilityForEntry(*DBRow, Index);
						}
					}
				)
		);
	}

	bool bAllFuturesReady = false;
	do 
	{
		bAllFuturesReady = true;
		for (TFuture<void>& CurrentFuture : AllFutures)
		{
			bAllFuturesReady &= CurrentFuture.IsReady();
			if (bAllFuturesReady == false)
			{
				break;
			}
		}
		if (bAllFuturesReady == false)
		{
			FPlatformProcess::Sleep(0.01);
		}
	} while (bAllFuturesReady != true);

	for (auto Iterator = FVisualLoggerDatabase::Get().GetRowIterator(); Iterator; ++Iterator)
	{
		FVisualLoggerDBRow& DBRow = *Iterator;
		FVisualLoggerDatabase::Get().SetRowVisibility(DBRow.GetOwnerName(), DBRow.GetNumberOfHiddenItems() != DBRow.GetItems().Num());
	}

	if (LogsList.IsValid())
	{
		// it depends on rows visibility so it have to be called here, manually after changes to rows visibilities
		LogsList->OnFiltersSearchChanged(Filter);
	}

	if (VisualLoggerCanvasRenderer.IsValid())
	{
		VisualLoggerCanvasRenderer->DirtyCachedData();
	}

	const uint32 EndCycles = FPlatformTime::Cycles();
	const int32 BlockingCycles = int32(FPlatformTime::Cycles() - StartCycles);
	UE_LOG(LogVisualLogger, Display, TEXT("SVisualLogger::OnFiltersSearchChanged: %5.2fms"), FPlatformTime::ToMilliseconds(BlockingCycles));
}

void SVisualLogger::OnNewItemHandler(const FVisualLoggerDBRow& DBRow, int32 ItemIndex)
{
	UpdateVisibilityForEntry(DBRow, ItemIndex);
}

void SVisualLogger::UpdateVisibilityForEntry(const FVisualLoggerDBRow& DBRow, int32 ItemIndex)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	const FVisualLogDevice::FVisualLogEntryItem& CurrentEntry = DBRow.GetItems()[ItemIndex];
	FString SearchString = FVisualLoggerFilters::Get().GetSearchString();

	TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
	FVisualLoggerHelpers::GetCategories(CurrentEntry.Entry, OutCategories);
	bool bHasValidCategories = false;
	for (FVisualLoggerCategoryVerbosityPair& CategoryPair : OutCategories)
	{
		if (FVisualLoggerFilters::Get().MatchCategoryFilters(CategoryPair.CategoryName.ToString(), CategoryPair.Verbosity))
		{
			bHasValidCategories = true;
			break;
		}
	}

	if (Settings->bSearchInsideLogs && bHasValidCategories && SearchString.Len() > 0)
	{
		bool bMatchSearchString = false;
		for (const FVisualLogLine& CurrentLine : CurrentEntry.Entry.LogLines)
		{
			if (CurrentLine.Line.Find(SearchString) != INDEX_NONE || CurrentLine.Category.ToString().Find(SearchString) != INDEX_NONE)
			{
				bMatchSearchString = true;
				break;
			}
		}
		if (!bMatchSearchString)
		{
			for (const FVisualLogEvent& CurrentEvent : CurrentEntry.Entry.Events)
			{
				if (CurrentEvent.Name.Find(SearchString) != INDEX_NONE)
				{
					bMatchSearchString = true;
					break;
				}
			}
		}

		FVisualLoggerDatabase::Get().GetRowByName(DBRow.GetOwnerName()).SetItemVisibility(ItemIndex, bMatchSearchString);
	}
	else
	{
		FVisualLoggerDatabase::Get().GetRowByName(DBRow.GetOwnerName()).SetItemVisibility(ItemIndex, bHasValidCategories);
	}
}

void SVisualLogger::OnLogLineSelectionChanged(TSharedPtr<struct FLogEntryItem> SelectedItem, int64 UserData, FName TagName)
{
	const TMap<FName, FVisualLogExtensionInterface*>& AllExtensions = FVisualLogger::Get().GetAllExtensions();
	for (auto Iterator = AllExtensions.CreateConstIterator(); Iterator; ++Iterator)
	{
		FVisualLogExtensionInterface* Extension = (*Iterator).Value;
		if (Extension != NULL)
		{
			Extension->OnLogLineSelectionChanged(FVisualLoggerEditorInterface::Get(), SelectedItem, UserData);
		}
	}
}

void SVisualLogger::OnScrubPositionChanged(float NewScrubPosition, bool bScrubbing)
{
	const TArray<FName> &SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
	const float ScrubTime = FLogVisualizer::Get().GetTimeSliderController()->GetTimeSliderArgs().ScrubPosition.Get();
	for (auto RowName : SelectedRows)
	{
		auto& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
		const int32 ClosestItem = SelectedRows.Num() > 1 ? DBRow.GetClosestItem(NewScrubPosition, ScrubTime) : DBRow.GetClosestItem(NewScrubPosition);
		const TArray<FVisualLogDevice::FVisualLogEntryItem>& Items = DBRow.GetItems();
		if (Items.IsValidIndex(ClosestItem) && Items[ClosestItem].Entry.TimeStamp <= NewScrubPosition)
		{
			DBRow.MoveTo(ClosestItem);
		}
	}
}

FReply SVisualLogger::OnKeyboaedRedirection(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply ReturnValue = FReply::Unhandled();

	const TArray<FName>& SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
	if (SelectedRows.Num() == 0)
	{
		return ReturnValue;
	}

	// find time to move by
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Right)
	{
		const float ScrubTime = FLogVisualizer::Get().GetTimeSliderController()->GetTimeSliderArgs().ScrubPosition.Get();
		float NewTimeToSet = ScrubTime;
		float BestTimeDifference = FLT_MAX;

		const int32 MoveDist = InKeyEvent.IsLeftControlDown() ? InKeyEvent.IsLeftShiftDown() ? 20 : 10 : 1;
		for (auto RowName : SelectedRows)
		{
			const FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(RowName);
			const int32 CurrentItemIndex = DBRow.GetCurrentItemIndex();
			if (CurrentItemIndex == INDEX_NONE)
			{
				continue;
			}

			if (Key == EKeys::Right)
			{
				float TimeDifference = DBRow.GetCurrentItem().Entry.TimeStamp - ScrubTime;
				if (TimeDifference > 0 && FMath::Abs(TimeDifference) < FMath::Abs(BestTimeDifference))
				{
					BestTimeDifference = TimeDifference;
					NewTimeToSet = DBRow.GetCurrentItem().Entry.TimeStamp;
				}

				const int32 NextItemIndex = FLogVisualizer::Get().GetNextItem(RowName, MoveDist);
				TimeDifference = DBRow.GetItems()[NextItemIndex].Entry.TimeStamp - ScrubTime;
				if (TimeDifference > 0 && FMath::Abs(TimeDifference) < FMath::Abs(BestTimeDifference))
				{
					BestTimeDifference = TimeDifference;
					NewTimeToSet = DBRow.GetItems()[NextItemIndex].Entry.TimeStamp;
				}
			}
			else if (Key == EKeys::Left)
			{
				float TimeDifference = DBRow.GetCurrentItem().Entry.TimeStamp - ScrubTime;
				if (TimeDifference < 0 && FMath::Abs(TimeDifference) < FMath::Abs(BestTimeDifference))
				{
					BestTimeDifference = TimeDifference;
					NewTimeToSet = DBRow.GetCurrentItem().Entry.TimeStamp;
				}

				const int32 PrevItemIndex = FLogVisualizer::Get().GetPreviousItem(RowName, MoveDist);
				TimeDifference = DBRow.GetItems()[PrevItemIndex].Entry.TimeStamp - ScrubTime;
				if (TimeDifference < 0 && FMath::Abs(TimeDifference) > 0 && FMath::Abs(TimeDifference) < FMath::Abs(BestTimeDifference))
				{
					BestTimeDifference = TimeDifference;
					NewTimeToSet = DBRow.GetItems()[PrevItemIndex].Entry.TimeStamp;
				}
			}
		}

		FLogVisualizer::Get().GetTimeSliderController()->CommitScrubPosition(NewTimeToSet, false);
		ReturnValue = FReply::Handled();
	}

	FName OwnerName = SelectedRows[SelectedRows.Num() - 1];
	const FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(OwnerName);
	if (DBRow.GetCurrentItemIndex() != INDEX_NONE)
	{
		if (Key == EKeys::Home)
		{
			FLogVisualizer::Get().GotoFirstItem(OwnerName);
			ReturnValue = FReply::Handled();
		}
		else if (Key == EKeys::End)
		{
			FLogVisualizer::Get().GotoLastItem(OwnerName);
			ReturnValue = FReply::Handled();
		}
		else if (Key == EKeys::Enter)
		{
			FLogVisualizer::Get().UpdateCameraPosition(OwnerName, DBRow.GetCurrentItemIndex());
			ReturnValue = FReply::Handled();
		}
	}

	return ReturnValue;
}

#undef LOCTEXT_NAMESPACE
