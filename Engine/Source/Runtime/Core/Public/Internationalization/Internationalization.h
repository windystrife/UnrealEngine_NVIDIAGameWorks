// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Delegates/Delegate.h"
#include "Internationalization/Text.h"
#include "LocTesting.h"

#include "Templates/UniqueObj.h"

#define LOC_DEFINE_REGION

class FCulture;
class FICUInternationalization;
class FLegacyInternationalization;

class FInternationalization
{
	friend class FTextChronoFormatter;

public:
	static CORE_API FInternationalization& Get();

	/** Checks to see that an internationalization instance exists, and has been initialized. Usually you would use Get(), however this may be used to work out whether TearDown() has been called when cleaning up on shutdown. */
	static CORE_API bool IsAvailable();

	static CORE_API void TearDown();

	static CORE_API FText ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey);

	/**
	 * Struct that can be used to capture a snapshot of the active culture state in a way that can be re-applied losslessly.
	 * Mostly used during automation testing.
	 */
	struct FCultureStateSnapshot
	{
		FString Language;
		FString Locale;
		TArray<TTuple<FName, FString>> AssetGroups;
	};

	/**
	 * Set the current culture by name.
	 * @note This function is a sledgehammer, and will set both the language and locale, as well as clear out any asset group cultures that may be set.
	 * @note SetCurrentCulture should be avoided in Core/Engine code as it may stomp over Editor/Game user-settings.
	 */
	CORE_API bool SetCurrentCulture(const FString& InCultureName);

	/**
	 * Get the current culture.
	 * @note This function exists for legacy API parity with SetCurrentCulture and is equivalent to GetCurrentLanguage. It should *never* be used in internal localization/internationalization code!
	 */
	CORE_API FCultureRef GetCurrentCulture() const
	{
		return CurrentLanguage.ToSharedRef();
	}

	/**
	 * Set *only* the current language (for localization) by name.
	 * @note Unless you're doing something advanced, you likely want SetCurrentLanguageAndLocale or SetCurrentCulture instead.
	 */
	CORE_API bool SetCurrentLanguage(const FString& InCultureName);

	/**
	 * Get the current language (for localization).
	 */
	CORE_API FCultureRef GetCurrentLanguage() const
	{
		return CurrentLanguage.ToSharedRef();
	}

	/**
	 * Set *only* the current locale (for internationalization) by name.
	 * @note Unless you're doing something advanced, you likely want SetCurrentLanguageAndLocale or SetCurrentCulture instead.
	 */
	CORE_API bool SetCurrentLocale(const FString& InCultureName);

	/**
	 * Get the current locale (for internationalization).
	 */
	CORE_API FCultureRef GetCurrentLocale() const
	{
		return CurrentLocale.ToSharedRef();
	}

	/**
	 * Set the current language (for localization) and locale (for internationalization) by name.
	 */
	CORE_API bool SetCurrentLanguageAndLocale(const FString& InCultureName);

	/**
	 * Set the given asset group category culture by name.
	 */
	CORE_API bool SetCurrentAssetGroupCulture(const FName& InAssetGroupName, const FString& InCultureName);

	/**
	 * Get the given asset group category culture.
	 * @note Returns the current language if the group category doesn't have a culture override.
	 */
	CORE_API FCultureRef GetCurrentAssetGroupCulture(const FName& InAssetGroupName) const;

	/**
	 * Clear the given asset group category culture.
	 */
	CORE_API void ClearCurrentAssetGroupCulture(const FName& InAssetGroupName);

	/**
	 * Get the culture corresponding to the given name.
	 * @return The culture for the given name, or null if it couldn't be found.
	 */
	CORE_API FCulturePtr GetCulture(const FString& InCultureName);

	/**
	 * Get the default culture specified by the OS.
	 * @note This function exists for legacy API parity with GetCurrentCulture and is equivalent to GetDefaultLanguage. It should *never* be used in internal localization/internationalization code!
	 */
	CORE_API FCultureRef GetDefaultCulture() const
	{
		return DefaultLanguage.ToSharedRef();
	}

	/**
	 * Get the default language specified by the OS.
	 */
	CORE_API FCultureRef GetDefaultLanguage() const
	{
		return DefaultLanguage.ToSharedRef();
	}

	/**
	 * Get the default locale specified by the OS.
	 */
	CORE_API FCultureRef GetDefaultLocale() const
	{
		return DefaultLocale.ToSharedRef();
	}

	/**
	 * Get the invariant culture that can be used when you don't care about localization/internationalization.
	 */
	CORE_API FCultureRef GetInvariantCulture() const
	{
		return InvariantCulture.ToSharedRef();
	}

	/**
	 * Backup the current culture state to the given snapshot struct.
	 */
	CORE_API void BackupCultureState(FCultureStateSnapshot& OutSnapshot) const;

	/**
	 * Restore a previous culture state from the given snapshot struct.
	 */
	CORE_API void RestoreCultureState(const FCultureStateSnapshot& InSnapshot);

	CORE_API bool IsInitialized() const {return bIsInitialized;}

	// Load and cache the data needed for every culture we know about (this is usually done per-culture as required)
	CORE_API void LoadAllCultureData();

#if ENABLE_LOC_TESTING
	static CORE_API FString& Leetify(FString& SourceString);
#endif

	CORE_API void GetCultureNames(TArray<FString>& CultureNames) const;

	CORE_API TArray<FString> GetPrioritizedCultureNames(const FString& Name);

	// Given some paths to look at, populate a list of cultures that we have available localization information for. If bIncludeDerivedCultures, include cultures that are derived from those we have localization data for.
	CORE_API void GetCulturesWithAvailableLocalization(const TArray<FString>& InLocalizationPaths, TArray< FCultureRef >& OutAvailableCultures, const bool bIncludeDerivedCultures);

	/** Broadcasts whenever the current culture changes */
	DECLARE_EVENT(FInternationalization, FCultureChangedEvent)
	CORE_API FCultureChangedEvent& OnCultureChanged() { return CultureChangedEvent; }

private:
	FInternationalization();

	void BroadcastCultureChanged() { CultureChangedEvent.Broadcast(); }

	void Initialize();
	void Terminate();

private:
	static FInternationalization* Instance;
	bool bIsInitialized;

	FCultureChangedEvent CultureChangedEvent;

#if UE_ENABLE_ICU
	friend class FICUInternationalization;
	typedef FICUInternationalization FImplementation;
#else
	friend class FLegacyInternationalization;
	typedef FLegacyInternationalization FImplementation;
#endif
	TUniqueObj<FImplementation> Implementation;

	/**
	 * The currently active language (for localization).
	 */
	FCulturePtr CurrentLanguage;

	/**
	 * The currently active locale (for internationalization).
	 */
	FCulturePtr CurrentLocale;

	/**
	 * The currently active asset group cultures (for package localization).
	 * @note This is deliberately an array for performance reasons (we expect to have a very small number of groups).
	 */
	TArray<TTuple<FName, FCulturePtr>> CurrentAssetGroupCultures;

	/**
	 * The default language specified by the OS.
	 */
	FCulturePtr DefaultLanguage;

	/**
	 * The default locale specified by the OS.
	 */
	FCulturePtr DefaultLocale;

	/**
	 * An invariant culture that can be used when you don't care about localization/internationalization.
	 */
	FCulturePtr InvariantCulture;
};

/** The global namespace that must be defined/undefined to wrap uses of the NS-prefixed macros below */
#undef LOCTEXT_NAMESPACE

/**
 * Creates an FText. All parameters must be string literals. All literals will be passed through the localization system.
 * The global LOCTEXT_NAMESPACE macro must be first set to a string literal to specify this localization key's namespace.
 */
#define LOCTEXT( InKey, InTextLiteral ) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( TEXT( InTextLiteral ), TEXT(LOCTEXT_NAMESPACE), TEXT( InKey ) )

/**
 * Creates an FText. All parameters must be string literals. All literals will be passed through the localization system.
 */
#define NSLOCTEXT( InNamespace, InKey, InTextLiteral ) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText( TEXT( InTextLiteral ), TEXT( InNamespace ), TEXT( InKey ) )

#undef LOC_DEFINE_REGION
