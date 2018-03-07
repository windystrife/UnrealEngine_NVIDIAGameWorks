// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

#if WITH_FANCY_TEXT
	#include "Framework/Text/PlainTextLayoutMarshaller.h"
	#include "Framework/Text/SyntaxTokenizer.h"
#endif

class FTextLayout;

#if WITH_FANCY_TEXT

/**
 * Get/set the raw text to/from a text layout, and also inject syntax highlighting
 */
class SLATE_API FSyntaxHighlighterTextLayoutMarshaller : public FPlainTextLayoutMarshaller
{
public:

	virtual ~FSyntaxHighlighterTextLayoutMarshaller();

	// ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual bool RequiresLiveUpdate() const override;

	void EnableSyntaxHighlighting(const bool bEnable);
	bool IsSyntaxHighlightingEnabled() const;

protected:

	virtual void ParseTokens(const FString& SourceString, FTextLayout& TargetTextLayout, TArray<FSyntaxTokenizer::FTokenizedLine> TokenizedLines) = 0;

	FSyntaxHighlighterTextLayoutMarshaller(TSharedPtr< FSyntaxTokenizer > InTokenizer);

	/** Tokenizer used to style the text */
	TSharedPtr< FSyntaxTokenizer > Tokenizer;

	/** True if syntax highlighting is enabled, false to fallback to plain text */
	bool bSyntaxHighlightingEnabled;

};

/**
 * Get/set the raw text to/from a text layout, and also inject syntax highlighting for our rich-text markup
 */
class SLATE_API FRichTextSyntaxHighlighterTextLayoutMarshaller : public FSyntaxHighlighterTextLayoutMarshaller
{
public:

	struct FSyntaxTextStyle
	{
		FSyntaxTextStyle()
			: NormalTextStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.Normal"))
			, NodeTextStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.Node"))
			, NodeAttributeKeyTextStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.NodeAttributeKey"))
			, NodeAttribueAssignmentTextStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.NodeAttribueAssignment"))
			, NodeAttributeValueTextStyle(FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.NodeAttributeValue"))
		{
		}

		FSyntaxTextStyle(const FTextBlockStyle& InNormalTextStyle, const FTextBlockStyle& InNodeTextStyle, const FTextBlockStyle& InNodeAttributeKeyTextStyle, const FTextBlockStyle& InNodeAttribueAssignmentTextStyle, const FTextBlockStyle& InNodeAttributeValueTextStyle)
			: NormalTextStyle(InNormalTextStyle)
			, NodeTextStyle(InNodeTextStyle)
			, NodeAttributeKeyTextStyle(InNodeAttributeKeyTextStyle)
			, NodeAttribueAssignmentTextStyle(InNodeAttribueAssignmentTextStyle)
			, NodeAttributeValueTextStyle(InNodeAttributeValueTextStyle)
		{
		}

		FTextBlockStyle NormalTextStyle;
		FTextBlockStyle NodeTextStyle;
		FTextBlockStyle NodeAttributeKeyTextStyle;
		FTextBlockStyle NodeAttribueAssignmentTextStyle;
		FTextBlockStyle NodeAttributeValueTextStyle;
	};

	static TSharedRef< FRichTextSyntaxHighlighterTextLayoutMarshaller > Create(const FSyntaxTextStyle& InSyntaxTextStyle);

	virtual ~FRichTextSyntaxHighlighterTextLayoutMarshaller();

protected:

	virtual void ParseTokens(const FString& SourceString, FTextLayout& TargetTextLayout, TArray<FSyntaxTokenizer::FTokenizedLine> TokenizedLines) override;

	FRichTextSyntaxHighlighterTextLayoutMarshaller(TSharedPtr< FSyntaxTokenizer > InTokenizer, const FSyntaxTextStyle& InSyntaxTextStyle);

	/** Styles used to display the text */
	FSyntaxTextStyle SyntaxTextStyle;

};

#endif //WITH_FANCY_TEXT
