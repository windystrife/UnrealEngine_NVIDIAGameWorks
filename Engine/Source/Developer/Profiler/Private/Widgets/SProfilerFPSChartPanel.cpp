// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerFPSChartPanel.h"
#include "Widgets/SBoxPanel.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "ProfilerFPSAnalyzer.h"
#include "Widgets/SHistogram.h"

#define LOCTEXT_NAMESPACE "SProfilerFPSChartPanel"


class SProfilerFPSStatisticsPanel
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SProfilerFPSStatisticsPanel )
	{}
		SLATE_ARGUMENT( TSharedPtr<FFPSAnalyzer>, FPSAnalyzer )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		FPSAnalyzer = InArgs._FPSAnalyzer;

		ChildSlot
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding( 2.0f )
			[
				SNew( SVerticalBox )

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("StatisticsLabel", "Statistics"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SProfilerFPSStatisticsPanel::HandleSampleCount)
				]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleMinFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleMaxFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleAverageFPS)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS90)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS60)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS30)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS25)
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SProfilerFPSStatisticsPanel::HandleFPS20)
					]
			]	
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION
		
	void SetFPSAnalyzer(const TSharedPtr<FFPSAnalyzer>& InAnalyzer)
	{
		FPSAnalyzer = InAnalyzer;
	}

protected:
	FText HandleSampleCount() const
	{
		return FText::Format(LOCTEXT("SamplesCountFmt", "Samples: {0}"), FText::AsNumber(FPSAnalyzer->Samples.Num()));
	}
	FText HandleMinFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("MinFPSFmt", "Min FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->MinFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleMaxFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("MaxFPSFmt", "Max FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->MaxFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleAverageFPS() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("AverageFPSFmt", "Ave FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(FPSAnalyzer->AveFPS, &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS90() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+90FPSFmt", "+90FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS90 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS60() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+60FPSFmt", "+60FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS60 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS30() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+30FPSFmt", "+30FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS30 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS25() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+25FPSFmt", "+25FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS25 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}
	FText HandleFPS20() const
	{
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::Format(LOCTEXT("+20FPSFmt", "+20FPS: {0}"), ((FPSAnalyzer->Samples.Num() > 0) ? FText::AsNumber(100.0f*(float)FPSAnalyzer->FPS20 / (float)FPSAnalyzer->Samples.Num(), &FormatOptions) : FText::GetEmpty()));
	}

protected:
	TSharedPtr<FFPSAnalyzer> FPSAnalyzer;
};

SProfilerFPSChartPanel::~SProfilerFPSChartPanel()
{
	// Remove ourselves from the profiler manager.
	auto ProfilerManager = FProfilerManager::Get();
	if (ProfilerManager.IsValid())
	{
		ProfilerManager->OnViewModeChanged().RemoveAll(this);
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProfilerFPSChartPanel::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		.Padding( 2.0f )
		[
			SNew( SHorizontalBox )

			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(Histogram, SHistogram)
				.Description(FHistogramDescription(InArgs._FPSAnalyzer.ToSharedRef(), 5, 0, 90, true))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(StatisticsPanel, SProfilerFPSStatisticsPanel)
				.FPSAnalyzer( InArgs._FPSAnalyzer )
			]
		]	
	];

	auto ProfilerManager = FProfilerManager::Get();
	if (ProfilerManager.IsValid())
	{
		ProfilerManager->OnViewModeChanged().AddSP(this, &SProfilerFPSChartPanel::ProfilerManager_OnViewModeChanged);
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProfilerFPSChartPanel::ProfilerManager_OnViewModeChanged(EProfilerViewMode NewViewMode)
{
	auto ProfilerManager = FProfilerManager::Get();
	if (ProfilerManager.IsValid())
	{
		const TSharedPtr<FProfilerSession> ProfilerSession = ProfilerManager->GetProfilerSession();

		Histogram->SetFPSAnalyzer(ProfilerSession->FPSAnalyzer);
		StatisticsPanel->SetFPSAnalyzer(ProfilerSession->FPSAnalyzer);
	}
}

#undef LOCTEXT_NAMESPACE
