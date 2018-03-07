// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Framework/Text/TextLineHighlight.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/ITextDecorator.h"
#if WITH_FANCY_TEXT
	#include "Framework/Text/BaseTextLayoutMarshaller.h"
#endif

class FSlateTextUnderlineLineHighlighter;
class IRichTextMarkupParser;
class IRichTextMarkupWriter;
class ISlateStyle;
struct FTextBlockStyle;

#if WITH_FANCY_TEXT

/**
 * Get/set the raw text to/from a text layout as rich text
 */
class SLATE_API FRichTextLayoutMarshaller : public FBaseTextLayoutMarshaller
{
public:

	static TSharedRef< FRichTextLayoutMarshaller > Create(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);
	static TSharedRef< FRichTextLayoutMarshaller > Create(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);

	virtual ~FRichTextLayoutMarshaller();

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;

	/**
	 * Append an inline decorator to this marshaller
	 */
	inline FRichTextLayoutMarshaller& AppendInlineDecorator(const TSharedRef< ITextDecorator >& DecoratorToAdd)
	{
		InlineDecorators.Add(DecoratorToAdd);
		return *this;
	}

protected:

	FRichTextLayoutMarshaller(TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);
	FRichTextLayoutMarshaller(TSharedPtr< IRichTextMarkupParser > InParser, TSharedPtr< IRichTextMarkupWriter > InWriter, TArray< TSharedRef< ITextDecorator > > InDecorators, const ISlateStyle* const InDecoratorStyleSet);

	TSharedPtr< ITextDecorator > TryGetDecorator(const FString& Line, const FTextRunParseResults& TextRun) const;

	virtual void AppendRunsForText(
		const int32 LineIndex,
		const FTextRunParseResults& TextRun, 
		const FString& ProcessedString, 
		const FTextBlockStyle& DefaultTextStyle,
		const TSharedRef<FString>& InOutModelText, 
		FTextLayout& TargetTextLayout,
		TArray<TSharedRef<IRun>>& Runs,
		TArray<FTextLineHighlight>& LineHighlights,
		TMap<const FTextBlockStyle*, TSharedPtr<FSlateTextUnderlineLineHighlighter>>& CachedUnderlineHighlighters
		);

	/** The parser used to resolve any markup used in the provided string. */
	TSharedPtr< IRichTextMarkupParser > Parser;

	/** The writer used to recreate any markup needed to preserve the styled runs. */
	TSharedPtr< IRichTextMarkupWriter > Writer;

	/** Any decorators that should be used while parsing the text. */
	TArray< TSharedRef< ITextDecorator > > Decorators;

	/** Additional decorators can be appended inline. Inline decorators get precedence over decorators not specified inline. */
	TArray< TSharedRef< ITextDecorator > > InlineDecorators;

	/** The style set used for looking up styles used by decorators */
	const ISlateStyle* DecoratorStyleSet;

};

#endif //WITH_FANCY_TEXT
