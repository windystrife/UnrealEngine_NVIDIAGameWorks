// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"

struct FDecimalNumberFormattingRules;

#if UE_ENABLE_ICU
THIRD_PARTY_INCLUDES_START
	#include <unicode/locid.h>
	#include <unicode/brkiter.h>
	#include <unicode/coll.h>
	#include <unicode/numfmt.h>
	#include <unicode/decimfmt.h>
	#include <unicode/datefmt.h>
	#include <unicode/plurrule.h>
THIRD_PARTY_INCLUDES_END

struct FDecimalNumberFormattingRules;

inline UColAttributeValue UEToICU(const ETextComparisonLevel::Type ComparisonLevel)
{
	UColAttributeValue Value;
	switch(ComparisonLevel)
	{
	case ETextComparisonLevel::Default:
		Value = UColAttributeValue::UCOL_DEFAULT;
		break;
	case ETextComparisonLevel::Primary:
		Value = UColAttributeValue::UCOL_PRIMARY;
		break;
	case ETextComparisonLevel::Secondary:
		Value = UColAttributeValue::UCOL_SECONDARY;
		break;
	case ETextComparisonLevel::Tertiary:
		Value = UColAttributeValue::UCOL_TERTIARY;
		break;
	case ETextComparisonLevel::Quaternary:
		Value = UColAttributeValue::UCOL_QUATERNARY;
		break;
	case ETextComparisonLevel::Quinary:
		Value = UColAttributeValue::UCOL_IDENTICAL;
		break;
	default:
		Value = UColAttributeValue::UCOL_DEFAULT;
		break;
	}
	return Value;
}

inline icu::DateFormat::EStyle UEToICU(const EDateTimeStyle::Type DateTimeStyle)
{
	icu::DateFormat::EStyle Value;
	switch(DateTimeStyle)
	{
	case EDateTimeStyle::Short:
		Value = icu::DateFormat::kShort;
		break;
	case EDateTimeStyle::Medium:
		Value = icu::DateFormat::kMedium;
		break;
	case EDateTimeStyle::Long:
		Value = icu::DateFormat::kLong;
		break;
	case EDateTimeStyle::Full:
		Value = icu::DateFormat::kFull;
		break;
	case EDateTimeStyle::Default:
		Value = icu::DateFormat::kDefault;
		break;
	default:
		Value = icu::DateFormat::kDefault;
		break;
	}
	return Value;
}

inline icu::DecimalFormat::ERoundingMode UEToICU(const ERoundingMode RoundingMode)
{
	icu::DecimalFormat::ERoundingMode Value;
	switch(RoundingMode)
	{
	case ERoundingMode::HalfToEven:
		Value = icu::DecimalFormat::ERoundingMode::kRoundHalfEven;
		break;
	case ERoundingMode::HalfFromZero:
		Value = icu::DecimalFormat::ERoundingMode::kRoundHalfUp;
		break;
	case ERoundingMode::HalfToZero:
		Value = icu::DecimalFormat::ERoundingMode::kRoundHalfDown;
		break;
	case ERoundingMode::FromZero:
		Value = icu::DecimalFormat::ERoundingMode::kRoundUp;
		break;
	case ERoundingMode::ToZero:
		Value = icu::DecimalFormat::ERoundingMode::kRoundDown;
		break;
	case ERoundingMode::ToNegativeInfinity:
		Value = icu::DecimalFormat::ERoundingMode::kRoundFloor;
		break;
	case ERoundingMode::ToPositiveInfinity:
		Value = icu::DecimalFormat::ERoundingMode::kRoundCeiling;
		break;
	default:
		Value = icu::DecimalFormat::ERoundingMode::kRoundHalfEven;
		break;
	}
	return Value;
}

inline ERoundingMode ICUToUE(const icu::DecimalFormat::ERoundingMode RoundingMode)
{
	ERoundingMode Value;
	switch(RoundingMode)
	{
	case icu::DecimalFormat::ERoundingMode::kRoundHalfEven:
		Value = ERoundingMode::HalfToEven;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundHalfUp:
		Value = ERoundingMode::HalfFromZero;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundHalfDown:
		Value = ERoundingMode::HalfToZero;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundUp:
		Value = ERoundingMode::FromZero;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundDown:
		Value = ERoundingMode::ToZero;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundFloor:
		Value = ERoundingMode::ToNegativeInfinity;
		break;
	case icu::DecimalFormat::ERoundingMode::kRoundCeiling:
		Value = ERoundingMode::ToPositiveInfinity;
		break;
	default:
		Value = ERoundingMode::HalfToEven;
		break;
	}
	return Value;
}

ETextPluralForm ICUPluralFormToUE(const icu::UnicodeString& InICUTag);

enum class EBreakIteratorType
{
	Grapheme,
	Word,
	Line,
	Sentence,
	Title
};

/**
 * Basic Least Recently Used (LRU) cache to potentially keep ICU formatters alive for multiple format calls
 */
template <typename TFormatterType, ESPMode Mode>
class FLRUFormatterCache
{
public:
	typedef TSharedPtr<const TFormatterType, Mode> ValueType;

	FLRUFormatterCache( int32 InMaxNumElements )
		: LookupSet()
		, MostRecent(nullptr)
		, LeastRecent(nullptr)
		, MaxNumElements(InMaxNumElements)
	{
	}

	~FLRUFormatterCache()
	{
		Empty();
	}

	/**
	 * Accesses an item in the cache.  
	 */
	FORCEINLINE const ValueType AccessItem(const FNumberFormattingOptions& Key)
	{
		CacheEntry** Entry = LookupSet.Find( Key );
		if( Entry )
		{
			MarkAsRecent( *Entry );
			return ((*Entry)->Value);
		}
		
		return nullptr;
	}

	void Add( const FNumberFormattingOptions& Key, const ValueType& Value )
	{
		CacheEntry** Entry = LookupSet.Find( Key );
	
		// Make a new link
		if( !Entry )
		{
			if( LookupSet.Num() == MaxNumElements )
			{
				Eject();
				checkf( LookupSet.Num() < MaxNumElements, TEXT("Could not eject item from the LRU: (%d of %d)"), LookupSet.Num(), MaxNumElements );
			}

			CacheEntry* NewEntry = new CacheEntry( Key, Value );

			// Link before the most recent so that we become the most recent
			NewEntry->Link(MostRecent);
			MostRecent = NewEntry;

			if( LeastRecent == nullptr )
			{
				LeastRecent = NewEntry;
			}

			LookupSet.Add( NewEntry );
		}
		else
		{
			// Trying to add an existing value 
			CacheEntry* EntryPtr = *Entry;
			
			// Update the value
			EntryPtr->Value = Value;
			checkSlow( EntryPtr->Key.IsIdentical( Key ) );
			// Mark as the most recent
			MarkAsRecent( EntryPtr );
		}
	}

	void Empty()
	{
		for( auto& Entry : LookupSet )
		{
			// Note no need to unlink anything here. we are emptying the entire list
			delete Entry;
		}
		LookupSet.Empty();

		MostRecent = LeastRecent = nullptr;
	}

private:
	struct CacheEntry
	{
		FNumberFormattingOptions Key;
		ValueType Value;
		CacheEntry* Next;
		CacheEntry* Prev;

		CacheEntry( const FNumberFormattingOptions& InKey, const ValueType& InValue )
			: Key( InKey )
			, Value( InValue )
			, Next( nullptr )
			, Prev( nullptr )
		{
		}

		~CacheEntry()
		{
		}

		FORCEINLINE void Link( CacheEntry* Before )
		{
			Next = Before;

			if( Before )
			{
				Before->Prev = this;
			}
		}

		FORCEINLINE void Unlink()
		{
			if( Prev )
			{
				Prev->Next = Next;
			}

			if( Next )
			{
				Next->Prev = Prev;
			}

			Prev = nullptr;
			Next = nullptr;
		}
	};

	struct FNumberFormattingOptionsKeyFuncs : BaseKeyFuncs<CacheEntry*, FNumberFormattingOptions>
	{
		FORCEINLINE static const FNumberFormattingOptions& GetSetKey( const CacheEntry* Entry )
		{
			return Entry->Key;
		}

		FORCEINLINE static bool Matches(const FNumberFormattingOptions& A,const FNumberFormattingOptions& B)
		{
			return A.IsIdentical( B );
		}

		FORCEINLINE static uint32 GetKeyHash(const FNumberFormattingOptions& Identifier)
		{
			return GetTypeHash( Identifier );
		}
	};

	/**
	 * Marks the link as the the most recent
	 *
	 * @param Link	The link to mark as most recent
	 */
	FORCEINLINE void MarkAsRecent( CacheEntry* Entry )
	{
		checkSlow( LeastRecent && MostRecent );

		// If we are the least recent entry we are no longer the least recent
		// The previous least recent item is now the most recent.  If it is nullptr then this entry is the only item in the list
		if( Entry == LeastRecent && LeastRecent->Prev != nullptr )
		{
			LeastRecent = LeastRecent->Prev;
		}

		// No need to relink if we happen to already be accessing the most recent value
		if( Entry != MostRecent )
		{
			// Unlink from its current spot
			Entry->Unlink();

			// Relink before the most recent so that we become the most recent
			Entry->Link(MostRecent);
			MostRecent = Entry;
		}
	}

	/**
	 * Removes the least recent item from the cache
	 */
	FORCEINLINE void Eject()
	{
		CacheEntry* EntryToRemove = LeastRecent;
		// Eject the least recent, no more space
		check( EntryToRemove );
		LookupSet.Remove( EntryToRemove->Key );

		LeastRecent = LeastRecent->Prev;

		// Unlink the LRU
		EntryToRemove->Unlink();

		delete EntryToRemove;
	}

private:
	TSet< CacheEntry*, FNumberFormattingOptionsKeyFuncs > LookupSet;
	/** Most recent item in the cache */
	CacheEntry* MostRecent;
	/** Least recent item in the cache */
	CacheEntry* LeastRecent;
	/** The maximum number of elements in the cache */
	int32 MaxNumElements;
};

class FCulture::FICUCultureImplementation
{
	friend class FCulture;
	friend class FText;
	friend class FTextChronoFormatter;
	friend class FICUBreakIteratorManager;

	FICUCultureImplementation(const FString& LocaleName);

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

	FString GetRegion() const;

	FString GetNativeRegion() const;

	FString GetScript() const;

	FString GetVariant() const;

	TSharedRef<const icu::BreakIterator> GetBreakIterator(const EBreakIteratorType Type);
	TSharedRef<const icu::Collator, ESPMode::ThreadSafe> GetCollator(const ETextComparisonLevel::Type ComparisonLevel);
	TSharedRef<const icu::DecimalFormat, ESPMode::ThreadSafe> GetDecimalFormatter(const FNumberFormattingOptions* const Options = NULL);
	TSharedRef<const icu::DecimalFormat> GetCurrencyFormatter(const FString& CurrencyCode = FString(), const FNumberFormattingOptions* const Options = NULL);
	TSharedRef<const icu::DecimalFormat> GetPercentFormatter(const FNumberFormattingOptions* const Options = NULL);
	TSharedRef<const icu::DateFormat> GetDateFormatter(const EDateTimeStyle::Type DateStyle, const FString& TimeZone);
	TSharedRef<const icu::DateFormat> GetTimeFormatter(const EDateTimeStyle::Type TimeStyle, const FString& TimeZone);
	TSharedRef<const icu::DateFormat> GetDateTimeFormatter(const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone);

	const FDecimalNumberFormattingRules& GetDecimalNumberFormattingRules();
	const FDecimalNumberFormattingRules& GetPercentFormattingRules();
	const FDecimalNumberFormattingRules& GetCurrencyFormattingRules(const FString& InCurrencyCode);

	ETextPluralForm GetPluralForm(int32 Val, const ETextPluralType PluralType);
	ETextPluralForm GetPluralForm(double Val, const ETextPluralType PluralType);

	icu::Locale ICULocale;
	TSharedPtr<const icu::BreakIterator> ICUGraphemeBreakIterator;
	TSharedPtr<const icu::BreakIterator> ICUWordBreakIterator;
	TSharedPtr<const icu::BreakIterator> ICULineBreakIterator;
	TSharedPtr<const icu::BreakIterator> ICUSentenceBreakIterator;
	TSharedPtr<const icu::BreakIterator> ICUTitleBreakIterator;
	TSharedPtr<const icu::Collator, ESPMode::ThreadSafe> ICUCollator;

	TSharedPtr<const icu::DecimalFormat, ESPMode::ThreadSafe> ICUDecimalFormat_DefaultForCulture;
	TSharedPtr<const icu::DecimalFormat, ESPMode::ThreadSafe> ICUDecimalFormat_DefaultWithGrouping;
	TSharedPtr<const icu::DecimalFormat, ESPMode::ThreadSafe> ICUDecimalFormat_DefaultNoGrouping;
	FLRUFormatterCache<icu::DecimalFormat, ESPMode::ThreadSafe> ICUDecimalFormatLRUCache;
	FCriticalSection ICUDecimalFormatLRUCacheCS;

	TSharedPtr<const icu::DecimalFormat> ICUCurrencyFormat;
	TSharedPtr<const icu::DecimalFormat> ICUPercentFormat;
	TSharedPtr<const icu::DateFormat> ICUDateFormat;
	TSharedPtr<const icu::DateFormat> ICUTimeFormat;
	TSharedPtr<const icu::DateFormat> ICUDateTimeFormat;

	const icu::PluralRules* ICUCardinalPluralRules;
	const icu::PluralRules* ICUOrdianalPluralRules;

	TSharedPtr<const FDecimalNumberFormattingRules, ESPMode::ThreadSafe> UEDecimalNumberFormattingRules;
	FCriticalSection UEDecimalNumberFormattingRulesCS;

	TSharedPtr<const FDecimalNumberFormattingRules, ESPMode::ThreadSafe> UEPercentFormattingRules;
	FCriticalSection UEPercentFormattingRulesCS;

	TSharedPtr<const FDecimalNumberFormattingRules, ESPMode::ThreadSafe> UECurrencyFormattingRules;
	FCriticalSection UECurrencyFormattingRulesCS;

	TMap<FString, TSharedPtr<const FDecimalNumberFormattingRules>> UEAlternateCurrencyFormattingRules;
	FCriticalSection UEAlternateCurrencyFormattingRulesCS;
};
#endif
