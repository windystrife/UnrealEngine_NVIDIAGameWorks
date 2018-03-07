// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SEditableText.h"

class IErrorReportingWidget;
class SBox;
class SHorizontalBox;
enum class ETextFlowDirection : uint8;
enum class ETextShapingMethod : uint8;

/**
 * Editable text box widget
 */
class SLATE_API SEditableTextBox : public SBorder
{

public:

	SLATE_BEGIN_ARGS( SEditableTextBox )
		: _Style(&FCoreStyle::Get().GetWidgetStyle< FEditableTextBoxStyle >("NormalEditableTextBox"))
		, _Text()
		, _HintText()
		, _SearchText()
		, _Font()
		, _ForegroundColor()
		, _ReadOnlyForegroundColor()
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus ( true )
		, _SelectAllTextWhenFocused( false )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit( true )
		, _AllowContextMenu(true)
		, _MinDesiredWidth( 0.0f )
		, _SelectAllTextOnCommit( false )
		, _BackgroundColor()
		, _Padding()
		, _ErrorReporting()
		, _VirtualKeyboardTrigger(EVirtualKeyboardTrigger::OnFocusByPointer)
		, _VirtualKeyboardDismissAction(EVirtualKeyboardDismissAction::TextChangeOnDismiss)
		{}

		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, Style )

		/** Sets the text content for this editable text box widget */
		SLATE_ATTRIBUTE( FText, Text )

		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Text to search for (a new search is triggered whenever this text changes) */
		SLATE_ATTRIBUTE( FText, SearchText )

		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		
		/** Text color and opacity when read-only (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ReadOnlyForegroundColor )

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Sets whether this text box is for storing a password */
		SLATE_ATTRIBUTE( bool, IsPassword )

		/** Workaround as we loose focus when the auto completion closes. */
		SLATE_ATTRIBUTE( bool, IsCaretMovedWhenGainFocus )

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE( bool, SelectAllTextWhenFocused )

		/** Whether to allow the user to back out of changes when they press the escape key */
		SLATE_ATTRIBUTE( bool, RevertTextOnEscape )

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, ClearKeyboardFocusOnCommit )

		/** Whether the context menu can be opened */
		SLATE_ATTRIBUTE(bool, AllowContextMenu)

		/** Delegate to call before a context menu is opened. User returns the menu content or null to the disable context menu */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Whether to select all text when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, SelectAllTextOnCommit )

		/** Callback delegate to have first chance handling of the OnKeyDown event */
		SLATE_EVENT(FOnKeyDown, OnKeyDownHandler)

		/** The color of the background/border around the editable text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, BackgroundColor )

		/** Padding between the box/border and the text widget inside (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** Provide a alternative mechanism for error reporting. */
		SLATE_ARGUMENT( TSharedPtr<class IErrorReportingWidget>, ErrorReporting )

		/** The type of virtual keyboard to use on mobile devices */
		SLATE_ATTRIBUTE(EKeyboardType, VirtualKeyboardType)

		/** The type of event that will trigger the display of the virtual keyboard */
		SLATE_ATTRIBUTE(EVirtualKeyboardTrigger, VirtualKeyboardTrigger)

		/** The message action to take when the virtual keyboard is dismissed by the user */
		SLATE_ATTRIBUTE(EVirtualKeyboardDismissAction, VirtualKeyboardDismissAction)

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT(TOptional<ETextShapingMethod>, TextShapingMethod)

		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT(TOptional<ETextFlowDirection>, TextFlowDirection)

	SLATE_END_ARGS()
	
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Returns the text string
	 *
	 * @return  Text string
	 */
	FText GetText() const
	{
		return EditableText->GetText();
	}

	/** See attribute Style */
	void SetStyle(const FEditableTextBoxStyle* InStyle);

	/**
	 * Sets the text string currently being edited 
	 *
	 * @param  InNewText  The new text string
	 */
	void SetText( const TAttribute< FText >& InNewText );
	
	/** See the HintText attribute */
	void SetHintText( const TAttribute< FText >& InHintText );
	
	/** Set the text that is currently being searched for (if any) */
	void SetSearchText(const TAttribute<FText>& InSearchText);

	/** Get the text that is currently being searched for (if any) */
	FText GetSearchText() const;

	/** See the IsReadOnly attribute */
	void SetIsReadOnly( TAttribute< bool > InIsReadOnly );
	
	/** See the IsPassword attribute */
	void SetIsPassword( TAttribute< bool > InIsPassword );

	/** See the AllowContextMenu attribute */
	void SetAllowContextMenu(TAttribute< bool > InAllowContextMenu);

	/**
	 * Sets the font used to draw the text
	 *
	 * @param  InFont	The new font to use
	 */
	void SetFont(const TAttribute<FSlateFontInfo>& InFont);

	/**
	 * Sets the text color and opacity (overrides Style)
	 *
	 * @param  InForegroundColor 	The text color and opacity
	 */
	void SetTextBoxForegroundColor(const TAttribute<FSlateColor>& InForegroundColor);

	/**
	 * Sets the color of the background/border around the editable text (overrides Style) 
	 *
	 * @param  InBackgroundColor 	The background/border color
	 */
	void SetTextBoxBackgroundColor(const TAttribute<FSlateColor>& InBackgroundColor);

	/**
	 * Sets the text color and opacity when read-only (overrides Style) 
	 *
	 * @param  InReadOnlyForegroundColor 	The read-only text color and opacity
	 */
	void SetReadOnlyForegroundColor(const TAttribute<FSlateColor>& InReadOnlyForegroundColor);

	/**
	 * Sets the minimum width that a text box should be.
	 *
	 * @param  InMinimumDesiredWidth	The minimum width
	 */
	void SetMinimumDesiredWidth(const TAttribute<float>& InMinimumDesiredWidth);

	/**
	 * Workaround as we loose focus when the auto completion closes.
	 *
	 * @param  InIsCaretMovedWhenGainFocus	Workaround
	 */
	void SetIsCaretMovedWhenGainFocus(const TAttribute<bool>& InIsCaretMovedWhenGainFocus);

	/**
	 * Sets whether to select all text when the user clicks to give focus on the widget
	 *
	 * @param  InSelectAllTextWhenFocused	Select all text when the user clicks?
	 */
	void SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused);

	/**
	 * Sets whether to allow the user to back out of changes when they press the escape key
	 *
	 * @param  InRevertTextOnEscape			Allow the user to back out of changes?
	 */
	void SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape);

	/**
	 * Sets whether to clear keyboard focus when pressing enter to commit changes
	 *
	 * @param  InClearKeyboardFocusOnCommit		Clear keyboard focus when pressing enter?
	 */
	void SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit);

	/**
	 * Sets whether to select all text when pressing enter to commit changes
	 *
	 * @param  InSelectAllTextOnCommit		Select all text when pressing enter?
	 */
	void SetSelectAllTextOnCommit(const TAttribute<bool>& InSelectAllTextOnCommit);

	/**
	 * If InError is a non-empty string the TextBox will the ErrorReporting provided during construction
	 * If no error reporting was provided, the TextBox will create a default error reporter.
	 */
	void SetError( const FText& InError );
	void SetError( const FString& InError );

	/**
	 * Sets the OnKeyDownHandler to provide first chance handling of the SEditableText's OnKeyDown event
	 *
	 * @param InOnKeyDownHandler			Delegate to call during OnKeyDown event
	 */
	void SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler);

	/** See TextShapingMethod attribute */
	void SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod);

	/** See TextFlowDirection attribute */
	void SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection);

	/** Query to see if any text is selected within the document */
	bool AnyTextSelected() const;

	/** Select all the text in the document */
	void SelectAllText();

	/** Clear the active text selection */
	void ClearSelection();

	/** Get the currently selected text */
	FText GetSelectedText() const;

	/** Move the cursor to the given location in the document */
	void GoTo(const FTextLocation& NewLocation);

	/** Move the cursor to the specified location */
	void GoTo(ETextLocation NewLocation)
	{
		EditableText->GoTo(NewLocation);
	}

	/** Scroll to the given location in the document (without moving the cursor) */
	void ScrollTo(const FTextLocation& NewLocation);

	/** Begin a new text search (this is called automatically when the bound search text changes) */
	void BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase, const bool InReverse = false);

	/** Advance the current search to the next match (does nothing if not currently searching) */
	void AdvanceSearch(const bool InReverse = false);

	bool HasError() const;

	// SWidget overrides
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	const FEditableTextBoxStyle* Style;

	/** Box widget that adds padding around the editable text */
	TSharedPtr< SBox > PaddingBox;

	/** Editable text widget */
	TSharedPtr< SEditableText > EditableText;

	/** Padding (overrides style) */
	TAttribute<FMargin> PaddingOverride;

	/** Font (overrides style) */
	TAttribute<FSlateFontInfo> FontOverride;

	/** Foreground color (overrides style) */
	TAttribute<FSlateColor> ForegroundColorOverride;

	/** Background color (overrides style) */
	TAttribute<FSlateColor> BackgroundColorOverride;

	/** Read-only foreground color (overrides style) */
	TAttribute<FSlateColor> ReadOnlyForegroundColorOverride;

	/** Allows for inserting additional widgets that extend the functionality of the text box */
	TSharedPtr<SHorizontalBox> Box;

	/** SomeWidget reporting */
	TSharedPtr<class IErrorReportingWidget> ErrorReporting;

private:

	FMargin FORCEINLINE DeterminePadding() const { check(Style);  return PaddingOverride.IsSet() ? PaddingOverride.Get() : Style->Padding; }
	FSlateFontInfo FORCEINLINE DetermineFont() const { check(Style);  return FontOverride.IsSet() ? FontOverride.Get() : Style->Font;  }
	FSlateColor FORCEINLINE DetermineBackgroundColor() const { check(Style);  return BackgroundColorOverride.IsSet() ? BackgroundColorOverride.Get() : Style->BackgroundColor; }
	
	FSlateColor DetermineForegroundColor() const;

	/** Styling: border image to draw when not hovered or focused */
	const FSlateBrush* BorderImageNormal;
	/** Styling: border image to draw when hovered */
	const FSlateBrush* BorderImageHovered;
	/** Styling: border image to draw when focused */
	const FSlateBrush* BorderImageFocused;
	/** Styling: border image to draw when read only */
	const FSlateBrush* BorderImageReadOnly;

	/** @return Border image for the text box based on the hovered and focused state */
	const FSlateBrush* GetBorderImage() const;

};
