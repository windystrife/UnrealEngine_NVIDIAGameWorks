// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "TemplateProjectDefs.h"
#include "Internationalization/Culture.h"

namespace
{

FText GetLocalizedText(const TArray<FLocalizedTemplateString>& LocalizedStrings)
{
	const FString DefaultCultureISOLanguageName = "en";
	const FString CurrentCultureISOLanguageName = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
	FText FallbackText;

	for ( const FLocalizedTemplateString& LocalizedString : LocalizedStrings )
	{
		if ( LocalizedString.Language == CurrentCultureISOLanguageName )
		{
			return FText::FromString(LocalizedString.Text);
		}

		if ( LocalizedString.Language == DefaultCultureISOLanguageName )
		{
			FallbackText = FText::FromString(LocalizedString.Text);
		}
	}

	// Did we find an English fallback?
	if ( !FallbackText.IsEmpty() )
	{
		return FallbackText;
	}

	// We failed to find English, see if we have any translations available to use
	if ( LocalizedStrings.Num() )
	{
		return FText::FromString(LocalizedStrings[0].Text);
	}

	return FText();
}

}

FTemplateConfigValue::FTemplateConfigValue(const FString& InFile, const FString& InSection, const FString& InKey, const FString& InValue, bool InShouldReplaceExistingValue)
	: ConfigFile(InFile)
	, ConfigSection(InSection)
	, ConfigKey(InKey)
	, ConfigValue(InValue)
	, bShouldReplaceExistingValue(InShouldReplaceExistingValue)
{
}




UTemplateProjectDefs::UTemplateProjectDefs(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowProjectCreation = true;
	EditDetailLevelPreference = EFeaturePackDetailLevel::Standard;
}

void UTemplateProjectDefs::FixupStrings(const FString& TemplateName, const FString& ProjectName)
{
	for ( auto IgnoreIt = FoldersToIgnore.CreateIterator(); IgnoreIt; ++IgnoreIt )
	{
		FixString(*IgnoreIt, TemplateName, ProjectName);
	}

	for ( auto IgnoreIt = FilesToIgnore.CreateIterator(); IgnoreIt; ++IgnoreIt )
	{
		FixString(*IgnoreIt, TemplateName, ProjectName);
	}

	for ( auto RenameIt = FolderRenames.CreateIterator(); RenameIt; ++RenameIt )
	{
		FTemplateFolderRename& FolderRename = *RenameIt;
		FixString(FolderRename.From, TemplateName, ProjectName);
		FixString(FolderRename.To, TemplateName, ProjectName);
	}

	for ( auto ReplacementsIt = FilenameReplacements.CreateIterator(); ReplacementsIt; ++ReplacementsIt )
	{
		FTemplateReplacement& Replacement = *ReplacementsIt;
		FixString(Replacement.From, TemplateName, ProjectName);
		FixString(Replacement.To, TemplateName, ProjectName);
	}

	for ( auto ReplacementsIt = ReplacementsInFiles.CreateIterator(); ReplacementsIt; ++ReplacementsIt )
	{
		FTemplateReplacement& Replacement = *ReplacementsIt;
		FixString(Replacement.From, TemplateName, ProjectName);
		FixString(Replacement.To, TemplateName, ProjectName);
	}
}

FText UTemplateProjectDefs::GetDisplayNameText()
{
	return GetLocalizedText(LocalizedDisplayNames);
}

FText UTemplateProjectDefs::GetLocalizedDescription()
{
	return GetLocalizedText(LocalizedDescriptions);
}

void UTemplateProjectDefs::FixString(FString& InOutStringToFix, const FString& TemplateName, const FString& ProjectName)
{
	InOutStringToFix.ReplaceInline(TEXT("%TEMPLATENAME%"), *TemplateName, ESearchCase::CaseSensitive);
	InOutStringToFix.ReplaceInline(TEXT("%TEMPLATENAME_UPPERCASE%"), *TemplateName.ToUpper(), ESearchCase::CaseSensitive);
	InOutStringToFix.ReplaceInline(TEXT("%TEMPLATENAME_LOWERCASE%"), *TemplateName.ToLower(), ESearchCase::CaseSensitive);

	InOutStringToFix.ReplaceInline(TEXT("%PROJECTNAME%"), *ProjectName, ESearchCase::CaseSensitive);
	InOutStringToFix.ReplaceInline(TEXT("%PROJECTNAME_UPPERCASE%"), *ProjectName.ToUpper(), ESearchCase::CaseSensitive);
	InOutStringToFix.ReplaceInline(TEXT("%PROJECTNAME_LOWERCASE%"), *ProjectName.ToLower(), ESearchCase::CaseSensitive);
}
