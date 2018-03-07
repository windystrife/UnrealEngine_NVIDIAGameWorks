// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintTypes.h"
#include "MeshPaintSettings.h"

#include "PaintModeSettings.generated.h"

UENUM()
enum class ETextureWeightTypes : uint8
{
	/** Lerp Between Two Textures using Alpha Value */
	AlphaLerp = 2 UMETA(DisplayName = "Alpha (Two Textures)"),

	/** Weighting Three Textures according to Channels*/
	RGB = 3 UMETA(DisplayName = "RGB (Three Textures)"),

	/**  Weighting Four Textures according to Channels*/
	ARGB = 4 UMETA(DisplayName = "ARGB (Four Textures)"),

	/**  Weighting Five Textures according to Channels */
	OneMinusARGB = 5 UMETA(DisplayName = "ARGB - 1 (Five Textures)")
};

UENUM()
enum class ETexturePaintIndex : uint8
{
	TextureOne = 0,
	TextureTwo,
	TextureThree,
	TextureFour,
	TextureFive
};

/** Vertex Painting settings structure used for vertex color and texture blend weight painting */
USTRUCT()
struct FVertexPaintSettings
{
	GENERATED_BODY()
public:
	FVertexPaintSettings()
		: MeshPaintMode(EMeshPaintMode::PaintColors),
		PaintColor(FLinearColor::White),
		EraseColor(FLinearColor::Black),
		bWriteRed(true),
		bWriteGreen(true),
		bWriteBlue(true),
		bWriteAlpha(false),
		TextureWeightType(ETextureWeightTypes::AlphaLerp),
		PaintTextureWeightIndex(ETexturePaintIndex::TextureOne),
		EraseTextureWeightIndex(ETexturePaintIndex::TextureTwo)	{}	

	UPROPERTY()
	EMeshPaintMode MeshPaintMode;

	/** Color used for Applying Vertex Color Painting */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta=(EnumCondition = 0))
	FLinearColor PaintColor;

	/** Color used for Erasing Vertex Color Painting */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (EnumCondition = 0))
	FLinearColor EraseColor;

	/** Whether or not to apply Vertex Color Painting to the Red Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Red", EnumCondition = 0))
	bool bWriteRed; 

	/** Whether or not to apply Vertex Color Painting to the Green Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Green", EnumCondition = 0))
	bool bWriteGreen; 

	/** Whether or not to apply Vertex Color Painting to the Blue Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Blue", EnumCondition = 0))
	bool bWriteBlue; 

	/** Whether or not to apply Vertex Color Painting to the Alpha Channel */
	UPROPERTY(EditAnywhere, Category = ColorPainting, meta = (DisplayName = "Alpha", EnumCondition = 0))
	bool bWriteAlpha;

	/** Texture Blend Weight Painting Mode */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	ETextureWeightTypes TextureWeightType;

	/** Texture Blend Weight index which should be applied during Painting */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	ETexturePaintIndex PaintTextureWeightIndex;

	/** Texture Blend Weight index which should be erased during Painting */
	UPROPERTY(EditAnywhere, Category = WeightPainting, meta = (EnumCondition = 1))
	ETexturePaintIndex EraseTextureWeightIndex;

	/** When unchecked the painting on the base LOD will be propagate automatically to all other LODs when exiting the mode or changing the selection */
	UPROPERTY(EditAnywhere, Category = Painting)
	bool bPaintOnSpecificLOD;

	/** LOD Index to which should specifically be painted */
	UPROPERTY(EditAnywhere, Category = Painting, meta = (UIMin = "0", ClampMin = "0", EditCondition = "bPaintOnSpecificLOD"))
	int32 LODIndex;
};

/** Texture painting settings structure */
USTRUCT()
struct FTexturePaintSettings
{
	GENERATED_BODY()
public:
	FTexturePaintSettings()
		: 
		PaintColor(FLinearColor::White),
		EraseColor(FLinearColor::Black),
		bWriteRed(true),
		bWriteGreen(true),
		bWriteBlue(true),
		bWriteAlpha(false),
		UVChannel(0),
		bEnableSeamPainting(false),
		PaintTexture(nullptr) {}

	EMeshVertexPaintTarget VertexPaintTarget;

	/** Color used for Applying Texture Painting */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	FLinearColor PaintColor;

	/** Color used for Erasing Texture Painting */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	FLinearColor EraseColor;

	/** Whether or not to apply Vertex Color Painting to the Red Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Red"))
	bool bWriteRed; 

	/** Whether or not to apply Vertex Color Painting to the Green Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Green"))
	bool bWriteGreen; 

	/** Whether or not to apply Vertex Color Painting to the Blue Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Blue"))
	bool bWriteBlue; 

	/** Whether or not to apply Vertex Color Painting to the Alpha Channel */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta = (DisplayName = "Alpha"))
	bool bWriteAlpha;

	/** UV channel which should be used for paint textures */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	int32 UVChannel; 

	/** Seam painting flag, True if we should enable dilation to allow the painting of texture seams */
	UPROPERTY(EditAnywhere, Category = TexturePainting)
	bool bEnableSeamPainting;

	/** Texture to which Painting should be Applied */
	UPROPERTY(EditAnywhere, Category = TexturePainting, meta=(DisplayThumbnail="true"))
	UTexture2D* PaintTexture;
};

/** Enum used to switch between vertex and texture painting in the level editor paint mode */
UENUM()
enum class EPaintMode : uint8
{
	Vertices,
	Textures
};

/** Paint mode settings class derives from base mesh painting settings */
UCLASS()
class UPaintModeSettings : public UMeshPaintSettings
{
	GENERATED_UCLASS_BODY()
public:

	static UPaintModeSettings* Get();
	
	UPROPERTY()
	EPaintMode PaintMode;

	UPROPERTY(EditAnywhere, Category = VertexPainting, meta=(ShowOnlyInnerProperties))
	FVertexPaintSettings VertexPaintSettings;

	UPROPERTY(EditAnywhere, Category = TexturePainting, meta=(ShowOnlyInnerProperties))
	FTexturePaintSettings TexturePaintSettings;
};
