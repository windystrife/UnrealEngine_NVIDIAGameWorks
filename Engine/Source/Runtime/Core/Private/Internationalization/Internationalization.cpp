// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Internationalization.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "Internationalization/TextCache.h"

#if UE_ENABLE_ICU
#include "Internationalization/ICUInternationalization.h"
#else
#include "LegacyInternationalization.h"
#endif

#define LOCTEXT_NAMESPACE "Internationalization"

FInternationalization* FInternationalization::Instance;

FInternationalization& FInternationalization::Get()
{
	if(!Instance)
	{
		Instance = new FInternationalization();
	}
	if(Instance && !Instance->IsInitialized())
	{
		Instance->Initialize();
	}
	return *Instance;
}

bool FInternationalization::IsAvailable()
{
	return Instance && Instance->IsInitialized();
}

void FInternationalization::TearDown()
{
	if (Instance && Instance->IsInitialized())
	{
		Instance->Terminate();
		FTextCache::Get().Flush();
	}
}

FText FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	return FTextCache::Get().FindOrCache(InTextLiteral, InNamespace, InKey);
}

bool FInternationalization::SetCurrentCulture(const FString& InCultureName)
{
	FCulturePtr NewCulture = Implementation->GetCulture(InCultureName);

	if (NewCulture.IsValid())
	{
		if (CurrentLanguage != NewCulture || CurrentLocale != NewCulture || CurrentAssetGroupCultures.Num() > 0)
		{
			CurrentLanguage = NewCulture;
			CurrentLocale = NewCulture;
			CurrentAssetGroupCultures.Reset();
	
			Implementation->HandleLanguageChanged(InCultureName);
	
			BroadcastCultureChanged();
		}
	}

	return CurrentLanguage == NewCulture && CurrentLocale == NewCulture && CurrentAssetGroupCultures.Num() == 0;
}

bool FInternationalization::SetCurrentLanguage(const FString& InCultureName)
{
	FCulturePtr NewCulture = Implementation->GetCulture(InCultureName);

	if (NewCulture.IsValid())
	{
		if (CurrentLanguage != NewCulture)
		{
			CurrentLanguage = NewCulture;

			Implementation->HandleLanguageChanged(InCultureName);

			BroadcastCultureChanged();
		}
	}

	return CurrentLanguage == NewCulture;
}

bool FInternationalization::SetCurrentLocale(const FString& InCultureName)
{
	FCulturePtr NewCulture = Implementation->GetCulture(InCultureName);

	if (NewCulture.IsValid())
	{
		if (CurrentLocale != NewCulture)
		{
			CurrentLocale = NewCulture;

			BroadcastCultureChanged();
		}
	}

	return CurrentLocale == NewCulture;
}

bool FInternationalization::SetCurrentLanguageAndLocale(const FString& InCultureName)
{
	FCulturePtr NewCulture = Implementation->GetCulture(InCultureName);

	if (NewCulture.IsValid())
	{
		if (CurrentLanguage != NewCulture || CurrentLocale != NewCulture)
		{
			CurrentLanguage = NewCulture;
			CurrentLocale = NewCulture;

			Implementation->HandleLanguageChanged(InCultureName);

			BroadcastCultureChanged();
		}
	}

	return CurrentLanguage == NewCulture && CurrentLocale == NewCulture;
}

bool FInternationalization::SetCurrentAssetGroupCulture(const FName& InAssetGroupName, const FString& InCultureName)
{
	FCulturePtr NewCulture = Implementation->GetCulture(InCultureName);

	if (NewCulture.IsValid())
	{
		TTuple<FName, FCulturePtr>* EntryToUpdate = CurrentAssetGroupCultures.FindByPredicate([InAssetGroupName](const TTuple<FName, FCulturePtr>& InCurrentAssetGroupCulturePair)
		{
			return InCurrentAssetGroupCulturePair.Key == InAssetGroupName;
		});

		bool bChangedCulture = false;
		if (EntryToUpdate)
		{
			if (EntryToUpdate->Value != NewCulture)
			{
				bChangedCulture = true;
				EntryToUpdate->Value = NewCulture;
			}
		}
		else
		{
			bChangedCulture = true;
			CurrentAssetGroupCultures.Add(MakeTuple(InAssetGroupName, NewCulture));
		}

		if (bChangedCulture)
		{
			BroadcastCultureChanged();
		}

		return true;
	}

	return false;
}

FCultureRef FInternationalization::GetCurrentAssetGroupCulture(const FName& InAssetGroupName) const
{
	for (const auto& CurrentAssetGroupCulturePair : CurrentAssetGroupCultures)
	{
		if (CurrentAssetGroupCulturePair.Key == InAssetGroupName)
		{
			return CurrentAssetGroupCulturePair.Value.ToSharedRef();
		}
	}
	return GetCurrentLanguage();
}

void FInternationalization::ClearCurrentAssetGroupCulture(const FName& InAssetGroupName)
{
	CurrentAssetGroupCultures.RemoveAll([InAssetGroupName](const TTuple<FName, FCulturePtr>& InCurrentAssetGroupCulturePair)
	{
		return InCurrentAssetGroupCulturePair.Key == InAssetGroupName;
	});
}

void FInternationalization::BackupCultureState(FCultureStateSnapshot& OutSnapshot) const
{
	OutSnapshot.Language = CurrentLanguage->GetName();
	OutSnapshot.Locale = CurrentLocale->GetName();

	OutSnapshot.AssetGroups.Reset(CurrentAssetGroupCultures.Num());
	for (const auto& CurrentAssetGroupCulturePair : CurrentAssetGroupCultures)
	{
		OutSnapshot.AssetGroups.Add(MakeTuple(CurrentAssetGroupCulturePair.Key, CurrentAssetGroupCulturePair.Value->GetName()));
	}
}

void FInternationalization::RestoreCultureState(const FCultureStateSnapshot& InSnapshot)
{
	bool bChangedCulture = false;

	// Apply the language
	if (!InSnapshot.Language.IsEmpty())
	{
		FCulturePtr NewCulture = Implementation->GetCulture(InSnapshot.Language);

		if (NewCulture.IsValid())
		{
			if (CurrentLanguage != NewCulture)
			{
				bChangedCulture = true;

				CurrentLanguage = NewCulture;

				Implementation->HandleLanguageChanged(InSnapshot.Language);
			}
		}
	}

	// Apply the locale
	if (!InSnapshot.Locale.IsEmpty())
	{
		FCulturePtr NewCulture = Implementation->GetCulture(InSnapshot.Locale);

		if (NewCulture.IsValid())
		{
			if (CurrentLocale != NewCulture)
			{
				bChangedCulture = true;

				CurrentLocale = NewCulture;
			}
		}
	}

	// Apply the asset groups
	bChangedCulture |= CurrentAssetGroupCultures.Num() > 0;
	CurrentAssetGroupCultures.Reset(InSnapshot.AssetGroups.Num());
	for (const auto& AssetGroupCultureNamePair : InSnapshot.AssetGroups)
	{
		FCulturePtr NewCulture = Implementation->GetCulture(AssetGroupCultureNamePair.Value);
		if (NewCulture.IsValid())
		{
			bChangedCulture = true;
			CurrentAssetGroupCultures.Add(MakeTuple(AssetGroupCultureNamePair.Key, NewCulture));
		}
	}

	if (bChangedCulture)
	{
		BroadcastCultureChanged();
	}
}

FCulturePtr FInternationalization::GetCulture(const FString& InCultureName)
{
	return Implementation->GetCulture(InCultureName);
}

void FInternationalization::Initialize()
{
	static bool IsInitializing = false;

	if(IsInitialized() || IsInitializing)
	{
		return;
	}
	struct FInitializingGuard
	{
		FInitializingGuard()	{IsInitializing = true;}
		~FInitializingGuard()	{IsInitializing = false;}
	} InitializingGuard;

	bIsInitialized = Implementation->Initialize();
}

void FInternationalization::Terminate()
{
	CurrentLanguage.Reset();
	CurrentLocale.Reset();
	CurrentAssetGroupCultures.Reset();

	DefaultLanguage.Reset();
	DefaultLocale.Reset();

	InvariantCulture.Reset();

	Implementation->Terminate();

	bIsInitialized = false;
	delete Instance;
	Instance = nullptr;
}

#if ENABLE_LOC_TESTING
FString& FInternationalization::Leetify(FString& SourceString)
{
	static const TCHAR LeetifyTextStartMarker = TEXT('\x2021');
	static const TCHAR LeetifyTextEndMarker = TEXT('\x2021');
	static const TCHAR LeetifyArgumentStartMarker = TEXT('\x00AB');
	static const TCHAR LeetifyArgumentEndMarker = TEXT('\x00BB');
	static const TCHAR SourceArgumentStartMarker = TEXT('{');
	static const TCHAR SourceArgumentEndMarker = TEXT('}');
	static const TCHAR SourceEscapeMarker = TEXT('`');

	auto LeetifyCharacter = [](const TCHAR InChar) -> TCHAR
	{
		switch (InChar)
		{
		case TEXT('A'): return TEXT('4');
		case TEXT('a'): return TEXT('@');
		case TEXT('B'): return TEXT('8');
		case TEXT('b'): return TEXT('8');
		case TEXT('E'): return TEXT('3');
		case TEXT('e'): return TEXT('3');
		case TEXT('G'): return TEXT('9');
		case TEXT('g'): return TEXT('9');
		case TEXT('I'): return TEXT('1');
		case TEXT('i'): return TEXT('!');
		case TEXT('O'): return TEXT('0');
		case TEXT('o'): return TEXT('0');
		case TEXT('S'): return TEXT('5');
		case TEXT('s'): return TEXT('$');
		case TEXT('T'): return TEXT('7');
		case TEXT('t'): return TEXT('7');
		case TEXT('Z'): return TEXT('2');
		case TEXT('z'): return TEXT('2');
		default:		return InChar;
		}
	};

	if (SourceString.IsEmpty() || (SourceString.Len() >= 2 && SourceString[0] == LeetifyTextStartMarker && SourceString[SourceString.Len() - 1] == LeetifyTextEndMarker))
	{
		// Already leetified
		return SourceString;
	}

	// We insert a start and end marker (+2), and format strings typically have <= 8 argument blocks which we'll wrap with a start and end marker (+16), so +18 should be a reasonable slack
	FString LeetifiedString;
	LeetifiedString.Reserve(SourceString.Len() + 18);

	// Inject the start marker
	LeetifiedString.AppendChar(LeetifyTextStartMarker);

	// Iterate and leetify each character in the source string, but don't change argument names as that will break formatting
	{
		bool bEscapeNextChar = false;

		const int32 SourceStringLen = SourceString.Len();
		for (int32 SourceCharIndex = 0; SourceCharIndex < SourceStringLen; ++SourceCharIndex)
		{
			const TCHAR SourceChar = SourceString[SourceCharIndex];

			if (!bEscapeNextChar && SourceChar == SourceArgumentStartMarker)
			{
				const TCHAR* RawSourceStringPtr = SourceString.GetCharArray().GetData();

				// Walk forward to find the end of this argument block to make sure we have a pair of tokens
				const TCHAR* ArgumentEndPtr = FCString::Strchr(RawSourceStringPtr + SourceCharIndex + 1, SourceArgumentEndMarker);
				if (ArgumentEndPtr)
				{
					const int32 ArgumentEndIndex = ArgumentEndPtr - RawSourceStringPtr;

					// Inject a marker before the argument block
					LeetifiedString.AppendChar(LeetifyArgumentStartMarker);

					// Copy the body of the argument, including the opening and closing tags
					check(ArgumentEndIndex >= SourceCharIndex);
					LeetifiedString.AppendChars(RawSourceStringPtr + SourceCharIndex, (ArgumentEndIndex - SourceCharIndex) + 1);

					// Inject a marker after the end of the argument block
					LeetifiedString.AppendChar(LeetifyArgumentEndMarker);

					// Move to the end of the argument we just parsed
					SourceCharIndex = ArgumentEndIndex;
					continue;
				}
			}

			if (SourceChar == SourceEscapeMarker)
			{
				bEscapeNextChar = !bEscapeNextChar;
			}
			else
			{
				bEscapeNextChar = false;
			}

			LeetifiedString.AppendChar(LeetifyCharacter(SourceChar));
		}
	}

	// Inject the end marker
	LeetifiedString.AppendChar(LeetifyTextEndMarker);

	SourceString = LeetifiedString;
	return SourceString;
}
#endif

void FInternationalization::LoadAllCultureData()
{
	Implementation->LoadAllCultureData();
}

void FInternationalization::GetCultureNames(TArray<FString>& CultureNames) const
{
	Implementation->GetCultureNames(CultureNames);
}

TArray<FString> FInternationalization::GetPrioritizedCultureNames(const FString& Name)
{
	return Implementation->GetPrioritizedCultureNames(Name);
}

void FInternationalization::GetCulturesWithAvailableLocalization(const TArray<FString>& InLocalizationPaths, TArray< FCultureRef >& OutAvailableCultures, const bool bIncludeDerivedCultures)
{
	OutAvailableCultures.Reset();

	TArray<FString> AllLocalizationFolders;
	IFileManager& FileManager = IFileManager::Get();
	for(const auto& LocalizationPath : InLocalizationPaths)
	{
		/* Visitor class used to enumerate directories of culture */
		class FCultureEnumeratorVistor : public IPlatformFile::FDirectoryVisitor
		{
		public:
			FCultureEnumeratorVistor( TArray<FString>& OutLocalizationFolders )
				: LocalizationFolders(OutLocalizationFolders)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if(bIsDirectory)
				{
					// UE localization resource folders use "en-US" style while ICU uses "en_US"
					const FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
					const FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
					LocalizationFolders.AddUnique(CanonicalName);
				}

				return true;
			}

			/** Array to fill with the names of the UE localization folders available at the given path */
			TArray<FString>& LocalizationFolders;
		};

		FCultureEnumeratorVistor CultureEnumeratorVistor(AllLocalizationFolders);
		FileManager.IterateDirectory(*LocalizationPath, CultureEnumeratorVistor);
	}

	// Find any cultures that are a partial match for those we have translations for.
	if(bIncludeDerivedCultures)
	{
		TArray<FString> CultureNames;
		GetCultureNames(CultureNames);
		for(const FString& CultureName : CultureNames)
		{
			FCulturePtr Culture = GetCulture(CultureName);
			if (Culture.IsValid())
			{
				TArray<FString> PrioritizedParentCultureNames = Culture->GetPrioritizedParentCultureNames();

				for (const FString& PrioritizedParentCultureName : PrioritizedParentCultureNames)
				{
					if(AllLocalizationFolders.Contains(PrioritizedParentCultureName))
					{
						OutAvailableCultures.AddUnique(Culture.ToSharedRef());
						break;
					}
				}
			}
		}
	}
	// Find any cultures that are a complete match for those we have translations for.
	else
	{
		for(const FString& LocalizationFolder : AllLocalizationFolders)
		{
			FCulturePtr Culture = GetCulture(LocalizationFolder);
			if(Culture.IsValid())
			{
				OutAvailableCultures.AddUnique(Culture.ToSharedRef());
			}
		}
	}

	// Remove any cultures that were explicitly disabled
	OutAvailableCultures.RemoveAll([&](const FCultureRef& InCulture) -> bool
	{
		return Implementation->IsCultureDisabled(InCulture->GetName());
	});
}

FInternationalization::FInternationalization()
	:	bIsInitialized(false)
	,	Implementation(this)
{

}

#undef LOCTEXT_NAMESPACE
