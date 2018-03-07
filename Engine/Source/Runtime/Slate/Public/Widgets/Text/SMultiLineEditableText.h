// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "SlateGlobals.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/SlateDelegates.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#if WITH_FANCY_TEXT
	#include "Widgets/Text/ISlateEditableTextWidget.h"
	#include "Framework/Text/SlateTextLayoutFactory.h"
#endif

class FActiveTimerHandle;
class FArrangedChildren;
class FPaintArgs;
class FSlateEditableTextLayout;
class FSlateWindowElementList;
class ITextLayoutMarshaller;
enum class ETextShapingMethod : uint8;

#if WITH_FANCY_TEXT

class ITextLayoutMarshaller;
class FSlateEditableTextLayout;

/** An editable text widget that supports multiple lines and soft word-wrapping. */
class SLATE_API SMultiLineEditableText : public SWidget, public ISlateEditableTextWidget
{
public:
	/** Called when the cursor is moved within the text area */
	DECLARE_DELEGATE_OneParam( FOnCursorMoved, const FTextLocation& );

	SLATE_BEGIN_ARGS( SMultiLineEditableText )
		: _Text()
		, _HintText()
		, _SearchText()
		, _Marshaller()
		, _WrapTextAt( 0.0f )
		, _AutoWrapText(false)
		, _WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ) )
		, _Font()
		, _Margin( FMargin() )
		, _LineHeightPercentage( 1.0f )
		, _Justification( ETextJustify::Left )
		, _IsReadOnly(false)
		, _OnTextChanged()
		, _OnTextCommitted()
		, _SelectAllTextWhenFocused(false)
		, _ClearTextSelectionOnFocusLoss(true)
		, _RevertTextOnEscape(false)
		, _ClearKeyboardFocusOnCommit(true)
		, _AllowContextMenu(true)
		, _OnCursorMoved()
		, _ContextMenuExtender()
		, _ModiferKeyForNewLine(EModifierKey::None)
		, _VirtualKeyboardTrigger(EVirtualKeyboardTrigger::OnFocusByPointer)
		, _VirtualKeyboardDismissAction(EVirtualKeyboardDismissAction::TextChangeOnDismiss)
		, _TextShapingMethod()
		, _TextFlowDirection()
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}

		/** The initial text that will appear in the widget. */
		SLATE_ATTRIBUTE(FText, Text)

		/** Hint text that appears when there is no text in the text box */
		SLATE_ATTRIBUTE(FText, HintText)

		/** Text to search for (a new search is triggered whenever this text changes) */
		SLATE_ATTRIBUTE(FText, SearchText)

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr< ITextLayoutMarshaller >, Marshaller)

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE(float, WrapTextAt)

		/** Whether to wrap text automatically based on the widget's computed horizontal space.  IMPORTANT: Using automatic wrapping can result
		    in visual artifacts, as the the wrapped size will computed be at least one frame late!  Consider using WrapTextAt instead.  The initial 
			desired size will not be clamped.  This works best in cases where the text block's size is not affecting other widget's layout. */
		SLATE_ATTRIBUTE(bool, AutoWrapText)

		/** The wrapping policy to use */
		SLATE_ATTRIBUTE(ETextWrappingPolicy, WrappingPolicy)

		/** Pointer to a style of the text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)

		/** Font color and opacity (overrides Style) */
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)

		/** The amount of blank space left around the edges of text area. */
		SLATE_ATTRIBUTE(FMargin, Margin)

		/** The amount to scale each lines height by. */
		SLATE_ATTRIBUTE(float, LineHeightPercentage)

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)

		/** Sets whether this text box can actually be modified interactively by the user */
		SLATE_ATTRIBUTE(bool, IsReadOnly)

		/** The horizontal scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, HScrollBar)

		/** The vertical scroll bar widget */
		SLATE_ARGUMENT(TSharedPtr< SScrollBar >, VScrollBar)

		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)

		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT(FOnTextCommitted, OnTextCommitted)

		/** Whether to select all text when the user clicks to give focus on the widget */
		SLATE_ATTRIBUTE(bool, SelectAllTextWhenFocused)

		/** Whether to clear text selection when focus is lost */
		SLATE_ATTRIBUTE( bool, ClearTextSelectionOnFocusLoss )

		/** Whether to allow the user to back out of changes when they press the escape key */
		SLATE_ATTRIBUTE(bool, RevertTextOnEscape)

		/** Whether to clear keyboard focus when pressing enter to commit changes */
		SLATE_ATTRIBUTE(bool, ClearKeyboardFocusOnCommit)

		/** Whether to prevent the context menu from being displayed  */
		SLATE_ATTRIBUTE(bool, AllowContextMenu)

		/** Delegate to call before a context menu is opened. User returns the menu content or null to the disable context menu */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

		/** Called whenever the horizontal scrollbar is moved by the user */
		SLATE_EVENT(FOnUserScrolled, OnHScrollBarUserScrolled)

		/** Called whenever the vertical scrollbar is moved by the user */
		SLATE_EVENT(FOnUserScrolled, OnVScrollBarUserScrolled)

		/** Called when the cursor is moved within the text area */
		SLATE_EVENT(FOnCursorMoved, OnCursorMoved)

		/** Callback delegate to have first chance handling of the OnKeyDown event */
		SLATE_EVENT(FOnKeyDown, OnKeyDownHandler)

		/** Menu extender for the right-click context menu */
		SLATE_EVENT(FMenuExtensionDelegate, ContextMenuExtender)

		/** Delegate used to create text layouts for this widget. If none is provided then FSlateTextLayout will be used. */
		SLATE_EVENT(FCreateSlateTextLayout, CreateSlateTextLayout)

		/** The optional modifier key necessary to create a newline when typing into the editor. */
		SLATE_ARGUMENT(EModifierKey::Type, ModiferKeyForNewLine)

		/** The type of event that will trigger the display of the virtual keyboard */
		SLATE_ATTRIBUTE(EVirtualKeyboardTrigger, VirtualKeyboardTrigger)

		/** The message action to take when the virtual keyboard is dismissed by the user */
		SLATE_ATTRIBUTE(EVirtualKeyboardDismissAction, VirtualKeyboardDismissAction)

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT( TOptional<ETextShapingMethod>, TextShapingMethod )
		
		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT( TOptional<ETextFlowDirection>, TextFlowDirection )

	SLATE_END_ARGS()

	SMultiLineEditableText();
	~SMultiLineEditableText();

	void Construct( const FArguments& InArgs );

	/**
	 * Sets the text for this text block
	 */
	void SetText(const TAttribute< FText >& InText);

	/**
	 * Returns the text string
	 *
	 * @return  Text string
	 */
	FText GetText() const;

	/**
	 * Returns the plain text string without richtext formatting
	 * @return  Text string
	 */
	FText GetPlainText() const;

	/**
	 * Sets the text that appears when there is no text in the text box
	 */
	void SetHintText(const TAttribute< FText >& InHintText);

	/** Get the text that appears when there is no text in the text box */
	FText GetHintText() const;

	/** Set the text that is currently being searched for (if any) */
	void SetSearchText(const TAttribute<FText>& InSearchText);

	/** Get the text that is currently being searched for (if any) */
	FText GetSearchText() const;

	/** See attribute TextStyle */
	void SetTextStyle(const FTextBlockStyle* InTextStyle);

	/** See attribute Font */
	void SetFont(const TAttribute< FSlateFontInfo >& InNewFont);

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

	/** Sets the ReadOnly attribute */
	void SetIsReadOnly(const TAttribute< bool >& InIsReadOnly);

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

	/** Move the cursor to the given location in the document (will also scroll to this point) */
	void GoTo(const FTextLocation& NewLocation);

	/** Move the cursor specified location */
	void GoTo(ETextLocation NewLocation);

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

	/** Refresh this editable text immediately, rather than wait for the usual caching mechanisms to take affect on the text Tick */
	void Refresh();

	/**
	 * Sets the OnKeyDownHandler to provide first chance handling of the OnKeyDown event
	 *
	 * @param InOnKeyDownHandler			Delegate to call during OnKeyDown event
	 */
	void SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
	{
		OnKeyDownHandler = InOnKeyDownHandler;
	}

protected:
	//~ Begin SWidget Interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void CacheDesiredSize(float LayoutScaleMultiplier) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual bool IsInteractable() const override;
	virtual bool ComputeVolatility() const override;
	//~ End SWidget Interface

protected:
	void OnHScrollBarMoved(const float InScrollOffsetFraction);
	void OnVScrollBarMoved(const float InScrollOffsetFraction);

	/** Return whether a RMB+Drag scroll operation is taking place */
	bool IsRightClickScrolling() const;

public:
	//~ Begin ISlateEditableTextWidget Interface
	virtual bool IsTextReadOnly() const override;
	virtual bool IsTextPassword() const override;
	virtual bool IsMultiLineTextEdit() const override;
	//~ End ISlateEditableTextWidget Interface

protected:
	//~ Begin ISlateEditableTextWidget Interface
	virtual bool ShouldJumpCursorToEndWhenFocused() const override;
	virtual bool ShouldSelectAllTextWhenFocused() const override;
	virtual bool ShouldClearTextSelectionOnFocusLoss() const override;
	virtual bool ShouldRevertTextOnEscape() const override;
	virtual bool ShouldClearKeyboardFocusOnCommit() const override;
	virtual bool ShouldSelectAllTextOnCommit() const override;
	virtual bool CanInsertCarriageReturn() const override;
	virtual bool CanTypeCharacter(const TCHAR InChar) const override;
	virtual void EnsureActiveTick() override;
	virtual EKeyboardType GetVirtualKeyboardType() const override;
	virtual EVirtualKeyboardTrigger GetVirtualKeyboardTrigger() const override;
	virtual EVirtualKeyboardDismissAction GetVirtualKeyboardDismissAction() const override;
	virtual TSharedRef<SWidget> GetSlateWidget() override;
	virtual TSharedPtr<SWidget> GetSlateWidgetPtr() override;
	virtual TSharedPtr<SWidget> BuildContextMenuContent() const override;
	virtual void OnTextChanged(const FText& InText) override;
	virtual void OnTextCommitted(const FText& InText, const ETextCommit::Type InTextAction) override;
	virtual void OnCursorMoved(const FTextLocation& InLocation) override;
	virtual float UpdateAndClampHorizontalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride) override;
	virtual float UpdateAndClampVerticalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride) override;
	//~ End ISlateEditableTextWidget Interface

protected:
	/** The text layout that deals with the editable text */
	TUniquePtr<FSlateEditableTextLayout> EditableTextLayout;

	/** Whether to select all text when the user clicks to give focus on the widget */
	TAttribute<bool> bSelectAllTextWhenFocused;

	/** Whether to clear text selection when focus is lost */
	TAttribute<bool> bClearTextSelectionOnFocusLoss;

	/** True if any changes should be reverted if we receive an escape key */
	TAttribute<bool> bRevertTextOnEscape;

	/** True if we want the text control to lose focus on an text commit/revert events */
	TAttribute<bool> bClearKeyboardFocusOnCommit;

	/** Whether the context menu can be opened */
	TAttribute<bool> bAllowContextMenu;

	/** Sets whether this text box can actually be modified interactively by the user */
	TAttribute<bool> bIsReadOnly;

	/** Delegate to call before a context menu is opened */
	FOnContextMenuOpening OnContextMenuOpening;

	/** Called whenever the text is changed interactively by the user */
	FOnTextChanged OnTextChangedCallback;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	FOnTextCommitted OnTextCommittedCallback;

	/** Called when the cursor is moved within the text area */
	FOnCursorMoved OnCursorMovedCallback;

	/** The horizontal scroll bar widget */
	TSharedPtr<SScrollBar> HScrollBar;

	/** The vertical scroll bar widget */
	TSharedPtr<SScrollBar> VScrollBar;

	/** Called whenever the horizontal scrollbar is moved by the user */
	FOnUserScrolled OnHScrollBarUserScrolled;

	/** Called whenever the vertical scrollbar is moved by the user */
	FOnUserScrolled OnVScrollBarUserScrolled;

	/** Menu extender for right-click context menu */
	TSharedPtr<FExtender> MenuExtender;

	/** The optional modifier key necessary to create a newline when typing into the editor. */
	EModifierKey::Type ModiferKeyForNewLine;

	/** The timer that is actively driving this widget to Tick() even when Slate is idle */
	TWeakPtr<FActiveTimerHandle> ActiveTickTimer;

	/** How much we scrolled while RMB was being held */
	float AmountScrolledWhileRightMouseDown;

	/** Whether a software cursor is currently active */
	bool bIsSoftwareCursor;

	/**	The current position of the software cursor */
	FVector2D SoftwareCursorPosition;

	/** Callback delegate to have first chance handling of the OnKeyDown event */
	FOnKeyDown OnKeyDownHandler;

	/** The type of event that will trigger the display of the virtual keyboard */
	TAttribute<EVirtualKeyboardTrigger> VirtualKeyboardTrigger;

	/** The message action to take when the virtual keyboard is dismissed by the user */
	TAttribute<EVirtualKeyboardDismissAction> VirtualKeyboardDismissAction;
};

#endif //WITH_FANCY_TEXT
