// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextHistory.h"
#include "UObject/ObjectVersion.h"
#include "Internationalization/ITextData.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "UObject/PropertyPortFlags.h"

#include "Internationalization/FastDecimalFormat.h"
#include "Internationalization/TextFormatter.h"
#include "Internationalization/TextChronoFormatter.h"
#include "Internationalization/TextTransformer.h"
#include "Internationalization/TextNamespaceUtil.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTextHistory, Log, All);
DEFINE_LOG_CATEGORY(LogTextHistory);

///////////////////////////////////////
// FTextHistory

/** Base class for all FText history types */

FTextHistory::FTextHistory()
	: Revision(FTextLocalizationManager::Get().GetTextRevision())
{
}

FTextHistory::FTextHistory(FTextHistory&& Other)
	: Revision(MoveTemp(Other.Revision))
{
}

FTextHistory& FTextHistory::operator=(FTextHistory&& Other)
{
	if (this != &Other)
	{
		Revision = Other.Revision;
	}
	return *this;
}

bool FTextHistory::IsOutOfDate() const
{
	return Revision != FTextLocalizationManager::Get().GetTextRevision();
}

const FString* FTextHistory::GetSourceString() const
{
	return NULL;
}

void FTextHistory::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
}

bool FTextHistory::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	return false;
}

void FTextHistory::SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString)
{
	if(Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;

		//When duplicating, the CDO is used as the template, then values for the instance are assigned.
		//If we don't duplicate the string, the CDO and the instance are both pointing at the same thing.
		//This would result in all subsequently duplicated objects stamping over formerly duplicated ones.
		InOutDisplayString = MakeShared<FString, ESPMode::ThreadSafe>();
	}
}

void FTextHistory::Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString)
{
	const bool bIsOutOfDate = IsOutOfDate();
	if(bIsOutOfDate)
	{
		// FTextHistory_Base will never report being able to rebuild its text, but we need to keep the history 
		// revision in sync with the head culture so that FTextSnapshot::IdenticalTo still works correctly
		Revision = FTextLocalizationManager::Get().GetTextRevision();

		const bool bCanRebuildLocalizedDisplayString = CanRebuildLocalizedDisplayString();
		if(bCanRebuildLocalizedDisplayString)
		{
			InDisplayString.Get() = BuildLocalizedDisplayString();
		}
	}
}

///////////////////////////////////////
// FTextHistory_Base

FTextHistory_Base::FTextHistory_Base(FString&& InSourceString)
	: SourceString(MoveTemp(InSourceString))
{
}

FTextHistory_Base::FTextHistory_Base(FTextHistory_Base&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceString(MoveTemp(Other.SourceString))
{
}

FTextHistory_Base& FTextHistory_Base::operator=(FTextHistory_Base&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceString = MoveTemp(Other.SourceString);
	}
	return *this;
}

FString FTextHistory_Base::BuildLocalizedDisplayString() const
{
	// This should never be called for base text (CanRebuildLocalizedDisplayString is false)
	check(0);
	return FString();
}

FString FTextHistory_Base::BuildInvariantDisplayString() const
{
	return SourceString;
}

const FString* FTextHistory_Base::GetSourceString() const
{
	return &SourceString;
}

void FTextHistory_Base::Serialize( FArchive& Ar )
{
	// If I serialize out the Namespace and Key HERE, then we can load it up.
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::Base;
		Ar << HistoryType;
	}
}

void FTextHistory_Base::SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString)
{
	if(Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;

		FString Namespace;
		FString Key;

		Ar << Namespace;
		Ar << Key;
		Ar << SourceString;

#if USE_STABLE_LOCALIZATION_KEYS
		// Make sure the package namespace for this text property is up-to-date
		// We do this on load (as well as save) to handle cases where data is being duplicated, as it will be written by one package and loaded into another
		if (GIsEditor && !Ar.HasAnyPortFlags(PPF_DuplicateVerbatim | PPF_DuplicateForPIE))
		{
			const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(Ar);
			if (!PackageNamespace.IsEmpty())
			{
				const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
				if (!Namespace.Equals(FullNamespace, ESearchCase::CaseSensitive))
				{
					// We may assign a new key when loading if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
					// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
					Namespace = FullNamespace;
					Key = FGuid::NewGuid().ToString();
				}
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
#if WITH_EDITOR
		if (!GIsEditor)
		{
			// Strip the package localization ID to match how text works at runtime (properties do this when saving during cook)
			Namespace = TextNamespaceUtil::StripPackageNamespace(Namespace);
		}
#endif // WITH_EDITOR

		// Using the deserialized namespace and key, find the DisplayString.
		InOutDisplayString = FTextLocalizationManager::Get().GetDisplayString(Namespace, Key, &SourceString);
	}
	else if(Ar.IsSaving())
	{
		check(InOutDisplayString.IsValid());

		FString Namespace;
		FString Key;
		const bool bFoundNamespaceAndKey = FTextLocalizationManager::Get().FindNamespaceAndKeyFromDisplayString(InOutDisplayString.ToSharedRef(), Namespace, Key);

		if (Ar.IsCooking())
		{
			// We strip the package localization off the serialized text for a cooked game, as they're not used at runtime
			Namespace = TextNamespaceUtil::StripPackageNamespace(Namespace);
		}
		else
#if USE_STABLE_LOCALIZATION_KEYS
		// Make sure the package namespace for this text property is up-to-date
		if (GIsEditor && !Ar.HasAnyPortFlags(PPF_DuplicateVerbatim | PPF_DuplicateForPIE))
		{
			const FString PackageNamespace = TextNamespaceUtil::GetPackageNamespace(Ar);
			if (!PackageNamespace.IsEmpty())
			{
				const FString FullNamespace = TextNamespaceUtil::BuildFullNamespace(Namespace, PackageNamespace);
				if (!Namespace.Equals(FullNamespace, ESearchCase::CaseSensitive))
				{
					// We may assign a new key when saving if we don't have the correct package namespace in order to avoid identity conflicts when instancing (which duplicates without any special flags)
					// This can happen if an asset was duplicated (and keeps the same keys) but later both assets are instanced into the same world (causing them to both take the worlds package id, and conflict with each other)
					Namespace = FullNamespace;
					Key = FGuid::NewGuid().ToString();
				}
			}
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		// If this has no key, give it a GUID for a key
		if (!bFoundNamespaceAndKey && GIsEditor && (Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate)))
		{
			Key = FGuid::NewGuid().ToString();
			if (!FTextLocalizationManager::Get().AddDisplayString(InOutDisplayString.ToSharedRef(), Namespace, Key))
			{
				// Could not add display string, reset namespace and key.
				Namespace.Empty();
				Key.Empty();
			}
		}

		// Serialize the Namespace
		Ar << Namespace;

		// Serialize the Key
		Ar << Key;

		// Serialize the SourceString
		Ar << SourceString;
	}
}

///////////////////////////////////////
// FTextHistory_NamedFormat

FTextHistory_NamedFormat::FTextHistory_NamedFormat(FTextFormat&& InSourceFmt, FFormatNamedArguments&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_NamedFormat::FTextHistory_NamedFormat(FTextHistory_NamedFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_NamedFormat& FTextHistory_NamedFormat::operator=(FTextHistory_NamedFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_NamedFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_NamedFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_NamedFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::NamedFormat;
		Ar << HistoryType;
	}

	if (Ar.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Ar << FormatText;
	}
	else if (Ar.IsLoading())
	{
		FText FormatText;
		Ar << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Ar << Arguments;
}

void FTextHistory_NamedFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FFormatArgumentValue& ArgumentValue = It.Value();
		if (ArgumentValue.GetType() == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentValue.GetTextValue(), OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), FFormatNamedArguments(Arguments));
}

///////////////////////////////////////
// FTextHistory_OrderedFormat

FTextHistory_OrderedFormat::FTextHistory_OrderedFormat(FTextFormat&& InSourceFmt, FFormatOrderedArguments&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_OrderedFormat::FTextHistory_OrderedFormat(FTextHistory_OrderedFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_OrderedFormat& FTextHistory_OrderedFormat::operator=(FTextHistory_OrderedFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_OrderedFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_OrderedFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_OrderedFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::OrderedFormat;
		Ar << HistoryType;
	}

	if (Ar.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Ar << FormatText;
	}
	else if (Ar.IsLoading())
	{
		FText FormatText;
		Ar << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Ar << Arguments;
}

void FTextHistory_OrderedFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (auto It = Arguments.CreateConstIterator(); It; ++It)
	{
		const FFormatArgumentValue& ArgumentValue = *It;
		if (ArgumentValue.GetType() == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentValue.GetTextValue(), OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	FFormatNamedArguments NamedArgs;
	NamedArgs.Reserve(Arguments.Num());
	for (int32 ArgIndex = 0; ArgIndex < Arguments.Num(); ++ArgIndex)
	{
		const FFormatArgumentValue& ArgumentValue = Arguments[ArgIndex];
		NamedArgs.Emplace(FString::FromInt(ArgIndex), ArgumentValue);
	}
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), MoveTemp(NamedArgs));
}

///////////////////////////////////////
// FTextHistory_ArgumentDataFormat

FTextHistory_ArgumentDataFormat::FTextHistory_ArgumentDataFormat(FTextFormat&& InSourceFmt, TArray<FFormatArgumentData>&& InArguments)
	: SourceFmt(MoveTemp(InSourceFmt))
	, Arguments(MoveTemp(InArguments))
{
}

FTextHistory_ArgumentDataFormat::FTextHistory_ArgumentDataFormat(FTextHistory_ArgumentDataFormat&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceFmt(MoveTemp(Other.SourceFmt))
	, Arguments(MoveTemp(Other.Arguments))
{
}

FTextHistory_ArgumentDataFormat& FTextHistory_ArgumentDataFormat::operator=(FTextHistory_ArgumentDataFormat&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceFmt = MoveTemp(Other.SourceFmt);
		Arguments = MoveTemp(Other.Arguments);
	}
	return *this;
}

FString FTextHistory_ArgumentDataFormat::BuildLocalizedDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, false);
}

FString FTextHistory_ArgumentDataFormat::BuildInvariantDisplayString() const
{
	return FTextFormatter::FormatStr(SourceFmt, Arguments, true, true);
}

void FTextHistory_ArgumentDataFormat::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::ArgumentFormat;
		Ar << HistoryType;
	}

	if (Ar.IsSaving())
	{
		FText FormatText = SourceFmt.GetSourceText();
		Ar << FormatText;
	}
	else if (Ar.IsLoading())
	{
		FText FormatText;
		Ar << FormatText;
		SourceFmt = FTextFormat(FormatText);
	}

	Ar << Arguments;
}

void FTextHistory_ArgumentDataFormat::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	// Process the formatting text in-case it's a recursive format
	FTextInspector::GetHistoricFormatData(SourceFmt.GetSourceText(), OutHistoricFormatData);

	for (const FFormatArgumentData& ArgumentData : Arguments)
	{
		if (ArgumentData.ArgumentValueType == EFormatArgumentType::Text)
		{
			// Process the text argument in-case it's a recursive format
			FTextInspector::GetHistoricFormatData(ArgumentData.ArgumentValue, OutHistoricFormatData);
		}
	}

	// Add ourself now that we've processed any format dependencies
	FFormatNamedArguments NamedArgs;
	NamedArgs.Reserve(Arguments.Num());
	for (const FFormatArgumentData& ArgumentData : Arguments)
	{
		FFormatArgumentValue ArgumentValue;
		switch (ArgumentData.ArgumentValueType)
		{
		case EFormatArgumentType::Int:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueInt);
			break;
		case EFormatArgumentType::Float:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueFloat);
			break;
		case EFormatArgumentType::Gender:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValueGender);
			break;
		default:
			ArgumentValue = FFormatArgumentValue(ArgumentData.ArgumentValue);
			break;
		}

		NamedArgs.Emplace(ArgumentData.ArgumentName, MoveTemp(ArgumentValue));
	}
	OutHistoricFormatData.Emplace(InText, FTextFormat(SourceFmt), MoveTemp(NamedArgs));
}

///////////////////////////////////////
// FTextHistory_FormatNumber

FTextHistory_FormatNumber::FTextHistory_FormatNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: SourceValue(MoveTemp(InSourceValue))
	, FormatOptions()
	, TargetCulture(MoveTemp(InTargetCulture))
{
	if(InFormatOptions)
	{
		FormatOptions = *InFormatOptions;
	}
}

FTextHistory_FormatNumber::FTextHistory_FormatNumber(FTextHistory_FormatNumber&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceValue(MoveTemp(Other.SourceValue))
	, FormatOptions(MoveTemp(Other.FormatOptions))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_FormatNumber& FTextHistory_FormatNumber::operator=(FTextHistory_FormatNumber&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceValue = MoveTemp(Other.SourceValue);
		FormatOptions = MoveTemp(Other.FormatOptions);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_FormatNumber::Serialize(FArchive& Ar)
{
	Ar << SourceValue;

	bool bHasFormatOptions = FormatOptions.IsSet();
	Ar << bHasFormatOptions;

	if(Ar.IsLoading())
	{
		if(bHasFormatOptions)
		{
			FormatOptions = FNumberFormattingOptions();
		}
		else
		{
			FormatOptions.Reset();
		}
	}
	if(bHasFormatOptions)
	{
		check(FormatOptions.IsSet());
		Ar << FormatOptions.GetValue();
	}

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FString FTextHistory_FormatNumber::BuildNumericDisplayString(const FDecimalNumberFormattingRules& InFormattingRules, const int32 InValueMultiplier) const
{
	check(InValueMultiplier > 0);

	const FNumberFormattingOptions& FormattingOptions = (FormatOptions.IsSet()) ? FormatOptions.GetValue() : InFormattingRules.CultureDefaultFormattingOptions;
	switch (SourceValue.GetType())
	{
	case EFormatArgumentType::Int:
		return FastDecimalFormat::NumberToString(SourceValue.GetIntValue() * static_cast<int64>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::UInt:
		return FastDecimalFormat::NumberToString(SourceValue.GetUIntValue() * static_cast<uint64>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::Float:
		return FastDecimalFormat::NumberToString(SourceValue.GetFloatValue() * static_cast<float>(InValueMultiplier), InFormattingRules, FormattingOptions);
	case EFormatArgumentType::Double:
		return FastDecimalFormat::NumberToString(SourceValue.GetDoubleValue() * static_cast<double>(InValueMultiplier), InFormattingRules, FormattingOptions);
	default:
		break;
	}
	return FString();
}

///////////////////////////////////////
// FTextHistory_AsNumber

FTextHistory_AsNumber::FTextHistory_AsNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
{
}

FTextHistory_AsNumber::FTextHistory_AsNumber(FTextHistory_AsNumber&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
{
}

FTextHistory_AsNumber& FTextHistory_AsNumber::operator=(FTextHistory_AsNumber&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	return *this;
}

FString FTextHistory_AsNumber::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
	return BuildNumericDisplayString(FormattingRules);
}

FString FTextHistory_AsNumber::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetDecimalNumberFormattingRules();
	return BuildNumericDisplayString(FormattingRules);
}

void FTextHistory_AsNumber::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsNumber;
		Ar << HistoryType;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

bool FTextHistory_AsNumber::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	OutHistoricNumericData = FHistoricTextNumericData(FHistoricTextNumericData::EType::AsNumber, SourceValue, FormatOptions);
	return true;
}

///////////////////////////////////////
// FTextHistory_AsPercent

FTextHistory_AsPercent::FTextHistory_AsPercent(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
{
}

FTextHistory_AsPercent::FTextHistory_AsPercent(FTextHistory_AsPercent&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
{
}

FTextHistory_AsPercent& FTextHistory_AsPercent::operator=(FTextHistory_AsPercent&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	return *this;
}

FString FTextHistory_AsPercent::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetPercentFormattingRules();
	return BuildNumericDisplayString(FormattingRules, 100);
}

FString FTextHistory_AsPercent::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetPercentFormattingRules();
	return BuildNumericDisplayString(FormattingRules, 100);
}

void FTextHistory_AsPercent::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsPercent;
		Ar << HistoryType;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

bool FTextHistory_AsPercent::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	OutHistoricNumericData = FHistoricTextNumericData(FHistoricTextNumericData::EType::AsPercent, SourceValue, FormatOptions);
	return true;
}

///////////////////////////////////////
// FTextHistory_AsCurrency

FTextHistory_AsCurrency::FTextHistory_AsCurrency(FFormatArgumentValue InSourceValue, FString InCurrencyCode, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture)
	: FTextHistory_FormatNumber(MoveTemp(InSourceValue), InFormatOptions, MoveTemp(InTargetCulture))
	, CurrencyCode(MoveTemp(InCurrencyCode))
{
}

FTextHistory_AsCurrency::FTextHistory_AsCurrency(FTextHistory_AsCurrency&& Other)
	: FTextHistory_FormatNumber(MoveTemp(Other))
	, CurrencyCode(MoveTemp(Other.CurrencyCode))
{
}

FTextHistory_AsCurrency& FTextHistory_AsCurrency::operator=(FTextHistory_AsCurrency&& Other)
{
	FTextHistory_FormatNumber::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		CurrencyCode = MoveTemp(Other.CurrencyCode);
	}
	return *this;
}

FString FTextHistory_AsCurrency::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	// when we remove AsCurrency should be easy to switch these to AsCurrencyBase and change SourceValue to be BaseVal in AsCurrencyBase (currently is the pre-divided value)
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	return BuildNumericDisplayString(FormattingRules);
}

FString FTextHistory_AsCurrency::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	// when we remove AsCurrency should be easy to switch these to AsCurrencyBase and change SourceValue to be BaseVal in AsCurrencyBase (currently is the pre-divided value)
	const FDecimalNumberFormattingRules& FormattingRules = Culture.GetCurrencyFormattingRules(CurrencyCode);
	return BuildNumericDisplayString(FormattingRules);
}

void FTextHistory_AsCurrency::Serialize( FArchive& Ar )
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsCurrency;
		Ar << HistoryType;
	}

	if (Ar.UE4Ver() >= VER_UE4_ADDED_CURRENCY_CODE_TO_FTEXT)
	{
		Ar << CurrencyCode;
	}

	FTextHistory_FormatNumber::Serialize(Ar);
}

///////////////////////////////////////
// FTextHistory_AsDate

FTextHistory_AsDate::FTextHistory_AsDate(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, DateStyle(InDateStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsDate::FTextHistory_AsDate(FTextHistory_AsDate&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, DateStyle(Other.DateStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsDate& FTextHistory_AsDate::operator=(FTextHistory_AsDate&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		DateStyle = Other.DateStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsDate::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDate;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 DateStyleInt8 = (int8)DateStyle;
	Ar << DateStyleInt8;
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	if( Ar.UE4Ver() >= VER_UE4_FTEXT_HISTORY_DATE_TIMEZONE )
	{
		Ar << TimeZone;
	}

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FString FTextHistory_AsDate::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsDate(SourceDateTime, DateStyle, TimeZone, Culture);
}

FString FTextHistory_AsDate::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsDate(SourceDateTime, DateStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_AsTime

FTextHistory_AsTime::FTextHistory_AsTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, TimeStyle(InTimeStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsTime::FTextHistory_AsTime(FTextHistory_AsTime&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, TimeStyle(Other.TimeStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsTime& FTextHistory_AsTime::operator=(FTextHistory_AsTime&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		TimeStyle = Other.TimeStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsTime::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsTime;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Ar << TimeStyleInt8;
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Ar << TimeZone;

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FString FTextHistory_AsTime::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsTime(SourceDateTime, TimeStyle, TimeZone, Culture);
}

FString FTextHistory_AsTime::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsTime(SourceDateTime, TimeStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_AsDateTime

FTextHistory_AsDateTime::FTextHistory_AsDateTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture)
	: SourceDateTime(MoveTemp(InSourceDateTime))
	, DateStyle(InDateStyle)
	, TimeStyle(InTimeStyle)
	, TimeZone(MoveTemp(InTimeZone))
	, TargetCulture(MoveTemp(InTargetCulture))
{
}

FTextHistory_AsDateTime::FTextHistory_AsDateTime(FTextHistory_AsDateTime&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceDateTime(MoveTemp(Other.SourceDateTime))
	, DateStyle(Other.DateStyle)
	, TimeStyle(Other.TimeStyle)
	, TimeZone(MoveTemp(Other.TimeZone))
	, TargetCulture(MoveTemp(Other.TargetCulture))
{
}

FTextHistory_AsDateTime& FTextHistory_AsDateTime::operator=(FTextHistory_AsDateTime&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceDateTime = MoveTemp(Other.SourceDateTime);
		DateStyle = Other.DateStyle;
		TimeStyle = Other.TimeStyle;
		TimeZone = MoveTemp(Other.TimeZone);
		TargetCulture = MoveTemp(Other.TargetCulture);
	}
	return *this;
}

void FTextHistory_AsDateTime::Serialize(FArchive& Ar)
{
	if(Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::AsDateTime;
		Ar << HistoryType;
	}

	Ar << SourceDateTime;

	int8 DateStyleInt8 = (int8)DateStyle;
	Ar << DateStyleInt8;
	DateStyle = (EDateTimeStyle::Type)DateStyleInt8;

	int8 TimeStyleInt8 = (int8)TimeStyle;
	Ar << TimeStyleInt8;
	TimeStyle = (EDateTimeStyle::Type)TimeStyleInt8;

	Ar << TimeZone;

	if(Ar.IsSaving())
	{
		FString CultureName = TargetCulture.IsValid()? TargetCulture->GetName() : FString();
		Ar << CultureName;
	}
	else if(Ar.IsLoading())
	{
		FString CultureName;
		Ar << CultureName;

		if(!CultureName.IsEmpty())
		{
			TargetCulture = FInternationalization::Get().GetCulture(CultureName);
		}
	}
}

FString FTextHistory_AsDateTime::BuildLocalizedDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = TargetCulture.IsValid() ? *TargetCulture : *I18N.GetCurrentLocale();

	return FTextChronoFormatter::AsDateTime(SourceDateTime, DateStyle, TimeStyle, TimeZone, Culture);
}

FString FTextHistory_AsDateTime::BuildInvariantDisplayString() const
{
	FInternationalization& I18N = FInternationalization::Get();
	checkf(I18N.IsInitialized() == true, TEXT("FInternationalization is not initialized. An FText formatting method was likely used in static object initialization - this is not supported."));
	const FCulture& Culture = *I18N.GetInvariantCulture();

	return FTextChronoFormatter::AsDateTime(SourceDateTime, DateStyle, TimeStyle, TimeZone, Culture);
}

///////////////////////////////////////
// FTextHistory_Transform

FTextHistory_Transform::FTextHistory_Transform(FText InSourceText, const ETransformType InTransformType)
	: SourceText(MoveTemp(InSourceText))
	, TransformType(InTransformType)
{
}

FTextHistory_Transform::FTextHistory_Transform(FTextHistory_Transform&& Other)
	: FTextHistory(MoveTemp(Other))
	, SourceText(MoveTemp(Other.SourceText))
	, TransformType(Other.TransformType)
{
}

FTextHistory_Transform& FTextHistory_Transform::operator=(FTextHistory_Transform&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		SourceText = MoveTemp(Other.SourceText);
		TransformType = Other.TransformType;
	}
	return *this;
}

void FTextHistory_Transform::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::Transform;
		Ar << HistoryType;
	}

	Ar << SourceText;
	Ar << (uint8&)TransformType;
}

FString FTextHistory_Transform::BuildLocalizedDisplayString() const
{
	SourceText.Rebuild();

	switch (TransformType)
	{
	case ETransformType::ToLower:
		return FTextTransformer::ToLower(SourceText.ToString());
	case ETransformType::ToUpper:
		return FTextTransformer::ToUpper(SourceText.ToString());
	default:
		break;
	}
	return FString();
}

FString FTextHistory_Transform::BuildInvariantDisplayString() const
{
	SourceText.Rebuild();

	switch (TransformType)
	{
	case ETransformType::ToLower:
		return FTextTransformer::ToLower(SourceText.BuildSourceString());
	case ETransformType::ToUpper:
		return FTextTransformer::ToUpper(SourceText.BuildSourceString());
	default:
		break;
	}
	return FString();
}

void FTextHistory_Transform::GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const
{
	FTextInspector::GetHistoricFormatData(SourceText, OutHistoricFormatData);
}

bool FTextHistory_Transform::GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const
{
	return FTextInspector::GetHistoricNumericData(SourceText, OutHistoricNumericData);
}

///////////////////////////////////////
// FTextHistory_StringTableEntry

FTextHistory_StringTableEntry::FTextHistory_StringTableEntry(FName InTableId, FString&& InKey)
	: TableId(InTableId)
	, Key(MoveTemp(InKey))
{
	GetStringTableEntry();
}

FTextHistory_StringTableEntry::FTextHistory_StringTableEntry(FTextHistory_StringTableEntry&& Other)
	: FTextHistory(MoveTemp(Other))
	, TableId(Other.TableId)
	, Key(MoveTemp(Other.Key))
	, StringTableEntry(MoveTemp(Other.StringTableEntry))
{
	Other.TableId = FName();
}

FTextHistory_StringTableEntry& FTextHistory_StringTableEntry::operator=(FTextHistory_StringTableEntry&& Other)
{
	FTextHistory::operator=(MoveTemp(Other));
	if (this != &Other)
	{
		TableId = Other.TableId;
		Key = MoveTemp(Other.Key);
		StringTableEntry = MoveTemp(Other.StringTableEntry);
	
		Other.TableId = FName();
	}
	return *this;
}

FString FTextHistory_StringTableEntry::BuildLocalizedDisplayString() const
{
	// This should never be called for string table entries (CanRebuildLocalizedDisplayString is false)
	check(0);
	return FString();
}

FString FTextHistory_StringTableEntry::BuildInvariantDisplayString() const
{
	return *GetSourceString();
}

const FString* FTextHistory_StringTableEntry::GetSourceString() const
{
	FStringTableEntryConstPtr StringTableEntryPin = GetStringTableEntry();
	if (StringTableEntryPin.IsValid())
	{
		return &StringTableEntryPin->GetSourceString();
	}

	static const FString MissingSourceString = TEXT("<MISSING STRING TABLE ENTRY>");
	return &MissingSourceString;
}

FTextDisplayStringRef FTextHistory_StringTableEntry::GetDisplayString() const
{
	FStringTableEntryConstPtr StringTableEntryPin = GetStringTableEntry();
	if (StringTableEntryPin.IsValid())
	{
		FTextDisplayStringPtr DisplayString = StringTableEntryPin->GetDisplayString();
		if (DisplayString.IsValid())
		{
			return DisplayString.ToSharedRef();
		}
	}

	static const FTextDisplayStringRef MissingDisplayString = MakeShared<FString, ESPMode::ThreadSafe>(TEXT("<MISSING STRING TABLE ENTRY>"));
	return MissingDisplayString;
}

void FTextHistory_StringTableEntry::GetTableIdAndKey(FName& OutTableId, FString& OutKey) const
{
	OutTableId = TableId;
	OutKey = Key;
}

void FTextHistory_StringTableEntry::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		int8 HistoryType = (int8)ETextHistoryType::StringTableEntry;
		Ar << HistoryType;
	}

	if (Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;

		Ar << TableId;
		Ar << Key;

		// String Table assets should already have been created via dependency loading when using the EDL (although they may not be fully loaded yet)
		FStringTableRedirects::RedirectTableIdAndKey(TableId, Key, GEventDrivenLoaderEnabled ? EStringTableLoadingPolicy::Find : EStringTableLoadingPolicy::FindOrLoad);

		// Re-cache the pointer
		StringTableEntry.Reset();
		GetStringTableEntry();
	}
	else if (Ar.IsSaving())
	{
		// Update the table ID and key on save to make sure they're up-to-date
		FStringTableEntryConstPtr StringTableEntryPin = GetStringTableEntry();
		if (StringTableEntryPin.IsValid())
		{
			FTextDisplayStringPtr DisplayString = StringTableEntryPin->GetDisplayString();
			FStringTableRegistry::Get().FindTableIdAndKey(DisplayString.ToSharedRef(), TableId, Key);
		}

		Ar << TableId;
		Ar << Key;
	}

	// Collect string table asset references
	FStringTableReferenceCollection::CollectAssetReferences(TableId, Ar);
}

void FTextHistory_StringTableEntry::SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString)
{
	if (Ar.IsLoading())
	{
		// We will definitely need to do a rebuild later
		Revision = 0;
	}
}

FStringTableEntryConstPtr FTextHistory_StringTableEntry::GetStringTableEntry() const
{
	FStringTableEntryConstPtr StringTableEntryPin = StringTableEntry.Pin();

	bool bSuppressMissingEntryWarning = false;

	if (!StringTableEntryPin.IsValid() || !StringTableEntryPin->IsOwned())
	{
		FScopeLock ScopeLock(&StringTableEntryCS);

		// Test again now that we've taken the lock in-case another thread beat us to it
		StringTableEntryPin = StringTableEntry.Pin();
		if (!StringTableEntryPin.IsValid() || !StringTableEntryPin->IsOwned())
		{
			StringTableEntryPin.Reset();

			FStringTableConstPtr StringTable = FStringTableRegistry::Get().FindStringTable(TableId);
			if (StringTable.IsValid())
			{
				bSuppressMissingEntryWarning = !StringTable->IsLoaded();
				StringTableEntryPin = StringTable->FindEntry(Key);
			}

			StringTableEntry = StringTableEntryPin;
		}
	}

	if (!StringTableEntryPin.IsValid() && !bSuppressMissingEntryWarning)
	{
		FStringTableRegistry::Get().LogMissingStringTableEntry(TableId, Key);
	}

	return StringTableEntryPin;
}
