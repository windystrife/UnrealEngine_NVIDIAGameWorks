// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ITextDecorator.h"
#include "RichTextBlockDecorator.generated.h"

class ISlateStyle;
class URichTextBlockDecorator;

class FDefaultRichTextDecorator : public ITextDecorator
{
public:
	FDefaultRichTextDecorator(URichTextBlockDecorator* Decorator, const FSlateFontInfo& DefaultFont, const FLinearColor& DefaultColor);

	virtual ~FDefaultRichTextDecorator();

	virtual bool Supports(const FTextRunParseResults& RunParseResult, const FString& Text) const override;

	virtual TSharedRef<ISlateRun> Create(const TSharedRef<FTextLayout>& TextLayout, const FTextRunParseResults& RunParseResult, const FString& OriginalText, const TSharedRef<FString>& InOutModelText, const ISlateStyle* Style) override;

protected:

	virtual TSharedRef<ISlateRun> CreateRun(const TSharedRef<FTextLayout>& TextLayout, const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FTextBlockStyle& Style, const FTextRange& InRange);

	FTextBlockStyle CreateTextBlockStyle(const FRunInfo& InRunInfo) const;

	void ExplodeRunInfo(const FRunInfo& InRunInfo, FSlateFontInfo& OutFont, FLinearColor& OutFontColor) const;

protected:

	FSlateFontInfo DefaultFont;
	FLinearColor DefaultColor;

private:
	TWeakObjectPtr<URichTextBlockDecorator> Decorator;
};

UCLASS(editinlinenew)
class UMG_API URichTextBlockDecorator : public UObject
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation)
	bool bReveal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation)
	int32 RevealedIndex;

public:
	URichTextBlockDecorator(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<ITextDecorator> CreateDecorator(const FSlateFontInfo& DefaultFont, const FLinearColor& DefaultColor);
};
