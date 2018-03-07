// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/ICUTextCharacterIterator.h"
#include "Internationalization/Text.h"

#if UE_ENABLE_ICU

#include "Internationalization/ICUUtilities.h"

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FICUTextCharacterIterator_NativeUTF16)

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FText& InText)
	: InternalString(InText.ToString())
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FString& InString)
	: InternalString(InString)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const TCHAR* const InString, const int32 InStringLength)
	: InternalString(InString, InStringLength)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(FString&& InString)
	: InternalString(InString)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FString* const InString)
	: StringPtr(InString)
{
	check(StringPtr);
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::FICUTextCharacterIterator_NativeUTF16(const FICUTextCharacterIterator_NativeUTF16& Other)
	: icu::UCharCharacterIterator(Other)
	, InternalString(*Other.StringPtr)
	, StringPtr(&InternalString)
{
	setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
}

FICUTextCharacterIterator_NativeUTF16::~FICUTextCharacterIterator_NativeUTF16()
{
}

FICUTextCharacterIterator_NativeUTF16& FICUTextCharacterIterator_NativeUTF16::operator=(const FICUTextCharacterIterator_NativeUTF16& Other)
{
	if(this != &Other)
	{
		icu::UCharCharacterIterator::operator=(Other);
		InternalString = *Other.StringPtr;
		StringPtr = &InternalString;
		setText(reinterpret_cast<const UChar*>(**StringPtr), StringPtr->Len()); // scary cast from TCHAR* to UChar* so that this builds on platforms where TCHAR isn't UTF-16 (but we won't use it!)
	}
	return *this;
}

int32 FICUTextCharacterIterator_NativeUTF16::InternalIndexToSourceIndex(const int32 InInternalIndex) const
{
	// If the UTF-16 variant is being used, then FString must be UTF-16 so no conversion is required
	return InInternalIndex;
}

int32 FICUTextCharacterIterator_NativeUTF16::SourceIndexToInternalIndex(const int32 InSourceIndex) const
{
	// If the UTF-16 variant is being used, then FString must be UTF-16 so no conversion is required
	return InSourceIndex;
}

icu::CharacterIterator* FICUTextCharacterIterator_NativeUTF16::clone() const
{
	return new FICUTextCharacterIterator_NativeUTF16(*this);
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FICUTextCharacterIterator_ConvertToUnicodeString)

FICUTextCharacterIterator_ConvertToUnicodeStringPrivate::FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(const FString& InString)
	: SourceString(InString)
	, InternalString(ICUUtilities::ConvertString(SourceString))
{
}

FICUTextCharacterIterator_ConvertToUnicodeStringPrivate::FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(FString&& InString)
	: SourceString(MoveTemp(InString))
	, InternalString(ICUUtilities::ConvertString(SourceString))
{
}

FICUTextCharacterIterator_ConvertToUnicodeStringPrivate::FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(const FICUTextCharacterIterator_ConvertToUnicodeStringPrivate& Other)
	: SourceString(Other.SourceString)
	, InternalString(Other.InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeStringPrivate& FICUTextCharacterIterator_ConvertToUnicodeStringPrivate::operator=(const FICUTextCharacterIterator_ConvertToUnicodeStringPrivate& Other)
{
	if (this != &Other)
	{
		SourceString = Other.SourceString;
		InternalString = Other.InternalString;
	}
	return *this;
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FText& InText)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(InText.ToString())
	, icu::StringCharacterIterator(InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FString& InString)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(InString)
	, icu::StringCharacterIterator(InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const TCHAR* const InString, const int32 InStringLength)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(FString(InString, InStringLength))
	, icu::StringCharacterIterator(InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(FString&& InString)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(MoveTemp(InString))
	, icu::StringCharacterIterator(InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FString* const InString)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(*InString)
	, icu::StringCharacterIterator(InternalString)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::FICUTextCharacterIterator_ConvertToUnicodeString(const FICUTextCharacterIterator_ConvertToUnicodeString& Other)
	: FICUTextCharacterIterator_ConvertToUnicodeStringPrivate(Other)
	, icu::StringCharacterIterator(Other)
{
}

FICUTextCharacterIterator_ConvertToUnicodeString::~FICUTextCharacterIterator_ConvertToUnicodeString()
{
}

FICUTextCharacterIterator_ConvertToUnicodeString& FICUTextCharacterIterator_ConvertToUnicodeString::operator=(const FICUTextCharacterIterator_ConvertToUnicodeString& Other)
{
	if(this != &Other)
	{
		FICUTextCharacterIterator_ConvertToUnicodeStringPrivate::operator=(Other);
		icu::StringCharacterIterator::operator=(Other);
	}
	return *this;
}

int32 FICUTextCharacterIterator_ConvertToUnicodeString::InternalIndexToSourceIndex(const int32 InInternalIndex) const
{
	// Convert from the ICU UTF-16 index to whatever FString needs
	return InInternalIndex == INDEX_NONE ? INDEX_NONE : ICUUtilities::GetNativeStringLength(InternalString, 0, InInternalIndex);
}

int32 FICUTextCharacterIterator_ConvertToUnicodeString::SourceIndexToInternalIndex(const int32 InSourceIndex) const
{
	// Convert from whatever FString is to ICU UTF-16
	return InSourceIndex == INDEX_NONE ? INDEX_NONE : ICUUtilities::GetUnicodeStringLength(*SourceString, 0, InSourceIndex);
}

icu::CharacterIterator* FICUTextCharacterIterator_ConvertToUnicodeString::clone() const
{
	return new FICUTextCharacterIterator_ConvertToUnicodeString(*this);
}

#endif
