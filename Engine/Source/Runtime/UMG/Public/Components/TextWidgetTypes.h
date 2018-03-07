// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Components/Widget.h"
#include "Framework/Text/TextLayout.h"
#include "TextWidgetTypes.generated.h"

enum class ETextShapingMethod : uint8;

/**
 * Common data for all widgets that use shaped text.
 * Contains the common options that should be exposed for the underlying Slate widget.
 */
USTRUCT(BlueprintType)
struct UMG_API FShapedTextOptions
{
	GENERATED_USTRUCT_BODY()

	FShapedTextOptions();

	/** Synchronize the properties with the given widget. A template as the Slate widgets conform to the same API, but don't derive from a common base. */
	template <typename TWidgetType>
	void SynchronizeShapedTextProperties(TWidgetType& InWidget)
	{
		InWidget.SetTextShapingMethod(bOverride_TextShapingMethod ? TOptional<ETextShapingMethod>(TextShapingMethod) : TOptional<ETextShapingMethod>());
		InWidget.SetTextFlowDirection(bOverride_TextFlowDirection ? TOptional<ETextFlowDirection>(TextFlowDirection) : TOptional<ETextFlowDirection>());
	}

	/**  */
	UPROPERTY(EditAnywhere, Category=Localization, meta=(InlineEditConditionToggle))
	uint32 bOverride_TextShapingMethod : 1;

	/**  */
	UPROPERTY(EditAnywhere, Category=Localization, meta=(InlineEditConditionToggle))
	uint32 bOverride_TextFlowDirection : 1;

	/** Which text shaping method should the text within this widget use? (unset to use the default returned by GetDefaultTextShapingMethod) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Localization, AdvancedDisplay, meta=(EditCondition="bOverride_TextShapingMethod"))
	ETextShapingMethod TextShapingMethod;
		
	/** Which text flow direction should the text within this widget use? (unset to use the default returned by GetDefaultTextFlowDirection) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Localization, AdvancedDisplay, meta=(EditCondition="bOverride_TextFlowDirection"))
	ETextFlowDirection TextFlowDirection;
};


/**
 * Base class for all widgets that use a text layout.
 * Contains the common options that should be exposed for the underlying Slate widget.
 */
UCLASS(Abstract, BlueprintType)
class UMG_API UTextLayoutWidget : public UWidget
{
	GENERATED_UCLASS_BODY()

protected:
	/** Synchronize the properties with the given widget. A template as the Slate widgets conform to the same API, but don't derive from a common base. */
	template <typename TWidgetType>
	void SynchronizeTextLayoutProperties(TWidgetType& InWidget)
	{
		ShapedTextOptions.SynchronizeShapedTextProperties(InWidget);

		InWidget.SetJustification(Justification);
		InWidget.SetAutoWrapText(AutoWrapText);
		InWidget.SetWrapTextAt(WrapTextAt != 0 ? WrapTextAt : TAttribute<float>());
		InWidget.SetWrappingPolicy(WrappingPolicy);
		InWidget.SetMargin(Margin);
		InWidget.SetLineHeightPercentage(LineHeightPercentage);
	}

	/** Controls how the text within this widget should be shaped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Localization, AdvancedDisplay, meta=(ShowOnlyInnerProperties))
	FShapedTextOptions ShapedTextOptions;

	/** How the text should be aligned with the margin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	TEnumAsByte<ETextJustify::Type> Justification;

	/** True if we're wrapping text automatically based on the computed horizontal space for this widget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Wrapping)
	bool AutoWrapText;

	/** Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Wrapping)
	float WrapTextAt;

	/** The wrapping policy to use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Wrapping, AdvancedDisplay)
	ETextWrappingPolicy WrappingPolicy;

	/** The amount of blank space left around the edges of text area. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	FMargin Margin;

	/** The amount to scale each lines height by. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	float LineHeightPercentage;
};
