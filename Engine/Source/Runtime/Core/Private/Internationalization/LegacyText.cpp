// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/DateTime.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextChronoFormatter.h"
#include "Internationalization/TextTransformer.h"
#include "Internationalization/Internationalization.h"

#if !UE_ENABLE_ICU
#include "Text.h"
#include "TextData.h"

FString FTextChronoFormatter::AsDate( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const FString& TimeZone, const FCulture& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return DateTime.ToString(TEXT("%Y.%m.%d"));
}

FString FTextChronoFormatter::AsTime( const FDateTime& DateTime, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulture& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return DateTime.ToString(TEXT("%H.%M.%S"));
}

FString FTextChronoFormatter::AsDateTime( const FDateTime& DateTime, const EDateTimeStyle::Type DateStyle, const EDateTimeStyle::Type TimeStyle, const FString& TimeZone, const FCulture& TargetCulture )
{
	checkf(FInternationalization::Get().IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	return DateTime.ToString(TEXT("%Y.%m.%d-%H.%M.%S"));
}

FString FTextTransformer::ToLower(const FString& InStr)
{
	return InStr.ToLower();
}

FString FTextTransformer::ToUpper(const FString& InStr)
{
	return InStr.ToUpper();
}

bool FText::IsWhitespace(const TCHAR Char)
{
	return FChar::IsWhitespace(Char);
}

int32 FText::CompareTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return ToString().Compare(Other.ToString(), ESearchCase::CaseSensitive);
}

int32 FText::CompareToCaseIgnored( const FText& Other ) const
{
	return ToString().Compare(Other.ToString(), ESearchCase::IgnoreCase);
}

bool FText::EqualTo( const FText& Other, const ETextComparisonLevel::Type ComparisonLevel ) const
{
	return ToString().Equals(Other.ToString(), ESearchCase::CaseSensitive);
}

bool FText::EqualToCaseIgnored( const FText& Other ) const
{
	return ToString().Equals(Other.ToString(), ESearchCase::IgnoreCase);
}

FText::FSortPredicate::FSortPredicate(const ETextComparisonLevel::Type ComparisonLevel)
{

}

bool FText::FSortPredicate::operator()(const FText& A, const FText& B) const
{
	return A.ToString() < B.ToString();
}

bool FUnicodeChar::CodepointToString(const uint32 InCodepoint, FString& OutString)
{
	return false;
}

namespace TextBiDi
{

namespace Internal
{

class FLegacyTextBiDi : public ITextBiDi
{
public:
	virtual ETextDirection ComputeTextDirection(const FText& InText) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(InText.ToString());
	}

	virtual ETextDirection ComputeTextDirection(const FString& InString) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(*InString, 0, InString.Len());
	}

	virtual ETextDirection ComputeTextDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen) override
	{
		return ETextDirection::LeftToRight;
	}

	virtual ETextDirection ComputeTextDirection(const FText& InText, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(InText.ToString(), InBaseDirection, OutTextDirectionInfo);
	}

	virtual ETextDirection ComputeTextDirection(const FString& InString, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		return FLegacyTextBiDi::ComputeTextDirection(*InString, 0, InString.Len(), InBaseDirection, OutTextDirectionInfo);
	}

	virtual ETextDirection ComputeTextDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo) override
	{
		OutTextDirectionInfo.Reset();

		if (InStringLen > 0)
		{
			FTextDirectionInfo TextDirectionInfo;
			TextDirectionInfo.StartIndex = InStringStartIndex;
			TextDirectionInfo.Length = InStringLen;
			TextDirectionInfo.TextDirection = ETextDirection::LeftToRight;
			OutTextDirectionInfo.Add(MoveTemp(TextDirectionInfo));
		}

		return ETextDirection::LeftToRight;
	}

	virtual ETextDirection ComputeBaseDirection(const FText& InText) override
	{
		return FLegacyTextBiDi::ComputeBaseDirection(InText.ToString());
	}

	virtual ETextDirection ComputeBaseDirection(const FString& InString) override
	{
		return FLegacyTextBiDi::ComputeBaseDirection(*InString, 0, InString.Len());
	}

	virtual ETextDirection ComputeBaseDirection(const TCHAR*, const int32 InStringStartIndex, const int32 InStringLen) override
	{
		return ETextDirection::LeftToRight;
	}
};

} // namespace Internal

TUniquePtr<ITextBiDi> CreateTextBiDi()
{
	return MakeUnique<Internal::FLegacyTextBiDi>();
}

ETextDirection ComputeTextDirection(const FText& InText)
{
	return ComputeTextDirection(InText.ToString());
}

ETextDirection ComputeTextDirection(const FString& InString)
{
	return ComputeTextDirection(*InString, 0, InString.Len());
}

ETextDirection ComputeTextDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen)
{
	return ETextDirection::LeftToRight;
}

ETextDirection ComputeTextDirection(const FText& InText, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	return ComputeTextDirection(InText.ToString(), InBaseDirection, OutTextDirectionInfo);
}

ETextDirection ComputeTextDirection(const FString& InString, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	return ComputeTextDirection(*InString, 0, InString.Len(), InBaseDirection, OutTextDirectionInfo);
}

ETextDirection ComputeTextDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen, const ETextDirection InBaseDirection, TArray<FTextDirectionInfo>& OutTextDirectionInfo)
{
	OutTextDirectionInfo.Reset();

	if (InStringLen > 0)
	{
		FTextDirectionInfo TextDirectionInfo;
		TextDirectionInfo.StartIndex = InStringStartIndex;
		TextDirectionInfo.Length = InStringLen;
		TextDirectionInfo.TextDirection = ETextDirection::LeftToRight;
		OutTextDirectionInfo.Add(MoveTemp(TextDirectionInfo));
	}

	return ETextDirection::LeftToRight;
}

ETextDirection ComputeBaseDirection(const FText& InText)
{
	return ComputeBaseDirection(InText.ToString());
}

ETextDirection ComputeBaseDirection(const FString& InString)
{
	return ComputeBaseDirection(*InString, 0, InString.Len());
}

ETextDirection ComputeBaseDirection(const TCHAR* InString, const int32 InStringStartIndex, const int32 InStringLen)
{
	return ETextDirection::LeftToRight;
}

} // namespace TextBiDi

#endif
