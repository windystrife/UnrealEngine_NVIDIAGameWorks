// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/IMenu.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Widgets/Text/ISlateEditableTextWidget.h"
#include "Framework/Text/ITextLayoutMarshaller.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/TextLineHighlight.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/Text/SlateEditableTextTypes.h"
#include "Framework/Text/SlateTextLayoutFactory.h"
#include "GenericPlatform/ITextInputMethodSystem.h"

class FArrangedChildren;
class FExtender;
class FPaintArgs;
class FSlateWindowElementList;
class FTextBlockLayout;
class FUICommandList;
class IBreakIterator;
class SWindow;
enum class ETextShapingMethod : uint8;

/** Class to handle the cached layout of SEditableText/SMultiLineEditableText by proxying around a FTextLayout */
class SLATE_API FSlateEditableTextLayout
{
public:
	FSlateEditableTextLayout(ISlateEditableTextWidget& InOwnerWidget, const TAttribute<FText>& InInitialText, FTextBlockStyle InTextStyle, const TOptional<ETextShapingMethod> InTextShapingMethod, const TOptional<ETextFlowDirection> InTextFlowDirection, const FCreateSlateTextLayout& InCreateSlateTextLayout, TSharedRef<ITextLayoutMarshaller> InTextMarshaller, TSharedRef<ITextLayoutMarshaller> InHintTextMarshaller);
	~FSlateEditableTextLayout();

	void SetText(const TAttribute<FText>& InText);
	FText GetText() const;

	void SetHintText(const TAttribute<FText>& InHintText);
	FText GetHintText() const;

	void SetSearchText(const TAttribute<FText>& InSearchText);
	FText GetSearchText() const;

	void SetTextStyle(const FTextBlockStyle& InTextStyle);
	const FTextBlockStyle& GetTextStyle() const;

	/** Set the brush to use when drawing the cursor */
	void SetCursorBrush(const TAttribute<const FSlateBrush*>& InCursorBrush);

	/** Set the brush to use when drawing the composition highlight */
	void SetCompositionBrush(const TAttribute<const FSlateBrush*>& InCompositionBrush);

	/** Get the plain text string without rich-text formatting */
	FText GetPlainText() const;

	/**
	 * Sets the current editable text for this text block
	 * Note: Doesn't update the value of BoundText, nor does it call OnTextChanged
	 *
	 * @param TextToSet	The new text to set in the internal TextLayout
	 * @param bForce	True to force the update, even if the text currently matches what's in the TextLayout
	 *
	 * @return true if the text was updated, false if the text wasn't update (because it was already up-to-date)
	 */
	bool SetEditableText(const FText& TextToSet, const bool bForce = false);

	/**
	 * Gets the current editable text for this text block
	 * Note: We don't store text in this form (it's stored as lines in the text layout) so every call to this function has to reconstruct it
	 */
	FText GetEditableText() const;

	/** Get the currently selected text */
	FText GetSelectedText() const;

	/** Set the text shaping method that the internal text layout should use */
	void SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod);

	/** Set the text flow direction that the internal text layout should use */
	void SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection);

	/** Set the wrapping to use for this document */
	void SetTextWrapping(const TAttribute<float>& InWrapTextAt, const TAttribute<bool>& InAutoWrapText, const TAttribute<ETextWrappingPolicy>& InWrappingPolicy);

	/** Set whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs */
	void SetWrapTextAt(const TAttribute<float>& InWrapTextAt);

	/** Set whether to wrap text automatically based on the widget's computed horizontal space */
	void SetAutoWrapText(const TAttribute<bool>& InAutoWrapText);

	/** Set the wrapping policy to use */
	void SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy);

	/** Set the amount of blank space left around the edges of text area */
	void SetMargin(const TAttribute<FMargin>& InMargin);

	/** Set how the text should be aligned with the margin */
	void SetJustification(const TAttribute<ETextJustify::Type>& InJustification);

	/** Set the amount to scale each lines height by */
	void SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage);

	/** Set the information used to help identify who owns this text layout in the case of an error */
	void SetDebugSourceInfo(const TAttribute<FString>& InDebugSourceInfo);

	/** Get the virtual keyboard handler for this text layout */
	TSharedRef<IVirtualKeyboardEntry> GetVirtualKeyboardEntry() const;

	/** Get the IME context for this text layout */
	TSharedRef<ITextInputMethodContext> GetTextInputMethodContext() const;

	/** Refresh this editable text immediately, rather than wait for the usual caching mechanisms to take affect on the text Tick */
	bool Refresh();

	/** Force the text layout to be updated from the marshaller */
	void ForceRefreshTextLayout(const FText& CurrentText);

	/** Begin a new text search (this is called automatically when BoundSearchText changes) */
	void BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase = ESearchCase::IgnoreCase, const bool InReverse = false);

	/** Advance the current search to the next match (does nothing if not currently searching) */
	void AdvanceSearch(const bool InReverse = false);

	/** Update the horizontal scroll amount from the given fraction */
	FVector2D SetHorizontalScrollFraction(const float InScrollOffsetFraction);

	/** Update the vertical scroll amount from the given fraction */
	FVector2D SetVerticalScrollFraction(const float InScrollOffsetFraction);

	/** Set the absolute scroll offset value */
	FVector2D SetScrollOffset(const FVector2D& InScrollOffset, const FGeometry& InGeometry);

	/** Get the absolute scroll offset value */
	FVector2D GetScrollOffset() const;

	/** Called when our parent widget receives focus */
	bool HandleFocusReceived(const FFocusEvent& InFocusEvent);

	/** Called when our parent widget loses focus */
	bool HandleFocusLost(const FFocusEvent& InFocusEvent);

	/** Called to handle an OnKeyChar event from our parent widget */
	FReply HandleKeyChar(const FCharacterEvent& InCharacterEvent);

	/** Called to handle an OnKeyDown event from our parent widget */
	FReply HandleKeyDown(const FKeyEvent& InKeyEvent);

	/** Called to handle an OnKeyUp event from our parent widget */
	FReply HandleKeyUp(const FKeyEvent& InKeyEvent);

	/** Called to handle an OnMouseButtonDown event from our parent widget */
	FReply HandleMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent);

	/** Called to handle an OnMouseButtonUp event from our parent widget */
	FReply HandleMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent);

	/** Called to handle an OnMouseMove event from our parent widget */
	FReply HandleMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called to handle an OnMouseButtonDoubleClick event from our parent widget */
	FReply HandleMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called to handle an escape action acting on the current selection */
	bool HandleEscape();

	/** Called to handle a backspace action acting on the current selection or at the cursor position */
	bool HandleBackspace();

	/** Called to handle a delete action acting on the current selection or at the cursor position */
	bool HandleDelete();

	/** Called to handle a typing a character on the current selection or at the cursor position */
	bool HandleTypeChar(const TCHAR InChar);

	/** Called to handle a carriage return action acting on the current selection or at the cursor position */
	bool HandleCarriageReturn();

	/** Are we able to delete the currently selected text? */
	bool CanExecuteDelete() const;

	/** Delete any currently selected text */
	void DeleteSelectedText();

	/** Query to see if any text is selected within the document */
	bool AnyTextSelected() const;

	/** Query to see if the text under the given position is currently selected */
	bool IsTextSelectedAt(const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition) const;

	/** Query to see if the text under the given position is currently selected (the position is local to the text layout space) */
	bool IsTextSelectedAt(const FVector2D& InLocalPosition) const;

	/** Are we able to execute the "Select All" command? */
	bool CanExecuteSelectAll() const;

	/** Select all the text in the document */
	void SelectAllText();

	/** Select the word under the given position */
	void SelectWordAt(const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition);

	/** Select the word under the given position (the position is local to the text layout space) */
	void SelectWordAt(const FVector2D& InLocalPosition);

	/** Clear the active text selection */
	void ClearSelection();

	/** Are we able to cut the currently selected text? */
	bool CanExecuteCut() const;

	/** Cut the currently selected text and place it on the clipboard */
	void CutSelectedTextToClipboard();

	/** Are we able to copy the currently selected text? */
	bool CanExecuteCopy() const;

	/** Copy the currently selected text and place it on the clipboard */
	void CopySelectedTextToClipboard();

	/** Are we able to paste the text from the clipboard into this document? */
	bool CanExecutePaste() const;

	/** Paste the text from the clipboard into this document */
	void PasteTextFromClipboard();

	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursor(const FString& InString);

	/** Insert the given run at the current cursor position */
	void InsertRunAtCursor(TSharedRef<IRun> InRun);

	/** Move the cursor in the document using the specified move method */
	bool MoveCursor(const FMoveCursor& InArgs);

	/** Move the cursor to the given location in the document (will also scroll to this point) */
	void GoTo(const FTextLocation& NewLocation);

	/** Move the cursor specified location */
	void GoTo(ETextLocation NewLocation);

	/** Jump the cursor to the given location in the document */
	void JumpTo(ETextLocation JumpLocation, ECursorAction Action);

	/** Scroll to the given location in the document (without moving the cursor) */
	void ScrollTo(const FTextLocation& NewLocation);

	/** Update the active cursor highlight based on the state of the text layout */
	void UpdateCursorHighlight();

	/** Remove any active cursor highlights */
	void RemoveCursorHighlight();

	/** Update the preferred offset of the cursor, based on the current state of the text layout */
	void UpdatePreferredCursorScreenOffsetInLine();

	/** Apply the given style to the currently selected text (or insert a new run at the current cursor position if no text is selected) */
	void ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle);

	/** Get the run currently under the cursor, or null if there is no run currently under the cursor */
	TSharedPtr<const IRun> GetRunUnderCursor() const;

	/** Get the runs currently that are current selected, some of which may be only partially selected */
	TArray<TSharedRef<const IRun>> GetSelectedRuns() const;

	/**
	 * Given a location and a Direction to offset, return a new location.
	 *
	 * @param Location    Cursor location from which to offset
	 * @param Direction   Positive means right, negative means left.
	 */
	FTextLocation TranslatedLocation(const FTextLocation& CurrentLocation, int8 Direction) const;

	/**
	 * Given a location and a Direction to offset, return a new location.
	 *
	 * @param Location              Cursor location from which to offset
	 * @param NumLinesToMove        Number of lines to move in a given direction. Positive means down, negative means up.
	 * @param GeometryScale         Geometry DPI scale at which the widget is being rendered
	 * @param OutCursorPosition     Fill with the updated cursor position.
	 * @param OutCursorAlignment    Optionally fill with a new cursor alignment (will be auto-calculated if not set).
	 */
	void TranslateLocationVertical(const FTextLocation& Location, int32 NumLinesToMove, float GeometryScale, FTextLocation& OutCursorPosition, TOptional<SlateEditableTextTypes::ECursorAlignment>& OutCursorAlignment) const;

	/** Find the closest word boundary */
	FTextLocation ScanForWordBoundary(const FTextLocation& Location, int8 Direction) const;

	/** Get the character at Location */
	TCHAR GetCharacterAt(const FTextLocation& Location) const;

	/** Are we at the beginning of all the text. */
	bool IsAtBeginningOfDocument(const FTextLocation& Location) const;

	/** Are we at the end of all the text. */
	bool IsAtEndOfDocument(const FTextLocation& Location) const;

	/** Is this location the beginning of a line */
	bool IsAtBeginningOfLine(const FTextLocation& Location) const;

	/** Is this location the end of a line. */
	bool IsAtEndOfLine(const FTextLocation& Location) const;

	/** Are we currently at the beginning of a word */
	bool IsAtWordStart(const FTextLocation& Location) const;

	/** Restores the text to the original state */
	void RestoreOriginalText();

	/** Returns whether the current text varies from the original */
	bool HasTextChangedFromOriginal() const;

	/** Called to begin an undoable editable text transaction */
	void BeginEditTransation();

	/** Called to end an undoable editable text transaction */
	void EndEditTransaction();

	/** Push the given undo state onto the undo stack */
	void PushUndoState(const SlateEditableTextTypes::FUndoState& InUndoState);

	/** Clear the current undo stack */
	void ClearUndoStates();

	/** Create an undo state that reflects the current state of the document */
	void MakeUndoState(SlateEditableTextTypes::FUndoState& OutUndoState);

	/** Are we currently able to execute an undo action? */
	bool CanExecuteUndo() const;
	
	/** Execute an undo action */
	void Undo();

	/** Are we currently able to execute a redo action? */
	bool CanExecuteRedo() const;

	/** Execute a redo action */
	void Redo();

	void SaveText(const FText& TextToSave);

	void LoadText();

	bool ComputeVolatility() const;

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled);

	void CacheDesiredSize(float LayoutScaleMultiplier);

	FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const;

	FChildren* GetChildren();

	void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const;

	FVector2D GetSize() const;

	TSharedRef<SWidget> BuildDefaultContextMenu(const TSharedPtr<FExtender>& InMenuExtender) const;

	bool HasActiveContextMenu() const;

private:
	/** Insert the given text at the current cursor position, correctly taking into account new line characters */
	void InsertTextAtCursorImpl(const FString& InString);

	/** Insert a new-line at the current cursor position */
	void InsertNewLineAtCursorImpl();

	/** Implementation of Refresh that actually updates the layout. Optionally takes text to set, or will use the current editable text if none if provided */
	bool RefreshImpl(const FText* InTextToSet, const bool bForce = false);

	/** Create a text or password run using the given text and style */
	TSharedRef<IRun> CreateTextOrPasswordRun(const FRunInfo& InRunInfo, const TSharedRef<const FString>& InText, const FTextBlockStyle& InStyle);

	/** Called when the active context menu is closed */
	void OnContextMenuClosed(TSharedRef<IMenu> Menu);

private:
	/** Virtual keyboard handler for an editable text layout */
	friend class FVirtualKeyboardEntry;
	class FVirtualKeyboardEntry : public IVirtualKeyboardEntry
	{
	public:
		static TSharedRef<FVirtualKeyboardEntry> Create(FSlateEditableTextLayout& InOwnerLayout);

		virtual void SetTextFromVirtualKeyboard(const FText& InNewText, ETextEntryType TextEntryType) override;
		virtual FText GetText() const override;
		virtual FText GetHintText() const override;
		virtual EKeyboardType GetVirtualKeyboardType() const override;
		virtual bool IsMultilineEntry() const override;

	private:
		FVirtualKeyboardEntry(FSlateEditableTextLayout& InOwnerLayout);
		FSlateEditableTextLayout* OwnerLayout;
	};

private:
	/**
	 * Note: The IME interface for the multiline editable text uses the pre-flowed version of the string since the IME APIs are designed to work with flat strings
	 *		 This means we have to do a bit of juggling to convert between the two
	 */
	friend class FTextInputMethodContext;
	class FTextInputMethodContext : public ITextInputMethodContext
	{
	public:
		static TSharedRef<FTextInputMethodContext> Create(FSlateEditableTextLayout& InOwnerLayout);

		void CacheWindow();

		FORCEINLINE void KillContext()
		{
			OwnerLayout = nullptr;
			bIsComposing = false;
		}

		FORCEINLINE FTextRange GetCompositionRange() const
		{
			return FTextRange(CompositionBeginIndex, CompositionBeginIndex + CompositionLength);
		}

		bool UpdateCachedGeometry(const FGeometry& InAllottedGeometry)
		{
			if (CachedGeometry != InAllottedGeometry)
			{
				CachedGeometry = InAllottedGeometry;
				return true;
			}
			return false;
		}

		virtual bool IsComposing() override;
		virtual bool IsReadOnly() override;
		virtual uint32 GetTextLength() override;
		virtual void GetSelectionRange(uint32& BeginIndex, uint32& Length, ECaretPosition& CaretPosition) override;
		virtual void SetSelectionRange(const uint32 BeginIndex, const uint32 Length, const ECaretPosition CaretPosition) override;
		virtual void GetTextInRange(const uint32 BeginIndex, const uint32 Length, FString& OutString) override;
		virtual void SetTextInRange(const uint32 BeginIndex, const uint32 Length, const FString& InString) override;
		virtual int32 GetCharacterIndexFromPoint(const FVector2D& Point) override;
		virtual bool GetTextBounds(const uint32 BeginIndex, const uint32 Length, FVector2D& Position, FVector2D& Size) override;
		virtual void GetScreenBounds(FVector2D& Position, FVector2D& Size) override;
		virtual TSharedPtr<FGenericWindow> GetWindow() override;
		virtual void BeginComposition() override;
		virtual void UpdateCompositionRange(const int32 InBeginIndex, const uint32 InLength) override;
		virtual void EndComposition() override;

	private:
		FTextInputMethodContext(FSlateEditableTextLayout& InOwnerLayout);
		FSlateEditableTextLayout* OwnerLayout;
		TWeakPtr<SWindow> CachedParentWindow;

		FGeometry CachedGeometry;
		bool bIsComposing;
		int32 CompositionBeginIndex;
		uint32 CompositionLength;
	};

private:
	/** Pointer to the interface for our owner widget */
	ISlateEditableTextWidget* OwnerWidget;

	/** The iterator to use to detect grapheme cluster boundaries */
	TSharedPtr<IBreakIterator> GraphemeBreakIterator;

	/** The marshaller used to get/set the BoundText text to/from the text layout. */
	TSharedPtr<ITextLayoutMarshaller> Marshaller;

	/** The marshaller used to get/set the HintText text to/from the text layout. */
	TSharedPtr<ITextLayoutMarshaller> HintMarshaller;

	/** Delegate used to create internal text layouts. */
	FCreateSlateTextLayout CreateSlateTextLayout;

	/** In control of the layout and wrapping of the BoundText */
	TSharedPtr<FSlateTextLayout> TextLayout;

	/** In control of the layout and wrapping of the HintText */
	TUniquePtr<FTextBlockLayout> HintTextLayout;

	/** Default style used by the TextLayout */
	FTextBlockStyle TextStyle;

	/** Style used to draw the hint text (only valid when HintTextLayout is set) */
	FTextBlockStyle HintTextStyle;

	/** The text displayed in this text block */
	TAttribute<FText> BoundText;

	/** The state of BoundText last Tick() (used to allow updates when the text is changed) */
	FTextSnapshot BoundTextLastTick;

	/** Was the editable text showing a password last Tick() (allows a forcible text layout update when changing state) */
	bool bWasPasswordLastTick;

	/** The text that appears when there is no text in the text box */
	TAttribute<FText> HintText;

	/** The text to be searched for */
	TAttribute<FText> BoundSearchText;

	/** The state of BoundSearchText last Tick() (used to allow updates when the text is changed) */
	FTextSnapshot BoundSearchTextLastTick;

	/** The active search text (set from BeginSearch) */
	FText SearchText;

	/** The case-sensitivity of the active search (set from BeginSearch) */
	ESearchCase::Type SearchCase;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	TAttribute<float> WrapTextAt;

	/** True if we're wrapping text automatically based on the computed horizontal space for this widget */
	TAttribute<bool> AutoWrapText;

	/** The wrapping policy we're using */
	TAttribute<ETextWrappingPolicy> WrappingPolicy;

	/** The amount of blank space left around the edges of text area */
	TAttribute<FMargin> Margin;

	/** How the text should be aligned with the margin */
	TAttribute<ETextJustify::Type> Justification;

	/** The amount to scale each lines height by */
	TAttribute<float> LineHeightPercentage;

	/** The information used to help identify who owns this text layout in the case of an error */
	TAttribute<FString> DebugSourceInfo;

	/** Virtual keyboard handler for this text layout */
	TSharedPtr<FVirtualKeyboardEntry> VirtualKeyboardEntry;

	/** True if the IME context for this text layout has been registered with the input method manager */
	bool bHasRegisteredTextInputMethodContext;

	/** IME context for this text layout */
	TSharedPtr<FTextInputMethodContext> TextInputMethodContext;

	/** Notification interface object for IMEs */
	TSharedPtr<ITextInputMethodChangeNotifier> TextInputMethodChangeNotifier;

	/** Layout highlighter used to draw the cursor */
	TSharedPtr<SlateEditableTextTypes::FCursorLineHighlighter> CursorLineHighlighter;

	/** Layout highlighter used to draw an active text composition */
	TSharedPtr<SlateEditableTextTypes::FTextCompositionHighlighter> TextCompositionHighlighter;

	/** Layout highlighter used to draw the active text selection */
	TSharedPtr<SlateEditableTextTypes::FTextSelectionHighlighter> TextSelectionHighlighter;

	/** Layout highlighter used to draw the active search selection */
	TSharedPtr<SlateEditableTextTypes::FTextSearchHighlighter> SearchSelectionHighlighter;

	/** Line highlights that have been added from this editable text layout (used for cleanup without removing) */
	TArray<FTextLineHighlight> ActiveLineHighlights;

	/** The scroll offset (in unscaled Slate units) for this text */
	FVector2D ScrollOffset;

	/** If set, the pending data containing a position that should be scrolled into view */
	TOptional<SlateEditableTextTypes::FScrollInfo> PositionToScrollIntoView;

	/** That start of the selection when there is a selection. The end is implicitly wherever the cursor happens to be. */
	TOptional<FTextLocation> SelectionStart;

	/** The user probably wants the cursor where they last explicitly positioned it horizontally. */
	float PreferredCursorScreenOffsetInLine;

	/** Current cursor data */
	SlateEditableTextTypes::FCursorInfo CursorInfo;

	/** Undo states */
	TArray<SlateEditableTextTypes::FUndoState> UndoStates;

	/** Current undo state level that we've rolled back to, or INDEX_NONE if we haven't undone. Used for 'Redo'. */
	int32 CurrentUndoLevel;

	/** Undo state that will be pushed if text is actually changed between calls to BeginEditTransation() and EndEditTransaction() */
	TOptional<SlateEditableTextTypes::FUndoState> StateBeforeChangingText;

	/** Original text undo state */
	SlateEditableTextTypes::FUndoState OriginalText;

	/** True if we're currently selecting text by dragging the mouse cursor with the left button held down */
	bool bIsDragSelecting;

	/** True if the last mouse down caused us to receive keyboard focus */
	bool bWasFocusedByLastMouseDown;

	/** True if characters were selected by dragging since the last keyboard focus.  Used for text selection. */
	bool bHasDragSelectedSinceFocused;

	/** Whether the text has been changed by a virtual keyboard */
	bool bTextChangedByVirtualKeyboard;

	/** Whether the text has been committed by a virtual keyboard */
	bool bTextCommittedByVirtualKeyboard;

	/** What text was submitted by a virtual keyboard */
	FText VirtualKeyboardText;

	/** How the text was committed by the virtual keyboard */
	ETextCommit::Type VirtualKeyboardTextCommitType;

	/** The last known size of the widget from the previous OnPaint, used to recalculate wrapping */
	FVector2D CachedSize;

	/** A list commands to execute if a user presses the corresponding key-binding in the text box */
	TSharedPtr<FUICommandList> UICommandList;

	/** Information about any active context menu widgets */
	FActiveTextEditContextMenu ActiveContextMenu;
};
