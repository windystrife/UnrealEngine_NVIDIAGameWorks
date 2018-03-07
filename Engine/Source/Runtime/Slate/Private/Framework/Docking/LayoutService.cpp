// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Docking/LayoutService.h"
#include "Misc/ConfigCacheIni.h"


const TCHAR* EditorLayoutsSectionName = TEXT("EditorLayouts");


const FString& FLayoutSaveRestore::GetAdditionalLayoutConfigIni()
{
	static const FString IniSectionAdditionalConfig = TEXT("SlateAdditionalLayoutConfig");
	return IniSectionAdditionalConfig;
}


void FLayoutSaveRestore::SaveToConfig( FString ConfigFileName, const TSharedRef<FTabManager::FLayout>& LayoutToSave )
{
	const FString LayoutAsString = FLayoutSaveRestore::PrepareLayoutStringForIni( LayoutToSave->ToString() );

	GConfig->SetString(EditorLayoutsSectionName, *LayoutToSave->GetLayoutName().ToString(), *LayoutAsString, ConfigFileName );
}


TSharedRef<FTabManager::FLayout> FLayoutSaveRestore::LoadFromConfig( const FString& ConfigFileName, const TSharedRef<FTabManager::FLayout>& DefaultLayout )
{
	const FName LayoutName = DefaultLayout->GetLayoutName();
	FString UserLayoutString;
	if ( GConfig->GetString(EditorLayoutsSectionName, *LayoutName.ToString(), UserLayoutString, ConfigFileName ) && !UserLayoutString.IsEmpty() )
	{
		TSharedPtr<FTabManager::FLayout> UserLayout = FTabManager::FLayout::NewFromString( FLayoutSaveRestore::GetLayoutStringFromIni( UserLayoutString ));
		if ( UserLayout.IsValid() && UserLayout->GetPrimaryArea().IsValid() )
		{
			return UserLayout.ToSharedRef();
		}
	}

	return DefaultLayout;
}


void FLayoutSaveRestore::MigrateConfig( const FString& OldConfigFileName, const FString& NewConfigFileName )
{
	TArray<FString> OldSectionStrings;

	// check whether any layout configuration needs to be migrated
	if (!GConfig->GetSection(EditorLayoutsSectionName, OldSectionStrings, OldConfigFileName) || (OldSectionStrings.Num() == 0))
	{
		return;
	}

	TArray<FString> NewSectionStrings;

	// migrate old configuration if a new layout configuration does not yet exist
	if (!GConfig->GetSection(EditorLayoutsSectionName, NewSectionStrings, NewConfigFileName) || (NewSectionStrings.Num() == 0))
	{
		FString Key, Value;

		for (auto SectionString : OldSectionStrings)
		{
			if (SectionString.Split(TEXT("="), &Key, &Value))
			{
				GConfig->SetString(EditorLayoutsSectionName, *Key, *Value, NewConfigFileName);
			}
		}
	}

	// remove old configuration
	GConfig->EmptySection(EditorLayoutsSectionName, OldConfigFileName);
	GConfig->Flush(false, OldConfigFileName);
	GConfig->Flush(false, NewConfigFileName);
}


FString FLayoutSaveRestore::PrepareLayoutStringForIni(const FString& LayoutString)
{
	// Have to store braces as parentheses due to braces causing ini issues
	return LayoutString.Replace(TEXT("{"), TEXT("(")).Replace(TEXT("}"), TEXT(")")).Replace(LINE_TERMINATOR, TEXT("\\") LINE_TERMINATOR);
}


FString FLayoutSaveRestore::GetLayoutStringFromIni(const FString& LayoutString)
{
	// Revert parenthesis to braces, from ini readable to Json readable
	return LayoutString.Replace(TEXT("("), TEXT("{")).Replace(TEXT(")"), TEXT("}")).Replace(TEXT("\\") LINE_TERMINATOR, LINE_TERMINATOR);
}
