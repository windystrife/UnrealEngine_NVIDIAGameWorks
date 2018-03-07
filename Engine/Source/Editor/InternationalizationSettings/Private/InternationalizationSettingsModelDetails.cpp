// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModelDetails.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "InternationalizationSettingsModel.h"
#include "EdGraph/EdGraphSchema.h"
#include "SCulturePicker.h"

#define LOCTEXT_NAMESPACE "InternationalizationSettingsModelDetails"

TSharedRef<IDetailCustomization> FInternationalizationSettingsModelDetails::MakeInstance()
{
	return MakeShareable(new FInternationalizationSettingsModelDetails);
}

namespace
{
	struct FLocalizedCulturesFlyweight
	{
		TArray<FCultureRef> LocalizedCulturesForEditor;
		TArray<FCultureRef> LocalizedCulturesForGame;

		FLocalizedCulturesFlyweight()
		{
			FInternationalization::Get().GetCulturesWithAvailableLocalization(FPaths::GetEditorLocalizationPaths(), LocalizedCulturesForEditor, true);
			FInternationalization::Get().GetCulturesWithAvailableLocalization(FPaths::GetGameLocalizationPaths(), LocalizedCulturesForGame, true);
		}
	};

	class SEditorLanguageComboButton : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SEditorLanguageComboButton){}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const TWeakObjectPtr<UInternationalizationSettingsModel>& InSettingsModel, const TSharedRef<FLocalizedCulturesFlyweight>& InLocalizedCulturesFlyweight)
		{
			SettingsModel = InSettingsModel;
			LocalizedCulturesFlyweight = InLocalizedCulturesFlyweight;

			ChildSlot
				[
					SAssignNew(EditorCultureComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SEditorLanguageComboButton::GetContentText)
					]
				];
			EditorCultureComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SEditorLanguageComboButton::GetMenuContent));
		}

	private:
		TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel;
		TSharedPtr<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight;
		TSharedPtr<SComboButton> EditorCultureComboButton;

	private:
		FText GetContentText() const
		{
			FCulturePtr EditorLanguage = FInternationalization::Get().GetCurrentLanguage();
			return EditorLanguage.IsValid() ? FText::FromString(EditorLanguage->GetNativeName()) : LOCTEXT("None", "None");
		}

		TSharedRef<SWidget> GetMenuContent()
		{
			FCulturePtr EditorLanguage = FInternationalization::Get().GetCurrentLanguage();

			const auto& OnSelectionChangedLambda = [=](FCulturePtr& SelectedCulture, ESelectInfo::Type SelectInfo)
			{
				if (SettingsModel.IsValid())
				{
					FInternationalization& I18N = FInternationalization::Get();
					const bool bSetLanguageAndLocale = I18N.GetCurrentLanguage() == I18N.GetCurrentLocale();

					SettingsModel->SetEditorLanguage(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					if (bSetLanguageAndLocale)
					{
						SettingsModel->SetEditorLocale(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					}

					if (SelectedCulture.IsValid())
					{
						if (bSetLanguageAndLocale)
						{
							I18N.SetCurrentLanguageAndLocale(SelectedCulture->GetName());
						}
						else
						{
							I18N.SetCurrentLanguage(SelectedCulture->GetName());
						}

						// Find all Schemas and force a visualization cache clear
						for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
						{
							UClass* CurrentClass = *ClassIt;

							if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
							{
								Schema->ForceVisualizationCacheClear();
							}
						}
					}
				}
				if (EditorCultureComboButton.IsValid())
				{
					EditorCultureComboButton->SetIsOpen(false);
				}
			};

			const auto& IsCulturePickableLambda = [=](FCulturePtr Culture) -> bool
			{
				TArray<FString> CultureNames = Culture->GetPrioritizedParentCultureNames();
				for (const FString& CultureName : CultureNames)
				{
					if (LocalizedCulturesFlyweight->LocalizedCulturesForEditor.Contains(Culture))
					{
						return true;
					}
				}
				return false;
			};

			const auto& CulturePicker = SNew(SCulturePicker)
				.InitialSelection(EditorLanguage)
				.OnSelectionChanged_Lambda(OnSelectionChangedLambda)
				.IsCulturePickable_Lambda(IsCulturePickableLambda)
				.DisplayNameFormat(SCulturePicker::ECultureDisplayFormat::ActiveAndNativeCultureDisplayName);

			return SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.WidthOverride(300.0f)
				[
					CulturePicker
				];
		}
	};

	class SEditorLocaleComboButton : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SEditorLocaleComboButton){}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const TWeakObjectPtr<UInternationalizationSettingsModel>& InSettingsModel, const TSharedRef<FLocalizedCulturesFlyweight>& InLocalizedCulturesFlyweight)
		{
			SettingsModel = InSettingsModel;
			LocalizedCulturesFlyweight = InLocalizedCulturesFlyweight;

			ChildSlot
				[
					SAssignNew(EditorCultureComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SEditorLocaleComboButton::GetContentText)
					]
				];
			EditorCultureComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SEditorLocaleComboButton::GetMenuContent));
		}

	private:
		TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel;
		TSharedPtr<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight;
		TSharedPtr<SComboButton> EditorCultureComboButton;

	private:
		FText GetContentText() const
		{
			FCulturePtr EditorLocale = FInternationalization::Get().GetCurrentLocale();
			return EditorLocale.IsValid() ? FText::FromString(EditorLocale->GetNativeName()) : LOCTEXT("None", "None");
		}

		TSharedRef<SWidget> GetMenuContent()
		{
			FCulturePtr EditorLocale = FInternationalization::Get().GetCurrentLocale();

			const auto& OnSelectionChangedLambda = [=](FCulturePtr& SelectedCulture, ESelectInfo::Type SelectInfo)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->SetEditorLocale(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					if (SelectedCulture.IsValid())
					{
						FInternationalization& I18N = FInternationalization::Get();
						I18N.SetCurrentLocale(SelectedCulture->GetName());

						// Find all Schemas and force a visualization cache clear
						for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
						{
							UClass* CurrentClass = *ClassIt;

							if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
							{
								Schema->ForceVisualizationCacheClear();
							}
						}
					}
				}
				if (EditorCultureComboButton.IsValid())
				{
					EditorCultureComboButton->SetIsOpen(false);
				}
			};

			const auto& IsCulturePickableLambda = [=](FCulturePtr Culture) -> bool
			{
				TArray<FString> CultureNames = Culture->GetPrioritizedParentCultureNames();
				for (const FString& CultureName : CultureNames)
				{
					if (LocalizedCulturesFlyweight->LocalizedCulturesForEditor.Contains(Culture))
					{
						return true;
					}
				}
				return false;
			};

			const auto& CulturePicker = SNew(SCulturePicker)
				.InitialSelection(EditorLocale)
				.OnSelectionChanged_Lambda(OnSelectionChangedLambda)
				.IsCulturePickable_Lambda(IsCulturePickableLambda)
				.DisplayNameFormat(SCulturePicker::ECultureDisplayFormat::ActiveAndNativeCultureDisplayName);

			return SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.WidthOverride(300.0f)
				[
					CulturePicker
				];
		}
	};

	class SPreviewGameLanguageComboButton : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SPreviewGameLanguageComboButton){}
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs, const TWeakObjectPtr<UInternationalizationSettingsModel>& InSettingsModel, const TSharedRef<FLocalizedCulturesFlyweight>& InLocalizedCulturesFlyweight)
		{
			SettingsModel = InSettingsModel;
			LocalizedCulturesFlyweight = InLocalizedCulturesFlyweight;

			ChildSlot
				[
					SAssignNew(PreviewGameCultureComboButton, SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &SPreviewGameLanguageComboButton::GetContentText)
					]
				];
			PreviewGameCultureComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SPreviewGameLanguageComboButton::GetMenuContent));
		}

	private:
		TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel;
		TSharedPtr<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight;
		TSharedPtr<SComboButton> PreviewGameCultureComboButton;

	private:
		FText GetContentText() const
		{
			bool IsConfigured = false;
			FString PreviewGameLanguage;
			if (SettingsModel.IsValid())
			{
				IsConfigured = SettingsModel->GetPreviewGameLanguage(PreviewGameLanguage);
			}
			FCulturePtr Culture;
			if (!PreviewGameLanguage.IsEmpty())
			{
				FInternationalization& I18N = FInternationalization::Get();
				Culture = I18N.GetCulture(PreviewGameLanguage);
			}
			return Culture.IsValid() ? FText::FromString(Culture->GetDisplayName()) : LOCTEXT("None", "None");
		}

		TSharedRef<SWidget> GetMenuContent()
		{
			FCulturePtr PreviewGameCulture;
			{
				bool IsConfigured = false;
				FString PreviewGameLanguage;
				if (SettingsModel.IsValid())
				{
					IsConfigured = SettingsModel->GetPreviewGameLanguage(PreviewGameLanguage);
				}
				if (!PreviewGameLanguage.IsEmpty())
				{
					FInternationalization& I18N = FInternationalization::Get();
					PreviewGameCulture = I18N.GetCulture(PreviewGameLanguage);
				}
			}

			const auto& CulturePickerSelectLambda = [=](FCulturePtr& SelectedCulture, ESelectInfo::Type SelectInfo)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->SetPreviewGameLanguage(SelectedCulture.IsValid() ? SelectedCulture->GetName() : TEXT(""));
					FTextLocalizationManager::Get().RefreshResources();
				}
				if (PreviewGameCultureComboButton.IsValid())
				{
					PreviewGameCultureComboButton->SetIsOpen(false);
				}
			};
			const auto& CulturePickerIsPickableLambda = [=](FCulturePtr Culture) -> bool
			{
				TArray<FString> CultureNames = Culture->GetPrioritizedParentCultureNames();
				for (const FString& CultureName : CultureNames)
				{
					if (LocalizedCulturesFlyweight->LocalizedCulturesForGame.Contains(Culture))
					{
						return true;
					}
				}
				return false;
			};
			return SNew(SBox)
				.MaxDesiredHeight(300.0f)
				.WidthOverride(300.0f)
				[
					SNew(SCulturePicker)
					.InitialSelection(PreviewGameCulture)
					.OnSelectionChanged_Lambda(CulturePickerSelectLambda)
					.IsCulturePickable_Lambda(CulturePickerIsPickableLambda)
					.CanSelectNone(true)
				];
		}
	};
}

void FInternationalizationSettingsModelDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TWeakObjectPtr<UInternationalizationSettingsModel> SettingsModel = [&]()
	{
		TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
		DetailLayout.GetObjectsBeingCustomized(ObjectsBeingCustomized);
		check(ObjectsBeingCustomized.Num() == 1);
		return Cast<UInternationalizationSettingsModel>(ObjectsBeingCustomized.Top().Get());
	}();

	IDetailCategoryBuilder& DetailCategoryBuilder = DetailLayout.EditCategory("Internationalization", LOCTEXT("InternationalizationCategory", "Internationalization"));

	const TSharedRef<FLocalizedCulturesFlyweight> LocalizedCulturesFlyweight = MakeShareable(new FLocalizedCulturesFlyweight());

	// Editor Language Setting
	const FText EditorLanguageSettingDisplayName = LOCTEXT("EditorLanguageSettingDisplayName", "Editor Language");
	const FText EditorLanguageSettingToolTip = LOCTEXT("EditorLanguageSettingToolTip", "The language that the Editor should use for localization (the display language).");
	DetailCategoryBuilder.AddCustomRow(EditorLanguageSettingDisplayName)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(EditorLanguageSettingDisplayName)
			.ToolTipText(EditorLanguageSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditorLanguageComboButton, SettingsModel, LocalizedCulturesFlyweight)
		];

	// Editor Locale Setting
	const FText EditorLocaleSettingDisplayName = LOCTEXT("EditorLocaleSettingDisplayName", "Editor Locale");
	const FText EditorLocaleSettingToolTip = LOCTEXT("EditorLocaleSettingToolTip", "The locale that the Editor should use for internationalization (numbers, dates, times, etc).");
	DetailCategoryBuilder.AddCustomRow(EditorLocaleSettingDisplayName)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(EditorLocaleSettingDisplayName)
			.ToolTipText(EditorLocaleSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditorLocaleComboButton, SettingsModel, LocalizedCulturesFlyweight)
		];

	// Preview Game Language Setting
	const FText PreviewGameLanguageSettingDisplayName = LOCTEXT("PreviewGameLanguageSettingDisplayName", "Preview Game Language");
	const FText PreviewGameLanguageSettingToolTip = LOCTEXT("PreviewGameLanguageSettingToolTip", "The language to preview game localization in");
	DetailCategoryBuilder.AddCustomRow(PreviewGameLanguageSettingDisplayName)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(PreviewGameLanguageSettingDisplayName)
			.ToolTipText(PreviewGameLanguageSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SPreviewGameLanguageComboButton, SettingsModel, LocalizedCulturesFlyweight)
		];

	// Localized Field Names
	const FText FieldNamesSettingDisplayName = LOCTEXT("EditorFieldNamesLabel", "Use Localized Field Names");
	const FText FieldNamesSettingToolTip = LOCTEXT("EditorFieldNamesTooltip", "Toggle showing localized field names. NOTE: Requires restart to take effect.");
	DetailCategoryBuilder.AddCustomRow(FieldNamesSettingDisplayName)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FieldNamesSettingDisplayName)
			.ToolTipText(FieldNamesSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		]
		.ValueContent()
		.MaxDesiredWidth(300.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([=]()
			{
				return SettingsModel.IsValid() && SettingsModel->ShouldLoadLocalizedPropertyNames() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
				.ToolTipText(FieldNamesSettingToolTip)
				.OnCheckStateChanged_Lambda([=](ECheckBoxState State)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->ShouldLoadLocalizedPropertyNames(State == ECheckBoxState::Checked);
					FTextLocalizationManager::Get().RefreshResources();
				}
			})
		];

	// Localized Node and Pin Names
	const FText NodeAndPinsNamesSettingDisplayName = LOCTEXT("GraphEditorNodesAndPinsLocalized", "Use Localized Graph Editor Nodes and Pins");
	const FText NodeAndPinsNamesSettingToolTip = LOCTEXT("GraphEditorNodesAndPinsLocalized_Tooltip", "Toggle localized node and pin titles in all graph editors.");
	DetailCategoryBuilder.AddCustomRow(NodeAndPinsNamesSettingDisplayName)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NodeAndPinsNamesSettingDisplayName)
			.ToolTipText(NodeAndPinsNamesSettingToolTip)
			.Font(DetailLayout.GetDetailFont())
		]
		.ValueContent()
		.MaxDesiredWidth(300.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([=]()
			{
				return SettingsModel.IsValid() && SettingsModel->ShouldShowNodesAndPinsUnlocalized() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
			})
				.ToolTipText(NodeAndPinsNamesSettingToolTip)
				.OnCheckStateChanged_Lambda([=](ECheckBoxState State)
			{
				if (SettingsModel.IsValid())
				{
					SettingsModel->ShouldShowNodesAndPinsUnlocalized(State == ECheckBoxState::Unchecked);

					// Find all Schemas and force a visualization cache clear
					for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
					{
						UClass* CurrentClass = *ClassIt;

						if (UEdGraphSchema* Schema = Cast<UEdGraphSchema>(CurrentClass->GetDefaultObject()))
						{
							Schema->ForceVisualizationCacheClear();
						}
					}
				}
			})
		];
}

#undef LOCTEXT_NAMESPACE
