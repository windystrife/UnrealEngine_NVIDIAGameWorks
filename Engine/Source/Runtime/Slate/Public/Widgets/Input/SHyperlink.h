// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"

enum class ETextFlowDirection : uint8;
enum class ETextShapingMethod : uint8;

/**
 * A hyperlink widget is what you would expect from a browser hyperlink.
 * When a hyperlink is clicked in invokes an OnNavigate() delegate.
 */
class SHyperlink
	: public SButton
{
public:

	SLATE_BEGIN_ARGS(SHyperlink)
		: _Text()
		, _Style(&FCoreStyle::Get().GetWidgetStyle< FHyperlinkStyle >("Hyperlink"))
		, _TextStyle(nullptr)
		, _UnderlineStyle(nullptr)
		, _Padding()
		, _OnNavigate()
		, _TextShapingMethod()
		, _TextFlowDirection()
		{}

		SLATE_ATTRIBUTE( FText, Text )
		SLATE_STYLE_ARGUMENT( FHyperlinkStyle, Style )
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )
		SLATE_STYLE_ARGUMENT( FButtonStyle, UnderlineStyle )
		SLATE_ATTRIBUTE( FMargin, Padding )
		SLATE_EVENT( FSimpleDelegate, OnNavigate )
		SLATE_ARGUMENT( TOptional<ETextShapingMethod>, TextShapingMethod )
		SLATE_ARGUMENT( TOptional<ETextFlowDirection>, TextFlowDirection )
	SLATE_END_ARGS()

	/**
	 * Construct the hyperlink widgets from a declaration
	 * 
	 * @param InArgs    Widget declaration from which to construct the hyperlink.
	 */
	void Construct( const FArguments& InArgs )
	{
		this->OnNavigate = InArgs._OnNavigate;

		check (InArgs._Style);
		const FButtonStyle* UnderlineStyle = InArgs._UnderlineStyle != nullptr ? InArgs._UnderlineStyle : &InArgs._Style->UnderlineStyle;
		const FTextBlockStyle* TextStyle = InArgs._TextStyle != nullptr ? InArgs._TextStyle : &InArgs._Style->TextStyle;
		TAttribute<FMargin> Padding = InArgs._Padding.IsSet() ? InArgs._Padding : InArgs._Style->Padding;

		SButton::Construct(
			SButton::FArguments()
			.Text( InArgs._Text )
			.ContentPadding( Padding )
			.ButtonStyle( UnderlineStyle )
			.TextStyle( TextStyle )
			.OnClicked(this, &SHyperlink::Hyperlink_OnClicked)
			.ForegroundColor(FSlateColor::UseForeground())
			.TextShapingMethod( InArgs._TextShapingMethod )
			.TextFlowDirection( InArgs._TextFlowDirection )
		);
	}

public:

	// SWidget overrides

	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override
	{
		return FCursorReply::Cursor( EMouseCursor::Hand );
	}

protected:

	/** Invoke the OnNavigate method */
	FReply Hyperlink_OnClicked()
	{
		OnNavigate.ExecuteIfBound();

		return FReply::Handled();
	}

	/** The delegate to invoke when someone clicks the hyperlink */
	FSimpleDelegate OnNavigate;
};
