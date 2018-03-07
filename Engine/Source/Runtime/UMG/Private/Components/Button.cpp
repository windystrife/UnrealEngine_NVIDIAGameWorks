// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/Button.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Components/ButtonSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UButton

UButton::UButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SButton::FArguments ButtonDefaults;
	WidgetStyle = *ButtonDefaults._ButtonStyle;

	ColorAndOpacity = FLinearColor::White;
	BackgroundColor = FLinearColor::White;

	ClickMethod = EButtonClickMethod::DownAndUp;
	TouchMethod = EButtonTouchMethod::DownAndUp;

	IsFocusable = true;
}

void UButton::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyButton.Reset();
}

TSharedRef<SWidget> UButton::RebuildWidget()
{
	MyButton = SNew(SButton)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
		.OnHovered_UObject( this, &ThisClass::SlateHandleHovered )
		.OnUnhovered_UObject( this, &ThisClass::SlateHandleUnhovered )
		.ButtonStyle(&WidgetStyle)
		.ClickMethod(ClickMethod)
		.TouchMethod(TouchMethod)
		.IsFocusable(IsFocusable)
		;

	if ( GetChildrenCount() > 0 )
	{
		Cast<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
	}
	
	return MyButton.ToSharedRef();
}

void UButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyButton->SetColorAndOpacity( ColorAndOpacity );
	MyButton->SetBorderBackgroundColor( BackgroundColor );
}

UClass* UButton::GetSlotClass() const
{
	return UButtonSlot::StaticClass();
}

void UButton::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyButton.IsValid() )
	{
		CastChecked<UButtonSlot>(InSlot)->BuildSlot(MyButton.ToSharedRef());
	}
}

void UButton::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyButton.IsValid() )
	{
		MyButton->SetContent(SNullWidget::NullWidget);
	}
}

void UButton::SetStyle(const FButtonStyle& InStyle)
{
	WidgetStyle = InStyle;
	if ( MyButton.IsValid() )
	{
		MyButton->SetButtonStyle(&WidgetStyle);
	}
}

void UButton::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;
	if ( MyButton.IsValid() )
	{
		MyButton->SetColorAndOpacity(InColorAndOpacity);
	}
}

void UButton::SetBackgroundColor(FLinearColor InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
	if ( MyButton.IsValid() )
	{
		MyButton->SetBorderBackgroundColor(InBackgroundColor);
	}
}

bool UButton::IsPressed() const
{
	if ( MyButton.IsValid() )
	{
		return MyButton->IsPressed();
	}

	return false;
}

void UButton::SetClickMethod(EButtonClickMethod::Type InClickMethod)
{
	ClickMethod = InClickMethod;
	if ( MyButton.IsValid() )
	{
		MyButton->SetClickMethod(ClickMethod);
	}
}

void UButton::SetTouchMethod(EButtonTouchMethod::Type InTouchMethod)
{
	TouchMethod = InTouchMethod;
	if ( MyButton.IsValid() )
	{
		MyButton->SetTouchMethod(TouchMethod);
	}
}

//void UButton::SetPressMethod(EButtonPressMethod::Type InPressMethod)
//{
//	PressMethod = InPressMethod;
//	if ( MyButton.IsValid() )
//	{
//		MyButton->SetPressMethod(PressMethod);
//	}
//}

void UButton::PostLoad()
{
	Super::PostLoad();

	if ( GetChildrenCount() > 0 )
	{
		//TODO UMG Pre-Release Upgrade, now buttons have slots of their own.  Convert existing slot to new slot.
		if ( UPanelSlot* PanelSlot = GetContentSlot() )
		{
			UButtonSlot* ButtonSlot = Cast<UButtonSlot>(PanelSlot);
			if ( ButtonSlot == NULL )
			{
				ButtonSlot = NewObject<UButtonSlot>(this);
				ButtonSlot->Content = GetContentSlot()->Content;
				ButtonSlot->Content->Slot = ButtonSlot;
				Slots[0] = ButtonSlot;
			}
		}
	}

	if( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS && Style_DEPRECATED != nullptr )
	{
		const FButtonStyle* StylePtr = Style_DEPRECATED->GetStyle<FButtonStyle>();
		if(StylePtr != nullptr)
		{
			WidgetStyle = *StylePtr;
		}

		Style_DEPRECATED = nullptr;
	}
}

FReply UButton::SlateHandleClicked()
{
	OnClicked.Broadcast();

	return FReply::Handled();
}

void UButton::SlateHandlePressed()
{
	OnPressed.Broadcast();
}

void UButton::SlateHandleReleased()
{
	OnReleased.Broadcast();
}

void UButton::SlateHandleHovered()
{
	OnHovered.Broadcast();
}

void UButton::SlateHandleUnhovered()
{
	OnUnhovered.Broadcast();
}

#if WITH_EDITOR

const FText UButton::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
