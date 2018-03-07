// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/SMultiLineEditableText.h"

class ITextLayoutMarshaller;

class SCodeEditableText : public SMultiLineEditableText
{
	SLATE_BEGIN_ARGS( SCodeEditableText )
	{}
		/** The initial text that will appear in the widget. */
		SLATE_ATTRIBUTE(FText, Text)

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr< ITextLayoutMarshaller >, Marshaller)

		/** The horizontal scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, HScrollBar)

		/** The vertical scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, VScrollBar)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

private:
	virtual FReply OnKeyChar(const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent) override;
};
