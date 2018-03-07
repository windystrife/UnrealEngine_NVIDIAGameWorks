// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/LegacyCulture.h"
#include "Misc/ScopeLock.h"

#if !UE_ENABLE_ICU

FCulture::FLegacyCultureImplementation::FLegacyCultureImplementation(
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
	)
	: DisplayName(InDisplayName)
	, EnglishName(InEnglishName)
	, KeyboardLayoutId(InKeyboardLayoutId)
	, LCID(InLCID)
	, Name( InName )
	, NativeName(InNativeName)
	, UnrealLegacyThreeLetterISOLanguageName( InUnrealLegacyThreeLetterISOLanguageName )
	, ThreeLetterISOLanguageName( InThreeLetterISOLanguageName )
	, TwoLetterISOLanguageName( InTwoLetterISOLanguageName )
	, DecimalNumberFormattingRules( InDecimalNumberFormattingRules )
	, PercentFormattingRules( InPercentFormattingRules )
	, BaseCurrencyFormattingRules( InBaseCurrencyFormattingRules )
{ 
}

FString FCulture::FLegacyCultureImplementation::GetDisplayName() const
{
	return DisplayName.ToString();
}

FString FCulture::FLegacyCultureImplementation::GetEnglishName() const
{
	return EnglishName;
}

int FCulture::FLegacyCultureImplementation::GetKeyboardLayoutId() const
{
	return KeyboardLayoutId;
}

int FCulture::FLegacyCultureImplementation::GetLCID() const
{
	return LCID;
}

FString FCulture::FLegacyCultureImplementation::GetName() const
{
	return Name;
}

FString FCulture::FLegacyCultureImplementation::GetCanonicalName(const FString& Name)
{
	return Name;
}

FString FCulture::FLegacyCultureImplementation::GetNativeName() const
{
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetNativeLanguage() const
{
	int32 LastBracket = INDEX_NONE;
	int32 FirstBracket = INDEX_NONE;
	if ( NativeName.FindLastChar( ')', LastBracket ) && NativeName.FindChar( '(', FirstBracket ) && LastBracket != FirstBracket )
	{
		return NativeName.Left( FirstBracket-1 );
	}
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetNativeRegion() const
{
	int32 LastBracket = INDEX_NONE;
	int32 FirstBracket = INDEX_NONE;
	if ( NativeName.FindLastChar( ')', LastBracket ) && NativeName.FindChar( '(', FirstBracket ) && LastBracket != FirstBracket )
	{
		return NativeName.Mid( FirstBracket+1, LastBracket-FirstBracket-1 );
	}
	return NativeName;
}

FString FCulture::FLegacyCultureImplementation::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	return UnrealLegacyThreeLetterISOLanguageName;
}

FString FCulture::FLegacyCultureImplementation::GetThreeLetterISOLanguageName() const
{
	return ThreeLetterISOLanguageName;
}

FString FCulture::FLegacyCultureImplementation::GetTwoLetterISOLanguageName() const
{
	return TwoLetterISOLanguageName;
}

const FDecimalNumberFormattingRules& FCulture::FLegacyCultureImplementation::GetDecimalNumberFormattingRules()
{
	return DecimalNumberFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FLegacyCultureImplementation::GetPercentFormattingRules()
{
	return PercentFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FLegacyCultureImplementation::GetCurrencyFormattingRules(const FString& InCurrencyCode)
{
	const bool bUseDefaultFormattingRules = InCurrencyCode.IsEmpty();

	if (bUseDefaultFormattingRules)
	{
		return BaseCurrencyFormattingRules;
	}
	else
	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(InCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}
	}

	FDecimalNumberFormattingRules NewUECurrencyFormattingRules = BaseCurrencyFormattingRules;
	NewUECurrencyFormattingRules.NegativePrefixString.ReplaceInline(TEXT("$"), *InCurrencyCode, ESearchCase::CaseSensitive);
	NewUECurrencyFormattingRules.NegativeSuffixString.ReplaceInline(TEXT("$"), *InCurrencyCode, ESearchCase::CaseSensitive);
	NewUECurrencyFormattingRules.PositivePrefixString.ReplaceInline(TEXT("$"), *InCurrencyCode, ESearchCase::CaseSensitive);
	NewUECurrencyFormattingRules.PositiveSuffixString.ReplaceInline(TEXT("$"), *InCurrencyCode, ESearchCase::CaseSensitive);

	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		// Find again in case another thread beat us to it
		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(InCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}

		FoundUEAlternateCurrencyFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUECurrencyFormattingRules));
		UEAlternateCurrencyFormattingRules.Add(InCurrencyCode, FoundUEAlternateCurrencyFormattingRules);
		return *FoundUEAlternateCurrencyFormattingRules;
	}
}

namespace
{

template <typename T>
ETextPluralForm GetDefaultPluralForm(T Val, const ETextPluralType PluralType)
{
	if (PluralType == ETextPluralType::Cardinal)
	{
		return (Val == 1) ? ETextPluralForm::One : ETextPluralForm::Other;
	}
	else
	{
		check(PluralType == ETextPluralType::Ordinal);

		if (Val % 10 == 1 && Val % 100 != 11)
		{
			return ETextPluralForm::One;
		}
		if (Val % 10 == 2 && Val % 100 != 12)
		{
			return ETextPluralForm::Two;
		}
		if (Val % 10 == 3 && Val % 100 != 13)
		{
			return ETextPluralForm::Few;
		}
	}

	return ETextPluralForm::Other;
}

}

ETextPluralForm FCulture::FLegacyCultureImplementation::GetPluralForm(int32 Val, const ETextPluralType PluralType)
{
	checkf(Val >= 0, TEXT("GetPluralFormImpl requires a positive value"));
	return GetDefaultPluralForm(Val, PluralType);
}

ETextPluralForm FCulture::FLegacyCultureImplementation::GetPluralForm(double Val, const ETextPluralType PluralType)
{
	checkf(!FMath::IsNegativeDouble(Val), TEXT("GetPluralFormImpl requires a positive value"));
	return GetDefaultPluralForm((int64)Val, PluralType);
}

#endif
