// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequenceRecorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SOverlay.h"
#include "ActorRecording.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "AnimationRecorder.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "SequenceRecorderCommands.h"
#include "SequenceRecorderSettings.h"
#include "SequenceRecorder.h"
#include "SDropTarget.h"

#define LOCTEXT_NAMESPACE "SequenceRecorder"

static const FName ActorColumnName(TEXT("Actor"));
static const FName AnimationColumnName(TEXT("Animation"));
static const FName LengthColumnName(TEXT("Length"));

/** A widget to display information about an animation recording in the list view */
class SSequenceRecorderListRow : public SMultiColumnTableRow<UActorRecording*>
{
public:
	SLATE_BEGIN_ARGS(SSequenceRecorderListRow) {}

	/** The list item for this row */
	SLATE_ARGUMENT(UActorRecording*, Recording)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RecordingPtr = Args._Recording;

		SMultiColumnTableRow<UActorRecording*>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			OwnerTableView
			);
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == ActorColumnName)
		{
			return
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequenceRecorderListRow::GetRecordingActorName)));
		}
		else if (ColumnName == AnimationColumnName)
		{
			return
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequenceRecorderListRow::GetRecordingAnimationName)));
		}
		else if (ColumnName == LengthColumnName)
		{
			return
				SNew(STextBlock)
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequenceRecorderListRow::GetRecordingLengthText)));
		}

		return SNullWidget::NullWidget;
	}

private:
	FText GetRecordingActorName() const
	{
		FText ActorName(LOCTEXT("InvalidActorName", "None"));
		if (RecordingPtr.IsValid() && RecordingPtr.Get()->GetActorToRecord() != nullptr)
		{
			ActorName = FText::FromString(RecordingPtr.Get()->GetActorToRecord()->GetActorLabel());
		}

		return ActorName;
	}

	FText GetRecordingAnimationName() const
	{
		FText AnimationName(LOCTEXT("InvalidAnimationName", "None"));
		if (RecordingPtr.IsValid())
		{
			if(!RecordingPtr.Get()->bSpecifyTargetAnimation)
			{
				AnimationName = LOCTEXT("AutoCreatedAnimationName", "Auto-created");
			}
			else if(RecordingPtr.Get()->TargetAnimation.IsValid())
			{
				AnimationName = FText::FromString(RecordingPtr.Get()->TargetAnimation.Get()->GetName());
			}
		}

		return AnimationName;
	}

	FText GetRecordingLengthText() const
	{
		FText LengthText(LOCTEXT("InvalidLengthName", "None"));
		if (RecordingPtr.IsValid())
		{
			LengthText = FText::AsNumber(RecordingPtr.Get()->AnimationSettings.Length);
		}

		return LengthText;
	}

	TWeakObjectPtr<UActorRecording> RecordingPtr;
};


void SSequenceRecorder::Construct(const FArguments& Args)
{
	CommandList = MakeShareable(new FUICommandList);

	BindCommands();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;

	ActorRecordingDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	SequenceRecordingDetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
	SequenceRecordingDetailsView->SetObject(GetMutableDefault<USequenceRecorderSettings>());

	FToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None);

	ToolBarBuilder.BeginSection(TEXT("Recording"));
	{
		ToolBarBuilder.AddToolBarButton(FSequenceRecorderCommands::Get().RecordAll);
		ToolBarBuilder.AddToolBarButton(FSequenceRecorderCommands::Get().StopAll);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection(TEXT("RecordingManagement"));
	{
		ToolBarBuilder.AddToolBarButton(FSequenceRecorderCommands::Get().AddRecording);
		ToolBarBuilder.AddToolBarButton(FSequenceRecorderCommands::Get().RemoveRecording);
		ToolBarBuilder.AddToolBarButton(FSequenceRecorderCommands::Get().RemoveAllRecordings);
	}
	ToolBarBuilder.EndSection();

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+SSplitter::Slot()
		.Value(0.33f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				ToolBarBuilder.MakeWidget()
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew( SDropTarget )
							.OnAllowDrop( this, &SSequenceRecorder::OnRecordingListAllowDrop )
							.OnDrop( this, &SSequenceRecorder::OnRecordingListDrop )
							.Content()
							[
								SAssignNew(ListView, SListView<UActorRecording*>)
								.ListItemsSource(&FSequenceRecorder::Get().GetQueuedRecordings())
								.SelectionMode(ESelectionMode::SingleToggle)
								.OnGenerateRow(this, &SSequenceRecorder::MakeListViewWidget)
								.OnSelectionChanged(this, &SSequenceRecorder::OnSelectionChanged)
								.HeaderRow
								(
									SNew(SHeaderRow)
									+ SHeaderRow::Column(ActorColumnName)
									.FillWidth(43.0f)
									.DefaultLabel(LOCTEXT("ActorHeaderName", "Actor"))
									+ SHeaderRow::Column(AnimationColumnName)
									.FillWidth(43.0f)
									.DefaultLabel(LOCTEXT("AnimationHeaderName", "Animation"))
									+ SHeaderRow::Column(LengthColumnName)
									.FillWidth(14.0f)
									.DefaultLabel(LOCTEXT("LengthHeaderName", "Length"))
								)
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SSequenceRecorder::GetTargetSequenceName)
						]
					]
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)	
						+SVerticalBox::Slot()
						.VAlign(VAlign_Bottom)
						.MaxHeight(2.0f)
						[
							SAssignNew(DelayProgressBar, SProgressBar)
							.Percent(this, &SSequenceRecorder::GetDelayPercent)
							.Visibility(this, &SSequenceRecorder::GetDelayProgressVisibilty)
						]
					]
				]
			]
		]
		+SSplitter::Slot()
		.Value(0.66f)
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				.IsEnabled_Lambda([]() { return !FSequenceRecorder::Get().IsRecording(); })
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SequenceRecordingDetailsView.ToSharedRef()
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					ActorRecordingDetailsView.ToSharedRef()
				]	
			]
		]
	];

	// Register refresh timer
	if (!ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(this, &SSequenceRecorder::HandleRefreshItems));
	}
}

void SSequenceRecorder::BindCommands()
{
	CommandList->MapAction(FSequenceRecorderCommands::Get().RecordAll,
		FExecuteAction::CreateSP(this, &SSequenceRecorder::HandleRecord),
		FCanExecuteAction::CreateSP(this, &SSequenceRecorder::CanRecord),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &SSequenceRecorder::IsRecordVisible)
		);

	CommandList->MapAction(FSequenceRecorderCommands::Get().StopAll,
		FExecuteAction::CreateSP(this, &SSequenceRecorder::HandleStopAll),
		FCanExecuteAction::CreateSP(this, &SSequenceRecorder::CanStopAll),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &SSequenceRecorder::IsStopAllVisible)
		);

	CommandList->MapAction(FSequenceRecorderCommands::Get().AddRecording,
		FExecuteAction::CreateSP(this, &SSequenceRecorder::HandleAddRecording),
		FCanExecuteAction::CreateSP(this, &SSequenceRecorder::CanAddRecording)
		);

	CommandList->MapAction(FSequenceRecorderCommands::Get().RemoveRecording,
		FExecuteAction::CreateSP(this, &SSequenceRecorder::HandleRemoveRecording),
		FCanExecuteAction::CreateSP(this, &SSequenceRecorder::CanRemoveRecording)
		);

	CommandList->MapAction(FSequenceRecorderCommands::Get().RemoveAllRecordings,
		FExecuteAction::CreateSP(this, &SSequenceRecorder::HandleRemoveAllRecordings),
		FCanExecuteAction::CreateSP(this, &SSequenceRecorder::CanRemoveAllRecordings)
		);
}

TSharedRef<ITableRow> SSequenceRecorder::MakeListViewWidget(UActorRecording* Recording, const TSharedRef<STableViewBase>& OwnerTable) const
{
	return
		SNew(SSequenceRecorderListRow, OwnerTable)
		.Recording(Recording);
}

void SSequenceRecorder::OnSelectionChanged(UActorRecording* Recording, ESelectInfo::Type SelectionType) const
{
	if(Recording)
	{
		ActorRecordingDetailsView->SetObject(Recording);
	}
	else
	{
		ActorRecordingDetailsView->SetObject(nullptr);
	}
}

void SSequenceRecorder::HandleRecord()
{
	FSequenceRecorder::Get().StartRecording();
}

bool SSequenceRecorder::CanRecord() const
{
	return FSequenceRecorder::Get().HasQueuedRecordings();
}

bool SSequenceRecorder::IsRecordVisible() const
{
	return !FSequenceRecorder::Get().IsRecording() && !FAnimationRecorderManager::Get().IsRecording() && !FSequenceRecorder::Get().IsDelaying();
}

void SSequenceRecorder::HandleStopAll()
{
	FSequenceRecorder::Get().StopRecording();
}

bool SSequenceRecorder::CanStopAll() const
{
	return FSequenceRecorder::Get().IsRecording() || FAnimationRecorderManager::Get().IsRecording() || FSequenceRecorder::Get().IsDelaying();
}

bool SSequenceRecorder::IsStopAllVisible() const
{
	return FSequenceRecorder::Get().IsRecording() || FAnimationRecorderManager::Get().IsRecording() || FSequenceRecorder::Get().IsDelaying();
}

void SSequenceRecorder::HandleAddRecording()
{
	FSequenceRecorder::Get().AddNewQueuedRecording();
}

bool SSequenceRecorder::CanAddRecording() const
{
	return !FAnimationRecorderManager::Get().IsRecording();
}

void SSequenceRecorder::HandleRemoveRecording()
{
	TArray<UActorRecording*> SelectedRecordings;
	ListView->GetSelectedItems(SelectedRecordings);
	UActorRecording* SelectedRecording = SelectedRecordings.Num() > 0 ? SelectedRecordings[0] : nullptr;
	if(SelectedRecording)
	{
		FSequenceRecorder::Get().RemoveQueuedRecording(SelectedRecording);

		TArray<TWeakObjectPtr<UObject>> SelectedObjects = ActorRecordingDetailsView->GetSelectedObjects();
		if(SelectedObjects.Num() > 0 && SelectedObjects[0].Get() == SelectedRecording)
		{
			ActorRecordingDetailsView->SetObject(nullptr);
		}
	}
}

bool SSequenceRecorder::CanRemoveRecording() const
{
	return ListView->GetNumItemsSelected() > 0 && !FAnimationRecorderManager::Get().IsRecording();
}

void SSequenceRecorder::HandleRemoveAllRecordings()
{
	FSequenceRecorder::Get().ClearQueuedRecordings();
	ActorRecordingDetailsView->SetObject(nullptr);
}

bool SSequenceRecorder::CanRemoveAllRecordings() const
{
	return FSequenceRecorder::Get().HasQueuedRecordings() && !FAnimationRecorderManager::Get().IsRecording();
}

EActiveTimerReturnType SSequenceRecorder::HandleRefreshItems(double InCurrentTime, float InDeltaTime)
{
	if(FSequenceRecorder::Get().AreQueuedRecordingsDirty())
	{
		ListView->RequestListRefresh();

		FSequenceRecorder::Get().ResetQueuedRecordingsDirty();
	}

	return EActiveTimerReturnType::Continue;
}

TOptional<float> SSequenceRecorder::GetDelayPercent() const
{
	const float Delay = GetDefault<USequenceRecorderSettings>()->RecordingDelay;
	const float Countdown = FSequenceRecorder::Get().GetCurrentDelay();
	return Delay > 0.0f ? Countdown / Delay : 0.0f;
}

EVisibility SSequenceRecorder::GetDelayProgressVisibilty() const
{
	return FSequenceRecorder::Get().IsDelaying() ? EVisibility::Visible : EVisibility::Hidden;
}

FText SSequenceRecorder::GetTargetSequenceName() const
{
	return FText::Format(LOCTEXT("NextSequenceFormat", "Next Sequence: {0}"), FText::FromString(FSequenceRecorder::Get().GetNextSequenceName()));
}

bool SSequenceRecorder::OnRecordingListAllowDrop( TSharedPtr<FDragDropOperation> DragDropOperation )
{
	return DragDropOperation->IsOfType<FActorDragDropOp>();
}


FReply SSequenceRecorder::OnRecordingListDrop( TSharedPtr<FDragDropOperation> DragDropOperation )
{
	if ( DragDropOperation->IsOfType<FActorDragDropOp>() )
	{
		TSharedPtr<FActorDragDropOp> ActorDragDropOperation = StaticCastSharedPtr<FActorDragDropOp>( DragDropOperation );

		for (auto Actor : ActorDragDropOperation->Actors)
		{
			if (Actor.IsValid())
			{
				FSequenceRecorder::Get().AddNewQueuedRecording(Actor.Get());
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
