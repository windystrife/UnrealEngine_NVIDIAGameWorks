// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "ArrowComponent.generated.h"

class FPrimitiveSceneProxy;
struct FConvexVolume;
struct FEngineShowFlags;

/** 
 * A simple arrow rendered using lines. Useful for indicating which way an object is facing.
 */
UCLASS(ClassGroup=Utility, hidecategories=(Object,LOD,Physics,Lighting,TextureStreaming,Activation,"Components|Activation",Collision), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UArrowComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	FColor ArrowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	float ArrowSize;

	/** Set to limit the screen size of this arrow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	bool bIsScreenSizeScaled;

	/** The size on screen to limit this arrow to (in screen space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	float ScreenSize;

	/** If true, don't show the arrow when EngineShowFlags.BillboardSprites is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	uint32 bTreatAsASprite:1;

#if WITH_EDITORONLY_DATA
	/** Sprite category that the arrow component belongs to, if being treated as a sprite. Value serves as a key into the localization file. */
	UPROPERTY()
	FName SpriteCategoryName_DEPRECATED;

	/** Sprite category information regarding the arrow component, if being treated as a sprite. */
	UPROPERTY()
	struct FSpriteCategoryInfo SpriteInfo;

	/** If true, this arrow component is attached to a light actor */
	UPROPERTY()
	uint32 bLightAttachment:1;

	/** Whether to use in-editor arrow scaling (i.e. to be affected by the global arrow scale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ArrowComponent)
	bool bUseInEditorScaling;	
#endif // WITH_EDITORONLY_DATA
	/** Updates the arrow's colour, and tells it to refresh */
	UFUNCTION(BlueprintCallable, DisplayName="SetArrowColor", Category="Components|Arrow")
	virtual void SetArrowColor(FLinearColor NewColor);

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
#if WITH_EDITOR
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

#if WITH_EDITORONLY_DATA
	/** Set the scale that we use when rendering in-editor */
	static void SetEditorScale(float InEditorScale);

	/** The scale we use when rendering in-editor */
	static float EditorScale;
#endif
};



