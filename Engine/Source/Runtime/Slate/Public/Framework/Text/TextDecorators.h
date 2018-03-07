// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Framework/Text/ITextDecorator.h"
#include "Framework/Text/SlateWidgetRun.h"

class ISlateStyle;

#if WITH_FANCY_TEXT

class SLATE_API FWidgetDecorator : public ITextDecorator
{
public:

	DECLARE_DELEGATE_RetVal_TwoParams( FSlateWidgetRun::FWidgetRunInfo, FCreateWidget, const FTextRunInfo& /*RunInfo*/, const ISlateStyle* /*Style*/ )

public:

	static TSharedRef< FWidgetDecorator > Create( FString InRunName, const FCreateWidget& InCreateWidgetDelegate );
	virtual ~FWidgetDecorator() {}

public:

	virtual bool Supports( const FTextRunParseResults& RunParseResult, const FString& Text ) const override;

	virtual TSharedRef< ISlateRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override;

private:

	FWidgetDecorator( FString InRunName, const FCreateWidget& InCreateWidgetDelegate );

private:

	FString RunName;
	FCreateWidget CreateWidgetDelegate;
};

class SLATE_API FImageDecorator : public ITextDecorator
{
public:

	static TSharedRef< FImageDecorator > Create( FString InRunName, const ISlateStyle* const InOverrideStyle = NULL );
	virtual ~FImageDecorator() {}

public:

	virtual bool Supports( const FTextRunParseResults& RunParseResult, const FString& Text ) const override;

	virtual TSharedRef< ISlateRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override;

private:

	FImageDecorator( FString InRunName, const ISlateStyle* const InOverrideStyle );

private:

	FString RunName;
	const ISlateStyle* OverrideStyle;
};

class SLATE_API FHyperlinkDecorator : public ITextDecorator
{
public:

	static TSharedRef< FHyperlinkDecorator > Create( FString Id, const FSlateHyperlinkRun::FOnClick& NavigateDelegate, const FSlateHyperlinkRun::FOnGetTooltipText& InToolTipTextDelegate = FSlateHyperlinkRun::FOnGetTooltipText(), const FSlateHyperlinkRun::FOnGenerateTooltip& InToolTipDelegate = FSlateHyperlinkRun::FOnGenerateTooltip() );
	virtual ~FHyperlinkDecorator() {}

public:

	virtual bool Supports( const FTextRunParseResults& RunParseResult, const FString& Text ) const override;

	virtual TSharedRef< ISlateRun > Create(const TSharedRef<class FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef< FString >& InOutModelText, const ISlateStyle* Style) override;

protected:

	FHyperlinkDecorator( FString InId, const FSlateHyperlinkRun::FOnClick& InNavigateDelegate, const FSlateHyperlinkRun::FOnGetTooltipText& InToolTipTextDelegate, const FSlateHyperlinkRun::FOnGenerateTooltip& InToolTipDelegate );

protected:

	FSlateHyperlinkRun::FOnClick NavigateDelegate;

protected:
	FString Id;
	FSlateHyperlinkRun::FOnGetTooltipText ToolTipTextDelegate;
	FSlateHyperlinkRun::FOnGenerateTooltip ToolTipDelegate;
};


#endif //WITH_FANCY_TEXT
