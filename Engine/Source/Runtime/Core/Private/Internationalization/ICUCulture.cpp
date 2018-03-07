// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/ICUCulture.h"
#include "Misc/ScopeLock.h"
#include "Internationalization/FastDecimalFormat.h"

#if UE_ENABLE_ICU
#include "Internationalization/ICUUtilities.h"

namespace
{
	TSharedRef<const icu::BreakIterator> CreateBreakIterator( const icu::Locale& ICULocale, const EBreakIteratorType Type)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		icu::BreakIterator* (*FactoryFunction)(const icu::Locale&, UErrorCode&) = nullptr;
		switch (Type)
		{
		case EBreakIteratorType::Grapheme:
			FactoryFunction = icu::BreakIterator::createCharacterInstance;
			break;
		case EBreakIteratorType::Word:
			FactoryFunction = icu::BreakIterator::createWordInstance;
			break;
		case EBreakIteratorType::Line:
			FactoryFunction = icu::BreakIterator::createLineInstance;
			break;
		case EBreakIteratorType::Sentence:
			FactoryFunction = icu::BreakIterator::createSentenceInstance;
			break;
		case EBreakIteratorType::Title:
			FactoryFunction = icu::BreakIterator::createTitleInstance;
			break;
		default:
			checkf(false, TEXT("Unhandled break iterator type"));
		}
		TSharedPtr<const icu::BreakIterator> Ptr = MakeShareable( FactoryFunction(ICULocale, ICUStatus) );
		checkf(Ptr.IsValid(), TEXT("Creating a break iterator object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::Collator, ESPMode::ThreadSafe> CreateCollator( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<const icu::Collator, ESPMode::ThreadSafe> Ptr = MakeShareable( icu::Collator::createInstance( ICULocale, ICUStatus ) );
		checkf(Ptr.IsValid(), TEXT("Creating a collator object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe> CreateDecimalFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<const icu::DecimalFormat, ESPMode::ThreadSafe> Ptr = MakeShareable( static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance( ICULocale, ICUStatus )) );
		checkf(Ptr.IsValid(), TEXT("Creating a decimal format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DecimalFormat> CreateCurrencyFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<const icu::DecimalFormat> Ptr = MakeShareable( static_cast<icu::DecimalFormat*>(icu::NumberFormat::createCurrencyInstance( ICULocale, ICUStatus )) );
		checkf(Ptr.IsValid(), TEXT("Creating a currency format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DecimalFormat> CreatePercentFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<const icu::DecimalFormat> Ptr = MakeShareable( static_cast<icu::DecimalFormat*>(icu::NumberFormat::createPercentInstance( ICULocale, ICUStatus )) );
		checkf(Ptr.IsValid(), TEXT("Creating a percent format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateDateFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createDateInstance( icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a date format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateTimeFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createTimeInstance( icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a time format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}

	TSharedRef<const icu::DateFormat> CreateDateTimeFormat( const icu::Locale& ICULocale )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		TSharedPtr<icu::DateFormat> Ptr = MakeShareable( icu::DateFormat::createDateTimeInstance( icu::DateFormat::EStyle::kDefault, icu::DateFormat::EStyle::kDefault, ICULocale ) );
		checkf(Ptr.IsValid(), TEXT("Creating a date-time format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
		Ptr->adoptTimeZone( icu::TimeZone::createDefault() );
		return Ptr.ToSharedRef();
	}
}

ETextPluralForm ICUPluralFormToUE(const icu::UnicodeString& InICUTag)
{
	static const icu::UnicodeString ZeroStr("zero");
	static const icu::UnicodeString OneStr("one");
	static const icu::UnicodeString TwoStr("two");
	static const icu::UnicodeString FewStr("few");
	static const icu::UnicodeString ManyStr("many");
	static const icu::UnicodeString OtherStr("other");

	if (InICUTag == ZeroStr)
	{
		return ETextPluralForm::Zero;
	}
	if (InICUTag == OneStr)
	{
		return ETextPluralForm::One;
	}
	if (InICUTag == TwoStr)
	{
		return ETextPluralForm::Two;
	}
	if (InICUTag == FewStr)
	{
		return ETextPluralForm::Few;
	}
	if (InICUTag == ManyStr)
	{
		return ETextPluralForm::Many;
	}
	if (InICUTag == OtherStr)
	{
		return ETextPluralForm::Other;
	}

	ensureAlwaysMsgf(false, TEXT("Unknown ICU plural form tag! Returning 'other'."));
	return ETextPluralForm::Other;
}

FCulture::FICUCultureImplementation::FICUCultureImplementation(const FString& LocaleName)
	: ICULocale( TCHAR_TO_ANSI( *LocaleName ) )
	, ICUDecimalFormatLRUCache( 10 )
{
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		ICUCardinalPluralRules = icu::PluralRules::forLocale(ICULocale, UPLURAL_TYPE_CARDINAL, ICUStatus);
		checkf(U_SUCCESS(ICUStatus) && ICUCardinalPluralRules, TEXT("Creating a cardinal plural rules object failed using locale %s. Perhaps this locale has no data."), *LocaleName);
	}
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		ICUOrdianalPluralRules = icu::PluralRules::forLocale(ICULocale, UPLURAL_TYPE_ORDINAL, ICUStatus);
		checkf(U_SUCCESS(ICUStatus) && ICUOrdianalPluralRules, TEXT("Creating an ordinal plural rules object failed using locale %s. Perhaps this locale has no data."), *LocaleName);
	}
}

FString FCulture::FICUCultureImplementation::GetDisplayName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

FString FCulture::FICUCultureImplementation::GetEnglishName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(icu::Locale("en"), ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

int FCulture::FICUCultureImplementation::GetKeyboardLayoutId() const
{
	return 0;
}

int FCulture::FICUCultureImplementation::GetLCID() const
{
	return ICULocale.getLCID();
}

FString FCulture::FICUCultureImplementation::GetCanonicalName(const FString& Name)
{
	const FString SanitizedName = ICUUtilities::SanitizeCultureCode(Name);

	static const int32 CanonicalNameBufferSize = 64;
	char CanonicalNameBuffer[CanonicalNameBufferSize];

	UErrorCode ICUStatus = U_ZERO_ERROR;
	uloc_canonicalize(TCHAR_TO_ANSI(*SanitizedName), CanonicalNameBuffer, CanonicalNameBufferSize-1, &ICUStatus);
	CanonicalNameBuffer[CanonicalNameBufferSize-1] = 0;

	FString CanonicalNameString = CanonicalNameBuffer;
	CanonicalNameString.ReplaceInline(TEXT("_"), TEXT("-"));
	return CanonicalNameString;
}

FString FCulture::FICUCultureImplementation::GetName() const
{
	FString Result = ICULocale.getName();
	Result.ReplaceInline(TEXT("_"), TEXT("-"), ESearchCase::IgnoreCase);
	return Result;
}

FString FCulture::FICUCultureImplementation::GetNativeName() const
{
	icu::UnicodeString ICUResult;
	ICULocale.getDisplayName(ICULocale, ICUResult);
	return ICUUtilities::ConvertString(ICUResult);
}

FString FCulture::FICUCultureImplementation::GetUnrealLegacyThreeLetterISOLanguageName() const
{
	FString Result( ICULocale.getISO3Language() );

	// Legacy Overrides (INT, JPN, KOR), also for new web localization (CHN)
	// and now for any other languages (FRA, DEU...) for correct redirection of documentation web links
	if (Result == TEXT("eng"))
	{
		Result = TEXT("INT");
	}
	else
	{
		Result = Result.ToUpper();
	}

	return Result;
}

FString FCulture::FICUCultureImplementation::GetThreeLetterISOLanguageName() const
{
	return ICULocale.getISO3Language();
}

FString FCulture::FICUCultureImplementation::GetTwoLetterISOLanguageName() const
{
	return ICULocale.getLanguage();
}

FString FCulture::FICUCultureImplementation::GetNativeLanguage() const
{
	icu::UnicodeString ICUNativeLanguage;
	ICULocale.getDisplayLanguage(ICULocale, ICUNativeLanguage);
	FString NativeLanguage;
	ICUUtilities::ConvertString(ICUNativeLanguage, NativeLanguage);

	icu::UnicodeString ICUNativeScript;
	ICULocale.getDisplayScript(ICULocale, ICUNativeScript);
	FString NativeScript;
	ICUUtilities::ConvertString(ICUNativeScript, NativeScript);

	if ( !NativeScript.IsEmpty() )
	{
		return NativeLanguage + TEXT(" (") + NativeScript + TEXT(")");
	}
	return NativeLanguage;
}

FString FCulture::FICUCultureImplementation::GetRegion() const
{
	return ICULocale.getCountry();
}

FString FCulture::FICUCultureImplementation::GetNativeRegion() const
{
	icu::UnicodeString ICUNativeCountry;
	ICULocale.getDisplayCountry(ICULocale, ICUNativeCountry);
	FString NativeCountry;
	ICUUtilities::ConvertString(ICUNativeCountry, NativeCountry);

	icu::UnicodeString ICUNativeVariant;
	ICULocale.getDisplayVariant(ICULocale, ICUNativeVariant);
	FString NativeVariant;
	ICUUtilities::ConvertString(ICUNativeVariant, NativeVariant);

	if ( !NativeVariant.IsEmpty() )
	{
		return NativeCountry + TEXT(", ") + NativeVariant;
	}
	return NativeCountry;
}

FString FCulture::FICUCultureImplementation::GetScript() const
{
	return ICULocale.getScript();
}

FString FCulture::FICUCultureImplementation::GetVariant() const
{
	return ICULocale.getVariant();
}

TSharedRef<const icu::BreakIterator> FCulture::FICUCultureImplementation::GetBreakIterator(const EBreakIteratorType Type)
{
	TSharedPtr<const icu::BreakIterator> Result;

	switch (Type)
	{
	case EBreakIteratorType::Grapheme:
		{
			Result = ICUGraphemeBreakIterator.IsValid() ? ICUGraphemeBreakIterator : ( ICUGraphemeBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Word:
		{
			Result = ICUWordBreakIterator.IsValid() ? ICUWordBreakIterator : ( ICUWordBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Line:
		{
			Result = ICULineBreakIterator.IsValid() ? ICULineBreakIterator : ( ICULineBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Sentence:
		{
			Result = ICUSentenceBreakIterator.IsValid() ? ICUSentenceBreakIterator : ( ICUSentenceBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	case EBreakIteratorType::Title:
		{
			Result = ICUTitleBreakIterator.IsValid() ? ICUTitleBreakIterator : ( ICUTitleBreakIterator = CreateBreakIterator(ICULocale, Type) );
		}
		break;
	}

	return Result.ToSharedRef();
}

TSharedRef<const icu::Collator, ESPMode::ThreadSafe> FCulture::FICUCultureImplementation::GetCollator(const ETextComparisonLevel::Type ComparisonLevel)
{
	if (!ICUCollator.IsValid())
	{
		ICUCollator = CreateCollator( ICULocale );
	}

	UErrorCode ICUStatus = U_ZERO_ERROR;
	const bool bIsDefault = (ComparisonLevel == ETextComparisonLevel::Default);
	const TSharedRef<const icu::Collator, ESPMode::ThreadSafe> DefaultCollator( ICUCollator.ToSharedRef() );
	if(bIsDefault)
	{
		return DefaultCollator;
	}
	else
	{
		const TSharedRef<icu::Collator, ESPMode::ThreadSafe> Collator( DefaultCollator->clone() );
		Collator->setAttribute(UColAttribute::UCOL_STRENGTH, UEToICU(ComparisonLevel), ICUStatus);
		return Collator;
	}
}

TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe> FCulture::FICUCultureImplementation::GetDecimalFormatter(const FNumberFormattingOptions* const Options)
{
	if (!ICUDecimalFormat_DefaultForCulture.IsValid())
	{
		ICUDecimalFormat_DefaultForCulture = CreateDecimalFormat( ICULocale );
	}

	const bool bIsCultureDefault = Options == nullptr;
	const TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe> DefaultFormatter( ICUDecimalFormat_DefaultForCulture.ToSharedRef() );
	if (bIsCultureDefault)
	{
		return DefaultFormatter;
	}
	else if (FNumberFormattingOptions::DefaultWithGrouping().IsIdentical(*Options))
	{
		if (!ICUDecimalFormat_DefaultWithGrouping.IsValid())
		{
			const TSharedRef<icu::DecimalFormat, ESPMode::ThreadSafe> Formatter( static_cast<icu::DecimalFormat*>(DefaultFormatter->clone()) );
			Formatter->setGroupingUsed(Options->UseGrouping);
			Formatter->setRoundingMode(UEToICU(Options->RoundingMode));
			Formatter->setMinimumIntegerDigits(Options->MinimumIntegralDigits);
			Formatter->setMaximumIntegerDigits(Options->MaximumIntegralDigits);
			Formatter->setMinimumFractionDigits(Options->MinimumFractionalDigits);
			Formatter->setMaximumFractionDigits(Options->MaximumFractionalDigits);
			ICUDecimalFormat_DefaultWithGrouping = Formatter;
		}
		return ICUDecimalFormat_DefaultWithGrouping.ToSharedRef();
	}
	else if (FNumberFormattingOptions::DefaultNoGrouping().IsIdentical(*Options))
	{
		if (!ICUDecimalFormat_DefaultNoGrouping.IsValid())
		{
			const TSharedRef<icu::DecimalFormat, ESPMode::ThreadSafe> Formatter( static_cast<icu::DecimalFormat*>(DefaultFormatter->clone()) );
			Formatter->setGroupingUsed(Options->UseGrouping);
			Formatter->setRoundingMode(UEToICU(Options->RoundingMode));
			Formatter->setMinimumIntegerDigits(Options->MinimumIntegralDigits);
			Formatter->setMaximumIntegerDigits(Options->MaximumIntegralDigits);
			Formatter->setMinimumFractionDigits(Options->MinimumFractionalDigits);
			Formatter->setMaximumFractionDigits(Options->MaximumFractionalDigits);
			ICUDecimalFormat_DefaultNoGrouping = Formatter;
		}
		return ICUDecimalFormat_DefaultNoGrouping.ToSharedRef();
	}
	else
	{
		FScopeLock ScopeLock(&ICUDecimalFormatLRUCacheCS);

		const TSharedPtr<const icu::DecimalFormat, ESPMode::ThreadSafe> CachedFormatter = ICUDecimalFormatLRUCache.AccessItem(*Options);
		if (CachedFormatter.IsValid())
		{
			return CachedFormatter.ToSharedRef();
		}

		const TSharedRef<icu::DecimalFormat, ESPMode::ThreadSafe> Formatter( static_cast<icu::DecimalFormat*>(DefaultFormatter->clone()) );
		Formatter->setGroupingUsed(Options->UseGrouping);
		Formatter->setRoundingMode(UEToICU(Options->RoundingMode));
		Formatter->setMinimumIntegerDigits(Options->MinimumIntegralDigits);
		Formatter->setMaximumIntegerDigits(Options->MaximumIntegralDigits);
		Formatter->setMinimumFractionDigits(Options->MinimumFractionalDigits);
		Formatter->setMaximumFractionDigits(Options->MaximumFractionalDigits);

		ICUDecimalFormatLRUCache.Add(*Options, Formatter);

		return Formatter;
	}
}

TSharedRef<const icu::DecimalFormat> FCulture::FICUCultureImplementation::GetCurrencyFormatter(const FString& CurrencyCode, const FNumberFormattingOptions* const Options)
{
	if (!ICUCurrencyFormat.IsValid())
	{
		ICUCurrencyFormat = CreateCurrencyFormat( ICULocale );
	}

	const FString SanitizedCurrencyCode = ICUUtilities::SanitizeCurrencyCode(CurrencyCode);
	const bool bIsDefault = Options == NULL && SanitizedCurrencyCode.IsEmpty();
	const TSharedRef<const icu::DecimalFormat> DefaultFormatter( ICUCurrencyFormat.ToSharedRef() );

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DecimalFormat> Formatter( static_cast<icu::DecimalFormat*>(DefaultFormatter->clone()) );
		
		if (!CurrencyCode.IsEmpty())
		{
			icu::UnicodeString ICUCurrencyCode;
			ICUUtilities::ConvertString(SanitizedCurrencyCode, ICUCurrencyCode);
			Formatter->setCurrency(ICUCurrencyCode.getBuffer());
		}

		if(Options)
		{
			Formatter->setGroupingUsed(Options->UseGrouping);
			Formatter->setRoundingMode(UEToICU(Options->RoundingMode));
			Formatter->setMinimumIntegerDigits(Options->MinimumIntegralDigits);
			Formatter->setMaximumIntegerDigits(Options->MaximumIntegralDigits);
			Formatter->setMinimumFractionDigits(Options->MinimumFractionalDigits);
			Formatter->setMaximumFractionDigits(Options->MaximumFractionalDigits);
		}

		return Formatter;
	}
}

TSharedRef<const icu::DecimalFormat> FCulture::FICUCultureImplementation::GetPercentFormatter(const FNumberFormattingOptions* const Options)
{
	if (!ICUPercentFormat.IsValid())
	{
		ICUPercentFormat = CreatePercentFormat( ICULocale );
	}

	const bool bIsDefault = Options == NULL;
	const TSharedRef<const icu::DecimalFormat> DefaultFormatter( ICUPercentFormat.ToSharedRef() );
	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DecimalFormat> Formatter( static_cast<icu::DecimalFormat*>(DefaultFormatter->clone()) );
		if(Options)
		{
			Formatter->setGroupingUsed(Options->UseGrouping);
			Formatter->setRoundingMode(UEToICU(Options->RoundingMode));
			Formatter->setMinimumIntegerDigits(Options->MinimumIntegralDigits);
			Formatter->setMaximumIntegerDigits(Options->MaximumIntegralDigits);
			Formatter->setMinimumFractionDigits(Options->MinimumFractionalDigits);
			Formatter->setMaximumFractionDigits(Options->MaximumFractionalDigits);
		}
		return Formatter;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetDateFormatter(const EDateTimeStyle::Type DateStyle, const FString& TimeZone)
{
	if (!ICUDateFormat.IsValid())
	{
		ICUDateFormat = CreateDateFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUDateFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		DateStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createDateInstance( UEToICU(DateStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetTimeFormatter(const EDateTimeStyle::Type TimeStyle, const FString& TimeZone)
{
	if (!ICUTimeFormat.IsValid())
	{
		ICUTimeFormat = CreateTimeFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUTimeFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		TimeStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createTimeInstance( UEToICU(TimeStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

TSharedRef<const icu::DateFormat> FCulture::FICUCultureImplementation::GetDateTimeFormatter(const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone)
{
	if (!ICUDateTimeFormat.IsValid())
	{
		ICUDateTimeFormat = CreateDateTimeFormat( ICULocale );
	}

	const FString SanitizedTimezoneCode = ICUUtilities::SanitizeTimezoneCode(TimeZone);

	icu::UnicodeString InputTimeZoneID;
	ICUUtilities::ConvertString(SanitizedTimezoneCode, InputTimeZoneID, false);

	const TSharedRef<const icu::DateFormat> DefaultFormatter( ICUDateTimeFormat.ToSharedRef() );

	bool bIsDefaultTimeZone = SanitizedTimezoneCode.IsEmpty();
	if( !bIsDefaultTimeZone )
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;

		icu::UnicodeString CanonicalInputTimeZoneID;
		icu::TimeZone::getCanonicalID(InputTimeZoneID, CanonicalInputTimeZoneID, ICUStatus);

		icu::UnicodeString DefaultTimeZoneID;
		DefaultFormatter->getTimeZone().getID(DefaultTimeZoneID);

		icu::UnicodeString CanonicalDefaultTimeZoneID;
		icu::TimeZone::getCanonicalID(DefaultTimeZoneID, CanonicalDefaultTimeZoneID, ICUStatus);

		bIsDefaultTimeZone = (CanonicalInputTimeZoneID == CanonicalDefaultTimeZoneID ? true : false);
	}

	const bool bIsDefault = 
		DateStyle == EDateTimeStyle::Default &&
		TimeStyle == EDateTimeStyle::Default &&
		bIsDefaultTimeZone;

	if(bIsDefault)
	{
		return DefaultFormatter;
	}
	else
	{
		const TSharedRef<icu::DateFormat> Formatter( icu::DateFormat::createDateTimeInstance( UEToICU(DateStyle), UEToICU(TimeStyle), ICULocale ) );
		Formatter->adoptTimeZone( bIsDefaultTimeZone ? icu::TimeZone::createDefault() : icu::TimeZone::createTimeZone(InputTimeZoneID) );
		return Formatter;
	}
}

namespace
{

FDecimalNumberFormattingRules ExtractNumberFormattingRulesFromICUDecimalFormatter(icu::DecimalFormat& InICUDecimalFormat)
{
	FDecimalNumberFormattingRules NewUEDecimalNumberFormattingRules;

	// Extract the default formatting options before we mess around with the formatter object settings
	NewUEDecimalNumberFormattingRules.CultureDefaultFormattingOptions
		.SetUseGrouping(InICUDecimalFormat.isGroupingUsed() != 0)
		.SetRoundingMode(ICUToUE(InICUDecimalFormat.getRoundingMode()))
		.SetMinimumIntegralDigits(InICUDecimalFormat.getMinimumIntegerDigits())
		.SetMaximumIntegralDigits(InICUDecimalFormat.getMaximumIntegerDigits())
		.SetMinimumFractionalDigits(InICUDecimalFormat.getMinimumFractionDigits())
		.SetMaximumFractionalDigits(InICUDecimalFormat.getMaximumFractionDigits());

	// We force grouping to be on, even if a culture doesn't use it by default, so that we can extract meaningful grouping information
	// This allows us to use the correct groupings if we should ever force grouping for a number, rather than use the culture default
	InICUDecimalFormat.setGroupingUsed(true);

	auto ICUStringToTCHAR = [](const icu::UnicodeString& InICUString) -> TCHAR
	{
		check(InICUString.length() == 1);
		return static_cast<TCHAR>(InICUString.charAt(0));
	};

	auto ExtractFormattingSymbolAsCharacter = [&](icu::DecimalFormatSymbols::ENumberFormatSymbol InSymbolToExtract) -> TCHAR
	{
		const icu::UnicodeString& ICUSymbolString = InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(InSymbolToExtract);
		return ICUStringToTCHAR(ICUSymbolString); // For efficiency we assume that these symbols are always a single character
	};

	icu::UnicodeString ScratchICUString;

	// Extract the rules from the decimal formatter
	NewUEDecimalNumberFormattingRules.NaNString						= ICUUtilities::ConvertString(InICUDecimalFormat.getDecimalFormatSymbols()->getConstSymbol(icu::DecimalFormatSymbols::kNaNSymbol));
	NewUEDecimalNumberFormattingRules.NegativePrefixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getNegativePrefix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.NegativeSuffixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getNegativeSuffix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.PositivePrefixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getPositivePrefix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.PositiveSuffixString			= ICUUtilities::ConvertString(InICUDecimalFormat.getPositiveSuffix(ScratchICUString));
	NewUEDecimalNumberFormattingRules.GroupingSeparatorCharacter	= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol);
	NewUEDecimalNumberFormattingRules.DecimalSeparatorCharacter		= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol);
	NewUEDecimalNumberFormattingRules.PrimaryGroupingSize			= static_cast<uint8>(InICUDecimalFormat.getGroupingSize());
	NewUEDecimalNumberFormattingRules.SecondaryGroupingSize			= (InICUDecimalFormat.getSecondaryGroupingSize() < 1) 
																		? NewUEDecimalNumberFormattingRules.PrimaryGroupingSize 
																		: static_cast<uint8>(InICUDecimalFormat.getSecondaryGroupingSize());

	NewUEDecimalNumberFormattingRules.DigitCharacters[0]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kZeroDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[1]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kOneDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[2]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kTwoDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[3]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kThreeDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[4]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kFourDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[5]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kFiveDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[6]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kSixDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[7]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kSevenDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[8]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kEightDigitSymbol);
	NewUEDecimalNumberFormattingRules.DigitCharacters[9]			= ExtractFormattingSymbolAsCharacter(icu::DecimalFormatSymbols::kNineDigitSymbol);

	return NewUEDecimalNumberFormattingRules;
}

} // anonymous namespace

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetDecimalNumberFormattingRules()
{
	if (UEDecimalNumberFormattingRules.IsValid())
	{
		return *UEDecimalNumberFormattingRules;
	}

	// Create a culture decimal formatter (doesn't call CreateDecimalFormat as we need a mutable instance)
	TSharedPtr<icu::DecimalFormat> DecimalFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		DecimalFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createInstance(ICULocale, ICUStatus)));
		checkf(DecimalFormatterForCulture.IsValid(), TEXT("Creating a decimal format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	const FDecimalNumberFormattingRules NewUEDecimalNumberFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*DecimalFormatterForCulture);

	// Check the pointer again in case another thread beat us to it
	{
		FScopeLock PtrLock(&UEDecimalNumberFormattingRulesCS);

		if (!UEDecimalNumberFormattingRules.IsValid())
		{
			UEDecimalNumberFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUEDecimalNumberFormattingRules));
		}
	}

	return *UEDecimalNumberFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetPercentFormattingRules()
{
	if (UEPercentFormattingRules.IsValid())
	{
		return *UEPercentFormattingRules;
	}

	// Create a culture percent formatter (doesn't call CreatePercentFormat as we need a mutable instance)
	TSharedPtr<icu::DecimalFormat> PercentFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		PercentFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createPercentInstance(ICULocale, ICUStatus)));
		checkf(PercentFormatterForCulture.IsValid(), TEXT("Creating a percent format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	const FDecimalNumberFormattingRules NewUEPercentFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*PercentFormatterForCulture);

	// Check the pointer again in case another thread beat us to it
	{
		FScopeLock PtrLock(&UEPercentFormattingRulesCS);

		if (!UEPercentFormattingRules.IsValid())
		{
			UEPercentFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUEPercentFormattingRules));
		}
	}

	return *UEPercentFormattingRules;
}

const FDecimalNumberFormattingRules& FCulture::FICUCultureImplementation::GetCurrencyFormattingRules(const FString& InCurrencyCode)
{
	const FString SanitizedCurrencyCode = ICUUtilities::SanitizeCurrencyCode(InCurrencyCode);
	const bool bUseDefaultFormattingRules = SanitizedCurrencyCode.IsEmpty();

	if (bUseDefaultFormattingRules)
	{
		if (UECurrencyFormattingRules.IsValid())
		{
			return *UECurrencyFormattingRules;
		}
	}
	else
	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(SanitizedCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}
	}

	// Create a currency specific formatter (doesn't call CreateCurrencyFormat as we need a mutable instance)
	TSharedPtr<icu::DecimalFormat> CurrencyFormatterForCulture;
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		CurrencyFormatterForCulture = MakeShareable(static_cast<icu::DecimalFormat*>(icu::NumberFormat::createCurrencyInstance(ICULocale, ICUStatus)));
		checkf(CurrencyFormatterForCulture.IsValid(), TEXT("Creating a currency format object failed using locale %s. Perhaps this locale has no data."), StringCast<TCHAR>(ICULocale.getName()).Get());
	}

	if (!bUseDefaultFormattingRules)
	{
		// Set the custom currency before we extract the data from the formatter
		icu::UnicodeString ICUCurrencyCode = ICUUtilities::ConvertString(SanitizedCurrencyCode);
		CurrencyFormatterForCulture->setCurrency(ICUCurrencyCode.getBuffer());
	}

	const FDecimalNumberFormattingRules NewUECurrencyFormattingRules = ExtractNumberFormattingRulesFromICUDecimalFormatter(*CurrencyFormatterForCulture);

	if (bUseDefaultFormattingRules)
	{
		// Check the pointer again in case another thread beat us to it
		{
			FScopeLock PtrLock(&UECurrencyFormattingRulesCS);

			if (!UECurrencyFormattingRules.IsValid())
			{
				UECurrencyFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUECurrencyFormattingRules));
			}
		}

		return *UECurrencyFormattingRules;
	}
	else
	{
		FScopeLock MapLock(&UEAlternateCurrencyFormattingRulesCS);

		// Find again in case another thread beat us to it
		auto FoundUEAlternateCurrencyFormattingRules = UEAlternateCurrencyFormattingRules.FindRef(SanitizedCurrencyCode);
		if (FoundUEAlternateCurrencyFormattingRules.IsValid())
		{
			return *FoundUEAlternateCurrencyFormattingRules;
		}

		FoundUEAlternateCurrencyFormattingRules = MakeShareable(new FDecimalNumberFormattingRules(NewUECurrencyFormattingRules));
		UEAlternateCurrencyFormattingRules.Add(SanitizedCurrencyCode, FoundUEAlternateCurrencyFormattingRules);
		return *FoundUEAlternateCurrencyFormattingRules;
	}
}

ETextPluralForm FCulture::FICUCultureImplementation::GetPluralForm(int32 Val, const ETextPluralType PluralType)
{
	checkf(Val >= 0, TEXT("GetPluralFormImpl requires a positive value"));

	const icu::PluralRules* ICUPluralRules = (PluralType == ETextPluralType::Cardinal) ? ICUCardinalPluralRules : ICUOrdianalPluralRules;
	const icu::UnicodeString ICUPluralFormTag = ICUPluralRules->select(Val);

	return ICUPluralFormToUE(ICUPluralFormTag);
}

ETextPluralForm FCulture::FICUCultureImplementation::GetPluralForm(double Val, const ETextPluralType PluralType)
{
	checkf(!FMath::IsNegativeDouble(Val), TEXT("GetPluralFormImpl requires a positive value"));

	const icu::PluralRules* ICUPluralRules = (PluralType == ETextPluralType::Cardinal) ? ICUCardinalPluralRules : ICUOrdianalPluralRules;
	const icu::UnicodeString ICUPluralFormTag = ICUPluralRules->select(Val);

	return ICUPluralFormToUE(ICUPluralFormTag);
}

#endif
