// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/TextRange.h"

/**
 * Tokenize the text based upon the given rule set
 */
class SLATE_API FSyntaxTokenizer
{
public:

	/** Denotes the type of token */
	enum class ETokenType : uint8
	{
		/** Syntax token matching the tokenizing rules provided */
		Syntax,
		/** String token containing some literal text */
		Literal,
	};

	/** A token referencing a range in the original text */
	struct FToken
	{
		FToken(const ETokenType InType, const FTextRange& InRange)
			: Type(InType)
			, Range(InRange)
		{
		}

		ETokenType Type;
		FTextRange Range;
	};

	/** A line containing a series of tokens */
	struct FTokenizedLine
	{
		FTextRange Range;
		TArray<FToken> Tokens;
	};

	/** Rule used to match syntax token types */
	struct FRule
	{
		FRule(FString InMatchText)
			: MatchText(MoveTemp(InMatchText))
		{
		}

		FString MatchText;
	};

	/** 
	 * Create a new tokenizer which will use the given rules to match syntax tokens
	 * @param InRules Rules to control the tokenizer, processed in-order so the most greedy matches must come first
	 */
	static TSharedRef< FSyntaxTokenizer > Create(TArray<FRule> InRules);

	virtual ~FSyntaxTokenizer();

	void Process(TArray<FTokenizedLine>& OutTokenizedLines, const FString& Input);

private:

	FSyntaxTokenizer(TArray<FRule> InRules);

	void TokenizeLineRanges(const FString& Input, const TArray<FTextRange>& LineRanges, TArray<FTokenizedLine>& OutTokenizedLines);

	/** Rules to control the tokenizer, processed in-order so the most greedy matches must come first */
	TArray<FRule> Rules;

};
