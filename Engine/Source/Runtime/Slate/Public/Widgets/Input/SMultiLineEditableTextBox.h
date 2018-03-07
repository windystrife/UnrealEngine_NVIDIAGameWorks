// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "SlateGlobals.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Text/SMultiLineEditableText.h"

class IErrorReportingWidget;
class ITextLayoutMarshaller;
class SBox;
class SHorizontalBox;
enum class ETextShapingMethod : uint8;

#if WITH_FANCY_TEXT

/**
 * Editable text box widget
 */
class SLATE_API SMultiLineEditableTextBox : public SBorder
{

public:

	SLATE_BEGIN_ARGS( SMultiLineEditableTextBox )
		: _Style(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		, _TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		, _Marshaller()
		, _Text()
		, _HintText()
		, _SearchText()
		, _Font()
		, _ForegroundColor()
		, _ReadOnlyForegroundColor()
		, _Justification(ETextJustify::Left)
		, _LineHeightPercentage(1.0f)
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus ( true )
		, _SelectAllTextWhenFocused( false )
		, _ClearTextSelectionOnFocusLoss( true )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit( true )
		, _AllowContextMenu(true)
		, _AlwaysShowScrollbars( false )
		, _HScrollBar()
		, _VScrollBar()
		, _WrapTextAt(0.0f)
		, _AutoWrapText(false)
		, _WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
		, _SelectAllTextOnCommit( false )
		, _BackgroundColor()		
		, _Padding()
		, _Margin()
		, _ErrorReporting()
		, _ModiferKeyForNewLine(EModifierKey::None)
		, _VirtualKeyboardTrigger(EVirtualKeyboardTrigger::OnFocusByPointer)
		, _VirtualKeyboardDismissAction(EVirtualKeyboardDismissAction::TextChangeOnDismiss)
		, _TextShapingMethod()
		, _TextFlowDirection()
		{}

		/** The styling of the textbox */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, Style )

		/** Pointer to a style of the text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr< ITextLayoutMarshaller >, Marshaller)

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

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)

		/** The amount to scale each lines height by. */
		SLATE_ATTRIBUTE(float, LineHeightPercentage)

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Sets whether this text box is for storing a password */
		SLATE_ATTRIBUTE( bool, IsPassword )

		/** Workaround as we loose focus when the auto completion closes. */
		SLATE_ATTRIBUTE( bool, IsCaretMovedWhenGainFocus )

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE( bool, SelectAllTextWhenFocused )

		/** Whether to clear text selection when focus is lost */
		SLATE_ATTRIBUTE( bool, ClearTextSelectionOnFocusLoss )

		/** Whether to allow the user to back out of changes when they press the escape key */
		SLATE_ATTRIBUTE( bool, RevertTextOnEscape )

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, ClearKeyboardFocusOnCommit )

		/** Whether the context menu can be opened  */
		SLATE_ATTRIBUTE(bool, AllowContextMenu)

		/** Should we always show the scrollbars (only affects internally created scroll bars) */
		SLATE_ARGUMENT(bool, AlwaysShowScrollbars)

		/** The horizontal scroll bar widget, or null to create one internally */
		SLATE_ARGUMENT( TSharedPtr< SScrollBar >, HScrollBar )

		/** The vertical scroll bar widget, or null to create one internally */
		SLATE_ARGUMENT( TSharedPtr< SScrollBar >, VScrollBar )

		/** Padding around the horizontal scrollbar (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, HScrollBarPadding )

		/** Padding around the vertical scrollbar (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, VScrollBarPadding )

		/** Delegate to call before a context menu is opened. User returns the menu content or null to the disable context menu */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnTextChanged, OnTextChanged )

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )

		/** Called whenever the horizontal scrollbar is moved by the user */
		SLATE_EVENT( FOnUserScrolled, OnHScrollBarUserScrolled )

		/** Called whenever the vertical scrollbar is moved by the user */
		SLATE_EVENT( FOnUserScrolled, OnVScrollBarUserScrolled )

		/** Called when the cursor is moved within the text area */
		SLATE_EVENT( SMultiLineEditableText::FOnCursorMoved, OnCursorMoved )

		/** Callback delegate to have first chance handling of the OnKeyDown event */
		SLATE_EVENT(FOnKeyDown, OnKeyDownHandler)

		/** Menu extender for the right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )

		/** Delegate used to create text layouts for this widget. If none is provided then FSlateTextLayout will be used. */
		SLATE_EVENT( FCreateSlateTextLayout, CreateSlateTextLayout )

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE( float, WrapTextAt )

		/** Whether to wrap text automatically based on the widget's computed horizontal space.  IMPORTANT: Using automatic wrapping can result
			in visual artifacts, as the the wrapped size will computed be at least one frame late!  Consider using WrapTextAt instead.  The initial 
			desired size will not be clamped.  This works best in cases where the text block's size is not affecting other widget's layout. */
		SLATE_ATTRIBUTE( bool, AutoWrapText )

		/** The wrapping policy to use */
		SLATE_ATTRIBUTE( ETextWrappingPolicy, WrappingPolicy )

		/** Whether to select all text when pressing enter to commit changes */
		SLATE_ATTRIBUTE( bool, SelectAllTextOnCommit )

		/** The color of the background/border around the editable text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, BackgroundColor )

		/** Padding between the box/border and the text widget inside (overrides Style) */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** The amount of blank space left around the edges of text area. 
			This is different to Padding because this area is still considered part of the text area, and as such, can still be interacted with */
		SLATE_ATTRIBUTE( FMargin, Margin )

		/** Provide a alternative mechanism for error reporting. */
		SLATE_ARGUMENT( TSharedPtr<class IErrorReportingWidget>, ErrorReporting )

		/** The optional modifier key necessary to create a newline when typing into the editor. */
		SLATE_ARGUMENT( EModifierKey::Type, ModiferKeyForNewLine)

		/** The type of event that will trigger the display of the virtual keyboard */
		SLATE_ATTRIBUTE( EVirtualKeyboardTrigger, VirtualKeyboardTrigger )

		/** The message action to take when the virtual keyboard is dismissed by the user */
		SLATE_ATTRIBUTE( EVirtualKeyboardDismissAction, VirtualKeyboardDismissAction )

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT( TOptional<ETextShapingMethod>, TextShapingMethod )
		
		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT( TOptional<ETextFlowDirection>, TextFlowDirection )

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

	/**
	 * Returns the plain text string without richtext formatting
	 *
	 * @return  Text string
	 */
	FText GetPlainText() const
	{
		return EditableText->GetPlainText();
	}

	/** See attribute Style */
	void SetStyle(const FEditableTextBoxStyle* InStyle);

	/**
	 * Sets the text string currently being edited 
	 *
	 * @param  InNewText  The new text string
	 */
	void SetText( const TAttribute< FText >& InNewText );

	/**
	 * Sets the text that appears when there is no text in the text box
	 *
	 * @param  InHintText The hint text string
	 */
	void SetHintText( const TAttribute< FText >& InHintText );

	/** Set the text that is currently being searched for (if any) */
	void SetSearchText(const TAttribute<FText>& InSearchText);

	/** Get the text that is currently being searched for (if any) */
	FText GetSearchText() const;

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

	/** See TextShapingMethod attribute */
	void SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod);

	/** See TextFlowDirection attribute */
	void SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection);

	/** See WrapTextAt attribute */
	void SetWrapTextAt(const TAttribute<float>& InWrapTextAt);

	/** See AutoWrapText attribute */
	void SetAutoWrapText(const TAttribute<bool>& InAutoWrapText);

	/** Set WrappingPolicy attribute */
	void SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy);

	/** See LineHeightPercentage attribute */
	void SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage);

	/** See Margin attribute */
	void SetMargin(const TAttribute<FMargin>& InMargin);

	/** See Justification attribute */
	void SetJustification(const TAttribute<ETextJustify::Type>& InJustification);

	/** See the AllowContextMenu attribute */
	void SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu);

	/** Set the ReadOnly attribute */
	void SetIsReadOnly(const TAttribute< bool >& InIsReadOnly);

	/**
	 * If InError is a non-empty string the TextBox will the ErrorReporting provided during construction
	 * If no error reporting was provided, the TextBox will create a default error reporter.
	 */
	void SetError( const FText& InError );
	void SetError( const FString& InError );

	// SWidget overrides
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

	/** Query to see if any text is selected within the document */
	bool AnyTextSelected() const;

	/** Select all the text in the document */
	void SelectAllText();

	/** Clear the active text selection */
	void ClearSelection();

	/** Get the currently selected text */
	FText GetSelectedText() const;

	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursor(const FText& InText);
	void InsertTextAtCursor(const FString& InString);

	/** Insert the given run at the current cursor position */
	void InsertRunAtCursor(TSharedRef<IRun> InRun);

	/** Move the cursor to the given location in the document */
	void GoTo(const FTextLocation& NewLocation);

	/** Move the cursor to the specified location */
	void GoTo(ETextLocation NewLocation)
	{
		EditableText->GoTo(NewLocation);
	}

	/** Scroll to the given location in the document (without moving the cursor) */
	void ScrollTo(const FTextLocation& NewLocation);

	/** Apply the given style to the currently selected text (or insert a new run at the current cursor position if no text is selected) */
	void ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle);

	/** Begin a new text search (this is called automatically when the bound search text changes) */
	void BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase, const bool InReverse = false);

	/** Advance the current search to the next match (does nothing if not currently searching) */
	void AdvanceSearch(const bool InReverse = false);

	/** Get the run currently under the cursor, or null if there is no run currently under the cursor */
	TSharedPtr<const IRun> GetRunUnderCursor() const;

	/** Get the runs currently that are current selected, some of which may be only partially selected */
	TArray<TSharedRef<const IRun>> GetSelectedRuns() const;

	/** Get the horizontal scroll bar widget */
	TSharedPtr<const SScrollBar> GetHScrollBar() const;

	/** Get the vertical scroll bar widget */
	TSharedPtr<const SScrollBar> GetVScrollBar() const;

	/** Refresh this text box immediately, rather than wait for the usual caching mechanisms to take affect on the text Tick */
	void Refresh();

	/**
	 * Sets the OnKeyDownHandler to provide first chance handling of the SMultiLineEditableText's OnKeyDown event
	 *
	 * @param InOnKeyDownHandler			Delegate to call during OnKeyDown event
	 */
	void SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler);

protected:

	/** Editable text widget */
	TSharedPtr< SMultiLineEditableText > EditableText;

	/** Padding (overrides style) */
	TAttribute<FMargin> PaddingOverride;

	/** Horiz scrollbar padding (overrides style) */
	TAttribute<FMargin> HScrollBarPaddingOverride;

	/** Vert scrollbar padding (overrides style) */
	TAttribute<FMargin> VScrollBarPaddingOverride;

	/** Font (overrides style) */
	TAttribute<FSlateFontInfo> FontOverride;

	/** Foreground color (overrides style) */
	TAttribute<FSlateColor> ForegroundColorOverride;

	/** Background color (overrides style) */
	TAttribute<FSlateColor> BackgroundColorOverride;

	/** Read-only foreground color (overrides style) */
	TAttribute<FSlateColor> ReadOnlyForegroundColorOverride;

	/** Whether to disable the context menu */
	TAttribute< bool > AllowContextMenu;

	/** Allows for inserting additional widgets that extend the functionality of the text box */
	TSharedPtr<SHorizontalBox> Box;


	/** Whether we have an externally supplied horizontal scrollbar or one created internally */
	bool bHasExternalHScrollBar;

	/** Horiz scrollbar */
	TSharedPtr<SScrollBar> HScrollBar;

	/** Box around the horiz scrollbar used for adding padding */
	TSharedPtr<SBox> HScrollBarPaddingBox;

	/** Whether we have an externally supplied vertical scrollbar or one created internally */
	bool bHasExternalVScrollBar;

	/** Vert scrollbar */
	TSharedPtr<SScrollBar> VScrollBar;

	/** Box around the vert scrollbar used for adding padding */
	TSharedPtr<SBox> VScrollBarPaddingBox;

	/** SomeWidget reporting */
	TSharedPtr<class IErrorReportingWidget> ErrorReporting;

private:

	const FEditableTextBoxStyle* Style;

	FMargin FORCEINLINE DeterminePadding() const { check(Style);  return PaddingOverride.IsSet() ? PaddingOverride.Get() : Style->Padding; }
	FMargin FORCEINLINE DetermineHScrollBarPadding() const { check(Style);  return HScrollBarPaddingOverride.IsSet() ? HScrollBarPaddingOverride.Get() : Style->HScrollBarPadding; }
	FMargin FORCEINLINE DetermineVScrollBarPadding() const { check(Style);  return VScrollBarPaddingOverride.IsSet() ? VScrollBarPaddingOverride.Get() : Style->VScrollBarPadding; }
	FSlateFontInfo FORCEINLINE DetermineFont() const { check(Style);  return FontOverride.IsSet() ? FontOverride.Get() : Style->Font; }
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


#endif //WITH_FANCY_TEXT
