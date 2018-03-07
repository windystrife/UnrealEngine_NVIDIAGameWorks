// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerDebugVisualizer.h"
#include "CommonMovieSceneTools.h"
#include "EditorStyleSet.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/ArrangedChildren.h"

#define LOCTEXT_NAMESPACE "SSequencerDebugVisualizer"

void SSequencerDebugVisualizer::Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
{
	WeakSequencer = InSequencer;

	InSequencer->GetSequenceInstance().OnUpdated().AddSP(this, &SSequencerDebugVisualizer::Refresh);
	InSequencer->OnActivateSequence().AddSP(this, &SSequencerDebugVisualizer::OnSequenceActivated);

	SetClipping(EWidgetClipping::ClipToBounds);

	ViewRange = InArgs._ViewRange;

	Refresh();
}

void SSequencerDebugVisualizer::OnSequenceActivated(FMovieSceneSequenceIDRef)
{
	Refresh();
}

const FMovieSceneEvaluationTemplateInstance* SSequencerDebugVisualizer::GetTemplate() const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	return Sequencer.IsValid() ? Sequencer->GetSequenceInstance().GetInstance(FocusedSequenceID) : nullptr;
}

void SSequencerDebugVisualizer::Refresh()
{
	Children.Empty();

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FocusedSequenceID = Sequencer->GetFocusedTemplateID();

	const FMovieSceneEvaluationTemplateInstance* ActiveInstance = GetTemplate();
	if (!ActiveInstance)
	{
		return;
	}

	const FMovieSceneEvaluationField& EvaluationField = ActiveInstance->Template->EvaluationField;

	TArray<int32> SegmentComplexity;
	SegmentComplexity.Reserve(EvaluationField.Groups.Num());

	// Heatmap complexity
	float AverageComplexity = 0;
	int32 MaxComplexity = 0;

	for (const FMovieSceneEvaluationGroup& Group : EvaluationField.Groups)
	{
		int32 Complexity = 0;
		for (const FMovieSceneEvaluationGroupLUTIndex& LUTIndex : Group.LUTIndices)
		{
			// more groups is more complex
			Complexity += 1;
			// Add total init and eval complexity
			Complexity += LUTIndex.NumInitPtrs + LUTIndex.NumEvalPtrs;
		}

		SegmentComplexity.Add(Complexity);
		MaxComplexity = FMath::Max(MaxComplexity, Complexity);
		AverageComplexity += Complexity;
	}

	AverageComplexity /= SegmentComplexity.Num();

	static const FSlateBrush* SectionBackgroundBrush = FEditorStyle::GetBrush("Sequencer.Section.Background");
	static const FSlateBrush* SectionBackgroundTintBrush = FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint");

	for (int32 Index = 0; Index < EvaluationField.Ranges.Num(); ++Index)
	{
		TRange<float> Range = EvaluationField.Ranges[Index];

		const float Complexity = SegmentComplexity[Index];
		
		float Lerp = FMath::Clamp((Complexity - AverageComplexity) / (MaxComplexity - AverageComplexity), 0.f, 1.f) * 0.5f +
			FMath::Clamp(Complexity / AverageComplexity, 0.f, 1.f) * 0.5f;

		// Blend from blue (240deg) to red (0deg)
		FLinearColor ComplexityColor = FLinearColor(FMath::Lerp(240.f, 0.f, FMath::Clamp(Lerp, 0.f, 1.f)), 1.f, 1.f, 0.5f).HSVToLinearRGB();

		Children.Add(
			SNew(SSequencerDebugSlot, Index)
			.Visibility(this, &SSequencerDebugVisualizer::GetSegmentVisibility, Range)
			.ToolTip(
				SNew(SToolTip)
				[
					GetTooltipForSegment(Index)
				]
			)
			[
				SNew(SBorder)
				.BorderImage(SectionBackgroundBrush)
				.Padding(FMargin(1.f))
				[
					SNew(SBorder)
					.BorderImage(SectionBackgroundTintBrush)
					.BorderBackgroundColor(ComplexityColor)
					.ForegroundColor(FLinearColor::Black)
					[
						SNew(STextBlock)
						.Text(FText::AsNumber(Index))
					]
				]
			]
		);
	}
}

FVector2D SSequencerDebugVisualizer::ComputeDesiredSize(float) const
{
	// Note: X Size is not used
	FVector2D Size(100, 0.f);
	if (Children.Num())
	{
		for (int32 Index = 0; Index < Children.Num(); ++Index)
		{
			Size.Y = FMath::Max(Size.Y, Children[Index]->GetDesiredSize().Y);
		}
	}
	return Size;
}

FGeometry SSequencerDebugVisualizer::GetSegmentGeometry(const FGeometry& AllottedGeometry, const SSequencerDebugSlot& Slot, const FTimeToPixel& TimeToPixelConverter) const
{
	const FMovieSceneEvaluationTemplateInstance* ActiveInstance = GetTemplate();
	if (!ActiveInstance)
	{
		return AllottedGeometry.MakeChild(FVector2D(0,0), FVector2D(0,0));
	}

	TRange<float> SegmentRange = ActiveInstance->Template->EvaluationField.Ranges[Slot.GetSegmentIndex()];

	float PixelStartX = SegmentRange.GetLowerBound().IsOpen() ? 0.f : TimeToPixelConverter.TimeToPixel(SegmentRange.GetLowerBoundValue());
	float PixelEndX = SegmentRange.GetUpperBound().IsOpen() ? AllottedGeometry.GetDrawSize().X : TimeToPixelConverter.TimeToPixel(SegmentRange.GetUpperBoundValue());

	const float MinSectionWidth = 1.f;
	float SectionLength = FMath::Max(MinSectionWidth, PixelEndX - PixelStartX);

	return AllottedGeometry.MakeChild(
		FVector2D(PixelStartX, 0),
		FVector2D(SectionLength, Slot.GetDesiredSize().Y)
		);
}

EVisibility SSequencerDebugVisualizer::GetSegmentVisibility(TRange<float> Range) const
{
	return ViewRange.Get().Overlaps(Range) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SSequencerDebugVisualizer::GetTooltipForSegment(int32 SegmentIndex) const
{
	const FMovieSceneEvaluationTemplateInstance* ActiveInstance = GetTemplate();
	if (!ActiveInstance)
	{
		return SNullWidget::NullWidget;
	}

	const FMovieSceneEvaluationGroup& Group = ActiveInstance->Template->EvaluationField.Groups[SegmentIndex];

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	int32 NumInitEntities = 0;
	for (int32 Index = 0; Index < Group.LUTIndices.Num(); ++Index)
	{
		VerticalBox->AddSlot()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("EvalGroupFormat", "Evaluation Group {0}:"), Index))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("EvalTrackFormat", "{0} initialization steps, {1} evaluation steps"),
					FText::AsNumber(Group.LUTIndices[Index].NumInitPtrs),
					FText::AsNumber(Group.LUTIndices[Index].NumEvalPtrs)
				))
			]
		];
	}

	return VerticalBox;
}

void SSequencerDebugVisualizer::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	FTimeToPixel TimeToPixelConverter = FTimeToPixel(AllottedGeometry, ViewRange.Get());

	for (int32 WidgetIndex = 0; WidgetIndex < Children.Num(); ++WidgetIndex)
	{
		const TSharedRef<SSequencerDebugSlot>& Child = Children[WidgetIndex];

		EVisibility WidgetVisibility = Child->GetVisibility();
		if( ArrangedChildren.Accepts( WidgetVisibility ) )
		{
			FGeometry SegmentGeometry = GetSegmentGeometry(AllottedGeometry, *Child, TimeToPixelConverter);
			
			ArrangedChildren.AddWidget( 
				WidgetVisibility, 
				AllottedGeometry.MakeChild(Child, SegmentGeometry.Position, SegmentGeometry.GetDrawSize())
				);
		}
	}
}

#undef LOCTEXT_NAMESPACE
