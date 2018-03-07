// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"
#include "Internationalization/Text.h"
#include "Internationalization/StringTableCoreFwd.h"
#include "Misc/DateTime.h"

struct FDecimalNumberFormattingRules;

enum class ETextHistoryType
{
	Base = 0,
	NamedFormat,
	OrderedFormat,
	ArgumentFormat,
	AsNumber,
	AsPercent,
	AsCurrency,
	AsDate,
	AsTime,
	AsDateTime,
	Transform,
	StringTableEntry,

	// Add new enum types at the end only! They are serialized by index.
};

/** Base interface class for all FText history types */
class CORE_API FTextHistory
{
public:
	FTextHistory();

	virtual ~FTextHistory() {}

	/** Allow moving */
	FTextHistory(FTextHistory&& Other);
	FTextHistory& operator=(FTextHistory&& Other);

	/** Get the type of this history */
	virtual ETextHistoryType GetType() const = 0;

	/** Build the display string for the current culture */
	virtual FString BuildLocalizedDisplayString() const = 0;

	/** Build the display string for the invariant culture */
	virtual FString BuildInvariantDisplayString() const = 0;
	
	/** Serializes the history to/from an FArchive */
	virtual void Serialize(FArchive& Ar) = 0;

	/** Serializes data needed to get the FText's DisplayString */
	virtual void SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString);

	/** Returns TRUE if the Revision is out of date */
	virtual bool IsOutOfDate() const;

	/** Returns the source string managed by the history (if any). */
	virtual const FString* GetSourceString() const;

	/** Get any historic text format data from this history */
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const;

	/** Get any historic numeric format data from this history */
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const;

	/** Will rebuild the display string if out of date. */
	void Rebuild(TSharedRef< FString, ESPMode::ThreadSafe > InDisplayString);

	/** Get the raw revision history. Note: Usually you can to call IsOutOfDate rather than test this! */
	uint16 GetRevision() const { return Revision; }

protected:
	/** Returns true if this kind of text history is able to rebuild its localized display string */
	virtual bool CanRebuildLocalizedDisplayString() { return true; }

	/** Revision index of this history, rebuilds when it is out of sync with the FTextLocalizationManager */
	uint16 Revision;

private:
	/** Disallow copying */
	FTextHistory(const FTextHistory&);
	FTextHistory& operator=(FTextHistory&);
};

/** No complexity to it, just holds the source string. */
class CORE_API FTextHistory_Base : public FTextHistory
{
public:
	FTextHistory_Base() {}
	explicit FTextHistory_Base(FString&& InSourceString);

	/** Allow moving */
	FTextHistory_Base(FTextHistory_Base&& Other);
	FTextHistory_Base& operator=(FTextHistory_Base&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::Base; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString) override;
	virtual const FString* GetSourceString() const override;
	//~ End FTextHistory Interface

protected:
	//~ Begin FTextHistory Interface
	virtual bool CanRebuildLocalizedDisplayString() { return false; }
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_Base(const FTextHistory_Base&);
	FTextHistory_Base& operator=(FTextHistory_Base&);

	/** The source string for an FText */
	FString SourceString;
};

/** Handles history for FText::Format when passing named arguments */
class CORE_API FTextHistory_NamedFormat : public FTextHistory
{
public:
	FTextHistory_NamedFormat() {}
	FTextHistory_NamedFormat(FTextFormat&& InSourceFmt, FFormatNamedArguments&& InArguments);

	/** Allow moving */
	FTextHistory_NamedFormat(FTextHistory_NamedFormat&& Other);
	FTextHistory_NamedFormat& operator=(FTextHistory_NamedFormat&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::NamedFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_NamedFormat(const FTextHistory_NamedFormat&);
	FTextHistory_NamedFormat& operator=(FTextHistory_NamedFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	FFormatNamedArguments Arguments;
};

/** Handles history for FText::Format when passing ordered arguments */
class CORE_API FTextHistory_OrderedFormat : public FTextHistory
{
public:
	FTextHistory_OrderedFormat() {}
	FTextHistory_OrderedFormat(FTextFormat&& InSourceFmt, FFormatOrderedArguments&& InArguments);

	/** Allow moving */
	FTextHistory_OrderedFormat(FTextHistory_OrderedFormat&& Other);
	FTextHistory_OrderedFormat& operator=(FTextHistory_OrderedFormat&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::OrderedFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_OrderedFormat(const FTextHistory_OrderedFormat&);
	FTextHistory_OrderedFormat& operator=(FTextHistory_OrderedFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	FFormatOrderedArguments Arguments;
};

/** Handles history for FText::Format when passing raw argument data */
class CORE_API FTextHistory_ArgumentDataFormat : public FTextHistory
{
public:
	FTextHistory_ArgumentDataFormat() {}
	FTextHistory_ArgumentDataFormat(FTextFormat&& InSourceFmt, TArray<FFormatArgumentData>&& InArguments);

	/** Allow moving */
	FTextHistory_ArgumentDataFormat(FTextHistory_ArgumentDataFormat&& Other);
	FTextHistory_ArgumentDataFormat& operator=(FTextHistory_ArgumentDataFormat&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::ArgumentFormat; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_ArgumentDataFormat(const FTextHistory_ArgumentDataFormat&);
	FTextHistory_ArgumentDataFormat& operator=(FTextHistory_ArgumentDataFormat&);

	/** The pattern used to format the text */
	FTextFormat SourceFmt;
	/** Arguments to replace in the pattern string */
	TArray<FFormatArgumentData> Arguments;
};

/** Base class for managing formatting FText's from: AsNumber, AsPercent, and AsCurrency. Manages data serialization of these history events */
class CORE_API FTextHistory_FormatNumber : public FTextHistory
{
public:
	FTextHistory_FormatNumber() {}
	FTextHistory_FormatNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_FormatNumber(FTextHistory_FormatNumber&& Other);
	FTextHistory_FormatNumber& operator=(FTextHistory_FormatNumber&& Other);

	//~ Begin FTextHistory Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End FTextHistory interface

protected:
	/** Build the numeric display string using the given formatting rules */
	FString BuildNumericDisplayString(const FDecimalNumberFormattingRules& InFormattingRules, const int32 InValueMultiplier = 1) const;

	/** The source value to format from */
	FFormatArgumentValue SourceValue;
	/** All the formatting options available to format using. This can be empty. */
	TOptional<FNumberFormattingOptions> FormatOptions;
	/** The culture to format using */
	FCulturePtr TargetCulture;

private:
	/** Disallow copying */
	FTextHistory_FormatNumber(const FTextHistory_FormatNumber&);
	FTextHistory_FormatNumber& operator=(FTextHistory_FormatNumber&);
};

/**  Handles history for formatting using AsNumber */
class CORE_API FTextHistory_AsNumber : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsNumber() {}
	FTextHistory_AsNumber(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsNumber(FTextHistory_AsNumber&& Other);
	FTextHistory_AsNumber& operator=(FTextHistory_AsNumber&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsNumber; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory interface

private:
	/** Disallow copying */
	FTextHistory_AsNumber(const FTextHistory_AsNumber&);
	FTextHistory_AsNumber& operator=(FTextHistory_AsNumber&);
};

/**  Handles history for formatting using AsPercent */
class CORE_API FTextHistory_AsPercent : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsPercent() {}
	FTextHistory_AsPercent(FFormatArgumentValue InSourceValue, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsPercent(FTextHistory_AsPercent&& Other);
	FTextHistory_AsPercent& operator=(FTextHistory_AsPercent&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsPercent; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory interface

private:
	/** Disallow copying */
	FTextHistory_AsPercent(const FTextHistory_AsPercent&);
	FTextHistory_AsPercent& operator=(FTextHistory_AsPercent&);
};

/**  Handles history for formatting using AsCurrency */
class CORE_API FTextHistory_AsCurrency : public FTextHistory_FormatNumber
{
public:
	FTextHistory_AsCurrency() {}
	FTextHistory_AsCurrency(FFormatArgumentValue InSourceValue, FString InCurrencyCode, const FNumberFormattingOptions* const InFormatOptions, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsCurrency(FTextHistory_AsCurrency&& Other);
	FTextHistory_AsCurrency& operator=(FTextHistory_AsCurrency&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsCurrency; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsCurrency(const FTextHistory_AsCurrency&);
	FTextHistory_AsCurrency& operator=(FTextHistory_AsCurrency&);

	/** The currency used to format the number. */
	FString CurrencyCode;
};

/**  Handles history for formatting using AsDate */
class CORE_API FTextHistory_AsDate : public FTextHistory
{
public:
	FTextHistory_AsDate() {}
	FTextHistory_AsDate(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsDate(FTextHistory_AsDate&& Other);
	FTextHistory_AsDate& operator=(FTextHistory_AsDate&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsDate; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsDate(const FTextHistory_AsDate&);
	FTextHistory_AsDate& operator=(FTextHistory_AsDate&);

	/** The source date structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the date in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsTime */
class CORE_API FTextHistory_AsTime : public FTextHistory
{
public:
	FTextHistory_AsTime() {}
	FTextHistory_AsTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsTime(FTextHistory_AsTime&& Other);
	FTextHistory_AsTime& operator=(FTextHistory_AsTime&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsTime; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_AsTime(const FTextHistory_AsTime&);
	FTextHistory_AsTime& operator=(FTextHistory_AsTime&);

	/** The source time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};

/**  Handles history for formatting using AsDateTime */
class CORE_API FTextHistory_AsDateTime : public FTextHistory
{
public:
	FTextHistory_AsDateTime() {}
	FTextHistory_AsDateTime(FDateTime InSourceDateTime, const EDateTimeStyle::Type InDateStyle, const EDateTimeStyle::Type InTimeStyle, FString InTimeZone, FCulturePtr InTargetCulture);

	/** Allow moving */
	FTextHistory_AsDateTime(FTextHistory_AsDateTime&& Other);
	FTextHistory_AsDateTime& operator=(FTextHistory_AsDateTime&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::AsDateTime; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FTextHistory Interfaces

private:
	/** Disallow copying */
	FTextHistory_AsDateTime(const FTextHistory_AsDateTime&);
	FTextHistory_AsDateTime& operator=(FTextHistory_AsDateTime&);

	/** The source date and time structure to format */
	FDateTime SourceDateTime;
	/** Style to format the date using */
	EDateTimeStyle::Type DateStyle;
	/** Style to format the time using */
	EDateTimeStyle::Type TimeStyle;
	/** Timezone to put the time in */
	FString TimeZone;
	/** Culture to format the time in */
	FCulturePtr TargetCulture;
};

/**  Handles history for transforming text (eg, ToLower/ToUpper) */
class CORE_API FTextHistory_Transform : public FTextHistory
{
public:
	enum class ETransformType : uint8
	{
		ToLower = 0,
		ToUpper,

		// Add new enum types at the end only! They are serialized by index.
	};

	FTextHistory_Transform() {}
	FTextHistory_Transform(FText InSourceText, const ETransformType InTransformType);

	/** Allow moving */
	FTextHistory_Transform(FTextHistory_Transform&& Other);
	FTextHistory_Transform& operator=(FTextHistory_Transform&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::Transform; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetHistoricFormatData(const FText& InText, TArray<FHistoricTextFormatData>& OutHistoricFormatData) const override;
	virtual bool GetHistoricNumericData(const FText& InText, FHistoricTextNumericData& OutHistoricNumericData) const override;
	//~ End FTextHistory Interfaces

private:
	/** Disallow copying */
	FTextHistory_Transform(const FTextHistory_Transform&);
	FTextHistory_Transform& operator=(FTextHistory_Transform&);

	/** The source text instance that was transformed */
	FText SourceText;
	/** How the source text was transformed */
	ETransformType TransformType;
};

/** Holds a pointer to a referenced display string from a string table. */
class CORE_API FTextHistory_StringTableEntry : public FTextHistory
{
public:
	FTextHistory_StringTableEntry() {}
	FTextHistory_StringTableEntry(FName InTableId, FString&& InKey);

	/** Allow moving */
	FTextHistory_StringTableEntry(FTextHistory_StringTableEntry&& Other);
	FTextHistory_StringTableEntry& operator=(FTextHistory_StringTableEntry&& Other);

	//~ Begin FTextHistory Interface
	virtual ETextHistoryType GetType() const override { return ETextHistoryType::StringTableEntry; }
	virtual FString BuildLocalizedDisplayString() const override;
	virtual FString BuildInvariantDisplayString() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void SerializeForDisplayString(FArchive& Ar, FTextDisplayStringPtr& InOutDisplayString) override;
	virtual const FString* GetSourceString() const override;
	//~ End FTextHistory Interface

	FTextDisplayStringRef GetDisplayString() const;

	void GetTableIdAndKey(FName& OutTableId, FString& OutKey) const;

protected:
	//~ Begin FTextHistory Interface
	virtual bool CanRebuildLocalizedDisplayString() { return false; }
	//~ End FTextHistory Interface

private:
	/** Disallow copying */
	FTextHistory_StringTableEntry(const FTextHistory_StringTableEntry&);
	FTextHistory_StringTableEntry& operator=(FTextHistory_StringTableEntry&);

	/** Get the string table pointer, potentially re-caching it if it's missing or stale */
	FStringTableEntryConstPtr GetStringTableEntry() const;

	/** The string table ID being referenced */
	FName TableId;

	/** The key within the string table being referenced */
	FString Key;

	/** Cached string table entry pointer */
	mutable FStringTableEntryConstWeakPtr StringTableEntry;

	/** Critical section preventing concurrent access when re-caching StringTableEntry */
	mutable FCriticalSection StringTableEntryCS;
};
