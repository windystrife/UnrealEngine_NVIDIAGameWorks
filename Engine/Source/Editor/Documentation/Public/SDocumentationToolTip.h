// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class IDocumentationPage;
class SBox;
class SVerticalBox;

class DOCUMENTATION_API SDocumentationToolTip : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SDocumentationToolTip )
		: _Text()
		, _Style( TEXT("Documentation.SDocumentationTooltip") )
		, _SubduedStyle( TEXT("Documentation.SDocumentationTooltipSubdued") )
		, _HyperlinkTextStyle( TEXT("Documentation.SDocumentationTooltipHyperlinkText") )
		, _HyperlinkButtonStyle( TEXT("Documentation.SDocumentationTooltipHyperlinkButton") )
		, _ColorAndOpacity( FLinearColor::Black )
		, _AddDocumentation( true )
		, _DocumentationMargin(0)
		, _Content()
		{}

		/** The text displayed in this tool tip */
		SLATE_ATTRIBUTE( FText, Text )

		/** The text style to use for this tool tip */
		SLATE_ARGUMENT( FName, Style )
		
		/** The text style to use for subdued footer text in this tool tip */
		SLATE_ARGUMENT( FName, SubduedStyle )
		
		/** The text style to use for hyperlinks in this tool tip */
		SLATE_ARGUMENT( FName, HyperlinkTextStyle )

		/** Hyperlink button style */
		SLATE_ARGUMENT( FName, HyperlinkButtonStyle )

		/** Font color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/**  */
		SLATE_ARGUMENT( bool, AddDocumentation )

		SLATE_ARGUMENT( FMargin, DocumentationMargin )

		/**  */
		SLATE_ARGUMENT( FString, DocumentationLink )

		/**  */
		SLATE_ARGUMENT( FString, ExcerptName )

		/** Arbitrary content to be displayed in the tool tip; overrides any text that may be set. */
		SLATE_DEFAULT_SLOT( FArguments, Content )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	bool IsInteractive() const;

	virtual const FText& GetTextTooltip() const
	{
		return TextContent.Get();
	}

	/**
	 * Adds slots to the provided Vertical Box containing the documentation information.
	 * If you specify not to add it (AddDocumentation = false) you may call this externally to do custom tooltip layout
	 *
	 * @param	VerticalBox	The vertical box to add it to
	 */
	void AddDocumentation(TSharedPtr< SVerticalBox > VerticalBox);

private:

	void ConstructSimpleTipContent();

	void ConstructFullTipContent();

	FReply ReloadDocumentation();

	void CreateExcerpt( FString FileSource, FString ExcerptName );

private:

	/** Text block widget */
	TAttribute< FText > TextContent;
	TSharedPtr< SWidget > OverrideContent;
	FTextBlockStyle StyleInfo;
	FTextBlockStyle SubduedStyleInfo;
	FTextBlockStyle HyperlinkTextStyleInfo;
	FButtonStyle HyperlinkButtonStyleInfo;
	TAttribute< FSlateColor > ColorAndOpacity;

	/** The link to the documentation */
	FString DocumentationLink;
	FString ExcerptName;

	/** Content widget */
	TSharedPtr< SBox > WidgetContent;

	TSharedPtr< SWidget > SimpleTipContent;
	bool IsDisplayingDocumentationLink;

	TSharedPtr< SWidget > FullTipContent;

	TSharedPtr< IDocumentationPage > DocumentationPage;
	bool IsShowingFullTip;

	bool bAddDocumentation;
	FMargin DocumentationMargin;
};
