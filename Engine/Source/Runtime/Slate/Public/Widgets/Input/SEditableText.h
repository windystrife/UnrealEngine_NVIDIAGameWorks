// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Framework/SlateDelegates.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/Text/ISlateEditableTextWidget.h"

class FActiveTimerHandle;
class FArrangedChildren;
class FChildren;
class FPaintArgs;
class FPlainTextLayoutMarshaller;
class FSlateEditableTextLayout;
class FSlateWindowElementList;
class IBreakIterator;
struct FTextLocation;
enum class ETextFlowDirection : uint8;
enum class ETextShapingMethod : uint8;

/**
 * Editable text widget
 */
class SLATE_API SEditableText : public SWidget, public ISlateEditableTextWidget
{
public:
	SLATE_BEGIN_ARGS( SEditableText )
		: _Text()
		, _HintText()
		, _SearchText()
		, _Style(&FCoreStyle::Get().GetWidgetStyle< FEditableTextStyle >("NormalEditableText"))
		, _Font()
		, _ColorAndOpacity()
		, _BackgroundImageSelected()
		, _BackgroundImageComposing()
		, _CaretImage()
		, _IsReadOnly( false )
		, _IsPassword( false )
		, _IsCaretMovedWhenGainFocus( true )
		, _SelectAllTextWhenFocused( false )
		, _RevertTextOnEscape( false )
		, _ClearKeyboardFocusOnCommit(true)
		, _AllowContextMenu(true)
		, _MinDesiredWidth(0.0f)
		, _SelectAllTextOnCommit( false )
		, _VirtualKeyboardType(EKeyboardType::Keyboard_Default)
		, _VirtualKeyboardTrigger(EVirtualKeyboardTrigger::OnFocusByPointer)
		, _VirtualKeyboardDismissAction(EVirtualKeyboardDismissAction::TextChangeOnDismiss)
		, _TextShapingMethod()
		, _TextFlowDirection()
		{
			_Clipping = EWidgetClipping::ClipToBounds;
		}

		/** Sets the text content for this editable text widget */
		SLATE_ATTRIBUTE( FText, Text )

		/** The text that appears when there is nothing typed into the search box */
		SLATE_ATTRIBUTE( FText, HintText )

		/** Text to search for (a new search is triggered whenever this text changes) */
		SLATE_ATTRIBUTE( FText, SearchText )

		/** The style of the text block, which dictates the font, color */
		SLATE_STYLE_ARGUMENT( FEditableTextStyle, Style )

		/** Sets the font used to draw the text (overrides Style) */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity (overrides Style) */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Background image for the selected text (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, BackgroundImageSelected )

		/** Background image for the composing text (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, BackgroundImageComposing )

		/** Image brush used for the caret (overrides Style) */
		SLATE_ATTRIBUTE( const FSlateBrush*, CaretImage )

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

		/** How should the value be justified in the editable text field. */
		SLATE_ATTRIBUTE(ETextJustify::Type, Justification)

		/** Whether the context menu can be opened  */
		SLATE_ATTRIBUTE(bool, AllowContextMenu)

		/** Delegate to call before a context menu is opened. User returns the menu content or null to the disable context menu */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

		/**
		 * This is NOT for validating input!
		 * 
		 * Called whenever a character is typed.
		 * Not called for copy, paste, or any other text changes!
		 */
		SLATE_EVENT( FOnIsTypedCharValid, OnIsTypedCharValid )

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

		/** Menu extender for the right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )

		/** The type of virtual keyboard to use on mobile devices */
		SLATE_ATTRIBUTE( EKeyboardType, VirtualKeyboardType)

		/** The type of event that will trigger the display of the virtual keyboard */
		SLATE_ATTRIBUTE( EVirtualKeyboardTrigger, VirtualKeyboardTrigger )

		/** The message action to take when the virtual keyboard is dismissed by the user */
		SLATE_ATTRIBUTE( EVirtualKeyboardDismissAction, VirtualKeyboardDismissAction )

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT(TOptional<ETextShapingMethod>, TextShapingMethod)

		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT(TOptional<ETextFlowDirection>, TextFlowDirection)

	SLATE_END_ARGS()

	/** Constructor */
	SEditableText();

	/** Destructor */
	~SEditableText();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Sets the text currently being edited 
	 *
	 * @param  InNewText  The new text
	 */
	void SetText( const TAttribute< FText >& InNewText );
	
	/**
	 * Returns the text string
	 *
	 * @return  Text string
	 */
	FText GetText() const;

	/** See the HintText attribute */
	void SetHintText( const TAttribute< FText >& InHintText );
	
	/** Get the text that appears when there is no text in the text box */
	FText GetHintText() const;

	/** Set the text that is currently being searched for (if any) */
	void SetSearchText(const TAttribute<FText>& InSearchText);

	/** Get the text that is currently being searched for (if any) */
	FText GetSearchText() const;

	/** See the IsReadOnly attribute */
	void SetIsReadOnly( TAttribute< bool > InIsReadOnly );
	
	/** See the IsPassword attribute */
	void SetIsPassword( TAttribute< bool > InIsPassword );
	
	/** See the ColorAndOpacity attribute */
	void SetColorAndOpacity(TAttribute<FSlateColor> Color);

	/** See the AllowContextMenu attribute */
	void SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu);

	/**
	 * Sets the font used to draw the text
	 *
	 * @param  InNewFont	The new font to use
	 */
	void SetFont( const TAttribute< FSlateFontInfo >& InNewFont );

	/**
	 * Sets the minimum width that a text block should be.
	 *
	 * @param  InMinDesiredWidth	The minimum width
	 */
	void SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth);

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
	 * Sets the OnKeyDownHandler to provide first chance handling of the OnKeyDown event
	 *
	 * @param InOnKeyDownHandler			Delegate to call during OnKeyDown event
	 */
	void SetOnKeyDownHandler(FOnKeyDown InOnKeyDownHandler)
	{
		OnKeyDownHandler = InOnKeyDownHandler;
	}

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

	/** Move the cursor to the given location in the document (will also scroll to this point) */
	void GoTo(const FTextLocation& NewLocation);

	/** Move the cursor specified location */
	void GoTo(ETextLocation NewLocation);

	/** Scroll to the given location in the document (without moving the cursor) */
	void ScrollTo(const FTextLocation& NewLocation);

	/** Begin a new text search (this is called automatically when the bound search text changes) */
	void BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase, const bool InReverse = false);

	/** Advance the current search to the next match (does nothing if not currently searching) */
	void AdvanceSearch(const bool InReverse = false);

protected:
	//~ Begin SWidget Interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void CacheDesiredSize(float LayoutScaleMultiplier) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnKeyChar( const FGeometry& MyGeometry,  const FCharacterEvent& InCharacterEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual const FSlateBrush* GetFocusBrush() const;
	virtual bool IsInteractable() const override;
	virtual bool ComputeVolatility() const override;
	//~ End SWidget Interface

protected:
	/** Synchronize the text style currently set (including from overrides) and update the text layout if required */
	void SynchronizeTextStyle();

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
	/** Text marshaller used by the editable text layout */
	TSharedPtr<FPlainTextLayoutMarshaller> PlainTextMarshaller;

	/** The text layout that deals with the editable text */
	TUniquePtr<FSlateEditableTextLayout> EditableTextLayout;

	/** The font used to draw the text */
	TAttribute<FSlateFontInfo> Font;

	/** Text color and opacity */
	TAttribute<FSlateColor> ColorAndOpacity;

	/** Background image for the selected text */
	TAttribute<const FSlateBrush*> BackgroundImageSelected;

	/** Sets whether this text box can actually be modified interactively by the user */
	TAttribute<bool> bIsReadOnly;

	/** Sets whether this text box is for storing a password */
	TAttribute<bool> bIsPassword;

	/** Workaround as we loose focus when the auto completion closes. */
	TAttribute<bool> bIsCaretMovedWhenGainFocus;
	
	/** Whether to select all text when the user clicks to give focus on the widget */
	TAttribute<bool> bSelectAllTextWhenFocused;

	/** Whether to allow the user to back out of changes when they press the escape key */
	TAttribute<bool> bRevertTextOnEscape;

	/** Whether to clear keyboard focus when pressing enter to commit changes */
	TAttribute<bool> bClearKeyboardFocusOnCommit;

	/** Whether to select all text when pressing enter to commit changes */
	TAttribute<bool> bSelectAllTextOnCommit;

	/** Whether to disable the context menu */
	TAttribute<bool> bAllowContextMenu;

	/** Delegate to call before a context menu is opened */
	FOnContextMenuOpening OnContextMenuOpening;

	/** Called when a character is typed and we want to know if the text field supports typing this character. */
	FOnIsTypedCharValid OnIsTypedCharValid;

	/** Called whenever the text is changed interactively by the user */
	FOnTextChanged OnTextChangedCallback;

	/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
	FOnTextCommitted OnTextCommittedCallback;

	/** Prevents the editable text from being smaller than desired in certain cases (e.g. when it is empty) */
	TAttribute<float> MinDesiredWidth;

	/** Menu extender for right-click context menu */
	TSharedPtr<FExtender> MenuExtender;

	/** The timer that is actively driving this widget to Tick() even when Slate is idle */
	TWeakPtr<FActiveTimerHandle> ActiveTickTimer;

	/** The iterator to use to detect word boundaries */
	mutable TSharedPtr<IBreakIterator> WordBreakIterator;

	/** Callback delegate to have first chance handling of the OnKeyDown event */
	FOnKeyDown OnKeyDownHandler;

	/** The type of virtual keyboard to use for editing this text on mobile */
	TAttribute<EKeyboardType> VirtualKeyboardType;

	/** The type of event that will trigger the display of the virtual keyboard */
	TAttribute<EVirtualKeyboardTrigger> VirtualKeyboardTrigger;

	/** The message action to take when the virtual keyboard is dismissed by the user */
	TAttribute<EVirtualKeyboardDismissAction> VirtualKeyboardDismissAction;
};
