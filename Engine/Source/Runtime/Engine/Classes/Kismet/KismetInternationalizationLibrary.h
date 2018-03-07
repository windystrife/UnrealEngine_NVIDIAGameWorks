// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetInternationalizationLibrary.generated.h"

UCLASS(meta=(BlueprintThreadSafe))
class ENGINE_API UKismetInternationalizationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Set the current culture.
	 * @note This function is a sledgehammer, and will set both the language and locale, as well as clear out any asset group cultures that may be set.
	 * @param Culture The culture to set, as an IETF language tag (eg, "zh-Hans-CN").
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 * @return True if the culture was set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="1"))
	static bool SetCurrentCulture(const FString& Culture, const bool SaveToConfig = false);

	/**
	 * Get the current culture as an IETF language tag:
	 *  - A two-letter ISO 639-1 language code (eg, "zh").
	 *  - An optional four-letter ISO 15924 script code (eg, "Hans").
	 *  - An optional two-letter ISO 3166-1 country code (eg, "CN").
	 * @note This function exists for legacy API parity with SetCurrentCulture and is equivalent to GetCurrentLanguage.
	 * @return The culture as an IETF language tag (eg, "zh-Hans-CN").
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Internationalization")
	static FString GetCurrentCulture();

	/**
	 * Set *only* the current language (for localization).
	 * @note Unless you're doing something advanced, you likely want SetCurrentLanguageAndLocale or SetCurrentCulture instead.
	 * @param Culture The language to set, as an IETF language tag (eg, "zh-Hans-CN").
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 * @return True if the language was set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="1"))
	static bool SetCurrentLanguage(const FString& Culture, const bool SaveToConfig = false);

	/**
	 * Get the current language (for localization) as an IETF language tag:
	 *  - A two-letter ISO 639-1 language code (eg, "zh").
	 *  - An optional four-letter ISO 15924 script code (eg, "Hans").
	 *  - An optional two-letter ISO 3166-1 country code (eg, "CN").
	 * @return The language as an IETF language tag (eg, "zh-Hans-CN").
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Internationalization")
	static FString GetCurrentLanguage();

	/**
	 * Set *only* the current locale (for internationalization).
	 * @note Unless you're doing something advanced, you likely want SetCurrentLanguageAndLocale or SetCurrentCulture instead.
	 * @param Culture The locale to set, as an IETF language tag (eg, "zh-Hans-CN").
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 * @return True if the locale was set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="1"))
	static bool SetCurrentLocale(const FString& Culture, const bool SaveToConfig = false);

	/**
	 * Get the current locale (for internationalization) as an IETF language tag:
	 *  - A two-letter ISO 639-1 language code (eg, "zh").
	 *  - An optional four-letter ISO 15924 script code (eg, "Hans").
	 *  - An optional two-letter ISO 3166-1 country code (eg, "CN").
	 * @return The locale as an IETF language tag (eg, "zh-Hans-CN").
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Internationalization")
	static FString GetCurrentLocale();

	/**
	 * Set the current language (for localization) and locale (for internationalization).
	 * @param Culture The language and locale to set, as an IETF language tag (eg, "zh-Hans-CN").
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 * @return True if the language and locale were set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="1"))
	static bool SetCurrentLanguageAndLocale(const FString& Culture, const bool SaveToConfig = false);

	/**
	 * Set the given asset group category culture from an IETF language tag (eg, "zh-Hans-CN").
	 * @param AssetGroup The asset group to set the culture for.
	 * @param Culture The culture to set, as an IETF language tag (eg, "zh-Hans-CN").
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 * @return True if the culture was set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="2"))
	static bool SetCurrentAssetGroupCulture(const FName AssetGroup, const FString& Culture, const bool SaveToConfig = false);

	/**
	 * Get the given asset group category culture.
	 * @note Returns the current language if the group category doesn't have a culture override.
	 * @param AssetGroup The asset group to get the culture for.
	 * @return The culture as an IETF language tag (eg, "zh-Hans-CN").
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Internationalization")
	static FString GetCurrentAssetGroupCulture(const FName AssetGroup);

	/**
	 * Clear the given asset group category culture.
	 * @param AssetGroup The asset group to clear the culture for.
	 * @param SaveToConfig If true, save the new setting to the users' "GameUserSettings" config so that it persists after a reload.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Internationalization", meta=(AdvancedDisplay="1"))
	static void ClearCurrentAssetGroupCulture(const FName AssetGroup, const bool SaveToConfig = false);
};
