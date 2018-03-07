// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/EditableText.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SEditableText.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UEditableText

UEditableText::UEditableText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SEditableText::FArguments Defaults;
	WidgetStyle = *Defaults._Style;

	ColorAndOpacity_DEPRECATED = FLinearColor::Black;

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font_DEPRECATED = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
	}

	// Grab other defaults from slate arguments.
	IsReadOnly = Defaults._IsReadOnly.Get();
	IsPassword = Defaults._IsPassword.Get();
	MinimumDesiredWidth = Defaults._MinDesiredWidth.Get();
	IsCaretMovedWhenGainFocus = Defaults._IsCaretMovedWhenGainFocus.Get();
	SelectAllTextWhenFocused = Defaults._SelectAllTextWhenFocused.Get();
	RevertTextOnEscape = Defaults._RevertTextOnEscape.Get();
	ClearKeyboardFocusOnCommit = Defaults._ClearKeyboardFocusOnCommit.Get();
	SelectAllTextOnCommit = Defaults._SelectAllTextOnCommit.Get();
	AllowContextMenu = Defaults._AllowContextMenu.Get();
	Clipping = Defaults._Clipping;
}

void UEditableText::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyEditableText.Reset();
}

TSharedRef<SWidget> UEditableText::RebuildWidget()
{
	MyEditableText = SNew( SEditableText )
		.Style( &WidgetStyle )
		.MinDesiredWidth( MinimumDesiredWidth )
		.IsCaretMovedWhenGainFocus( IsCaretMovedWhenGainFocus )
		.SelectAllTextWhenFocused( SelectAllTextWhenFocused )
		.RevertTextOnEscape( RevertTextOnEscape )
		.ClearKeyboardFocusOnCommit( ClearKeyboardFocusOnCommit )
		.SelectAllTextOnCommit( SelectAllTextOnCommit )
		.OnTextChanged( BIND_UOBJECT_DELEGATE( FOnTextChanged, HandleOnTextChanged ) )
		.OnTextCommitted( BIND_UOBJECT_DELEGATE( FOnTextCommitted, HandleOnTextCommitted ) )
		.VirtualKeyboardType( EVirtualKeyboardType::AsKeyboardType( KeyboardType.GetValue() ) );
	
	return MyEditableText.ToSharedRef();
}

void UEditableText::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = PROPERTY_BINDING(FText, Text);
	TAttribute<FText> HintTextBinding = PROPERTY_BINDING(FText, HintText);

	MyEditableText->SetText(TextBinding);
	MyEditableText->SetHintText(HintTextBinding);
	MyEditableText->SetIsReadOnly(IsReadOnly);
	MyEditableText->SetIsPassword(IsPassword);
	MyEditableText->SetAllowContextMenu(AllowContextMenu);
	// TODO UMG Complete making all properties settable on SEditableText

	ShapedTextOptions.SynchronizeShapedTextProperties(*MyEditableText);
}

FText UEditableText::GetText() const
{
	if ( MyEditableText.IsValid() )
	{
		return MyEditableText->GetText();
	}

	return Text;
}

void UEditableText::SetText(FText InText)
{
	Text = InText;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetText(Text);
	}
}

void UEditableText::SetIsPassword(bool InbIsPassword)
{
	IsPassword = InbIsPassword;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetIsPassword(IsPassword);
	}
}

void UEditableText::SetHintText(FText InHintText)
{
	HintText = InHintText;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetHintText(HintText);
	}
}

void UEditableText::SetIsReadOnly(bool InbIsReadyOnly)
{
	IsReadOnly = InbIsReadyOnly;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetIsReadOnly(IsReadOnly);
	}
}

void UEditableText::HandleOnTextChanged(const FText& InText)
{
	OnTextChanged.Broadcast(InText);
}

void UEditableText::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

void UEditableText::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FEditableTextStyle* StylePtr = Style_DEPRECATED->GetStyle<FEditableTextStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if ( BackgroundImageSelected_DEPRECATED != nullptr )
		{
			WidgetStyle.BackgroundImageSelected = BackgroundImageSelected_DEPRECATED->Brush;
			BackgroundImageSelected_DEPRECATED = nullptr;
		}

		if ( BackgroundImageComposing_DEPRECATED != nullptr )
		{
			WidgetStyle.BackgroundImageComposing = BackgroundImageComposing_DEPRECATED->Brush;
			BackgroundImageComposing_DEPRECATED = nullptr;
		}

		if ( CaretImage_DEPRECATED != nullptr )
		{
			WidgetStyle.CaretImage = CaretImage_DEPRECATED->Brush;
			CaretImage_DEPRECATED = nullptr;
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES)
	{
		if (Font_DEPRECATED.HasValidFont())
		{
			WidgetStyle.Font = Font_DEPRECATED;
			Font_DEPRECATED = FSlateFontInfo();
		}

		if (ColorAndOpacity_DEPRECATED != FLinearColor::Black)
		{
			WidgetStyle.ColorAndOpacity = ColorAndOpacity_DEPRECATED;
			ColorAndOpacity_DEPRECATED = FLinearColor::Black;
		}
	}
}

#if WITH_EDITOR

const FText UEditableText::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
