// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/IToolTip.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/ISlateRun.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
class FWidgetViewModel;
enum class ETextHitPoint : uint8;

#if WITH_FANCY_TEXT

class SLATE_API FSlateHyperlinkRun : public ISlateRun, public TSharedFromThis< FSlateHyperlinkRun >
{
public:

	typedef TMap< FString, FString > FMetadata;
	DECLARE_DELEGATE_OneParam( FOnClick, const FMetadata& /*Metadata*/ );
	DECLARE_DELEGATE_RetVal_OneParam( FText, FOnGetTooltipText, const FMetadata& /*Metadata*/ );
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<IToolTip>, FOnGenerateTooltip, const FMetadata& /*Metadata*/ );

public:

	class FWidgetViewModel
	{
	public:

		bool IsPressed() const { return bIsPressed; }
		bool IsHovered() const { return bIsHovered; }

		void SetIsPressed( bool Value ) { bIsPressed = Value; }
		void SetIsHovered( bool Value ) { bIsHovered = Value; }

	private:

		bool bIsPressed;
		bool bIsHovered;
	};

public:

	static TSharedRef< FSlateHyperlinkRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate );
																																	 
	static TSharedRef< FSlateHyperlinkRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick NavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange );

public:

	virtual ~FSlateHyperlinkRun() {}

	virtual FTextRange GetTextRange() const override;

	virtual void SetTextRange( const FTextRange& Value ) override;

	virtual int16 GetBaseLine( float Scale ) const override;

	virtual int16 GetMaxHeight( float Scale ) const override;

	virtual FVector2D Measure( int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext ) const override;

	virtual int8 GetKerning(int32 CurrentIndex, float Scale, const FRunTextContext& TextContext) const override;

	virtual TSharedRef< ILayoutBlock > CreateBlock( int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer ) override;

	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	virtual const TArray< TSharedRef<SWidget> >& GetChildren() override;

	virtual void ArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	virtual int32 GetTextIndexAt( const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint = nullptr ) const override;

	virtual FVector2D GetLocationAt( const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale ) const override;

	virtual void BeginLayout() override { Children.Empty(); }
	virtual void EndLayout() override {}

	virtual void Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange) override;
	virtual TSharedRef<IRun> Clone() const override;

	virtual void AppendTextTo(FString& AppendToText) const override;
	virtual void AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const override;

	virtual const FRunInfo& GetRunInfo() const override;

	virtual ERunAttributes GetRunAttributes() const override;

protected:

	FSlateHyperlinkRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate );
																										 
	FSlateHyperlinkRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FHyperlinkStyle& InStyle, FOnClick InNavigateDelegate, FOnGenerateTooltip InTooltipDelegate, FOnGetTooltipText InTooltipTextDelegate, const FTextRange& InRange );

	FSlateHyperlinkRun( const FSlateHyperlinkRun& Run );

protected:

	void OnNavigate();

protected:

	FRunInfo RunInfo;
	TSharedRef< const FString > Text;
	FTextRange Range;
	FHyperlinkStyle Style;
	FOnClick NavigateDelegate;
	FOnGenerateTooltip TooltipDelegate;
	FOnGetTooltipText TooltipTextDelegate;

	TSharedRef< FWidgetViewModel > ViewModel;
	TArray< TSharedRef<SWidget> > Children;
};

#endif //WITH_FANCY_TEXT
