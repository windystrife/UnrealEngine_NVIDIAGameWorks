// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/InputKeySelector.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SInputKeySelector.h"

UInputKeySelector::UInputKeySelector( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SInputKeySelector::FArguments InputKeySelectorDefaults;
	WidgetStyle = *InputKeySelectorDefaults._ButtonStyle;
	TextStyle = *InputKeySelectorDefaults._TextStyle;
	KeySelectionText = InputKeySelectorDefaults._KeySelectionText;
	NoKeySpecifiedText = InputKeySelectorDefaults._NoKeySpecifiedText;
	SelectedKey = InputKeySelectorDefaults._SelectedKey.Get();
	bAllowModifierKeys = InputKeySelectorDefaults._AllowModifierKeys;
	bAllowGamepadKeys = InputKeySelectorDefaults._AllowGamepadKeys;

	EscapeKeys.AddUnique(EKeys::Gamepad_Special_Right); // In most (if not all) cases this is going to be the menu button

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		TextStyle.Font = FSlateFontInfo(RobotoFontObj.Object, 24, FName("Bold"));
	}
}

void UInputKeySelector::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
}

void UInputKeySelector::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::InputKeySelectorTextStyle)
	{
		TextStyle.Font = Font_DEPRECATED;
		TextStyle.ColorAndOpacity = ColorAndOpacity_DEPRECATED;
	}
}

void UInputKeySelector::SetSelectedKey( const FInputChord& InSelectedKey )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetSelectedKey( InSelectedKey );
	}
	SelectedKey = InSelectedKey;
}

void UInputKeySelector::SetKeySelectionText( FText InKeySelectionText )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetKeySelectionText( InKeySelectionText );
	}
	KeySelectionText = MoveTemp(InKeySelectionText);
}

void UInputKeySelector::SetNoKeySpecifiedText(FText InNoKeySpecifiedText)
{
	if (MyInputKeySelector.IsValid())
	{
		MyInputKeySelector->SetNoKeySpecifiedText(InNoKeySpecifiedText);
	}
	NoKeySpecifiedText = MoveTemp(InNoKeySpecifiedText);
}

void UInputKeySelector::SetAllowModifierKeys( const bool bInAllowModifierKeys )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetAllowModifierKeys( bInAllowModifierKeys );
	}
	bAllowModifierKeys = bInAllowModifierKeys;
}

void UInputKeySelector::SetAllowGamepadKeys(const bool bInAllowGamepadKeys)
{
	if (MyInputKeySelector.IsValid())
	{
		MyInputKeySelector->SetAllowGamepadKeys(bInAllowGamepadKeys);
	}
	bAllowGamepadKeys = bInAllowGamepadKeys;
}

bool UInputKeySelector::GetIsSelectingKey() const
{
	return MyInputKeySelector.IsValid() ? MyInputKeySelector->GetIsSelectingKey() : false;
}

void UInputKeySelector::SetButtonStyle( const FButtonStyle* InButtonStyle )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetButtonStyle(InButtonStyle);
	}
	WidgetStyle = *InButtonStyle;
}

void UInputKeySelector::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyInputKeySelector->SetSelectedKey( SelectedKey );
	MyInputKeySelector->SetMargin( Margin );
	MyInputKeySelector->SetButtonStyle( &WidgetStyle );
	MyInputKeySelector->SetTextStyle( &TextStyle );
	MyInputKeySelector->SetKeySelectionText( KeySelectionText );
	MyInputKeySelector->SetAllowModifierKeys( bAllowModifierKeys );
	MyInputKeySelector->SetAllowGamepadKeys(bAllowGamepadKeys);
	MyInputKeySelector->SetEscapeKeys(EscapeKeys);
}

void UInputKeySelector::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyInputKeySelector.Reset();
}

TSharedRef<SWidget> UInputKeySelector::RebuildWidget()
{
	MyInputKeySelector = SNew(SInputKeySelector)
		.SelectedKey(SelectedKey)
		.Margin(Margin)
		.ButtonStyle(&WidgetStyle)
		.TextStyle(&TextStyle)
		.KeySelectionText(KeySelectionText)
		.AllowModifierKeys(bAllowModifierKeys)
		.AllowGamepadKeys(bAllowGamepadKeys)
		.EscapeKeys(EscapeKeys)
		.OnKeySelected( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnKeySelected, HandleKeySelected ) )
		.OnIsSelectingKeyChanged( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnIsSelectingKeyChanged, HandleIsSelectingKeyChanged ) );
	return MyInputKeySelector.ToSharedRef();
}

void UInputKeySelector::HandleKeySelected(const FInputChord& InSelectedKey)
{
	SelectedKey = InSelectedKey;
	OnKeySelected.Broadcast(SelectedKey);
}

void UInputKeySelector::HandleIsSelectingKeyChanged()
{
	OnIsSelectingKeyChanged.Broadcast();
}

void UInputKeySelector::SetTextBlockVisibility(const ESlateVisibility InVisibility)
{
	if (MyInputKeySelector.IsValid())
	{
		EVisibility SlateVisibility = UWidget::ConvertSerializedVisibilityToRuntime(InVisibility);
		MyInputKeySelector->SetTextBlockVisibility(SlateVisibility);
	}
}
