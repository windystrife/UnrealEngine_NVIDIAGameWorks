// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Framework/Layout/SlateScrollHelper.h"

class FPaintArgs;
class FSlateWindowElementList;

class SLATE_API SVirtualKeyboardEntry : public SLeafWidget, public IVirtualKeyboardEntry
{

public:

	SLATE_BEGIN_ARGS( SVirtualKeyboardEntry )
		: _Text()
		, _HintText()
		, _Font( FCoreStyle::Get().GetFontStyle("NormalFont") )
		, _ColorAndOpacity( FSlateColor::UseForeground() )
		, _IsReadOnly( false )
		, _ClearKeyboardFocusOnCommit( true )
		, _MinDesiredWidth( 0.0f )
		, _KeyboardType ( EKeyboardType::Keyboard_Default )
		{}

		/** Sets the text content for this editable text widget */
		SLATE_ATTRIBUTE( FText, Text )

		/** The text that appears when there is nothing typed into the search box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Sets the font used to draw the text */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, ClearKeyboardFocusOnCommit )

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Sets the text content for this editable text widget */
		SLATE_ATTRIBUTE( EKeyboardType, KeyboardType )

		SLATE_END_ARGS()


	/** Constructor */
	SVirtualKeyboardEntry();
	
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Restores the text to the original state */
	void RestoreOriginalText();

	/** @return whether the current text varies from the original */
	bool HasTextChangedFromOriginal() const;

	/**
	* Sets the text currently being edited
	*
	* @param  InNewText  The new text
	*/
	void SetText(const TAttribute< FText >& InNewText);

	/**
	 * Sets the font used to draw the text
	 *
	 * @param  InNewFont	The new font to use
	 */
	void SetFont( const TAttribute< FSlateFontInfo >& InNewFont );

	/** @return Whether the user can edit the text. */
	bool GetIsReadOnly() const;

public:
	//~ Begin IVirtualKeyboardEntry Interface
	virtual void SetTextFromVirtualKeyboard(const FText& InNewText, ETextEntryType TextEntryType) override;

	virtual FText GetText() const override
	{
		return Text.Get();
	}

	virtual FText GetHintText() const override
	{
		return HintText.Get();
	}

	virtual EKeyboardType GetVirtualKeyboardType() const override
	{
		return KeyboardType.Get();
	}

	virtual bool IsMultilineEntry() const override
	{
		return false;
	}
	//~ End IVirtualKeyboardEntry Interface

protected:

	//~ Begin SWidget Interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	//~ End SWidget Interface

private:

	/** @return The string that needs to be rendered */
	FString GetStringToRender() const;

private:

	/** The text content for this editable text widget */
	TAttribute< FText > Text;

	TAttribute< FText > HintText;

	/** The font used to draw the text */
	TAttribute< FSlateFontInfo > Font;

	/** Text color and opacity */
	TAttribute<FSlateColor> ColorAndOpacity;

	/** Sets whether this text box can actually be modified interactively by the user */
	TAttribute< bool > IsReadOnly;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	TAttribute< bool > ClearKeyboardFocusOnCommit;

	/** Called whenever the text is changed interactively by the user */
	FOnTextChanged OnTextChanged;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	FOnTextCommitted OnTextCommitted;

	/** Text string currently being edited */
	FText EditedText;

	/** Original text prior to user edits.  This is used to restore the text if the user uses the escape key. */
	FText OriginalText;

	/** Current scrolled position */
	FScrollHelper ScrollHelper;

	/** True if the last mouse down caused us to receive keyboard focus */
	bool bWasFocusedByLastMouseDown;

	/** True if we're currently (potentially) changing the text string */
	bool bIsChangingText;

	/** Prevents the editabletext from being smaller than desired in certain cases (e.g. when it is empty) */
	TAttribute<float> MinDesiredWidth;

	TAttribute<EKeyboardType> KeyboardType;

	bool bNeedsUpdate;

};
