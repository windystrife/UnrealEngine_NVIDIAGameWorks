// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "ActorRecording.h"

class FActiveTimerHandle;
class FUICommandList;
class IDetailsView;
class SProgressBar;
class FDragDropOperation;

class SSequenceRecorder : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSequenceRecorder)
	{
	}

	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

private:

	void BindCommands();

	TSharedRef<ITableRow> MakeListViewWidget(class UActorRecording* Recording, const TSharedRef<STableViewBase>& OwnerTable) const;

	FText GetRecordingActorName(class UActorRecording* Recording) const;

	void OnSelectionChanged(UActorRecording* Recording, ESelectInfo::Type SelectionType) const;

	void HandleRecord();

	EActiveTimerReturnType StartDelayedRecord(double InCurrentTime, float InDeltaTime);

	bool CanRecord() const;

	bool IsRecordVisible() const;

	void HandleStopAll();

	bool CanStopAll() const;

	bool IsStopAllVisible() const;

	void HandleAddRecording();

	bool CanAddRecording() const;

	void HandleRemoveRecording();

	bool CanRemoveRecording() const;

	void HandleRemoveAllRecordings();

	bool CanRemoveAllRecordings() const;

	EActiveTimerReturnType HandleRefreshItems(double InCurrentTime, float InDeltaTime);

	TOptional<float> GetDelayPercent() const;

	void OnDelayChanged(float NewValue);

	EVisibility GetDelayProgressVisibilty() const;

	FText GetTargetSequenceName() const;

	FReply OnRecordingListDrop( TSharedPtr<FDragDropOperation> DragDropOperation );

	bool OnRecordingListAllowDrop( TSharedPtr<FDragDropOperation> DragDropOperation );

private:
	TSharedPtr<IDetailsView> SequenceRecordingDetailsView;

	TSharedPtr<IDetailsView> ActorRecordingDetailsView;

	TSharedPtr<SListView<UActorRecording*>> ListView;

	TSharedPtr<FUICommandList> CommandList;

	/** The handle to the refresh tick timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	TSharedPtr<SProgressBar> DelayProgressBar;
};
