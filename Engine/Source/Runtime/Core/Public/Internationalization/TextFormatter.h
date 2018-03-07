// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/Function.h"
#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "Misc/ExpressionParserTypes.h"
#include "Internationalization/ITextFormatArgumentModifier.h"

struct FPrivateTextFormatArguments;

/**
 * A text formatter is responsible for formatting text patterns using a set of named or ordered arguments.
 */
class CORE_API FTextFormatter
{
public:
	/** Callback function used to compile an argument modifier. Takes an argument modifier string and returns the compiled result. */
	typedef TFunction<TSharedPtr<ITextFormatArgumentModifier>(const FTextFormatString&)> FCompileTextArgumentModifierFuncPtr;

	/** Singleton access */
	static FTextFormatter& Get();

	void RegisterTextArgumentModifier(const FTextFormatString& InKeyword, FCompileTextArgumentModifierFuncPtr InCompileFunc);
	void UnregisterTextArgumentModifier(const FTextFormatString& InKeyword);
	FCompileTextArgumentModifierFuncPtr FindTextArgumentModifier(const FTextFormatString& InKeyword) const;

	/** Get the text format definitions used when formatting text */
	const FTokenDefinitions& GetTextFormatDefinitions() const;

	/** Low-level versions of Format. You probably want to use FText::Format(...) rather than call these directly */
	static FText Format(FTextFormat&& InFmt, FFormatNamedArguments&& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);
	static FText Format(FTextFormat&& InFmt, FFormatOrderedArguments&& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);
	static FText Format(FTextFormat&& InFmt, TArray<FFormatArgumentData>&& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);

	/** Low-level version of Format that returns a string. This should typically only be used externally when rebuilding the display string for some formatted text */
	static FString FormatStr(const FTextFormat& InFmt, const FFormatNamedArguments& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);
	static FString FormatStr(const FTextFormat& InFmt, const FFormatOrderedArguments& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);
	static FString FormatStr(const FTextFormat& InFmt, const TArray<FFormatArgumentData>& InArguments, const bool bInRebuildText, const bool bInRebuildAsSource);

	/** Incredibly low-level version of format. You should only be calling this if you're implementing a custom argument modifier type that itself needs to format using the private arguments */
	static FString Format(const FTextFormat& InFmt, const FPrivateTextFormatArguments& InFormatArgs);

	/** Incredibly low-level version of FFormatArgumentValue::ToFormattedString. You should only be calling this if you're implementing a custom argument modifier type that itself needs to convert the argument to a string */
	static void ArgumentValueToFormattedString(const FFormatArgumentValue& InValue, const FPrivateTextFormatArguments& InFormatArgs, FString& OutResult);

private:
	static int32 EstimateArgumentValueLength(const FFormatArgumentValue& ArgumentValue);

	FTextFormatter();

	/** Token definitions for the text format lexer */
	FTokenDefinitions TextFormatDefinitions;

	/** Functions for constructing argument modifier data */
	TMap<FTextFormatString, FCompileTextArgumentModifierFuncPtr> TextArgumentModifiers;

	/** Critical section protecting the argument modifiers map from being modified concurrently */
	mutable FCriticalSection TextArgumentModifiersCS;
};
