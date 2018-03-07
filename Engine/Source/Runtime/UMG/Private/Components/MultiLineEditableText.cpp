// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/MultiLineEditableText.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/SMultiLineEditableText.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UMultiLineEditableText

UMultiLineEditableText::UMultiLineEditableText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SMultiLineEditableText::FArguments Defaults;
	WidgetStyle = *Defaults._TextStyle;
	bIsReadOnly = Defaults._IsReadOnly.Get();
	AllowContextMenu = Defaults._AllowContextMenu.Get();
	Clipping = Defaults._Clipping;
	AutoWrapText = true;
	
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font_DEPRECATED = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));

		WidgetStyle.SetFont(Font_DEPRECATED);
	}
}

void UMultiLineEditableText::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMultiLineEditableText.Reset();
}

TSharedRef<SWidget> UMultiLineEditableText::RebuildWidget()
{
	MyMultiLineEditableText = SNew(SMultiLineEditableText)
	.TextStyle(&WidgetStyle)
	.AllowContextMenu(AllowContextMenu)
	.IsReadOnly(bIsReadOnly)
//	.MinDesiredWidth(MinimumDesiredWidth)
//	.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
//	.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
//	.RevertTextOnEscape(RevertTextOnEscape)
//	.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
//	.SelectAllTextOnCommit(SelectAllTextOnCommit)
//	.BackgroundImageSelected(BackgroundImageSelected ? TAttribute<const FSlateBrush*>(&BackgroundImageSelected->Brush) : TAttribute<const FSlateBrush*>())
//	.BackgroundImageSelectionTarget(BackgroundImageSelectionTarget ? TAttribute<const FSlateBrush*>(&BackgroundImageSelectionTarget->Brush) : TAttribute<const FSlateBrush*>())
//	.BackgroundImageComposing(BackgroundImageComposing ? TAttribute<const FSlateBrush*>(&BackgroundImageComposing->Brush) : TAttribute<const FSlateBrush*>())
//	.CaretImage(CaretImage ? TAttribute<const FSlateBrush*>(&CaretImage->Brush) : TAttribute<const FSlateBrush*>())
	.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
	.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
	;
	
	return MyMultiLineEditableText.ToSharedRef();
}

void UMultiLineEditableText::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> HintTextBinding = PROPERTY_BINDING(FText, HintText);

	MyMultiLineEditableText->SetTextStyle(&WidgetStyle);
	MyMultiLineEditableText->SetText(Text);
	MyMultiLineEditableText->SetHintText(HintTextBinding);
	MyMultiLineEditableText->SetAllowContextMenu(AllowContextMenu);
	MyMultiLineEditableText->SetIsReadOnly(bIsReadOnly);

//	MyMultiLineEditableText->SetIsPassword(IsPassword);
//	MyMultiLineEditableText->SetColorAndOpacity(ColorAndOpacity);

	// TODO UMG Complete making all properties settable on SMultiLineEditableText

	Super::SynchronizeTextLayoutProperties(*MyMultiLineEditableText);
}

FText UMultiLineEditableText::GetText() const
{
	if ( MyMultiLineEditableText.IsValid() )
	{
		return MyMultiLineEditableText->GetText();
	}

	return Text;
}

void UMultiLineEditableText::SetText(FText InText)
{
	Text = InText;
	if ( MyMultiLineEditableText.IsValid() )
	{
		MyMultiLineEditableText->SetText(Text);
	}
}

void UMultiLineEditableText::SetIsReadOnly(bool bReadOnly)
{
	bIsReadOnly = bReadOnly;

	if ( MyMultiLineEditableText.IsValid() )
	{
		MyMultiLineEditableText->SetIsReadOnly(bIsReadOnly);
	}
}

void UMultiLineEditableText::HandleOnTextChanged(const FText& InText)
{
	OnTextChanged.Broadcast(InText);
}

void UMultiLineEditableText::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

void UMultiLineEditableText::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES)
	{
		if (Font_DEPRECATED.HasValidFont())
		{
			WidgetStyle.Font = Font_DEPRECATED;
			Font_DEPRECATED = FSlateFontInfo();
		}
	}
}

#if WITH_EDITOR

const FText UMultiLineEditableText::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
