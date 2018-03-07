// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Visibility.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "UnrealString.h"

/**
 * Implementation for a box that only accepts a numeric value or that can display an undetermined value via a string
 * Supports an optional spin box for manipulating a value by dragging with the mouse
 * Supports an optional label inset in the text box
 */ 
template<typename NumericType>
class SNumericEntryBox : public SCompoundWidget
{
public: 

	static const FLinearColor RedLabelBackgroundColor;
	static const FLinearColor GreenLabelBackgroundColor;
	static const FLinearColor BlueLabelBackgroundColor;
	static const FText DefaultUndeterminedString;

	/** Notification for numeric value change */
	DECLARE_DELEGATE_OneParam(FOnValueChanged, NumericType /*NewValue*/);

	/** Notification for numeric value committed */
	DECLARE_DELEGATE_TwoParams(FOnValueCommitted, NumericType /*NewValue*/, ETextCommit::Type /*CommitType*/);

	/** Notification for change of undetermined values */
	DECLARE_DELEGATE_OneParam(FOnUndeterminedValueChanged, FText /*NewValue*/ );

	/** Notification for committing undetermined values */
	DECLARE_DELEGATE_TwoParams(FOnUndeterminedValueCommitted, FText /*NewValue*/, ETextCommit::Type /*CommitType*/);

	/** Notification when the max/min spinner values are changed (only apply if SupportDynamicSliderMaxValue or SupportDynamicSliderMinValue are true) */
	DECLARE_DELEGATE_FourParams(FOnDynamicSliderMinMaxValueChanged, NumericType, TWeakPtr<SWidget>, bool, bool);

public:

	SLATE_BEGIN_ARGS( SNumericEntryBox<NumericType> )
		: _EditableTextBoxStyle( &FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox") )
		, _SpinBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FSpinBoxStyle>("NumericEntrySpinBox") )
		, _Label()
		, _LabelVAlign( VAlign_Fill )
		, _LabelPadding( FMargin(3,0) )
		, _BorderForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _BorderBackgroundColor(FLinearColor::White)
		, _UndeterminedString( SNumericEntryBox<NumericType>::DefaultUndeterminedString )
		, _AllowSpin(false)
		, _ShiftMouseMovePixelPerDelta(1)
		, _SupportDynamicSliderMaxValue(false)
		, _SupportDynamicSliderMinValue(false)
		, _Delta(0)
		, _MinValue(TNumericLimits<NumericType>::Lowest())
		, _MaxValue(TNumericLimits<NumericType>::Max())
		, _MinSliderValue(0)				
		, _MaxSliderValue(100)
		, _SliderExponent(1.f)
		, _MinDesiredValueWidth(0)		
	{}		

		/** Style to use for the editable text box within this widget */
		SLATE_STYLE_ARGUMENT( FEditableTextBoxStyle, EditableTextBoxStyle )

		/** Style to use for the spin box within this widget */
		SLATE_STYLE_ARGUMENT( FSpinBoxStyle, SpinBoxStyle )

		/** Slot for this button's content (optional) */
		SLATE_NAMED_SLOT( FArguments, Label )
		/** Vertical alignment of the label content */
		SLATE_ARGUMENT( EVerticalAlignment, LabelVAlign )
		/** Padding around the label content */
		SLATE_ARGUMENT( FMargin, LabelPadding )
		/** Border Foreground Color */
		SLATE_ARGUMENT( FSlateColor, BorderForegroundColor )
		/** Border Background Color */
		SLATE_ARGUMENT( FSlateColor, BorderBackgroundColor )
		/** The value that should be displayed.  This value is optional in the case where a value cannot be determined */
		SLATE_ATTRIBUTE( TOptional<NumericType>, Value )
		/** The string to display if the value cannot be determined */
		SLATE_ARGUMENT( FText, UndeterminedString )
		/** Font color and opacity */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		/** Whether or not the user should be able to change the value by dragging with the mouse cursor */
		SLATE_ARGUMENT( bool, AllowSpin )
		/** How many pixel the mouse must move to change the value of the delta step (only use if there is a spinbox allow) */
		SLATE_ARGUMENT( int32, ShiftMouseMovePixelPerDelta )
		/** Tell us if we want to support dynamically changing of the max value using ctrl  (only use if there is a spinbox allow) */
		SLATE_ATTRIBUTE(bool, SupportDynamicSliderMaxValue)
		/** Tell us if we want to support dynamically changing of the min value using ctrl  (only use if there is a spinbox allow) */
		SLATE_ATTRIBUTE(bool, SupportDynamicSliderMinValue)
		/** Called right after the spinner max value is changed (only relevant if SupportDynamicSliderMaxValue is true) */
		SLATE_EVENT(FOnDynamicSliderMinMaxValueChanged, OnDynamicSliderMaxValueChanged)
		/** Called right after the spinner min value is changed (only relevant if SupportDynamicSliderMinValue is true) */
		SLATE_EVENT(FOnDynamicSliderMinMaxValueChanged, OnDynamicSliderMinValueChanged)
		/** Delta to increment the value as the slider moves.  If not specified will determine automatically */
		SLATE_ATTRIBUTE( NumericType, Delta )
		/** The minimum value that can be entered into the text edit box */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MinValue )
		/** The maximum value that can be entered into the text edit box */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MaxValue )
		/** The minimum value that can be specified by using the slider */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MinSliderValue )
		/** The maximum value that can be specified by using the slider */
		SLATE_ATTRIBUTE( TOptional< NumericType >, MaxSliderValue )
		/** Use exponential scale for the slider */
		SLATE_ATTRIBUTE( float, SliderExponent )
		/** When using exponential scale specify a neutral value where we want the maximum precision (by default it is the smallest slider value)*/
		SLATE_ATTRIBUTE(NumericType, SliderExponentNeutralValue )
		/** The minimum desired width for the value portion of the control. */
		SLATE_ATTRIBUTE( float, MinDesiredValueWidth )
		/** The text margin to use if overridden. */
		SLATE_ATTRIBUTE( FMargin, OverrideTextMargin )
		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnValueChanged, OnValueChanged )
		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnValueCommitted, OnValueCommitted )
		/** Called whenever the text is changed interactively by the user */
		SLATE_EVENT( FOnUndeterminedValueChanged, OnUndeterminedValueChanged )
		/** Called whenever the text is committed.  This happens when the user presses enter or the text box loses focus. */
		SLATE_EVENT( FOnUndeterminedValueCommitted, OnUndeterminedValueCommitted )
		/** Called right before the slider begins to move */
		SLATE_EVENT( FSimpleDelegate, OnBeginSliderMovement )
		/** Called right after the slider handle is released by the user */
		SLATE_EVENT( FOnValueChanged, OnEndSliderMovement )		
		/** Menu extender for right-click context menu */
		SLATE_EVENT( FMenuExtensionDelegate, ContextMenuExtender )
		/** Provide custom type conversion functionality to this spin box */
		SLATE_ARGUMENT( TSharedPtr< INumericTypeInterface<NumericType> >, TypeInterface )

	SLATE_END_ARGS()
	SNumericEntryBox()
	{
	}

	void Construct( const FArguments& InArgs )
	{
		check(InArgs._EditableTextBoxStyle);

		OnValueChanged = InArgs._OnValueChanged;
		OnValueCommitted = InArgs._OnValueCommitted;
		OnUndeterminedValueChanged = InArgs._OnUndeterminedValueChanged;
		OnUndeterminedValueCommitted = InArgs._OnUndeterminedValueCommitted;
		ValueAttribute = InArgs._Value;
		UndeterminedString = InArgs._UndeterminedString;
		MinDesiredValueWidth = InArgs._MinDesiredValueWidth;
		BorderImageNormal = &InArgs._EditableTextBoxStyle->BackgroundImageNormal;
		BorderImageHovered = &InArgs._EditableTextBoxStyle->BackgroundImageHovered;
		BorderImageFocused = &InArgs._EditableTextBoxStyle->BackgroundImageFocused;
		Interface = InArgs._TypeInterface.IsValid() ? InArgs._TypeInterface : MakeShareable( new TDefaultNumericTypeInterface<NumericType> );

		TAttribute<FMargin> TextMargin = InArgs._OverrideTextMargin.IsSet() ? InArgs._OverrideTextMargin : InArgs._EditableTextBoxStyle->Padding;
		const bool bAllowSpin = InArgs._AllowSpin;
		TSharedPtr<SWidget> FinalWidget;

		if( bAllowSpin )
		{
			SAssignNew( SpinBox, SSpinBox<NumericType> )
				.Style( InArgs._SpinBoxStyle )
				.Font( InArgs._Font.IsSet() ? InArgs._Font : InArgs._EditableTextBoxStyle->Font )
				.ContentPadding( TextMargin )
				.Value( this, &SNumericEntryBox<NumericType>::OnGetValueForSpinBox )
				.Delta( InArgs._Delta )
				.ShiftMouseMovePixelPerDelta(InArgs._ShiftMouseMovePixelPerDelta)
				.SupportDynamicSliderMaxValue(InArgs._SupportDynamicSliderMaxValue)
				.SupportDynamicSliderMinValue(InArgs._SupportDynamicSliderMinValue)
				.OnDynamicSliderMaxValueChanged(InArgs._OnDynamicSliderMaxValueChanged)
				.OnDynamicSliderMinValueChanged(InArgs._OnDynamicSliderMinValueChanged)
				.OnValueChanged( OnValueChanged )
				.OnValueCommitted( OnValueCommitted )
				.MinSliderValue(InArgs._MinSliderValue)
				.MaxSliderValue(InArgs._MaxSliderValue)
				.MaxValue(InArgs._MaxValue)
				.MinValue(InArgs._MinValue)
				.SliderExponent(InArgs._SliderExponent)
				.SliderExponentNeutralValue(InArgs._SliderExponentNeutralValue)
				.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
				.OnEndSliderMovement(InArgs._OnEndSliderMovement)
				.MinDesiredWidth(InArgs._MinDesiredValueWidth)
				.TypeInterface(Interface);
		}

		// Always create an editable text box.  In the case of an undetermined value being passed in, we cant use the spinbox.
		SAssignNew( EditableText, SEditableText )
			.Text( this, &SNumericEntryBox<NumericType>::OnGetValueForTextBox )
			.Visibility( bAllowSpin ? EVisibility::Collapsed : EVisibility::Visible )
			.Font( InArgs._Font.IsSet() ? InArgs._Font : InArgs._EditableTextBoxStyle->Font )
			.SelectAllTextWhenFocused( true )
			.ClearKeyboardFocusOnCommit( false )
			.OnTextChanged( this, &SNumericEntryBox<NumericType>::OnTextChanged  )
			.OnTextCommitted( this, &SNumericEntryBox<NumericType>::OnTextCommitted )
			.SelectAllTextOnCommit( true )
			.ContextMenuExtender( InArgs._ContextMenuExtender )
			.MinDesiredWidth(InArgs._MinDesiredValueWidth);

		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);
	
		if( InArgs._Label.Widget != SNullWidget::NullWidget )
		{
			HorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(InArgs._LabelVAlign)
				.Padding(InArgs._LabelPadding)
				[
					InArgs._Label.Widget
				];
		}

		// Add the spin box if we have one
		if( bAllowSpin )
		{
			HorizontalBox->AddSlot()
				.HAlign(HAlign_Fill) 
				.VAlign(VAlign_Center) 
				.FillWidth(1)
				[
					SpinBox.ToSharedRef()
				];
		}

		HorizontalBox->AddSlot()
			.HAlign(HAlign_Fill) 
			.VAlign(VAlign_Center)
			.Padding(TextMargin)
			.FillWidth(1)
			[
				EditableText.ToSharedRef()
			];

		ChildSlot
			[
				SNew( SBorder )
				.BorderImage( this, &SNumericEntryBox<NumericType>::GetBorderImage )
				.BorderBackgroundColor( InArgs._BorderBackgroundColor )
				.ForegroundColor( InArgs._BorderForegroundColor )
				.Padding(0)
				[
					HorizontalBox
				]
			];
	}

	static TSharedRef<SWidget> BuildLabel(TAttribute<FText> LabelText, const FSlateColor& ForegroundColor, const FSlateColor& BackgroundColor)
	{
		return
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NumericEntrySpinBox.Decorator"))
			.BorderBackgroundColor(BackgroundColor)
			.ForegroundColor(ForegroundColor)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(FMargin(1, 0, 6, 0))
			[
				SNew(STextBlock)
				.Text(LabelText)
			];
	}

	/** Return the internally created SpinBox if bAllowSpin is true */
	TSharedPtr<SWidget> GetSpinBox() const { return SpinBox; }

private:

	//~ SWidget Interface

	virtual bool SupportsKeyboardFocus() const override
	{
		return StaticCastSharedPtr<SWidget>(EditableText)->SupportsKeyboardFocus();
	}

	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override
	{
		FReply Reply = FReply::Handled();

		// The widget to forward focus to changes depending on whether we have a SpinBox or not.
		TSharedPtr<SWidget> FocusWidget;
		if (SpinBox.IsValid() && SpinBox->GetVisibility() == EVisibility::Visible) 
		{
			FocusWidget = SpinBox;
		}
		else
		{
			FocusWidget = EditableText;
		}

		if ( InFocusEvent.GetCause() != EFocusCause::Cleared )
		{
			// Forward keyboard focus to our chosen widget
			Reply.SetUserFocus(FocusWidget.ToSharedRef(), InFocusEvent.GetCause());
		}

		return Reply;
	}

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		FKey Key = InKeyEvent.GetKey();

		if( Key == EKeys::Escape && EditableText->HasKeyboardFocus() )
		{
			return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Cleared);
		}

		return FReply::Unhandled();
	}

private:

	/**
	 * @return the Label that should be displayed                   
	 */
	FString GetLabel() const
	{
		// Should always be set if this is being called
		return LabelAttribute.Get().GetValue();
	}

	/**
	 * Called to get the value for the spin box                   
	 */
	NumericType OnGetValueForSpinBox() const
	{
		const auto& Value = ValueAttribute.Get();

		// Get the value or 0 if its not set
		if( Value.IsSet() == true )
		{
			return Value.GetValue();
		}

		return 0;
	}

	/**
	 * Called to get the value for the text box as FText                 
	 */
	FText OnGetValueForTextBox() const
	{
		FText NewText = FText::GetEmpty();

		if( EditableText->GetVisibility() == EVisibility::Visible )
		{
			const auto& Value = ValueAttribute.Get();

			// If the value was set convert it to a string, otherwise the value cannot be determined
			if( Value.IsSet() == true )
			{
				NewText = FText::FromString(Interface->ToString(Value.GetValue()));
			}
			else
			{
				NewText = UndeterminedString;
			}
		}

		// The box isnt visible, just return an empty string
		return NewText;
	}


	/**
	 * Called when the text changes in the text box                   
	 */
	void OnTextChanged( const FText& NewValue )
	{
		const auto& Value = ValueAttribute.Get();

		if (Value.IsSet() || !OnUndeterminedValueChanged.IsBound())
		{
			SendChangesFromText( NewValue, false, ETextCommit::Default );
		}
		else
		{
			OnUndeterminedValueChanged.Execute(NewValue);
		}
	}

	/**
	 * Called when the text is committed from the text box                   
	 */
	void OnTextCommitted( const FText& NewValue, ETextCommit::Type CommitInfo )
	{
		const auto& Value = ValueAttribute.Get();

		if (Value.IsSet() || !OnUndeterminedValueCommitted.IsBound())
		{
			SendChangesFromText( NewValue, true, CommitInfo );
		}
		else
		{
			OnUndeterminedValueCommitted.Execute(NewValue, CommitInfo);
		}
	}

	/**
	 * Called to get the border image of the box                   
	 */
	const FSlateBrush* GetBorderImage() const
	{
		TSharedPtr<const SWidget> EditingWidget;

		if (SpinBox.IsValid() && SpinBox->GetVisibility() == EVisibility::Visible) 
		{
			EditingWidget = SpinBox;
		}
		else
		{
			EditingWidget = EditableText;
		}

		if ( EditingWidget->HasKeyboardFocus() )
		{
			return BorderImageFocused;
		}

		if ( EditingWidget->IsHovered() )
		{
			return BorderImageHovered;
		}

		return BorderImageNormal;
	}

	/**
	 * Calls the value commit or changed delegate set for this box when the value is set from a string
	 *
	 * @param NewValue	The new value as a string
	 * @param bCommit	Whether or not to call the commit or changed delegate
	 */
	void SendChangesFromText( const FText& NewValue, bool bCommit, ETextCommit::Type CommitInfo )
	{
		if (NewValue.IsEmpty())
		{
			return;
		}

		// Only call the delegates if we have a valid numeric value
		if (bCommit)
		{
			TOptional<NumericType> ExistingValue = ValueAttribute.Get();
			TOptional<NumericType> NumericValue = Interface->FromString(NewValue.ToString(), ExistingValue.Get(0));

			if (NumericValue.IsSet())
			{
				OnValueCommitted.ExecuteIfBound(NumericValue.GetValue(), CommitInfo );
			}
		}
		else
		{
			NumericType NumericValue;
			if (LexicalConversion::TryParseString(NumericValue, *NewValue.ToString()))
			{
				OnValueChanged.ExecuteIfBound( NumericValue );
			}
		}
	}

	/**
	 * Caches the value and performs widget visibility maintenance
	 */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{
		// Visibility toggle only matters if the spinbox is used
		if (!SpinBox.IsValid())
		{
			return;
		}

		const auto& Value = ValueAttribute.Get();

		if (Value.IsSet() == true)
		{
			if (SpinBox->GetVisibility() != EVisibility::Visible)
			{
				// Set the visibility of the spinbox to visible if we have a valid value
				SpinBox->SetVisibility( EVisibility::Visible );
				// The text box should be invisible
				EditableText->SetVisibility( EVisibility::Collapsed );
			}
		}
		else
		{
			// The value isn't set so the spinbox should be hidden and the text box shown
			SpinBox->SetVisibility(EVisibility::Collapsed);
			EditableText->SetVisibility(EVisibility::Visible);
		}
	}

private:

	/** Attribute for getting the label */
	TAttribute< TOptional<FString > > LabelAttribute;
	/** Attribute for getting the value.  If the value is not set we display the undetermined string */
	TAttribute< TOptional<NumericType> > ValueAttribute;
	/** Spinbox widget */
	TSharedPtr<SWidget> SpinBox;
	/** Editable widget */
	TSharedPtr<SEditableText> EditableText;
	/** Delegate to call when the value changes */
	FOnValueChanged OnValueChanged;
	/** Delegate to call when the value is committed */
	FOnValueCommitted OnValueCommitted;
	/** Delegate to call when an undetermined value changes */
	FOnUndeterminedValueChanged OnUndeterminedValueChanged;
	/** Delegate to call when an undetermined is committed */
	FOnUndeterminedValueCommitted OnUndeterminedValueCommitted;
	/** The undetermined string to display when needed */
	FText UndeterminedString;
	/** Styling: border image to draw when not hovered or focused */
	const FSlateBrush* BorderImageNormal;
	/** Styling: border image to draw when hovered */
	const FSlateBrush* BorderImageHovered;
	/** Styling: border image to draw when focused */
	const FSlateBrush* BorderImageFocused;
	/** Prevents the value portion of the control from being smaller than desired in certain cases. */
	TAttribute<float> MinDesiredValueWidth;
	/** Type interface that defines how we should deal with the templated numeric type. Always valid after construction. */
	TSharedPtr< INumericTypeInterface<NumericType> > Interface;
};


template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::RedLabelBackgroundColor(0.594f,0.0197f,0.0f);

template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::GreenLabelBackgroundColor(0.1349f,0.3959f,0.0f);

template <typename NumericType>
const FLinearColor SNumericEntryBox<NumericType>::BlueLabelBackgroundColor(0.0251f,0.207f,0.85f);

template <typename NumericType>
const FText SNumericEntryBox<NumericType>::DefaultUndeterminedString = FText::FromString(TEXT("---"));
