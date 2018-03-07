// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotBrowser.cpp: Implements the SScreenShotBrowser class.
=============================================================================*/

#include "Widgets/SScreenShotBrowser.h"
#include "HAL/FileManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "Widgets/SScreenComparisonRow.h"
#include "Models/ScreenComparisonModel.h"
#include "Misc/FeedbackContext.h"
#include "EditorStyleSet.h"
#include "Modules/ModuleManager.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

#define LOCTEXT_NAMESPACE "ScreenshotComparison"

void SScreenShotBrowser::Construct( const FArguments& InArgs,  IScreenShotManagerRef InScreenShotManager  )
{
	ScreenShotManager = InScreenShotManager;
	ComparisonRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/Comparisons"));
	bReportsChanged = true;

	FModuleManager::Get().LoadModuleChecked(FName("ImageWrapper"));

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SDirectoryPicker)
				.Directory(ComparisonRoot)
				.OnDirectoryChanged(this, &SScreenShotBrowser::OnDirectoryChanged)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("DeleteAllReports", "Delete All Reports"))
				.ToolTipText(LOCTEXT("DeleteAllReportsTooltip", "Deletes all the current reports.  Reports are not removed unless the user resolves them, \nso if you just want to reset the state of the reports, clear them here and then re-run the tests."))
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Danger")
				.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
				.OnClicked_Lambda([this]()
				{
					while ( ComparisonList.Num() > 0 )
					{
						TSharedPtr<FScreenComparisonModel> Model = ComparisonList.Pop();
						Model->Complete();
					}

					return FReply::Handled();
				})
			]
		]
		
		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )
		[
			SAssignNew(ComparisonView, SListView< TSharedPtr<FScreenComparisonModel> >)
			.ListItemsSource(&ComparisonList)
			.OnGenerateRow(this, &SScreenShotBrowser::OnGenerateWidgetForScreenResults)
			.SelectionMode(ESelectionMode::None)
			.HeaderRow
			(
				SNew(SHeaderRow)

				+ SHeaderRow::Column("Name")
				.DefaultLabel(LOCTEXT("ColumnHeader_Name", "Name"))
				.FillWidth(1.0f)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Delta")
				.DefaultLabel(LOCTEXT("ColumnHeader_Delta", "Local | Global Delta"))
				.FixedWidth(120)
				.VAlignHeader(VAlign_Center)
				.HAlignHeader(HAlign_Center)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)

				+ SHeaderRow::Column("Preview")
				.DefaultLabel(LOCTEXT("ColumnHeader_Preview", "Preview"))
				.FixedWidth(500)
				.HAlignHeader(HAlign_Left)
				.HAlignCell(HAlign_Center)
				.VAlignCell(VAlign_Center)
			)
		]
	];

	RefreshDirectoryWatcher();
}

SScreenShotBrowser::~SScreenShotBrowser()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));

	if ( DirectoryWatchingHandle.IsValid() )
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(ComparisonRoot, DirectoryWatchingHandle);
	}
}

void SScreenShotBrowser::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if ( bReportsChanged )
	{
		RebuildTree();
	}
}

void SScreenShotBrowser::OnDirectoryChanged(const FString& Directory)
{
	ComparisonRoot = Directory;

	RefreshDirectoryWatcher();

	bReportsChanged = true;
}

void SScreenShotBrowser::RefreshDirectoryWatcher()
{
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	
	if ( DirectoryWatchingHandle.IsValid() )
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(ComparisonRoot, DirectoryWatchingHandle);
	}

	DirectoryWatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(ComparisonRoot, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &SScreenShotBrowser::OnReportsChanged), DirectoryWatchingHandle, IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
}

void SScreenShotBrowser::OnReportsChanged(const TArray<struct FFileChangeData>& /*FileChanges*/)
{
	bReportsChanged = true;
}

TSharedRef<ITableRow> SScreenShotBrowser::OnGenerateWidgetForScreenResults(TSharedPtr<FScreenComparisonModel> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	// Create the row widget.
	return
		SNew(SScreenComparisonRow, OwnerTable)
		.ScreenshotManager(ScreenShotManager)
		.ComparisonDirectory(ComparisonRoot)
		.ComparisonResult(InItem);
}

void SScreenShotBrowser::RebuildTree()
{
	bReportsChanged = false;
	ComparisonList.Reset();

	if ( ScreenShotManager->OpenComparisonReports(ComparisonRoot, CurrentReports) )
	{
		for ( const FComparisonReport& Report : CurrentReports )
		{
			TSharedPtr<FScreenComparisonModel> Model = MakeShared<FScreenComparisonModel>(Report);
			Model->OnComplete.AddLambda([this, Model] () {
				ComparisonList.Remove(Model);
				ComparisonView->RequestListRefresh();
			});

			ComparisonList.Add(Model);
		}
	}

	ComparisonView->RequestListRefresh();
}

#undef LOCTEXT_NAMESPACE
