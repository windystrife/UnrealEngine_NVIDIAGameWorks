// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SEditableText.h"
#include "Framework/Text/TextEditHelper.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Widgets/Text/SlateEditableTextLayout.h"
#include "Types/ReflectionMetadata.h"

SEditableText::SEditableText()
{
}

SEditableText::~SEditableText()
{
	// Needed to avoid "deletion of pointer to incomplete type 'FSlateEditableTextLayout'; no destructor called" error when using TUniquePtr
}

void SEditableText::Construct( const FArguments& InArgs )
{
	bIsReadOnly = InArgs._IsReadOnly;
	bIsPassword = InArgs._IsPassword;

	bIsCaretMovedWhenGainFocus = InArgs._IsCaretMovedWhenGainFocus;
	bSelectAllTextWhenFocused = InArgs._SelectAllTextWhenFocused;
	bRevertTextOnEscape = InArgs._RevertTextOnEscape;
	bClearKeyboardFocusOnCommit = InArgs._ClearKeyboardFocusOnCommit;
	bAllowContextMenu = InArgs._AllowContextMenu;
	OnContextMenuOpening = InArgs._OnContextMenuOpening;
	OnIsTypedCharValid = InArgs._OnIsTypedCharValid;
	OnTextChangedCallback = InArgs._OnTextChanged;
	OnTextCommittedCallback = InArgs._OnTextCommitted;
	MinDesiredWidth = InArgs._MinDesiredWidth;
	bSelectAllTextOnCommit = InArgs._SelectAllTextOnCommit;
	VirtualKeyboardType = InArgs._VirtualKeyboardType;
	VirtualKeyboardTrigger = InArgs._VirtualKeyboardTrigger;
	VirtualKeyboardDismissAction = InArgs._VirtualKeyboardDismissAction;
	OnKeyDownHandler = InArgs._OnKeyDownHandler;

	Font = InArgs._Font;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	BackgroundImageSelected = InArgs._BackgroundImageSelected;

	// We use the given style when creating the text layout as it may not be safe to call the override delegates until we've finished being constructed
	// The first call to SyncronizeTextStyle will apply the correct overrides, and that will happen before the first paint
	check(InArgs._Style);
	FTextBlockStyle TextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	TextStyle.Font = InArgs._Style->Font;
	TextStyle.ColorAndOpacity = InArgs._Style->ColorAndOpacity;
	TextStyle.HighlightShape = InArgs._Style->BackgroundImageSelected;

	PlainTextMarshaller = FPlainTextLayoutMarshaller::Create();
	PlainTextMarshaller->SetIsPassword(bIsPassword);

	// We use a separate marshaller for the hint text, as that should never be displayed as a password
	TSharedRef<FPlainTextLayoutMarshaller> HintTextMarshaller = FPlainTextLayoutMarshaller::Create();

	EditableTextLayout = MakeUnique<FSlateEditableTextLayout>(*this, InArgs._Text, TextStyle, InArgs._TextShapingMethod, InArgs._TextFlowDirection, FCreateSlateTextLayout(), PlainTextMarshaller.ToSharedRef(), HintTextMarshaller);
	EditableTextLayout->SetHintText(InArgs._HintText);
	EditableTextLayout->SetSearchText(InArgs._SearchText);
	EditableTextLayout->SetCursorBrush(InArgs._CaretImage.IsSet() ? InArgs._CaretImage : &InArgs._Style->CaretImage);
	EditableTextLayout->SetCompositionBrush(InArgs._BackgroundImageComposing.IsSet() ? InArgs._BackgroundImageComposing : &InArgs._Style->BackgroundImageComposing);
	EditableTextLayout->SetDebugSourceInfo(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateLambda([this]{ return FReflectionMetaData::GetWidgetDebugInfo(this); })));
	EditableTextLayout->SetJustification(InArgs._Justification);

	// build context menu extender
	MenuExtender = MakeShareable(new FExtender());
	MenuExtender->AddMenuExtension("EditText", EExtensionHook::Before, TSharedPtr<FUICommandList>(), InArgs._ContextMenuExtender);
}

void SEditableText::SetText( const TAttribute< FText >& InNewText )
{
	EditableTextLayout->SetText(InNewText);
}

FText SEditableText::GetText() const
{
	return EditableTextLayout->GetText();
}

void SEditableText::SetFont( const TAttribute< FSlateFontInfo >& InNewFont )
{
	Font = InNewFont;
}

void SEditableText::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	EditableTextLayout->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SEditableText::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FTextBlockStyle& EditableTextStyle = EditableTextLayout->GetTextStyle();
	const FLinearColor ForegroundColor = EditableTextStyle.ColorAndOpacity.GetColor(InWidgetStyle);

	FWidgetStyle TextWidgetStyle = FWidgetStyle(InWidgetStyle)
		.SetForegroundColor(ForegroundColor);

	LayerId = EditableTextLayout->OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, TextWidgetStyle, ShouldBeEnabled(bParentEnabled));

	return LayerId;
}

void SEditableText::CacheDesiredSize(float LayoutScaleMultiplier)
{
	SynchronizeTextStyle();
	EditableTextLayout->CacheDesiredSize(LayoutScaleMultiplier);
	SWidget::CacheDesiredSize(LayoutScaleMultiplier);
}

FVector2D SEditableText::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D TextLayoutSize = EditableTextLayout->ComputeDesiredSize(LayoutScaleMultiplier);
	TextLayoutSize.X = FMath::Max(TextLayoutSize.X, MinDesiredWidth.Get());
	return TextLayoutSize;
}

FChildren* SEditableText::GetChildren()
{
	return EditableTextLayout->GetChildren();
}

void SEditableText::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	EditableTextLayout->OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

FReply SEditableText::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if ( DragDropOp.IsValid() )
	{
		if ( DragDropOp->HasText() )
		{
			return FReply::Handled();
		}
	}
	
	return FReply::Unhandled();
}

FReply SEditableText::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if ( DragDropOp.IsValid() )
	{
		if ( DragDropOp->HasText() )
		{
			EditableTextLayout->SetText(FText::FromString(DragDropOp->GetText()));
			return FReply::Handled();
		}
	}
	
	return FReply::Unhandled();
}

bool SEditableText::SupportsKeyboardFocus() const
{
	return true;
}

FReply SEditableText::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	EditableTextLayout->HandleFocusReceived(InFocusEvent);
	return FReply::Handled();
}

void SEditableText::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	EditableTextLayout->HandleFocusLost(InFocusEvent);
}

FReply SEditableText::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return EditableTextLayout->HandleKeyChar(InCharacterEvent);
}

FReply SEditableText::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FReply Reply = FReply::Unhandled();

	// First call the user defined key handler, there might be overrides to normal functionality
	if (OnKeyDownHandler.IsBound())
	{
		Reply = OnKeyDownHandler.Execute(MyGeometry, InKeyEvent);
	}

	if (!Reply.IsEventHandled())
	{
		Reply = EditableTextLayout->HandleKeyDown(InKeyEvent);

		if (!Reply.IsEventHandled())
		{
			Reply = SWidget::OnKeyDown(MyGeometry, InKeyEvent);
		}
	}

	return Reply;
}

FReply SEditableText::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return EditableTextLayout->HandleKeyUp(InKeyEvent);
}

FReply SEditableText::OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return EditableTextLayout->HandleMouseButtonDown(InMyGeometry, InMouseEvent);
}

FReply SEditableText::OnMouseButtonUp( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return EditableTextLayout->HandleMouseButtonUp(InMyGeometry, InMouseEvent);
}

FReply SEditableText::OnMouseMove( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return EditableTextLayout->HandleMouseMove(InMyGeometry, InMouseEvent);
}

FReply SEditableText::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return EditableTextLayout->HandleMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

FCursorReply SEditableText::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	return FCursorReply::Cursor( EMouseCursor::TextEditBeam );
}

const FSlateBrush* SEditableText::GetFocusBrush() const
{
	return nullptr;
}

bool SEditableText::IsInteractable() const
{
	return IsEnabled();
}

bool SEditableText::ComputeVolatility() const
{
	return SWidget::ComputeVolatility()
		|| HasKeyboardFocus()
		|| EditableTextLayout->ComputeVolatility()
		|| Font.IsBound()
		|| ColorAndOpacity.IsBound()
		|| BackgroundImageSelected.IsBound()
		|| bIsReadOnly.IsBound()
		|| bIsPassword.IsBound()
		|| MinDesiredWidth.IsBound();
}

void SEditableText::SetHintText( const TAttribute< FText >& InHintText )
{
	EditableTextLayout->SetHintText(InHintText);
}

FText SEditableText::GetHintText() const
{
	return EditableTextLayout->GetHintText();
}

void SEditableText::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableTextLayout->SetSearchText(InSearchText);
}

FText SEditableText::GetSearchText() const
{
	return EditableTextLayout->GetSearchText();
}

void SEditableText::SetIsReadOnly( TAttribute< bool > InIsReadOnly )
{
	bIsReadOnly = InIsReadOnly;
}

void SEditableText::SetIsPassword( TAttribute< bool > InIsPassword )
{
	bIsPassword = InIsPassword;
	PlainTextMarshaller->SetIsPassword(bIsPassword);
}

void SEditableText::SetColorAndOpacity(TAttribute<FSlateColor> Color)
{
	ColorAndOpacity = Color;
}

void SEditableText::SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
}

void SEditableText::SetIsCaretMovedWhenGainFocus(const TAttribute<bool>& InIsCaretMovedWhenGainFocus)
{
	bIsCaretMovedWhenGainFocus = InIsCaretMovedWhenGainFocus;
}

void SEditableText::SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused)
{
	bSelectAllTextWhenFocused = InSelectAllTextWhenFocused;
}

void SEditableText::SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape)
{
	bRevertTextOnEscape = InRevertTextOnEscape;
}

void SEditableText::SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit)
{
	bClearKeyboardFocusOnCommit = InClearKeyboardFocusOnCommit;
}

void SEditableText::SetSelectAllTextOnCommit(const TAttribute<bool>& InSelectAllTextOnCommit)
{
	bSelectAllTextOnCommit = InSelectAllTextOnCommit;
}

void SEditableText::SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu)
{
	bAllowContextMenu = InAllowContextMenu;
}

void SEditableText::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableTextLayout->SetTextShapingMethod(InTextShapingMethod);
}

void SEditableText::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableTextLayout->SetTextFlowDirection(InTextFlowDirection);
}

bool SEditableText::AnyTextSelected() const
{
	return EditableTextLayout->AnyTextSelected();
}

void SEditableText::SelectAllText()
{
	EditableTextLayout->SelectAllText();
}

void SEditableText::ClearSelection()
{
	EditableTextLayout->ClearSelection();
}

FText SEditableText::GetSelectedText() const
{
	return EditableTextLayout->GetSelectedText();
}

void SEditableText::GoTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->GoTo(NewLocation);
}

void SEditableText::GoTo(ETextLocation GoToLocation)
{
	EditableTextLayout->GoTo(GoToLocation);
}

void SEditableText::ScrollTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->ScrollTo(NewLocation);
}

void SEditableText::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableTextLayout->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SEditableText::AdvanceSearch(const bool InReverse)
{
	EditableTextLayout->AdvanceSearch(InReverse);
}

void SEditableText::SynchronizeTextStyle()
{
	// Has the style used for this editable text changed?
	bool bTextStyleChanged = false;
	FTextBlockStyle NewTextStyle = EditableTextLayout->GetTextStyle();

	// Sync from the font override
	if (Font.IsSet())
	{
		const FSlateFontInfo& NewFontInfo = Font.Get();
		if (NewTextStyle.Font != NewFontInfo)
		{
			NewTextStyle.Font = NewFontInfo;
			bTextStyleChanged = true;
		}
	}

	// Sync from the color override
	if (ColorAndOpacity.IsSet())
	{
		const FSlateColor& NewColorAndOpacity = ColorAndOpacity.Get();
		if (NewTextStyle.ColorAndOpacity != NewColorAndOpacity)
		{
			NewTextStyle.ColorAndOpacity = NewColorAndOpacity;
			bTextStyleChanged = true;
		}
	}

	// Sync from the highlight shape override
	if (BackgroundImageSelected.IsSet())
	{
		const FSlateBrush* NewSelectionBrush = BackgroundImageSelected.Get();
		if (NewSelectionBrush && NewTextStyle.HighlightShape != *NewSelectionBrush)
		{
			NewTextStyle.HighlightShape = *NewSelectionBrush;
			bTextStyleChanged = true;
		}
	}

	if (bTextStyleChanged)
	{
		EditableTextLayout->SetTextStyle(NewTextStyle);
		EditableTextLayout->Refresh();
	}
}

bool SEditableText::IsTextReadOnly() const
{
	return bIsReadOnly.Get(false);
}

bool SEditableText::IsTextPassword() const
{
	return bIsPassword.Get(false);
}

bool SEditableText::IsMultiLineTextEdit() const
{
	return false;
}

bool SEditableText::ShouldJumpCursorToEndWhenFocused() const
{
	return bIsCaretMovedWhenGainFocus.Get(false);
}

bool SEditableText::ShouldSelectAllTextWhenFocused() const
{
	return bSelectAllTextWhenFocused.Get(false);
}

bool SEditableText::ShouldClearTextSelectionOnFocusLoss() const
{
	return true;
}

bool SEditableText::ShouldRevertTextOnEscape() const
{
	return bRevertTextOnEscape.Get(false);
}

bool SEditableText::ShouldClearKeyboardFocusOnCommit() const
{
	return bClearKeyboardFocusOnCommit.Get(false);
}

bool SEditableText::ShouldSelectAllTextOnCommit() const
{
	return bSelectAllTextOnCommit.Get(false);
}

bool SEditableText::CanInsertCarriageReturn() const
{
	return false;
}

bool SEditableText::CanTypeCharacter(const TCHAR InChar) const
{
	return !OnIsTypedCharValid.IsBound() || OnIsTypedCharValid.Execute(InChar);
}

void SEditableText::EnsureActiveTick()
{
	TSharedPtr<FActiveTimerHandle> ActiveTickTimerPin = ActiveTickTimer.Pin();
	if (ActiveTickTimerPin.IsValid())
	{
		return;
	}

	auto DoActiveTick = [this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
	{
		// Continue if we still have focus, otherwise treat as a fire-and-forget Tick() request
		const bool bShouldAppearFocused = HasKeyboardFocus() || EditableTextLayout->HasActiveContextMenu();
		return (bShouldAppearFocused) ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	};

	const float TickPeriod = EditableTextDefs::BlinksPerSecond * 0.5f;
	ActiveTickTimer = RegisterActiveTimer(TickPeriod, FWidgetActiveTimerDelegate::CreateLambda(DoActiveTick));
}

EKeyboardType SEditableText::GetVirtualKeyboardType() const
{
	return VirtualKeyboardType.Get();
}

EVirtualKeyboardTrigger SEditableText::GetVirtualKeyboardTrigger() const
{
	return VirtualKeyboardTrigger.Get();
}

EVirtualKeyboardDismissAction SEditableText::GetVirtualKeyboardDismissAction() const
{
	return VirtualKeyboardDismissAction.Get();
}

TSharedRef<SWidget> SEditableText::GetSlateWidget()
{
	return AsShared();
}

TSharedPtr<SWidget> SEditableText::GetSlateWidgetPtr()
{
	if (DoesSharedInstanceExist())
	{
		return AsShared();
	}
	return nullptr;
}

TSharedPtr<SWidget> SEditableText::BuildContextMenuContent() const
{
	if (!bAllowContextMenu.Get())
	{
		return nullptr;
	}

	if (OnContextMenuOpening.IsBound())
	{
		return OnContextMenuOpening.Execute();
	}
	
	return EditableTextLayout->BuildDefaultContextMenu(MenuExtender);
}

void SEditableText::OnTextChanged(const FText& InText)
{
	OnTextChangedCallback.ExecuteIfBound(InText);
}

void SEditableText::OnTextCommitted(const FText& InText, const ETextCommit::Type InTextAction)
{
	OnTextCommittedCallback.ExecuteIfBound(InText, InTextAction);
}

void SEditableText::OnCursorMoved(const FTextLocation& InLocation)
{
}

float SEditableText::UpdateAndClampHorizontalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	return EditableTextLayout->GetScrollOffset().X;
}

float SEditableText::UpdateAndClampVerticalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	return 0.0f;
}
