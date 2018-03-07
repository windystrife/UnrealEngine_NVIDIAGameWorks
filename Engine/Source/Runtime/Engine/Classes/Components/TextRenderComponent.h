// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Components/PrimitiveComponent.h"
#include "TextRenderComponent.generated.h"

class FPrimitiveSceneProxy;
class UFont;
class UMaterialInterface;

UENUM()
enum EHorizTextAligment
{
	/**  */
	EHTA_Left UMETA(DisplayName="Left"),
	/**  */
	EHTA_Center UMETA(DisplayName="Center"),
	/**  */
	EHTA_Right UMETA(DisplayName="Right"),
};

UENUM()
enum EVerticalTextAligment
{
	/**  */
	EVRTA_TextTop UMETA(DisplayName="Text Top"),
	/**  */
	EVRTA_TextCenter UMETA(DisplayName="Text Center"),
	/**  */
	EVRTA_TextBottom UMETA(DisplayName="Text Bottom"),
	/**  */
	EVRTA_QuadTop UMETA(DisplayName="Quad Top"),
};

/**
 * Renders text in the world with given font. Contains usual font related attributes such as Scale, Alignment, Color etc.
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Object,LOD,Physics,TextureStreaming,Activation,"Components|Activation",Collision), editinlinenew, meta=(BlueprintSpawnableComponent = ""))
class ENGINE_API UTextRenderComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** Text content, can be multi line using <br> as line separator */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(MultiLine=true))
	FText Text;

	/** Text material */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	class UMaterialInterface* TextMaterial;
	
	/** Text font */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	class UFont* Font;

	/** Horizontal text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(DisplayName = "Horizontal Alignment"))
	TEnumAsByte<enum EHorizTextAligment> HorizontalAlignment;

	/** Vertical text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text, meta=(DisplayName = "Vertical Alignment"))
	TEnumAsByte<enum EVerticalTextAligment> VerticalAlignment;

	/** Color of the text, can be accessed as vertex color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	FColor TextRenderColor;

	/** Horizontal scale, default is 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float XScale;

	/** Vertical scale, default is 1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float YScale;

	/** Vertical size of the fonts largest character in world units. Transform, XScale and YScale will affect final size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Text)
	float WorldSize;
	
	/** The inverse of the Font's character height. */
	UPROPERTY()
	float InvDefaultSize;

	/** Horizontal adjustment per character, default is 0.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Text)
	float HorizSpacingAdjust;

	/** Vertical adjustment per character, default is 0.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Text)
	float VertSpacingAdjust;

	/** Allows text to draw unmodified when using debug visualization modes. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	uint32 bAlwaysRenderAsText:1;

	// -----------------------------
	
	/**
	 * Change the text value and signal the primitives to be rebuilt 
	 * The FString variant is deprecated in favor of the FText variant
	 */
	DEPRECATED(4.8, "Passing text as FString is deprecated, please use FText instead (likely via a LOCTEXT).")
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender", meta=(DisplayName="Set Text (String)", DeprecatedFunction, DeprecationMessage="Use the SetText function taking an FText instead."))
	void SetText(const FString& Value);

	/** Change the text value and signal the primitives to be rebuilt */
	void SetText(const FText& Value);

	/** Change the text value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender", meta=(DisplayName="Set Text"))
	void K2_SetText(const FText& Value);

	/** Change the text material and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetTextMaterial(UMaterialInterface* Material);

	/** Change the font and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetFont(UFont* Value);

	/** Change the horizontal alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetHorizontalAlignment(EHorizTextAligment Value);

	/** Change the vertical alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetVerticalAlignment(EVerticalTextAligment Value);

	/** Change the text render color and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetTextRenderColor(FColor Value);

	/** Change the text X scale and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetXScale(float Value);

	/** Change the text Y scale and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetYScale(float Value);

	/** Change the text horizontal spacing adjustment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetHorizSpacingAdjust(float Value);

	/** Change the text vertical spacing adjustment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetVertSpacingAdjust(float Value);

	/** Change the world size of the text and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	void SetWorldSize(float Value);

	/** Get local size of text */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	FVector GetTextLocalSize() const;

	/** Get world space size of text */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|TextRender")
	FVector GetTextWorldSize() const;

	// -----------------------------

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false ) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual FMatrix GetRenderMatrix() const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject interface.

	static void InitializeMIDCache();
	static void ShutdownMIDCache();
};





