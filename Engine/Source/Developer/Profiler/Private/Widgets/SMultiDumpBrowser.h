// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ProfilerManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SEditableTextBox.h"

/**
 * A custom widget used to display a histogram.
 */
class SMultiDumpBrowser
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMultiDumpBrowser) { }
	SLATE_END_ARGS()

	/**
	 * Descriptor for a single stats file; associated with a table row, so we can load on demand.
	 */
	class FFileDescriptor
	{
	public:
		enum DescriptorStatus
		{
			EQueued = 0,
			ELoading,
			ELoaded,
			EAnalyzed
		};
		FFileDescriptor()
			:Status(EQueued)
		{
		}

		FString FullPath;
		FString DisplayName;
		FString TimeString;
		DescriptorStatus Status;
		FText GetDisplayNameString() const
		{
			return FText::FromString(DisplayName + " - " + TimeString);
		}
	};


	/**
	 * Table row for the stats dump file list view.
	 */
	class SFileTableRow
		: public STableRow<TSharedPtr<FFileDescriptor>>
	{
	public:
		SLATE_BEGIN_ARGS(SFileTableRow) { }
		SLATE_ARGUMENT(TSharedPtr<FFileDescriptor>, Desc)
			//		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
		SLATE_END_ARGS()

		TSharedPtr<FFileDescriptor> Desc;

		FText GetDisplayName() const
		{
			return Desc->GetDisplayNameString();
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FFileDescriptor>& InFileDesc)
		{
			STableRow<TSharedPtr<FFileDescriptor>>::Construct(STableRow<TSharedPtr<FFileDescriptor>>::FArguments(), OwnerTable);
			Desc = InFileDesc;

			ChildSlot
				[
					SNew(SBox)
					[
						SNew(STextBlock)
						.Text(this, &SFileTableRow::GetDisplayName)
					]
				];
		}
	};


	/**
	* Construct this widget.
	*
	* @param InArgs The declaration data for this widget.
	*/
	void Construct(const FArguments& InArgs);

	TSharedRef<ITableRow> GenerateFileRow(TSharedPtr<FFileDescriptor> FileItem, const TSharedRef<STableViewBase>& OwnerTable);
	
	void SelectionChanged(TSharedPtr<FFileDescriptor> SelectedItem, ESelectInfo::Type SelType);
	void Update();
	void AddFile(FFileDescriptor *InFileDesc);
	void Clear();

	/**
	 * Recursively searches starting at Event for stats matching the TotalsFilteringText.
	 * and returns total times.
	 *
	 * @param Event Start recursively accumulating at this event.
	 * @param OutTotalTime Total accumulated inclusive time for all stats matching TotalsFilteringText.
	 */
	void FindTotalsForPrefilterRecursive(TSharedPtr<FEventGraphSample> Event, float &OutTotalTime);

	/* Find total RT and GT times for stats in dump file Desc, matching TotalsFilteringText
 	 * @param	Desc	FileDescriptor for the stats dump file to traverse
	 */
	void FindTotalsForPrefilter(TSharedPtr<FFileDescriptor> Desc);
	void PrefilterTextCommitted(const FText& InText, const ETextCommit::Type InTextAction);

public:

	//~ SWidget interface

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	TArray<TSharedPtr<FFileDescriptor>> StatsFiles;

	TSharedPtr< SListView<TSharedPtr<FFileDescriptor>> > FileList;

	/** edit box determining for which stats names to show thread time totals in the file list. */
	TSharedPtr<SEditableTextBox> DisplayTotalsBox;

	FString TotalsFilteringText;
};
