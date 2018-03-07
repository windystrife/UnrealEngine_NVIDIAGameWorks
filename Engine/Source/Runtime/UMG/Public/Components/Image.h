// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Styling/SlateBrush.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Image.generated.h"

class SImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USlateBrushAsset;
class UTexture2D;

/**
 * The image widget allows you to display a Slate Brush, or texture or material in the UI.
 *
 * * No Children
 */
UCLASS()
class UMG_API UImage : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA
	/** Image to draw */
	UPROPERTY()
	USlateBrushAsset* Image_DEPRECATED;
#endif

	/** Image to draw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush Brush;

	/** A bindable delegate for the Image. */
	UPROPERTY()
	FGetSlateBrush BrushDelegate;

	/** Color and opacity */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=( sRGB="true") )
	FLinearColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ColorAndOpacityDelegate;

public:

	UPROPERTY(EditAnywhere, Category=Events, meta=( IsBindableEvent="True" ))
	FOnPointerEvent OnMouseButtonDownEvent;

public:

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FLinearColor InColorAndOpacity);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetOpacity(float InOpacity);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrush(const FSlateBrush& InBrush);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromAsset(USlateBrushAsset* Asset);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromTexture(UTexture2D* Texture, bool bMatchSize = false);

	/**  */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBrushFromTextureDynamic(UTexture2DDynamic* Texture, bool bMatchSize = false);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetBrushFromMaterial(UMaterialInterface* Material);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicMaterial();

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITORONLY_DATA
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget Interface
#endif

protected:
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	/** Translates the bound brush data and assigns it to the cached brush used by this widget. */
	const FSlateBrush* ConvertImage(TAttribute<FSlateBrush> InImageAsset) const;

	FReply HandleMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);

protected:
	TSharedPtr<SImage> MyImage;

protected:

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, ColorAndOpacity);
};
