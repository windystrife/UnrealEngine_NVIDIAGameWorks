// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "RichTextBlock.generated.h"

class SRichTextBlock;
class URichTextBlockDecorator;

/**
 * The rich text block
 *
 * * Fancy Text
 * * No Children
 */
UCLASS(Experimental)
class UMG_API URichTextBlock : public UTextLayoutWidget
{
	GENERATED_BODY()

public:
	URichTextBlock(const FObjectInitializer& ObjectInitializer);
	
	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	// End UWidget interface
#endif

protected:
	/** The text to display */
	UPROPERTY(EditAnywhere, Category=Content, meta=( MultiLine="true" ))
	FText Text;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

	/** The default font for the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateFontInfo Font;

	/** The default color for the text. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Instanced, Category=Decorators)
	TArray<URichTextBlockDecorator*> Decorators;

protected:
	FTextBlockStyle DefaultStyle;

	/** Native Slate Widget */
	TSharedPtr<SRichTextBlock> MyRichTextBlock;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

protected:
	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
};
