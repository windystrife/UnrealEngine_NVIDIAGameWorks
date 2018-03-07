// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagSearchFilter.h"
#include "Framework/Commands/UIAction.h"
#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "SGameplayTagWidget.h"


#define LOCTEXT_NAMESPACE "GameplayTagSearchFilter"

//////////////////////////////////////////////////////////////////////////
//

/** A filter that search for assets using a specific gameplay tag */
class FFrontendFilter_GameplayTags : public FFrontendFilter
{
public:
	FFrontendFilter_GameplayTags(TSharedPtr<FFrontendFilterCategory> InCategory)
		: FFrontendFilter(InCategory)
	{
		TagContainer = MakeShareable(new FGameplayTagContainer);

		EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(/*TagContainerOwner=*/ nullptr, TagContainer.Get()));
	}

	// FFrontendFilter implementation
	virtual FLinearColor GetColor() const override { return FLinearColor::Red; }
	virtual FString GetName() const override { return TEXT("GameplayTagFilter"); }
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTipText() const override;
	virtual void ModifyContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual void SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const override;
	virtual void LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) override;
	// End of FFrontendFilter implementation

	// IFilter implementation
	virtual bool PassesFilter(FAssetFilterType InItem) const override;
	// End of IFilter implementation

protected:
	// Container of selected search tags (the asset is shown if *any* of these match)
	TSharedPtr<FGameplayTagContainer> TagContainer;

	// Adaptor for the SGameplayTagWidget to edit our tag container
	TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;

protected:
	bool ProcessStruct(void* Data, UStruct* Struct) const;

	bool ProcessProperty(void* Data, UProperty* Prop) const;

	void OnTagWidgetChanged();
};

void FFrontendFilter_GameplayTags::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction Action;

	MenuBuilder.BeginSection(TEXT("ComparsionSection"), LOCTEXT("ComparisonSectionHeading", "Gameplay Tag(s) to search for"));

	TSharedRef<SWidget> TagWidget =
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(300)
		[
			SNew(SGameplayTagWidget, EditableContainers)
			.MultiSelect(true)
			.OnTagChanged_Raw(this, &FFrontendFilter_GameplayTags::OnTagWidgetChanged)
		];
 	MenuBuilder.AddWidget(TagWidget, FText::GetEmpty(), /*bNoIndent=*/ false);
}

FText FFrontendFilter_GameplayTags::GetDisplayName() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyGameplayTagDisplayName", "Gameplay Tags");
	}
	else
	{
		FString QueryString;

		int32 Count = 0;
		for (const FGameplayTag& Tag : *TagContainer.Get())
		{
			if (Count > 0)
			{
				QueryString += TEXT(" | ");
			}

			QueryString += Tag.ToString();
			++Count;
		}


		return FText::Format(LOCTEXT("GameplayTagListDisplayName", "Gameplay Tags ({0})"), FText::AsCultureInvariant(QueryString));
	}
}

FText FFrontendFilter_GameplayTags::GetToolTipText() const
{
	if (TagContainer->Num() == 0)
	{
		return LOCTEXT("AnyGameplayTagFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that contains a gameplay tag (right-click to choose tags).");
	}
	else
	{
		return LOCTEXT("GameplayTagFilterDisplayTooltip", "Search for any *loaded* Blueprint or asset that has a gameplay tag which matches any of the selected tags (right-click to choose tags).");
	}
}

void FFrontendFilter_GameplayTags::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	TArray<FString> TagStrings;
	TagStrings.Reserve(TagContainer->Num());
	for (const FGameplayTag& Tag : *TagContainer.Get())
	{
		TagStrings.Add(Tag.GetTagName().ToString());
	}

	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".Tags")), TagStrings, IniFilename);
}

void FFrontendFilter_GameplayTags::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	TArray<FString> TagStrings;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".Tags")), /*out*/ TagStrings, IniFilename);

	TagContainer->Reset();
	for (const FString& TagString : TagStrings)
	{
		FGameplayTag NewTag = Manager.RequestGameplayTag(*TagString, /*bErrorIfNotFound=*/ false);
		if (NewTag.IsValid())
		{
			TagContainer->AddTag(NewTag);
		}
	}
}

void FFrontendFilter_GameplayTags::OnTagWidgetChanged()
{
	BroadcastChangedEvent();
}

bool FFrontendFilter_GameplayTags::ProcessStruct(void* Data, UStruct* Struct) const
{
	for (TFieldIterator<UProperty> PropIt(Struct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		UProperty* Prop = *PropIt;

		if (ProcessProperty(Data, Prop))
		{
			return true;
		}
	}

	return false;
}

bool FFrontendFilter_GameplayTags::ProcessProperty(void* Data, UProperty* Prop) const
{
	void* InnerData = Prop->ContainerPtrToValuePtr<void>(Data);

	if (UStructProperty* StructProperty = Cast<UStructProperty>(Prop))
	{
		if (StructProperty->Struct == FGameplayTag::StaticStruct())
		{
			FGameplayTag& ThisTag = *static_cast<FGameplayTag*>(InnerData);

			const bool bAnyTagIsOK = TagContainer->Num() == 0;
			const bool bPassesTagSearch = bAnyTagIsOK || ThisTag.MatchesAny(*TagContainer);

			return bPassesTagSearch;
		}
		else
		{
			return ProcessStruct(InnerData, StructProperty->Struct);
		}
	}
	else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Prop))
	{
		FScriptArrayHelper ArrayHelper(ArrayProperty, InnerData);
		for (int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
		{
			void* ArrayData = ArrayHelper.GetRawPtr(ArrayIndex);

			if (ProcessProperty(ArrayData, ArrayProperty->Inner))
			{
				return true;
			}
		}
	}

	return false;
}

bool FFrontendFilter_GameplayTags::PassesFilter(FAssetFilterType InItem) const
{
	if (InItem.IsAssetLoaded())
	{
		if (UObject* Object = InItem.GetAsset())
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
			{
				return ProcessStruct(Blueprint->GeneratedClass->GetDefaultObject(), Blueprint->GeneratedClass);

				//@TODO: Check blueprint bytecode!
			}
			else if (UClass* Class = Cast<UClass>(Object))
			{
				return ProcessStruct(Class->GetDefaultObject(), Class);
			}
			else
			{
				return ProcessStruct(Object, Object->GetClass());
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
//

void UGameplayTagSearchFilter::AddFrontEndFilterExtensions(TSharedPtr<FFrontendFilterCategory> DefaultCategory, TArray< TSharedRef<class FFrontendFilter> >& InOutFilterList) const
{
	InOutFilterList.Add(MakeShareable(new FFrontendFilter_GameplayTags(DefaultCategory)));
}

#undef LOCTEXT_NAMESPACE
