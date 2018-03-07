// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SettingsCategory.h"
#include "SettingsSection.h"


/* FSettingsCategory structors
 *****************************************************************************/

FSettingsCategory::FSettingsCategory( const FName& InName )
	: Name(InName)
{ }


/* FSettingsCategory interface
 *****************************************************************************/

ISettingsSectionRef FSettingsCategory::AddSection( const FName& SectionName, const FText& InDisplayName, const FText& InDescription, const TWeakObjectPtr<UObject>& SettingsObject )
{
	TSharedPtr<FSettingsSection>& Section = Sections.FindOrAdd(SectionName);

	if (!Section.IsValid() || (Section->GetSettingsObject() != SettingsObject) || Section->GetCustomWidget().IsValid())
	{
		Section = MakeShareable(new FSettingsSection(AsShared(), SectionName, InDisplayName, InDescription, SettingsObject));
	}

	return Section.ToSharedRef();
}


ISettingsSectionRef FSettingsCategory::AddSection( const FName& SectionName, const FText& InDisplayName, const FText& InDescription, const TSharedRef<SWidget>& CustomWidget )
{
	TSharedPtr<FSettingsSection>& Section = Sections.FindOrAdd(SectionName);

	if (!Section.IsValid() || (Section->GetSettingsObject() != nullptr) || (Section->GetCustomWidget().Pin() != CustomWidget))
	{
		Section = MakeShareable(new FSettingsSection(AsShared(), SectionName, InDisplayName, InDescription, CustomWidget));
	}

	return Section.ToSharedRef();
}


void FSettingsCategory::Describe( const FText& InDisplayName, const FText& InDescription )
{
	Description = InDescription;
	DisplayName = InDisplayName;
}


void FSettingsCategory::RemoveSection( const FName& SectionName )
{
	Sections.Remove(SectionName);
}


/* ISettingsCategory interface
 *****************************************************************************/

ISettingsSectionPtr FSettingsCategory::GetSection( const FName& SectionName ) const
{
	return Sections.FindRef(SectionName);
}

int32 FSettingsCategory::GetSections( TArray<ISettingsSectionPtr>& OutSections ) const
{
	OutSections.Empty(Sections.Num());

	for (TMap<FName, TSharedPtr<FSettingsSection> >::TConstIterator It(Sections); It; ++It)
	{
		OutSections.Add(It.Value());
	}

	return OutSections.Num();
}
