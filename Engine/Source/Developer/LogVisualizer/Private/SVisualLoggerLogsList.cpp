// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SVisualLoggerLogsList.h"
#include "Misc/OutputDeviceHelper.h"
#include "Widgets/Text/STextBlock.h"
#include "LogVisualizerSettings.h"
#include "VisualLoggerDatabase.h"
#include "LogVisualizerStyle.h"
#include "Widgets/Views/SListView.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SVisualLoggerLogsList"

struct FLogEntryItem
{
	FString Category;
	FLinearColor CategoryColor;
	ELogVerbosity::Type Verbosity;
	FString Line;
	int64 UserData;
	FName TagName;
};

namespace ELogsSortMode
{
	enum Type
	{
		ByName,
		ByStartTime,
		ByEndTime,
	};
}

void SVisualLoggerLogsList::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
	ChildSlot
		[
			SAssignNew(LogsLinesWidget, SListView<TSharedPtr<FLogEntryItem> >)
			.ItemHeight(20)
			.ListItemsSource(&CachedLogEntryLines)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SVisualLoggerLogsList::LogEntryLineSelectionChanged)
			.OnGenerateRow(this, &SVisualLoggerLogsList::LogEntryLinesGenerateRow)
		];

	FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.AddRaw(this, &SVisualLoggerLogsList::OnItemSelectionChanged);
	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.AddRaw(this, &SVisualLoggerLogsList::ObjectSelectionChanged);
	FLogVisualizer::Get().GetEvents().OnFiltersChanged.AddRaw(this, &SVisualLoggerLogsList::OnFiltersChanged);
}

SVisualLoggerLogsList::~SVisualLoggerLogsList()
{
	FVisualLoggerDatabase::Get().GetEvents().OnItemSelectionChanged.RemoveAll(this);
	FVisualLoggerDatabase::Get().GetEvents().OnRowSelectionChanged.RemoveAll(this);
	FLogVisualizer::Get().GetEvents().OnFiltersChanged.RemoveAll(this);
}

FReply SVisualLoggerLogsList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::C && (InKeyEvent.IsLeftCommandDown() || InKeyEvent.IsLeftControlDown()))
	{
		FString ClipboardString;
		for (const TSharedPtr<struct FLogEntryItem>& CurrentItem : LogsLinesWidget->GetSelectedItems())
		{
			if (CurrentItem->Category.Len() > 0)
				ClipboardString += CurrentItem->Category + FString(TEXT(" (")) + FString(FOutputDeviceHelper::VerbosityToString(CurrentItem->Verbosity)) + FString(TEXT(") ")) + CurrentItem->Line;
			else
				ClipboardString += CurrentItem->Line;

			ClipboardString += TEXT("\n");
		}
		FPlatformApplicationMisc::ClipboardCopy(*ClipboardString);
		return FReply::Handled();
	}

	return SVisualLoggerBaseWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SVisualLoggerLogsList::OnFiltersChanged()
{
	RegenerateLogEntries();
	LogsLinesWidget->RequestListRefresh();
}

TSharedRef<ITableRow> SVisualLoggerLogsList::LogEntryLinesGenerateRow(TSharedPtr<FLogEntryItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (Item->Category.Len() > 0)
	{
		return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor(Item->CategoryColor))
					.Text(FText::FromString(Item->Category))
					.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(5.0f, 0.0f))
					[
						SNew(STextBlock)
						.ColorAndOpacity(FSlateColor(Item->Verbosity == ELogVerbosity::Error ? FLinearColor::Red : (Item->Verbosity == ELogVerbosity::Warning ? FLinearColor::Yellow : FLinearColor::Gray)))
						.Text(FText::FromString(FString(TEXT("(")) + FString(FOutputDeviceHelper::VerbosityToString(Item->Verbosity)) + FString(TEXT(")"))))
					]
				+ SHorizontalBox::Slot()
					.Padding(FMargin(5.0f, 0.0f))
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(Item->Verbosity == ELogVerbosity::Error ? FLinearColor::Red : (Item->Verbosity == ELogVerbosity::Warning ? FLinearColor::Yellow : FLinearColor::Gray)))
						.Text(FText::FromString(Item->Line))
						.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
						.TextStyle(FLogVisualizerStyle::Get(), TEXT("TextLogs.Text"))
					]
			];

	}
	else
	{
		return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(STextBlock)
					/*.AutoWrapText(true)*/
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.Text(FText::FromString(Item->Line))
					.HighlightText(this, &SVisualLoggerLogsList::GetFilterText)
					.Justification(ETextJustify::Center)
				]
			];
	}
}

FText SVisualLoggerLogsList::GetFilterText() const
{
	static FText NoText;
	const bool bSearchInsideLogs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bSearchInsideLogs;
	return bSearchInsideLogs ? FText::FromString(FVisualLoggerFilters::Get().GetSearchString()) : NoText;
}

void SVisualLoggerLogsList::OnFiltersSearchChanged(const FText& Filter)
{
	OnFiltersChanged();
}

void SVisualLoggerLogsList::LogEntryLineSelectionChanged(TSharedPtr<FLogEntryItem> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectedItem.IsValid() == true)
	{
		FLogVisualizer::Get().GetEvents().OnLogLineSelectionChanged.ExecuteIfBound(SelectedItem, SelectedItem->UserData, SelectedItem->TagName);
	}
	else
	{
		FLogVisualizer::Get().GetEvents().OnLogLineSelectionChanged.ExecuteIfBound(SelectedItem, 0, NAME_None);
	}
}

void SVisualLoggerLogsList::ObjectSelectionChanged(const TArray<FName>&)
{
	RegenerateLogEntries();
	LogsLinesWidget->RequestListRefresh();
}

void SVisualLoggerLogsList::ResetData()
{
	CachedLogEntryLines.Reset();
	LogsLinesWidget->RequestListRefresh();
}

void SVisualLoggerLogsList::OnItemSelectionChanged(const FVisualLoggerDBRow& BDRow, int32 ItemIndex)
{
	RegenerateLogEntries();
}

void SVisualLoggerLogsList::RegenerateLogEntries()
{
	CachedLogEntryLines.Reset();

	const TArray<FName> SelectedRows = FVisualLoggerDatabase::Get().GetSelectedRows();
	for (FName CurrentRow : SelectedRows)
	{
		if (FVisualLoggerDatabase::Get().IsRowVisible(CurrentRow)  == false)
		{
			continue;
		}

		const FVisualLoggerDBRow& DBRow = FVisualLoggerDatabase::Get().GetRowByName(CurrentRow);
		const TArray<FVisualLogDevice::FVisualLogEntryItem>& Entries = FVisualLoggerDatabase::Get().GetRowByName(CurrentRow).GetItems();
		
		int32 BestItemIndex = INDEX_NONE;
		float BestDistance = MAX_FLT;
		for (int32 Index = 0; Index < Entries.Num(); Index++)
		{
			auto& CurrentEntryItem = Entries[Index];
			if (DBRow.IsItemVisible(Index) == false)
			{
				continue;
			}

			TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
			const float CurrentDist = DBRow.GetCurrentItemIndex() == INDEX_NONE ? 0 : FMath::Abs(CurrentEntryItem.Entry.TimeStamp - DBRow.GetCurrentItem().Entry.TimeStamp);
			if (CurrentDist < BestDistance)
			{
				BestDistance = CurrentDist;
				BestItemIndex = Index;
			}
		}

		if (Entries.IsValidIndex(BestItemIndex))
		{
			GenerateLogs(Entries[BestItemIndex], SelectedRows.Num() > 1);
		}
	}
}

void SVisualLoggerLogsList::GenerateLogs(const FVisualLogDevice::FVisualLogEntryItem& LogEntry, bool bGenerateHeader)
{
	TArray<FVisualLoggerCategoryVerbosityPair> OutCategories;
	FVisualLoggerHelpers::GetCategories(LogEntry.Entry, OutCategories);
	bool bHasValidCategory = false;
	for (auto& CurrentCategory : OutCategories)
	{
		bHasValidCategory |= FVisualLoggerFilters::Get().MatchCategoryFilters(CurrentCategory.CategoryName.ToString(), CurrentCategory.Verbosity);
	}
	
	if (!bHasValidCategory)
	{
		return;
	}

	if (bGenerateHeader)
	{
		FLogEntryItem EntryItem;
		EntryItem.Category = FString();
		EntryItem.CategoryColor = FLinearColor::Black;
		EntryItem.Verbosity = ELogVerbosity::VeryVerbose;
		EntryItem.Line = LogEntry.OwnerName.ToString();
		EntryItem.UserData = 0;
		EntryItem.TagName = NAME_None;
		CachedLogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
	}

	const FVisualLogLine* LogLine = LogEntry.Entry.LogLines.GetData();
	const bool bSearchInsideLogs = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->bSearchInsideLogs;
	for (int LineIndex = 0; LineIndex < LogEntry.Entry.LogLines.Num(); ++LineIndex, ++LogLine)
	{
		bool bShowLine = FVisualLoggerFilters::Get().MatchCategoryFilters(LogLine->Category.ToString(), LogLine->Verbosity);
		if (bSearchInsideLogs)
		{
			FString String = FVisualLoggerFilters::Get().GetSearchString();
			if (String.Len() > 0)
			{
				bShowLine &= LogLine->Line.Find(String) != INDEX_NONE || LogLine->Category.ToString().Find(String) != INDEX_NONE;
			}
		}
		

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = LogLine->Category.ToString();
			EntryItem.CategoryColor = FLogVisualizer::Get().GetColorForCategory(LogLine->Category.ToString());
			EntryItem.Verbosity = LogLine->Verbosity;
			EntryItem.Line = LogLine->Line;
			EntryItem.UserData = LogLine->UserData;
			EntryItem.TagName = LogLine->TagName;

			CachedLogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
		}
	}

	for (auto& Event : LogEntry.Entry.Events)
	{
		bool bShowLine = FVisualLoggerFilters::Get().MatchCategoryFilters(Event.Name, Event.Verbosity);

		if (bShowLine)
		{
			FLogEntryItem EntryItem;
			EntryItem.Category = Event.Name;
			EntryItem.CategoryColor = FLogVisualizer::Get().GetColorForCategory(*EntryItem.Category);
			EntryItem.Verbosity = Event.Verbosity;
			EntryItem.Line = FString::Printf(TEXT("Registered event: '%s' (%d times)%s"), *Event.Name, Event.Counter, Event.EventTags.Num() ? TEXT("\n") : TEXT(""));
			for (auto& EventTag : Event.EventTags)
			{
				EntryItem.Line += FString::Printf(TEXT("%d times for tag: '%s'\n"), EventTag.Value, *EventTag.Key.ToString());
			}
			EntryItem.UserData = Event.UserData;
			EntryItem.TagName = Event.TagName;

			CachedLogEntryLines.Add(MakeShareable(new FLogEntryItem(EntryItem)));
		}

	}

	LogsLinesWidget->RequestListRefresh();
}
#undef LOCTEXT_NAMESPACE
