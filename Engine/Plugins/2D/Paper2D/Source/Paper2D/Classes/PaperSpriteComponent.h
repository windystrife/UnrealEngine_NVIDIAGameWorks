// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/MeshComponent.h"

#include "PaperSpriteComponent.generated.h"

class FPrimitiveSceneProxy;
class UPaperSprite;
class UTexture;

/**
 * A component that handles rendering and collision for a single instance of a UPaperSprite asset.
 *
 * This component is created when you drag a sprite asset from the content browser into a Blueprint, or
 * contained inside of the actor created when you drag one into the level.
 *
 * @see UPrimitiveComponent, UPaperSprite
 */

UCLASS(ShowCategories=(Mobility), ClassGroup=Paper2D, meta=(BlueprintSpawnableComponent))
class PAPER2D_API UPaperSpriteComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

protected:
	// The sprite asset used by this component
	UPROPERTY(Category=Sprite, EditAnywhere, BlueprintReadOnly, meta=(DisplayThumbnail = "true"))
	UPaperSprite* SourceSprite;

	// DEPRECATED in 4.4: The material override for this sprite component (if any); replaced by the Materials array inherited from UMeshComponent
	UPROPERTY()
	UMaterialInterface* MaterialOverride_DEPRECATED;

	// The color of the sprite (passed to the sprite material as a vertex color)
	UPROPERTY(BlueprintReadOnly, Interp, Category=Sprite)
	FLinearColor SpriteColor;

public:
	/** Change the PaperSprite used by this instance. */
	UFUNCTION(BlueprintCallable, Category="Sprite")
	virtual bool SetSprite(class UPaperSprite* NewSprite);

	/** Gets the PaperSprite used by this instance. */
	UFUNCTION(BlueprintPure, Category="Sprite")
	virtual UPaperSprite* GetSprite();

	/** Returns the current color of the sprite */
	FLinearColor GetSpriteColor() const { return SpriteColor; }

	/** Set color of the sprite */
	UFUNCTION(BlueprintCallable, Category="Sprite")
	void SetSpriteColor(FLinearColor NewColor);

	// Returns the wireframe color to use for this component.
	FLinearColor GetWireframeColor() const;

public:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	// End of UObject interface

	// UActorComponent interface
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual const UObject* AdditionalStatObject() const override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	// End of UActorComponent interface

	// USceneComponent interface
	virtual bool HasAnySockets() const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	// End of USceneComponent interface

	// UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual class UBodySetup* GetBodySetup() override;
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel) override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;
	virtual int32 GetNumMaterials() const override;
	// End of UPrimitiveComponent interface

#if WITH_EDITOR
	void SetTransientTextureOverride(const UTexture* TextureToModifyOverrideFor, UTexture* OverrideTexture);
#endif

protected:
	friend class FPaperSpriteSceneProxy;
};
