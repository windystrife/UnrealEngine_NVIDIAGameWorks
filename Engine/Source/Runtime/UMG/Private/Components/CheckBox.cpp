// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/CheckBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UCheckBox

UCheckBox::UCheckBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SCheckBox::FArguments SlateDefaults;
	WidgetStyle = *SlateDefaults._Style;

	CheckedState = ECheckBoxState::Unchecked;

	HorizontalAlignment = SlateDefaults._HAlign;
	Padding_DEPRECATED = SlateDefaults._Padding.Get();

	BorderBackgroundColor_DEPRECATED = FLinearColor::White;

	IsFocusable = true;

}

void UCheckBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyCheckbox.Reset();
}

TSharedRef<SWidget> UCheckBox::RebuildWidget()
{
	MyCheckbox = SNew(SCheckBox)
		.OnCheckStateChanged( BIND_UOBJECT_DELEGATE(FOnCheckStateChanged, SlateOnCheckStateChangedCallback) )
		.Style(&WidgetStyle)
		.HAlign( HorizontalAlignment )
		.IsFocusable(IsFocusable)
		;

	if ( GetChildrenCount() > 0 )
	{
		MyCheckbox->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
	}
	
	return MyCheckbox.ToSharedRef();
}

void UCheckBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyCheckbox->SetStyle(&WidgetStyle);
	MyCheckbox->SetIsChecked( PROPERTY_BINDING(ECheckBoxState, CheckedState) );
}

void UCheckBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetContent(InSlot->Content ? InSlot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UCheckBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetContent(SNullWidget::NullWidget);
	}
}

bool UCheckBox::IsPressed() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->IsPressed();
	}

	return false;
}

bool UCheckBox::IsChecked() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->IsChecked();
	}

	return ( CheckedState == ECheckBoxState::Checked );
}

ECheckBoxState UCheckBox::GetCheckedState() const
{
	if ( MyCheckbox.IsValid() )
	{
		return MyCheckbox->GetCheckedState();
	}

	return CheckedState;
}

void UCheckBox::SetIsChecked(bool InIsChecked)
{
	CheckedState = InIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetIsChecked(PROPERTY_BINDING(ECheckBoxState, CheckedState));
	}
}

void UCheckBox::SetCheckedState(ECheckBoxState InCheckedState)
{
	CheckedState = InCheckedState;
	if ( MyCheckbox.IsValid() )
	{
		MyCheckbox->SetIsChecked(PROPERTY_BINDING(ECheckBoxState, CheckedState));
	}
}

void UCheckBox::SlateOnCheckStateChangedCallback(ECheckBoxState NewState)
{
	CheckedState = NewState;

	//@TODO: Choosing to treat Undetermined as Checked
	const bool bWantsToBeChecked = NewState != ECheckBoxState::Unchecked;
	OnCheckStateChanged.Broadcast(bWantsToBeChecked);
}

void UCheckBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FCheckBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FCheckBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if ( UncheckedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UncheckedImage = UncheckedImage_DEPRECATED->Brush;
			UncheckedImage_DEPRECATED = nullptr;
		}

		if ( UncheckedHoveredImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UncheckedHoveredImage = UncheckedHoveredImage_DEPRECATED->Brush;
			UncheckedHoveredImage_DEPRECATED = nullptr;
		}

		if ( UncheckedPressedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UncheckedPressedImage = UncheckedPressedImage_DEPRECATED->Brush;
			UncheckedPressedImage_DEPRECATED = nullptr;
		}

		if ( CheckedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.CheckedImage = CheckedImage_DEPRECATED->Brush;
			CheckedImage_DEPRECATED = nullptr;
		}

		if ( CheckedHoveredImage_DEPRECATED != nullptr )
		{
			WidgetStyle.CheckedHoveredImage = CheckedHoveredImage_DEPRECATED->Brush;
			CheckedHoveredImage_DEPRECATED = nullptr;
		}

		if ( CheckedPressedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.CheckedPressedImage = CheckedPressedImage_DEPRECATED->Brush;
			CheckedPressedImage_DEPRECATED = nullptr;
		}

		if ( UndeterminedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UndeterminedImage = UndeterminedImage_DEPRECATED->Brush;
			UndeterminedImage_DEPRECATED = nullptr;
		}

		if ( UndeterminedHoveredImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UndeterminedHoveredImage = UndeterminedHoveredImage_DEPRECATED->Brush;
			UndeterminedHoveredImage_DEPRECATED = nullptr;
		}

		if ( UndeterminedPressedImage_DEPRECATED != nullptr )
		{
			WidgetStyle.UndeterminedPressedImage = UndeterminedPressedImage_DEPRECATED->Brush;
			UndeterminedPressedImage_DEPRECATED = nullptr;
		}
	}

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES)
	{
		WidgetStyle.Padding = Padding_DEPRECATED;
		Padding_DEPRECATED = FMargin(0);

		if (BorderBackgroundColor_DEPRECATED != FLinearColor::White)
		{
			WidgetStyle.BorderBackgroundColor = BorderBackgroundColor_DEPRECATED;
			BorderBackgroundColor_DEPRECATED = FLinearColor::White;
		}
	}
}

#if WITH_EDITOR

const FText UCheckBox::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
