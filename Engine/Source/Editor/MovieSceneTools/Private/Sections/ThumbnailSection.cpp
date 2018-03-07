// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/ThumbnailSection.h"
#include "Rendering/DrawElements.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Application/ThrottleManager.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "LevelEditorViewport.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IVREditorModule.h"


#define LOCTEXT_NAMESPACE "FThumbnailSection"


namespace ThumbnailSectionConstants
{
	const uint32 ThumbnailHeight = 90;
	const uint32 TrackWidth = 90;
	const float SectionGripSize = 4.0f;
}


/* FThumbnailSection structors
 *****************************************************************************/

FThumbnailSection::FThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, IViewportThumbnailClient* InViewportThumbanilClient, UMovieSceneSection& InSection)
	: Section(&InSection)
	, SequencerPtr(InSequencer)
	, ThumbnailCache(InThumbnailPool, InViewportThumbanilClient)
	, AdditionalDrawEffect(ESlateDrawEffect::None)
	, TimeSpace(ETimeSpace::Global)
{
	WhiteBrush = FEditorStyle::GetBrush("WhiteBrush");
	RedrawThumbnailDelegateHandle = GetMutableDefault<UMovieSceneUserThumbnailSettings>()->OnForceRedraw().AddRaw(this, &FThumbnailSection::RedrawThumbnails);
}


FThumbnailSection::FThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, ICustomThumbnailClient* InCustomThumbnailClient, UMovieSceneSection& InSection)
	: Section(&InSection)
	, SequencerPtr(InSequencer)
	, ThumbnailCache(InThumbnailPool, InCustomThumbnailClient)
	, AdditionalDrawEffect(ESlateDrawEffect::None)
	, TimeSpace(ETimeSpace::Global)
{
	WhiteBrush = FEditorStyle::GetBrush("WhiteBrush");
	RedrawThumbnailDelegateHandle = GetMutableDefault<UMovieSceneUserThumbnailSettings>()->OnForceRedraw().AddRaw(this, &FThumbnailSection::RedrawThumbnails);
}


FThumbnailSection::~FThumbnailSection()
{
	GetMutableDefault<UMovieSceneUserThumbnailSettings>()->OnForceRedraw().Remove(RedrawThumbnailDelegateHandle);
}


void FThumbnailSection::RedrawThumbnails()
{
	ThumbnailCache.ForceRedraw();
}


/* ISequencerSection interface
 *****************************************************************************/

TSharedRef<SWidget> FThumbnailSection::GenerateSectionWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(GetContentPadding())
		[
			SAssignNew(NameWidget, SInlineEditableTextBlock)
				.ToolTipText(CanRename() ? LOCTEXT("RenameThumbnail", "Click or hit F2 to rename") : FText::GetEmpty())
				.Text(this, &FThumbnailSection::HandleThumbnailTextBlockText)
				.ShadowOffset(FVector2D(1,1))
				.OnTextCommitted(this, &FThumbnailSection::HandleThumbnailTextBlockTextCommitted)
				.IsReadOnly(!CanRename())
		];
}

void FThumbnailSection::EnterRename()
{
	if (NameWidget.IsValid())
	{
		NameWidget->SetReadOnly(false);
		NameWidget->EnterEditingMode();
		NameWidget->SetReadOnly(!CanRename());
	}
}

void FThumbnailSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ViewMenuText", "View"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("ThumbnailsMenu", "Thumbnails"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& InMenuBuilder){

				TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

				FText CurrentTime = FText::FromString(Sequencer->GetZeroPadNumericTypeInterface()->ToString(Sequencer->GetLocalTime()));

				InMenuBuilder.BeginSection(NAME_None, LOCTEXT("ThisSectionText", "This Section"));
				{
					InMenuBuilder.AddMenuEntry(
						LOCTEXT("RefreshText", "Refresh"),
						LOCTEXT("RefreshTooltip", "Refresh this section's thumbnails"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FThumbnailSection::RedrawThumbnails))
					);
					InMenuBuilder.AddMenuEntry(
						FText::Format(LOCTEXT("SetSingleTime", "Set Thumbnail Time To {0}"), CurrentTime),
						LOCTEXT("SetSingleTimeTooltip", "Defines the time at which this section should draw its single thumbnail to the current cursor position"),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateLambda([=]{
								SetSingleTime(Sequencer->GetLocalTime());
								GetMutableDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails = true;
								GetMutableDefault<UMovieSceneUserThumbnailSettings>()->SaveConfig();
							})
						)
					);
				}
				InMenuBuilder.EndSection();

				InMenuBuilder.BeginSection(NAME_None, LOCTEXT("GlobalSettingsText", "Global Settings"));
				{
					InMenuBuilder.AddMenuEntry(
						LOCTEXT("RefreshAllText", "Refresh All"),
						LOCTEXT("RefreshAllTooltip", "Refresh all sections' thumbnails"),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([]{
							GetDefault<UMovieSceneUserThumbnailSettings>()->BroadcastRedrawThumbnails();
						}))
					);

					FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

					FDetailsViewArgs Args(false, false, false, FDetailsViewArgs::HideNameArea);
					TSharedRef<IDetailsView> DetailView = PropertyModule.CreateDetailView(Args);
					DetailView->SetObject(GetMutableDefault<UMovieSceneUserThumbnailSettings>());
					InMenuBuilder.AddWidget(DetailView, FText(), true);
				}
				InMenuBuilder.EndSection();
			})
		);
	}
	MenuBuilder.EndSection();
}


float FThumbnailSection::GetSectionGripSize() const
{
	return ThumbnailSectionConstants::SectionGripSize;
}


float FThumbnailSection::GetSectionHeight() const
{
	auto* Settings = GetDefault<UMovieSceneUserThumbnailSettings>();
	if (Settings->bDrawThumbnails)
	{
		return GetDefault<UMovieSceneUserThumbnailSettings>()->ThumbnailSize.Y;
	}
	else
	{
		return FEditorStyle::GetFontStyle("NormalFont").Size + 8.f;
	}
}


UMovieSceneSection* FThumbnailSection::GetSectionObject() 
{
	return Section;
}


FText FThumbnailSection::GetSectionTitle() const
{
	return FText::GetEmpty();
}


int32 FThumbnailSection::OnPaintSection( FSequencerSectionPainter& InPainter ) const
{
	if (!GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawThumbnails)
	{
		return InPainter.LayerId;
	}

	static const float SectionThumbnailPadding = 4.f;

	ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	int32 LayerId = InPainter.LayerId;

	const FGeometry& SectionGeometry = InPainter.SectionGeometry;

	// @todo Sequencer: Need a way to visualize the key here

	const TRange<float> VisibleRange = GetVisibleRange();
	const TRange<float> GenerationRange = GetTotalRange();

	const float TimePerPx = GenerationRange.Size<float>() / InPainter.SectionGeometry.GetLocalSize().X;
	
	FSlateRect ThumbnailClipRect = SectionGeometry.GetLayoutBoundingRect().InsetBy(FMargin(SectionThumbnailPadding, 0.f)).IntersectionWith(InPainter.SectionClippingRect);

	for (const TSharedPtr<FTrackEditorThumbnail>& Thumbnail : ThumbnailCache.GetThumbnails())
	{
		const float Fade = Thumbnail->bHasFinishedDrawing ? Thumbnail->GetFadeInCurve() : 1.f;
		FIntPoint ThumbnailSize = Thumbnail->GetSize();
		
		// Calculate the paint geometry for this thumbnail
		TOptional<float> SingleReferenceFrame = ThumbnailCache.GetSingleReferenceFrame();

		// Single thumbnails are always drawn at the start of the section, clamped to the visible range
		// Thumbnail sequences draw relative to their actual position in the sequence/section
		const int32 Offset = SingleReferenceFrame.IsSet() ?
			FMath::Max((VisibleRange.GetLowerBoundValue() - GenerationRange.GetLowerBoundValue()) / TimePerPx, 0.f) + SectionThumbnailPadding :
			(Thumbnail->GetTimeRange().GetLowerBoundValue() - GenerationRange.GetLowerBoundValue()) / TimePerPx;

		FPaintGeometry PaintGeometry = SectionGeometry.ToPaintGeometry(
			ThumbnailSize,
			FSlateLayoutTransform(
				SectionGeometry.Scale,
				FVector2D( Offset, (SectionGeometry.GetLocalSize().Y - ThumbnailSize.Y)*.5f)
			)
		);

		if (Fade <= 1.f)
		{
			if (IVREditorModule::Get().IsVREditorModeActive())
			{
				// In VR editor every widget is in the world and gamma corrected by the scene renderer.  Thumbnails will have already been gamma
				// corrected and so they need to be reversed
				DrawEffects |= ESlateDrawEffect::ReverseGamma;
			}
			else
			{
				DrawEffects |= ESlateDrawEffect::NoGamma;
			}

			FSlateClippingZone ClippingZone(ThumbnailClipRect);
			InPainter.DrawElements.PushClip(ClippingZone);

			FSlateDrawElement::MakeViewport(
				InPainter.DrawElements,
				LayerId,
				PaintGeometry,
				Thumbnail,
				DrawEffects | AdditionalDrawEffect,
				FLinearColor(1.f, 1.f, 1.f, 1.f - Fade)
			);

			InPainter.DrawElements.PopClip();
		}
	}

	return LayerId + 2;
}

TRange<float> FThumbnailSection::GetVisibleRange() const
{
	TRange<float> GlobalVisibleRange = SequencerPtr.Pin()->GetViewRange();
	if (TimeSpace == ETimeSpace::Global)
	{
		return GlobalVisibleRange;
	}
	
	return TRange<float>(
		GlobalVisibleRange.GetLowerBoundValue() - Section->GetStartTime(),
		GlobalVisibleRange.GetUpperBoundValue() - Section->GetStartTime()
	);
}

TRange<float> FThumbnailSection::GetTotalRange() const
{
	if (Section->IsInfinite())
	{
		return GetVisibleRange();
	}
	else if (TimeSpace == ETimeSpace::Global)
	{
		return Section->GetRange();
	}
	else
	{
		return TRange<float>(0.f, Section->GetRange().Size<float>());
	}
}

void FThumbnailSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FSlateThrottleManager::Get().IsAllowingExpensiveTasks() && GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawThumbnails)
	{
		const UMovieSceneUserThumbnailSettings* Settings = GetDefault<UMovieSceneUserThumbnailSettings>();

		FIntPoint AllocatedSize = AllottedGeometry.GetLocalSize().IntPoint();
		AllocatedSize.X = FMath::Max(AllocatedSize.X, 1);

		ThumbnailCache.Update(GetTotalRange(), GetVisibleRange(), AllocatedSize, Settings->ThumbnailSize, Settings->Quality, InCurrentTime);
	}
}


FViewportThumbnailSection::FViewportThumbnailSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection)
	: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
{
}


void FViewportThumbnailSection::PreDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport)
{
	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->EnterSilentMode();

		AActor* Camera = nullptr;
		FDelegateHandle Handle = Sequencer->OnCameraCut().AddLambda([&](UObject* InObject, bool){
			Camera = Cast<AActor>(InObject);
		});

		SavedPlaybackStatus = Sequencer->GetPlaybackStatus();
		Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Jumping);
		Sequencer->SetLocalTimeDirectly(Thumbnail.GetEvalPosition());
		Sequencer->ForceEvaluate();

		ViewportClient.SetActorLock(Camera);
		Sequencer->OnCameraCut().Remove(Handle);
	}
}


void FViewportThumbnailSection::PostDraw(FTrackEditorThumbnail& Thumbnail, FLevelEditorViewportClient& ViewportClient, FSceneViewport& SceneViewport)
{
	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer.IsValid())
	{
		Thumbnail.SetupFade(Sequencer->GetSequencerWidget());
		Sequencer->ExitSilentMode();
	}
}


#undef LOCTEXT_NAMESPACE
