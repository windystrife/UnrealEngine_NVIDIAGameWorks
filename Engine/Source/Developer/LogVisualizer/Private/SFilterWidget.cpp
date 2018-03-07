// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SFilterWidget.h"
#include "Misc/OutputDeviceHelper.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LogVisualizerSettings.h"
#include "LogVisualizerStyle.h"

#define LOCTEXT_NAMESPACE "SFilterWidget"
/** Constructs this widget with InArgs */
void SFilterWidget::Construct(const FArguments& InArgs)
{
	OnFilterChanged = InArgs._OnFilterChanged;
	OnRequestRemove = InArgs._OnRequestRemove;
	OnRequestEnableOnly = InArgs._OnRequestEnableOnly;
	OnRequestDisableAll = InArgs._OnRequestDisableAll;
	OnRequestRemoveAll = InArgs._OnRequestRemoveAll;
	FilterName = InArgs._FilterName;
	ColorCategory = InArgs._ColorCategory;
	BorderBackgroundColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.2f);

	LastVerbosity = ELogVerbosity::NoLogging;
	GetCaptionString(); // cache caption string

	bWasEnabledLastTime = !IsEnabled();
	GetTooltipString(); // cache tooltip string

	// Get the tooltip and color of the type represented by this filter
	FilterColor = ColorCategory;

	ChildSlot
		[
			SNew(SBorder)
			.Padding(2)
			.BorderBackgroundColor(this, &SFilterWidget::GetBorderBackgroundColor)
			.BorderImage(FLogVisualizerStyle::Get().GetBrush("ContentBrowser.FilterButtonBorder"))
			[
				SAssignNew(ToggleButtonPtr, SFilterCheckBox)
				.Style(FLogVisualizerStyle::Get(), "ContentBrowser.FilterButton")
				.ToolTipText(this, &SFilterWidget::GetTooltipString)
				.Padding(this, &SFilterWidget::GetFilterNamePadding)
				.IsChecked(this, &SFilterWidget::IsChecked)
				.OnCheckStateChanged(this, &SFilterWidget::FilterToggled)
				.OnGetMenuContent(this, &SFilterWidget::GetRightClickMenuContent)
				.ForegroundColor(this, &SFilterWidget::GetFilterForegroundColor)
				[
					SNew(STextBlock)
					.ColorAndOpacity(this, &SFilterWidget::GetFilterNameColorAndOpacity)
					.Font(FLogVisualizerStyle::Get().GetFontStyle("ContentBrowser.FilterNameFont"))
					.ShadowOffset(FVector2D(1.f, 1.f))
					.Text(this, &SFilterWidget::GetCaptionString)
				]
			]
		];

	ToggleButtonPtr->SetOnFilterDoubleClicked(FOnClicked::CreateSP(this, &SFilterWidget::FilterDoubleClicked));
	ToggleButtonPtr->SetOnFilterMiddleButtonClicked(FOnClicked::CreateSP(this, &SFilterWidget::FilterMiddleButtonClicked));
}

FText SFilterWidget::GetCaptionString() const
{
	FString CaptionString;
	FCategoryFilter& CategoryFilter = FVisualLoggerFilters::Get().GetCategoryByName(GetFilterNameAsString());
	if (CategoryFilter.LogVerbosity != LastVerbosity)
	{
		const FString VerbosityStr = FOutputDeviceHelper::VerbosityToString((ELogVerbosity::Type)CategoryFilter.LogVerbosity);
		if (CategoryFilter.LogVerbosity != ELogVerbosity::VeryVerbose)
		{
			CaptionString = FString::Printf(TEXT("%s [%s]"), *GetFilterNameAsString().Replace(TEXT("Log"), TEXT(""), ESearchCase::CaseSensitive), *VerbosityStr.Mid(0, 1));
		}
		else
		{
			CaptionString = FString::Printf(TEXT("%s [VV]"), *GetFilterNameAsString().Replace(TEXT("Log"), TEXT(""), ESearchCase::CaseSensitive));
		}

		CachedCaptionString = FText::FromString(CaptionString);
		LastVerbosity = (ELogVerbosity::Type)CategoryFilter.LogVerbosity;
	}
	return CachedCaptionString;
}

FText SFilterWidget::GetTooltipString() const
{
	if (bWasEnabledLastTime != IsEnabled())
	{
		FCategoryFilter& CategoryFilter = FVisualLoggerFilters::Get().GetCategoryByName(GetFilterNameAsString());
		const FString VerbosityStr = FOutputDeviceHelper::VerbosityToString((ELogVerbosity::Type)CategoryFilter.LogVerbosity);

		CachedTooltipString = FText::FromString(
			IsEnabled() ?
			FString::Printf(TEXT("Enabled '%s' category for '%s' verbosity and lower\nRight click to change verbosity"), *GetFilterNameAsString(), *VerbosityStr)
			:
			FString::Printf(TEXT("Disabled '%s' category"), *GetFilterNameAsString())
			);

		bWasEnabledLastTime = IsEnabled();
	}

	return CachedTooltipString;
}

TSharedRef<SWidget> SFilterWidget::GetRightClickMenuContent()
{
	FMenuBuilder MenuBuilder(true, NULL);

	if (IsEnabled())
	{
		FText FiletNameAsText = FText::FromString(GetFilterNameAsString());
		MenuBuilder.BeginSection("VerbositySelection", LOCTEXT("VerbositySelection", "Current verbosity selection"));
		{
			for (int32 Index = ELogVerbosity::NoLogging + 1; Index <= ELogVerbosity::VeryVerbose; Index++)
			{
				const FString VerbosityStr = FOutputDeviceHelper::VerbosityToString((ELogVerbosity::Type)Index);
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("UseVerbosity", "Use: {0}"), FText::FromString(VerbosityStr)),
					LOCTEXT("UseVerbosityTooltip", "Applay verbosity to selected flter."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SFilterWidget::SetVerbosityFilter, Index))
					);
			}
		}
		MenuBuilder.EndSection();
	}
	MenuBuilder.BeginSection("FilterAction", LOCTEXT("FilterAction", "Context actions"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DisableAllButThis", "Disable all but this"),
			LOCTEXT("HideAllButThisTooltip", "Disable all other categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SFilterWidget::DisableAllButThis))
			);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EnableAll", "Enable all categories"),
			LOCTEXT("EnableAllTooltip", "Enable all categories"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SFilterWidget::EnableAllCategories))
			);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SFilterWidget::IsEnabled() const 
{ 
	FCategoryFilter& CategoryFilter = FVisualLoggerFilters::Get().GetCategoryByName(GetFilterNameAsString());
	return CategoryFilter.Enabled;
}

void SFilterWidget::SetEnabled(bool InEnabled)
{
	FCategoryFilter& CategoryFilter = FVisualLoggerFilters::Get().GetCategoryByName(GetFilterNameAsString());
	if (InEnabled != CategoryFilter.Enabled)
	{
		CategoryFilter.Enabled = InEnabled;
		OnFilterChanged.ExecuteIfBound();
	}
}

void SFilterWidget::FilterToggled(ECheckBoxState NewState)
{
	SetEnabled(NewState == ECheckBoxState::Checked);
}

void SFilterWidget::SetVerbosityFilter(int32 SelectedVerbosityIndex)
{
	FCategoryFilter& CategoryFilter = FVisualLoggerFilters::Get().GetCategoryByName(GetFilterNameAsString());
	CategoryFilter.LogVerbosity = (ELogVerbosity::Type)SelectedVerbosityIndex;
	OnFilterChanged.ExecuteIfBound();
}

void SFilterWidget::DisableAllButThis()
{
	FVisualLoggerFilters::Get().DeactivateAllButThis(GetFilterNameAsString());
	OnFilterChanged.ExecuteIfBound();
}

void SFilterWidget::EnableAllCategories()
{
	FVisualLoggerFilters::Get().EnableAllCategories();
	OnFilterChanged.ExecuteIfBound();
}


#undef LOCTEXT_NAMESPACE
