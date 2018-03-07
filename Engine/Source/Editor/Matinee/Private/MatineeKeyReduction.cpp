// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Matinee/InterpTrack.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpGroupInst.h"
#include "MatineeOptions.h"
#include "MatineeTransBuffer.h"
#include "Matinee.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"


/**
 * Dialog that requests the key reduction parameters from the user.
 * These parameters are tolerance and the reduction interval.
 */
class SMatineeKeyReduction : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMatineeKeyReduction)
	{} 
	SLATE_END_ARGS()

	/** Destructor */
	~SMatineeKeyReduction() { }

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs, FMatinee* InMatinee, UInterpTrack* InTrack, UInterpTrackInst* InTrackInst, float InIntervalStart, float InIntervalEnd );

public:
	
	FMatinee *Matinee;
	UInterpTrack *Track;
	UInterpTrackInst *TrackInst;
	float InitialIntervalStart;
	float InitialIntervalEnd;

	//Return values
	int32 Tolerance;
	bool FullInterval;
	float IntervalStart;
	float IntervalEnd;

private:

	void SetTolerance(int32 InTolerance) { Tolerance = InTolerance; }
	void ToggleFullInterval(ECheckBoxState CheckState) { FullInterval = (CheckState == ECheckBoxState::Checked); }
	void SetIntervalStart(float InStart) { IntervalStart = InStart; }
	void SetIntervalEnd(float InEnd) { IntervalEnd = InEnd; }

	TOptional<int32> GetTolerance() const {return Tolerance;}
	bool  UseFullInterval() const { return FullInterval; }
	TOptional<float> GetIntervalStart() const {return IntervalStart; }
	TOptional<float> GetIntervalEnd() const {return IntervalEnd; }

	// The dialog control event-handlers.
	FReply OnOK();
};

void SMatineeKeyReduction::Construct( const FArguments& InArgs, 
	FMatinee* InMatinee, UInterpTrack* InTrack, UInterpTrackInst* InTrackInst, float InIntervalStart, float InIntervalEnd )
{
	Matinee = InMatinee;
	Track = InTrack;
	TrackInst = InTrackInst;

	IntervalStart = InIntervalStart;
	IntervalEnd = InIntervalEnd;

	InitialIntervalStart = IntervalStart;
	InitialIntervalEnd = IntervalEnd;

	Tolerance = 0;

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		.Padding(1)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(0,0,10,0)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.KeyReduction", "Tolerance", "Tolerance (%)"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SBox)
					.WidthOverride(100.f)
					[
						SNew(SNumericEntryBox<int32>)
						.AllowSpin(true)
						.MinValue(1)
						.MaxValue(100)
						.MinSliderValue(1)
						.MaxSliderValue(100)
						.Value(this, &SMatineeKeyReduction::GetTolerance)
						.OnValueChanged(this, &SMatineeKeyReduction::SetTolerance)
					]
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.KeyReduction", "FullInterval", "Full Interval"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this,&SMatineeKeyReduction::ToggleFullInterval)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.KeyReduction", "IntervalStart", "Interval Start"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(100.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SMatineeKeyReduction::GetIntervalStart)
					.OnValueChanged(this, &SMatineeKeyReduction::SetIntervalStart)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.KeyReduction", "IntervalEnd", "Interval End"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.MaxWidth(100.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SMatineeKeyReduction::GetIntervalEnd)
					.OnValueChanged(this, &SMatineeKeyReduction::SetIntervalEnd)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(EHorizontalAlignment::HAlign_Right)
				[
					SNew(SButton)
					.Text(NSLOCTEXT("UnrealEd", "OK", "OK"))
					.OnClicked(this, &SMatineeKeyReduction::OnOK)
				]
			]
		]
	];
}

FReply SMatineeKeyReduction::OnOK()
{
	if (FullInterval)
	{
		IntervalStart = InitialIntervalStart;
		IntervalEnd = InitialIntervalEnd;
	}

	// Allows for undo capabilities.
	Matinee->InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "ReduceKeys", "Reduce Keys...") );
	Track->Modify();
	Matinee->Opt->Modify();

	Matinee->ReduceKeysForTrack( Track, TrackInst, IntervalStart, IntervalEnd, Tolerance );

	// Update to current time, in case new key affects state of scene.
	Matinee->RefreshInterpPosition();

	// Dirty the track window viewports
	Matinee->InvalidateTrackWindowViewports();

	Matinee->InterpEdTrans->EndSpecial();

	Matinee->CloseEntryPopupMenu();

	return FReply::Handled();
}

void FMatinee::ReduceKeysForTrack( UInterpTrack* Track, UInterpTrackInst* TrackInst, float IntervalStart, float IntervalEnd, float Tolerance )
{
	Track->ReduceKeys(IntervalStart, IntervalEnd, Tolerance);
}

void FMatinee::ReduceKeys()
{
	// Set-up based on the "AddKey" function.
	// This set-up gives us access to the essential undo/redo functionality.
	ClearKeySelection();

	if( !HasATrackSelected() )
	{
		FNotificationInfo NotificationInfo(NSLOCTEXT("UnrealEd", "NoTrackSelected", "No track selected. Select a track from the track view before trying again."));
		NotificationInfo.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	}
	else
	{
		for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
		{
			UInterpTrack* Track = *TrackIt;
			UInterpGroup* Group = TrackIt.GetGroup();
			UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst( Group );
			check(GrInst);

			UInterpTrackInst* TrackInst = NULL;
			UInterpTrack* Parent = Cast<UInterpTrack>( Track->GetOuter() );

			if( Parent )
			{
				int32 Index = Group->InterpTracks.Find( Parent );
				check(Index != INDEX_NONE);
				TrackInst = GrInst->TrackInst[ Index ];
			}
			else
			{
				TrackInst = GrInst->TrackInst[ TrackIt.GetTrackIndex() ];
			}
			check(TrackInst);

			// Request the key reduction parameters from the user.
			float IntervalStart, IntervalEnd;
			Track->GetTimeRange(IntervalStart, IntervalEnd);

			TSharedRef<SMatineeKeyReduction> ParameterDialog = 
				SNew(SMatineeKeyReduction, this, Track, TrackInst, IntervalStart, IntervalEnd);

			EntryPopupMenu = FSlateApplication::Get().PushMenu(
				ToolkitHost.Pin()->GetParentWidget(),
				FWidgetPath(),
				ParameterDialog,
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
				);

			//only allow a single selection at a time
			break;
		}
	}
}
