// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SScalabilitySettings.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSlider.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Settings/EditorSettings.h"
#include "Editor/EditorPerformanceSettings.h"

#define LOCTEXT_NAMESPACE "EngineScalabiltySettings"

ECheckBoxState SScalabilitySettings::IsGroupQualityLevelSelected(const TCHAR* InGroupName, int32 InQualityLevel) const
{
	int32 QualityLevel = -1;

	if (FCString::Strcmp(InGroupName, TEXT("ResolutionQuality")) == 0) { QualityLevel = CachedQualityLevels.ResolutionQuality; }
	else if (FCString::Strcmp(InGroupName, TEXT("ViewDistanceQuality")) == 0) QualityLevel = CachedQualityLevels.ViewDistanceQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("AntiAliasingQuality")) == 0) QualityLevel = CachedQualityLevels.AntiAliasingQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("PostProcessQuality")) == 0) QualityLevel = CachedQualityLevels.PostProcessQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("ShadowQuality")) == 0) QualityLevel = CachedQualityLevels.ShadowQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("TextureQuality")) == 0) QualityLevel = CachedQualityLevels.TextureQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("EffectsQuality")) == 0) QualityLevel = CachedQualityLevels.EffectsQuality;
	else if (FCString::Strcmp(InGroupName, TEXT("FoliageQuality")) == 0) QualityLevel = CachedQualityLevels.FoliageQuality;

	return (QualityLevel == InQualityLevel) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SScalabilitySettings::OnGroupQualityLevelChanged(ECheckBoxState NewState, const TCHAR* InGroupName, int32 InQualityLevel)
{
	if (FCString::Strcmp(InGroupName, TEXT("ResolutionQuality")) == 0) CachedQualityLevels.ResolutionQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("ViewDistanceQuality")) == 0) CachedQualityLevels.ViewDistanceQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("AntiAliasingQuality")) == 0) CachedQualityLevels.AntiAliasingQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("PostProcessQuality")) == 0) CachedQualityLevels.PostProcessQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("ShadowQuality")) == 0) CachedQualityLevels.ShadowQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("TextureQuality")) == 0) CachedQualityLevels.TextureQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("EffectsQuality")) == 0) CachedQualityLevels.EffectsQuality = InQualityLevel;
	else if (FCString::Strcmp(InGroupName, TEXT("FoliageQuality")) == 0) CachedQualityLevels.FoliageQuality = InQualityLevel;

	Scalability::SetQualityLevels(CachedQualityLevels);
	Scalability::SaveState(GEditorSettingsIni);
	GEditor->RedrawAllViewports();
}

void SScalabilitySettings::OnResolutionScaleChanged(float InValue)
{
	CachedQualityLevels.ResolutionQuality = FMath::Lerp(Scalability::MinResolutionScale, Scalability::MaxResolutionScale, InValue);

	Scalability::SetQualityLevels(CachedQualityLevels);
	Scalability::SaveState(GEditorSettingsIni);
	GEditor->RedrawAllViewports();
}

float SScalabilitySettings::GetResolutionScale() const
{
	return (float)(CachedQualityLevels.ResolutionQuality - Scalability::MinResolutionScale) / (float)(Scalability::MaxResolutionScale - Scalability::MinResolutionScale);
}

FText SScalabilitySettings::GetResolutionScaleString() const
{
	return FText::AsPercent(FMath::Square(CachedQualityLevels.ResolutionQuality / 100.0f));
}

TSharedRef<SWidget> SScalabilitySettings::MakeButtonWidget(const FText& InName, const TCHAR* InGroupName, int32 InQualityLevel, const FText& InToolTip)
{
	return SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
		.OnCheckStateChanged(this, &SScalabilitySettings::OnGroupQualityLevelChanged, InGroupName, InQualityLevel)
		.IsChecked(this, &SScalabilitySettings::IsGroupQualityLevelSelected, InGroupName, InQualityLevel)
		.ToolTipText(InToolTip)
		.Content()
		[
			SNew(STextBlock)
			.Text(InName)
		];
}

TSharedRef<SWidget> SScalabilitySettings::MakeHeaderButtonWidget(const FText& InName, int32 InQualityLevel, const FText& InToolTip, Scalability::EQualityLevelBehavior Behavior)
{
	return SNew(SButton)
		.OnClicked(this, &SScalabilitySettings::OnHeaderClicked, InQualityLevel, Behavior)
		.ToolTipText(InToolTip)
		.Content()
		[
			SNew(STextBlock)
			.Text(InName)
		];
}

TSharedRef<SWidget> SScalabilitySettings::MakeAutoButtonWidget()
{
	return SNew(SButton)
		.OnClicked(this, &SScalabilitySettings::OnAutoClicked)
		.ToolTipText(LOCTEXT("AutoButtonTooltip", "We test your system and try to find the most suitable settings"))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AutoLabel", "Auto"))
		];
}

FReply SScalabilitySettings::OnHeaderClicked(int32 InQualityLevel, Scalability::EQualityLevelBehavior Behavior)
{
	Scalability::FQualityLevels LevelCounts = Scalability::GetQualityLevelCounts();
	switch (Behavior)
	{
		case Scalability::EQualityLevelBehavior::ERelativeToMax:
			CachedQualityLevels.SetFromSingleQualityLevelRelativeToMax(InQualityLevel);
			break;		
		case Scalability::EQualityLevelBehavior::EAbsolute:
		default:
			CachedQualityLevels.SetFromSingleQualityLevel(InQualityLevel);
			break;
	}
	Scalability::SetQualityLevels(CachedQualityLevels);
	Scalability::SaveState(GEditorSettingsIni);
	GEditor->RedrawAllViewports();
	return FReply::Handled();
}

FReply SScalabilitySettings::OnAutoClicked()
{
	auto* Settings = GetMutableDefault<UEditorSettings>();
	Settings->AutoApplyScalabilityBenchmark();
	Settings->LoadScalabilityBenchmark();

	CachedQualityLevels = Settings->EngineBenchmarkResult;

	GEditor->RedrawAllViewports();
	return FReply::Handled();
}

SGridPanel::FSlot& SScalabilitySettings::MakeGridSlot(int32 InCol, int32 InRow, int32 InColSpan /*= 1*/, int32 InRowSpan /*= 1*/)
{
	float PaddingH = 2.0f;
	float PaddingV = InRow == 0 ? 8.0f : 2.0f;
	return SGridPanel::Slot(InCol, InRow)
		.Padding(PaddingH, PaddingV)
		.RowSpan(InRowSpan)
		.ColumnSpan(InColSpan);
}

ECheckBoxState SScalabilitySettings::IsMonitoringPerformance() const
{
	const bool bMonitorEditorPerformance = GetDefault<UEditorPerformanceSettings>()->bMonitorEditorPerformance;
	return bMonitorEditorPerformance ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SScalabilitySettings::OnMonitorPerformanceChanged(ECheckBoxState NewState)
{
	const bool bNewEnabledState = ( NewState == ECheckBoxState::Checked );

	auto* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bMonitorEditorPerformance = bNewEnabledState;
	Settings->PostEditChange();
	Settings->SaveConfig();
}

void SScalabilitySettings::AddButtonsToGrid(int32 X0, int32 Y0, TSharedRef<SGridPanel> ButtonMatrix, const FText* FiveNameArray, int32 ButtonCount, const TCHAR* GroupName, const FText& TooltipShape)
{
	const int32 ExpectedNamesSize = 5;
	const bool bCanUseNameArray = (FiveNameArray != nullptr);
	const bool bCanAllUseNameArray = (ButtonCount == ExpectedNamesSize) && bCanUseNameArray;
	const int32 CineButtonName = ExpectedNamesSize - 1;
	const int32 CineButtonIndex = ButtonCount - 1;

	for (int32 ButtonIndex = 0; ButtonIndex < ButtonCount; ++ButtonIndex)
	{
		const bool bCineButton = ButtonIndex == CineButtonIndex;
		const bool bUseNameArray = bCanUseNameArray && (bCanAllUseNameArray || bCineButton);
		const int32 ButtonNameIndex = bCineButton ? CineButtonName : ButtonIndex;

		const FText ButtonLabel = bUseNameArray ? FiveNameArray[ButtonNameIndex] : FText::AsNumber(ButtonIndex);
		const FText ButtonTooltip = FText::Format(TooltipShape, ButtonLabel);

		ButtonMatrix->AddSlot(X0 + ButtonIndex, Y0)
		[
			MakeButtonWidget(ButtonLabel, GroupName, ButtonIndex, ButtonTooltip)
		];
	}
}

void SScalabilitySettings::Construct( const FArguments& InArgs )
{
	const FText NamesLow(LOCTEXT("QualityLowLabel", "Low"));
	const FText NamesMedium(LOCTEXT("QualityMediumLabel", "Medium"));
	const FText NamesHigh(LOCTEXT("QualityHighLabel", "High"));
	const FText NamesEpic(LOCTEXT("QualityEpicLabel", "Epic"));
	const FText NamesCine(LOCTEXT("QualityCineLabel", "Cinematic"));
	const FText NamesAuto(LOCTEXT("QualityAutoLabel", "Auto"));

	const FText DistanceNear = LOCTEXT("ViewDistanceLabel2", "Near");
	const FText DistanceMedium = LOCTEXT("ViewDistanceLabel3", "Medium");
	const FText DistanceFar = LOCTEXT("ViewDistanceLabel4", "Far");
	const FText DistanceEpic = LOCTEXT("ViewDistanceLabel5", "Epic");
	const FText DistanceCinematic = LOCTEXT("ViewDistanceLabel6", "Cinematic");
		
	const FText FiveNames[5] = { NamesLow, NamesMedium, NamesHigh, NamesEpic, NamesCine };
	const FText FiveDistanceNames[5] = { DistanceNear, DistanceMedium, DistanceFar, DistanceEpic, DistanceCinematic };

	auto TitleFont = FEditorStyle::GetFontStyle( FName( "Scalability.TitleFont" ) );
	auto GroupFont = FEditorStyle::GetFontStyle( FName( "Scalability.GroupFont" ) );

	InitialQualityLevels = CachedQualityLevels = Scalability::GetQualityLevels();
	static float Padding = 1.0f;
	static float QualityColumnCoeff = 1.0f;
	static float QualityLevelColumnCoeff = 0.2f;
		
	Scalability::FQualityLevels LevelCounts = Scalability::GetQualityLevelCounts();
	const int32 MaxLevelCount =
		FMath::Max(LevelCounts.ShadowQuality,
		FMath::Max(LevelCounts.TextureQuality,
		FMath::Max(LevelCounts.ViewDistanceQuality,
		FMath::Max(LevelCounts.EffectsQuality,
		FMath::Max(LevelCounts.FoliageQuality,
		FMath::Max(LevelCounts.PostProcessQuality, LevelCounts.AntiAliasingQuality)
		)))));
	const int32 TotalWidth = MaxLevelCount + 1;

	TSharedRef<SGridPanel> ButtonMatrix = 
		SNew(SGridPanel)
		.FillColumn(0, QualityColumnCoeff)

		+MakeGridSlot(0,1,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,2,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,3,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,4,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,5,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,6,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,7,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]
		+MakeGridSlot(0,8,TotalWidth,1) [ SNew (SBorder).BorderImage(FEditorStyle::GetBrush("Scalability.RowBackground")) ]

		+MakeGridSlot(0,0).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("QualityLabel", "Quality")).Font(TitleFont) ]
		+MakeGridSlot(1,0) [ MakeHeaderButtonWidget(NamesLow, 0, LOCTEXT("QualityLow", "Set all groups to low quality"), Scalability::EQualityLevelBehavior::EAbsolute) ]
		+MakeGridSlot(2,0) [ MakeHeaderButtonWidget(NamesMedium, 3, LOCTEXT("QualityMedium", "Set all groups to medium quality"), Scalability::EQualityLevelBehavior::ERelativeToMax) ]
		+MakeGridSlot(3,0) [ MakeHeaderButtonWidget(NamesHigh, 2, LOCTEXT("QualityHigh", "Set all groups to high quality"), Scalability::EQualityLevelBehavior::ERelativeToMax) ]
		+MakeGridSlot(4,0) [ MakeHeaderButtonWidget(NamesEpic, 1, LOCTEXT("QualityEpic", "Set all groups to epic quality"), Scalability::EQualityLevelBehavior::ERelativeToMax) ]
		+MakeGridSlot(5,0) [ MakeHeaderButtonWidget(NamesCine, 0, LOCTEXT("QualityCinematic", "Set all groups to offline cinematic quality"), Scalability::EQualityLevelBehavior::ERelativeToMax)]
		+MakeGridSlot(6,0) [ MakeAutoButtonWidget() ]

		+MakeGridSlot(0,1) [ SNew(STextBlock).Text(LOCTEXT("ResScaleLabel1", "Resolution Scale")).Font(GroupFont) ]
		+MakeGridSlot(5,1) [ SNew(STextBlock).Text(this, &SScalabilitySettings::GetResolutionScaleString) ]
		+MakeGridSlot(1,1,4,1) [ SNew(SSlider).OnValueChanged(this, &SScalabilitySettings::OnResolutionScaleChanged).Value(this, &SScalabilitySettings::GetResolutionScale) ]
				
		+MakeGridSlot(0,2) [ SNew(STextBlock).Text(LOCTEXT("ViewDistanceLabel1", "View Distance")).Font(GroupFont) ]

		+MakeGridSlot(0,3) [ SNew(STextBlock).Text(LOCTEXT("AntiAliasingQualityLabel1", "Anti-Aliasing")).Font(GroupFont) ]

		+MakeGridSlot(0,4) [ SNew(STextBlock).Text(LOCTEXT("PostProcessQualityLabel1", "Post Processing")).Font(GroupFont) ]

		+MakeGridSlot(0,5) [ SNew(STextBlock).Text(LOCTEXT("ShadowLabel1", "Shadows")).Font(GroupFont) ]

		+MakeGridSlot(0,6) [ SNew(STextBlock).Text(LOCTEXT("TextureQualityLabel1", "Textures")).Font(GroupFont) ]

		+MakeGridSlot(0,7) [ SNew(STextBlock).Text(LOCTEXT("EffectsQualityLabel1", "Effects")).Font(GroupFont) ]
		+MakeGridSlot(0,8) [ SNew(STextBlock).Text(LOCTEXT("FoliageQualityLabel1", "Foliage")).Font(GroupFont) ]
		;

	AddButtonsToGrid(1, 2, ButtonMatrix, FiveDistanceNames, LevelCounts.ViewDistanceQuality, TEXT("ViewDistanceQuality"), LOCTEXT("ViewDistanceQualityTooltip", "Set view distance to {0}"));
	AddButtonsToGrid(1, 3, ButtonMatrix, FiveNames, LevelCounts.AntiAliasingQuality, TEXT("AntiAliasingQuality"), LOCTEXT("AntiAliasingQualityTooltip", "Set anti-aliasing quality to {0}"));
	AddButtonsToGrid(1, 4, ButtonMatrix, FiveNames, LevelCounts.PostProcessQuality, TEXT("PostProcessQuality"), LOCTEXT("PostProcessQualityTooltip", "Set post processing quality to {0}"));
	AddButtonsToGrid(1, 5, ButtonMatrix, FiveNames, LevelCounts.ShadowQuality, TEXT("ShadowQuality"), LOCTEXT("ShadowQualityTooltip", "Set shadow quality to {0}"));
	AddButtonsToGrid(1, 6, ButtonMatrix, FiveNames, LevelCounts.TextureQuality, TEXT("TextureQuality"), LOCTEXT("TextureQualityTooltip", "Set texture quality to {0}"));
	AddButtonsToGrid(1, 7, ButtonMatrix, FiveNames, LevelCounts.EffectsQuality, TEXT("EffectsQuality"), LOCTEXT("EffectsQualityTooltip", "Set effects quality to {0}"));
	AddButtonsToGrid(1, 8, ButtonMatrix, FiveNames, LevelCounts.FoliageQuality, TEXT("FoliageQuality"), LOCTEXT("FoliageQualityTooltip", "Set foliage quality to {0}"));


	this->ChildSlot
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				ButtonMatrix
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5.f)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SScalabilitySettings::OnMonitorPerformanceChanged)
				.IsChecked(this, &SScalabilitySettings::IsMonitoringPerformance)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PerformanceWarningEnableDisableCheckbox", "Monitor Editor Performance?"))
				]
			]
		];
}

SScalabilitySettings::~SScalabilitySettings()
{
	// Record any changed quality levels
	if( InitialQualityLevels != CachedQualityLevels )
	{
		const bool bAutoApplied = false;
		Scalability::RecordQualityLevelsAnalytics(bAutoApplied);
	}
}

#undef LOCTEXT_NAMESPACE
