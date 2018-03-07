// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SHistogram.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SOverlay.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "ProfilerFPSAnalyzer.h"


#define LOCTEXT_NAMESPACE "SHistogram"


/*-----------------------------------------------------------------------------
	SHistogram
-----------------------------------------------------------------------------*/

void SHistogram::Construct( const FArguments& InArgs )
{
	Description = InArgs._Description.Get();

	ChildSlot
	[
		SNew(SOverlay)
		.Visibility( EVisibility::SelfHitTestInvisible )
	];
}

int32 SHistogram::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Rendering info.
	const bool bEnabled  = ShouldBeEnabled( bParentEnabled );
	ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* TimelineAreaBrush = FEditorStyle::GetBrush("Profiler.LineGraphArea");
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush("WhiteTexture");
	const FSlateBrush* FillImage  = FEditorStyle::GetBrush("TaskGraph.Mono");

	// Draw background.
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D(0,0), FVector2D(AllottedGeometry.GetLocalSize().X,AllottedGeometry.GetLocalSize().Y) ),
		TimelineAreaBrush,
		DrawEffects,
		TimelineAreaBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
	);
	LayerId++;

	const float LabelBuffer = 25.0f;

	// draw the grid lines
	uint32 CountX = (uint32)((AllottedGeometry.Size.X-LabelBuffer*2.0f) / Description.GetBinCount());
	float StartX = LabelBuffer;
	static const FLinearColor GridColor = FLinearColor(0.0f,0.0f,0.0f, 0.25f);
	static const FLinearColor GridTextColor = FLinearColor(1.0f,1.0f,1.0f, 0.25f);
	static const FLinearColor BorderColor = FLinearColor(0.0f,0.0f,0.0f,1.0f);
	FSlateFontInfo SummaryFont(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );
	const float MaxFontCharHeight = FontMeasureService->Measure( TEXT("!"), SummaryFont ).Y;
	TArray<FVector2D> LinePoints;

	// draw the histogram box
	LinePoints.Add( FVector2D(StartX-1, LabelBuffer-1) );
	LinePoints.Add( FVector2D(StartX + Description.GetBinCount()*CountX+1, LabelBuffer-1) );
	LinePoints.Add( FVector2D(StartX + Description.GetBinCount()*CountX+1, AllottedGeometry.GetLocalSize().Y - LabelBuffer+1) );
	LinePoints.Add( FVector2D(StartX-1, AllottedGeometry.Size.Y - LabelBuffer+1) );
	LinePoints.Add( FVector2D(StartX-1, LabelBuffer-1) );
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		LinePoints,
		DrawEffects,
		BorderColor
	);
	LinePoints.Empty();
	LayerId++;
	
	// draw the vertical lines
	for (int32 Index = 0; Index < Description.GetBinCount(); ++Index)
	{
		float MarkerPosX = StartX + Index * CountX;
		LinePoints.Add( FVector2D(MarkerPosX, LabelBuffer-1) );
		LinePoints.Add( FVector2D(MarkerPosX, AllottedGeometry.GetLocalSize().Y - LabelBuffer+1) );
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			DrawEffects,
			GridColor
		);
		LinePoints.Empty();

		// Bottom - X-Axes numbers, starting from MinValue
		const FString XLabel = FString::Printf(TEXT("%.0f"), Description.MinValue + Index*Description.Interval);
		float FontCharWidth = FontMeasureService->Measure(XLabel, SummaryFont).X;
		FSlateDrawElement::MakeText(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToOffsetPaintGeometry( FVector2D(MarkerPosX-FontCharWidth/2.0f,AllottedGeometry.GetLocalSize().Y-LabelBuffer/2.0f-MaxFontCharHeight/2.0f) ),
			XLabel,
			SummaryFont,
			DrawEffects,
			FLinearColor::White
		);

	}
	LayerId++;

	// draw the horizontal lines
	float CountY = (AllottedGeometry.GetLocalSize().Y-LabelBuffer*2.0f) / 4;
	float StartY = LabelBuffer;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		float MarkerPosY = StartY + Index * CountY;
		LinePoints.Add( FVector2D(StartX, MarkerPosY) );
		LinePoints.Add( FVector2D(StartX + Description.GetBinCount()*CountX, MarkerPosY) );
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			DrawEffects,
			GridColor
			);
		LinePoints.Empty();

		// Bottom - Y-Axes numbers, starting from 0
		const FString YLabel = FString::Printf(TEXT("%i"), Description.Normalize ? 25 * (4-Index) : Description.GetTotalCount() / 4 * Index);
		float FontCharWidth = FontMeasureService->Measure(YLabel, SummaryFont).X;
		FSlateDrawElement::MakeText
			(
			OutDrawElements, 
			LayerId, 
			AllottedGeometry.ToOffsetPaintGeometry( FVector2D(LabelBuffer/2.0f-FontCharWidth/2.0f,MarkerPosY-MaxFontCharHeight/2.0f) ),
			YLabel,
			SummaryFont,
			DrawEffects, 
			FLinearColor::White
			);

	}
	LayerId++;

	for (int32 Index = 0; Index < Description.GetBinCount(); ++Index)
	{
		float MarkerPosX = StartX + Index * CountX;
		float SizeY = (float)Description.GetCount(Index) / (float)Description.GetTotalCount() * (AllottedGeometry.GetLocalSize().Y - LabelBuffer*2.0f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry( FVector2D(MarkerPosX, AllottedGeometry.GetLocalSize().Y - SizeY- LabelBuffer), FVector2D(CountX, SizeY) ),
			FillImage,
			DrawEffects,
			FLinearColor::Green
			);
	}
	return SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );
}

void SHistogram::SetFPSAnalyzer(const TSharedPtr<FFPSAnalyzer>& InAnalyzer)
{
	Description.HistogramDataSource = InAnalyzer;
}

#undef LOCTEXT_NAMESPACE


