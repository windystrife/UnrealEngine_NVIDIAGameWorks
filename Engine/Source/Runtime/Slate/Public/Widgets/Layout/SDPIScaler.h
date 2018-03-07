// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;

/**
 * Controls the DPI scale of its content. Can be used for zooming or shrinking arbitrary widget content.
 */
class SLATE_API SDPIScaler : public SPanel
{
public:
	SLATE_BEGIN_ARGS( SDPIScaler )
		: _DPIScale( 1.0f )
		, _Content()
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		/**
		 * The scaling factor between Pixels and Slate Units (SU).
		 * For example, a value of 2 means 2 pixels for every linear Slate Unit and 4 pixels for every square Slate Unit.
		 */
		SLATE_ATTRIBUTE( float, DPIScale )

		/** The content being presented and scaled. */
		SLATE_DEFAULT_SLOT( FArguments, Content )

	SLATE_END_ARGS()

	SDPIScaler();

	void Construct( const FArguments& InArgs );

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	
	virtual FVector2D ComputeDesiredSize(float) const override;

	virtual FChildren* GetChildren() override;

	/** See the Content attribute */
	void SetContent(TSharedRef<SWidget> InContent);

	/** See the DPIScale attribute */
	void SetDPIScale(TAttribute<float> InDPIScale);

protected:

	virtual float GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const override;

	/**
	 * The scaling factor from 1:1 pixel to slate unit (SU) ratio.
	 * e.g. Value of 2 would mean that there are two pixels for every slate unit in every dimension.
	 *      This means that every 1x1 SU square is represented by 2x2=4 pixels.
	 */
	TAttribute<float> DPIScale;

	/** The content being scaled. */
	FSimpleSlot ChildSlot;
};
