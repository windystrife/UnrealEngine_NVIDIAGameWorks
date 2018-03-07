// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Layout/Margin.h"
#include "Sound/SlateSound.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/Layout/SBorder.h"

class FPaintArgs;
class FSlateWindowElementList;
enum class ETextFlowDirection : uint8;
enum class ETextShapingMethod : uint8;

/**
 * Slate's Buttons are clickable Widgets that can contain arbitrary widgets as its Content().
 */
class SLATE_API SButton
	: public SBorder
{
public:

	SLATE_BEGIN_ARGS( SButton )
		: _Content()
		, _ButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FButtonStyle >( "Button" ) )
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle< FTextBlockStyle >("NormalText") )
		, _HAlign( HAlign_Fill )
		, _VAlign( VAlign_Fill )
		, _ContentPadding(FMargin(4.0, 2.0))
		, _Text()
		, _ClickMethod( EButtonClickMethod::DownAndUp )
		, _TouchMethod( EButtonTouchMethod::DownAndUp )
		, _PressMethod( EButtonPressMethod::DownAndUp )
		, _DesiredSizeScale( FVector2D(1,1) )
		, _ContentScale( FVector2D(1,1) )
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ForegroundColor( FCoreStyle::Get().GetSlateColor( "InvertedForeground" ) )
		, _IsFocusable( true )
		{
		}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT( FArguments, Content )

		/** The visual style of the button */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )

		/** The text style of the button */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** Horizontal alignment */
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )

		/** Vertical alignment */
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )
	
		/** Spacing between button's border and the content. */
		SLATE_ATTRIBUTE( FMargin, ContentPadding )
	
		/** The text to display in this button, if no custom content is specified */
		SLATE_ATTRIBUTE( FText, Text )
	
		/** Called when the button is clicked */
		SLATE_EVENT( FOnClicked, OnClicked )

		/** Called when the button is pressed */
		SLATE_EVENT( FSimpleDelegate, OnPressed )

		/** Called when the button is released */
		SLATE_EVENT( FSimpleDelegate, OnReleased )

		SLATE_EVENT( FSimpleDelegate, OnHovered )

		SLATE_EVENT( FSimpleDelegate, OnUnhovered )

		/** Sets the rules to use for determining whether the button was clicked.  This is an advanced setting and generally should be left as the default. */
		SLATE_ARGUMENT( EButtonClickMethod::Type, ClickMethod )

		/** How should the button be clicked with touch events? */
		SLATE_ARGUMENT( EButtonTouchMethod::Type, TouchMethod )

		/** How should the button be clicked with keyboard/controller button events? */
		SLATE_ARGUMENT( EButtonPressMethod::Type, PressMethod )

		SLATE_ATTRIBUTE( FVector2D, DesiredSizeScale )

		SLATE_ATTRIBUTE( FVector2D, ContentScale )

		SLATE_ATTRIBUTE( FSlateColor, ButtonColorAndOpacity )

		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )

		/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
		SLATE_ARGUMENT( bool, IsFocusable )

		/** The sound to play when the button is pressed */
		SLATE_ARGUMENT( TOptional<FSlateSound>, PressedSoundOverride )

		/** The sound to play when the button is hovered */
		SLATE_ARGUMENT( TOptional<FSlateSound>, HoveredSoundOverride )

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT( TOptional<ETextShapingMethod>, TextShapingMethod )
		
		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT( TOptional<ETextFlowDirection>, TextFlowDirection )

	SLATE_END_ARGS()


	/** @return An image that represents this button's border*/
	const FSlateBrush* GetBorder() const;

	/**
	 * Returns true if this button is currently pressed
	 *
	 * @return	True if pressed, otherwise false
	 */
	virtual bool IsPressed() const
	{
		return bIsPressed;
	}

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/** See ContentPadding attribute */
	void SetContentPadding(const TAttribute<FMargin>& InContentPadding);

	/** See HoveredSound attribute */
	void SetHoveredSound(TOptional<FSlateSound> InHoveredSound);

	/** See PressedSound attribute */
	void SetPressedSound(TOptional<FSlateSound> InPressedSound);

	/** See OnClicked event */
	void SetOnClicked(FOnClicked InOnClicked);

	/** Set OnHovered event */
	void SetOnHovered(FSimpleDelegate InOnHovered);

	/** Set OnUnhovered event */
	void SetOnUnhovered(FSimpleDelegate InOnUnhovered);

	/** See ButtonStyle attribute */
	void SetButtonStyle(const FButtonStyle* ButtonStyle);

	void SetClickMethod(EButtonClickMethod::Type InClickMethod);
	void SetTouchMethod(EButtonTouchMethod::Type InTouchMethod);
	void SetPressMethod(EButtonPressMethod::Type InPressMethod);

public:

	// SWidget overrides
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent ) override;
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseCaptureLost() override;
	virtual bool IsInteractable() const override;
	// SWidget

protected:

	/** @return combines the user-specified margin and the button's internal margin. */
	FMargin GetCombinedPadding() const;

	/** @return True if the disab;ed effect should be shown. */
	bool GetShowDisabledEffect() const;

	/** Padding specified by the user; it will be combind with the button's internal padding. */
	TAttribute<FMargin> ContentPadding;

	/** Padding that accounts for the button border */
	FMargin BorderPadding;

	/** Padding that accounts for the button border when pressed */
	FMargin PressedBorderPadding;

	/** The location in screenspace the button was pressed */
	FVector2D PressedScreenSpacePosition;

	/** Style resource for the button */
	const FButtonStyle* Style;

	/** Brush resource that represents a button */
	const FSlateBrush* NormalImage;
	/** Brush resource that represents a button when it is hovered */
	const FSlateBrush* HoverImage;
	/** Brush resource that represents a button when it is pressed */
	const FSlateBrush* PressedImage;
	/** Brush resource that represents a button when it is disabled */
	const FSlateBrush* DisabledImage;

	/** The delegate to execute when the button is clicked */
	FOnClicked OnClicked;

	/** The delegate to execute when the button is pressed */
	FSimpleDelegate OnPressed;

	/** The delegate to execute when the button is released */
	FSimpleDelegate OnReleased;

	FSimpleDelegate OnHovered;

	FSimpleDelegate OnUnhovered;

	/** Press the button */
	virtual void Press();

	/** Release the button */
	virtual void Release();

	/** Utility function to determine if the incoming mouse event is for a precise tap or click */
	bool IsPreciseTapOrClick(const FPointerEvent& MouseEvent) const;

	/** Play the pressed sound */
	void PlayPressedSound() const;

	/** Play the hovered sound */
	void PlayHoverSound() const;

	/** The Sound to play when the button is hovered  */
	FSlateSound HoveredSound;

	/** The Sound to play when the button is pressed */
	FSlateSound PressedSound;

	/** Sets whether a click should be triggered on mouse down, mouse up, or that both a mouse down and up are required. */
	TEnumAsByte<EButtonClickMethod::Type> ClickMethod;

	/** How should the button be clicked with touch events? */
	TEnumAsByte<EButtonTouchMethod::Type> TouchMethod;

	/** How should the button be clicked with keyboard/controller button events? */
	TEnumAsByte<EButtonPressMethod::Type> PressMethod;

	/** Can this button be focused? */
	uint8 bIsFocusable:1;

	/** True if this button is currently in a pressed state */
	uint8 bIsPressed:1;

private:

	virtual FVector2D ComputeDesiredSize(float) const override;
};
