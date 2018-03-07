// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "CodeEditorStyle.h"
#include "Framework/Text/SyntaxTokenizer.h"
#include "Framework/Text/SyntaxHighlighterTextLayoutMarshaller.h"

class FTextLayout;

/**
 * Get/set the raw text to/from a text layout, and also inject syntax highlighting for our rich-text markup
 */
class FCPPRichTextSyntaxHighlighterTextLayoutMarshaller : public FSyntaxHighlighterTextLayoutMarshaller
{
public:

	struct FSyntaxTextStyle
	{
		FSyntaxTextStyle()
			: NormalTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.Normal"))
			, OperatorTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.Operator"))
			, KeywordTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.Keyword"))
			, StringTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.String"))
			, NumberTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.Number"))
			, CommentTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.Comment"))
			, PreProcessorKeywordTextStyle(FCodeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("SyntaxHighlight.CPP.PreProcessorKeyword"))
		{
		}

		FSyntaxTextStyle(const FTextBlockStyle& InNormalTextStyle, const FTextBlockStyle& InOperatorTextStyle, const FTextBlockStyle& InKeywordTextStyle, const FTextBlockStyle& InStringTextStyle, const FTextBlockStyle& InNumberTextStyle, const FTextBlockStyle& InCommentTextStyle, const FTextBlockStyle& InPreProcessorKeywordTextStyle)
			: NormalTextStyle(InNormalTextStyle)
			, OperatorTextStyle(InOperatorTextStyle)
			, KeywordTextStyle(InKeywordTextStyle)
			, StringTextStyle(InStringTextStyle)
			, NumberTextStyle(InNumberTextStyle)
			, CommentTextStyle(InCommentTextStyle)
			, PreProcessorKeywordTextStyle(InPreProcessorKeywordTextStyle)
		{
		}

		FTextBlockStyle NormalTextStyle;
		FTextBlockStyle OperatorTextStyle;
		FTextBlockStyle KeywordTextStyle;
		FTextBlockStyle StringTextStyle;
		FTextBlockStyle NumberTextStyle;
		FTextBlockStyle CommentTextStyle;
		FTextBlockStyle PreProcessorKeywordTextStyle;
	};

	static TSharedRef< FCPPRichTextSyntaxHighlighterTextLayoutMarshaller > Create(const FSyntaxTextStyle& InSyntaxTextStyle);

	virtual ~FCPPRichTextSyntaxHighlighterTextLayoutMarshaller();

protected:

	virtual void ParseTokens(const FString& SourceString, FTextLayout& TargetTextLayout, TArray<FSyntaxTokenizer::FTokenizedLine> TokenizedLines) override;

	FCPPRichTextSyntaxHighlighterTextLayoutMarshaller(TSharedPtr< FSyntaxTokenizer > InTokenizer, const FSyntaxTextStyle& InSyntaxTextStyle);

	/** Styles used to display the text */
	FSyntaxTextStyle SyntaxTextStyle;

	/** String representing tabs */
	FString TabString;
};
