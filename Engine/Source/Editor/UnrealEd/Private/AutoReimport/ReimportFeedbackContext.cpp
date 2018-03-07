// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutoReimport/ReimportFeedbackContext.h"
#include "Animation/CurveSequence.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"

#include "FileCacheUtilities.h"
#include "MessageLogModule.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "ReimportContext"

class SWidgetStack : public SVerticalBox
{
	SLATE_BEGIN_ARGS(SWidgetStack){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 InMaxNumVisible)
	{
		MaxNumVisible = InMaxNumVisible;
		SlideCurve = FCurveSequence(0.f, .5f, ECurveEaseFunction::QuadOut);
		SizeCurve = FCurveSequence(0.f, .5f, ECurveEaseFunction::QuadOut);

		StartSlideOffset = 0;
		StartSizeOffset = FVector2D(ForceInitToZero);
	}

	FVector2D ComputeTotalSize() const
	{
		FVector2D Size(ForceInitToZero);
		for (int32 Index = 0; Index < FMath::Min(NumSlots(), MaxNumVisible); ++Index)
		{
			const FVector2D& ChildSize = Children[Index].GetWidget()->GetDesiredSize();
			if (ChildSize.X > Size.X)
			{
				Size.X = ChildSize.X;
			}
			Size.Y += ChildSize.Y + Children[Index].SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
		}
		return Size;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Lerp = SizeCurve.GetLerp();
		return ComputeTotalSize() * Lerp + StartSizeOffset * (1.f-Lerp);
	}

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override
	{
		if (Children.Num() == 0)
		{
			return;
		}

		const float Alpha = 1.f - SlideCurve.GetLerp();
		float PositionSoFar = AllottedGeometry.GetLocalSize().Y + StartSlideOffset*Alpha;

		for (int32 Index = 0; Index < NumSlots(); ++Index)
		{
			const SBoxPanel::FSlot& CurChild = Children[Index];
			const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();

			if (ChildVisibility != EVisibility::Collapsed)
			{
				const FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

				const FMargin SlotPadding(CurChild.SlotPadding.Get());
				const FVector2D SlotSize(AllottedGeometry.Size.X, ChildDesiredSize.Y + SlotPadding.GetTotalSpaceAlong<Orient_Vertical>());

				const AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( SlotSize.X, CurChild, SlotPadding );
				const AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( SlotSize.Y, CurChild, SlotPadding );

				ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
					CurChild.GetWidget(),
					FVector2D( XAlignmentResult.Offset, PositionSoFar - SlotSize.Y + YAlignmentResult.Offset ),
					FVector2D( XAlignmentResult.Size, YAlignmentResult.Size )
					));

				PositionSoFar -= SlotSize.Y;
			}
		}
	}

	void Add(const TSharedRef<SWidget>& InWidget)
	{
		TSharedPtr<SWidgetStackItem> NewItem;

		InsertSlot(0)
		.AutoHeight()
		[
			SAssignNew(NewItem, SWidgetStackItem)
			[
				InWidget
			]
		];
		
		{
			auto Widget = Children[0].GetWidget();
			Widget->SlatePrepass();

			const float WidgetHeight = Widget->GetDesiredSize().Y;
			StartSlideOffset += WidgetHeight;
			// Fade in time is 1 second x the proportion of the slide amount that this widget takes up
			NewItem->FadeIn(WidgetHeight / StartSlideOffset);

			if (!SlideCurve.IsPlaying())
			{
				SlideCurve.Play(AsShared());
			}
		}

		const FVector2D NewSize = ComputeTotalSize();
		if (NewSize != StartSizeOffset)
		{
			StartSizeOffset = NewSize;

			if (!SizeCurve.IsPlaying())
			{
				SizeCurve.Play(AsShared());
			}
		}
	}
	
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (!SlideCurve.IsPlaying())
		{
			StartSlideOffset = 0;
		}

		// Delete any widgets that are now offscreen
		if (Children.Num() != 0)
		{
			const float Alpha = 1.f - SlideCurve.GetLerp();
			float PositionSoFar = AllottedGeometry.GetLocalSize().Y + Alpha * StartSlideOffset;

			for (int32 Index = 0; PositionSoFar > 0 && Index < NumSlots(); ++Index)
			{
				const SBoxPanel::FSlot& CurChild = Children[Index];
				const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();

				if (ChildVisibility != EVisibility::Collapsed)
				{
					const FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();
					PositionSoFar -= ChildDesiredSize.Y + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
				}
			}

			for (int32 Index = MaxNumVisible; Index < Children.Num(); )
			{
				if (StaticCastSharedRef<SWidgetStackItem>(Children[Index].GetWidget())->bIsFinished)
				{
					Children.RemoveAt(Index);
				}
				else
				{
					++Index;
				}
			}
		}
	}

	class SWidgetStackItem : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SWidgetStackItem){}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			bIsFinished = false;

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.ColorAndOpacity(this, &SWidgetStackItem::GetColorAndOpacity)
				.Padding(0)
				[
					InArgs._Content.Widget
				]
			];
		}
		
		void FadeIn(float Time)
		{
			OpacityCurve = FCurveSequence(0.f, Time, ECurveEaseFunction::QuadOut);
			OpacityCurve.Play(AsShared());
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
		{
			if (!bIsFinished && OpacityCurve.IsAtStart() && OpacityCurve.IsInReverse())
			{
				bIsFinished = true;
			}
		}

		FLinearColor GetColorAndOpacity() const
		{
			return FLinearColor(1.f, 1.f, 1.f, OpacityCurve.GetLerp());
		}

		bool bIsFinished;
		FCurveSequence OpacityCurve;
	};

	FCurveSequence SlideCurve, SizeCurve;

	float StartSlideOffset;
	FVector2D StartSizeOffset;

	int32 MaxNumVisible;
};

class SWidgetStack;

/** Feedback context that overrides GWarn for import operations to prevent popup spam */
class SReimportFeedback : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReimportFeedback) : _ExpireDuration(3.f) {}

		SLATE_ARGUMENT(TWeakPtr<FReimportFeedbackContext>, FeedbackContext)
		SLATE_ARGUMENT(float, ExpireDuration)
		SLATE_EVENT(FSimpleDelegate, OnExpired)

		SLATE_EVENT(FSimpleDelegate, OnPauseClicked)
		SLATE_EVENT(FSimpleDelegate, OnAbortClicked)

	SLATE_END_ARGS()


	virtual FVector2D ComputeDesiredSize(float LayoutScale) const override
	{
		auto Size = SCompoundWidget::ComputeDesiredSize(LayoutScale);
		// The width is determined by the top row, plus some padding
		Size.X = TopRow->GetDesiredSize().X + 100;
		return Size;
	}
	/** Construct this widget */
	void Construct(const FArguments& InArgs)
	{
		ExpireDuration = InArgs._ExpireDuration;
		OnExpired = InArgs._OnExpired;
		FeedbackContext = InArgs._FeedbackContext;

		bPaused = false;
		bExpired = false;

		auto OpenMessageLog = []{
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			MessageLogModule.OpenMessageLog("AssetReimport");
		};

		ChildSlot
		[
			SNew(SBorder)
			.Padding(FMargin(10))
			.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(TopRow, SHorizontalBox)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ProcessingChanges", "Processing source file changes..."))
						.Font(FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontLight")))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(4,0,4,0))
					[
						SAssignNew(PauseButton, SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("PauseTooltip", "Temporarily pause processing of these source content files"))
						.OnClicked(this, &SReimportFeedback::OnPauseClicked, InArgs._OnPauseClicked)
						[
							SNew(SImage)
							.ColorAndOpacity(FLinearColor(.8f,.8f,.8f,1.f))
							.Image(this, &SReimportFeedback::GetPlayPauseBrush)
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(AbortButton, SButton)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.ToolTipText(LOCTEXT("AbortTooltip", "Permanently abort processing of these source content files"))
						.OnClicked(this, &SReimportFeedback::OnAbortClicked, InArgs._OnAbortClicked)
						[
							SNew(SImage)
							.ColorAndOpacity(FLinearColor(0.8f,0.8f,0.8f,1.f))
							.Image(FEditorStyle::GetBrush("GenericStop"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.Padding(FMargin(0, 1))
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(2)
					[
						SAssignNew(ProgressBar, SProgressBar)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent( this, &SReimportFeedback::GetProgressFraction )
						.BackgroundImage( FEditorStyle::GetBrush("ProgressBar.ThinBackground") )
						.FillImage( FEditorStyle::GetBrush("ProgressBar.ThinFill") )
					]
				]

				+ SVerticalBox::Slot()
				.Padding(FMargin(0, 5, 0, 0))
				.AutoHeight()
				[
					SAssignNew(WidgetStack, SWidgetStack, 3)
				]

				+ SVerticalBox::Slot()
				.Padding(FMargin(0, 5, 0, 0))
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SHyperlink)
					.Visibility(this, &SReimportFeedback::GetHyperlinkVisibility)
					.Text(LOCTEXT("OpenMessageLog", "Open message log"))
					.TextStyle(FCoreStyle::Get(), "SmallText")
					.OnNavigate_Lambda(OpenMessageLog)
				]
			]
		];
	}

	/** Add a widget to this feedback's widget stack */
	void Add(const TSharedRef<SWidget>& Widget)
	{
		WidgetStack->Add(Widget);
	}

	/** Disable input to this widget's dynamic content (except the message log hyperlink) */
	void Disable()
	{

		ExpireTimeout = DirectoryWatcher::FTimeLimit(ExpireDuration);

		WidgetStack->SetVisibility(EVisibility::HitTestInvisible);
		PauseButton->SetVisibility(EVisibility::Collapsed);
		AbortButton->SetVisibility(EVisibility::Collapsed);
		ProgressBar->SetVisibility(EVisibility::Collapsed);
	}

	/** Enable, if previously disabled */
	void Enable()
	{
		ExpireTimeout = DirectoryWatcher::FTimeLimit();

		bPaused = false;
		WidgetStack->SetVisibility(EVisibility::Visible);
		PauseButton->SetVisibility(EVisibility::Visible);
		AbortButton->SetVisibility(EVisibility::Visible);
		ProgressBar->SetVisibility(EVisibility::Visible);
	}

private:
	
	TOptional<float> GetProgressFraction() const
	{
		auto PinnedContext = FeedbackContext.Pin();
		if (PinnedContext.IsValid() && PinnedContext->GetScopeStack().Num() >= 0 )
		{
			return PinnedContext->GetScopeStack().GetProgressFraction(0);
		}
		return 1.f;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (bExpired)
		{
			return;
		}
		else if(ExpireTimeout.IsValid())
		{
			if (ExpireTimeout.Exceeded())
			{	
				OnExpired.ExecuteIfBound();
				bExpired = true;
			}
		}
	}

	/** Get the play/pause image */
	const FSlateBrush* GetPlayPauseBrush() const
	{
		return bPaused ? FEditorStyle::GetBrush("GenericPlay") : FEditorStyle::GetBrush("GenericPause");
	}

	/** Called when pause is clicked */
	FReply OnPauseClicked(FSimpleDelegate UserOnClicked)
	{
		bPaused = !bPaused;

		UserOnClicked.ExecuteIfBound();
		return FReply::Handled();
	}
	
	/** Called when abort is clicked */
	FReply OnAbortClicked(FSimpleDelegate UserOnClicked)
	{
		//Destroy();
		UserOnClicked.ExecuteIfBound();
		return FReply::Handled();
	}

	/** Get the visibility of the hyperlink to open the message log */
	EVisibility GetHyperlinkVisibility() const
	{
		return WidgetStack->NumSlots() != 0 ? EVisibility::Visible : EVisibility::Collapsed;
	}

private:

	/** The expire timeout used to fire OnExpired. Invalid when no timeout is set. */
	DirectoryWatcher::FTimeLimit ExpireTimeout;

	/** Amount of time to wait after this widget has been disabled before calling OnExpired */
	float ExpireDuration;

	/** Event that is called when this widget has been inactive and open for too long, and will fade out */
	FSimpleDelegate OnExpired;

	/** Whether we are paused and/or expired */
	bool bPaused, bExpired;

	/** The widget stack, displaying contextural information about the current state of the process */
	TSharedPtr<SWidgetStack> WidgetStack;
	TSharedPtr<SWidget> PauseButton, AbortButton, ProgressBar, TopRow;
	TWeakPtr<FReimportFeedbackContext> FeedbackContext;
};

FReimportFeedbackContext::FReimportFeedbackContext(const FSimpleDelegate& InOnPauseClicked, const FSimpleDelegate& InOnAbortClicked)
	: bSuppressSlowTaskMessages(false)
	, OnPauseClickedEvent(InOnPauseClicked)
	, OnAbortClickedEvent(InOnAbortClicked)
	, MessageLog("AssetReimport")
{
}

void FReimportFeedbackContext::Show(int32 TotalWork)
{
	// Important - we first destroy the old main task, then create a new one to ensure that they are (removed from/added to) the scope stack in the correct order
	MainTask.Reset();
	MainTask.Reset(new FScopedSlowTask(TotalWork, FText(), true, *this));

	if (NotificationContent.IsValid())
	{
		NotificationContent->Enable();
	}
	else
	{
		NotificationContent = SNew(SReimportFeedback)
			.FeedbackContext(AsShared())
			.OnExpired(this, &FReimportFeedbackContext::OnNotificationExpired)
			.OnPauseClicked(OnPauseClickedEvent)
			.OnAbortClicked(OnAbortClickedEvent);

		FNotificationInfo Info(AsShared());
		Info.bFireAndForget = false;

		Notification = FSlateNotificationManager::Get().AddNotification(Info);

		MessageLog.NewPage(FText::Format(LOCTEXT("MessageLogPageLabel", "Outstanding source content changes {0}"), FText::AsTime(FDateTime::Now())));
	}
}

void FReimportFeedbackContext::Hide()
{
	MainTask.Reset();

	if (Notification.IsValid())
	{
		NotificationContent->Disable();
		Notification->SetCompletionState(SNotificationItem::CS_Success);
	}
}

void FReimportFeedbackContext::OnNotificationExpired()
{
	if (Notification.IsValid())
	{
		MessageLog.Notify(FText(), EMessageSeverity::Error);
		Notification->Fadeout();

		NotificationContent = nullptr;
		Notification = nullptr;
	}
}

void FReimportFeedbackContext::AddMessage(EMessageSeverity::Type Severity, const FText& Message)
{
	MessageLog.Message(Severity, Message);
	AddWidget(SNew(STextBlock).Text(Message));
}

void FReimportFeedbackContext::AddWidget(const TSharedRef<SWidget>& Widget)
{
	if (NotificationContent.IsValid())
	{
		NotificationContent->Add(Widget);
	}
}

TSharedRef<SWidget> FReimportFeedbackContext::AsWidget()
{
	return NotificationContent.ToSharedRef();
}

void FReimportFeedbackContext::StartSlowTask(const FText& Task, bool bShowCancelButton)
{
	FFeedbackContext::StartSlowTask(Task, bShowCancelButton);

	if (NotificationContent.IsValid() && !bSuppressSlowTaskMessages && !Task.IsEmpty())
	{
		if (SlowTaskText.IsValid())
		{
			SlowTaskText->SetText(FText::Format(LOCTEXT("SlowTaskPattern_Default", "{0} (0%)"), Task));
		}
		else
		{
			NotificationContent->Add(SAssignNew(SlowTaskText, STextBlock).Text(Task));
		}
	}
}

void FReimportFeedbackContext::ProgressReported(const float TotalProgressInterp, FText DisplayMessage)
{
	if (SlowTaskText.IsValid())
	{
		SlowTaskText->SetText(FText::Format(LOCTEXT("SlowTaskPattern", "{0} ({1}%)"), DisplayMessage, FText::AsNumber(int(TotalProgressInterp * 100))));
	}
}

void FReimportFeedbackContext::FinalizeSlowTask()
{
	if (SlowTaskText.IsValid())
	{
		SlowTaskText->SetVisibility(EVisibility::Collapsed);
		SlowTaskText = nullptr;
	}

	FFeedbackContext::FinalizeSlowTask();
}

#undef LOCTEXT_NAMESPACE
