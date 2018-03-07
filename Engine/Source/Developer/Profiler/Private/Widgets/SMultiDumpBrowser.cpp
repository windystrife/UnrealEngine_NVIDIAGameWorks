// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMultiDumpBrowser.h"


#define LOCTEXT_NAMESPACE "SMultiDumpBrowser"


void SMultiDumpBrowser::Construct(const FArguments& InArgs)
{
	this->SetEnabled(true);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.Padding(4)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(4).HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
						[
							SNew(STextBlock).Text(LOCTEXT("MultiDumpBrowserThreadTotals", "Show thread totals for:"))
						]
					+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
						[
							SAssignNew(DisplayTotalsBox, SEditableTextBox).OnTextCommitted(this, &SMultiDumpBrowser::PrefilterTextCommitted).ToolTipText(LOCTEXT("MultiDumpBrowserTooltip", "Use \"Load Folder\" above to load a folder of stats dumps. GT/RT totals for stats matching text entered here will be displayed in the list below - e.g. enter \"Particle\" here to show total thread times for particle emitters."))
						]
				]
			]
			+ SVerticalBox::Slot().Padding(2).FillHeight(1.0f)
			[
				SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(4).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
				[
					SAssignNew(FileList, SListView<TSharedPtr<FFileDescriptor>>)
					.ListItemsSource(&StatsFiles)
					.ItemHeight(16)
					.OnGenerateRow(this, &SMultiDumpBrowser::GenerateFileRow)
					.OnSelectionChanged(this, &SMultiDumpBrowser::SelectionChanged)
				]
			]
		]
	];
}


TSharedRef<ITableRow> SMultiDumpBrowser::GenerateFileRow(TSharedPtr<FFileDescriptor> FileItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SFileTableRow, OwnerTable, FileItem);
}


void SMultiDumpBrowser::SelectionChanged(TSharedPtr<FFileDescriptor> SelectedItem, ESelectInfo::Type SelType)
{
	FProfilerManager::Get()->LoadProfilerCapture(SelectedItem->FullPath);
}


void SMultiDumpBrowser::Update()
{
	FileList->RequestListRefresh();
}


void SMultiDumpBrowser::AddFile(FFileDescriptor *InFileDesc)
{
	StatsFiles.Add(MakeShareable(InFileDesc));
}


void SMultiDumpBrowser::Clear()
{
	StatsFiles.Empty();
}


void SMultiDumpBrowser::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	static bool bCurrentlyLoading = false;

	// iterate over all stats files, load one at a time, analyze
	for (auto Desc : StatsFiles)
	{
		// if we're not currently loading one and we've found one in the list that was added but not analyzed, load it
		if (Desc->Status == FFileDescriptor::EQueued && bCurrentlyLoading == false)
		{
			FProfilerManager::Get()->LoadProfilerCapture(Desc->FullPath);
			Desc->Status = FFileDescriptor::ELoading;
			Desc->TimeString = "Getting timings, please wait...";
			bCurrentlyLoading = true;
		}

		// if the current load is completed, start summing up thread totals for the term we're looking for in the DisplayTotalsBox
		if (FProfilerManager::Get()->IsCaptureFileFullyProcessed() && Desc->Status == FFileDescriptor::ELoading)
		{
			Desc->Status = FFileDescriptor::ELoaded;
			bCurrentlyLoading = false;
			FindTotalsForPrefilter(Desc);

			Desc->Status = FFileDescriptor::EAnalyzed;
		}
	}

	if (bCurrentlyLoading == true)
	{
		this->SetEnabled(false);
	}
	else
	{
		this->SetEnabled(true);
	}

	SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}


void SMultiDumpBrowser::FindTotalsForPrefilterRecursive(TSharedPtr<FEventGraphSample> Event, float &OutTotalTime)
{
	FString EventName = Event->_StatName.ToString().ToUpper();
	if (EventName.Contains(TotalsFilteringText.ToUpper()))
	{
		OutTotalTime += Event->_InclusiveTimeMS;
	}
	else
	{
		for (auto Child : Event->GetChildren())
		{
			FindTotalsForPrefilterRecursive(Child, OutTotalTime);
		}
	}
}


void SMultiDumpBrowser::FindTotalsForPrefilter(TSharedPtr<FFileDescriptor> Desc)
{
	float TotalRenderThreadTime = 0;
	float TotalGameThreadTime = 0;

	// if we don't have a totals filtering text, display the entire RT and GT times
	bool bGetBaseThreadTimes = false;
	if (TotalsFilteringText == "")
	{
		bGetBaseThreadTimes = true;
	}

	FEventGraphSamplePtr RootPtr = FProfilerManager::Get()->GetProfilerSession()->GetEventGraphDataAverage()->GetRoot();

	// start at the root's children, which are the threads
	for (auto Child : RootPtr->GetChildren())
	{
		TArray<FString>EventNameTokens;
		Child->_ThreadName.ToString().ParseIntoArray(EventNameTokens, TEXT(" "));

		// get cumulative times for stats on the render thread
		if (EventNameTokens[0] == "RenderThread")
		{
			if (bGetBaseThreadTimes)
			{
				TotalsFilteringText = "RenderThread";
			}
			FindTotalsForPrefilterRecursive(Child, TotalRenderThreadTime);
		}
		// get cumulative times for stats on the game thread
		else if (EventNameTokens[0] == "GameThread")
		{
			if (bGetBaseThreadTimes)
			{
				TotalsFilteringText = "GameThread";
			}
			FindTotalsForPrefilterRecursive(Child, TotalGameThreadTime);
		}

		if (bGetBaseThreadTimes)
		{
			TotalsFilteringText = "";
		}
	}

	Desc->TimeString = "RT " + FString::Printf(TEXT("%.2f"), TotalRenderThreadTime) + " / GT " + FString::Printf(TEXT("%.2f"), TotalGameThreadTime);
}


void SMultiDumpBrowser::PrefilterTextCommitted(const FText& InText, const ETextCommit::Type InTextAction)
{
	TotalsFilteringText = InText.ToString();
	for (auto Desc : StatsFiles)
	{
		Desc->Status = FFileDescriptor::EQueued;
	}
}


#undef LOCTEXT_NAMESPACE
