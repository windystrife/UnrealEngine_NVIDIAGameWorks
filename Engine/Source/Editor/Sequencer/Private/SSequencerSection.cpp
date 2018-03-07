// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerSection.h"
#include "Rendering/DrawElements.h"
#include "EditorStyleSet.h"
#include "SequencerSelectionPreview.h"
#include "GroupedKeyArea.h"
#include "SequencerSettings.h"
#include "Editor.h"
#include "Sequencer.h"
#include "SequencerSectionPainter.h"
#include "CommonMovieSceneTools.h"
#include "ISequencerEditTool.h"
#include "ISequencerSection.h"
#include "ISequencerHotspot.h"
#include "SequencerHotspots.h"
#include "SOverlay.h"
#include "SequencerObjectBindingNode.h"
#include "FontCache.h"
#include "SlateApplication.h"

double SSequencerSection::SelectionThrobEndTime = 0;

/** A point on an easing curve used for rendering */
struct FEasingCurvePoint
{
	FEasingCurvePoint(FVector2D InLocation, const FLinearColor& InPointColor) : Location(InLocation), Color(InPointColor) {}

	/** The location of the point (x=time, y=easing value [0-1]) */
	FVector2D Location;
	/** The color of the point */
	FLinearColor Color;
};

struct FSequencerSectionPainterImpl : FSequencerSectionPainter
{
	FSequencerSectionPainterImpl(FSequencer& InSequencer, UMovieSceneSection& InSection, FSlateWindowElementList& _OutDrawElements, const FGeometry& InSectionGeometry, const SSequencerSection& InSectionWidget)
		: FSequencerSectionPainter(_OutDrawElements, InSectionGeometry, InSection)
		, Sequencer(InSequencer)
		, SectionWidget(InSectionWidget)
		, TimeToPixelConverter(InSection.IsInfinite() ?
			FTimeToPixel(SectionGeometry, Sequencer.GetViewRange()) :
			FTimeToPixel(SectionGeometry, TRange<float>(InSection.GetStartTime(), InSection.GetEndTime()))
			)
	{
		CalculateSelectionColor();

		const ISequencerEditTool* EditTool = InSequencer.GetEditTool();
		Hotspot = EditTool ? EditTool->GetDragHotspot() : nullptr;
		if (!Hotspot)
		{
			Hotspot = Sequencer.GetHotspot().Get();
		}
	}

	FLinearColor GetFinalTintColor(const FLinearColor& Tint) const
	{
		FLinearColor FinalTint = FSequencerSectionPainter::BlendColor(Tint);
		if (bIsHighlighted && !Section.IsInfinite())
		{
			float Lum = FinalTint.ComputeLuminance()* 0.2f;
			FinalTint = FinalTint + FLinearColor(Lum, Lum, Lum, 0.f);
		}
		return FinalTint;
	}

	virtual int32 PaintSectionBackground(const FLinearColor& Tint) override
	{
		const ESlateDrawEffect DrawEffects = bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		static const FSlateBrush* SectionBackgroundBrush = FEditorStyle::GetBrush("Sequencer.Section.Background");
		static const FSlateBrush* SectionBackgroundTintBrush = FEditorStyle::GetBrush("Sequencer.Section.BackgroundTint");
		static const FSlateBrush* SelectedSectionOverlay = FEditorStyle::GetBrush("Sequencer.Section.SelectedSectionOverlay");

		FGeometry InfiniteGeometry = SectionGeometry.MakeChild(FVector2D(-100.f, 0.f), FVector2D(SectionGeometry.GetLocalSize().X + 200.f, SectionGeometry.GetLocalSize().Y));

		FLinearColor FinalTint = GetFinalTintColor(Tint);

		FPaintGeometry PaintGeometry = Section.IsInfinite() ?
				InfiniteGeometry.ToPaintGeometry() :
				SectionGeometry.ToPaintGeometry();

		if (!Section.IsInfinite() && Sequencer.GetSettings()->ShouldShowPrePostRoll())
		{
			TOptional<FSlateClippingState> PreviousClipState = DrawElements.GetClippingState();
			DrawElements.PopClip();

			static const FSlateBrush* PreRollBrush = FEditorStyle::GetBrush("Sequencer.Section.PreRoll");
			float BrushHeight = 16.f, BrushWidth = 10.f;

			const float PreRollPx = TimeToPixelConverter.TimeToPixel(Section.GetStartTime() + Section.GetPreRollTime()) - TimeToPixelConverter.TimeToPixel(Section.GetStartTime());
			if (PreRollPx > 0)
			{
				const float RoundedPreRollPx = (int(PreRollPx / BrushWidth)+1) * BrushWidth;

				// Round up to the nearest BrushWidth size
				FGeometry PreRollArea = SectionGeometry.MakeChild(
					FVector2D(RoundedPreRollPx, BrushHeight),
					FSlateLayoutTransform(FVector2D(-PreRollPx, (SectionGeometry.GetLocalSize().Y - BrushHeight)*.5f))
					);

				FSlateDrawElement::MakeBox(
					DrawElements,
					LayerId,
					PreRollArea.ToPaintGeometry(),
					PreRollBrush,
					DrawEffects
				);
			}

			const float PostRollPx = TimeToPixelConverter.TimeToPixel(Section.GetEndTime() + Section.GetPostRollTime()) - TimeToPixelConverter.TimeToPixel(Section.GetEndTime());
			if (PostRollPx > 0)
			{
				const float RoundedPostRollPx = (int(PostRollPx / BrushWidth)+1) * BrushWidth;
				const float Difference = RoundedPostRollPx - PostRollPx;

				// Slate border brushes tile UVs along +ve X, so we round the arrows to a multiple of the brush width, and offset, to ensure we don't have a partial tile visible at the end
				FGeometry PostRollArea = SectionGeometry.MakeChild(
					FVector2D(RoundedPostRollPx, BrushHeight),
					FSlateLayoutTransform(FVector2D(SectionGeometry.GetLocalSize().X - Difference, (SectionGeometry.GetLocalSize().Y - BrushHeight)*.5f))
					);

				FSlateDrawElement::MakeBox(
					DrawElements,
					LayerId,
					PostRollArea.ToPaintGeometry(),
					PreRollBrush,
					DrawEffects
				);
			}

			if (PreviousClipState.IsSet())
			{
				DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
			}
		}


		{
			TOptional<FSlateClippingState> PreviousClipState = DrawElements.GetClippingState();
			DrawElements.PopClip();

			// Draw the section background
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				PaintGeometry,
				SectionBackgroundBrush,
				DrawEffects
			);

			if (PreviousClipState.IsSet())
			{
				DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
			}
		}

		// Draw the section background tint over the background
		FSlateDrawElement::MakeBox(
			DrawElements,
			LayerId,
			PaintGeometry,
			SectionBackgroundTintBrush,
			DrawEffects,
			FinalTint
		);

		// Draw underlapping sections
		DrawOverlaps(FinalTint);

		// Draw empty space
		DrawEmptySpace();

		// Draw the blend type text
		DrawBlendType();

		// Draw easing curves
		DrawEasing(FinalTint);

		// Draw the selection hash
		if (SelectionColor.IsSet())
		{
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				SectionGeometry.ToPaintGeometry(FVector2D(1, 1), SectionGeometry.GetLocalSize() - FVector2D(2,2)),
				SelectedSectionOverlay,
				DrawEffects,
				SelectionColor.GetValue().CopyWithNewOpacity(0.8f)
			);
		}

		return LayerId;
	}

	virtual const FTimeToPixel& GetTimeConverter() const
	{
		return TimeToPixelConverter;
	}

	void CalculateSelectionColor()
	{
		// Don't draw selected if infinite
		if (Section.IsInfinite())
		{
			return;
		}

		FSequencerSelection& Selection = Sequencer.GetSelection();
		FSequencerSelectionPreview& SelectionPreview = Sequencer.GetSelectionPreview();

		ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(&Section);

		if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
		{
			// Explicitly not selected in the preview selection
			return;
		}
		
		if (SelectionPreviewState == ESelectionPreviewState::Undefined && !Selection.IsSelected(&Section))
		{
			// No preview selection for this section, and it's not selected
			return;
		}

		SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

		// Use a muted selection color for selection previews
		if (SelectionPreviewState == ESelectionPreviewState::Selected)
		{
			SelectionColor.GetValue() = SelectionColor.GetValue().LinearRGBToHSV();
			SelectionColor.GetValue().R += 0.1f; // +10% hue
			SelectionColor.GetValue().G = 0.6f; // 60% saturation

			SelectionColor = SelectionColor.GetValue().HSVToLinearRGB();
		}
	}

	void DrawBlendType()
	{
		// Draw the blend type text if necessary
		UMovieSceneTrack* Track = GetTrack();
		if (!Track || Track->GetSupportedBlendTypes().Num() <= 1 || !Section.GetBlendType().IsValid() || !bIsHighlighted || Section.GetBlendType().Get() == EMovieSceneBlendType::Absolute)
		{
			return;
		}

		TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

		UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMovieSceneBlendType"), true);
		FText DisplayText = Enum->GetDisplayNameTextByValue((int64)Section.GetBlendType().Get());

		FSlateFontInfo FontInfo = FEditorStyle::GetFontStyle("Sequencer.Section.BackgroundText");
		FontInfo.Size = 24;

		auto GetFontHeight = [&]
		{
			return FontCache->GetMaxCharacterHeight(FontInfo, 1.f) + FontCache->GetBaseline(FontInfo, 1.f);
		};
		while( GetFontHeight() > SectionGeometry.Size.Y && FontInfo.Size > 11 )
		{
			FontInfo.Size = FMath::Max(FMath::FloorToInt(FontInfo.Size - 6.f), 11);
		}

		FVector2D TextOffset = Section.IsInfinite() ? FVector2D(0.f, -1.f) : FVector2D(1.f, -1.f);
		FVector2D BottomLeft = SectionGeometry.AbsoluteToLocal(SectionClippingRect.GetBottomLeft()) + TextOffset;

		FSlateDrawElement::MakeText(
			DrawElements,
			LayerId,
			SectionGeometry.MakeChild(
				FVector2D(SectionGeometry.Size.X, GetFontHeight()),
				FSlateLayoutTransform(BottomLeft - FVector2D(0.f, GetFontHeight()+1.f))
			).ToPaintGeometry(),
			DisplayText,
			FontInfo,
			bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			FLinearColor(1.f,1.f,1.f,.2f)
		);
	}

	float GetEaseHighlightAmount(FSectionHandle Handle, float EaseInInterp, float EaseOutInterp) const
	{
		if (!Hotspot)
		{
			return 0.f;
		}

		const bool bEaseInHandle = Hotspot->GetType() == ESequencerHotspot::EaseInHandle;
		const bool bEaseOutHandle = Hotspot->GetType() == ESequencerHotspot::EaseOutHandle;

		float EaseInScale = 0.f, EaseOutScale = 0.f;
		if (bEaseInHandle || bEaseOutHandle)
		{
			if (static_cast<const FSectionEasingHandleHotspot*>(Hotspot)->Section == Handle)
			{
				if (bEaseInHandle)
				{
					EaseInScale = 1.f;
				}
				else
				{
					EaseOutScale = 1.f;
				}
			}
		}
		else if (Hotspot->GetType() == ESequencerHotspot::EasingArea)
		{
			for (const FEasingAreaHandle& Easing : static_cast<const FSectionEasingAreaHotspot*>(Hotspot)->Easings)
			{
				if (Easing.Section == Handle)
				{
					if (Easing.EasingType == ESequencerEasingType::In)
					{
						EaseInScale = 1.f;
					}
					else
					{
						EaseOutScale = 1.f;
					}
				}
			}
		}

		const float TotalScale = EaseInScale + EaseOutScale;
		return TotalScale > 0.f ? EaseInInterp * (EaseInScale/TotalScale) + ((1.f-EaseOutInterp) * (EaseOutScale/TotalScale)) : 0.f;
	}

	FEasingCurvePoint MakeCurvePoint(FSectionHandle SectionHandle, float Time, const FLinearColor& FinalTint, const FLinearColor& EaseSelectionColor) const
	{
		TOptional<float> EaseInValue, EaseOutValue;
		float EaseInInterp = 0.f, EaseOutInterp = 1.f;
		SectionHandle.GetSectionObject()->EvaluateEasing(Time, EaseInValue, EaseOutValue, &EaseInInterp, &EaseOutInterp);

		return FEasingCurvePoint(
			FVector2D(Time, EaseInValue.Get(1.f) * EaseOutValue.Get(1.f)),
			FMath::Lerp(FinalTint, EaseSelectionColor, GetEaseHighlightAmount(SectionHandle, EaseInInterp, EaseOutInterp))
		);
	}

	/** Adds intermediate control points for the specified section's easing up to a given threshold */
	void RefineCurvePoints(FSectionHandle SectionHandle, const FLinearColor& FinalTint, const FLinearColor& EaseSelectionColor, TArray<FEasingCurvePoint>& InOutPoints)
	{
		static float GradientThreshold = .05f;
		static float ValueThreshold = .05f;

		float MinTimeSize = FMath::Max(0.0001f, TimeToPixelConverter.PixelToTime(2.5) - TimeToPixelConverter.PixelToTime(0));

		UMovieSceneSection* SectionObject = SectionHandle.GetSectionObject();

		for (int32 Index = 0; Index < InOutPoints.Num() - 1; ++Index)
		{
			const FEasingCurvePoint& Lower = InOutPoints[Index];
			const FEasingCurvePoint& Upper = InOutPoints[Index + 1];

			if ((Upper.Location.X - Lower.Location.X)*.5f > MinTimeSize)
			{
				float NewPointTime = (Upper.Location.X + Lower.Location.X)*.5f;
				float NewPointValue = SectionObject->EvaluateEasing(NewPointTime);

				// Check that the gradient is changing significantly
				float LinearValue = (Upper.Location.Y + Lower.Location.Y) * .5f;
				float PointGradient = NewPointValue - SectionObject->EvaluateEasing(FMath::Lerp(Lower.Location.X, NewPointTime, 0.9f));
				float OuterGradient = Upper.Location.Y - Lower.Location.Y;
				if (!FMath::IsNearlyEqual(OuterGradient, PointGradient, GradientThreshold) ||
					!FMath::IsNearlyEqual(LinearValue, NewPointValue, ValueThreshold))
				{
					// Add the point
					InOutPoints.Insert(MakeCurvePoint(SectionHandle, NewPointTime, FinalTint, EaseSelectionColor), Index+1);
					--Index;
				}
			}
		}
	}

	void DrawEasingForSegment(const FSequencerOverlapRange& Segment, const FGeometry& InnerSectionGeometry, const FLinearColor& FinalTint)
	{
		const float StartTimePixel = TimeToPixelConverter.TimeToPixel(Section.GetStartTime());
		const float RangeStartPixel = TimeToPixelConverter.TimeToPixel(Segment.Range.GetLowerBoundValue());
		const float RangeEndPixel = TimeToPixelConverter.TimeToPixel(Segment.Range.GetUpperBoundValue());
		const float RangeSizePixel = RangeEndPixel - RangeStartPixel;

		FGeometry RangeGeometry = InnerSectionGeometry.MakeChild(FVector2D(RangeSizePixel, InnerSectionGeometry.Size.Y), FSlateLayoutTransform(FVector2D(RangeStartPixel - StartTimePixel, 0.f)));
		if (!FSlateRect::DoRectanglesIntersect(RangeGeometry.GetLayoutBoundingRect(), ParentClippingRect))
		{
			return;
		}

		UMovieSceneTrack* Track = Section.GetTypedOuter<UMovieSceneTrack>();
		if (!Track)
		{
			return;
		}

		const FSlateBrush* MyBrush = FEditorStyle::Get().GetBrush("Sequencer.Timeline.EaseInOut");
		FSlateShaderResourceProxy *ResourceProxy = FSlateDataPayload::ResourceManager->GetShaderResource(*MyBrush);
		FSlateResourceHandle ResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*MyBrush);

		FVector2D AtlasOffset = ResourceProxy ? ResourceProxy->StartUV : FVector2D(0.f, 0.f);
		FVector2D AtlasUVSize = ResourceProxy ? ResourceProxy->SizeUV : FVector2D(1.f, 1.f);

		FSlateRenderTransform RenderTransform;

		const FVector2D Pos = RangeGeometry.GetAbsolutePosition();
		const FVector2D Size = RangeGeometry.GetLocalSize();

		FLinearColor EaseSelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());

		FColor FillColor(0,0,0,51);

		TArray<FEasingCurvePoint> CurvePoints;

		// Segment.Impls are already sorted bottom to top
		for (int32 CurveIndex = 0; CurveIndex < Segment.Sections.Num(); ++CurveIndex)
		{
			FSectionHandle Handle = Segment.Sections[CurveIndex];

			// Make the points for the curve
			CurvePoints.Reset(20);
			{
				CurvePoints.Add(MakeCurvePoint(Handle, Segment.Range.GetLowerBoundValue(), FinalTint, EaseSelectionColor));
				CurvePoints.Add(MakeCurvePoint(Handle, Segment.Range.GetUpperBoundValue(), FinalTint, EaseSelectionColor));

				// Refine the control points
				int32 LastNumPoints;
				do
				{
					LastNumPoints = CurvePoints.Num();
					RefineCurvePoints(Handle, FinalTint, EaseSelectionColor, CurvePoints);
				} while(LastNumPoints != CurvePoints.Num());
			}

			TArray<SlateIndex> Indices;
			TArray<FSlateVertex> Verts;
			TArray<FVector2D> BorderPoints;
			TArray<FLinearColor> BorderPointColors;

			Indices.Reserve(CurvePoints.Num()*6);
			Verts.Reserve(CurvePoints.Num()*2);

			for (const FEasingCurvePoint& Point : CurvePoints)
			{
				float U = (Point.Location.X - Segment.Range.GetLowerBoundValue()) / Segment.Range.Size<float>();

				// Add verts top->bottom
				FVector2D UV(U, 0.f);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, (Pos + UV*Size*RangeGeometry.Scale), AtlasOffset + UV*AtlasUVSize, FillColor));

				UV.Y = 1.f - Point.Location.Y;
				BorderPoints.Add(UV*Size);
				BorderPointColors.Add(Point.Color);
				Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RenderTransform, (Pos + UV*Size*RangeGeometry.Scale), AtlasOffset + FVector2D(UV.X, 0.5f)*AtlasUVSize, FillColor));

				if (Verts.Num() >= 4)
				{
					int32 Index0 = Verts.Num()-4, Index1 = Verts.Num()-3, Index2 = Verts.Num()-2, Index3 = Verts.Num()-1;
					Indices.Add(Index0);
					Indices.Add(Index1);
					Indices.Add(Index2);

					Indices.Add(Index1);
					Indices.Add(Index2);
					Indices.Add(Index3);
				}
			}

			if (Indices.Num())
			{
				FSlateDrawElement::MakeCustomVerts(
					DrawElements,
					LayerId,
					ResourceHandle,
					Verts,
					Indices,
					nullptr,
					0,
					0, ESlateDrawEffect::PreMultipliedAlpha);

				const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
				FSlateDrawElement::MakeLines(
					DrawElements,
					LayerId + 1,
					RangeGeometry.ToPaintGeometry(),
					BorderPoints,
					BorderPointColors,
					DrawEffects | ESlateDrawEffect::PreMultipliedAlpha,
					FLinearColor::White,
					true);
			}
		}

		++LayerId;
	}

	void DrawEasing(const FLinearColor& FinalTint)
	{
		if (!Section.GetBlendType().IsValid())
		{
			return;
		}

		// Compute easing geometry by insetting from the current section geometry by 1px
		FGeometry InnerSectionGeometry = SectionGeometry.MakeChild(SectionGeometry.Size - FVector2D(2.f, 2.f), FSlateLayoutTransform(FVector2D(1.f, 1.f)));
		for (const FSequencerOverlapRange& Segment : SectionWidget.UnderlappingEasingSegments)
		{
			DrawEasingForSegment(Segment, InnerSectionGeometry, FinalTint);
		}
	}

	void DrawOverlaps(const FLinearColor& FinalTint)
	{
		FGeometry InnerSectionGeometry = SectionGeometry.MakeChild(SectionGeometry.Size - FVector2D(2.f, 2.f), FSlateLayoutTransform(FVector2D(1.f, 1.f)));

		UMovieSceneTrack* Track = Section.GetTypedOuter<UMovieSceneTrack>();
		if (!Track)
		{
			return;
		}

		static const FSlateBrush* PinCusionBrush = FEditorStyle::GetBrush("Sequencer.Section.PinCusion");
		static const FSlateBrush* OverlapBorderBrush = FEditorStyle::GetBrush("Sequencer.Section.OverlapBorder");

		const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const float StartTimePixel = Section.IsInfinite() ? 0.f : TimeToPixelConverter.TimeToPixel(Section.GetStartTime());

		for (int32 SegmentIndex = 0; SegmentIndex < SectionWidget.UnderlappingSegments.Num(); ++SegmentIndex)
		{
			const FSequencerOverlapRange& Segment = SectionWidget.UnderlappingSegments[SegmentIndex];

			const float RangeStartPixel	= Segment.Range.GetLowerBound().IsOpen() ? 0.f							: TimeToPixelConverter.TimeToPixel(Segment.Range.GetLowerBoundValue());
			const float RangeEndPixel	= Segment.Range.GetUpperBound().IsOpen() ? InnerSectionGeometry.Size.X	: TimeToPixelConverter.TimeToPixel(Segment.Range.GetUpperBoundValue());
			const float RangeSizePixel	= RangeEndPixel - RangeStartPixel;

			FGeometry RangeGeometry = InnerSectionGeometry.MakeChild(FVector2D(RangeSizePixel, InnerSectionGeometry.Size.Y), FSlateLayoutTransform(FVector2D(RangeStartPixel - StartTimePixel, 0.f)));
			if (!FSlateRect::DoRectanglesIntersect(RangeGeometry.GetLayoutBoundingRect(), ParentClippingRect))
			{
				continue;
			}

			const FSequencerOverlapRange* NextSegment = SegmentIndex < SectionWidget.UnderlappingSegments.Num() - 1 ? &SectionWidget.UnderlappingSegments[SegmentIndex+1] : nullptr;
			const bool bDrawRightMostBound = !NextSegment || !Segment.Range.Adjoins(NextSegment->Range);

			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				RangeGeometry.ToPaintGeometry(),
				PinCusionBrush,
				DrawEffects,
				FinalTint
			);

			FPaintGeometry PaintGeometry = bDrawRightMostBound ? RangeGeometry.ToPaintGeometry() : RangeGeometry.ToPaintGeometry(FVector2D(RangeGeometry.Size) + FVector2D(10.f, 0.f), FSlateLayoutTransform(FVector2D::ZeroVector));
			FSlateDrawElement::MakeBox(
				DrawElements,
				LayerId,
				PaintGeometry,
				OverlapBorderBrush,
				DrawEffects,
				FLinearColor(1.f,1.f,1.f,.3f)
			);
		}
	}

	void DrawEmptySpace()
	{
		const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
		static const FSlateBrush* EmptySpaceBrush = FEditorStyle::GetBrush("Sequencer.Section.EmptySpace");

		// Attach contiguous regions together
		TOptional<FSlateRect> CurrentArea;

		for (const FSectionLayoutElement& Element : SectionWidget.Layout->GetElements())
		{
			const bool bIsEmptySpace = Element.GetDisplayNode()->GetType() == ESequencerNode::KeyArea && !Element.GetKeyArea().IsValid();
			const bool bExistingEmptySpace = CurrentArea.IsSet();

			if (bIsEmptySpace && bExistingEmptySpace && FMath::IsNearlyEqual(CurrentArea->Bottom, Element.GetOffset()))
			{
				CurrentArea->Bottom = Element.GetOffset() + Element.GetHeight();
				continue;
			}

			if (bExistingEmptySpace)
			{
				FPaintGeometry PaintGeom = SectionGeometry.MakeChild(CurrentArea->GetSize(), FSlateLayoutTransform(CurrentArea->GetTopLeft())).ToPaintGeometry();
				FSlateDrawElement::MakeBox(DrawElements, LayerId, PaintGeom, EmptySpaceBrush, DrawEffects);
				CurrentArea.Reset();
			}

			if (bIsEmptySpace)
			{
				CurrentArea = FSlateRect::FromPointAndExtent(FVector2D(0.f, Element.GetOffset()), FVector2D(SectionGeometry.Size.X, Element.GetHeight()));
			}
		}

		if (CurrentArea.IsSet())
		{
			FPaintGeometry PaintGeom = SectionGeometry.MakeChild(CurrentArea->GetSize(), FSlateLayoutTransform(CurrentArea->GetTopLeft())).ToPaintGeometry();
			FSlateDrawElement::MakeBox(DrawElements, LayerId, PaintGeom, EmptySpaceBrush, DrawEffects);
		}
	}

	TOptional<FLinearColor> SelectionColor;
	FSequencer& Sequencer;
	const SSequencerSection& SectionWidget;
	FTimeToPixel TimeToPixelConverter;
	const ISequencerHotspot* Hotspot;

	/** The clipping rectangle of the parent widget */
	FSlateRect ParentClippingRect;
};

void SSequencerSection::Construct( const FArguments& InArgs, TSharedRef<FSequencerTrackNode> SectionNode, int32 InSectionIndex )
{
	SectionIndex = InSectionIndex;
	ParentSectionArea = SectionNode;
	SectionInterface = SectionNode->GetSections()[InSectionIndex];
	Layout = FSectionLayout(*SectionNode, InSectionIndex);
	HandleOffsetPx = 0.f;

	ChildSlot
	[
		SectionInterface->GenerateSectionWidget()
	];
}


FVector2D SSequencerSection::ComputeDesiredSize(float) const
{
	return FVector2D(100, Layout->GetTotalHeight());
}


FGeometry SSequencerSection::GetKeyAreaGeometry( const FSectionLayoutElement& KeyArea, const FGeometry& SectionGeometry ) const
{
	// Compute the geometry for the key area
	return SectionGeometry.MakeChild( FVector2D( 0, KeyArea.GetOffset() ), FVector2D( SectionGeometry.GetLocalSize().X, KeyArea.GetHeight()  ) );
}


FSequencerSelectedKey SSequencerSection::GetKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry ) const
{
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();

	// Search every key area until we find the one under the mouse
	for (const FSectionLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

		// Compute the current key area geometry
		FGeometry KeyAreaGeometryPadded = GetKeyAreaGeometry( Element, AllottedGeometry );

		// Is the key area under the mouse
		if( KeyAreaGeometryPadded.IsUnderLocation( MousePosition ) )
		{
			FGeometry KeyAreaGeometry = GetKeyAreaGeometry( Element, SectionGeometry );
			FVector2D LocalSpaceMousePosition = KeyAreaGeometry.AbsoluteToLocal( MousePosition );

			FTimeToPixel TimeToPixelConverter = Section.IsInfinite()
				? FTimeToPixel( ParentGeometry, GetSequencer().GetViewRange())
				: FTimeToPixel( KeyAreaGeometry, TRange<float>( Section.GetStartTime(), Section.GetEndTime() ) );

			// Check each key until we find one under the mouse (if any)
			TArray<FKeyHandle> KeyHandles = KeyArea->GetUnsortedKeyHandles();
			for( int32 KeyIndex = 0; KeyIndex < KeyHandles.Num(); ++KeyIndex )
			{
				FKeyHandle KeyHandle = KeyHandles[KeyIndex];
				float KeyPosition = TimeToPixelConverter.TimeToPixel( KeyArea->GetKeyTime(KeyHandle) );
				FGeometry KeyGeometry = KeyAreaGeometry.MakeChild( 
					FVector2D( KeyPosition - FMath::CeilToFloat(SequencerSectionConstants::KeySize.X/2.0f), ((KeyAreaGeometry.GetLocalSize().Y*.5f)-(SequencerSectionConstants::KeySize.Y*.5f)) ),
					SequencerSectionConstants::KeySize
				);

				if( KeyGeometry.IsUnderLocation( MousePosition ) )
				{
					// The current key is under the mouse
					return FSequencerSelectedKey( Section, KeyArea, KeyHandle );
				}
			}

			// no key was selected in the current key area but the mouse is in the key area so it cannot possibly be in any other key area
			return FSequencerSelectedKey();
		}
	}

	// No key was selected in any key area
	return FSequencerSelectedKey();
}


FSequencerSelectedKey SSequencerSection::CreateKeyUnderMouse( const FVector2D& MousePosition, const FGeometry& AllottedGeometry, FSequencerSelectedKey InPressedKey )
{
	UMovieSceneSection& Section = *SectionInterface->GetSectionObject();
	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	// Search every key area until we find the one under the mouse
	for (const FSectionLayoutElement& Element : Layout->GetElements())
	{
		TSharedPtr<IKeyArea> KeyArea = Element.GetKeyArea();
		if (!KeyArea.IsValid())
		{
			continue;
		}

		// Compute the current key area geometry
		FGeometry KeyAreaGeometryPadded = GetKeyAreaGeometry( Element, AllottedGeometry );

		// Is the key area under the mouse
		if( KeyAreaGeometryPadded.IsUnderLocation( MousePosition ) )
		{
			FTimeToPixel TimeToPixelConverter = Section.IsInfinite() ? 			
				FTimeToPixel( ParentGeometry, GetSequencer().GetViewRange()) : 
				FTimeToPixel( SectionGeometry, TRange<float>( Section.GetStartTime(), Section.GetEndTime() ) );

			// If a key was pressed on, get the pressed on key's time to duplicate that key
			float KeyTime;

			if (InPressedKey.IsValid())
			{
				KeyTime = KeyArea->GetKeyTime(InPressedKey.KeyHandle.GetValue());
			}
			// Otherwise, use the time where the mouse is pressed
			else
			{
				FVector2D LocalSpaceMousePosition = SectionGeometry.AbsoluteToLocal( MousePosition );
				KeyTime = TimeToPixelConverter.PixelToTime(LocalSpaceMousePosition.X);
			}

			if (Section.TryModify())
			{
				// If the pressed key exists, offset the new key and look for it in the newly laid out key areas
				if (InPressedKey.IsValid())
				{
					// Offset by 1 pixel worth of time
					const float TimeFuzz = (GetSequencer().GetViewRange().GetUpperBoundValue() - GetSequencer().GetViewRange().GetLowerBoundValue()) / ParentGeometry.GetLocalSize().X;

					TArray<FKeyHandle> KeyHandles = KeyArea->AddKeyUnique(KeyTime+TimeFuzz, GetSequencer().GetKeyInterpolation(), KeyTime);

					Layout = FSectionLayout(*ParentSectionArea, SectionIndex);

					// Look specifically for the key with the offset key time
					for (const FSectionLayoutElement& NewElement : Layout->GetElements())
					{
						TSharedPtr<IKeyArea> NewKeyArea = NewElement.GetKeyArea();
						if (!NewKeyArea.IsValid())
						{
							continue;
						}

						for (auto KeyHandle : KeyHandles)
						{
							for (auto UnsortedKeyHandle : NewKeyArea->GetUnsortedKeyHandles())
							{
								if (FMath::IsNearlyEqual(KeyTime+TimeFuzz, NewKeyArea->GetKeyTime(UnsortedKeyHandle), KINDA_SMALL_NUMBER))
								{
									return FSequencerSelectedKey(Section, NewKeyArea, UnsortedKeyHandle);
								}
							}
						}
					}
				}
				else
				{
					KeyArea->AddKeyUnique(KeyTime, GetSequencer().GetKeyInterpolation(), KeyTime);
					Layout = FSectionLayout(*ParentSectionArea, SectionIndex);
						
					return GetKeyUnderMouse(MousePosition, AllottedGeometry);
				}
			}
		}
	}

	return FSequencerSelectedKey();
}


bool SSequencerSection::CheckForEasingHandleInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	FTimeToPixel TimeToPixelConverter(MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface), ThisSection->IsInfinite() ? GetSequencer().GetViewRange() : ThisSection->GetRange());

	const float MouseTime = TimeToPixelConverter.PixelToTime(SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);
	// We intentionally give the handles a little more hit-test area than is visible as they are quite small
	const float HalfHandleSizeX = TimeToPixelConverter.PixelToTime(8.f) - TimeToPixelConverter.PixelToTime(0.f);

	// Now test individual easing handles if we're at the correct vertical position
	float LocalMouseY = SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).Y;
	if (LocalMouseY < 0.f || LocalMouseY > 5.f)
	{
		return false;
	}

	// Gather all underlapping sections
	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> EasingSection =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection* EasingSectionObj = EasingSection->GetSectionObject();
		if (EasingSectionObj->IsInfinite())
		{
			continue;
		}

		TRange<float> EaseInRange = EasingSectionObj->GetEaseInRange();
		if (FMath::IsNearlyEqual(MouseTime, EaseInRange.IsEmpty() ? EasingSectionObj->GetStartTime() : EaseInRange.GetUpperBoundValue(), HalfHandleSizeX))
		{
			GetSequencer().SetHotspot(MakeShared<FSectionEasingHandleHotspot>(ESequencerEasingType::In, Handle));
			return true;
		}

		TRange<float> EaseOutRange = EasingSectionObj->GetEaseOutRange();
		if (FMath::IsNearlyEqual(MouseTime, EaseOutRange.IsEmpty() ? EasingSectionObj->GetEndTime() :EaseOutRange.GetLowerBoundValue(), HalfHandleSizeX))
		{
			GetSequencer().SetHotspot(MakeShared<FSectionEasingHandleHotspot>(ESequencerEasingType::Out, Handle));
			return true;
		}
	}

	return false;
}


bool SSequencerSection::CheckForEdgeInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	FGeometry SectionGeometryWithoutHandles = MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface);
	FTimeToPixel TimeToPixelConverter(SectionGeometryWithoutHandles, ThisSection->IsInfinite() ? GetSequencer().GetViewRange() : ThisSection->GetRange());
	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> UnderlappingSection =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection* UnderlappingSectionObj = UnderlappingSection->GetSectionObject();
		if (!UnderlappingSection->SectionIsResizable() || UnderlappingSectionObj->IsInfinite())
		{
			continue;
		}

		const float ThisHandleOffset = UnderlappingSectionObj == ThisSection ? HandleOffsetPx : 0.f;
		FVector2D GripSize( UnderlappingSection->GetSectionGripSize(), SectionGeometry.Size.Y );

		// Make areas to the left and right of the geometry.  We will use these areas to determine if someone dragged the left or right edge of a section
		FGeometry SectionRectLeft = SectionGeometryWithoutHandles.MakeChild(
			FVector2D( TimeToPixelConverter.TimeToPixel(UnderlappingSectionObj->GetStartTime()) - ThisHandleOffset, 0.f ),
			GripSize
		);

		FGeometry SectionRectRight = SectionGeometryWithoutHandles.MakeChild(
			FVector2D( TimeToPixelConverter.TimeToPixel(UnderlappingSectionObj->GetEndTime()) - UnderlappingSection->GetSectionGripSize() + ThisHandleOffset, 0 ), 
			GripSize
		);

		if( SectionRectLeft.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
		{
			GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Left, Handle)) );
			return true;
		}
		else if( SectionRectRight.IsUnderLocation( MouseEvent.GetScreenSpacePosition() ) )
		{
			GetSequencer().SetHotspot(MakeShareable( new FSectionResizeHotspot(FSectionResizeHotspot::Right, Handle)) );
			return true;
		}
	}
	return false;
}

bool SSequencerSection::CheckForEasingAreaInteraction( const FPointerEvent& MouseEvent, const FGeometry& SectionGeometry )
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return false;
	}

	FTimeToPixel TimeToPixelConverter(MakeSectionGeometryWithoutHandles(SectionGeometry, SectionInterface), ThisSection->IsInfinite() ? GetSequencer().GetViewRange() : ThisSection->GetRange());

	const float MouseTime = TimeToPixelConverter.PixelToTime(SectionGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X);

	// First off, set the hotspot to an easing area if necessary
	for (const FSequencerOverlapRange& Segment : UnderlappingEasingSegments)
	{
		if (!Segment.Range.Contains(MouseTime))
		{
			continue;
		}

		TArray<FEasingAreaHandle> EasingAreas;
		for (FSectionHandle Handle : Segment.Sections)
		{
			UMovieSceneSection* Section = Handle.GetSectionObject();
			if (Section->GetEaseInRange().Contains(MouseTime))
			{
				EasingAreas.Add(FEasingAreaHandle{ Handle, ESequencerEasingType::In });
			}
			if (Section->GetEaseOutRange().Contains(MouseTime))
			{
				EasingAreas.Add(FEasingAreaHandle{ Handle, ESequencerEasingType::Out });
			}
		}

		if (EasingAreas.Num())
		{
			GetSequencer().SetHotspot(MakeShared<FSectionEasingAreaHotspot>(EasingAreas, FSectionHandle(ParentSectionArea, SectionIndex)));
			return true;
		}
	}
	return false;
}

FSequencer& SSequencerSection::GetSequencer() const
{
	return ParentSectionArea->GetSequencer();
}


int32 SSequencerSection::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	const ISequencerEditTool* EditTool = GetSequencer().GetEditTool();
	const ISequencerHotspot* Hotspot = EditTool ? EditTool->GetDragHotspot() : nullptr;
	if (!Hotspot)
	{
		Hotspot = GetSequencer().GetHotspot().Get();
	}

	const bool bEnabled = bParentEnabled && SectionObject.IsActive();
	const bool bLocked = SectionObject.IsLocked();
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );

	FSequencerSectionPainterImpl Painter(ParentSectionArea->GetSequencer(), SectionObject, OutDrawElements, SectionGeometry, *this);

	FGeometry PaintSpaceParentGeometry = ParentGeometry;
	PaintSpaceParentGeometry.AppendTransform(FSlateLayoutTransform(Inverse(Args.GetWindowToDesktopTransform())));

	Painter.ParentClippingRect = PaintSpaceParentGeometry.GetLayoutBoundingRect();

	// Clip vertically
	Painter.ParentClippingRect.Top = FMath::Max(Painter.ParentClippingRect.Top, MyCullingRect.Top);
	Painter.ParentClippingRect.Bottom = FMath::Min(Painter.ParentClippingRect.Bottom, MyCullingRect.Bottom);

	Painter.SectionClippingRect = Painter.SectionGeometry.GetLayoutBoundingRect().InsetBy(FMargin(1.f)).IntersectionWith(Painter.ParentClippingRect);

	Painter.LayerId = LayerId;
	Painter.bParentEnabled = bEnabled;
	Painter.bIsHighlighted = IsSectionHighlighted(FSectionHandle(ParentSectionArea, SectionIndex), Hotspot);

	FSlateClippingZone ClippingZone(Painter.SectionClippingRect);
	OutDrawElements.PushClip(ClippingZone);

	// Ask the interface to draw the section
	LayerId = SectionInterface->OnPaintSection(Painter);

	LayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );

	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SequencerSectionConstants::SelectionColorName).GetColor(FWidgetStyle());
	DrawSectionHandles(AllottedGeometry, OutDrawElements, LayerId, DrawEffects, SelectionColor, Hotspot);

	Painter.LayerId = LayerId;
	PaintEasingHandles( Painter, SelectionColor, Hotspot );
	PaintKeys( Painter, InWidgetStyle );

	LayerId = Painter.LayerId;
	if (bLocked)
	{
		static const FName SelectionBorder("Sequencer.Section.LockedBorder");

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush(SelectionBorder),
			DrawEffects,
			FLinearColor::Red
		);
	}

	// Section name with drop shadow
	FText SectionTitle = SectionInterface->GetSectionTitle();
	FMargin ContentPadding = SectionInterface->GetContentPadding();

	const float EaseInAmount = SectionObject.Easing.GetEaseInTime();
	if (EaseInAmount > 0.f)
	{
		ContentPadding.Left += Painter.GetTimeConverter().TimeToPixel(EaseInAmount) - Painter.GetTimeConverter().TimeToPixel(0.f);
	}

	if (!SectionTitle.IsEmpty())
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			Painter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(ContentPadding.Left + 1, ContentPadding.Top + 1)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			DrawEffects,
			FLinearColor(0,0,0,.5f)
		);

		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId,
			Painter.SectionGeometry.ToOffsetPaintGeometry(FVector2D(ContentPadding.Left, ContentPadding.Top)),
			SectionTitle,
			FEditorStyle::GetFontStyle("NormalFont"),
			DrawEffects,
			FColor(200, 200, 200)
		);
	}

	OutDrawElements.PopClip();
	return LayerId + 1;
}


void SSequencerSection::PaintKeys( FSequencerSectionPainter& InPainter, const FWidgetStyle& InWidgetStyle ) const
{
	static const FName HighlightBrushName("Sequencer.AnimationOutliner.DefaultBorder");

	static const FName BackgroundBrushName("Sequencer.Section.BackgroundTint");
	static const FName CircleKeyBrushName("Sequencer.KeyCircle");
	static const FName DiamondKeyBrushName("Sequencer.KeyDiamond");
	static const FName SquareKeyBrushName("Sequencer.KeySquare");
	static const FName TriangleKeyBrushName("Sequencer.KeyTriangle");
	static const FName StripeOverlayBrushName("Sequencer.Section.StripeOverlay");

	static const FName SelectionColorName("SelectionColor");
	static const FName SelectionInactiveColorName("SelectionColorInactive");
	static const FName SelectionColorPressedName("SelectionColor_Pressed");

	static const float BrushBorderWidth = 2.0f;

	const FLinearColor PressedKeyColor = FEditorStyle::GetSlateColor(SelectionColorPressedName).GetColor( InWidgetStyle );
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor( InWidgetStyle );
	FLinearColor SelectedKeyColor = SelectionColor;
	FSequencer& Sequencer = ParentSectionArea->GetSequencer();
	TSharedPtr<ISequencerHotspot> Hotspot = Sequencer.GetHotspot();

	// get hovered key
	FSequencerSelectedKey HoveredKey;

	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	auto& Selection = Sequencer.GetSelection();
	auto& SelectionPreview = Sequencer.GetSelectionPreview();

	const float ThrobScaleValue = GetSelectionThrobValue();

	// draw all keys in each key area
	UMovieSceneSection& SectionObject = *SectionInterface->GetSectionObject();

	const FSlateBrush* HighlightBrush = FEditorStyle::GetBrush(HighlightBrushName);
	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush(BackgroundBrushName);
	const FSlateBrush* StripeOverlayBrush = FEditorStyle::GetBrush(StripeOverlayBrushName);
	const FSlateBrush* CircleKeyBrush = FEditorStyle::GetBrush(CircleKeyBrushName);
	const FSlateBrush* DiamondKeyBrush = FEditorStyle::GetBrush(DiamondKeyBrushName);
	const FSlateBrush* SquareKeyBrush = FEditorStyle::GetBrush(SquareKeyBrushName);
	const FSlateBrush* TriangleKeyBrush = FEditorStyle::GetBrush(TriangleKeyBrushName);

	const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	for (const FSectionLayoutElement& LayoutElement : Layout->GetElements())
	{
		// get key handles
		TSharedPtr<IKeyArea> KeyArea = LayoutElement.GetKeyArea();

		FGeometry KeyAreaGeometry = GetKeyAreaGeometry(LayoutElement, InPainter.SectionGeometry);

		TOptional<FLinearColor> KeyAreaColor = KeyArea.IsValid() ? KeyArea->GetColor() : TOptional<FLinearColor>();

		// draw a box for the key area 
		if (KeyAreaColor.IsSet() && Sequencer.GetSettings()->GetShowChannelColors())
		{
			static float BoxThickness = 5.f;
			FVector2D KeyAreaSize = KeyAreaGeometry.GetLocalSize();
			FSlateDrawElement::MakeBox( 
				InPainter.DrawElements,
				InPainter.LayerId,
				KeyAreaGeometry.ToPaintGeometry(FVector2D(KeyAreaSize.X, BoxThickness), FSlateLayoutTransform(FVector2D(0.f, KeyAreaSize.Y*.5f - BoxThickness*.5f))),
				StripeOverlayBrush,
				DrawEffects,
				KeyAreaColor.GetValue()
			); 
		}

		if (LayoutElement.GetDisplayNode().IsValid())
		{
			FLinearColor HighlightColor;
			bool bDrawHighlight = false;
			if (Sequencer.GetSelection().NodeHasSelectedKeysOrSections(LayoutElement.GetDisplayNode().ToSharedRef()))
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);
			}
			else if (LayoutElement.GetDisplayNode()->IsHovered())
			{
				bDrawHighlight = true;
				HighlightColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.05f);
			}

			if (bDrawHighlight)
			{
				FSlateDrawElement::MakeBox(
					InPainter.DrawElements,
					InPainter.LayerId,
					KeyAreaGeometry.ToPaintGeometry(),
					HighlightBrush,
					DrawEffects,
					HighlightColor
				);
			}
		}

		if (Selection.IsSelected(LayoutElement.GetDisplayNode().ToSharedRef()))
		{
			static const FName SelectedTrackTint("Sequencer.Section.SelectedTrackTint");

			FLinearColor KeyAreaOutlineColor = SelectionColor;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId,
				KeyAreaGeometry.ToPaintGeometry(),
				FEditorStyle::GetBrush(SelectedTrackTint),
				DrawEffects,
				KeyAreaOutlineColor
			);
		}

		// Can't do any of the rest if there are no keys
		if (!KeyArea.IsValid())
		{
			continue;
		}

		// Gather keys for a region larger thean the view range to ensure we draw keys that are only just offscreen.
		TRange<float> PaddedViewRange;
		{
			const float KeyWidthAsTime = TimeToPixelConverter.PixelToTime(SequencerSectionConstants::KeySize.X) - TimeToPixelConverter.PixelToTime(0);
			const TRange<float> ViewRange = GetSequencer().GetViewRange();

			PaddedViewRange = TRange<float>(ViewRange.GetLowerBoundValue() - KeyWidthAsTime, ViewRange.GetUpperBoundValue() + KeyWidthAsTime);
		}

		const FSequencerCachedKeys& CachedKeys = CachedKeyAreaPositions.FindChecked(KeyArea);
		TArrayView<const FSequencerCachedKey> KeysInRange = CachedKeys.GetKeysInRange(PaddedViewRange);
		if (!KeysInRange.Num())
		{
			continue;
		}

		const int32 KeyLayer = InPainter.LayerId;

		TOptional<FSlateClippingState> PreviousClipState = InPainter.DrawElements.GetClippingState();
		InPainter.DrawElements.PopClip();

		static float PixelOverlapThreshold = 3.f;

		for (int32 KeyIndex = 0; KeyIndex < KeysInRange.Num(); ++KeyIndex)
		{
			FKeyHandle KeyHandle = KeysInRange[KeyIndex].Handle;
			const float KeyTime = KeysInRange[KeyIndex].Time;
			const float KeyPosition = TimeToPixelConverter.TimeToPixel(KeyTime);

			// Count the number of overlapping keys
			int32 NumOverlaps = 0;
			while (KeyIndex + 1 < KeysInRange.Num() && FMath::IsNearlyEqual(TimeToPixelConverter.TimeToPixel(KeysInRange[KeyIndex+1].Time), KeyPosition, PixelOverlapThreshold))
			{
				++KeyIndex;
				++NumOverlaps;
			}

			// omit keys which would not be visible
			if( !SectionObject.IsTimeWithinSection(KeyTime))
			{
				continue;
			}

			// determine the key's brush & color
			const FSlateBrush* KeyBrush;
			FLinearColor KeyColor;
			FVector2D FillOffset(0.0f, 0.0f);

			switch (KeyArea->GetKeyInterpMode(KeyHandle))
			{
			case RCIM_Linear:
				KeyBrush = TriangleKeyBrush;
				KeyColor = FLinearColor(0.0f, 0.617f, 0.449f, 1.0f); // blueish green
				FillOffset = FVector2D(0.0f, 1.0f);
				break;

			case RCIM_Constant:
				KeyBrush = SquareKeyBrush;
				KeyColor = FLinearColor(0.0f, 0.445f, 0.695f, 1.0f); // blue
				break;

			case RCIM_Cubic:
				KeyBrush = CircleKeyBrush;

				switch (KeyArea->GetKeyTangentMode(KeyHandle))
				{
				case RCTM_Auto:
					KeyColor = FLinearColor(0.972f, 0.2f, 0.2f, 1.0f); // vermillion
					break;

				case RCTM_Break:
					KeyColor = FLinearColor(0.336f, 0.703f, 0.5f, 0.91f); // sky blue
					break;

				case RCTM_User:
					KeyColor = FLinearColor(0.797f, 0.473f, 0.5f, 0.652f); // reddish purple
					break;

				default:
					KeyColor = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f); // light gray
				}
				break;

			default:
				KeyBrush = DiamondKeyBrush;
				KeyColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f); // white
				break;
			}

			// allow group & section overrides
			const FSlateBrush* OverrideBrush = nullptr;

			if (LayoutElement.GetType() == FSectionLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				OverrideBrush = Group->GetBrush(KeyHandle);
			}
			else
			{
				OverrideBrush = SectionInterface->GetKeyBrush(KeyHandle);
			}

			if (OverrideBrush != nullptr)
			{
				KeyBrush = OverrideBrush;
				FillOffset = SectionInterface->GetKeyBrushOrigin(KeyHandle);
			}

			// determine draw colors based on hover, selection, etc.
			FLinearColor BorderColor;
			FLinearColor FillColor;

			FSequencerSelectedKey TestKey(SectionObject, KeyArea, KeyHandle);
			ESelectionPreviewState SelectionPreviewState = SelectionPreview.GetSelectionState(TestKey);
			bool bSelected = Selection.IsSelected(TestKey);

			if (SelectionPreviewState == ESelectionPreviewState::Selected)
			{
				FLinearColor PreviewSelectionColor = SelectionColor.LinearRGBToHSV();
				PreviewSelectionColor.R += 0.1f; // +10% hue
				PreviewSelectionColor.G = 0.6f; // 60% saturation
				BorderColor = PreviewSelectionColor.HSVToLinearRGB();
				FillColor = BorderColor;
			}
			else if (SelectionPreviewState == ESelectionPreviewState::NotSelected)
			{
				BorderColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
				FillColor = KeyColor;
			}
			else if (bSelected)
			{
				BorderColor = SelectedKeyColor;
				FillColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
			}
			else if (TestKey == HoveredKey)
			{
				BorderColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
				FillColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				BorderColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);
				FillColor = KeyColor;
			}

			// Color keys with overlaps with a red border
			if (NumOverlaps > 0)
			{
				BorderColor = FLinearColor(0.83f, 0.12f, 0.12f, 1.0f); // Red
			}

			// allow group to tint the color
			if (LayoutElement.GetType() == FSectionLayoutElement::Group)
			{
				auto Group = StaticCastSharedPtr<FGroupedKeyArea>(KeyArea);
				KeyColor *= Group->GetKeyTint(KeyHandle);
			}

			// draw border
			static FVector2D ThrobAmount(12.f, 12.f);
			const FVector2D KeySize = bSelected ? SequencerSectionConstants::KeySize + ThrobAmount * ThrobScaleValue : SequencerSectionConstants::KeySize;

			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 1 : KeyLayer,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FVector2D(
						KeyPosition - FMath::CeilToFloat(KeySize.X / 2.0f),
						((KeyAreaGeometry.GetLocalSize().Y / 2.0f) - (KeySize.Y / 2.0f))
					),
					KeySize
				),
				KeyBrush,
				DrawEffects,
				BorderColor
			);

			// draw fill
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				// always draw selected keys on top of other keys
				bSelected ? KeyLayer + 2 : KeyLayer + 1,
				// Center the key along Y.  Ensure the middle of the key is at the actual key time
				KeyAreaGeometry.ToPaintGeometry(
					FillOffset + 
					FVector2D(
						(KeyPosition - FMath::CeilToFloat((KeySize.X / 2.0f) - BrushBorderWidth)),
						((KeyAreaGeometry.GetLocalSize().Y / 2.0f) - ((KeySize.Y / 2.0f) - BrushBorderWidth))
					),
					KeySize - 2.0f * BrushBorderWidth
				),
				KeyBrush,
				DrawEffects,
				FillColor
			);
		}

		if (PreviousClipState.IsSet())
		{
			InPainter.DrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
		}

		InPainter.LayerId = KeyLayer + 2;
	}
}


void SSequencerSection::PaintEasingHandles( FSequencerSectionPainter& InPainter, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const
{
	if (!SectionInterface->GetSectionObject()->GetBlendType().IsValid())
	{
		return;
	}

	TArray<FSectionHandle> AllUnderlappingSections;
	if (IsSectionHighlighted(FSectionHandle(ParentSectionArea, SectionIndex), Hotspot))
	{
		AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	}

	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			if (IsSectionHighlighted(Section, Hotspot) && !AllUnderlappingSections.Contains(Section))
			{
				AllUnderlappingSections.Add(Section);
			}
		}
	}

	FTimeToPixel TimeToPixelConverter = InPainter.GetTimeConverter();
	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		UMovieSceneSection* UnderlappingSectionObj = Handle.GetSectionInterface()->GetSectionObject();
		if (UnderlappingSectionObj->IsInfinite())
		{
			continue;
		}

		bool bDrawThisSectionsHandles = true;
		bool bLeftHandleActive = false;
		bool bRightHandleActive = false;

		// Get the hovered/selected state for the section handles from the hotspot
		if (Hotspot)
		{
			if (Hotspot->GetType() == ESequencerHotspot::EaseInHandle || Hotspot->GetType() == ESequencerHotspot::EaseOutHandle)
			{
				const FSectionEasingHandleHotspot* EasingHotspot = static_cast<const FSectionEasingHandleHotspot*>(Hotspot);

				bDrawThisSectionsHandles = EasingHotspot->Section == Handle;
				bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::EaseInHandle;
				bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::EaseOutHandle;
			}
			else if (Hotspot->GetType() == ESequencerHotspot::EasingArea)
			{
				const FSectionEasingAreaHotspot* EasingAreaHotspot = static_cast<const FSectionEasingAreaHotspot*>(Hotspot);
				for (const FEasingAreaHandle& Easing : EasingAreaHotspot->Easings)
				{
					if (Easing.Section == Handle)
					{
						if (Easing.EasingType == ESequencerEasingType::In)
						{
							bLeftHandleActive = true;
						}
						else
						{
							bRightHandleActive = true;
						}

						if (bLeftHandleActive && bRightHandleActive)
						{
							break;
						}
					}
				}
			}
		}

		if (!bDrawThisSectionsHandles)
		{
			continue;
		}

		const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FSlateBrush* EasingHandle = FEditorStyle::GetBrush("Sequencer.Section.EasingHandle");
		FVector2D HandleSize(10.f, 10.f);

		TRange<float> EaseInRange = UnderlappingSectionObj->GetEaseInRange();
		// Always draw handles if the section is highlighted, even if there is no range (to allow manual adjustment)
		FVector2D HandlePos(TimeToPixelConverter.TimeToPixel(EaseInRange.IsEmpty() ? UnderlappingSectionObj->GetStartTime() : EaseInRange.GetUpperBoundValue()), 0.f);
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			// always draw selected keys on top of other keys
			InPainter.LayerId,
			// Center the key along X.  Ensure the middle of the key is at the actual key time
			InPainter.SectionGeometry.ToPaintGeometry(
				HandlePos - FVector2D(HandleSize.X*0.5f, 0.f),
				HandleSize
			),
			EasingHandle,
			DrawEffects,
			(bLeftHandleActive ? SelectionColor : EasingHandle->GetTint(FWidgetStyle()))
		);

		TRange<float> EaseOutRange = UnderlappingSectionObj->GetEaseOutRange();
		// Always draw handles if the section is highlighted, even if there is no range (to allow manual adjustment)
		HandlePos = FVector2D(TimeToPixelConverter.TimeToPixel(EaseOutRange.IsEmpty() ? UnderlappingSectionObj->GetEndTime() : EaseOutRange.GetLowerBoundValue()), 0.f);
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			// always draw selected keys on top of other keys
			InPainter.LayerId,
			// Center the key along X.  Ensure the middle of the key is at the actual key time
			InPainter.SectionGeometry.ToPaintGeometry(
				HandlePos - FVector2D(HandleSize.X*0.5f, 0.f),
				HandleSize
			),
			EasingHandle,
			DrawEffects,
			(bRightHandleActive ? SelectionColor : EasingHandle->GetTint(FWidgetStyle()))
		);
	}
}


void SSequencerSection::DrawSectionHandles( const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect DrawEffects, FLinearColor SelectionColor, const ISequencerHotspot* Hotspot ) const
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	if (!ThisSection)
	{
		return;
	}

	TOptional<FSlateClippingState> PreviousClipState = OutDrawElements.GetClippingState();
	OutDrawElements.PopClip();

	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry.GetLayoutBoundingRect()));

	TArray<FSectionHandle> AllUnderlappingSections;
	AllUnderlappingSections.Add(FSectionHandle(ParentSectionArea, SectionIndex));
	for (const FSequencerOverlapRange& Segment : UnderlappingSegments)
	{
		for (FSectionHandle Section : Segment.Sections)
		{
			AllUnderlappingSections.AddUnique(Section);
		}
	}

	FGeometry SectionGeometryWithoutHandles = MakeSectionGeometryWithoutHandles(AllottedGeometry, SectionInterface);
	FTimeToPixel TimeToPixelConverter(SectionGeometryWithoutHandles, ThisSection->IsInfinite() ? GetSequencer().GetViewRange() : ThisSection->GetRange());
	for (FSectionHandle Handle : AllUnderlappingSections)
	{
		TSharedRef<ISequencerSection> UnderlappingSection =  Handle.TrackNode->GetSections()[Handle.SectionIndex];
		UMovieSceneSection* UnderlappingSectionObj = UnderlappingSection->GetSectionObject();
		if (!UnderlappingSection->SectionIsResizable() || UnderlappingSectionObj->IsInfinite())
		{
			continue;
		}

		bool bDrawThisSectionsHandles = (UnderlappingSectionObj == ThisSection && HandleOffsetPx != 0) || IsSectionHighlighted(Handle, Hotspot);
		bool bLeftHandleActive = false;
		bool bRightHandleActive = false;

		// Get the hovered/selected state for the section handles from the hotspot
		if (Hotspot && (
			Hotspot->GetType() == ESequencerHotspot::SectionResize_L ||
			Hotspot->GetType() == ESequencerHotspot::SectionResize_R))
		{
			const FSectionResizeHotspot* ResizeHotspot = static_cast<const FSectionResizeHotspot*>(Hotspot);
			if (ResizeHotspot->Section == Handle)
			{
				bDrawThisSectionsHandles = true;
				bLeftHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_L;
				bRightHandleActive = Hotspot->GetType() == ESequencerHotspot::SectionResize_R;
			}
			else
			{
				bDrawThisSectionsHandles = false;
			}
		}

		if (!bDrawThisSectionsHandles)
		{
			continue;
		}

		const float ThisHandleOffset = UnderlappingSectionObj == ThisSection ? HandleOffsetPx : 0.f;
		FVector2D GripSize( UnderlappingSection->GetSectionGripSize(), AllottedGeometry.Size.Y );

		// Make areas to the left and right of the geometry.  We will use these areas to determine if someone dragged the left or right edge of a section
		FGeometry SectionRectLeft = SectionGeometryWithoutHandles.MakeChild(
			FVector2D( TimeToPixelConverter.TimeToPixel(UnderlappingSectionObj->GetStartTime()) - ThisHandleOffset, 0.f ),
			GripSize
		);

		FGeometry SectionRectRight = SectionGeometryWithoutHandles.MakeChild(
			FVector2D( TimeToPixelConverter.TimeToPixel(UnderlappingSectionObj->GetEndTime()) - UnderlappingSection->GetSectionGripSize() + ThisHandleOffset, 0 ), 
			GripSize
		);

		const FSlateBrush* LeftGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripLeft");
		const FSlateBrush* RightGripBrush = FEditorStyle::GetBrush("Sequencer.Section.GripRight");

		float Opacity = 0.5f;
		if (UnderlappingSectionObj == ThisSection && HandleOffsetPx != 0)
		{
			Opacity = FMath::Clamp(.5f + HandleOffsetPx / UnderlappingSection->GetSectionGripSize() * .5f, .5f, 1.f);
		}
		
		// Left Grip
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			LayerId,
			SectionRectLeft.ToPaintGeometry(),
			LeftGripBrush,
			DrawEffects,
			(bLeftHandleActive ? SelectionColor : LeftGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
		);
		
		// Right Grip
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			LayerId,
			SectionRectRight.ToPaintGeometry(),
			RightGripBrush,
			DrawEffects,
			(bRightHandleActive ? SelectionColor : RightGripBrush->GetTint(FWidgetStyle())).CopyWithNewOpacity(Opacity)
		);
	}

	OutDrawElements.PopClip();
	if (PreviousClipState.IsSet())
	{
		OutDrawElements.GetClippingManager().PushClippingState(PreviousClipState.GetValue());
	}
}

void SSequencerSection::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{	
	if( GetVisibility() == EVisibility::Visible )
	{
		Layout = FSectionLayout(*ParentSectionArea, SectionIndex);

		// Update cached key area key positions
		for (const FSectionLayoutElement& LayoutElement : Layout->GetElements())
		{
			TSharedPtr<IKeyArea> KeyArea = LayoutElement.GetKeyArea();
			if (KeyArea.IsValid())
			{
				CachedKeyAreaPositions.FindOrAdd(KeyArea).Update(KeyArea.ToSharedRef());
			}
		}

		UMovieSceneSection* Section = SectionInterface->GetSectionObject();
		if (Section && !Section->IsInfinite())
		{
			FTimeToPixel TimeToPixelConverter(ParentGeometry, GetSequencer().GetViewRange());

			const int32 SectionLengthPx = FMath::Max(0,
				FMath::RoundToInt(
					TimeToPixelConverter.TimeToPixel(Section->GetEndTime())) - FMath::RoundToInt(TimeToPixelConverter.TimeToPixel(Section->GetStartTime())
				)
			);

			const float SectionGripSize = SectionInterface->GetSectionGripSize();
			HandleOffsetPx = FMath::Max(FMath::RoundToFloat((2*SectionGripSize - SectionLengthPx) * .5f), 0.f);
		}
		else
		{
			HandleOffsetPx = 0;
		}

		FGeometry SectionGeometry = MakeSectionGeometryWithoutHandles( AllottedGeometry, SectionInterface );
		SectionInterface->Tick(SectionGeometry, ParentGeometry, InCurrentTime, InDeltaTime);

		UpdateUnderlappingSegments();
	}
}

FReply SSequencerSection::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FSequencer& Sequencer = GetSequencer();

	FSequencerSelectedKey HoveredKey;
	
	// The hovered key is defined from the sequencer hotspot
	TSharedPtr<ISequencerHotspot> Hotspot = GetSequencer().GetHotspot();
	if (Hotspot.IsValid() && Hotspot->GetType() == ESequencerHotspot::Key)
	{
		HoveredKey = static_cast<FKeyHotspot*>(Hotspot.Get())->Key;
	}

	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->BeginTransaction(NSLOCTEXT("Sequencer", "CreateKey_Transaction", "Create Key"));

		// Generate a key and set it as the PressedKey
		FSequencerSelectedKey NewKey = CreateKeyUnderMouse(MouseEvent.GetScreenSpacePosition(), MyGeometry, HoveredKey);

		if (NewKey.IsValid())
		{
			HoveredKey = NewKey;

			Sequencer.GetSelection().EmptySelectedKeys();
			Sequencer.GetSelection().AddToSelection(NewKey);

			// Pass the event to the tool to copy the hovered key and move it
			GetSequencer().SetHotspot( MakeShareable( new FKeyHotspot(NewKey) ) );

			// Return unhandled so that the EditTool can handle the mouse down based on the newly created keyframe and prepare to move it
			return FReply::Unhandled();
		}
	}

	return FReply::Unhandled();
}


FGeometry SSequencerSection::MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const
{
	return AllottedGeometry.MakeChild(
		AllottedGeometry.GetLocalSize() - FVector2D( HandleOffsetPx*2, 0.0f ),
		FSlateLayoutTransform(FVector2D(HandleOffsetPx, 0 ))
	);
}

void SSequencerSection::UpdateUnderlappingSegments()
{
	UMovieSceneSection* ThisSection = SectionInterface->GetSectionObject();
	UMovieSceneTrack* Track = ThisSection ? ThisSection->GetTypedOuter<UMovieSceneTrack>() : nullptr;
	if (!Track)
	{
		UnderlappingSegments.Reset();
		UnderlappingEasingSegments.Reset();
	}
	else if (Track->GetSignature() != CachedTrackSignature)
	{
		UnderlappingSegments = ParentSectionArea->GetUnderlappingSections(ThisSection);
		UnderlappingEasingSegments = ParentSectionArea->GetEasingSegmentsForSection(ThisSection);
		CachedTrackSignature = Track->GetSignature();
	}
}

FReply SSequencerSection::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		FReply Reply = SectionInterface->OnSectionDoubleClicked( MyGeometry, MouseEvent );

		if (!Reply.IsEventHandled())
		{
			// Find the object binding this node is underneath
			FGuid ObjectBinding;
			if (ParentSectionArea.IsValid())
			{
				TSharedPtr<FSequencerObjectBindingNode> ObjectBindingNode = ParentSectionArea->FindParentObjectBindingNode();
				if (ObjectBindingNode.IsValid())
				{
					ObjectBinding = ObjectBindingNode->GetObjectBinding();
				}
			}

			Reply = SectionInterface->OnSectionDoubleClicked(MyGeometry, MouseEvent, ObjectBinding);
		}

		if (Reply.IsEventHandled())
		{
			return Reply;
		}

		GetSequencer().ZoomToSelectedSections();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SSequencerSection::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Checked for hovered key
	FSequencerSelectedKey KeyUnderMouse = GetKeyUnderMouse( MouseEvent.GetScreenSpacePosition(), MyGeometry );
	if ( KeyUnderMouse.IsValid() )
	{
		GetSequencer().SetHotspot( MakeShareable( new FKeyHotspot(KeyUnderMouse) ) );
	}
	// Check other interaction points in order of importance
	else if (
		!CheckForEasingHandleInteraction(MouseEvent, MyGeometry) &&
		!CheckForEdgeInteraction(MouseEvent, MyGeometry) &&
		!CheckForEasingAreaInteraction(MouseEvent, MyGeometry))
	{
		// If nothing was hit, we just hit the section
		GetSequencer().SetHotspot( MakeShareable( new FSectionHotspot(FSectionHandle(ParentSectionArea, SectionIndex))) );
	}

	return FReply::Unhandled();
}
	
FReply SSequencerSection::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		GEditor->EndTransaction();

		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SSequencerSection::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave( MouseEvent );
	GetSequencer().SetHotspot(nullptr);
}

static float ThrobDurationSeconds = .5f;
void SSequencerSection::ThrobSelection(int32 ThrobCount)
{
	SelectionThrobEndTime = FPlatformTime::Seconds() + ThrobCount*ThrobDurationSeconds;
}

float EvaluateThrob(float Alpha)
{
	return .5f - FMath::Cos(FMath::Pow(Alpha, 0.5f) * 2 * PI) * .5f;
}

float SSequencerSection::GetSelectionThrobValue()
{
	double CurrentTime = FPlatformTime::Seconds();

	if (SelectionThrobEndTime > CurrentTime)
	{
		float Difference = SelectionThrobEndTime - CurrentTime;
		return EvaluateThrob(1.f - FMath::Fmod(Difference, ThrobDurationSeconds));
	}

	return 0.f;
}

bool SSequencerSection::IsSectionHighlighted(FSectionHandle InSectionHandle, const ISequencerHotspot* Hotspot)
{
	if (!Hotspot)
	{
		return false;
	}

	switch(Hotspot->GetType())
	{
	case ESequencerHotspot::Key:
		return static_cast<const FKeyHotspot*>(Hotspot)->Key.Section == InSectionHandle.GetSectionObject();
	case ESequencerHotspot::Section:
		return static_cast<const FSectionHotspot*>(Hotspot)->Section == InSectionHandle;
	case ESequencerHotspot::SectionResize_L:
	case ESequencerHotspot::SectionResize_R:
		return static_cast<const FSectionResizeHotspot*>(Hotspot)->Section == InSectionHandle;
	case ESequencerHotspot::EaseInHandle:
	case ESequencerHotspot::EaseOutHandle:
		return static_cast<const FSectionEasingHandleHotspot*>(Hotspot)->Section == InSectionHandle;
	case ESequencerHotspot::EasingArea:
		return static_cast<const FSectionEasingAreaHotspot*>(Hotspot)->Contains(InSectionHandle);
	default:
		return false;
	}
}
