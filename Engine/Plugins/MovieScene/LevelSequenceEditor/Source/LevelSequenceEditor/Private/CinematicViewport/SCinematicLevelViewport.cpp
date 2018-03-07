// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CinematicViewport/SCinematicLevelViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "LevelSequenceEditorToolkit.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "LevelSequenceEditorCommands.h"
#include "SLevelViewport.h"
#include "MovieScene.h"
#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "ISequencerKeyCollection.h"
#include "CinematicViewport/SCinematicTransportRange.h"
#include "CinematicViewport/FilmOverlays.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "CineCameraComponent.h"
#include "Math/UnitConversion.h"
#include "LevelEditorSequencerIntegration.h"


#define LOCTEXT_NAMESPACE "SCinematicLevelViewport"

template<typename T>
struct SNonThrottledSpinBox : SSpinBox<T>
{
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SSpinBox<T>::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			Reply.PreventThrottling();
		}
		return Reply;
	}
};

struct FTypeInterfaceProxy : INumericTypeInterface<float>
{
	TSharedPtr<INumericTypeInterface<float>> Impl;

	/** Convert the type to/from a string */
	virtual FString ToString(const float& Value) const override
	{
		if (Impl.IsValid())
		{
			return Impl->ToString(Value);
		}
		return FString();
	}

	virtual TOptional<float> FromString(const FString& InString, const float& InExistingValue) override
	{
		if (Impl.IsValid())
		{
			return Impl->FromString(InString, InExistingValue);
		}
		return TOptional<float>();
	}

	/** Check whether the typed character is valid */
	virtual bool IsCharacterValid(TCHAR InChar) const
	{
		if (Impl.IsValid())
		{
			return Impl->IsCharacterValid(InChar);
		}
		return false;
	}
};

FCinematicViewportClient::FCinematicViewportClient()
	: FLevelEditorViewportClient(nullptr)
{
	bDrawAxes = false;
	bIsRealtime = true;
	SetGameView(true);
	SetAllowCinematicPreview(true);
	bDisableInput = false;
}

class SPreArrangedBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnArrange, const FGeometry&);

	SLATE_BEGIN_ARGS(SPreArrangedBox){}
		SLATE_EVENT(FOnArrange, OnArrange)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnArrange = InArgs._OnArrange;
		ChildSlot
		[
			InArgs._Content.Widget
		];
	}

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
	{
		OnArrange.ExecuteIfBound(AllottedGeometry);
		SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
	}

private:

	FOnArrange OnArrange;
};

class SCinematicPreviewViewport : public SLevelViewport
{
public:
	virtual const FSlateBrush* OnGetViewportBorderBrush() const override { return nullptr; }
	virtual EVisibility GetCurrentLevelTextVisibility() const override { return EVisibility::Collapsed; }
	virtual EVisibility GetViewportControlsVisibility() const override { return EVisibility::Collapsed; }

	virtual TSharedPtr<SWidget> MakeViewportToolbar() { return nullptr; }

	TSharedPtr<SWidget> MakeExternalViewportToolbar() { return SLevelViewport::MakeViewportToolbar(); }

	FSlateColor GetBorderColorAndOpacity() const
	{
		return OnGetViewportBorderColorAndOpacity();
	}

	const FSlateBrush* GetBorderBrush() const
	{
		return SLevelViewport::OnGetViewportBorderBrush();
	}

	EVisibility GetBorderVisibility() const
	{
		const EVisibility ViewportContentVisibility = SLevelViewport::OnGetViewportContentVisibility();
		return ViewportContentVisibility == EVisibility::Visible ? EVisibility::HitTestInvisible : ViewportContentVisibility;
	}

private:
	bool bShowToolbar;
};


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SCinematicLevelViewport::Construct(const FArguments& InArgs)
{
	ParentLayout = InArgs._ParentLayout;
	LayoutName = InArgs._LayoutName;
	RevertToLayoutName = InArgs._RevertToLayoutName;

	ViewportClient = MakeShareable( new FCinematicViewportClient() );

	ViewportWidget = SNew(SCinematicPreviewViewport)
		.LevelEditorViewportClient(ViewportClient)
		.ParentLevelEditor(InArgs._ParentLevelEditor)
		.ParentLayout(ParentLayout.Pin())
		.ConfigKey(LayoutName.ToString())
		.Realtime(true);

	ViewportClient->SetViewportWidget(ViewportWidget);

	TypeInterfaceProxy = MakeShareable( new FTypeInterfaceProxy );

	FLevelSequenceEditorToolkit::OnOpened().AddSP(this, &SCinematicLevelViewport::OnEditorOpened);

	FLinearColor Gray(.3f, .3f, .3f, 1.f);

	TSharedRef<SFilmOverlayOptions> FilmOverlayOptions = SNew(SFilmOverlayOptions);

	DecoratedTransportControls = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(nullptr)
			.ForegroundColor(FEditorStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle()))
			[
				SNew(SNonThrottledSpinBox<float>)
				.TypeInterface(TypeInterfaceProxy)
				.Style(FEditorStyle::Get(), "Sequencer.HyperlinkSpinBox")
				.Font(FEditorStyle::GetFontStyle("Sequencer.FixedFont"))
				.OnValueCommitted(this, &SCinematicLevelViewport::OnTimeCommitted)
				.OnValueChanged(this, &SCinematicLevelViewport::SetTime)
				.OnEndSliderMovement(this, &SCinematicLevelViewport::SetTime)
				.MinValue(this, &SCinematicLevelViewport::GetMinTime)
				.MaxValue(this, &SCinematicLevelViewport::GetMaxTime)
				.Value(this, &SCinematicLevelViewport::GetTime)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SAssignNew(TransportControlsContainer, SBox)
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		];

	TSharedRef<SWidget> MainViewport = 	SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("BlackBrush"))
		.ForegroundColor(Gray)
		.Padding(0)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(5.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				[
					ViewportWidget->MakeExternalViewportToolbar().ToSharedRef()
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					FilmOverlayOptions
				]
			]

			+ SVerticalBox::Slot()
			[
				SNew(SPreArrangedBox)
				.OnArrange(this, &SCinematicLevelViewport::CacheDesiredViewportSize)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					[
						SNew(SSpacer)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(this, &SCinematicLevelViewport::GetDesiredViewportHeight)
						.WidthOverride(this, &SCinematicLevelViewport::GetDesiredViewportWidth)
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							[
								ViewportWidget.ToSharedRef()
							]

							+ SOverlay::Slot()
							[
								FilmOverlayOptions->GetFilmOverlayWidget()
							]
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(10.f, 0.f)
					[
						SAssignNew(ViewportControls, SBox)
						.Visibility(this, &SCinematicLevelViewport::GetControlsVisibility)
						.WidthOverride(this, &SCinematicLevelViewport::GetDesiredViewportWidth)
						.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Left)
								.AutoWidth()
								[
									SNew(STextBlock)
									.ColorAndOpacity(Gray)
									.Text_Lambda([=]{ return UIData.ShotName; })
								]

								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.AutoWidth()
								.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
								[
									SNew(STextBlock)
									.ColorAndOpacity(Gray)
									.Text_Lambda([=]{ return UIData.CameraName; })
								]
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoWidth()
							[
								SNew(STextBlock)
								.ColorAndOpacity(Gray)
								.Text_Lambda([=]{ return UIData.Filmback; })
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							[
								SNew(STextBlock)
								.Font(FEditorStyle::GetFontStyle("Sequencer.FixedFont"))
								.ColorAndOpacity(Gray)
								.Text_Lambda([=]{ return UIData.Frame; })
							]
						]
					]
				
					+ SVerticalBox::Slot()
					[
						SNew(SSpacer)
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding(5.f)
			.AutoHeight()
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex(this, &SCinematicLevelViewport::GetVisibleWidgetIndex)

				+ SWidgetSwitcher::Slot()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5.f)
					[
						SAssignNew(TransportRange, SCinematicTransportRange)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5.f, 0.f)
					[
						SAssignNew(TimeRangeContainer, SBox)
					]
				]

				+ SWidgetSwitcher::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.Padding(FMargin(5.f, 10.f))
					[
						SNew(STextBlock)
						.ColorAndOpacity(Gray)
						.Text(LOCTEXT("NoSequencerMessage", "No active Level Sequencer detected. Please edit a Level Sequence to enable full controls."))
					]
				]
			]
		];

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			MainViewport
		]

		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(ViewportWidget.Get(), &SCinematicPreviewViewport::GetBorderBrush)
			.BorderBackgroundColor(ViewportWidget.Get(), &SCinematicPreviewViewport::GetBorderColorAndOpacity)
			.Visibility(ViewportWidget.Get(), &SCinematicPreviewViewport::GetBorderVisibility)
			.Padding(0.0f)
			.ShowEffectWhenDisabled( false )
		]
	];

	FLevelSequenceEditorToolkit::IterateOpenToolkits([&](FLevelSequenceEditorToolkit& Toolkit){
		Setup(Toolkit);
		return false;
	});

	CommandList = MakeShareable( new FUICommandList );
	// Ensure the commands are registered
	FLevelSequenceEditorCommands::Register();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedPtr<SLevelViewport> SCinematicLevelViewport::GetLevelViewport() const
{
	return ViewportWidget;
}

int32 SCinematicLevelViewport::GetVisibleWidgetIndex() const
{
	return CurrentToolkit.IsValid() ? 0 : 1;
}

EVisibility SCinematicLevelViewport::GetControlsVisibility() const
{
	return CurrentToolkit.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SCinematicLevelViewport::GetMinTime() const
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		return Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange.GetLowerBoundValue();
	}
	return TOptional<float>();
}

TOptional<float> SCinematicLevelViewport::GetMaxTime() const
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		return Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetEditorData().WorkingRange.GetUpperBoundValue();
	}
	return TOptional<float>();
}

void SCinematicLevelViewport::OnTimeCommitted(float Value, ETextCommit::Type)
{
	SetTime(Value);
}

void SCinematicLevelViewport::SetTime(float Value)
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		Sequencer->SetLocalTime(Value);
	}
}

float SCinematicLevelViewport::GetTime() const
{
	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		return Sequencer->GetLocalTime();
	}
	return 0.f;
}

void SCinematicLevelViewport::CacheDesiredViewportSize(const FGeometry& AllottedGeometry)
{
	FVector2D AllowableSpace = AllottedGeometry.GetLocalSize();
	AllowableSpace.Y -= ViewportControls->GetDesiredSize().Y;

	if (ViewportClient->IsAspectRatioConstrained())
	{
		const float MinSize = FMath::TruncToFloat(FMath::Min(AllowableSpace.X / ViewportClient->AspectRatio, AllowableSpace.Y));
		DesiredViewportSize = FVector2D(FMath::TruncToFloat(ViewportClient->AspectRatio * MinSize), MinSize);
	}
	else
	{
		DesiredViewportSize = AllowableSpace;
	}
}

FOptionalSize SCinematicLevelViewport::GetDesiredViewportWidth() const
{
	return DesiredViewportSize.X;
}

FOptionalSize SCinematicLevelViewport::GetDesiredViewportHeight() const
{
	return DesiredViewportSize.Y;
}

FReply SCinematicLevelViewport::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) 
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	ISequencer* Sequencer = GetSequencer();
	if (Sequencer && Sequencer->GetCommandBindings()->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCinematicLevelViewport::Setup(FLevelSequenceEditorToolkit& NewToolkit)
{
	CurrentToolkit = StaticCastSharedRef<FLevelSequenceEditorToolkit>(NewToolkit.AsShared());

	NewToolkit.OnClosed().AddSP(this, &SCinematicLevelViewport::OnEditorClosed);

	ISequencer* Sequencer = GetSequencer();
	if (Sequencer)
	{
		TypeInterfaceProxy->Impl = Sequencer->GetZeroPadNumericTypeInterface();

		if (TransportRange.IsValid())
		{
			TransportRange->SetSequencer(Sequencer->AsShared());
		}

		if (TransportControlsContainer.IsValid())
		{
			TransportControlsContainer->SetContent(Sequencer->MakeTransportControls(true));
		}

		if (TimeRangeContainer.IsValid())
		{
			const bool bShowWorkingRange = true, bShowViewRange = false, bShowPlaybackRange = true;
			TimeRangeContainer->SetContent(Sequencer->MakeTimeRange(DecoratedTransportControls.ToSharedRef(), bShowWorkingRange, bShowViewRange, bShowPlaybackRange));
		}
	}
}

void SCinematicLevelViewport::CleanUp()
{
	TransportControlsContainer->SetContent(SNullWidget::NullWidget);
	TimeRangeContainer->SetContent(SNullWidget::NullWidget);

}

void SCinematicLevelViewport::OnEditorOpened(FLevelSequenceEditorToolkit& Toolkit)
{
	if (!CurrentToolkit.IsValid())
	{
		Setup(Toolkit);
	}
}

void SCinematicLevelViewport::OnEditorClosed()
{
	CleanUp();

	FLevelSequenceEditorToolkit* NewToolkit = nullptr;
	FLevelSequenceEditorToolkit::IterateOpenToolkits([&](FLevelSequenceEditorToolkit& Toolkit){
		NewToolkit = &Toolkit;
		return false;
	});

	if (NewToolkit)
	{
		Setup(*NewToolkit);
	}
}

ISequencer* SCinematicLevelViewport::GetSequencer() const
{
	TSharedPtr<FLevelSequenceEditorToolkit> Toolkit = CurrentToolkit.Pin();
	if (Toolkit.IsValid())
	{
		return Toolkit->GetSequencer().Get();
	}

	return nullptr;
}

void SCinematicLevelViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	ISequencer* Sequencer = GetSequencer();
	if (!Sequencer)
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	if (!Sequence)
	{
		return;
	}

	// Find either a cinematic shot track or a sub track
	UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Sequence->GetMovieScene()->FindMasterTrack(UMovieSceneCinematicShotTrack::StaticClass()));
	if (!SubTrack)
	{
		SubTrack = Cast<UMovieSceneSubTrack>(Sequence->GetMovieScene()->FindMasterTrack(UMovieSceneSubTrack::StaticClass()));
	}

	UMovieSceneSubSection* SubSection = nullptr;
	if (SubTrack)
	{
		const float CurrentTime = Sequencer->GetLocalTime();

		for (UMovieSceneSection* Section : SubTrack->GetAllSections())
		{
			if (Section->IsInfinite() || Section->IsTimeWithinSection(CurrentTime))
			{
				SubSection = CastChecked<UMovieSceneSubSection>(Section);
			}
		}
	}

	const float AbsoluteTime = Sequencer->GetLocalTime();

	FText TimeFormat = LOCTEXT("TimeFormat", "{0}");

	TSharedPtr<INumericTypeInterface<float>> ZeroPadTypeInterface = Sequencer->GetZeroPadNumericTypeInterface();

	if (SubSection)
	{
		float PlaybackRangeStart = 0.f;
		if (SubSection->GetSequence() != nullptr)
		{
			UMovieScene* ShotMovieScene = SubSection->GetSequence()->GetMovieScene();
			PlaybackRangeStart = ShotMovieScene->GetPlaybackRange().GetLowerBoundValue();
		}

		const float InnerOffset = (AbsoluteTime - SubSection->GetStartTime()) * SubSection->Parameters.TimeScale;
		const float AbsoluteShotPosition = PlaybackRangeStart + SubSection->Parameters.StartOffset + InnerOffset;

		UIData.Frame = FText::Format(
			TimeFormat,
			FText::FromString(ZeroPadTypeInterface->ToString(AbsoluteShotPosition))
		);

		UMovieSceneCinematicShotSection* CinematicShotSection = Cast<UMovieSceneCinematicShotSection>(SubSection);
		if (CinematicShotSection)
		{
			UIData.ShotName = CinematicShotSection->GetShotDisplayName();
		}
		else if (SubSection->GetSequence() != nullptr)
		{
			UIData.ShotName = SubSection->GetSequence()->GetDisplayName();
		}
	}
	else
	{
		UIData.Frame = FText::Format(
			TimeFormat,
			FText::FromString(ZeroPadTypeInterface->ToString(AbsoluteTime))
			);

		UIData.ShotName = Sequence->GetDisplayName();
	}

	TRange<float> EntireRange = Sequence->GetMovieScene()->GetEditorData().WorkingRange;

	UIData.MasterStartText = FText::Format(
		TimeFormat,
		FText::FromString(ZeroPadTypeInterface->ToString(EntireRange.GetLowerBoundValue()))
	);

	UIData.MasterEndText = FText::Format(
		TimeFormat,
		FText::FromString(ZeroPadTypeInterface->ToString(EntireRange.GetUpperBoundValue()))
	);

	UIData.CameraName = FText::GetEmpty();

	UCameraComponent* CameraComponent = ViewportClient->GetCameraComponentForView();
	if (CameraComponent)
	{
		AActor* OuterActor = Cast<AActor>(CameraComponent->GetOuter());
		if (OuterActor != nullptr)
		{
			UIData.CameraName = FText::FromString(OuterActor->GetActorLabel());
		}

		if (UCineCameraComponent* CineCam = Cast<UCineCameraComponent>(CameraComponent))
		{
			const float SensorWidth = CineCam->FilmbackSettings.SensorWidth;
			const float SensorHeight = CineCam->FilmbackSettings.SensorHeight;

			// Search presets for one that matches
			const FNamedFilmbackPreset* Preset = UCineCameraComponent::GetFilmbackPresets().FindByPredicate([&](const FNamedFilmbackPreset& InPreset){
				return InPreset.FilmbackSettings.SensorWidth == SensorWidth && InPreset.FilmbackSettings.SensorHeight == SensorHeight;
			});

			if (Preset)
			{
				UIData.Filmback = FText::FromString(Preset->Name);
			}
			else
			{
				FNumberFormattingOptions Opts = FNumberFormattingOptions().SetMaximumFractionalDigits(1);
				UIData.Filmback = FText::Format(
					LOCTEXT("CustomFilmbackFormat", "Custom ({0}mm x {1}mm)"),
					FText::AsNumber(SensorWidth, &Opts),
					FText::AsNumber(SensorHeight, &Opts)
				);
			}
		}
		else
		{
			// Just use the FoV
			UIData.Filmback = FText::FromString(Lex::ToString(FNumericUnit<float>(CameraComponent->FieldOfView, EUnit::Degrees)));
		}
	}
	else
	{
		UIData.Filmback = FText();
	}
}

#undef LOCTEXT_NAMESPACE
