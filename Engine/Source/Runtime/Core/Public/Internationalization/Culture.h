// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/CulturePointer.h"

struct FDecimalNumberFormattingRules;
enum class ETextPluralForm : uint8;
enum class ETextPluralType : uint8;

class CORE_API FCulture
{
#if UE_ENABLE_ICU
	friend class FText;
	friend class FTextChronoFormatter;
	friend class FICUBreakIteratorManager;
#endif

public:
#if UE_ENABLE_ICU
	static FCulturePtr Create(const FString& LocaleName);
#else
	static FCulturePtr Create(
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
#endif

	const FString& GetDisplayName() const;

	const FString& GetEnglishName() const;

	int GetKeyboardLayoutId() const;

	int GetLCID() const;

	TArray<FString> GetPrioritizedParentCultureNames() const;

	static TArray<FString> GetPrioritizedParentCultureNames(const FString& LanguageCode, const FString& ScriptCode, const FString& RegionCode);

	static FString GetCanonicalName(const FString& Name);

	const FString& GetName() const;
	
	const FString& GetNativeName() const;

	const FString& GetUnrealLegacyThreeLetterISOLanguageName() const;

	const FString& GetThreeLetterISOLanguageName() const;

	const FString& GetTwoLetterISOLanguageName() const;

	const FString& GetNativeLanguage() const;

	const FString& GetRegion() const;

	const FString& GetNativeRegion() const;

	const FString& GetScript() const;

	const FString& GetVariant() const;

	const FDecimalNumberFormattingRules& GetDecimalNumberFormattingRules() const;

	const FDecimalNumberFormattingRules& GetPercentFormattingRules() const;

	const FDecimalNumberFormattingRules& GetCurrencyFormattingRules(const FString& InCurrencyCode) const;

	/**
	 * Get the correct plural form to use for the given number
	 * @param PluralType The type of plural form to get (cardinal or ordinal)
	 */
	ETextPluralForm GetPluralForm(float Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(double Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(int8 Val,		const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(int16 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(int32 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(int64 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(uint8 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(uint16 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(uint32 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(uint64 Val,	const ETextPluralType PluralType) const;
	ETextPluralForm GetPluralForm(long Val,		const ETextPluralType PluralType) const;

	void HandleCultureChanged();

private:
#if UE_ENABLE_ICU
	class FICUCultureImplementation;
	typedef FICUCultureImplementation FImplementation;
	TSharedRef<FICUCultureImplementation> Implementation;
#else
	class FLegacyCultureImplementation;
	typedef FLegacyCultureImplementation FImplementation;
	TSharedRef<FLegacyCultureImplementation> Implementation;
#endif

protected:
#if UE_ENABLE_ICU
	FCulture(const FString& LocaleName);
#else
	FCulture(
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
#endif

private:
	FString CachedDisplayName;
	FString CachedEnglishName;
	FString CachedName;
	FString CachedNativeName;
	FString CachedUnrealLegacyThreeLetterISOLanguageName;
	FString CachedThreeLetterISOLanguageName;
	FString CachedTwoLetterISOLanguageName;
	FString CachedNativeLanguage;
	FString CachedRegion;
	FString CachedNativeRegion;
	FString CachedScript;
	FString CachedVariant;
};
