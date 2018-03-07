// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Text/TextLayout.h"

class FPaintArgs;
class FSlateWindowElementList;
class FTextBlockLayout;
class IBreakIterator;
enum class ETextShapingMethod : uint8;

namespace ETextRole
{
	enum Type
	{
		Custom,
		ButtonText,
		ComboText
	};
}


/**
 * A simple static text widget
 */
class SLATE_API STextBlock : public SLeafWidget
{

public:

	SLATE_BEGIN_ARGS( STextBlock )
		: _Text()
		, _TextStyle( &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText" ) )
		, _Font()
		, _ColorAndOpacity()
		, _ShadowOffset()
		, _ShadowColorAndOpacity()
		, _HighlightColor()
		, _HighlightShape()
		, _HighlightText()
		, _WrapTextAt(0.0f)
		, _AutoWrapText(false)
		, _WrappingPolicy(ETextWrappingPolicy::DefaultWrapping)
		, _Margin()
		, _LineHeightPercentage(1.0f)
		, _Justification(ETextJustify::Left)
		, _MinDesiredWidth(0.0f)
		, _TextShapingMethod()
		, _TextFlowDirection()
		, _LineBreakPolicy()
		{
			_Clipping = EWidgetClipping::OnDemand;
		}

		/** The text displayed in this text block */
		SLATE_ATTRIBUTE( FText, Text )

		/** Pointer to a style of the text block, which dictates the font, color, and shadow options. */
		SLATE_STYLE_ARGUMENT( FTextBlockStyle, TextStyle )

		/** Sets the font used to draw the text */
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )

		/** Text color and opacity */
		SLATE_ATTRIBUTE( FSlateColor, ColorAndOpacity )

		/** Drop shadow offset in pixels */
		SLATE_ATTRIBUTE( FVector2D, ShadowOffset )

		/** Shadow color and opacity */
		SLATE_ATTRIBUTE( FLinearColor, ShadowColorAndOpacity )

		/** The color used to highlight the specified text */
		SLATE_ATTRIBUTE( FLinearColor, HighlightColor )

		/** The brush used to highlight the specified text*/
		SLATE_ATTRIBUTE( const FSlateBrush*, HighlightShape )

		/** Highlight this text in the text block */
		SLATE_ATTRIBUTE( FText, HighlightText )

		/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
		SLATE_ATTRIBUTE( float, WrapTextAt )

		/** Whether to wrap text automatically based on the widget's computed horizontal space.  IMPORTANT: Using automatic wrapping can result
		    in visual artifacts, as the the wrapped size will computed be at least one frame late!  Consider using WrapTextAt instead.  The initial 
			desired size will not be clamped.  This works best in cases where the text block's size is not affecting other widget's layout. */
		SLATE_ATTRIBUTE( bool, AutoWrapText )

		/** The wrapping policy to use */
		SLATE_ATTRIBUTE( ETextWrappingPolicy, WrappingPolicy )

		/** The amount of blank space left around the edges of text area. */
		SLATE_ATTRIBUTE( FMargin, Margin )

		/** The amount to scale each lines height by. */
		SLATE_ATTRIBUTE( float, LineHeightPercentage )

		/** How the text should be aligned with the margin. */
		SLATE_ATTRIBUTE( ETextJustify::Type, Justification )

		/** Minimum width that a text block should be */
		SLATE_ATTRIBUTE( float, MinDesiredWidth )

		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT( TOptional<ETextShapingMethod>, TextShapingMethod )
		
		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT( TOptional<ETextFlowDirection>, TextFlowDirection )

		/** The iterator to use to detect appropriate soft-wrapping points for lines (or null to use the default) */
		SLATE_ARGUMENT( TSharedPtr<IBreakIterator>, LineBreakPolicy )

		/** Called when this text is double clicked */
		SLATE_EVENT( FOnClicked, OnDoubleClicked )

	SLATE_END_ARGS()

	/** Constructor */
	STextBlock();

	/** Destructor */
	~STextBlock();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	/**
	 * Gets the text assigned to this text block
	 *
	 * @return	This text block's text string
	 */
	const FText& GetText() const
	{
		return BoundText.Get();
	}
	
	/**
	 * Sets the text for this text block
	 *
	 * @param	InText	The new text to display
	 */
	void SetText( const TAttribute< FString >& InText );
	void SetText( const FString& InText );
	void SetText( const TAttribute< FText >& InText );
	void SetText( const FText& InText );

	/**
	* Sets the highlight text for this text block 
	*
	* @param	InText	The new text to highlight
	*/
	void SetHighlightText(TAttribute<FText> InText);

	/**
	 * Sets the font used to draw the text
	 *
	 * @param	InFont	The new font to use
	 */
	void SetFont(const TAttribute< FSlateFontInfo >& InFont);

	/** See ColorAndOpacity attribute */
	void SetColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity);

	/** See TextStyle argument */
	void SetTextStyle(const FTextBlockStyle* InTextStyle);

	/** See TextShapingMethod attribute */
	void SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod);

	/** See TextFlowDirection attribute */
	void SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection);

	/** See WrapTextAt attribute */
	void SetWrapTextAt(const TAttribute<float>& InWrapTextAt);

	/** See AutoWrapText attribute */
	void SetAutoWrapText(const TAttribute<bool>& InAutoWrapText);

	/** Set WrappingPolicy attribute */
	void SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy);

	/** See ShadowOffset attribute */
	void SetShadowOffset(const TAttribute<FVector2D>& InShadowOffset);

	/** See ShadowColorAndOpacity attribute */
	void SetShadowColorAndOpacity(const TAttribute<FLinearColor>& InShadowColorAndOpacity);

	/** See MinDesiredWidth attribute */
	void SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth);

	/** See LineHeightPercentage attribute */
	void SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage);

	/** See Margin attribute */
	void SetMargin(const TAttribute<FMargin>& InMargin);

	/** See Justification attribute */
	void SetJustification(const TAttribute<ETextJustify::Type>& InJustification);

	// SWidget interface
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End of SWidget interface

protected:
	// Begin SWidget overrides.
	virtual bool ComputeVolatility() const override;
	// End of SWidget interface

private:
	/** Get the computed text style to use with the text marshaller */
	FTextBlockStyle GetComputedTextStyle() const;

	/** Gets the current foreground color */
	FSlateColor GetColorAndOpacity() const;

	/** Gets the current font */
	FSlateFontInfo GetFont() const;

	/** Gets the current shadow offset */
	FVector2D GetShadowOffset() const;

	/** Gets the current shadow color and opacity */
	FLinearColor GetShadowColorAndOpacity() const;

	/** Gets the current highlight color */
	FLinearColor GetHighlightColor() const;

	/** Gets the current highlight shape */
	const FSlateBrush* GetHighlightShape() const;

private:
	/** The text displayed in this text block */
	TAttribute< FText > BoundText;

	/** The wrapped layout for this text block */
	TUniquePtr< FTextBlockLayout > TextLayoutCache;

	/** Default style used by the TextLayout */
	FTextBlockStyle TextStyle;

	/** Sets the font used to draw the text */
	TAttribute< FSlateFontInfo > Font;

	/** Text color and opacity */
	TAttribute<FSlateColor> ColorAndOpacity;

	/** Drop shadow offset in pixels */
	TAttribute< FVector2D > ShadowOffset;

	/** Shadow color and opacity */
	TAttribute<FLinearColor> ShadowColorAndOpacity;

	TAttribute<FLinearColor> HighlightColor;

	TAttribute< const FSlateBrush* > HighlightShape;

	/** Highlight this text in the textblock */
	TAttribute<FText> HighlightText;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	TAttribute<float> WrapTextAt;

	/** True if we're wrapping text automatically based on the computed horizontal space for this widget */
	TAttribute<bool> AutoWrapText;

	/** The wrapping policy we're using */
	TAttribute<ETextWrappingPolicy> WrappingPolicy;

	/** The amount of blank space left around the edges of text area. */
	TAttribute< FMargin > Margin;

	/** The amount to scale each lines height by. */
	TAttribute< ETextJustify::Type > Justification; 

	/** How the text should be aligned with the margin. */
	TAttribute< float > LineHeightPercentage;

	/** Prevents the text block from being smaller than desired in certain cases (e.g. when it is empty) */
	TAttribute<float> MinDesiredWidth;

	/** The delegate to execute when this text is double clicked */
	FOnClicked OnDoubleClicked;
};
