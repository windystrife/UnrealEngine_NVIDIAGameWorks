// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/EditableTextBox.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UEditableTextBox

UEditableTextBox::UEditableTextBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ForegroundColor_DEPRECATED = FLinearColor::Black;
	BackgroundColor_DEPRECATED = FLinearColor::White;
	ReadOnlyForegroundColor_DEPRECATED = FLinearColor::Black;

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font_DEPRECATED = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
	}

	// Grab other defaults from slate arguments.
	SEditableTextBox::FArguments Defaults;
	IsReadOnly = Defaults._IsReadOnly.Get();
	IsPassword = Defaults._IsPassword.Get();
	MinimumDesiredWidth = Defaults._MinDesiredWidth.Get();
	Padding_DEPRECATED = Defaults._Padding.Get();
	IsCaretMovedWhenGainFocus = Defaults._IsCaretMovedWhenGainFocus.Get();
	SelectAllTextWhenFocused = Defaults._SelectAllTextWhenFocused.Get();
	RevertTextOnEscape = Defaults._RevertTextOnEscape.Get();
	ClearKeyboardFocusOnCommit = Defaults._ClearKeyboardFocusOnCommit.Get();
	SelectAllTextOnCommit = Defaults._SelectAllTextOnCommit.Get();
	AllowContextMenu = Defaults._AllowContextMenu.Get();

	WidgetStyle = *Defaults._Style;
}

void UEditableTextBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyEditableTextBlock.Reset();
}

TSharedRef<SWidget> UEditableTextBox::RebuildWidget()
{
	MyEditableTextBlock = SNew(SEditableTextBox)
		.Style(&WidgetStyle)
		.MinDesiredWidth(MinimumDesiredWidth)
		.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
		.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
		.RevertTextOnEscape(RevertTextOnEscape)
		.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
		.SelectAllTextOnCommit(SelectAllTextOnCommit)
		.AllowContextMenu(AllowContextMenu)
		.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
		.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
		.VirtualKeyboardType(EVirtualKeyboardType::AsKeyboardType(KeyboardType.GetValue()));

	return MyEditableTextBlock.ToSharedRef();
}

void UEditableTextBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = PROPERTY_BINDING(FText, Text);
	TAttribute<FText> HintTextBinding = PROPERTY_BINDING(FText, HintText);

	MyEditableTextBlock->SetStyle(&WidgetStyle);
	MyEditableTextBlock->SetText(TextBinding);
	MyEditableTextBlock->SetHintText(HintTextBinding);
	MyEditableTextBlock->SetIsReadOnly(IsReadOnly);
	MyEditableTextBlock->SetIsPassword(IsPassword);
	MyEditableTextBlock->SetMinimumDesiredWidth(MinimumDesiredWidth);
	MyEditableTextBlock->SetIsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus);
	MyEditableTextBlock->SetSelectAllTextWhenFocused(SelectAllTextWhenFocused);
	MyEditableTextBlock->SetRevertTextOnEscape(RevertTextOnEscape);
	MyEditableTextBlock->SetClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit);
	MyEditableTextBlock->SetSelectAllTextOnCommit(SelectAllTextOnCommit);
	MyEditableTextBlock->SetAllowContextMenu(AllowContextMenu);

	ShapedTextOptions.SynchronizeShapedTextProperties(*MyEditableTextBlock);
}

FText UEditableTextBox::GetText() const
{
	if ( MyEditableTextBlock.IsValid() )
	{
		return MyEditableTextBlock->GetText();
	}

	return Text;
}

void UEditableTextBox::SetText(FText InText)
{
	Text = InText;
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetText(Text);
	}
}

void UEditableTextBox::SetHintText(FText InText)
{
	HintText = InText;
	if (MyEditableTextBlock.IsValid())
	{
		MyEditableTextBlock->SetHintText(HintText);
	}
}

void UEditableTextBox::SetError(FText InError)
{
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetError(InError);
	}
}

void UEditableTextBox::SetIsReadOnly(bool bReadOnly)
{
	IsReadOnly = bReadOnly;
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetIsReadOnly(IsReadOnly);
	}
}

void UEditableTextBox::ClearError()
{
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetError(FText::GetEmpty());
	}
}

bool UEditableTextBox::HasError() const
{
	if ( MyEditableTextBlock.IsValid() )
	{
		return MyEditableTextBlock->HasError();
	}

	return false;
}

void UEditableTextBox::HandleOnTextChanged(const FText& InText)
{
	Text = InText;
	OnTextChanged.Broadcast(InText);
}

void UEditableTextBox::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	Text = InText;
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

void UEditableTextBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FEditableTextBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FEditableTextBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES)
	{
		if (Font_DEPRECATED.HasValidFont())
		{
			WidgetStyle.Font = Font_DEPRECATED;
			Font_DEPRECATED = FSlateFontInfo();
		}

		WidgetStyle.Padding = Padding_DEPRECATED;
		Padding_DEPRECATED = FMargin(0);

		if (ForegroundColor_DEPRECATED != FLinearColor::Black)
		{
			WidgetStyle.ForegroundColor = ForegroundColor_DEPRECATED;
			ForegroundColor_DEPRECATED = FLinearColor::Black;
		}

		if (BackgroundColor_DEPRECATED != FLinearColor::White)
		{
			WidgetStyle.BackgroundColor = BackgroundColor_DEPRECATED;
			BackgroundColor_DEPRECATED = FLinearColor::White;
		}

		if (ReadOnlyForegroundColor_DEPRECATED != FLinearColor::Black)
		{
			WidgetStyle.ReadOnlyForegroundColor = ReadOnlyForegroundColor_DEPRECATED;
			ReadOnlyForegroundColor_DEPRECATED = FLinearColor::Black;
		}
	}
}

#if WITH_EDITOR

const FText UEditableTextBox::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
