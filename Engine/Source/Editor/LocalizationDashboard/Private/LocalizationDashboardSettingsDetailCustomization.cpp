// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDashboardSettingsDetailCustomization.h"
#include "Styling/SlateTypes.h"
#include "ILocalizationServiceProvider.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "LocalizationSettings.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "ILocalizationServiceModule.h"
#include "ILocalizationDashboardModule.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "LocalizationDashboard"

FLocalizationDashboardSettingsDetailCustomization::FLocalizationDashboardSettingsDetailCustomization()
	: DetailLayoutBuilder(nullptr)
	, ServiceProviderCategoryBuilder(nullptr)
{
	TArray<ILocalizationServiceProvider*> ActualProviders = ILocalizationDashboardModule::Get().GetLocalizationServiceProviders();
	for (ILocalizationServiceProvider* ActualProvider : ActualProviders)
	{
		TSharedPtr<FLocalizationServiceProviderWrapper> Provider = MakeShareable(new FLocalizationServiceProviderWrapper(ActualProvider));
		Providers.Add(Provider);
	}
}

void FLocalizationDashboardSettingsDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailLayoutBuilder = &DetailBuilder;

	// Localization Service Provider
	{
		ServiceProviderCategoryBuilder = &DetailLayoutBuilder->EditCategory("ServiceProvider", LOCTEXT("LocalizationServiceProvider", "Localization Service Provider"), ECategoryPriority::Important);
		FDetailWidgetRow& DetailWidgetRow = ServiceProviderCategoryBuilder->AddCustomRow(LOCTEXT("SelectedLocalizationServiceProvider", "Selected Localization Service Provider"));

		int32 CurrentlySelectedProviderIndex = 0;

		for (int ProviderIndex = 0; ProviderIndex < Providers.Num(); ++ProviderIndex)
		{
			FName CurrentlySelectedProviderName = ILocalizationServiceModule::Get().GetProvider().GetName();
			if (Providers[ProviderIndex].IsValid() && Providers[ProviderIndex]->Provider && Providers[ProviderIndex]->Provider->GetName() == CurrentlySelectedProviderName)
			{
				CurrentlySelectedProviderIndex = ProviderIndex;
				break;
			}
		}

		DetailWidgetRow.NameContent()
			[
				SNew(STextBlock)
				.Font(DetailLayoutBuilder->GetDetailFont())
				.Text(LOCTEXT("LocalizationServiceProvider", "Localization Service Provider"))
			];
		DetailWidgetRow.ValueContent()
			.MinDesiredWidth(TOptional<float>())
			.MaxDesiredWidth(TOptional<float>())
			[
				SNew(SComboBox< TSharedPtr<FLocalizationServiceProviderWrapper>>)
				.OptionsSource(&(Providers))
				.OnSelectionChanged(this, &FLocalizationDashboardSettingsDetailCustomization::ServiceProviderComboBox_OnSelectionChanged)
				.OnGenerateWidget(this, &FLocalizationDashboardSettingsDetailCustomization::ServiceProviderComboBox_OnGenerateWidget)
				.InitiallySelectedItem(Providers[CurrentlySelectedProviderIndex])
				.Content()
				[
					SNew(STextBlock)
					.Font(DetailLayoutBuilder->GetDetailFont())
					.Text_Lambda([]()
					{
						return ILocalizationServiceModule::Get().GetProvider().GetDisplayName();
					})
				]
			];

		const ILocalizationServiceProvider& LSP = ILocalizationServiceModule::Get().GetProvider();
		if (ServiceProviderCategoryBuilder != nullptr)
		{
			LSP.CustomizeSettingsDetails(*ServiceProviderCategoryBuilder);
		}
	}

	// Source Control
	{
		IDetailCategoryBuilder& SourceControlCategoryBuilder = DetailLayoutBuilder->EditCategory("SourceControl", LOCTEXT("SourceControl", "Source Control"), ECategoryPriority::Important);

		// Enable Source Control
		{
			SourceControlCategoryBuilder.AddCustomRow(LOCTEXT("EnableSourceControl", "Enable Source Control"))
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailLayoutBuilder->GetDetailFont())
					.Text(LOCTEXT("EnableSourceControl", "Enable Source Control"))
					.ToolTipText(LOCTEXT("EnableSourceControlToolTip", "Should we use source control when running the localization commandlets. This will optionally pass \"-EnableSCC\" to the commandlet."))
				]
				.ValueContent()
				.MinDesiredWidth(TOptional<float>())
				.MaxDesiredWidth(TOptional<float>())
				[
					SNew(SCheckBox)
					.ToolTipText(LOCTEXT("EnableSourceControlToolTip", "Should we use source control when running the localization commandlets. This will optionally pass \"-EnableSCC\" to the commandlet."))
					.IsEnabled_Lambda([]() -> bool
					{
						return FLocalizationSourceControlSettings::IsSourceControlAvailable();
					})
					.IsChecked_Lambda([]() -> ECheckBoxState
					{
						return FLocalizationSourceControlSettings::IsSourceControlEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([](ECheckBoxState InCheckState)
					{
						FLocalizationSourceControlSettings::SetSourceControlEnabled(InCheckState == ECheckBoxState::Checked);
					})
				];
		}

		// Enable Auto Submit
		{
			SourceControlCategoryBuilder.AddCustomRow(LOCTEXT("EnableSourceControlAutoSubmit", "Enable Auto Submit"))
				.NameContent()
				[
					SNew(STextBlock)
					.Font(DetailLayoutBuilder->GetDetailFont())
					.Text(LOCTEXT("EnableSourceControlAutoSubmit", "Enable Auto Submit"))
					.ToolTipText(LOCTEXT("EnableSourceControlAutoSubmitToolTip", "Should we automatically submit changed files after running the commandlet. This will optionally pass \"-DisableSCCSubmit\" to the commandlet."))
				]
				.ValueContent()
				.MinDesiredWidth(TOptional<float>())
				.MaxDesiredWidth(TOptional<float>())
				[
					SNew(SCheckBox)
					.ToolTipText(LOCTEXT("EnableSourceControlAutoSubmitToolTip", "Should we automatically submit changed files after running the commandlet. This will optionally pass \"-DisableSCCSubmit\" to the commandlet."))
					.IsEnabled_Lambda([]() -> bool
					{
						return FLocalizationSourceControlSettings::IsSourceControlAvailable();
					})
					.IsChecked_Lambda([]() -> ECheckBoxState
					{
						return FLocalizationSourceControlSettings::IsSourceControlAutoSubmitEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([](ECheckBoxState InCheckState)
					{
						FLocalizationSourceControlSettings::SetSourceControlAutoSubmitEnabled(InCheckState == ECheckBoxState::Checked);
					})
				];
		}
	}
}

FText FLocalizationDashboardSettingsDetailCustomization::GetCurrentServiceProviderDisplayName() const
{
	const ILocalizationServiceProvider& LSP = ILocalizationServiceModule::Get().GetProvider();
	return LSP.GetDisplayName();
}

TSharedRef<SWidget> FLocalizationDashboardSettingsDetailCustomization::ServiceProviderComboBox_OnGenerateWidget(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper) const
{
	ILocalizationServiceProvider* LSP = LSPWrapper.IsValid() ? LSPWrapper->Provider : nullptr;

	return	SNew(STextBlock)
		.Text(LSP ? LSP->GetDisplayName() : LOCTEXT("NoServiceProviderName", "None"));
}

void FLocalizationDashboardSettingsDetailCustomization::ServiceProviderComboBox_OnSelectionChanged(TSharedPtr<FLocalizationServiceProviderWrapper> LSPWrapper, ESelectInfo::Type SelectInfo)
{
	ILocalizationServiceProvider* LSP = LSPWrapper.IsValid() ? LSPWrapper->Provider : nullptr;

	FName ServiceProviderName = LSP ? LSP->GetName() : FName(TEXT("None"));
	ILocalizationServiceModule::Get().SetProvider(ServiceProviderName);

	if (LSP && ServiceProviderCategoryBuilder)
	{
		LSP->CustomizeSettingsDetails(*ServiceProviderCategoryBuilder);
	}
	DetailLayoutBuilder->ForceRefreshDetails();
}

#undef LOCTEXT_NAMESPACE
