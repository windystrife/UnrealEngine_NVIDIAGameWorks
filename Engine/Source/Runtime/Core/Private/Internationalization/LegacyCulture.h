// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/FastDecimalFormat.h"

#if !UE_ENABLE_ICU
class FCulture::FLegacyCultureImplementation
{
	friend class FCulture;

	FLegacyCultureImplementation(
		const FText& InDisplayName, 
		const FString& InEnglishName, 
		const int InKeyboardLayoutId, 
		const int InLCID, 
		const FString& InName, 
		const FString& InNativeName, 
		const FString& InUnrealLegacyThreeLetterISOLanguageName, 
		const FString& InThreeLetterISOLanguageName, 
		const FString& InTwoLetterISOLanguageName,
		const FDecimalNumberFormattingRules& InDecimalNumberFormattingRules,
		const FDecimalNumberFormattingRules& InPercentFormattingRules,
		const FDecimalNumberFormattingRules& InBaseCurrencyFormattingRules
		);

	FString GetDisplayName() const;

	FString GetEnglishName() const;

	int GetKeyboardLayoutId() const;

	int GetLCID() const;

	static FString GetCanonicalName(const FString& Name);

	FString GetName() const;

	FString GetNativeName() const;

	FString GetUnrealLegacyThreeLetterISOLanguageName() const;

	FString GetThreeLetterISOLanguageName() const;

	FString GetTwoLetterISOLanguageName() const;

	FString GetNativeLanguage() const;

	FString GetNativeRegion() const;

	const FDecimalNumberFormattingRules& GetDecimalNumberFormattingRules();

	const FDecimalNumberFormattingRules& GetPercentFormattingRules();

	const FDecimalNumberFormattingRules& GetCurrencyFormattingRules(const FString& InCurrencyCode);

	ETextPluralForm GetPluralForm(int32 Val, const ETextPluralType PluralType);

	ETextPluralForm GetPluralForm(double Val, const ETextPluralType PluralType);

	// Full localized culture name
	const FText DisplayName;

	// The English name of the culture in format languagefull [country/regionfull]
	const FString EnglishName;

	// Keyboard input locale id
	const int KeyboardLayoutId;

	// id for this Culture
	const int LCID;

	// Name of the culture in languagecode2-country/regioncode2 format
	const FString Name;

	// The culture name, consisting of the language, the country/region, and the optional script
	const FString NativeName;

	// ISO 639-2 three letter code of the language - for the purpose of supporting legacy Unreal documentation.
	const FString UnrealLegacyThreeLetterISOLanguageName;

	// ISO 639-2 three letter code of the language
	const FString ThreeLetterISOLanguageName;

	// ISO 639-1 two letter code of the language
	const FString TwoLetterISOLanguageName;

	// Rules for formatting decimal numbers in this culture
	const FDecimalNumberFormattingRules DecimalNumberFormattingRules;

	// Rules for formatting percentile numbers in this culture
	const FDecimalNumberFormattingRules PercentFormattingRules;

	// Rules for formatting currency numbers in this culture
	const FDecimalNumberFormattingRules BaseCurrencyFormattingRules;

	// Rules for formatting alternate currencies in this culture
	TMap<FString, TSharedPtr<const FDecimalNumberFormattingRules>> UEAlternateCurrencyFormattingRules;
	FCriticalSection UEAlternateCurrencyFormattingRulesCS;
};
#endif
