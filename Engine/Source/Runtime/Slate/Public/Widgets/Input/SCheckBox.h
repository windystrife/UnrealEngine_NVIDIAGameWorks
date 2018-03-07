// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Sound/SlateSound.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"

class SBorder;


/** Delegate that is executed when the check box state changes */
DECLARE_DELEGATE_OneParam( FOnCheckStateChanged, ECheckBoxState );


/**
 * Check box Slate control
 */
class SLATE_API SCheckBox : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SCheckBox )
		: _Content()
		, _Style( &FCoreStyle::Get().GetWidgetStyle< FCheckBoxStyle >("Checkbox") )
		, _Type()
		, _OnCheckStateChanged()
		, _IsChecked( ECheckBoxState::Unchecked )
		, _HAlign( HAlign_Fill )
		, _Padding()
		, _ClickMethod( EButtonClickMethod::DownAndUp )
		, _ForegroundColor()
		, _BorderBackgroundColor ()
		, _IsFocusable( true )
		, _UncheckedImage( nullptr )
		, _UncheckedHoveredImage( nullptr )
		, _UncheckedPressedImage( nullptr )
		, _CheckedImage( nullptr )
		, _CheckedHoveredImage( nullptr )
		, _CheckedPressedImage( nullptr )
		, _UndeterminedImage( nullptr )
		, _UndeterminedHoveredImage( nullptr )
		, _UndeterminedPressedImage( nullptr )
		{}

		/** Content to be placed next to the check box, or for a toggle button, the content to be placed inside the button */
		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** The style structure for this checkbox' visual style */
		SLATE_STYLE_ARGUMENT( FCheckBoxStyle, Style )

		/** Type of check box (set by the Style arg but the Style can be overridden with this) */
		SLATE_ARGUMENT( TOptional<ESlateCheckBoxType::Type>, Type )

		/** Called when the checked state has changed */
		SLATE_EVENT( FOnCheckStateChanged, OnCheckStateChanged )

		/** Whether the check box is currently in a checked state */
		SLATE_ATTRIBUTE( ECheckBoxState, IsChecked )

		/** How the content of the toggle button should align within the given space*/
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Spacing between the check box image and its content (set by the Style arg but the Style can be overridden with this) */
		SLATE_ATTRIBUTE( FMargin, Padding )

		/** Sets the rules to use for determining whether the button was clicked.  This is an advanced setting and generally should be left as the default. */
		SLATE_ATTRIBUTE( EButtonClickMethod::Type, ClickMethod )

		/** Foreground color for the checkbox's content and parts (set by the Style arg but the Style can be overridden with this) */
		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		/** The color of the background border (set by the Style arg but the Style can be overridden with this) */
		SLATE_ATTRIBUTE( FSlateColor, BorderBackgroundColor )

		SLATE_ARGUMENT( bool, IsFocusable )
		
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )

		/** The sound to play when the check box is checked */
		SLATE_ARGUMENT( TOptional<FSlateSound>, CheckedSoundOverride )

		/** The sound to play when the check box is unchecked */
		SLATE_ARGUMENT( TOptional<FSlateSound>, UncheckedSoundOverride )

		/** The sound to play when the check box is hovered */
		SLATE_ARGUMENT( TOptional<FSlateSound>, HoveredSoundOverride )

		/** The unchecked image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UncheckedImage)

		/** The unchecked hovered image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UncheckedHoveredImage)

		/** The unchecked pressed image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UncheckedPressedImage)

		/** The checked image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, CheckedImage)

		/** The checked hovered image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, CheckedHoveredImage)

		/** The checked pressed image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, CheckedPressedImage)

		/** The undetermined image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UndeterminedImage)

		/** The undetermined hovered image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UndeterminedHoveredImage)

		/** The undetermined pressed image for the checkbox - overrides the style's */
		SLATE_ARGUMENT(const FSlateBrush*, UndeterminedPressedImage)

	SLATE_END_ARGS()


	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual bool IsInteractable() const override;
	// End of SWidget interface

	/**
	 * Returns true if the checkbox is currently checked
	 *
	 * @return	True if checked, otherwise false
	 */
	bool IsChecked() const
	{
		return ( IsCheckboxChecked.Get() == ECheckBoxState::Checked );
	}

	/** @return The current checked state of the checkbox. */
	ECheckBoxState GetCheckedState() const;

	/**
	 * Returns true if this button is currently pressed
	 *
	 * @return	True if pressed, otherwise false
	 */
	bool IsPressed() const
	{
		return bIsPressed;
	}

	/**
	 * Toggles the checked state for this check box, fire events as needed
	 */
	void ToggleCheckedState();

	/** See the IsChecked attribute */
	void SetIsChecked(TAttribute<ECheckBoxState> InIsChecked);
	
	/** See the Content slot */
	void SetContent(const TSharedRef< SWidget >& InContent);
	
	/** See the Style attribute */
	void SetStyle(const FCheckBoxStyle* InStyle);
	
	/** See the UncheckedImage attribute */
	void SetUncheckedImage(const FSlateBrush* Brush);
	/** See the UncheckedHoveredImage attribute */
	void SetUncheckedHoveredImage(const FSlateBrush* Brush);
	/** See the UncheckedPressedImage attribute */
	void SetUncheckedPressedImage(const FSlateBrush* Brush);
	
	/** See the CheckedImage attribute */
	void SetCheckedImage(const FSlateBrush* Brush);
	/** See the CheckedHoveredImage attribute */
	void SetCheckedHoveredImage(const FSlateBrush* Brush);
	/** See the CheckedPressedImage attribute */
	void SetCheckedPressedImage(const FSlateBrush* Brush);
	
	/** See the UndeterminedImage attribute */
	void SetUndeterminedImage(const FSlateBrush* Brush);
	/** See the UndeterminedHoveredImage attribute */
	void SetUndeterminedHoveredImage(const FSlateBrush* Brush);
	/** See the UndeterminedPressedImage attribute */
	void SetUndeterminedPressedImage(const FSlateBrush* Brush);

protected:

	/** Rebuilds the checkbox based on the current ESlateCheckBoxType */
	void BuildCheckBox(TSharedRef<SWidget> InContent);

	/** Attribute getter for the foreground color */
	FSlateColor OnGetForegroundColor() const;
	/** Attribute getter for the padding */
	FMargin OnGetPadding() const;
	/** Attribute getter for the border background color */
	FSlateColor OnGetBorderBackgroundColor() const;
	/** Attribute getter for the checkbox type */
	ESlateCheckBoxType::Type OnGetCheckBoxType() const;

	/**
	 * Gets the check image to display for the current state of the check box
	 * @return	The name of the image to display
	 */
	const FSlateBrush* OnGetCheckImage() const;
	
	const FSlateBrush* GetUncheckedImage() const;
	const FSlateBrush* GetUncheckedHoveredImage() const;
	const FSlateBrush* GetUncheckedPressedImage() const;
	
	const FSlateBrush* GetCheckedImage() const;
	const FSlateBrush* GetCheckedHoveredImage() const;
	const FSlateBrush* GetCheckedPressedImage() const;
	
	const FSlateBrush* GetUndeterminedImage() const;
	const FSlateBrush* GetUndeterminedHoveredImage() const;
	const FSlateBrush* GetUndeterminedPressedImage() const;
	
protected:
	
	const FCheckBoxStyle* Style;

	/** True if this check box is currently in a pressed state */
	bool bIsPressed;

	/** Are we checked */
	TAttribute<ECheckBoxState> IsCheckboxChecked;

	/** Delegate called when the check box changes state */
	FOnCheckStateChanged OnCheckStateChanged;

	/** Image to use when the checkbox is unchecked */
	const FSlateBrush* UncheckedImage;
	/** Image to use when the checkbox is unchecked and hovered*/
	const FSlateBrush* UncheckedHoveredImage;
	/** Image to use when the checkbox is unchecked and pressed*/
	const FSlateBrush* UncheckedPressedImage;
	/** Image to use when the checkbox is checked*/
	const FSlateBrush* CheckedImage;
	/** Image to use when the checkbox is checked and hovered*/
	const FSlateBrush* CheckedHoveredImage;
	/** Image to use when the checkbox is checked and pressed*/
	const FSlateBrush* CheckedPressedImage;
	/** Image to use when the checkbox is in an ambiguous state*/
	const FSlateBrush* UndeterminedImage;
	/** Image to use when the checkbox is in an ambiguous state and hovered*/
	const FSlateBrush* UndeterminedHoveredImage;
	/** Image to use when the checkbox is in an ambiguous state and pressed*/
	const FSlateBrush* UndeterminedPressedImage;

	/** Overrides padding in the widget style, if set */
	TAttribute<FMargin> PaddingOverride;
	/** Overrides foreground color in the widget style, if set */
	TAttribute<FSlateColor> ForegroundColorOverride;
	/** Overrides border background color in the widget style, if set */
	TAttribute<FSlateColor> BorderBackgroundColorOverride;
	/** Overrides checkbox type in the widget style, if set */
	TOptional<ESlateCheckBoxType::Type> CheckBoxTypeOverride;

	/** Horiz align setting if in togglebox mode */
	EHorizontalAlignment HorizontalAlignment;

	/** Sets whether a click should be triggered on mouse down, mouse up, or that both a mouse down and up are required. */
	EButtonClickMethod::Type ClickMethod;

	/** When true, this checkbox will be keyboard focusable. Defaults to true. */
	bool bIsFocusable;

	/** Delegate to execute to get the menu content of this button */
	FOnGetContent OnGetMenuContent;

	/** Play the checked sound */
	void PlayCheckedSound() const;

	/** Play the unchecked sound */
	void PlayUncheckedSound() const;

	/** Play the hovered sound */
	void PlayHoverSound() const;

	/** The Sound to play when the check box is hovered  */
	FSlateSound HoveredSound;

	/** The Sound to play when the check box is checked */
	FSlateSound CheckedSound;

	/** The Sound to play when the check box is unchecked */
	FSlateSound UncheckedSound;

protected:
	/** When in toggle button mode, this will hold the pointer to the toggle button's border */
	TSharedPtr<SBorder> ContentContainer;
};
