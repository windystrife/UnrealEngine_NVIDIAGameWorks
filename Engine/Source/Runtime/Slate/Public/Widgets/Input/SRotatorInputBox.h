// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Input/NumericTypeInterface.h"

class FArrangedChildren;

/**
 * FRotator Slate control
 */
class SLATE_API SRotatorInputBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRotatorInputBox )
		: _bColorAxisLabels(false)
		, _AllowResponsiveLayout(false)
		, _Font(FCoreStyle::Get().GetFontStyle("NormalFont"))
		, _AllowSpin(true)
		{}

		/** Roll component of the rotator */
		SLATE_ATTRIBUTE( TOptional<float>, Roll )

		/** Pitch component of the rotator */
		SLATE_ATTRIBUTE( TOptional<float>, Pitch )

		/** Yaw component of the rotator */
		SLATE_ATTRIBUTE( TOptional<float>, Yaw )

		/** Should the axis labels be colored */
		SLATE_ARGUMENT( bool, bColorAxisLabels )		

			/** Allow responsive layout to crush the label and margins when there is not a lot of room */
		SLATE_ARGUMENT(bool, AllowResponsiveLayout)

		/** Font to use for the text in this box */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Whether or not values can be spun or if they should be typed in */
		SLATE_ARGUMENT( bool, AllowSpin )

		/** Called when the pitch value is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnPitchChanged )

		/** Called when the yaw value is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnYawChanged )

		/** Called when the roll value is changed */
		SLATE_EVENT( FOnFloatValueChanged, OnRollChanged )

		/** Called when the pitch value is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnPitchCommitted )

		/** Called when the yaw value is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnYawCommitted )

		/** Called when the roll value is committed */
		SLATE_EVENT( FOnFloatValueCommitted, OnRollCommitted )

		/** Called when the slider begins to move on any axis */
		SLATE_EVENT( FSimpleDelegate, OnBeginSliderMovement )

		/** Called when the slider for any axis is released */
		SLATE_EVENT( FOnFloatValueChanged, OnEndSliderMovement )

		/** Provide custom type functionality for the rotator */
		SLATE_ARGUMENT( TSharedPtr< INumericTypeInterface<float> >, TypeInterface )

	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );


	// SWidget interface
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	// End of SWidget interface

private:
	/** Are we allowed to be crushed? */
	bool bCanBeCrushed;

	/** Are we currently being crushed? */
	mutable bool bIsBeingCrushed;

private:
	/** Returns the index of the label widget to use (crushed or uncrushed) */
	int32 GetLabelActiveSlot() const;

	/** Returns the desired text margin for the label */
	FMargin GetTextMargin() const;

	/** Creates a decorator label (potentially adding a switcher widget if this is cruhsable) */
	TSharedRef<SWidget> BuildDecoratorLabel(FLinearColor BackgroundColor, FLinearColor ForegroundColor, FText Label);
};
