// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsSettingsCustomization.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsModule.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "FGameplayTagsSettingsCustomization"

TSharedRef<IDetailCustomization> FGameplayTagsSettingsCustomization::MakeInstance()
{
	return MakeShareable( new FGameplayTagsSettingsCustomization() );
}

FGameplayTagsSettingsCustomization::FGameplayTagsSettingsCustomization()
{
	IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &FGameplayTagsSettingsCustomization::OnTagTreeChanged);
}

FGameplayTagsSettingsCustomization::~FGameplayTagsSettingsCustomization()
{
	IGameplayTagsModule::OnTagSettingsChanged.RemoveAll(this);
}

void FGameplayTagsSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	IDetailCategoryBuilder& GameplayTagsCategory = DetailLayout.EditCategory("GameplayTags");
	{
		TArray<TSharedRef<IPropertyHandle>> GameplayTagsProperties;
		GameplayTagsCategory.GetDefaultProperties(GameplayTagsProperties, true, false);

		TSharedPtr<IPropertyHandle> TagListProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UGameplayTagsList, GameplayTagList));
		TagListProperty->MarkHiddenByCustomization();

		for (TSharedPtr<IPropertyHandle> Property : GameplayTagsProperties)
		{
			if (Property->GetProperty() != TagListProperty->GetProperty())
			{
				GameplayTagsCategory.AddProperty(Property);
			}
			else
			{
				// Create a custom widget for the tag list

				GameplayTagsCategory.AddCustomRow(TagListProperty->GetPropertyDisplayName(), false)
				.NameContent()
				[
					TagListProperty->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(MaxPropertyWidth)
				[
					SAssignNew(TagWidget, SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>())
					.Filter(TEXT(""))
					.MultiSelect(false)
					.GameplayTagUIMode(EGameplayTagUIMode::ManagementMode)
					.MaxHeight(MaxPropertyHeight)
					.OnTagChanged(this, &FGameplayTagsSettingsCustomization::OnTagChanged)
				];
			}
		}
	}
}

void FGameplayTagsSettingsCustomization::OnTagChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshTags();
	}
}

void FGameplayTagsSettingsCustomization::OnTagTreeChanged()
{
	if (TagWidget.IsValid())
	{
		TagWidget->RefreshOnNextTick();
	}
}

#undef LOCTEXT_NAMESPACE
