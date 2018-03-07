// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet/KismetInternationalizationLibrary.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "ConfigCacheIni.h"

#define LOCTEXT_NAMESPACE "Kismet"

bool UKismetInternationalizationLibrary::SetCurrentCulture(const FString& Culture, const bool SaveToConfig)
{
	if (FInternationalization::Get().SetCurrentCulture(Culture))
	{
		if (!GIsEditor && SaveToConfig)
		{
			GConfig->SetString(TEXT("Internationalization"), TEXT("Culture"), *Culture, GGameUserSettingsIni);
			GConfig->EmptySection(TEXT("Internationalization.AssetGroupCultures"), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		return true;
	}

	return false;
}

FString UKismetInternationalizationLibrary::GetCurrentCulture()
{
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

bool UKismetInternationalizationLibrary::SetCurrentLanguage(const FString& Culture, const bool SaveToConfig)
{
	if (FInternationalization::Get().SetCurrentLanguage(Culture))
	{
		if (!GIsEditor && SaveToConfig)
		{
			GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *Culture, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		return true;
	}

	return false;
}

FString UKismetInternationalizationLibrary::GetCurrentLanguage()
{
	return FInternationalization::Get().GetCurrentLanguage()->GetName();
}

bool UKismetInternationalizationLibrary::SetCurrentLocale(const FString& Culture, const bool SaveToConfig)
{
	if (FInternationalization::Get().SetCurrentLocale(Culture))
	{
		if (!GIsEditor && SaveToConfig)
		{
			GConfig->SetString(TEXT("Internationalization"), TEXT("Locale"), *Culture, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		return true;
	}

	return false;
}

FString UKismetInternationalizationLibrary::GetCurrentLocale()
{
	return FInternationalization::Get().GetCurrentLocale()->GetName();
}

bool UKismetInternationalizationLibrary::SetCurrentLanguageAndLocale(const FString& Culture, const bool SaveToConfig)
{
	if (FInternationalization::Get().SetCurrentLanguageAndLocale(Culture))
	{
		if (!GIsEditor && SaveToConfig)
		{
			GConfig->SetString(TEXT("Internationalization"), TEXT("Language"), *Culture, GGameUserSettingsIni);
			GConfig->SetString(TEXT("Internationalization"), TEXT("Locale"), *Culture, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		return true;
	}

	return false;
}

bool UKismetInternationalizationLibrary::SetCurrentAssetGroupCulture(const FName AssetGroup, const FString& Culture, const bool SaveToConfig)
{
	if (FInternationalization::Get().SetCurrentAssetGroupCulture(AssetGroup, Culture))
	{
		if (!GIsEditor && SaveToConfig)
		{
			if (FConfigSection* AssetGroupCulturesSection = GConfig->GetSectionPrivate(TEXT("Internationalization.AssetGroupCultures"), false, false, GGameUserSettingsIni))
			{
				AssetGroupCulturesSection->Remove(AssetGroup);
				AssetGroupCulturesSection->Add(AssetGroup, Culture);
			}
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		return true;
	}

	return false;
}

FString UKismetInternationalizationLibrary::GetCurrentAssetGroupCulture(const FName AssetGroup)
{
	return FInternationalization::Get().GetCurrentAssetGroupCulture(AssetGroup)->GetName();
}

void UKismetInternationalizationLibrary::ClearCurrentAssetGroupCulture(const FName AssetGroup, const bool SaveToConfig)
{
	FInternationalization::Get().ClearCurrentAssetGroupCulture(AssetGroup);

	if (!GIsEditor && SaveToConfig)
	{
		if (FConfigSection* AssetGroupCulturesSection = GConfig->GetSectionPrivate(TEXT("Internationalization.AssetGroupCultures"), false, false, GGameUserSettingsIni))
		{
			AssetGroupCulturesSection->Remove(AssetGroup);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

#undef LOCTEXT_NAMESPACE
