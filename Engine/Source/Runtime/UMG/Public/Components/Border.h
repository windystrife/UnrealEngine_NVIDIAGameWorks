// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateBrush.h"
#include "Components/ContentWidget.h"
#include "Border.generated.h"

class SBorder;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USlateBrushAsset;
class UTexture2D;

/**
 * A border is a container widget that can contain one child widget, providing an opportunity 
 * to surround it with a background image and adjustable padding.
 *
 * * Single Child
 * * Image
 */
UCLASS()
class UMG_API UBorder : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:

	/** The alignment of the content horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the content vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

	/** Whether or not to show the disabled effect when this border is disabled */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, AdvancedDisplay)
	uint8 bShowEffectWhenDisabled:1;

	/** Color and opacity multiplier of content in the border */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content", meta=( sRGB="true" ))
	FLinearColor ContentColorAndOpacity;

	/** A bindable delegate for the ContentColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ContentColorAndOpacityDelegate;

	/** The padding area between the slot and the content it contains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Content")
	FMargin Padding;

	/** Brush to drag as the background */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( DisplayName="Brush" ))
	FSlateBrush Background;

	/** A bindable delegate for the Brush. */
	UPROPERTY()
	FGetSlateBrush BackgroundDelegate;

	/** Color and opacity of the actual border image */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( sRGB="true" ))
	FLinearColor BrushColor;

	/** A bindable delegate for the BrushColor. */
	UPROPERTY()
	FGetLinearColor BrushColorDelegate;

	/**
	 * Scales the computed desired size of this border and its contents. Useful
	 * for making things that slide open without having to hard-code their size.
	 * Note: if the parent widget is set up to ignore this widget's desired size,
	 * then changing this value will have no effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FVector2D DesiredSizeScale;

public:

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonDownEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonUpEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseMoveEvent;

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseDoubleClickEvent;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetContentColorAndOpacity(FLinearColor InContentColorAndOpacity);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushColor(FLinearColor InBrushColor);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrush(const FSlateBrush& InBrush);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromAsset(USlateBrushAsset* Asset);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromTexture(UTexture2D* Texture);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromMaterial(UMaterialInterface* Material);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicMaterial();

public:
	/**
	* Sets the DesireSizeScale of this border.
	*
	* @param InScale    The X and Y multipliers for the desired size
	*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetDesiredSizeScale(FVector2D InScale);

public:

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	virtual const FText GetPaletteCategory() override;
#endif

protected:

	// UPanelWidget
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;
	// End UPanelWidget

protected:
	TSharedPtr<SBorder> MyBorder;

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	FReply HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleMouseDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	/** Translates the bound brush data and assigns it to the cached brush used by this widget. */
	const FSlateBrush* ConvertImage(TAttribute<FSlateBrush> InImageAsset) const;

#if WITH_EDITORONLY_DATA
protected:
	/** Image to use for the border */
	UPROPERTY()
	USlateBrushAsset* Brush_DEPRECATED;
#endif

	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, ContentColorAndOpacity)
};
