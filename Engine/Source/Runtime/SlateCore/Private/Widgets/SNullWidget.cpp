// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNullWidget.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"


FNoChildren NullWidgetNoChildren;

class SLATECORE_API SNullWidgetContent
	: public SWidget
{
public:

	SLATE_BEGIN_ARGS(SNullWidgetContent)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs )
	{
	}

	SNullWidgetContent()
	{
		bCanTick = false;
		bCanSupportFocus = false;
		bCanHaveChildren = false;
	}

private:
	virtual void SetVisibility( TAttribute<EVisibility> InVisibility ) override final
	{
		ensureMsgf( false, TEXT("Attempting to SetVisibility() on SNullWidget. Mutating SNullWidget is not allowed.") );
	}
public:
	
	// SWidget interface

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override
	{
		return LayerId;
	}

	virtual FChildren* GetChildren( ) override final
	{
		return &NullWidgetNoChildren;
	}

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override final
	{
		// Nothing to arrange; Null Widgets do not have children.
	}

protected:
	virtual FVector2D ComputeDesiredSize(float) const override final
	{
		return FVector2D(0.0f, 0.0f);
	}

	// End of SWidget interface
};


SLATECORE_API TSharedRef<SWidget> SNullWidget::NullWidget = SNew(SNullWidgetContent).Visibility(EVisibility::Hidden);
